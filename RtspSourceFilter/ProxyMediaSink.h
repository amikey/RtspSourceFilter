#pragma once

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#include "MediaPacketSample.h"
#include "RtspSourceFilter.h"

/*
 * Media sink that accumulates received frames into given queue
 */
class ProxyMediaSink : public MediaSink
{
public:
    ProxyMediaSink(UsageEnvironment& env, MediaSubsession& subsession,
                   MediaPacketQueue& mediaPacketQueue, size_t receiveBufferSize);
    virtual ~ProxyMediaSink();

    static void afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
                                  struct timeval presentationTime, unsigned durationInMicroseconds);

    void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                           struct timeval presentationTime, unsigned durationInMicroseconds);

private:
    virtual Boolean continuePlaying();

private:
    size_t _receiveBufferSize;
    uint8_t* _receiveBuffer;
    MediaSubsession& _subsession;
    MediaPacketQueue& _mediaPacketQueue;
};
