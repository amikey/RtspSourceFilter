#pragma once

#include <system_error>
#include <future>
#include <string>

typedef std::error_code RtspResult;
typedef std::future<RtspResult> RtspAsyncResult;

class RtspAsyncRequest
{
public:
    /**
     * Types of RTSP asynchronous request
     */
    enum Type
    {
        Unknown,
        Open,
        Play,
        Stop,
        Reconnect,
        Done
    };

    /**
     * Construct asynchronous RTSP request 
     */
    explicit RtspAsyncRequest(Type request, std::string requestData = "")
        : _request(request), _requestData(std::move(requestData))
    {
    }

    /**
     * Construct an invalid RTSP request 
     */
    RtspAsyncRequest()
        : _request(Unknown)
    {
    }

    RtspAsyncRequest(const RtspAsyncRequest&) = delete;
    RtspAsyncRequest& operator=(const RtspAsyncRequest&) = delete;

    /**
     * Move constructor
     */
    RtspAsyncRequest(RtspAsyncRequest&& other) { *this = std::move(other); }

    /**
     * Move operator
     */
    RtspAsyncRequest& operator=(RtspAsyncRequest&& other)
    {
        if (this != &other)
        {
            _request = other._request;
            _promise = std::move(other._promise);
            _requestData = std::move(other._requestData);
        }
        return *this;
    }

    RtspAsyncResult GetAsyncResult() { return _promise.get_future(); }
    Type GetRequest() const { return _request; }
    const std::string& GetRequestData() const { return _requestData; }
    void SetValue(RtspResult ec) { _promise.set_value(ec); }

private:
    Type _request;
    std::promise<RtspResult> _promise;
    std::string _requestData;
};

inline const char* GetRtspAsyncRequestTypeString(RtspAsyncRequest::Type type)
{
    switch (type)
    {
    case RtspAsyncRequest::Open: return "Open";
    case RtspAsyncRequest::Play: return "Play";
    case RtspAsyncRequest::Stop: return "Stop";
    case RtspAsyncRequest::Reconnect: return "Reconnect";
    case RtspAsyncRequest::Done: return "Done";
    case RtspAsyncRequest::Unknown:
    default: return "Unknown";
    }
}