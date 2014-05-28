#include "Presenter.h"

#include <cassert>

namespace
{
    #ifndef SAFE_RELEASE
    template <class T>
    inline void SAFE_RELEASE(T*& p)
    {
        if (p)
        {
            p->Release();
            p = NULL;
        }
    }
    #endif
}

#define BREAK_FAIL(x) if (FAILED(hr = (x))) { break; }

HRESULT VMR9CustomPresenter::CreateInstance(IUnknown *pUnkOuter, REFIID iid, void **ppv)
{
    if (ppv == NULL) return E_POINTER;

    // This object does not support aggregation.
    if (pUnkOuter != NULL)
    {
        return CLASS_E_NOAGGREGATION;
    }

    HRESULT hr = S_OK;

    VMR9CustomPresenter* pObject = new VMR9CustomPresenter(hr);

    if (pObject == NULL)
    {
        hr = E_OUTOFMEMORY;
    }
    
    if (FAILED(hr))
    {
        SAFE_RELEASE(pObject);
        return hr;
    }

    hr = pObject->QueryInterface(iid, ppv);

    SAFE_RELEASE(pObject);
    return hr;
}

VMR9CustomPresenter::VMR9CustomPresenter(HRESULT& hr)
    : m_RefCount(1)
    , m_pPresentCb(NULL)
    , m_pD3D9(NULL)
    , m_pDevice(NULL)
    , m_pSurfAllocNotify(NULL)
    , m_D3D9Ex(false)
{
    hr = InitializeD3D();

    if (SUCCEEDED(hr))
    {
        hr = CreateD3DDevice();
    }
}

VMR9CustomPresenter::~VMR9CustomPresenter()
{
    for (size_t i = 0; i < m_Surfaces.size(); ++i)
        SAFE_RELEASE(m_Surfaces[i]);
    m_Surfaces.clear();

    SAFE_RELEASE(m_pDevice);
    SAFE_RELEASE(m_pD3D9);
    SAFE_RELEASE(m_pPresentCb);
}

HRESULT VMR9CustomPresenter::InitializeD3D()
{
    assert(m_pD3D9 == NULL);
    typedef HRESULT (WINAPI *pfnDirect3DCreate9Ex)(UINT SDKVersion, IDirect3D9Ex**);

    HRESULT hr = S_OK;

    HMODULE libHandle = LoadLibraryA("d3d9.dll");
    if (!libHandle)
    {
        return E_FAIL;
    }

    pfnDirect3DCreate9Ex Direct3DCreate9Ex = (pfnDirect3DCreate9Ex) GetProcAddress(libHandle, "Direct3DCreate9Ex");
    if (Direct3DCreate9Ex != NULL)
    {
        IDirect3D9Ex* pD3D9Ex = NULL;
        hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D9Ex);
        if (FAILED(hr))
        {
            FreeLibrary(libHandle);
            return hr;
        }

        m_pD3D9 = pD3D9Ex;
        m_D3D9Ex = true;
    }
    else
    {
        m_pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
        if (m_pD3D9 == NULL)
        {
            hr = E_FAIL;
        }
        m_D3D9Ex = false;
    }

    FreeLibrary(libHandle);

    return hr;
}

HRESULT VMR9CustomPresenter::CreateD3DDevice()
{
    AutoLock lock(m_ObjectLock);

    if (!m_pD3D9)
    {
        return E_FAIL;;
    }

    HRESULT hr = S_OK;
    IDirect3DDevice9* pDevice = NULL;

    do
    {
        HWND hwnd = GetDesktopWindow();

        D3DPRESENT_PARAMETERS pp = { 0 };

        pp.BackBufferWidth = 1;
        pp.BackBufferHeight = 1;
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_COPY;
        pp.BackBufferFormat = D3DFMT_UNKNOWN;
        pp.hDeviceWindow = hwnd;
        pp.Flags = D3DPRESENTFLAG_VIDEO;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

        D3DCAPS9 ddCaps;
        BREAK_FAIL(m_pD3D9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &ddCaps));

        DWORD vp = D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE;

        if (ddCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
        {
            vp |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
        }
        else
        {
            vp |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        }

        if (m_D3D9Ex)
        {
            IDirect3D9Ex* pD3D9Ex = static_cast<IDirect3D9Ex*>(m_pD3D9);
            IDirect3DDevice9Ex* pDeviceEx = NULL;

            BREAK_FAIL(pD3D9Ex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, pp.hDeviceWindow,
                vp, &pp, NULL, &pDeviceEx));

            SAFE_RELEASE(m_pDevice);
            pDeviceEx->QueryInterface(__uuidof(IDirect3DDevice9), (void**) &m_pDevice);
            SAFE_RELEASE(pDeviceEx);
        }
        else
        {
            IDirect3DDevice9* pDevice = NULL;
            m_pD3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, pp.hDeviceWindow, vp, &pp, &pDevice);

            SAFE_RELEASE(m_pDevice);
            m_pDevice = pDevice;
        }

    } while (0);

    if (FAILED(hr))
    {
        SAFE_RELEASE(pDevice);
    }

    return hr;
}

HRESULT VMR9CustomPresenter::InitializeDevice(DWORD_PTR dwUserID, VMR9AllocationInfo* lpAllocInfo, DWORD* lpNumBuffers)
{
    if (lpNumBuffers == NULL) return E_POINTER;
    if (lpAllocInfo == NULL) return E_POINTER;
    if (m_pSurfAllocNotify == NULL) return E_FAIL;

    HRESULT hr = S_OK;

    for (size_t i = 0; i < m_Surfaces.size(); ++i)
        SAFE_RELEASE(m_Surfaces[i]);
    m_Surfaces.resize(*lpNumBuffers);

    lpAllocInfo->Format = D3DFMT_X8R8G8B8;
    hr = m_pSurfAllocNotify->AllocateSurfaceHelper(lpAllocInfo, lpNumBuffers, &m_Surfaces[0]);

    return hr;
}

HRESULT VMR9CustomPresenter::TerminateDevice(DWORD_PTR dwID)
{
    AutoLock lock(m_ObjectLock);

    for (size_t i = 0; i < m_Surfaces.size(); ++i)
        SAFE_RELEASE(m_Surfaces[i]);
    m_Surfaces.clear();

    return S_OK;
}

HRESULT VMR9CustomPresenter::GetSurface(DWORD_PTR dwUserID, DWORD SurfaceIndex, DWORD SurfaceFlags, IDirect3DSurface9** lplpSurface)
{
    if (lplpSurface == NULL) return E_POINTER;
    if (SurfaceIndex >= m_Surfaces.size()) return E_FAIL;

    AutoLock lock(m_ObjectLock);

    *lplpSurface = m_Surfaces[SurfaceIndex];
    (*lplpSurface)->AddRef();

    return S_OK;
}

HRESULT VMR9CustomPresenter::AdviseNotify(IVMRSurfaceAllocatorNotify9* lpIVMRSurfAllocNotify)
{
    if (lpIVMRSurfAllocNotify == NULL) return E_POINTER;

    AutoLock lock(m_ObjectLock);
    m_pSurfAllocNotify = lpIVMRSurfAllocNotify;
    HMONITOR hMonitor = m_pD3D9->GetAdapterMonitor(D3DADAPTER_DEFAULT);
    HRESULT hr = m_pSurfAllocNotify->SetD3DDevice(m_pDevice, hMonitor);
    return hr;
}

HRESULT VMR9CustomPresenter::StartPresenting(DWORD_PTR dwUserID)
{
    AutoLock lock(m_ObjectLock);
    return m_pDevice == NULL ? E_FAIL : S_OK;
}

HRESULT VMR9CustomPresenter::StopPresenting(DWORD_PTR dwUserID)
{
    return S_OK;
}

HRESULT VMR9CustomPresenter::PresentImage(DWORD_PTR dwUserID, VMR9PresentationInfo* lpPresInfo)
{
    AutoLock lock(m_ObjectLock);

    /// TODO: Check device lost
    if (!m_D3D9Ex)
    {
        HRESULT hr = m_pDevice->TestCooperativeLevel();
        if (FAILED(hr))
        {
            char buf[128];
            sprintf(buf, "HR=0x%08X\n", hr);
            OutputDebugStringA(buf);
        }
    }
    else
    {
        IDirect3DDevice9Ex* pDeviceEx = (IDirect3DDevice9Ex*) m_pDevice;
        HRESULT hr = pDeviceEx->CheckDeviceState(NULL);
        if (FAILED(hr))
        {
            char buf[128];
            sprintf(buf, "HR=0x%08X\n", hr);
            OutputDebugStringA(buf);
        }
    }

    if (m_pPresentCb != NULL)
        m_pPresentCb->PresentSurfaceCB(lpPresInfo->lpSurf);

    return S_OK;
}

HRESULT VMR9CustomPresenter::RegisterCallback(IVMR9PresenterCallback* pCallback)
{
    SAFE_RELEASE(m_pPresentCb);
    m_pPresentCb = pCallback;
    if (m_pPresentCb != NULL)
        m_pPresentCb->AddRef();
    return S_OK;
}

HRESULT VMR9CustomPresenter::QueryInterface(REFIID riid, void** ppvObject)
{
    if (ppvObject == NULL) return E_POINTER;

    if (riid == IID_IVMRSurfaceAllocator9)
    {
        *ppvObject = static_cast<IVMRSurfaceAllocator9*>(this);
    }
    else if (riid == IID_IVMRImagePresenter9)
    {
        *ppvObject = static_cast<IVMRImagePresenter9*>(this);
    }
    else if (riid == __uuidof(IVMR9PresenterRegisterCallback))
    {
        *ppvObject = static_cast<IVMR9PresenterRegisterCallback*>(this);
    }
    else if (riid == IID_IUnknown)
    {
        *ppvObject = static_cast<IUnknown*>(static_cast<IVMRSurfaceAllocator9*>(this));
    }
    else
    {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

ULONG VMR9CustomPresenter::AddRef()
{
    return _InterlockedIncrement(&m_RefCount);
}

ULONG VMR9CustomPresenter::Release()
{
    ULONG ret = _InterlockedDecrement(&m_RefCount);
    if (ret == 0)
    {
        delete this;
    }
    return ret;
}
