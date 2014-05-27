using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using DirectShowLib;
using Microsoft.Win32.SafeHandles;
using RtspSourceWpf.DirectShow;

namespace RtspSourceWpf
{
    public enum SessionCommand
    {
        Play,
        Stop,
        Terminate
    }

    internal class DirectShowSession : IDisposable
    {
        private readonly RtspPlayer _player;
        private readonly String _rtspUrl;
        private readonly IntPtr _hwnd;
        private Thread _workerThread;
        private bool _isDisposed = false;
        private bool _isDone = false;
        private ManualResetEvent _initialReconnectEvent = new ManualResetEvent(false);
        private BlockingCollection<SessionCommand> _queue = new BlockingCollection<SessionCommand>();
        private AutoResetEvent _manualRequest = new AutoResetEvent(false);
        private IPresenter _customPresenter;

        private uint _autoReconnectionPeriod = 5000;
        public uint AutoReconnectionPeriod
        { 
            get { return _autoReconnectionPeriod; } 
            set { _autoReconnectionPeriod = value; }
        }

        public DirectShowSession(RtspPlayer player, String rtspUrl, IntPtr hwnd)
        {
            _player = player;
            _rtspUrl = rtspUrl;
            _hwnd = hwnd;
            _workerThread = new Thread(WorkerThread);
            _workerThread.Start();
        }

        public void Dispose()
        {
            if (_isDisposed) return;

            _isDone = true;
            _initialReconnectEvent.Set();

            _queue.Add(SessionCommand.Terminate);
            _manualRequest.Set();

            if (_workerThread != null)
            {
                _workerThread.Join();
                _workerThread = null;
            }

            _isDisposed = true;
        }

        public void Play()
        {
            _queue.Add(SessionCommand.Play);
            _manualRequest.Set();
        }

        public void Stop()
        {
            _queue.Add(SessionCommand.Stop);
            _manualRequest.Set();
        }

        private IBaseFilter GetSourceFilter()
        {
            var rtspSourceFilter = (IBaseFilter)new RtspSourceFilter();
            var rtspSourceConfig = (IRtspSourceConfig) rtspSourceFilter;

            //rtspSourceConfig.SetInitialSeekTime(100.0);
            rtspSourceConfig.SetLatency(500);
            rtspSourceConfig.SetAutoReconnectionPeriod(AutoReconnectionPeriod);

            var rtspFileSource = (IFileSourceFilter) rtspSourceFilter;
            do
            {
                if (rtspFileSource.Load(_rtspUrl, null) != 0)
                {
                    Console.WriteLine("Couldn't load " + _rtspUrl);
                    _initialReconnectEvent.WaitOne((int)AutoReconnectionPeriod);
                }
                else
                {
                    break;
                }
            }
            while (!_isDone);

            return rtspSourceFilter;
        }

        private IBaseFilter GetDecoderFilter()
        {
            var videoDecoder = (IBaseFilter)new LAVVideo();

            var lavVideoSettings = videoDecoder as ILAVVideoSettings;
            if (lavVideoSettings != null)
            {
                lavVideoSettings.SetRuntimeConfig(true);
                lavVideoSettings.SetNumThreads(1);
                // If Presenter doesnt support DXVA2 than it fallbacks to avcodec
                lavVideoSettings.SetHWAccel(LAVHWAccel.HWAccel_DXVA2Native);
            }

            return videoDecoder;
        }

        private IBaseFilter GetRendererFilter()
        {
            var evr = new EnhancedVideoRenderer();
            var evrFilter = (IBaseFilter)evr;

            // Initialize the EVR renderer with our custom video presenter
            var evrPresenter = EVRPresenter.Create();
            ((IMFVideoRenderer)evr).InitializeRenderer(null, evrPresenter.VideoPresenter);

            // Configure the presenter with our hWnd
            var displayControl = (IMFVideoDisplayControl)evrPresenter.VideoPresenter;
            displayControl.SetVideoWindow(_hwnd);

            _customPresenter = evrPresenter;
            _customPresenter.NewSurfaceEvent += _player.NewSurface;
            _customPresenter.NewFrameEvent += _player.NewFrame;

            return evrFilter;
        }

        private IBaseFilter GetSoundOutputFilter()
        {
            return (IBaseFilter) new DSoundRender();
        }

        private void WorkerThread()
        {
            try
            {
                var filterGraph = (IGraphBuilder)new FilterGraph();

                var sourceFilter = GetSourceFilter();
                if (_isDone) 
                    return;

                var decoderFilter = GetDecoderFilter();
                var rendererFilter = GetRendererFilter();
                var soundOutputFilter = GetSoundOutputFilter();

                filterGraph.AddFilter(sourceFilter, "Source");
                filterGraph.AddFilter(decoderFilter, "Video decoder");
                filterGraph.AddFilter(rendererFilter, "Renderer");
                filterGraph.AddFilter(soundOutputFilter, "Sound output");

                var captureGraphBuilder = (ICaptureGraphBuilder2)new CaptureGraphBuilder2();
                captureGraphBuilder.SetFiltergraph(filterGraph);
                captureGraphBuilder.RenderStream(null, MediaType.Video, sourceFilter, decoderFilter, rendererFilter);
                captureGraphBuilder.RenderStream(null, MediaType.Audio, sourceFilter, null, soundOutputFilter);

                var mediaControl = (IMediaControl)filterGraph;
                var mediaEvent = (IMediaEvent)filterGraph;

                // Get media event handler
                IntPtr ptr;
                mediaEvent.GetEventHandle(out ptr);

                var hMediaEvent = new ManualResetEvent(false);
                hMediaEvent.SafeWaitHandle = new SafeWaitHandle(ptr, false);

                EventCode eventCode;
                IntPtr lParam1, lParam2;

                while (true)
                {
                    WaitHandle[] waitHandles = { hMediaEvent, _manualRequest };
                    int res = EventWaitHandle.WaitAny(waitHandles, Timeout.Infinite);

                    if (res == 0)
                    {
                        // Media event
                        mediaEvent.GetEvent(out eventCode, out lParam1, out lParam2, 0);
                        mediaEvent.FreeEventParams(eventCode, lParam1, lParam2);
                    }
                    else /* if (res == 1) */
                    {
                        SessionCommand cmd = _queue.Take();
                        Debug.WriteLine("Request: {0}", cmd);

                        if (cmd == SessionCommand.Play)
                        {
                            mediaControl.Run();
                        }
                        else if(cmd == SessionCommand.Stop)
                        {
                            mediaControl.StopWhenReady();
                        }
                        else if (cmd == SessionCommand.Terminate)
                        {
                            mediaControl.StopWhenReady();
                            break;
                        }
                        else
                        {
                            Debug.WriteLine("Request {0} not supported", cmd);
                        }
                    }
                }

            }
            catch(Exception ex)
            {
                App.Current.Dispatcher.Invoke(new Action(() => 
                {
                    MessageBox.Show(ex.ToString());
                }));
            }
            finally
            {
                if (_customPresenter != null)
                    _customPresenter.Dispose();
            }
        }
    }

    public class RtspPlayer : UserControl, IDisposable
    {
        private Image _image = new Image();
        private D3DImage _d3dImage = new D3DImage();
        private IntPtr _surface = IntPtr.Zero;
        private IntPtr _hwndSource = IntPtr.Zero;
        private DirectShowSession _session = null;
        private bool _isDisposed = false;
        private object _sync = new object();

        public RtspPlayer()
        {
            Loaded += RtspPlayer_Loaded;
            _d3dImage.IsFrontBufferAvailableChanged += IsFrontBufferAvailableChanged;
            _image.Source = _d3dImage;
            Content = _image;
        }

        void IsFrontBufferAvailableChanged(object sender, DependencyPropertyChangedEventArgs e)
        {
            throw new NotImplementedException();
        }

        public void Dispose()
        {
            lock (_sync)
            {
                if (_isDisposed) return;

                if (_session != null)
                {
                    _session.Dispose();
                    _session = null;
                }

                _isDisposed = true;
            }
        }

        public void Connect(String rtspUrl)
        {
            lock (_sync)
            {
                CheckDisposed();

                if (_session != null)
                    _session.Dispose();

                if (_hwndSource == IntPtr.Zero)
                {
                    _hwndSource = GetHwndSourceFromMainWindow();
                    if (_hwndSource == IntPtr.Zero)
                        throw new NullReferenceException("HwndSource is null");
                }

                _session = new DirectShowSession(this, rtspUrl, _hwndSource);
            }
        }

        public void Stop()
        {
            lock (_sync)
            {
                CheckDisposed();

                if (_session != null)
                {
                    _session.Stop();
                }
            }
        }

        public void Play()
        {
            lock (_sync)
            {
                CheckDisposed();

                if (_session != null)
                {
                    _session.Play();
                }
            }
        }

        private void RtspPlayer_Loaded(object sender, RoutedEventArgs e)
        {
            HwndSource hwndSource = HwndSource.FromVisual(this) as HwndSource;
            if (hwndSource != null)
            {
                _hwndSource = hwndSource.Handle;
            }
        }

        private IntPtr GetHwndSourceFromMainWindow()
        {
            return new WindowInteropHelper(App.Current.MainWindow).Handle;
        }

        private void CheckDisposed()
        {
            if (_isDisposed) throw new ObjectDisposedException("RtspPlayer");
        }

        internal void NewSurface(IntPtr surfacePtr)
        {
            _surface = surfacePtr;

            Dispatcher.BeginInvoke(new Action(() =>
            {
                _d3dImage.Lock();
                _d3dImage.SetBackBuffer(D3DResourceType.IDirect3DSurface9, surfacePtr);
                _d3dImage.Unlock();
            }));
        }

        internal void NewFrame()
        {
            Dispatcher.BeginInvoke(new Action(() =>
            {
                if (_d3dImage.IsFrontBufferAvailable && _surface != IntPtr.Zero)
                {
                    _d3dImage.Lock();
                    _d3dImage.AddDirtyRect(new Int32Rect(0, 0, _d3dImage.PixelWidth, _d3dImage.PixelHeight));
                    _d3dImage.Unlock();
                }
            }));
        }
    }
}
