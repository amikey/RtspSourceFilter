#include "RtspError.h"

class ErrorCategory : public std::error_category
{
public:
    virtual const char* name() const { return "RTSP"; }

    virtual std::string message(int ev) const
    {
        switch (ev)
        {
        case error::Success:
            return "Success";
        case error::WrongState:
            return "Wrong state of session object";
        case error::ClientCreateFailed:
            return "Failed to create a RTSP client for given URL";
        case error::ServerNotReachable:
            return "Host not reachable";
        case error::OptionsFailed:
            return "OPTIONS command failed";
        case error::DescribeFailed:
            return "Failed to get a SDP description for given URL";
        case error::SdpInvalid:
            return "Failed to create a MediaSession object from the SDP description";
        case error::NoSubsessions:
            return "MediaSession does not have any media subsessions (i.e, no \"m=\" "
                   "lines)";
        case error::SetupFailed:
            return "SETUP command failed";
        case error::NoSubsessionsSetup:
            return "No subsession was setup - either none of them were supported or "
                   "they couldn't be initialized";
        case error::PlayFailed:
            return "Failed to start playing session";
        case error::SinkCreationFailed:
            return "";
        default:
            return "Unknown error";
        }
    }

    virtual std::error_condition default_error_condition(int ev) const
    {
        switch (ev)
        {
        case error::WrongState:
        case error::ClientCreateFailed:
        case error::ServerNotReachable:
        case error::OptionsFailed:
        case error::DescribeFailed:
        case error::SdpInvalid:
        case error::NoSubsessions:
        case error::SetupFailed:
        case error::NoSubsessionsSetup:
        case error::PlayFailed:
        case error::SinkCreationFailed:
            return error::RtspError;
        default:
            return std::error_condition(ev, *this);
        }
    }
};

namespace error
{
    std::error_code make_error_code(ErrorCode e)
    {
        return std::error_code(static_cast<int>(e), GetErrorCategory());
    }

    std::error_condition make_error_condition(ErrorCondition e)
    {
        return std::error_condition(static_cast<int>(e), GetErrorCategory());
    }
}

const std::error_category& GetErrorCategory()
{
    static ErrorCategory instance;
    return instance;
}
