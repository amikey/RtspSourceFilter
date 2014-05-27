#include <Windows.h>

#include <tchar.h>
#include <dshow.h>
#include <atlbase.h>

#include <cstdint>
#include <string>
#include <iostream>

#include "LAVVideoSettings.h"
#include "EVRPresenter.h"
#include "VMR9Presenter.h"

// {AF645432-7263-49C1-9FA3-E6DA0B346EAB}
static const GUID CLSID_RtspSourceFilter = 
{ 0xaf645432, 0x7263, 0x49c1, { 0x9f, 0xa3, 0xe6, 0xda, 0xb, 0x34, 0x6e, 0xab } };

// {EE30215D-164F-4A92-A4EB-9D4C13390F9F}
static const GUID CLSID_LAVVideo =
{ 0xEE30215D, 0x164F, 0x4A92, { 0xA4, 0xEB, 0x9D, 0x4C, 0x13, 0x39, 0x0F, 0x9F } };

MIDL_INTERFACE("C4D310F4-160D-408D-9A60-3C6275E2D3B2")
IRtspSourceConfig : public IUnknown
{
    STDMETHOD_(void, SetInitialSeekTime(DOUBLE secs)) = 0;
    STDMETHOD_(void, SetStreamingOverTcp(BOOL streamOverTcp)) = 0;
    STDMETHOD_(void, SetTunnelingOverHttpPort(WORD tunnelOverHttpPort)) = 0;
    STDMETHOD_(void, SetAutoReconnectionPeriod(DWORD dwMSecs)) = 0;
    STDMETHOD_(void, SetLatency(DWORD dwMSecs)) = 0;
};

#define BREAK_FAIL(x) if (FAILED(hr = (x))) break;
#define USE_EVR

int main()
{
    HRESULT hr;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    do
    {
        CComPtr<IBaseFilter> pRtspSource;
        BREAK_FAIL(pRtspSource.CoCreateInstance(CLSID_RtspSourceFilter));

        CComQIPtr<IRtspSourceConfig> pRtspConfig = pRtspSource;
        if (pRtspConfig)
        {
            //pRtspConfig->SetStreamingOverTcp(TRUE);
            //pRtspConfig->SetTunnelingOverHttpPort(80);
            //pRtspConfig->SetInitialSeekTime(50.0);
            pRtspConfig->SetLatency(500);
            pRtspConfig->SetAutoReconnectionPeriod(5000);
        }

        CComQIPtr<IFileSourceFilter> fileRtspSource = pRtspSource;
        if (fileRtspSource)
        {
            BREAK_FAIL(fileRtspSource->Load(//L"rtsp://localhost:8554/live",
                                            L"rtsp://184.72.239.149/vod/mp4:BigBuckBunny_115k.mov",
                                            nullptr));
        }
        else
        {
            break;
        }

        CComPtr<IBaseFilter> pDecoder;
        BREAK_FAIL(pDecoder.CoCreateInstance(CLSID_LAVVideo));

        CComQIPtr<ILAVVideoSettings> settings = pDecoder;
        if (settings)
        {
            settings->SetRuntimeConfig(TRUE);
            settings->SetNumThreads(1);
            settings->SetHWAccel(HWAccel_DXVA2Native);
        }

#ifndef USE_EVR
        CComPtr<IBaseFilter> pVideoRenderer;
        BREAK_FAIL(pVideoRenderer.CoCreateInstance(CLSID_VideoMixingRenderer9));

        CComQIPtr<IVMRFilterConfig9> pFilterConfig = pVideoRenderer;
        if (pFilterConfig == NULL)
        {
            hr = E_NOINTERFACE;
            break;
        }

        pFilterConfig->SetRenderingMode(VMR9Mode_Renderless);
        pFilterConfig->SetNumberOfStreams(1);

        CComQIPtr<IVMRSurfaceAllocatorNotify9> pSurfaceAllocatorNotify = pVideoRenderer;
        if (pSurfaceAllocatorNotify == NULL)
        {
            hr = E_NOINTERFACE;
            break;
        }

        CComPtr<IVMRSurfaceAllocator9> pCustomVmrPresenter;
        BREAK_FAIL(pCustomVmrPresenter.CoCreateInstance(CLSID_CustomVMR9Presenter));

        BREAK_FAIL(pSurfaceAllocatorNotify->AdviseSurfaceAllocator(0xACDCACDC, pCustomVmrPresenter));
        BREAK_FAIL(pCustomVmrPresenter->AdviseNotify(pSurfaceAllocatorNotify));

        CComPtr<VMR9Presenter> presenter;
        presenter.Attach(new VMR9Presenter());
        CComQIPtr<IVMR9PresenterRegisterCallback> registerCb = pCustomVmrPresenter;
        registerCb->RegisterCallback(presenter);

#else
        CComPtr<EVRPresenter> presenter;
        presenter.Attach(new EVRPresenter());

        CComPtr<IMFVideoPresenter> pCustomEvrPresenter;
        BREAK_FAIL(pCustomEvrPresenter.CoCreateInstance(CLSID_CustomEVRPresenter));

        CComQIPtr<IMFVideoDisplayControl> displayControl = pCustomEvrPresenter;
        displayControl->SetVideoWindow(GetDesktopWindow());

        CComQIPtr<IEVRPresenterRegisterCallback> registerCb = pCustomEvrPresenter;
        registerCb->RegisterCallback(presenter);
        CComQIPtr<IEVRPresenterSettings> evrSettings = pCustomEvrPresenter;
        evrSettings->SetBufferCount(3);

        CComPtr<IBaseFilter> pVideoRenderer;
        BREAK_FAIL(pVideoRenderer.CoCreateInstance(CLSID_EnhancedVideoRenderer));
        CComQIPtr<IMFVideoRenderer> pEvrPresenter = pVideoRenderer;
        BREAK_FAIL(pEvrPresenter->InitializeRenderer(nullptr, pCustomEvrPresenter));
#endif

        CComPtr<IGraphBuilder> pGraph;
        BREAK_FAIL(pGraph.CoCreateInstance(CLSID_FilterGraph));

        CComPtr<IBaseFilter> pAudioDevice;
        BREAK_FAIL(pAudioDevice.CoCreateInstance(CLSID_DSoundRender));

        pGraph->AddFilter(pRtspSource, L"livestream");
        pGraph->AddFilter(pDecoder, L"H.264 Video Decoder");
        pGraph->AddFilter(pVideoRenderer, L"Video Renderer");
        pGraph->AddFilter(pAudioDevice, L"DirectSound Device");

        CComPtr<ICaptureGraphBuilder2> pBuilder;
        BREAK_FAIL(pBuilder.CoCreateInstance(CLSID_CaptureGraphBuilder2));
        BREAK_FAIL(pBuilder->SetFiltergraph(pGraph));
        BREAK_FAIL(pBuilder->RenderStream(nullptr, &MEDIATYPE_Video, pRtspSource, pDecoder, pVideoRenderer));
        BREAK_FAIL(pBuilder->RenderStream(nullptr, &MEDIATYPE_Audio, pRtspSource, nullptr, pAudioDevice));

        CComQIPtr<IMediaControl> pMediaControl = pGraph;
        CComQIPtr<IMediaEvent> pMediaEvent = pGraph;

        if (!pMediaControl || !pMediaEvent)
            break;

        CComQIPtr<ILAVVideoStatus> status = pDecoder;
        if (status)
            fprintf(stderr, "Decoder name: %S\n", status->GetActiveDecoderName());

        BREAK_FAIL(pMediaControl->Run());
        //LONG evCode;
        //pMediaEvent->WaitForCompletion(INFINITE, &evCode);
        MessageBoxA(NULL, "Blocking", "Blocking", MB_OK);

        pMediaControl->Stop();
    } while (0);

    if (FAILED(hr))
        fprintf(stderr, "Error: 0x%08x\n", hr);

    CoUninitialize();
}