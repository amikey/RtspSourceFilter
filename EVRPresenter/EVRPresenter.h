//////////////////////////////////////////////////////////////////////////
//
// EVRPresenter.h : Internal header for building the DLL.
// 
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//
//////////////////////////////////////////////////////////////////////////

#pragma once

#include <windows.h>
#include <intsafe.h>
#include <math.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <d3d9.h>
#include <dxva2api.h>
#include <evr9.h>
#include <evcode.h> // EVR event codes (IMediaEventSink)

// Common helper code.
#define USE_LOGGING
#include "common.h"
#include "registry.h"
using namespace MediaFoundationSamples;

#define CHECK_HR(hr) IF_FAILED_GOTO(hr, done)

typedef ComPtrList<IMFSample>           VideoSampleList;

// Custom Attributes

// MFSamplePresenter_SampleCounter
// Data type: UINT32
//
// Version number for the video samples. When the presenter increments the version
// number, all samples with the previous version number are stale and should be
// discarded.
static const GUID MFSamplePresenter_SampleCounter = 
{ 0xb0bb83cc, 0xf10f, 0x4e2e, { 0xaa, 0x2b, 0x29, 0xea, 0x5e, 0x92, 0xef, 0x85 } };

// MFSamplePresenter_SampleSwapChain
// Data type: IUNKNOWN
// 
// Pointer to a Direct3D swap chain.
static const GUID MFSamplePresenter_SampleSwapChain = 
{ 0xad885bd1, 0x7def, 0x414a, { 0xb5, 0xb0, 0xd3, 0xd2, 0x63, 0xd6, 0xe9, 0x6d } };

MIDL_INTERFACE("576DCFD9-3C6B-4317-A635-2A1494E08B75")
IEVRPresenterCallback : public IUnknown
{
public:
    STDMETHOD(PresentSurfaceCB)(IDirect3DSurface9 *pSurface) = 0;
};

MIDL_INTERFACE("F3AB6A07-5A21-4034-908A-A9F48FAC2F63")
IEVRPresenterRegisterCallback : public IUnknown
{
public:
    STDMETHOD(RegisterCallback)(IEVRPresenterCallback *pCallback) = 0;
};

MIDL_INTERFACE("9B594575-859E-4848-812B-441406F94E5F")
IEVRPresenterSettings : public IUnknown
{
public:
    STDMETHOD(SetBufferCount)(int bufferCount) = 0;
};

// Project headers.
#include "Helpers.h"
#include "Scheduler.h"
#include "PresentEngine.h"
#include "Presenter.h"


