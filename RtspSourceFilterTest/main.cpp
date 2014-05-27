#include <Windows.h>

#include <tchar.h>
#include <dshow.h>
#include <atlbase.h>

#include <cstdint>
#include <string>
#include <iostream>

#include <thread>

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

        CComPtr<IBaseFilter> pVideoRenderer;

#ifndef USE_EVR
        BREAK_FAIL(pVideoRenderer.CoCreateInstance(CLSID_VideoMixingRenderer9));

        CComQIPtr<IVMRFilterConfig9> pFilterConfig = pVideoRenderer;
        if (pFilterConfig == NULL) { hr = E_NOINTERFACE; break; }

        BREAK_FAIL(pFilterConfig->SetRenderingMode(VMR9Mode_Renderless));
        BREAK_FAIL(pFilterConfig->SetNumberOfStreams(1));

        CComPtr<IVMRSurfaceAllocator9> pCustomVmrPresenter;
        BREAK_FAIL(pCustomVmrPresenter.CoCreateInstance(CLSID_CustomVMR9Presenter));

        CComQIPtr<IVMRSurfaceAllocatorNotify9> pSurfaceAllocatorNotify = pVideoRenderer;
        if (pSurfaceAllocatorNotify == NULL) { hr = E_NOINTERFACE; break; }
        BREAK_FAIL(pSurfaceAllocatorNotify->AdviseSurfaceAllocator(0xACDCACDC, pCustomVmrPresenter));
        BREAK_FAIL(pCustomVmrPresenter->AdviseNotify(pSurfaceAllocatorNotify));

        CComPtr<VMR9Presenter> presenter;
        presenter.Attach(new VMR9Presenter());
        CComQIPtr<IVMR9PresenterRegisterCallback> registerCb = pCustomVmrPresenter;
        if (registerCb == NULL) { hr = E_NOINTERFACE; break; }
        BREAK_FAIL(registerCb->RegisterCallback(presenter));

#else
        BREAK_FAIL(pVideoRenderer.CoCreateInstance(CLSID_EnhancedVideoRenderer));

        CComPtr<IMFVideoPresenter> pCustomEvrPresenter;
        BREAK_FAIL(pCustomEvrPresenter.CoCreateInstance(CLSID_CustomEVRPresenter));

        CComQIPtr<IMFVideoDisplayControl> displayControl = pCustomEvrPresenter;
        if (displayControl == NULL) { hr = E_NOINTERFACE; break; }
        BREAK_FAIL(displayControl->SetVideoWindow(GetDesktopWindow()));

        CComQIPtr<IMFVideoRenderer> pEvrPresenter = pVideoRenderer;
        if (pEvrPresenter == NULL) { hr = E_NOINTERFACE; break; }
        BREAK_FAIL(pEvrPresenter->InitializeRenderer(nullptr, pCustomEvrPresenter));

        CComPtr<EVRPresenter> presenter;
        presenter.Attach(new EVRPresenter());
        CComQIPtr<IEVRPresenterRegisterCallback> registerCb = pCustomEvrPresenter;
        if (registerCb == NULL) { hr = E_NOINTERFACE; break; }
        BREAK_FAIL(registerCb->RegisterCallback(presenter));
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

        //MessageBoxA(NULL, "Blocking", "Blocking", MB_OK);

        HANDLE hManualRequest = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        HANDLE hMediaEvent;
        pMediaEvent->GetEventHandle((OAEVENT*)&hMediaEvent);

        // Stop streaming from different thread using event for manual request
        std::thread th([&] {
            std::this_thread::sleep_for(std::chrono::seconds(15));
            SetEvent(hManualRequest);
        });
        th.detach();

        while (true)
        {
            HANDLE handles[] = { hMediaEvent, hManualRequest };
            DWORD dwRes = WaitForMultipleObjects(sizeof(handles) / sizeof(handles[0]), handles, FALSE, INFINITE);
            if (dwRes == WAIT_OBJECT_0)
            {
                // Media event from filter graph
                LONG evCode, param1, param2;
                pMediaEvent->GetEvent(&evCode, &param1, &param2, 0);
                // Ignore it
                pMediaEvent->FreeEventParams(evCode, param1, param2);
            }
            else // if (dwRes == WAIT_OBJECT_0 + 1)
            {
                // Manual request from "the outside"
                // Could also get a command from command queue
                break;
            }
        }

        pMediaControl->Stop();
    } 
    while (0);

    if (FAILED(hr))
        fprintf(stderr, "Error: 0x%08x\n", hr);

    CoUninitialize();
}