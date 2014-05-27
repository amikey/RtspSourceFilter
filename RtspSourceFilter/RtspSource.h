#pragma once

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#include <Windows.h>
#include <strsafe.h>
#include <streams.h>
#include <source.h>

// Disable deprecated warnings
#pragma warning(push)
#pragma warning(disable : 4995)
#include <string>
#include <thread>
#include <memory>
#pragma warning(pop)

#include "ConcurrentQueue.h"
#include "RtspAsyncRequest.h"
#include "MediaPacketSample.h"
#include "RtspSourceFilter.h"

#include "Debug.h"

class RtspSourceFilter : public CSource,
                         public IFileSourceFilter,
                         public IAMFilterMiscFlags,
                         public IRtspSourceConfig
{
public:
    static CUnknown* WINAPI CreateInstance(IUnknown* pUnk, HRESULT* phr);

    RtspSourceFilter(const RtspSourceFilter&) = delete;
    RtspSourceFilter& operator=(const RtspSourceFilter&) = delete;

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;

    // IAMFilterMiscFlags
    STDMETHODIMP_(ULONG) GetMiscFlags() override { return AM_FILTER_MISC_FLAGS_IS_SOURCE; }

    // IFileSourceFilter
    STDMETHODIMP GetCurFile(LPOLESTR* outFileName, AM_MEDIA_TYPE* outMediaType) override;
    STDMETHODIMP Load(LPCOLESTR inFileName, const AM_MEDIA_TYPE* inMediaType) override;

    STDMETHODIMP GetState(DWORD dwMSecs, __out FILTER_STATE* State) override;

    // CBaseFilter
    STDMETHODIMP Stop() override;
    STDMETHODIMP Pause() override;
    STDMETHODIMP Run(REFERENCE_TIME tStart) override;

    // IRtspSourceConfig
    STDMETHODIMP_(void) SetInitialSeekTime(DOUBLE secs);
    STDMETHODIMP_(void) SetStreamingOverTcp(BOOL streamOverTcp);
    STDMETHODIMP_(void) SetTunnelingOverHttpPort(WORD tunnelOverHttpPort);
    STDMETHODIMP_(void) SetAutoReconnectionPeriod(DWORD dwMSecs);
    STDMETHODIMP_(void) SetLatency(DWORD dwMSecs);

    DECLARE_IUNKNOWN

private:
    // Also a forward declaration
    friend class RtspSourcePin;

    RtspSourceFilter(IUnknown* pUnk, HRESULT* phr);
    virtual ~RtspSourceFilter();

    RtspAsyncResult AsyncOpenUrl(const std::string& url);
    RtspAsyncResult AsyncPlay();
    RtspAsyncResult AsyncShutdown();
    RtspAsyncResult AsyncReconnect();

    RtspAsyncResult MakeRequest(RtspAsyncRequest::Type request, const std::string& requestData);

    void OpenUrl(const std::string& url);
    void Play();
    void Shutdown();
    void Reconnect();
    void CloseSession();
    void CloseClient();
    void SetupSubsession();
    bool ScheduleNextReconnect();
    void DescribeRequestTimeout();
    void UnscheduleAllDelayedTasks();

    // Thin proxies for real handlers
    static void HandleOptionsResponse(RTSPClient* client, int resultCode, char* resultString);
    static void HandleDescribeResponse(RTSPClient* client, int resultCode, char* resultString);
    static void HandleSetupResponse(RTSPClient* client, int resultCode, char* resultString);
    static void HandlePlayResponse(RTSPClient* client, int resultCode, char* resultString);

    // Called when a stream's subsession (e.g., audio or video substream) ends
    static void HandleSubsessionFinished(void* clientData);
    // Called when a RTCP "BYE" is received for a subsession
    static void HandleSubsessionByeHandler(void* clientData);
    // Called at the end of a stream's expected duration
    // (if the stream has not already signaled its end using a RTCP "BYE")
    static void CheckInterPacketGaps(void* clientData);
    static void Reconnect(void* clientData);
    static void DescribeRequestTimeout(void* clientData);
    static void SendLivenessCommand(void* clientData);
    static void HandleMediaEnded(void* clientData);

    // "Real" handlers
    void HandleOptionsResponse(int resultCode, char* resultString);
    void HandleDescribeResponse(int resultCode, char* resultString);
    void HandleSetupResponse(int resultCode, char* resultString);
    void HandlePlayResponse(int resultCode, char* resultString);
    void CheckInterPacketGaps();

    void WorkerThread();

private:
    std::unique_ptr<RtspSourcePin> _videoPin;
    std::unique_ptr<RtspSourcePin> _audioPin;
    MediaPacketQueue _videoMediaQueue;
    MediaPacketQueue _audioMediaQueue;

    bool _streamOverTcp;
    uint16_t _tunnelOverHttpPort;
    unsigned _autoReconnectionMSecs;
    std::mutex _criticalSection;
    uint32_t _latencyMSecs;

    // live555 stuff
    enum class State
    {
        Initial,
        SettingUp,
        ReadyToPlay,
        Playing,
        Reconnecting
    };
    State _state;

    struct env_deleter { void operator()(MyUsageEnvironment* ptr) const { ptr->reclaim(); } };
    std::unique_ptr<BasicTaskScheduler0> _scheduler;
    std::unique_ptr<MyUsageEnvironment, env_deleter> _env;

    Authenticator _authenticator;
    std::string _rtspUrl;

    unsigned _sessionTimeout;
    unsigned _totNumPacketsReceived;
    TaskToken _interPacketGapCheckTimerTask;
    TaskToken _reconnectionTimerTask;
    TaskToken _firstCallTimeoutTask;
    TaskToken _livenessCommandTask;
    TaskToken _sessionTimerTask;

    class RtspClient* _rtsp;
    int _numSubsessions;

    double _sessionDuration;
    double _initialSeekTime;
    double _endTime;

    ConcurrentQueue<RtspAsyncRequest> _requestQueue;
    RtspAsyncRequest _currentRequest;
    std::thread _workerThread;
};

class RtspSourcePin : public CSourceStream
{
public:
    RtspSourcePin(HRESULT* phr, CSource* pFilter, 
        MediaSubsession* mediaSubsession, 
        MediaPacketQueue& mediaPacketQueue);
    virtual ~RtspSourcePin();

    HRESULT DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pRequest) override;
    HRESULT FillBuffer(IMediaSample* pSample) override;

    // Override the version that offers exactly one media type
    HRESULT GetMediaType(CMediaType* pMediaType) override;

    STDMETHODIMP Notify(IBaseFilter* pSelf, Quality q) override { return E_FAIL; }

    void ResetTimeBaselines();
    REFERENCE_TIME CurrentPlayTime() const { return _currentPlayTime; }

    void ResetMediaSubsession(MediaSubsession* mediaSubsession)
    { /* No thread-safe! */ _mediaSubsession = mediaSubsession; }

protected:
    HRESULT OnThreadCreate() override;
    HRESULT OnThreadDestroy() override;
    HRESULT OnThreadStartPlay() override;

private:
    HRESULT InitializeMediaType();
    REFERENCE_TIME SynchronizeTimestamp(const MediaPacketSample& mediaSample);

private:
    MediaSubsession* _mediaSubsession;
    MediaPacketQueue& _mediaPacketQueue;
    CMediaType _mediaType;
    DWORD _codecFourCC;

    REFERENCE_TIME _currentPlayTime;
    REFERENCE_TIME _rtpPresentationTimeBaseline;
    REFERENCE_TIME _streamTimeBaseline;
    bool _firstSample;
    bool _rtcpSynced;
};