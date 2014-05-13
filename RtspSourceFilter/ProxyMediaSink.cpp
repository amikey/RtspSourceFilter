#include "ProxyMediaSink.h"

ProxyMediaSink::ProxyMediaSink(UsageEnvironment& env, MediaSubsession& subsession,
    MediaPacketQueue& mediaPacketQueue, size_t receiveBufferSize)
    : MediaSink(env)
    , _receiveBufferSize(receiveBufferSize)
    , _receiveBuffer(new uint8_t[receiveBufferSize])
    , _subsession(subsession)
    , _mediaPacketQueue(mediaPacketQueue)
{
}

ProxyMediaSink::~ProxyMediaSink()
{
    delete[] _receiveBuffer;
}

void ProxyMediaSink::afterGettingFrame(void* clientData, unsigned frameSize,
    unsigned numTruncatedBytes, struct timeval presentationTime, unsigned durationInMicroseconds)
{
    ProxyMediaSink* sink = static_cast<ProxyMediaSink*>(clientData);
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void ProxyMediaSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
    struct timeval presentationTime, unsigned durationInMicroseconds)
{
    if (numTruncatedBytes == 0)
    {
        bool isRtcpSynced = _subsession.rtpSource() && 
                            _subsession.rtpSource()->hasBeenSynchronizedUsingRTCP();
        _mediaPacketQueue.push(MediaPacketSample(_receiveBuffer, frameSize, presentationTime, isRtcpSynced));
    }
    else
    {
    }

    continuePlaying();
}

Boolean ProxyMediaSink::continuePlaying()
{
    if (fSource == nullptr) return False;
    fSource->getNextFrame(_receiveBuffer, _receiveBufferSize,
        afterGettingFrame, this, onSourceClosure, this);
    return True;
}
