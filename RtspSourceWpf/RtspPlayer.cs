using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using RtspSourceWpf.Enums;

namespace RtspSourceWpf
{
    public class RtspPlayer : UserControl, IDisposable
    {
        private Image _image = new Image();
        private D3DImage _d3dImage = new D3DImage();
        private IntPtr _surface = IntPtr.Zero;
        private IntPtr _hwndSource = IntPtr.Zero;
        private DirectShowSession _session = null;
        private bool _isDisposed = false;
        private object _sync = new object();
        private EDecoderType _decoderType;
        private EVideoRendererType _videoRendererType;

        public RtspPlayer()
            : this(EDecoderType.LAVVideo, EVideoRendererType.EnhancedVideoRenderer)
        {
        }

        public RtspPlayer(EDecoderType decoderType, EVideoRendererType videoRendererType)
        {
            _decoderType = decoderType;
            _videoRendererType = videoRendererType;
            _d3dImage.IsFrontBufferAvailableChanged += IsFrontBufferAvailableChanged;
            _image.Source = _d3dImage;

            Loaded += RtspPlayer_Loaded;
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

                _session = new DirectShowSession(this, rtspUrl, _hwndSource,
                    _decoderType, _videoRendererType);
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

            DispatchToUI(() => 
            {
                _d3dImage.Lock();
                _d3dImage.SetBackBuffer(D3DResourceType.IDirect3DSurface9, surfacePtr);
                _d3dImage.Unlock();
            });
        }

        internal void NewFrame()
        {
            DispatchToUI(() => 
            {
                if (_d3dImage.IsFrontBufferAvailable && _surface != IntPtr.Zero)
                {
                    _d3dImage.Lock();
                    _d3dImage.AddDirtyRect(new Int32Rect(0, 0, _d3dImage.PixelWidth, _d3dImage.PixelHeight));
                    _d3dImage.Unlock();
                }
            });
        }

        private void DispatchToUI(Action action)
        {
            if (Dispatcher.CheckAccess())
            {
                action.Invoke();
            }
            else
            {
                Dispatcher.BeginInvoke(action);
            }
        }
    }
}
