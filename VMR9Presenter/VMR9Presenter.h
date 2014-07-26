#pragma once

#include <windows.h>
#include <dshow.h>
#include <d3d9.h>
#include <vmr9.h>

MIDL_INTERFACE("6C9EF7BD-2436-44B7-8407-84352BC29937")
IVMR9PresenterCallback : public IUnknown
{
    STDMETHOD(PresentSurfaceCB)(IDirect3DSurface9* pSurface) = 0;
};

MIDL_INTERFACE("AFCDB40E-CFE7-4F77-9222-21D9303187CB")
IVMR9PresenterRegisterCallback : public IUnknown
{
    STDMETHOD(RegisterCallback)(IVMR9PresenterCallback* pCallback) = 0;
};
