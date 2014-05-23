using System;
using System.Runtime.InteropServices;

namespace RtspSourceNET.DirectShow
{
    [ComImport, Guid("AF645432-7263-49C1-9FA3-E6DA0B346EAB")]
    class RtspSourceFilter
    {
    }

    [Guid("C4D310F4-160D-408D-9A60-3C6275E2D3B2"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IRtspSourceConfig
    {
        [PreserveSig]
        void SetInitialSeekTime([In] double secs);

        [PreserveSig]
        void SetStreamingOverTcp([In, MarshalAs(UnmanagedType.Bool)] bool streamOverTcp);

        [PreserveSig]
        void SetTunnelingOverHttpPort([In] short tunnelOverHttpPort);

        [PreserveSig]
        void SetAutoReconnectionPeriod([In] uint dwMSecs);

        [PreserveSig]
        void SetLatency([In] uint dwMSecs);

        [PreserveSig]
        void StopStreaming();
    }
}
