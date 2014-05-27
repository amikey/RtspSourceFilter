using System;

namespace RtspSourceWpf
{
    public delegate void NewSurfaceDelegate(IntPtr surfacePtr);
    public delegate void NewFrameDelegate();

    public interface IPresenter : IDisposable
    {
        event NewSurfaceDelegate NewSurfaceEvent;
        event NewFrameDelegate NewFrameEvent;
    }
}
