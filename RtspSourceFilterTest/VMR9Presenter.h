#pragma once

#include <windows.h>
#include <dshow.h>
#include <d3d9.h>
#include <vmr9.h>

static const GUID CLSID_CustomVMR9Presenter =
{ 0xac61d46, 0x465c, 0x4b1d, { 0xa9, 0xa6, 0x93, 0x8b, 0xb1, 0x6, 0x40, 0xe1 } };

MIDL_INTERFACE("6C9EF7BD-2436-44B7-8407-84352BC29937")
IVMR9PresenterCallback : public IUnknown
{
    STDMETHOD(PresentSurfaceCB)(IDirect3DSurface9 *pSurface) = 0;
};

MIDL_INTERFACE("AFCDB40E-CFE7-4F77-9222-21D9303187CB")
IVMR9PresenterRegisterCallback : public IUnknown
{
    STDMETHOD(RegisterCallback)(IVMR9PresenterCallback* pCallback) = 0;
};

class VMR9Presenter : public IVMR9PresenterCallback
{
public:
    VMR9Presenter() : _surface(nullptr), _refCount(1) {}

    virtual HRESULT STDMETHODCALLTYPE PresentSurfaceCB(IDirect3DSurface9* pSurface)
    {
        if (_surface != pSurface)
        {
            D3DSURFACE_DESC desc;
            pSurface->GetDesc(&desc);
            fprintf(stderr, "New surface (%dx%d)!\n", desc.Width, desc.Height);
        }

        _surface = pSurface;
        fprintf(stderr, "New frame\n");
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** pvObject)
    {
        if (pvObject == NULL) return E_POINTER;

        if (riid == __uuidof(IVMR9PresenterCallback))
        {
            *pvObject = static_cast<IVMR9PresenterCallback*>(this);
        }
        else if (riid == IID_IUnknown)
        {
            *pvObject = static_cast<IUnknown*>(this);
        }
        else
        {
            *pvObject = NULL;
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    virtual ULONG STDMETHODCALLTYPE AddRef(void)
    {
        return _InterlockedIncrement(&_refCount);
    }

    virtual ULONG STDMETHODCALLTYPE Release(void)
    {
        long ref = _InterlockedDecrement(&_refCount);
        if (ref == 0) delete this;
        return ref;
    }

private:
    IDirect3DSurface9* _surface;
    long _refCount;
};