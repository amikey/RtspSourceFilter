#pragma once

#include "VMR9Presenter.h"

#include <vector>
#include "common/critsec.h"

using namespace MediaFoundationSamples;

class VMR9CustomPresenter : public IVMRSurfaceAllocator9, 
                            public IVMRImagePresenter9,
                            public IVMR9PresenterRegisterCallback
{
public:
    static HRESULT CreateInstance(IUnknown *pUnkOuter, REFIID iid, void **ppv);

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    // IVMRSurfaceAllocator9
    STDMETHOD(InitializeDevice)(DWORD_PTR dwUserID, VMR9AllocationInfo* lpAllocInfo, DWORD* lpNumBuffers);
    STDMETHOD(TerminateDevice)(DWORD_PTR dwID);
    STDMETHOD(GetSurface)(DWORD_PTR dwUserID, DWORD SurfaceIndex, DWORD SurfaceFlags, IDirect3DSurface9** lplpSurface);
    STDMETHOD(AdviseNotify)(IVMRSurfaceAllocatorNotify9* lpIVMRSurfAllocNotify);

    // IVMRImagePresenter9
    STDMETHOD(StartPresenting)(DWORD_PTR dwUserID);
    STDMETHOD(StopPresenting)(DWORD_PTR dwUserID);
    STDMETHOD(PresentImage)(DWORD_PTR dwUserID, VMR9PresentationInfo* lpPresInfo);

    // IVMR9PresenterRegisterCallback
    STDMETHOD(IVMR9PresenterRegisterCallback::RegisterCallback)(IVMR9PresenterCallback* pCallback);

protected:
    VMR9CustomPresenter(HRESULT& hr);
    virtual ~VMR9CustomPresenter();

    HRESULT InitializeD3D();
    HRESULT CreateD3DDevice();

private:
    CritSec                          m_ObjectLock;
    long                             m_RefCount;
    IVMR9PresenterCallback*          m_pPresentCb;

    IDirect3D9*                      m_pD3D9;
    IDirect3DDevice9*                m_pDevice;
    IVMRSurfaceAllocatorNotify9*     m_pSurfAllocNotify;
    std::vector<IDirect3DSurface9*>  m_Surfaces;
    bool                             m_D3D9Ex;
};