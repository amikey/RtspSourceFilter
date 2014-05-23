using System;
using System.Runtime.InteropServices;

namespace RtspSourceNET.DirectShow
{
    [ComImport, Guid("EE30215D-164F-4A92-A4EB-9D4C13390F9F")]
    class LAVVideo
    {
    }

    // Codecs supported in the LAV Video configuration
    // Codecs not listed here cannot be turned off. You can request codecs to be added to this list, if you wish.
    public enum LAVVideoCodec
    {
        Codec_H264,
        Codec_VC1,
        Codec_MPEG1,
        Codec_MPEG2,
        Codec_MPEG4,
        Codec_MSMPEG4,
        Codec_VP8,
        Codec_WMV3,
        Codec_WMV12,
        Codec_MJPEG,
        Codec_Theora,
        Codec_FLV1,
        Codec_VP6,
        Codec_SVQ,
        Codec_H261,
        Codec_H263,
        Codec_Indeo,
        Codec_TSCC,
        Codec_Fraps,
        Codec_HuffYUV,
        Codec_QTRle,
        Codec_DV,
        Codec_Bink,
        Codec_Smacker,
        Codec_RV12,
        Codec_RV34,
        Codec_Lagarith,
        Codec_Cinepak,
        Codec_Camstudio,
        Codec_QPEG,
        Codec_ZLIB,
        Codec_QTRpza,
        Codec_PNG,
        Codec_MSRLE,
        Codec_ProRes,
        Codec_UtVideo,
        Codec_Dirac,
        Codec_DNxHD,
        Codec_MSVideo1,
        Codec_8BPS,
        Codec_LOCO,
        Codec_ZMBV,
        Codec_VCR1,
        Codec_Snow,
        Codec_FFV1,
        Codec_v210,
        Codec_JPEG2000,
        Codec_VMNC,
        Codec_FLIC,
        Codec_G2M,
        Codec_ICOD,
        Codec_THP,
        Codec_HEVC,
        Codec_VP9,
        Codec_TrueMotion,
        Codec_VideoNB
    }

    // Codecs with hardware acceleration
    public enum LAVVideoHWCodec
    {
        HWCodec_H264  = LAVVideoCodec.Codec_H264,
        HWCodec_VC1   = LAVVideoCodec.Codec_VC1,
        HWCodec_MPEG2 = LAVVideoCodec.Codec_MPEG2,
        HWCodec_MPEG4 = LAVVideoCodec.Codec_MPEG4,
        HWCodec_MPEG2DVD,
        HWCodec_NB    = HWCodec_MPEG2DVD + 1
    }

    [Flags]
    public enum LAVHWResFlag
    {
        LAVHWResFlag_SD  = 0x0001,
        LAVHWResFlag_HD  = 0x0002,
        LAVHWResFlag_UHD = 0x0004
    }

    // Type of hardware accelerations
    public enum LAVHWAccel 
    {
        HWAccel_None,
        HWAccel_CUDA,
        HWAccel_QuickSync,
        HWAccel_DXVA2,
        HWAccel_DXVA2CopyBack = HWAccel_DXVA2,
        HWAccel_DXVA2Native
    }

    // Deinterlace algorithms offered by the hardware decoders
    public enum LAVHWDeintModes
    {
        HWDeintMode_Weave,
        HWDeintMode_BOB, // Deprecated
        HWDeintMode_Hardware
    }

    // Software deinterlacing algorithms
    public enum LAVSWDeintModes
    {
        SWDeintMode_None,
        SWDeintMode_YADIF
    }

    // Deinterlacing processing mode
    public enum LAVDeintMode
    {
        DeintMode_Auto,
        DeintMode_Aggressive,
        DeintMode_Force,
        DeintMode_Disable
    }

    // Type of deinterlacing to perform
    // - FramePerField re-constructs one frame from every field, resulting in 50/60 fps.
    // - FramePer2Field re-constructs one frame from every 2 fields, resulting in 25/30 fps.
    // Note: Weave will always use FramePer2Field
    public enum LAVDeintOutput
    {
        DeintOutput_FramePerField,
        DeintOutput_FramePer2Field
    }

    // Control the field order of the deinterlacer
    public enum LAVDeintFieldOrder
    {
        DeintFieldOrder_Auto,
        DeintFieldOrder_TopFieldFirst,
        DeintFieldOrder_BottomFieldFirst
    }

    // Supported output pixel formats
    public enum LAVOutPixFmts
    {
        LAVOutPixFmt_None = -1,
        LAVOutPixFmt_YV12,            // 4:2:0, 8bit, planar
        LAVOutPixFmt_NV12,            // 4:2:0, 8bit, Y planar, U/V packed
        LAVOutPixFmt_YUY2,            // 4:2:2, 8bit, packed
        LAVOutPixFmt_UYVY,            // 4:2:2, 8bit, packed
        LAVOutPixFmt_AYUV,            // 4:4:4, 8bit, packed
        LAVOutPixFmt_P010,            // 4:2:0, 10bit, Y planar, U/V packed
        LAVOutPixFmt_P210,            // 4:2:2, 10bit, Y planar, U/V packed
        LAVOutPixFmt_Y410,            // 4:4:4, 10bit, packed
        LAVOutPixFmt_P016,            // 4:2:0, 16bit, Y planar, U/V packed
        LAVOutPixFmt_P216,            // 4:2:2, 16bit, Y planar, U/V packed
        LAVOutPixFmt_Y416,            // 4:4:4, 16bit, packed
        LAVOutPixFmt_RGB32,           // 32-bit RGB (BGRA)
        LAVOutPixFmt_RGB24,           // 24-bit RGB (BGR)
        LAVOutPixFmt_v210,            // 4:2:2, 10bit, packed
        LAVOutPixFmt_v410,            // 4:4:4, 10bit, packed
        LAVOutPixFmt_YV16,            // 4:2:2, 8-bit, planar
        LAVOutPixFmt_YV24,            // 4:4:4, 8-bit, planar
        LAVOutPixFmt_RGB48,           // 48-bit RGB (16-bit per pixel, BGR)
        LAVOutPixFmt_NB               // Number of formats
    }

    public enum LAVDitherMode
    {
        LAVDither_Ordered,
        LAVDither_Random
    }

        // LAV Video status interface
    [Guid("FA40D6E9-4D38-4761-ADD2-71A9EC5FD32F"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface ILAVVideoSettings
    {
        // Switch to Runtime Config mode. This will reset all settings to default, and no changes to the settings will be saved
        // You can use this to programmatically configure LAV Video without interfering with the users settings in the registry.
        // Subsequent calls to this function will reset all settings back to defaults, even if the mode does not change.
        //
        // Note that calling this function during playback is not supported and may exhibit undocumented behaviour. 
        // For smooth operations, it must be called before LAV Video is connected to other filters.
        [PreserveSig]
        void SetRuntimeConfig([MarshalAs(UnmanagedType.Bool)][In] bool bRuntimeConfig);

        // Configure which codecs are enabled
        // If vCodec is invalid (possibly a version difference), Get will return FALSE, and Set E_FAIL.
        [PreserveSig]
        [return:MarshalAs(UnmanagedType.Bool)]
        bool GetFormatConfiguration([In] LAVVideoCodec vCodec);

        [PreserveSig]
        void SetFormatConfiguration([In] LAVVideoCodec vCodec, [MarshalAs(UnmanagedType.Bool)][In] bool bEnabled);

        // Set the number of threads to use for Multi-Threaded decoding (where available)
        //  0 = Auto Detect (based on number of CPU cores)
        //  1 = 1 Thread -- No Multi-Threading
        // >1 = Multi-Threading with the specified number of threads
        [PreserveSig]
        void SetNumThreads([In] uint dwNum);

        // Get the number of threads to use for Multi-Threaded decoding (where available)
        //  0 = Auto Detect (based on number of CPU cores)
        //  1 = 1 Thread -- No Multi-Threading
        // >1 = Multi-Threading with the specified number of threads
        [PreserveSig]
        uint GetNumThreads();

        // Set whether the aspect ratio encoded in the stream should be forwarded to the renderer,
        // or the aspect ratio specified by the source filter should be kept.
        // 0 = AR from the source filter
        // 1 = AR from the Stream
        // 2 = AR from stream if source is not reliable
        [PreserveSig]
        void SetStreamAR([In] uint bStreamAR);

        // Get whether the aspect ratio encoded in the stream should be forwarded to the renderer,
        // or the aspect ratio specified by the source filter should be kept.
        // 0 = AR from the source filter
        // 1 = AR from the Stream
        // 2 = AR from stream if source is not reliable
        [PreserveSig]
        uint GetStreamAR();

        // Configure which pixel formats are enabled for output
        // If pixFmt is invalid, Get will return FALSE and Set E_FAIL
        [PreserveSig]
        [return:MarshalAs(UnmanagedType.Bool)]
        bool GetPixelFormat([In] LAVOutPixFmts pixFmt);

        [PreserveSig]
        void SetPixelFormat([In] LAVOutPixFmts pixFmt, [MarshalAs(UnmanagedType.Bool)][In] bool bEnabled);

        // Set the RGB output range for the YUV->RGB conversion
        // 0 = Auto (same as input), 1 = Limited (16-235), 2 = Full (0-255)
        [PreserveSig]
        void SetRGBOutputRange([In] uint dwRange);

        // Get the RGB output range for the YUV->RGB conversion
        // 0 = Auto (same as input), 1 = Limited (16-235), 2 = Full (0-255)
        [PreserveSig]
        uint GetRGBOutputRange();

        // Set the deinterlacing field order of the hardware decoder
        [PreserveSig]
        void SetDeintFieldOrder([In] LAVDeintFieldOrder fieldOrder);

        // get the deinterlacing field order of the hardware decoder
        [PreserveSig]
        LAVDeintFieldOrder GetDeintFieldOrder();

        // DEPRECATED, use SetDeinterlacingMode
        [PreserveSig]
        void SetDeintAggressive([MarshalAs(UnmanagedType.Bool)][In] bool bAggressive);

        // DEPRECATED, use GetDeinterlacingMode
        [PreserveSig]
        [return:MarshalAs(UnmanagedType.Bool)]
        bool GetDeintAggressive();

        // DEPRECATED, use SetDeinterlacingMode
        [PreserveSig]
        void SetDeintForce([MarshalAs(UnmanagedType.Bool)][In] bool bForce);

        // DEPRECATED, use GetDeinterlacingMode
        [PreserveSig]
        [return:MarshalAs(UnmanagedType.Bool)]
        bool GetDeintForce();

        // Check if the specified HWAccel is supported
        // Note: This will usually only check the availability of the required libraries (ie. for NVIDIA if a recent enough NVIDIA driver is installed)
        // and not check actual hardware support
        // Returns: 0 = Unsupported, 1 = Supported, 2 = Currently running
        [PreserveSig]
        uint CheckHWAccelSupport([In] LAVHWAccel hwAccel);

        // Set which HW Accel method is used
        // See LAVHWAccel for options.
        [PreserveSig]
        void SetHWAccel([In] LAVHWAccel hwAccel);

        // Get which HW Accel method is active
        [PreserveSig]
        LAVHWAccel GetHWAccel();

        // Set which codecs should use HW Acceleration
        [PreserveSig]
        void SetHWAccelCodec(LAVVideoHWCodec hwAccelCodec, [MarshalAs(UnmanagedType.Bool)][In] bool bEnabled);

        // Get which codecs should use HW Acceleration
        [PreserveSig]
        [return:MarshalAs(UnmanagedType.Bool)]
        bool GetHWAccelCodec([In] LAVVideoHWCodec hwAccelCodec);

        // Set the deinterlacing mode used by the hardware decoder
        [PreserveSig]
        void SetHWAccelDeintMode([In] LAVHWDeintModes deintMode);

        // Get the deinterlacing mode used by the hardware decoder
        [PreserveSig]
        LAVHWDeintModes GetHWAccelDeintMode();

        // Set the deinterlacing output for the hardware decoder
        [PreserveSig]
        void SetHWAccelDeintOutput([In] LAVDeintOutput deintOutput);

        // Get the deinterlacing output for the hardware decoder
        [PreserveSig]
        LAVDeintOutput GetHWAccelDeintOutput();

        // Set whether the hardware decoder should force high-quality deinterlacing
        // Note: this option is not supported on all decoder implementations and/or all operating systems
        [PreserveSig]
        void SetHWAccelDeintHQ([MarshalAs(UnmanagedType.Bool)][In] bool bHQ);

        // Get whether the hardware decoder should force high-quality deinterlacing
        // Note: this option is not supported on all decoder implementations and/or all operating systems
        [PreserveSig]
        [return:MarshalAs(UnmanagedType.Bool)]
        bool GetHWAccelDeintHQ();

        // Set the software deinterlacing mode used
        [PreserveSig]
        void SetSWDeintMode([In] LAVSWDeintModes deintMode);

        // Get the software deinterlacing mode used
        [PreserveSig]
        LAVSWDeintModes GetSWDeintMode();

        // Set the software deinterlacing output
        [PreserveSig]
        void SetSWDeintOutput([In] LAVDeintOutput deintOutput);

        // Get the software deinterlacing output
        [PreserveSig]
        LAVDeintOutput GetSWDeintOutput();

        // DEPRECATED, use SetDeinterlacingMode
        [PreserveSig]
        void SetDeintTreatAsProgressive([MarshalAs(UnmanagedType.Bool)][In] bool bEnabled);

        // DEPRECATED, use GetDeinterlacingMode
        [PreserveSig]
        [return:MarshalAs(UnmanagedType.Bool)]
        bool GetDeintTreatAsProgressive();

        // Set the dithering mode used
        [PreserveSig]
        void SetDitherMode([In] LAVDitherMode ditherMode);

        // Get the dithering mode used
        [PreserveSig]
        LAVDitherMode GetDitherMode();

        // Set if the MS WMV9 DMO Decoder should be used for VC-1/WMV3
        [PreserveSig]
        void SetUseMSWMV9Decoder([MarshalAs(UnmanagedType.Bool)][In] bool bEnabled);

        // Get if the MS WMV9 DMO Decoder should be used for VC-1/WMV3
        [PreserveSig]
        [return:MarshalAs(UnmanagedType.Bool)]
        bool GetUseMSWMV9Decoder();

        // Set if DVD Video support is enabled
        [PreserveSig]
        void SetDVDVideoSupport([MarshalAs(UnmanagedType.Bool)][In] bool bEnabled);

        // Get if DVD Video support is enabled
        [PreserveSig]
        [return:MarshalAs(UnmanagedType.Bool)]
        bool GetDVDVideoSupport();

        // Set the HW Accel Resolution Flags
        // flags: bitmask of LAVHWResFlag flags
        [PreserveSig]
        void SetHWAccelResolutionFlags([In] LAVHWResFlag dwResFlags);

        // Get the HW Accel Resolution Flags
        // flags: bitmask of LAVHWResFlag flags
        [PreserveSig]
        LAVHWResFlag GetHWAccelResolutionFlags();

        // Toggle Tray Icon
        [PreserveSig]
        void SetTrayIcon([MarshalAs(UnmanagedType.Bool)][In] bool bEnabled);

        // Get Tray Icon
        [PreserveSig]
        [return:MarshalAs(UnmanagedType.Bool)]
        bool GetTrayIcon();

        // Set the Deint Mode
        [PreserveSig]
        void SetDeinterlacingMode([In] LAVDeintMode deintMode);

        // Get the Deint Mode
        [PreserveSig]
        LAVDeintMode GetDeinterlacingMode();

        // Set the index of the GPU to be used for hardware decoding
        // Only supported for CUVID and DXVA2 copy-back. If the device is not valid, it'll fallback to auto-detection
        // Must be called before an input is connected to LAV Video, and the setting is non-persistent
        // NOTE: For CUVID, the index defines the index of the CUDA capable device, while for DXVA2, the list includes all D3D9 devices
        [PreserveSig]
        void SetGPUDeviceIndex([In] uint dwDevice);
    }

}
