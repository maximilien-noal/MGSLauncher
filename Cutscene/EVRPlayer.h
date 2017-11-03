//////////////////////////////////////////////////////////////////////////
// EVRPlayer.h: Main header.
// 
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////

#pragma

#include <windows.h>
#include <dshow.h>
#include <dvdmedia.h>

#include <mfapi.h>
#include <mfidl.h>
#include <evr.h>
#include <d3d9.h>
#include <evr9.h>
#include <mferror.h>

#include "DShowPlayer.h"


#ifndef CHECK_HR
#define IF_FAILED_GOTO(hr, label) if (FAILED(hr)) { goto label; }
#define CHECK_HR(hr) IF_FAILED_GOTO(hr, done)
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(x) if (x !=NULL) { delete x; x = NULL; }
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x != NULL) { x->Release(); x = NULL; } 
#endif