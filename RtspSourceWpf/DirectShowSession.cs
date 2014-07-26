using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Threading;
using System.Windows;
using DirectShowLib;
using Microsoft.Win32.SafeHandles;
using RtspSourceWpf.DirectShow;
using RtspSourceWpf.Enums;

namespace RtspSourceWpf
{
    internal class DirectShowSession : IDisposable
    {
        private readonly RtspPlayer _player;
        private readonly String _rtspUrl;
        private readonly IntPtr _hwnd;
        private Thread _workerThread;
        private bool _isDisposed = false;
        private bool _isDone = false;
        private ManualResetEvent _initialReconnectEvent = new ManualResetEvent(false);
        private BlockingCollection<ESessionCommand> _queue = new BlockingCollection<ESessionCommand>();
        private AutoResetEvent _manualRequest = new AutoResetEvent(false);
        private IPresenter _customPresenter;
        private EVideoRendererType _videoRendererType;
        private EDecoderType _decoderType;

        private uint _autoReconnectionPeriod = 5000;
        public uint AutoReconnectionPeriod
        {
            get { return _autoReconnectionPeriod; }
            set { _autoReconnectionPeriod = value; }
        }

        public DirectShowSession(RtspPlayer player, String rtspUrl, IntPtr hwnd, 
            EDecoderType decoderType, EVideoRendererType videoRendererType)
        {
            _player = player;
            _rtspUrl = rtspUrl;
            _hwnd = hwnd;
            _decoderType = decoderType;
            _videoRendererType = videoRendererType;
            _workerThread = new Thread(WorkerThread);
            _workerThread.Start();
        }

        public void Dispose()
        {
            if (_isDisposed) return;

            _isDone = true;
            _initialReconnectEvent.Set();

            _queue.Add(ESessionCommand.Terminate);
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
            _queue.Add(ESessionCommand.Play);
            _manualRequest.Set();
        }

        public void Stop()
        {
            _queue.Add(ESessionCommand.Stop);
            _manualRequest.Set();
        }

        private IBaseFilter GetSourceFilter()
        {
            var rtspSourceFilter = (IBaseFilter)new RtspSourceFilter();
            var rtspSourceConfig = (IRtspSourceConfig)rtspSourceFilter;

            //rtspSourceConfig.SetInitialSeekTime(100.0);
            rtspSourceConfig.SetLatency(500);
            rtspSourceConfig.SetAutoReconnectionPeriod(AutoReconnectionPeriod);

            var rtspFileSource = (IFileSourceFilter)rtspSourceFilter;
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

        private IBaseFilter GetDecoderFilter(EDecoderType decoderType)
        {
            switch(decoderType)
            {
                case EDecoderType.LAVVideo:
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

                case EDecoderType.MicrosoftVideoDecoder:
                    return (IBaseFilter) new MicrosoftVideoDecoder();
            }

            return null;
        }

        private IBaseFilter GetRendererFilter(EVideoRendererType videoRendererType)
        {
            switch (videoRendererType)
            {
                case EVideoRendererType.EnhancedVideoRenderer:
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

                case EVideoRendererType.VideoMixingRenderer:
                    var vmr = new VideoMixingRenderer9();
                    var vmrFilter = (IBaseFilter)vmr;

                    var vmrPresenter = VMR9Presenter.Create();

                    // Initialize the VMR renderer with out custom video presenter
                    var filterConfig = (IVMRFilterConfig9)vmr;
                    filterConfig.SetRenderingMode(VMR9Mode.Renderless);
                    filterConfig.SetNumberOfStreams(1);

                    var surfaceAllocatorNotify = (IVMRSurfaceAllocatorNotify9)vmr;

                    surfaceAllocatorNotify.AdviseSurfaceAllocator(IntPtr.Zero, vmrPresenter.SurfaceAllocator);
                    vmrPresenter.SurfaceAllocator.AdviseNotify(surfaceAllocatorNotify);

                    _customPresenter = vmrPresenter;
                    _customPresenter.NewSurfaceEvent += _player.NewSurface;
                    _customPresenter.NewFrameEvent += _player.NewFrame;

                    return vmrFilter;
            }

            return null;
        }

        private IBaseFilter GetSoundOutputFilter()
        {
            return (IBaseFilter)new DSoundRender();
        }

        private void WorkerThread()
        {
            try
            {
                var filterGraph = (IGraphBuilder)new FilterGraph();

                var sourceFilter = GetSourceFilter();
                if (_isDone)
                    return;

                var decoderFilter = GetDecoderFilter(_decoderType);
                var rendererFilter = GetRendererFilter(_videoRendererType);
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
                        ESessionCommand cmd = _queue.Take();
                        Debug.WriteLine("Request: {0}", cmd);

                        if (cmd == ESessionCommand.Play)
                        {
                            mediaControl.Run();
                        }
                        else if (cmd == ESessionCommand.Stop)
                        {
                            mediaControl.StopWhenReady();
                        }
                        else if (cmd == ESessionCommand.Terminate)
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
            catch (Exception ex)
            {
                App.Current.Dispatcher.Invoke(() =>
                {
                    MessageBox.Show(ex.ToString());
                });
            }
            finally
            {
                if (_customPresenter != null)
                    _customPresenter.Dispose();
            }
        }
    }
}
