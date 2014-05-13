#include "Debug.h"

#ifdef DEBUG
#pragma warning(push)
#pragma warning(disable : 4995)
void DebugLog(const char* fmt, ...)
{
    char dest[1024];
    va_list argptr;
    va_start(argptr, fmt);
    vsprintf(dest, fmt, argptr);
    va_end(argptr);
    OutputDebugStringA(dest);
}
#pragma warning(pop)
MyUsageEnvironment::MyUsageEnvironment(TaskScheduler& taskScheduler)
    : BasicUsageEnvironment(taskScheduler)
{
}

MyUsageEnvironment* MyUsageEnvironment::createNew(TaskScheduler& taskScheduler)
{
    return new MyUsageEnvironment(taskScheduler);
}

UsageEnvironment& MyUsageEnvironment::operator<<(char const* str)
{
    OutputDebugStringA(str);
    return *this;
}

UsageEnvironment& MyUsageEnvironment::operator<<(int i)
{
    sprintf_s(buffer, sizeof(buffer), "%d", i);
    OutputDebugStringA(buffer);
    return *this;
}

UsageEnvironment& MyUsageEnvironment::operator<<(unsigned u)
{
    sprintf_s(buffer, sizeof(buffer), "%u", u);
    OutputDebugStringA(buffer);
    return *this;
}

UsageEnvironment& MyUsageEnvironment::operator<<(double d)
{
    sprintf_s(buffer, sizeof(buffer), "%f", d);
    OutputDebugStringA(buffer);
    return *this;
}

UsageEnvironment& MyUsageEnvironment::operator<<(void* p)
{
    sprintf_s(buffer, sizeof(buffer), "%p", p);
    OutputDebugStringA(buffer);
    return *this;
}
char MyUsageEnvironment::buffer[2048] = {};
#endif