using System;
using System.Runtime.InteropServices;
using System.Security;

namespace RtspSourceWpf.DirectShow
{
    [ComImport, Guid("B21F3368-7260-4B45-9179-BA51590E3B9E")]
    internal class CustomEVRPresenter
    {
    }

    [ComVisible(true), ComImport, SuppressUnmanagedCodeSecurity,
     Guid("576DCFD9-3C6B-4317-A635-2A1494E08B75"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IEVRPresenterCallback
    {
        void PresentSurfaceCB(IntPtr pSurface);
    }

    [ComVisible(true), ComImport, SuppressUnmanagedCodeSecurity,
     Guid("F3AB6A07-5A21-4034-908A-A9F48FAC2F63"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IEVRPresenterRegisterCallback
    {
        void RegisterCallback(IEVRPresenterCallback pCallback);
    }

    [ComVisible(true), ComImport, SuppressUnmanagedCodeSecurity,
     Guid("9B594575-859E-4848-812B-441406F94E5F"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IEVRPresenterSettings
    {
        void SetBufferCount(int bufferCount);
    }

    public class EVRPresenter : IEVRPresenterCallback, IPresenter
    {
        private IntPtr _lastSurface;

        public IMFVideoPresenter VideoPresenter { get; private set; }

        public event NewSurfaceDelegate NewSurfaceEvent;
        public event NewFrameDelegate NewFrameEvent;

        public static EVRPresenter Create()
        {
            var evrPresenter = new EVRPresenter();
            var customEvr = new CustomEVRPresenter();

            evrPresenter.VideoPresenter = (IMFVideoPresenter)customEvr;

            // Register for present callback
            ((IEVRPresenterRegisterCallback)customEvr).RegisterCallback(evrPresenter);

            // Set buffer count
            ((IEVRPresenterSettings)customEvr).SetBufferCount(3);

            return evrPresenter;
        }

        public void Dispose()
        {
            if (VideoPresenter != null)
            {
                ((IEVRPresenterRegisterCallback)VideoPresenter).RegisterCallback(null);
            }
            VideoPresenter = null;
        }

        public void PresentSurfaceCB(IntPtr pSurface)
        {
            // Check if the surface is the same as the last
            if (_lastSurface != pSurface)
            {
                if (NewSurfaceEvent != null)
                {
                    NewSurfaceEvent(pSurface);
                }
            }

            // Store ref to the pointer so we can compare it next time this method is called
            _lastSurface = pSurface;

            if (NewFrameEvent != null)
            {
                NewFrameEvent();
            }
        }

        private EVRPresenter()
        {
        }
    }
}
