#include "RtspSource.h"
#include "MediaPacketSample.h"
#include "ConcurrentQueue.h"
#include "H264StreamParser.h"
#include "Debug.h"

#include <Windows.h>
#include <DShow.h>
#include <dvdmedia.h>
#include <wmcodecdsp.h>
#include <MMReg.h>
// Audio GUIDs
#pragma comment(lib, "wmcodecdspuuid.lib")

// Uncomment this to use H.264 without starting codes (AVC1 FOURCC)
//#define H264_USE_AVC1

namespace
{
    const int sequenceHeaderLengthFieldSize = 2;
    const int lengthFieldSize = 4;
    const int startCodesSize = 4;

    HRESULT GetMediaTypeH264(CMediaType& mediaType, MediaSubsession& mediaSubsession);
    HRESULT GetMediaTypeAVC1(CMediaType& mediaType, MediaSubsession& mediaSubsession);
    HRESULT GetMediaTypeAAC(CMediaType& mediaType, MediaSubsession& mediaSubsession);

    bool IsIdrFrame(const MediaPacketSample& mediaPacket);
}

RtspSourcePin::RtspSourcePin(HRESULT* phr, CSource* pFilter, 
    MediaSubsession* mediaSubsession, MediaPacketQueue& mediaPacketQueue)
    : CSourceStream(TEXT("RtspSourcePin"), phr, pFilter,
                    !strcmp(mediaSubsession->mediumName(), "video") ? L"Video" : L"Audio")
    , _mediaSubsession(mediaSubsession), _mediaPacketQueue(mediaPacketQueue)
    , _codecFourCC(0), _currentPlayTime(0), _rtpPresentationTimeBaseline(0)
    , _streamTimeBaseline(0), _firstSample(true), _rtcpSynced(false)
{
    HRESULT hr = InitializeMediaType();
    if (phr) *phr = hr;
}

RtspSourcePin::~RtspSourcePin() {}

HRESULT RtspSourcePin::OnThreadCreate()
{
    DebugLog("%S pin: %s\n", m_pName, __FUNCTION__);
    return __super::OnThreadCreate();
}

HRESULT RtspSourcePin::OnThreadDestroy()
{
    DebugLog("%S pin: %s\n", m_pName, __FUNCTION__);
    return __super::OnThreadDestroy();
}

HRESULT RtspSourcePin::OnThreadStartPlay()
{
    DebugLog("%S pin: %s\n", m_pName, __FUNCTION__);
    ResetTimeBaselines();
    return __super::OnThreadStartPlay();
}

void RtspSourcePin::ResetTimeBaselines()
{
    // Desynchronize with RTP timestamps
    _firstSample = true;
    _rtcpSynced = false;
    _rtpPresentationTimeBaseline = 0;
    _streamTimeBaseline = 0;
    _currentPlayTime = 0;
}

REFERENCE_TIME RtspSourcePin::SynchronizeTimestamp(const MediaPacketSample& mediaSample)
{
    auto SyncWithMediaSample =
        [this](const MediaPacketSample& mediaSample)
        {
            CRefTime streamTime;
            m_pFilter->StreamTime(streamTime);
            uint32_t latencyMSecs = static_cast<RtspSourceFilter*>(m_pFilter)->_latencyMSecs;
            _streamTimeBaseline = streamTime.GetUnits() + latencyMSecs * 10000i64;
            _rtpPresentationTimeBaseline = mediaSample.timestamp();
        };

    if (_firstSample)
    {
        SyncWithMediaSample(mediaSample);

        _firstSample = false;
        // If we're lucky the first sample is also synced using RTCP
        _rtcpSynced = mediaSample.isRtcpSynced();
    }
    // First sample wasn't RTCP sync'ed, try the next time
    else if (!_rtcpSynced)
    {
        _rtcpSynced = mediaSample.isRtcpSynced();
        if (_rtcpSynced)
            SyncWithMediaSample(mediaSample);
    }

    return mediaSample.timestamp() - _rtpPresentationTimeBaseline + _streamTimeBaseline;
}

HRESULT RtspSourcePin::FillBuffer(IMediaSample* pSample)
{
    MediaPacketSample mediaSample;
    _mediaPacketQueue.pop(mediaSample);
    if (mediaSample.invalid())
    {
        DebugLog("%S pin: End of streaming!\n", m_pName);
        return S_FALSE;
    }

    BYTE* pData;
    HRESULT hr = pSample->GetPointer(&pData);
    if (FAILED(hr)) return hr;
    long length = pSample->GetSize();

    if (_codecFourCC == DWORD('h264'))
    {
        // Append SPS and PPS to the first packet (they come out-band)
        if (_firstSample)
        {
            // Retrieve them from media type format buffer 
            BYTE* decoderSpecific = (BYTE*)(((VIDEOINFOHEADER2*)_mediaType.Format()) + 1);
            ULONG decoderSpecificLength = _mediaType.FormatLength() - sizeof(VIDEOINFOHEADER2);
            memcpy_s(pData, length, decoderSpecific, decoderSpecificLength);
            pData += decoderSpecificLength; length -= decoderSpecificLength;
        }

        // Append 4-byte start code 00 00 00 01 in network byte order that precedes each NALU
        ((uint32_t*)pData)[0] = 0x01000000;
        pData += startCodesSize; length -= startCodesSize;
        // Finally copy media packet contens to IMediaSample
        memcpy_s(pData, length, mediaSample.data(), mediaSample.size());
        pSample->SetActualDataLength(mediaSample.size() + startCodesSize);
        pSample->SetSyncPoint(IsIdrFrame(mediaSample));
    }
    else if (_codecFourCC == DWORD('avc1'))
    {
        // Append 4-byte length field (network byte order) that precedes each NALU
        uint32_t lengthField = static_cast<uint32_t>(mediaSample.size());
        pData[0] = ((uint8_t*)&lengthField)[3];
        pData[1] = ((uint8_t*)&lengthField)[2];
        pData[2] = ((uint8_t*)&lengthField)[1];
        pData[3] = ((uint8_t*)&lengthField)[0];
        pData += lengthFieldSize; length -= lengthFieldSize;
        // Finally copy media packet contens to IMediaSample
        memcpy_s(pData, length, mediaSample.data(), mediaSample.size());
        pSample->SetActualDataLength(mediaSample.size() + lengthFieldSize);
        pSample->SetSyncPoint(IsIdrFrame(mediaSample));
    }
    else
    {
        // No appending - just copy raw data
        memcpy_s(pData, length, mediaSample.data(), mediaSample.size());
        pSample->SetActualDataLength(mediaSample.size());
        pSample->SetSyncPoint(FALSE);
    }

    REFERENCE_TIME ts = SynchronizeTimestamp(mediaSample);
    pSample->SetTime(&ts, NULL);

    // Calculate current play time (does not include offset from initial time seek)
    CRefTime streamTime;
    m_pFilter->StreamTime(streamTime);
    uint32_t latencyMSecs = static_cast<RtspSourceFilter*>(m_pFilter)->_latencyMSecs;
    _currentPlayTime = streamTime.GetUnits() - (_streamTimeBaseline - latencyMSecs * 10000i64);

    return S_OK;
}

HRESULT RtspSourcePin::GetMediaType(CMediaType* pMediaType)
{
    // We only support one MediaType - the one that is streamed
    CheckPointer(pMediaType, E_POINTER);
    CAutoLock cAutoLock(m_pFilter->pStateLock());
    FreeMediaType(*pMediaType);
    return CopyMediaType(pMediaType, &_mediaType);
}

HRESULT RtspSourcePin::DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pRequest)
{
    CheckPointer(pAlloc, E_POINTER);
    CheckPointer(pRequest, E_POINTER);

    CAutoLock cAutoLock(m_pFilter->pStateLock());

    // Video pin
    if (m_mt.formattype == FORMAT_MPEG2Video || 
        m_mt.formattype == FORMAT_VideoInfo2 || 
        m_mt.formattype == FORMAT_VideoInfo)
    {
        // Ensure a minimum number of buffers
        if (pRequest->cBuffers == 0)
            pRequest->cBuffers = 10;
        pRequest->cbBuffer = 256*1024; // Should be more than enough
    }
    // Audio pin
    else
    {
        // Ensure a minimum number of buffers
        if (pRequest->cBuffers == 0)
            pRequest->cBuffers = 2;
        pRequest->cbBuffer = 4096;
    }

    ALLOCATOR_PROPERTIES Actual;
    HRESULT hr = pAlloc->SetProperties(pRequest, &Actual);
    if (FAILED(hr)) return hr;
    // Is this allocator unsuitable?
    if (Actual.cbBuffer < pRequest->cbBuffer) return E_FAIL;
    return S_OK;
}

HRESULT RtspSourcePin::InitializeMediaType()
{
    HRESULT hr = E_FAIL;
    _mediaType.InitMediaType();

    if (!strcmp(_mediaSubsession->mediumName(), "video"))
    {
        if (!strcmp(_mediaSubsession->codecName(), "H264"))
        {
#if !defined(H264_USE_AVC1)
            // h264 with start codes are "canonical" in network streaming
            hr = GetMediaTypeH264(_mediaType, *_mediaSubsession);
            _codecFourCC = DWORD('h264');
#else
            hr = GetMediaTypeAVC1(_mediaType, _mediaSubsession);
            _codecFourCC = DWORD('avc1');
#endif
        }
    }
    else if (!strcmp(_mediaSubsession->mediumName(), "audio"))
    {
        if (!strcmp(_mediaSubsession->codecName(), "MPEG4-GENERIC"))
        {
            hr = GetMediaTypeAAC(_mediaType, *_mediaSubsession);
            _codecFourCC = DWORD('mp4a');
        }
    }

    return hr;
}

namespace
{
    HRESULT GetMediaTypeH264(CMediaType& mediaType, MediaSubsession& mediaSubsession)
    {
        // We need to extract SPS and PPS from SDP attribute and append it to VIDEOINFOHEADER2 structure to be used later
        // see: http://msdn.microsoft.com/en-us/library/dd757808%28v=vs.85%29.aspx, pg: H.264 Bitstream with Start Codes
        unsigned numSPropRecords;
        SPropRecord* sPropRecords = ::parseSPropParameterSets(
            mediaSubsession.attrVal_str("sprop-parameter-sets"), numSPropRecords);
        size_t decoderSpecificSize = 0;
        for (unsigned i = 0; i < numSPropRecords; ++i)
            decoderSpecificSize += sPropRecords[i].sPropLength + startCodesSize;

        // "Hide" decoder specific data in FormatBuffer
        VIDEOINFOHEADER2* pVid = (VIDEOINFOHEADER2*)mediaType.AllocFormatBuffer(sizeof(VIDEOINFOHEADER2) + decoderSpecificSize);
        if (!pVid) return E_OUTOFMEMORY;
        ZeroMemory(pVid, sizeof(VIDEOINFOHEADER2) + decoderSpecificSize);

        unsigned videoWidth = 0, videoHeight = 0;
        double videoFramerate = 0.0;

        // Move decoder specific data after FormatBuffer 
        BYTE* decoderSpecific = (BYTE*)(pVid + 1);
        for (unsigned i = 0; i < numSPropRecords; ++i)
        {
            SPropRecord& prop = sPropRecords[i];

            // It's SPS (Sequence parameter set
            if ((prop.sPropBytes[0] & 0x1F) == 7)
            {
                H264StreamParser h264StreamParser(prop.sPropBytes, prop.sPropLength);
                videoWidth = h264StreamParser.GetWidth();
                videoHeight = h264StreamParser.GetHeight();
                videoFramerate = h264StreamParser.GetFramerate();
            }

            ((uint32_t*)decoderSpecific)[0] = 0x01000000;
            decoderSpecific += 4;

            memcpy(decoderSpecific, prop.sPropBytes, prop.sPropLength);
            decoderSpecific += prop.sPropLength;
        }

        delete[] sPropRecords;

        SetRect(&pVid->rcSource, 0, 0, videoWidth, videoHeight);
        SetRect(&pVid->rcTarget, 0, 0, videoWidth, videoHeight);

        REFERENCE_TIME timePerFrame = std::abs(videoFramerate) > 1e-6 ?
            (REFERENCE_TIME)(UNITS / videoFramerate) : 0;
        pVid->AvgTimePerFrame = timePerFrame;

        // BITMAPINFOHEADER
        pVid->bmiHeader.biSize = sizeof BITMAPINFOHEADER;
        pVid->bmiHeader.biWidth = videoWidth;
        pVid->bmiHeader.biHeight = videoHeight;
        pVid->bmiHeader.biCompression = MAKEFOURCC('H','2','6','4');

        mediaType.SetType(&MEDIATYPE_Video);
        mediaType.SetSubtype(&MEDIASUBTYPE_H264);
        mediaType.SetFormatType(&FORMAT_VideoInfo2);
        mediaType.SetTemporalCompression(TRUE);
        mediaType.SetSampleSize(0);

        return S_OK;
    }

    HRESULT GetMediaTypeAVC1(CMediaType& mediaType, MediaSubsession& mediaSubsession)
    {
        // We need to extract SPS and PPS from SDP attribute and append it to MPEG2VIDEOINFO structure
        // see: http://msdn.microsoft.com/en-us/library/dd757808%28v=vs.85%29.aspx, pg: H.264 Bitstream Without Start Codes
        unsigned numSPropRecords;
        SPropRecord* sPropRecords = parseSPropParameterSets(
            mediaSubsession.attrVal_str("sprop-parameter-sets"), numSPropRecords);
        size_t decoderSpecificSize = 0;
        for (unsigned i = 0; i < numSPropRecords; ++i)
            decoderSpecificSize += sPropRecords[i].sPropLength + sequenceHeaderLengthFieldSize;

        // Allocate format buffer
        size_t mpeg2VideoInfoBuffer = sizeof(MPEG2VIDEOINFO)+decoderSpecificSize - sizeof(DWORD);
        MPEG2VIDEOINFO* pVid = (MPEG2VIDEOINFO*)mediaType.AllocFormatBuffer(mpeg2VideoInfoBuffer);
        if (!pVid) return E_OUTOFMEMORY;
        ZeroMemory(pVid, sizeof(MPEG2VIDEOINFO));

        unsigned videoWidth = 0, videoHeight = 0;
        double videoFramerate = 0.0;

        // Move SPS and PPS to format buffer (sequence header part)
        pVid->cbSequenceHeader = decoderSpecificSize;
        BYTE* dstSequenceHeader = (BYTE*)&pVid->dwSequenceHeader;
        for (unsigned i = 0; i < numSPropRecords; ++i)
        {
            SPropRecord& prop = sPropRecords[i];

            // It's SPS (Sequence parameter set
            if ((prop.sPropBytes[0] & 0x1F) == 7)
            {
                H264StreamParser h264StreamParser(prop.sPropBytes, prop.sPropLength);
                videoWidth = h264StreamParser.GetWidth();
                videoHeight = h264StreamParser.GetHeight();
                videoFramerate = h264StreamParser.GetFramerate();
            }

            // Two-byte length field in network-byte order
            uint16_t lengthField = prop.sPropLength;
            dstSequenceHeader[0] = ((uint8_t*)&lengthField)[1];
            dstSequenceHeader[1] = ((uint8_t*)&lengthField)[0];

            memcpy(dstSequenceHeader + sequenceHeaderLengthFieldSize, prop.sPropBytes, prop.sPropLength);
            dstSequenceHeader += sequenceHeaderLengthFieldSize + prop.sPropLength;
        }

        delete[] sPropRecords;

        dstSequenceHeader = (BYTE*)&pVid->dwSequenceHeader;
        pVid->dwStartTimeCode = 0;
        pVid->dwProfile = dstSequenceHeader[sequenceHeaderLengthFieldSize + 1];
        pVid->dwLevel = dstSequenceHeader[sequenceHeaderLengthFieldSize + 3];
        pVid->dwFlags = lengthFieldSize;

        // VIDEOINFOHEADER2
        SetRect(&pVid->hdr.rcSource, 0, 0, videoWidth, videoHeight);
        SetRect(&pVid->hdr.rcTarget, 0, 0, videoWidth, videoHeight);

        REFERENCE_TIME timePerFrame = std::abs(videoFramerate) > 1e-6 ?
            (REFERENCE_TIME)(UNITS / videoFramerate) : 0;
        pVid->hdr.AvgTimePerFrame = timePerFrame;

        // BITMAPINFOHEADER
        pVid->hdr.bmiHeader.biSize = sizeof BITMAPINFOHEADER;
        pVid->hdr.bmiHeader.biWidth = videoWidth;
        pVid->hdr.bmiHeader.biHeight = videoHeight;
        pVid->hdr.bmiHeader.biCompression = MAKEFOURCC('a','v','c','1');

        mediaType.SetType(&MEDIATYPE_Video);
        mediaType.SetSubtype(&MEDIASUBTYPE_AVC1);
        mediaType.SetFormatType(&FORMAT_MPEG2Video);
        mediaType.SetTemporalCompression(TRUE);
        mediaType.SetSampleSize(0);

        return S_OK;
    }

    HRESULT GetMediaTypeAAC(CMediaType& mediaType, MediaSubsession& mediaSubsession)
    {
        // fmtp_configuration() looks like 1490. We need to convert it to 0x14 0x90
        const char* configuration = mediaSubsession.fmtp_configuration();
        int decoderSpecificSize = strlen(configuration) / 2;
        std::vector<uint8_t> decoderSpecific(decoderSpecificSize);
        for (int i = 0; i < decoderSpecificSize; ++i)
        {
            char hexStr[] = { configuration[2*i], configuration[2*i + 1], '\0' };
            uint8_t hex = static_cast<uint8_t>(strtoul(hexStr, nullptr, 16));
            decoderSpecific[i] = hex;
        }

        const int samplingFreqs[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                      16000, 12000, 11025, 8000,  7350,  0,     0,     0 };

        const size_t waveFormatBufferSize = sizeof(WAVEFORMATEX) + decoderSpecificSize;
        WAVEFORMATEX* pWave = (WAVEFORMATEX*)mediaType.AllocFormatBuffer(waveFormatBufferSize);
        if (!pWave) return E_OUTOFMEMORY;
        ZeroMemory(pWave, waveFormatBufferSize);

        pWave->wFormatTag = WAVE_FORMAT_RAW_AAC1;
        pWave->nChannels = (decoderSpecific[1] & 0x78) >> 3;
        pWave->nSamplesPerSec = samplingFreqs[((decoderSpecific[0] & 0x7) << 1) + ((decoderSpecific[1] & 0x80) >> 7)];
        pWave->nBlockAlign = 1;
        //pWave->nAvgBytesPerSec = 0;
        //pWave->wBitsPerSample = 16; // Can be 0 I guess
        pWave->cbSize = decoderSpecificSize;
        CopyMemory(pWave + 1, decoderSpecific.data(), decoderSpecificSize);

        mediaType.SetType(&MEDIATYPE_Audio);
        mediaType.SetSubtype(&MEDIASUBTYPE_RAW_AAC1);
        mediaType.SetFormatType(&FORMAT_WaveFormatEx);
        //_mediaType.SetSampleSize(256000); // Set to 1 on InitMedia
        mediaType.SetTemporalCompression(FALSE);

        return S_OK;
    }

    // Works only for H264/AVC1
    bool IsIdrFrame(const MediaPacketSample& mediaPacket)
    {
        const uint8_t* data = mediaPacket.data();
        // Take 5 LSBs and compare with 5 (IDR)
        // More NAL types: http://gentlelogic.blogspot.com/2011/11/exploring-h264-part-2-h264-bitstream.html
        return (data[0] & 0x1F) == 5;
    }
}