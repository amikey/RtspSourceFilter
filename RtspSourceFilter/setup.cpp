#include "RtspSource.h"

#include <initguid.h>
#include <streams.h>

#include "RtspSourceGuids.h"
// Note: It is better to register no media types than to register a partial 
// media type (subtype == GUID_NULL) because that can slow down intelligent connect 
// for everyone else.

// For a specialized source filter like this, it is best to leave out the
// AMOVIESETUP_FILTER altogether, so that the filter is not available for
// intelligent connect. Instead, use the CLSID to create the filter or just
// use 'new' in your application.

#define FILTER_NAME L"RTSP Source Filter"

// Filter setup data
const AMOVIESETUP_MEDIATYPE sudOpPinTypes = {&MEDIATYPE_Video,  // Major type
                                             &MEDIASUBTYPE_NULL // Minor type
};

const AMOVIESETUP_PIN sudOutputPinBitmap = {L"Output", // Obsolete, not used.
                                            FALSE,     // Is this pin rendered?
                                            TRUE,      // Is it an output pin?
                                            FALSE,     // Can the filter create zero instances?
                                            FALSE,     // Does the filter create multiple instances?
                                            &CLSID_NULL,   // Obsolete.
                                            NULL,          // Obsolete.
                                            1,             // Number of media types.
                                            &sudOpPinTypes // Pointer to media types.
};

const AMOVIESETUP_FILTER sudPushSourceBitmap = {&CLSID_RtspSourceFilter, // Filter CLSID
                                                FILTER_NAME,             // String name
                                                MERIT_DO_NOT_USE,        // Filter merit
                                                1,                       // Number pins
                                                &sudOutputPinBitmap      // Pin details
};

// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance.
// We provide a set of filters in this one DLL.
CFactoryTemplate g_Templates[] = {
    {sudPushSourceBitmap.strName,      // Name
     sudPushSourceBitmap.clsID,        // CLSID
     RtspSourceFilter::CreateInstance, // Method to create an instance of
                                       // MyComponent
     NULL,                             // Initialization function
     &sudPushSourceBitmap              // Set-up information (for filters)
    }};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

STDAPI DllRegisterServer() { return AMovieDllRegisterServer2(TRUE); }
STDAPI DllUnregisterServer() { return AMovieDllRegisterServer2(FALSE); }

//
// DllEntryPoint
//
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
    return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}