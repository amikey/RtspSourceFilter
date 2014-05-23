using System;
using System.Runtime.InteropServices;

namespace RtspSourceNET.DirectShow
{
    [ComImport, Guid("FA10746C-9B63-4b6c-BC49-FC300EA5F256")]
    public class EnhancedVideoRenderer
    {
    }

    [Guid("F6696E82-74F7-4F3D-A178-8A5E09C3659F"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IMFClockStateSink
    {
        void OnClockStart([In] long hnsSystemTime, [In] long llClockStartOffset);
        void OnClockStop([In] long hnsSystemTime);
        void OnClockPause([In] long hnsSystemTime);
        void OnClockRestart([In] long hnsSystemTime);
        void OnClockSetRate([In] long hnsSystemTime, [In] float flRate);
    }

    [Guid("29AFF080-182A-4A5D-AF3B-448F3A6346CB"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IMFVideoPresenter : IMFClockStateSink
    {
        new void OnClockStart([In] long hnsSystemTime, [In] long llClockStartOffset);
        new void OnClockStop([In] long hnsSystemTime);
        new void OnClockPause([In] long hnsSystemTime);
        new void OnClockRestart([In] long hnsSystemTime);
        new void OnClockSetRate([In] long hnsSystemTime, [In] float flRate);

        // Stubs
        void ProcessMessage(/*MFVP_MESSAGE_TYPE eMessage, uint ulParam*/);
        void GetCurrentMediaType(/*out IMFVideoMediaType ppMediaType*/);
    }

    [Guid("A490B1E4-AB84-4D31-A1B2-181E03B1077A"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IMFVideoDisplayControl
    {
        // Stubs
        void GetNativeVideoSize(/*ref tagSIZE pszVideo, ref tagSIZE pszARVideo*/);
        void GetIdealVideoSize(/*ref tagSIZE pszMin, ref tagSIZE pszMax*/);
        void SetVideoPosition(/*ref MFVideoNormalizedRect pnrcSource, ref tagRECT prcDest*/);
        void GetVideoPosition(/*out MFVideoNormalizedRect pnrcSource, out tagRECT prcDest*/);
        void SetAspectRatioMode([In] uint dwAspectRatioMode);
        void GetAspectRatioMode([Out] out uint dwAspectRatioMode);
        void SetVideoWindow([In] IntPtr hwndVideo);
        void GetVideoWindow([Out] out IntPtr hwndVideo);
        void RepaintVideo();
        void GetCurrentImage(/* only stub */);
        void SetBorderColor([In] uint color);
        void GetBorderColor([Out] out uint color);
        void SetRenderingPrefs(/*[MarshalAs(UnmanagedType.I4), In] MFVideoRenderPrefs dwRenderFlags*/);
        void GetRenderingPrefs(/*[MarshalAs(UnmanagedType.I4), Out] out MFVideoRenderPrefs dwRenderFlags*/);
        void SetFullscreen([MarshalAs(UnmanagedType.Bool), In] bool fullscreen);
        void GetFullscreen([MarshalAs(UnmanagedType.Bool), Out] out bool fullscreen);
    }

    [Guid("DFDFD197-A9CA-43D8-B341-6AF3503792CD")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IMFVideoRenderer
    {
        void InitializeRenderer([MarshalAs(UnmanagedType.Interface), In] object pVideoMixer, 
                                [MarshalAs(UnmanagedType.Interface), In] IMFVideoPresenter pVideoPresenter);
    }
}
