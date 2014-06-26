#include <Windows.h>
#include <comdef.h>

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

#define USE_EVR

_COM_SMARTPTR_TYPEDEF(IBaseFilter, __uuidof(IBaseFilter));
_COM_SMARTPTR_TYPEDEF(IRtspSourceConfig, __uuidof(IRtspSourceConfig));
_COM_SMARTPTR_TYPEDEF(IFileSourceFilter, __uuidof(IFileSourceFilter));
_COM_SMARTPTR_TYPEDEF(ILAVVideoSettings, __uuidof(ILAVVideoSettings));
_COM_SMARTPTR_TYPEDEF(IGraphBuilder, __uuidof(IGraphBuilder));
_COM_SMARTPTR_TYPEDEF(IVMRFilterConfig9, __uuidof(IVMRFilterConfig9));
_COM_SMARTPTR_TYPEDEF(IVMRSurfaceAllocatorNotify9, __uuidof(IVMRSurfaceAllocatorNotify9));
_COM_SMARTPTR_TYPEDEF(IVMRSurfaceAllocator9, __uuidof(IVMRSurfaceAllocator9));
_COM_SMARTPTR_TYPEDEF(IVMR9PresenterRegisterCallback, __uuidof(IVMR9PresenterRegisterCallback));
_COM_SMARTPTR_TYPEDEF(VMR9Presenter, __uuidof(VMR9Presenter));
_COM_SMARTPTR_TYPEDEF(IMFVideoPresenter, __uuidof(IMFVideoPresenter));
_COM_SMARTPTR_TYPEDEF(IMFVideoDisplayControl, __uuidof(IMFVideoDisplayControl));
_COM_SMARTPTR_TYPEDEF(IMFVideoRenderer, __uuidof(IMFVideoRenderer));
_COM_SMARTPTR_TYPEDEF(EVRPresenter, __uuidof(EVRPresenter));
_COM_SMARTPTR_TYPEDEF(IEVRPresenterRegisterCallback, __uuidof(IEVRPresenterRegisterCallback));
_COM_SMARTPTR_TYPEDEF(IGraphBuilder, __uuidof(IGraphBuilder));
_COM_SMARTPTR_TYPEDEF(ICaptureGraphBuilder2, __uuidof(ICaptureGraphBuilder2));
_COM_SMARTPTR_TYPEDEF(IMediaControl, __uuidof(IMediaControl));
_COM_SMARTPTR_TYPEDEF(IMediaEvent, __uuidof(IMediaEvent));
_COM_SMARTPTR_TYPEDEF(ILAVVideoStatus, __uuidof(ILAVVideoStatus));

int main()
{
    HRESULT hr;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    try
    {
        IBaseFilterPtr pRtspSource(CLSID_RtspSourceFilter);

        IRtspSourceConfigPtr pRtspConfig(pRtspSource);
        //pRtspConfig->SetStreamingOverTcp(TRUE);
        //pRtspConfig->SetTunnelingOverHttpPort(80);
        //pRtspConfig->SetInitialSeekTime(50.0);
        pRtspConfig->SetLatency(500);
        pRtspConfig->SetAutoReconnectionPeriod(5000);

        IFileSourceFilterPtr fileRtspSource(pRtspSource);
        hr = fileRtspSource->Load(L"rtsp://184.72.239.149/vod/mp4:BigBuckBunny_115k.mov", nullptr);
        if (FAILED(hr)) _com_issue_error(hr);

        IBaseFilterPtr pDecoder(CLSID_LAVVideo);

        ILAVVideoSettingsPtr settings(pDecoder);
        settings->SetRuntimeConfig(TRUE);
        settings->SetNumThreads(1);
        settings->SetHWAccel(HWAccel_DXVA2Native);

        
#ifndef USE_EVR
        IBaseFilterPtr pVideoRenderer(CLSID_VideoMixingRenderer9);

        IVMRFilterConfig9Ptr pFilterConfig(pVideoRenderer);
        hr = pFilterConfig->SetRenderingMode(VMR9Mode_Renderless);
        if (FAILED(hr)) _com_issue_error(hr);
        hr = pFilterConfig->SetNumberOfStreams(1);
        if (FAILED(hr)) _com_issue_error(hr);

        IVMRSurfaceAllocator9Ptr pCustomVmrPresenter(CLSID_CustomVMR9Presenter);
        IVMRSurfaceAllocatorNotify9Ptr pSurfaceAllocatorNotify(pVideoRenderer);
        hr = pSurfaceAllocatorNotify->AdviseSurfaceAllocator(0xACDCACDC, pCustomVmrPresenter);
        if (FAILED(hr)) _com_issue_error(hr);
        hr = pCustomVmrPresenter->AdviseNotify(pSurfaceAllocatorNotify);
        if (FAILED(hr)) _com_issue_error(hr);

        VMR9PresenterPtr presenter;
        presenter.Attach(new VMR9Presenter());
        IVMR9PresenterRegisterCallbackPtr registerCb(pCustomVmrPresenter);
        hr = registerCb->RegisterCallback(presenter);
        if (FAILED(hr)) _com_issue_error(hr);
#else
        IBaseFilterPtr pVideoRenderer(CLSID_EnhancedVideoRenderer);
        IMFVideoPresenterPtr pCustomEvrPresenter(CLSID_CustomEVRPresenter);

        IMFVideoDisplayControlPtr displayControl(pCustomEvrPresenter);
        hr = displayControl->SetVideoWindow(GetDesktopWindow());
        if (FAILED(hr)) _com_issue_error(hr);

        IMFVideoRendererPtr pEvrPresenter(pVideoRenderer);
        hr = pEvrPresenter->InitializeRenderer(nullptr, pCustomEvrPresenter);
        if (FAILED(hr)) _com_issue_error(hr);

        EVRPresenterPtr presenter;
        presenter.Attach(new EVRPresenter());
        IEVRPresenterRegisterCallbackPtr registerCb(pCustomEvrPresenter);
        hr = registerCb->RegisterCallback(presenter);
        if (FAILED(hr)) _com_issue_error(hr);
#endif

        IGraphBuilderPtr pGraph(CLSID_FilterGraph);
        IBaseFilterPtr pAudioDevice(CLSID_DSoundRender);

        pGraph->AddFilter(pRtspSource, L"livestream");
        pGraph->AddFilter(pDecoder, L"H.264 Video Decoder");
        pGraph->AddFilter(pVideoRenderer, L"Video Renderer");
        pGraph->AddFilter(pAudioDevice, L"DirectSound Device");

        ICaptureGraphBuilder2Ptr pBuilder(CLSID_CaptureGraphBuilder2);
        hr = pBuilder->SetFiltergraph(pGraph);
        if (FAILED(hr)) _com_issue_error(hr);
        hr = pBuilder->RenderStream(nullptr, &MEDIATYPE_Video, pRtspSource, pDecoder, pVideoRenderer);
        if (FAILED(hr)) _com_issue_error(hr);
        hr = pBuilder->RenderStream(nullptr, &MEDIATYPE_Audio, pRtspSource, nullptr, pAudioDevice);
        if (FAILED(hr)) _com_issue_error(hr);

        IMediaControlPtr pMediaControl(pGraph);
        IMediaEventPtr pMediaEvent(pGraph);
        ILAVVideoStatusPtr status(pDecoder);

        fprintf(stderr, "Decoder name: %S\n", status->GetActiveDecoderName());

        hr = pMediaControl->Run();
        if (FAILED(hr)) _com_issue_error(hr);

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
                LONG evCode;
                LONG_PTR param1, param2;
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

        CloseHandle(hManualRequest);

        hr = pMediaControl->Stop();
        if (FAILED(hr)) _com_issue_error(hr);
    }
    catch (_com_error& ex)
    {
        fprintf(stderr, "COM Exception! - %S\n", ex.ErrorMessage());
    }

    CoUninitialize();
}