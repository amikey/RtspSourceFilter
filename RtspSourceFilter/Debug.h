#pragma once

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#ifdef _DEBUG
#define DEBUG
#endif

#ifdef DEBUG
// UsageEnvironment that outputs to DebugString instead of stderr
class MyUsageEnvironment : public BasicUsageEnvironment
{
public:
    static MyUsageEnvironment* createNew(TaskScheduler& taskScheduler);
    virtual UsageEnvironment& operator<<(char const* str);
    virtual UsageEnvironment& operator<<(int i);
    virtual UsageEnvironment& operator<<(unsigned u);
    virtual UsageEnvironment& operator<<(double d);
    virtual UsageEnvironment& operator<<(void* p);

protected:
    static char buffer[2048];
    MyUsageEnvironment(TaskScheduler& taskScheduler);
};
void DebugLog(const char* fmt, ...);
#else
typedef BasicUsageEnvironment MyUsageEnvironment;
#define DebugLog(fmt, ...) 
#endif

