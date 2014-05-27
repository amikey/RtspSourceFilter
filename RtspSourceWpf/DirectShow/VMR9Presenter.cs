using DirectShowLib;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace RtspSourceWpf.DirectShow
{
    [ComImport, Guid("0AC61D46-465C-4B1D-A9A6-938BB10640E1")]
    internal class CustomVMR9Presenter
    {
    }

    [Guid("6C9EF7BD-2436-44B7-8407-84352BC29937"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IVMR9PresenterCallback
    {
        void PresentSurfaceCB([In] IntPtr pSurface);
    }

    [Guid("AFCDB40E-CFE7-4F77-9222-21D9303187CB"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IVMR9PresenterRegisterCallback
    {
        void RegisterCallback([In,MarshalAs(UnmanagedType.Interface)] IVMR9PresenterCallback pCallback);
    }

    public class VMR9Presenter : IVMR9PresenterCallback, IPresenter
    {
        private IntPtr _lastSurface;

        public IVMRSurfaceAllocator9 SurfaceAllocator { get; private set; }

        public event NewSurfaceDelegate NewSurfaceEvent;
        public event NewFrameDelegate NewFrameEvent;

        public static VMR9Presenter Create()
        {
            var vmr9Presenter = new VMR9Presenter();
            var customVmr = new CustomVMR9Presenter();

            vmr9Presenter.SurfaceAllocator = (IVMRSurfaceAllocator9)customVmr;

            // Register for present callback
            ((IVMR9PresenterRegisterCallback)customVmr).RegisterCallback(vmr9Presenter);

            return vmr9Presenter;
        }

        public void Dispose()
        {
            if (SurfaceAllocator != null)
            {
                ((IVMR9PresenterRegisterCallback)SurfaceAllocator).RegisterCallback(null);
            }
            SurfaceAllocator = null;
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

        private VMR9Presenter()
        {
        }
    }
}
