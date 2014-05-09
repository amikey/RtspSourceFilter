#pragma once

#include <cstdint>
#include <Windows.h>

MIDL_INTERFACE("C4D310F4-160D-408D-9A60-3C6275E2D3B2")
IRtspSourceConfig : public IUnknown
{
    STDMETHOD_(void, SetInitialSeekTime(DOUBLE secs)) = 0;
    STDMETHOD_(void, SetStreamingOverTcp(BOOL streamOverTcp)) = 0;
    STDMETHOD_(void, SetTunnelingOverHttpPort(WORD tunnelOverHttpPort)) = 0;
    STDMETHOD_(void, SetAutoReconnectionPeriod(DWORD dwMSecs)) = 0;
    STDMETHOD_(void, SetLatency(DWORD dwMSecs)) = 0;
    STDMETHOD_(void, StopStreaming()) = 0;
};