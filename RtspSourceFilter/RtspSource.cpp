#include "RtspSource.h"
#include "RtspSourceGuids.h"
#include "RtspError.h"
#include "ProxyMediaSink.h"
#include "GroupsockHelper.hh"
#include "Debug.h"

#include <new>
#include <Windows.h>
#include <DShow.h>

#undef min
#undef max

/*
 * In order to add support for new media format (f.e. HEVC) one needs to:
 * - appropriately modify function IsSubsessionSupported()
 * - based on given MediaSubsession initialize CMediaType (check GetMediaType{codec}() functions)
 *   and use it to RtspSourcePin::InitializeMediaType
 * - Modify RtspSourcePin::FilBuffer if needed
 */

#ifdef DEBUG 
#define RTSP_CLIENT_VERBOSITY_LEVEL 1
#else
#define RTSP_CLIENT_VERBOSITY_LEVEL 0
#endif

namespace
{
    const char* RtspClientAppName = "RtspSourceFilter";
    const int RtspClientVerbosityLevel = RTSP_CLIENT_VERBOSITY_LEVEL;
    const uint32_t defaultLatencyMSecs = 500;
    const int recvBufferVideo = 256 * 1024; // 256KB - H.264 IDR frames can be really big
    const int recvBufferAudio = 4096; // 4KB
    const int recvBufferText = 2048; // Should be more than enough
    const unsigned int packetReorderingThresholdTime = 200 * 1000; // 200 ms
    const int interPacketGapMaxTime = 2000; // 2000 msec - but effectively it's atleast twice that
    const Boolean forceMulticastOnUnspecified = False;
    const int firstCallTimeoutTime = 2000;

    bool IsSubsessionSupported(MediaSubsession& mediaSubsession);
    void SetThreadName(DWORD dwThreadID, char* threadName);
}

class RtspClient : public ::RTSPClient
{
public:
    static RtspClient* CreateRtspClient(RtspSourceFilter* filter,
        UsageEnvironment& env, char const* rtspUrl,
        int verbosityLevel = 0, char const* applicationName = nullptr,
        portNumBits tunnelOverHttpPortNum = 0)
    {
        return new (std::nothrow) RtspClient(filter, env, rtspUrl, verbosityLevel,
            applicationName, tunnelOverHttpPortNum);
    }

protected:
    RtspClient(RtspSourceFilter* filter, UsageEnvironment& env, char const* rtspUrl,
        int verbosityLevel, char const* applicationName, portNumBits tunnelOverHttpPortNum)
        : ::RTSPClient(env, rtspUrl, verbosityLevel, applicationName, tunnelOverHttpPortNum, -1)
        , filter(filter), mediaSession(nullptr), subsession(nullptr), iter(nullptr)
    {
    }

    virtual ~RtspClient()
    {
        _ASSERT(!mediaSession);
        _ASSERT(!subsession);
        _ASSERT(!iter);
    }

public:
    RtspSourceFilter* filter;
    MediaSession* mediaSession;
    MediaSubsession* subsession;
    MediaSubsessionIterator* iter;
};

CUnknown* WINAPI RtspSourceFilter::CreateInstance(LPUNKNOWN lpunk, HRESULT* phr)
{
    RtspSourceFilter* filter = new (std::nothrow) RtspSourceFilter(lpunk, phr);
    if (phr && SUCCEEDED(*phr))
    {
        if (!filter)
            *phr = E_OUTOFMEMORY;
        else
            *phr = S_OK;
    }
    else if (phr)
    {
        if (filter)
            delete filter;
        return NULL;
    }

    return filter;
}

RtspSourceFilter::RtspSourceFilter(IUnknown* pUnk, HRESULT* phr)
    : CSource(NAME("RtspSourceFilter"), pUnk, CLSID_RtspSourceFilter)
    , _streamOverTcp(false), _tunnelOverHttpPort(0U), _autoReconnectionMSecs(0)
    , _latencyMSecs(defaultLatencyMSecs), _state(State::Initial)
    , _scheduler(BasicTaskScheduler::createNew())
    , _env(MyUsageEnvironment::createNew(*_scheduler)), _totNumPacketsReceived(0)
    , _interPacketGapCheckTimerTask(nullptr), _reconnectionTimerTask(nullptr)
    , _firstCallTimeoutTask(nullptr), _livenessCommandTask(nullptr)
    , _sessionTimerTask(nullptr), _sessionTimeout(60), _rtsp(nullptr)
    , _numSubsessions(0), _sessionDuration(0), _initialSeekTime(0), _endTime(0)
    , _workerThread(&RtspSourceFilter::WorkerThread, this)
{
}

RtspSourceFilter::~RtspSourceFilter()
{
    _requestQueue.push(RtspAsyncRequest(RtspAsyncRequest::Done));
    _workerThread.join();
}

HRESULT RtspSourceFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IFileSourceFilter)
        return GetInterface((IFileSourceFilter*)this, ppv);
    else if (riid == IID_IAMFilterMiscFlags)
        return GetInterface((IAMFilterMiscFlags*)this, ppv);
    else if (riid == __uuidof(IRtspSourceConfig))
        return GetInterface((IRtspSourceConfig*)this, ppv);
    return __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT RtspSourceFilter::GetCurFile(LPOLESTR* outFileName, AM_MEDIA_TYPE* outMediaType)
{
    CheckPointer(outFileName, E_POINTER);
    *outFileName = nullptr;
    
    if (_rtspUrl.length() > 0)
    {
        _rtspUrl.c_str();
        WCHAR* pFileName = new WCHAR[_rtspUrl.length() + 1];
        size_t res = mbstowcs(pFileName, _rtspUrl.c_str(), _rtspUrl.length());
        if (res != -1)
        {
            pFileName[res] = L'\0';
        }
        else
        {
            delete[] pFileName;
            return S_OK;
        }
        DWORD n = sizeof(WCHAR) * (1 + wcslen(pFileName));
        *outFileName = (LPOLESTR)CoTaskMemAlloc(n);
        if (*outFileName != nullptr)
            CopyMemory(*outFileName, pFileName, n);
        delete[] pFileName;
    }
    // Ignore any request for media type - which one should we return anyway?
    if (outMediaType) outMediaType = nullptr;

    return S_OK;
}

HRESULT RtspSourceFilter::Load(LPCOLESTR inFileName, const AM_MEDIA_TYPE* inMediaType)
{
    // Don't allow to change url if we're already connected
    if (!_rtspUrl.empty())
        return E_FAIL;
    // Convert OLE string to std one
    size_t converted;
    errno_t err = wcstombs_s(&converted, nullptr, 0, inFileName, 0);
    if (err || converted == 0) return E_FAIL;
    _rtspUrl.resize(converted);
    wcstombs_s(&converted, const_cast<char*>(_rtspUrl.data()),
        _rtspUrl.size(), inFileName, _TRUNCATE);
    // Request new URL asynchronously but wait since we need a response now
    RtspAsyncResult result = AsyncOpenUrl(_rtspUrl);
    RtspResult ec = result.get();
    // Check if we're ready to play the media
    if (ec)
    {
        // NOTE: We don't fire auto reconnect mechanism now because it would be pretty useless.
        // Until we retrieve session description our filter is pinless
        // so we can't connect it to other filters in a graph
        (*_env) << "Error: " << ec.message().c_str() << "\n";
        _rtspUrl.clear(); // Allow the user to load different rtsp address
        return E_FAIL;
    }
    else
    {
        return S_OK;
    }
}

HRESULT RtspSourceFilter::GetState(DWORD dwMSecs, __out FILTER_STATE* State)
{
    CheckPointer(State, E_POINTER);
    *State = m_State;
    if (m_State == State_Paused)
        return VFW_S_CANT_CUE; // We're live - dont buffer anything
    return S_OK;
}

namespace
{
    const char* GetFilterStateName(FILTER_STATE fs)
    {
        switch (fs)
        {
        case State_Stopped: return "Stopped";
        case State_Paused: return "Paused";
        case State_Running: return "Running";
        }
        return "";
    }
}

HRESULT RtspSourceFilter::Stop()
{
    DebugLog("%s - state: %s\n", __FUNCTION__, GetFilterStateName(m_State));

    // Blocking call
    // Guarantees that filter is in initial state when done
    AsyncShutdown().get();

    return __super::Stop();
}

HRESULT RtspSourceFilter::Pause()
{
    DebugLog("%s - state: %s\n", __FUNCTION__, GetFilterStateName(m_State));

    if (m_State == State_Stopped)
    {
        // Ensure we won't be showing some old frames
        _videoMediaQueue.clear();
        _audioMediaQueue.clear();
    }

    return __super::Pause();
}

HRESULT RtspSourceFilter::Run(REFERENCE_TIME tStart)
{
    DebugLog("%s\n", __FUNCTION__); 

    // Need to reopen the session if we teardowned previous one
    if (_state == State::Initial)
    {
        // NOTE: We query internal state of a worker thread from different thread thus
        // this is only valid if we assume it's called as a first Run()
        // or it is called after Stop(). Both assumptions are respected if 
        // we call DirectShow from single thread.
        RtspAsyncResult result = AsyncOpenUrl(_rtspUrl);
        RtspResult ec = result.get();
        if (ec)
        {
            (*_env) << "Error: " << ec.message().c_str() << "\n";
            return E_FAIL;
        }
    }

    HRESULT hr = __super::Run(tStart);
    if (SUCCEEDED(hr))
        // Start playing asynchronously
        AsyncPlay();
    return hr;
}

void RtspSourceFilter::SetInitialSeekTime(DOUBLE secs)
{
    // Valid call only until first LoadFile call
    _initialSeekTime = secs;
}

void RtspSourceFilter::SetStreamingOverTcp(BOOL streamOverTcp)
{
    // Valid call only until first LoadFile call
    _streamOverTcp = streamOverTcp ? true : false;
}

void RtspSourceFilter::SetTunnelingOverHttpPort(WORD tunnelOverHttpPort)
{
    // Valid call only until first LoadFile call
    _tunnelOverHttpPort = tunnelOverHttpPort;
}

void RtspSourceFilter::SetAutoReconnectionPeriod(DWORD dwMSecs)
{
    // Valid before reconnection is scheduled
    _autoReconnectionMSecs = dwMSecs;
}

void RtspSourceFilter::SetLatency(DWORD dwMSecs)
{
    // Valid call only until first RTP packet arrival or after Stop/Run
    // This value is only used for first packet synchronization 
    // - either it's first RTCP synced or just the very first packet
    _latencyMSecs = dwMSecs;
}

void RtspSourceFilter::StopStreaming()
{
    // Blocking call
    // Guarantees that filter is in initial state when done
    AsyncShutdown().get();
}

RtspAsyncResult RtspSourceFilter::AsyncOpenUrl(const std::string& url)
{
    return MakeRequest(RtspAsyncRequest::Open, url);
}

RtspAsyncResult RtspSourceFilter::AsyncPlay()
{
    return MakeRequest(RtspAsyncRequest::Play, "");
}

RtspAsyncResult RtspSourceFilter::AsyncShutdown()
{
    return MakeRequest(RtspAsyncRequest::Stop, "");
}

RtspAsyncResult RtspSourceFilter::AsyncReconnect()
{
    return MakeRequest(RtspAsyncRequest::Reconnect, "");
}

RtspAsyncResult RtspSourceFilter::MakeRequest(RtspAsyncRequest::Type request, 
                                              const std::string& requestData)
{
    /// TODO: Check if worker thread is alive
    RtspAsyncRequest rtspRequest(request, requestData);
    RtspAsyncResult r(rtspRequest.GetAsyncResult());
    _requestQueue.push(std::move(rtspRequest));
    return r;
}

void RtspSourceFilter::OpenUrl(const std::string& url)
{
    // Should never fail (only when out of memory)
    _rtsp = RtspClient::CreateRtspClient(this, *_env, url.c_str(),
        RtspClientVerbosityLevel, RtspClientAppName, _tunnelOverHttpPort);
    if (!_rtsp)
    {
        _currentRequest.SetValue(error::ClientCreateFailed);
        _state = State::Initial;
        return;
    }
    _firstCallTimeoutTask = _scheduler->scheduleDelayedTask(firstCallTimeoutTime*1000,
        RtspSourceFilter::DescribeRequestTimeout, this);
    // Returns only CSeq number
    _rtsp->sendDescribeCommand(HandleDescribeResponse, &_authenticator);
}

void RtspSourceFilter::HandleDescribeResponse(RTSPClient* client, int resultCode, char* resultString)
{
    RtspClient* myClient = static_cast<RtspClient*>(client);
    myClient->filter->HandleDescribeResponse(resultCode, resultString);
}

void RtspSourceFilter::HandleDescribeResponse(int resultCode, char* resultString)
{
    // Don't need this anymore - we got a response in time
    if (_firstCallTimeoutTask != nullptr)
        _scheduler->unscheduleDelayedTask(_firstCallTimeoutTask);

    if (resultCode != 0)
    {
        delete[] resultString;

        CloseClient(); // No session to close yet
        if (ScheduleNextReconnect()) return;

        // Couldn't connect to the server
        if (resultCode == -WSAENOTCONN)
        {
            _state = State::Initial;
            _currentRequest.SetValue(error::ServerNotReachable);
        }
        else
        {
            _state = State::Initial;
            _currentRequest.SetValue(error::DescribeFailed);
        }

        return;
    }

    MediaSession* mediaSession = MediaSession::createNew(*_env, resultString);
    delete[] resultString;
    if (!mediaSession) // SDP is invalid or out of memory
    {
        CloseClient(); // No session to close to yet
        if (ScheduleNextReconnect()) return;

        _currentRequest.SetValue(error::SdpInvalid);
        _state = State::Initial;

        return;
    }
    // Sane check
    else if (!mediaSession->hasSubsessions())
    {
        // Close media session (don't wait for a response)
        _rtsp->sendTeardownCommand(*mediaSession, nullptr, &_authenticator);
        Medium::close(mediaSession);

        // Close client
        CloseClient();
        if (ScheduleNextReconnect()) return;

        _currentRequest.SetValue(error::NoSubsessions);
        _state = State::Initial;

        return;
    }

    // Start setuping media session
    MediaSubsessionIterator* iter = new MediaSubsessionIterator(*mediaSession);
    _rtsp->mediaSession = mediaSession;
    _rtsp->iter = iter;
    _numSubsessions = 0;

    SetupSubsession();
}

void RtspSourceFilter::SetupSubsession()
{
    MediaSubsessionIterator* iter = _rtsp->iter;
    MediaSubsession* subsession = iter->next();
    _rtsp->subsession = subsession;
    // There's still some subsession to be setup
    if (subsession != nullptr)
    {
        if (!IsSubsessionSupported(*subsession))
        {
            // Ignore unsupported subsessions
            SetupSubsession();
            return;
        }
        if (!subsession->initiate())
        {
            /// TODO: Ignore or quit?
            SetupSubsession();
            return;
        }

        RTPSource* rtpSource = subsession->rtpSource();
        if (rtpSource)
        {
            rtpSource->setPacketReorderingThresholdTime(packetReorderingThresholdTime);

            int recvBuffer = 0;
            if (!strcmp(subsession->mediumName(), "video"))
                recvBuffer = recvBufferVideo;
            else if (!strcmp(subsession->mediumName(), "audio"))
                recvBuffer = recvBufferAudio;

            // Increase receive buffer for rather big packets (like H.264 IDR)
            if (recvBuffer > 0 && rtpSource->RTPgs())
                ::increaseReceiveBufferTo(*_env, rtpSource->RTPgs()->socketNum(), recvBuffer);
        }

        _rtsp->sendSetupCommand(*subsession, HandleSetupResponse, False,
            _streamOverTcp, forceMulticastOnUnspecified && !_streamOverTcp, &_authenticator);
        return;
    }

    // We iterated over all available subsessions
    delete _rtsp->iter;
    _rtsp->iter = nullptr;

    // How many subsession we set up? If none then something is wrong and we shouldn't proceed further
    if (_numSubsessions == 0)
    {
        CloseSession();
        CloseClient();

        if (ScheduleNextReconnect()) return;

        _state = State::Initial;
        _currentRequest.SetValue(error::NoSubsessionsSetup);

        return;
    }

    if (_state != State::Reconnecting)
    {
        _state = State::ReadyToPlay;
        _currentRequest.SetValue(error::Success);
    }
    else
    {
        // Autostart playing if we're reconnecting
        _state = State::Playing;
        Play();
    }
}

void RtspSourceFilter::HandleSetupResponse(RTSPClient* client, int resultCode, char* resultString)
{
    RtspClient* myClient = static_cast<RtspClient*>(client);
    myClient->filter->HandleSetupResponse(resultCode, resultString);
}

void RtspSourceFilter::HandleSetupResponse(int resultCode, char* resultString)
{
    if (resultCode == 0)
    {
        delete[] resultString;
        MediaSubsession* subsession = _rtsp->subsession;

        if (!strcmp(subsession->mediumName(), "video"))
        {
            HRESULT hr;
            subsession->sink = new ProxyMediaSink(*_env, *subsession, _videoMediaQueue, recvBufferVideo);
            if (!_videoPin)
                _videoPin.reset(new RtspSourcePin(&hr, this, subsession, _videoMediaQueue));
            else
                _videoPin->ResetMediaSubsession(subsession);
        }
        else if (!strcmp(subsession->mediumName(), "audio"))
        {
            HRESULT hr;
            subsession->sink = new ProxyMediaSink(*_env, *subsession, _audioMediaQueue, recvBufferAudio);
            if (!_audioPin)
                _audioPin.reset(new RtspSourcePin(&hr, this, subsession, _audioMediaQueue));
            else
                _audioPin->ResetMediaSubsession(subsession);
        }

        // What about text medium ?

        if (subsession->sink == nullptr)
        {
            // unsupported medium or out of memory
            SetupSubsession();
            return;
        }

        subsession->miscPtr = _rtsp;
        subsession->sink->startPlaying(*(subsession->readSource()), HandleSubsessionFinished, subsession);

        // Set a handler to be called if a RTCP "BYE" arrives for this subsession
        if (subsession->rtcpInstance() != nullptr)
            subsession->rtcpInstance()->setByeHandler(HandleSubsessionByeHandler, subsession);

        ++_numSubsessions;
    }
    else
    {
        (*_env) << "SETUP failed, server response: " << resultString;
        delete[] resultString;
    }
    
    SetupSubsession();
}

void RtspSourceFilter::Play()
{
    MediaSession& mediaSession = *_rtsp->mediaSession;

    const float scale = 1.0f; // No trick play

    _sessionDuration = mediaSession.playEndTime() - _initialSeekTime;
    _sessionDuration = std::max(0.0, _sessionDuration);

    // For duration equal to 0 we got live stream with no end time (-1)
    _endTime = _sessionDuration > 0.0 ? _initialSeekTime + _sessionDuration : - 1.0;

    const char* absStartTime = mediaSession.absStartTime();
    if (absStartTime != nullptr)
    {
        // Either we or the server have specified that seeking should be done by 'absolute' time:
        _rtsp->sendPlayCommand(mediaSession, HandlePlayResponse, absStartTime, mediaSession.absEndTime(),
            scale, &_authenticator);
    }
    else
    {
        // Normal case: Seek by relative time (NPT):
        _rtsp->sendPlayCommand(mediaSession, HandlePlayResponse, _initialSeekTime, _endTime,
            scale, &_authenticator);
    }
}

void RtspSourceFilter::HandlePlayResponse(RTSPClient* client, int resultCode, char* resultString)
{
    RtspClient* myClient = static_cast<RtspClient*>(client);
    myClient->filter->HandlePlayResponse(resultCode, resultString);
}

void RtspSourceFilter::HandlePlayResponse(int resultCode, char* resultString)
{
    if (resultCode == 0)
    {
        _currentRequest.SetValue(error::Success);
        // State is already Playing
        _totNumPacketsReceived = 0;
        _sessionTimeout = _rtsp->sessionTimeoutParameter() != 0
            ? _rtsp->sessionTimeoutParameter() : 60;

        // Create timerTask for disconnection recognition and auto reconnect mechanism
        _interPacketGapCheckTimerTask = _scheduler->scheduleDelayedTask(interPacketGapMaxTime*1000, 
            &RtspSourceFilter::CheckInterPacketGaps, this);
        // Create timerTask for session keep-alive (use OPTIONS request to sustain session)
        _livenessCommandTask = _scheduler->scheduleDelayedTask(_sessionTimeout/3*1000000, 
            &RtspSourceFilter::SendLivenessCommand, this);

        if (_sessionDuration > 0)
        {
            double rangeAdjustment = (_rtsp->mediaSession->playEndTime() - 
                _rtsp->mediaSession->playStartTime()) - (_endTime - _initialSeekTime);
            if (_sessionDuration + rangeAdjustment > 0.0)
                _sessionDuration += rangeAdjustment;
            int64_t uSecsToDelay = (int64_t)(_sessionDuration*1000000.0);
            _sessionTimerTask = _scheduler->scheduleDelayedTask(uSecsToDelay, 
                &RtspSourceFilter::HandleMediaEnded, this);
        }
    }
    else
    {
        UnscheduleAllDelayedTasks();
        CloseSession();
        CloseClient();

        if (_autoReconnectionMSecs > 0)
        {
            _scheduler->scheduleDelayedTask(_autoReconnectionMSecs*1000, 
                &RtspSourceFilter::Reconnect, this);
            _state = State::Reconnecting;
            _currentRequest.SetValue(error::PlayFailed);
        }
        else
        {

            _state = State::Initial;
            _currentRequest.SetValue(error::PlayFailed);

            // Notify output pins PLAY command failed
            _videoMediaQueue.push(MediaPacketSample());
            _audioMediaQueue.push(MediaPacketSample());
        }
    }

    delete[] resultString;
}

void RtspSourceFilter::CloseSession()
{
    if (!_rtsp) return; // sane check
    MediaSession* mediaSession = _rtsp->mediaSession;
    if (mediaSession != nullptr)
    {
        // Don't bother waiting or response
        _rtsp->sendTeardownCommand(*mediaSession, nullptr, &_authenticator);
        // Close media sinks
        MediaSubsessionIterator iter(*mediaSession);
        MediaSubsession* subsession;
        while ((subsession = iter.next()) != nullptr)
        {
            Medium::close(subsession->sink);
            subsession->sink = nullptr;
        }
        // Close media session itself
        Medium::close(mediaSession);
        _rtsp->mediaSession = nullptr;
    }
}

void RtspSourceFilter::CloseClient()
{
    // Shutdown RTSP client
    Medium::close(_rtsp);
    _rtsp = nullptr;
}

void RtspSourceFilter::HandleSubsessionFinished(void* clientData)
{
    MediaSubsession* subsession = static_cast<MediaSubsession*>(clientData);
    RtspClient* rtsp = static_cast<RtspClient*>(subsession->miscPtr);
    // Close finished media subsession
    Medium::close(subsession->sink);
    subsession->sink = nullptr;
    // Check if there's at least one active subsession
    MediaSession& media_session = subsession->parentSession();
    MediaSubsessionIterator iter(media_session);
    while ((subsession = iter.next()) != nullptr)
    {
        if (subsession->sink != nullptr)
            return;
    }
    // No more subsessions active - close the session
    RtspSourceFilter* self = rtsp->filter;
    self->UnscheduleAllDelayedTasks();
    self->CloseSession();
    self->CloseClient();
    self->_state = State::Initial;
    self->_videoMediaQueue.push(MediaPacketSample());
    self->_audioMediaQueue.push(MediaPacketSample());
    // No request to reply to
}

void RtspSourceFilter::HandleSubsessionByeHandler(void* clientData)
{
    DebugLog("BYE!\n");
    // We were given a RTCP BYE packet - server close the connection (for example: session timeout)
    HandleSubsessionFinished(clientData);
}

void RtspSourceFilter::UnscheduleAllDelayedTasks()
{
    if (_firstCallTimeoutTask != nullptr)
        _scheduler->unscheduleDelayedTask(_firstCallTimeoutTask);
    if (_interPacketGapCheckTimerTask != nullptr)
        _scheduler->unscheduleDelayedTask(_interPacketGapCheckTimerTask);
    if (_reconnectionTimerTask != nullptr)
        _scheduler->unscheduleDelayedTask(_reconnectionTimerTask);
    if (_livenessCommandTask != nullptr)
        _scheduler->unscheduleDelayedTask(_livenessCommandTask);
    if (_sessionTimerTask != nullptr)
        _scheduler->unscheduleDelayedTask(_sessionTimerTask);
}

void RtspSourceFilter::Shutdown()
{
    UnscheduleAllDelayedTasks();
    CloseSession();
    CloseClient();

    _state = State::Initial;
    _currentRequest.SetValue(error::Success);

    // Notify pins we are tearing down
    _videoMediaQueue.push(MediaPacketSample());
    _audioMediaQueue.push(MediaPacketSample());
}

bool RtspSourceFilter::ScheduleNextReconnect()
{
    if (_state == State::Reconnecting)
    {
        _scheduler->scheduleDelayedTask(_autoReconnectionMSecs*1000, 
            &RtspSourceFilter::Reconnect, this);
        // state is still Reconnecting
        _currentRequest.SetValue(error::ReconnectFailed);
        return true;
    }
    return false;
}

/*
 * Task:_firstCallTimeoutTask
 * Allows for customized timeout on first call to the target RTSP server
 * Viable only in SettingUp an Reconnecing state.
 */
void RtspSourceFilter::DescribeRequestTimeout(void* clientData)
{
    RtspSourceFilter* self = static_cast<RtspSourceFilter*>(clientData);
    self->DescribeRequestTimeout();
}

void RtspSourceFilter::DescribeRequestTimeout()
{
    _ASSERT(_state == State::SettingUp || 
            _state == State::Reconnecting);
    _firstCallTimeoutTask = nullptr;

    if (_reconnectionTimerTask != nullptr)
        _scheduler->unscheduleDelayedTask(_reconnectionTimerTask);

    CloseClient(); // No session to close yet
    if (ScheduleNextReconnect()) return;

    _state = State::Initial;
    _currentRequest.SetValue(error::ServerNotReachable);
}

/*
 * Task:_interPacketGapCheckTimerTask:
 * Periodically calculates how many packets arrived allowing to detect connection lost.
 * Viable only in Playing state.
 */
void RtspSourceFilter::CheckInterPacketGaps(void* clientData)
{
    RtspSourceFilter* self = static_cast<RtspSourceFilter*>(clientData);
    self->CheckInterPacketGaps();
}

void RtspSourceFilter::CheckInterPacketGaps()
{
    _ASSERT(_state == State::Playing);
    _interPacketGapCheckTimerTask = nullptr;

    // Aliases
    UsageEnvironment& env = *_env;
    MediaSession& mediaSession = *_rtsp->mediaSession;    

    // Check each subsession, counting up how many packets have been received
    MediaSubsessionIterator iter(mediaSession);
    MediaSubsession* subsession;
    unsigned newTotNumPacketsReceived = 0;
    while ((subsession = iter.next()) != nullptr)
    {
        RTPSource* src = subsession->rtpSource();
        if (src == nullptr) continue;
        newTotNumPacketsReceived += src->receptionStatsDB().totNumPacketsReceived();
    }
    DebugLog("Total number of packets received: %u, queued packets: %u|%u\n", newTotNumPacketsReceived, 
        _videoMediaQueue.size(), _audioMediaQueue.size());
    // No additional packets have been received since the last time we checked
    if (newTotNumPacketsReceived == _totNumPacketsReceived)
    {
        DebugLog("No packets has been received since last time!\n");

        if (_livenessCommandTask != nullptr)
            _scheduler->unscheduleDelayedTask(_livenessCommandTask);
        if (_sessionTimerTask != nullptr)
            _scheduler->unscheduleDelayedTask(_sessionTimerTask);

        // If auto reconnect is off - notify pins to stop waiting for packets that most probably won't come
        if (_autoReconnectionMSecs == 0)
        {
            _videoMediaQueue.push(MediaPacketSample());
            _audioMediaQueue.push(MediaPacketSample());
        }
        // Schedule reconnection task
        else
        {
            // It's VoD - need to recalculate initial time seek for reconnect PLAY command
            if (_sessionDuration > 0)
            {
                // Retrieve current play time from output pins
                REFERENCE_TIME currentPlayTime = 0;
                if (_videoPin)
                {
                    currentPlayTime = _videoPin->CurrentPlayTime();
                    // Get minimum of two NPT 
                    if (_audioPin)
                        currentPlayTime = std::min(currentPlayTime, _audioPin->CurrentPlayTime());
                }
                else if (_audioPin)
                {
                    currentPlayTime = _audioPin->CurrentPlayTime();
                }

                _initialSeekTime += static_cast<double>(currentPlayTime) / UNITS;
            }

            // Notify pin to desynchronize
            if (_videoPin) _videoPin->ResetTimeBaselines();
            if (_audioPin) _audioPin->ResetTimeBaselines();

            // Finally schedule reconnect task
            _reconnectionTimerTask = _scheduler->scheduleDelayedTask(_autoReconnectionMSecs*1000,
                &RtspSourceFilter::Reconnect, this);
        }
    }
    else
    {
        _totNumPacketsReceived = newTotNumPacketsReceived;
        // Schedule next inspection
        _interPacketGapCheckTimerTask = _scheduler->scheduleDelayedTask(interPacketGapMaxTime*1000, 
            &RtspSourceFilter::CheckInterPacketGaps, this);
    }
}

/*
 * Task:_livenessCommandTask:
 * Periodically requests OPTION command to the server to keep alive the session
 * Viable only in Playing state.
 */
void RtspSourceFilter::SendLivenessCommand(void* clientData)
{
    RtspSourceFilter* self = static_cast<RtspSourceFilter*>(clientData);
    _ASSERT(self);
    _ASSERT(self->_state == State::Playing);

    self->_livenessCommandTask = nullptr;
    self->_rtsp->sendOptionsCommand(HandleOptionsResponse, &self->_authenticator);
}

void RtspSourceFilter::HandleOptionsResponse(RTSPClient* client, int resultCode, char* resultString)
{
    // If something bad happens between OPTIONS request and response, response handler shouldn't be call
    RtspClient* myClient = static_cast<RtspClient*>(client);
    myClient->filter->HandleOptionsResponse(resultCode, resultString);
}

void RtspSourceFilter::HandleOptionsResponse(int resultCode, char* resultString)
{
    _ASSERT(_state == State::Playing);

    // Used as a liveness command
    delete[] resultString;

    if (resultCode == 0)
    {
        // Schedule next keep-alive request if there wasn't any error along the way
        _livenessCommandTask = _scheduler->scheduleDelayedTask(_sessionTimeout/3*1000000,
            &RtspSourceFilter::SendLivenessCommand, this);
    }
}

/*
 * Task:_reconnectionTimerTask
 * Tries to reopen the connection and start to play from the moment connection was lost
 * Viable in Playing and Reconnecting (reattempt) state
 */
void RtspSourceFilter::Reconnect(void* clientData)
{
    RtspSourceFilter* self = static_cast<RtspSourceFilter*>(clientData);
    self->Reconnect();
}

void RtspSourceFilter::Reconnect()
{
    _ASSERT(_state == State::Playing || _state == State::Reconnecting);
    _reconnectionTimerTask = nullptr;

    // Called from worker thread as a delayed task
    DebugLog("Reconnect now!\n");
    AsyncReconnect();
}

/*
 * Task:_sessionTimerTask 
 * Perform shutdown when media come to end (for VOD)
 * Viable only in Playing state.
 */
void RtspSourceFilter::HandleMediaEnded(void* clientData)
{
    DebugLog("Media ended!\n");
    RtspSourceFilter* self = static_cast<RtspSourceFilter*>(clientData);
    _ASSERT(self->_state == State::Playing);
    self->_sessionTimerTask = nullptr;
    self->AsyncShutdown();
}

void RtspSourceFilter::WorkerThread()
{
    SetThreadName(-1, "RTSP source thread");
    bool done = false;

    // Uses internals of RtspSourceFilter
    auto GetRtspSourceStateString = 
        [](State state)
        {
            switch (state)
            {
            case State::Initial: return "Initial";
            case State::SettingUp: return "SettingUp";
            case State::ReadyToPlay: return "ReadyToPlay";
            case State::Playing: return "Playing";
            case State::Reconnecting: return "Reconnecting";
            default: return "Unknown";
            }
        };

    while (!done)
    {
        // In the middle of request - ignore any incoming requests untill done
        if (_state == State::SettingUp)
        {
            _scheduler->SingleStep();
            continue;
        }

        RtspAsyncRequest req;

        if (!_requestQueue.try_pop(req))
        {
            // No requests to process to - make a single step
            _scheduler->SingleStep();
            continue;
        }

        DebugLog("[WorkerThread] -  State: %s, Request: %s]\n",
                 GetRtspSourceStateString(_state),
                 GetRtspAsyncRequestTypeString(req.GetRequest()));

        // Process requests
        switch (_state)
        {
        case State::Initial:
            switch (req.GetRequest())
            {
            // Start opening url
            case RtspAsyncRequest::Open:
                _currentRequest = std::move(req);
                _state = State::SettingUp;
                OpenUrl(_currentRequest.GetRequestData());
                break;

            // Wrong transitions
            case RtspAsyncRequest::Play:
            case RtspAsyncRequest::Reconnect:
                req.SetValue(error::WrongState);
                break;

            case RtspAsyncRequest::Stop:
                // Needed if filter is re-started and fails to start running for some reason
                // and also output pins threads are already started and waiting for packets.
                // This is because Pause() is called before Run() which can fail if filter is restarted
                _videoMediaQueue.push(MediaPacketSample());
                _audioMediaQueue.push(MediaPacketSample());
                req.SetValue(error::Success);
                break;

            // Finish this thread
            case RtspAsyncRequest::Done:
                done = true;
                req.SetValue(error::Success);
                break;
            }
            break;

        case State::ReadyToPlay:
            switch (req.GetRequest())
            {
            // Wrong transition
            case RtspAsyncRequest::Open:
            case RtspAsyncRequest::Reconnect:
                req.SetValue(error::WrongState);
                break;

            // Start media streaming
            case RtspAsyncRequest::Play:
                _currentRequest = std::move(req);
                _state = State::Playing;
                Play();
                break;

            // Back down from streaming - close media session and its sink(s)
            case RtspAsyncRequest::Stop:
                _currentRequest = std::move(req);
                Shutdown();
                break;

            // Order from the dtor - finish this thread
            case RtspAsyncRequest::Done:
                _currentRequest = std::move(req);
                Shutdown();
                done = true;
                break;
            }
            break;

        case State::Playing:
            switch (req.GetRequest())
            {
            // Wrong transition
            case RtspAsyncRequest::Open:
            case RtspAsyncRequest::Play:
                req.SetValue(error::WrongState);
                break;

            // Try to reconnect
            case RtspAsyncRequest::Reconnect:
                _currentRequest = std::move(req);
                _state = State::Reconnecting;
                CloseSession();
                CloseClient();
                OpenUrl(_rtspUrl);
                break;

            // Back down from streaming - close media session and its sink(s)
            case RtspAsyncRequest::Stop:
                _currentRequest = std::move(req);
                Shutdown();
                break;

            // Order from the dtor - finish this thread
            case RtspAsyncRequest::Done:
                _currentRequest = std::move(req);
                Shutdown();
                done = true;
                break;
            }
            break;

        case State::Reconnecting:
            switch (req.GetRequest())
            {
            // Wrong transition
            case RtspAsyncRequest::Open:
            case RtspAsyncRequest::Play:
                req.SetValue(error::WrongState);
                break;

            // Try another round
            case RtspAsyncRequest::Reconnect:
                _currentRequest = std::move(req);
                // Session and client should be null here
                _ASSERT(!_rtsp);
                OpenUrl(_rtspUrl);
                break;

            // Giveup trying to reconnect
            case RtspAsyncRequest::Stop:
                _currentRequest = std::move(req);
                Shutdown();
                break;

            case RtspAsyncRequest::Done:
                _currentRequest = std::move(req);
                Shutdown();
                done = true;
                break;
            }
            break;

        default:
            // should never come here
            _ASSERT(false);
            break;
        }
    }
}

namespace
{
    bool IsSubsessionSupported(MediaSubsession& mediaSubsession)
    {
        if (!strcmp(mediaSubsession.mediumName(), "video"))
        {     
            if (!strcmp(mediaSubsession.codecName(), "H264"))
            {
                return true;
            }
        }
        else if (!strcmp(mediaSubsession.mediumName(), "audio"))
        {
            if (!strcmp(mediaSubsession.codecName(), "MPEG4-GENERIC"))
            {        
                return true;
            }
        }

        return false;
    }

    const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
    typedef struct tagTHREADNAME_INFO
    {
        DWORD dwType;     // Must be 0x1000.
        LPCSTR szName;    // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags;    // Reserved for future use, must be zero.
    } THREADNAME_INFO;
#pragma pack(pop)

    void SetThreadName(DWORD dwThreadID, char* threadName)
    {
        THREADNAME_INFO info = { 0x1000, threadName, dwThreadID, 0 };

        __try
        {
            RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }
}
