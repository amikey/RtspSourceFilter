# RtspSourceFilter - DirectShow source filter for RTSP streaming

RtspSourceFilter implements a DirectShow source filter on top of [live555](http://www.live555.com/liveMedia/) library.

For now there are only H264+AAC streams supported.

In order to cope with single-threaded nature of live555 RtspSourceFilter use future+promise mechanism and dedicated thread to talk to live555 internals.

## Building:

Build using attached self-contained solution: live555 + BaseClasses + filter itself. Tested with Visual Studio 2013.
Additionaly, there are also VMR9 and EVR (Windows7+) presenters bundled. 

## Usage:

Output dll file must be registered as a COM library (as any DirectShow filter):

```sh
regsvr32 RtspSourceFilter.ax
```
Note that Visual Studio project does it automatically after every successful build.


RtspSourceFilter implements IFileSourceFilter interface for specyfing target URL for RTSP stream.
In order to customize its behaviour such as automatic reconnection with configurable period or streaming over TCP use IRtspSourceConfig interface:

```cpp
_COM_SMARTPTR_TYPEDEF(IBaseFilter, __uuidof(IBaseFilter));
_COM_SMARTPTR_TYPEDEF(IRtspSourceConfig, __uuidof(IRtspSourceConfig));
_COM_SMARTPTR_TYPEDEF(IFileSourceFilter, __uuidof(IFileSourceFilter));

IBaseFilterPtr pRtspSource(CLSID_RtspSourceFilter);

IRtspSourceConfigPtr pRtspConfig(pRtspSource);
pRtspConfig->SetStreamingOverTcp(FALSE);
pRtspConfig->SetLatency(500);
pRtspConfig->SetAutoReconnectionPeriod(5000);

IFileSourceFilterPtr fileRtspSource(pRtspSource);
hr = fileRtspSource->Load(L"rtsp://184.72.239.149/vod/mp4:BigBuckBunny_115k.mov", nullptr);
if (FAILED(hr))
    _com_issue_error(hr);
```

For simple testing and prototyping you can use GraphEdit bundled with now pretty old Microsoft DirectShow SDK or (better) use modern alternatives such as [GraphStudio](http://blog.monogram.sk/janos/tools/monogram-graphstudio/) or [GraphStudioNext](https://github.com/cplussharp/graph-studio-next).

## Examples

RtspSourceFilterTest is a very basic C++ project that creates DirectShow graph from scratch and plays sample video+audio stream. It shows how to use your own (basic though) presenter callback. Requires [LAVVideo](https://github.com/Nevcairiel/LAVFilters) filter to be present.

For more practical usage check example project RtspSourceWpf with custom WPF user control.
