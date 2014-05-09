#pragma once

#include <system_error>
#include <string>

namespace error
{
    enum ErrorCode
    {
        Success = 0,
        WrongState,
        ClientCreateFailed,
        ServerNotReachable,
        OptionsFailed,
        DescribeFailed,
        SdpInvalid,
        NoSubsessions,
        SetupFailed,
        NoSubsessionsSetup,
        PlayFailed,
        SinkCreationFailed,
        ReconnectFailed
    };

    enum ErrorCondition
    {
        RtspError
    };

    // required by system_error to work with our custom error and error conditions
    std::error_code make_error_code(ErrorCode e);
    std::error_condition make_error_condition(ErrorCondition e);
}

const std::error_category& GetErrorCategory();

namespace std
{
    template <>
    struct is_error_code_enum<error::ErrorCode> : public true_type
    {
    };

    template <>
    struct is_error_condition_enum<error::ErrorCondition> : public true_type
    {
    };
}
