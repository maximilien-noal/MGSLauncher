// Cutscene.cpp : Defines the exported functions for the DLL application.
//
#include <dshow.h>
#include <strsafe.h>
#include <vmr9.h>

#include "stdafx.h"
#include "Cutscene.h"

#include <dshow.h>
#include <d3d9.h>
#include <vmr9.h>

#include "smartptr.h"
#include "common\dshowutil.h"
#include "windowless.h"

//
// Constants
//
#define KEYBOARD_SAMPLE_FREQ  100  // Sample user input on an interval

IGraphBuilder  *pGB = NULL;
IMediaControl  *pMC = NULL;
IMediaEventEx  *pME = NULL;
IBasicAudio    *pBA = NULL;

// VMR9 interfaces
IVMRWindowlessControl9 *pVW = NULL;

DWORD g_dwGraphRegister = 0;
HWND ghApp = 0;
PLAYSTATE g_psCurrent = Stopped;

//
// Function prototypes
//
void Msg(TCHAR *szFormat, ...);
void CloseInterfaces(void);
void CloseClip(void);
HRESULT InitializeWindowlessVMR(IBaseFilter **ppVmr9);

//
// Helper Macros (Jump-If-Failed, Log-If-Failed)
//

#define JIF(x) if (FAILED(hr=(x))) \
    {Msg(TEXT("FAILED(hr=0x%x) in ") TEXT(#x) TEXT("\n\0"), hr); goto CLEANUP;}

#define MIF(x) if (FAILED(hr=(x))) \
    {Msg(TEXT("FAILED(hr=0x%x) in ") TEXT(#x) TEXT("\n\0"), hr);}

#define LIF(x) if (FAILED(hr=(x))) \
    {Msg(TEXT("FAILED(hr=0x%x) in ") TEXT(#x) TEXT("\n\0"), hr); return hr;}



HRESULT PlayVideo(LPTSTR szMovie, HINSTANCE processHandle, HWND gameWindow)
{
	HRESULT hr;
	ghApp = gameWindow;

	// Get the interface for DirectShow's GraphBuilder
	JIF(CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
		IID_IGraphBuilder, (void **)&pGB));
	
	if (SUCCEEDED(hr))
	{
		SmartPtr <IBaseFilter> pVmr;

		// Create the Video Mixing Renderer and add it to the graph
		JIF(InitializeWindowlessVMR(&pVmr));

		// Render the file programmatically to use the VMR9 as renderer.
		// Pass TRUE to create an audio renderer also.
		if (FAILED(hr = RenderFileToVideoRenderer(pGB, szMovie, TRUE)))
			return hr;

		// QueryInterface for DirectShow interfaces
		JIF(pGB->QueryInterface(IID_IMediaControl, (void **)&pMC));
		JIF(pGB->QueryInterface(IID_IMediaEventEx, (void **)&pME));
		JIF(pGB->QueryInterface(IID_IBasicAudio, (void **)&pBA));

		// Have the graph signal event via window callbacks for performance
		JIF(pME->SetNotifyWindow((OAHWND)ghApp, WM_GRAPHNOTIFY, 0));

		// Complete initialization
		ShowWindow(ghApp, SW_SHOWNORMAL);
		UpdateWindow(ghApp);
		SetForegroundWindow(ghApp);
		SetFocus(ghApp);

		hr = AddGraphToRot(pGB, &g_dwGraphRegister);
		if (FAILED(hr))
		{
			Msg(TEXT("Failed to register filter graph with ROT!  hr=0x%x"), hr);
			g_dwGraphRegister = 0;
		}

		// Run the graph to play the media file
		JIF(pMC->Run());
		g_psCurrent = Running;

		SetFocus(ghApp);
	}

	return hr;

CLEANUP:
	CloseClip();
	CloseInterfaces();
	return hr;
}

void CloseClip()
{
	HRESULT hr;

	// Stop media playback
	if (pMC)
		hr = pMC->Stop();

	// Clear global flags
	g_psCurrent = Stopped;

	// Free DirectShow interfaces
	CloseInterfaces();

	// No current media state
	g_psCurrent = Init;

	// Reset the player window
	RECT rect;
	GetClientRect(ghApp, &rect);
	InvalidateRect(ghApp, &rect, TRUE);
}


void CloseInterfaces(void)
{
	HRESULT hr;

	// Disable event callbacks
	if (pME)
		hr = pME->SetNotifyWindow((OAHWND)NULL, 0, 0);

	if (g_dwGraphRegister)
	{
		RemoveGraphFromRot(g_dwGraphRegister);
		g_dwGraphRegister = 0;
	}

	// Release and zero DirectShow interfaces
	SAFE_RELEASE(pME);
	SAFE_RELEASE(pMC);
	SAFE_RELEASE(pBA);
	SAFE_RELEASE(pGB);
}



HRESULT InitializeWindowlessVMR(IBaseFilter **ppVmr9)
{
	IBaseFilter* pVmr = NULL;

	if (!ppVmr9)
		return E_POINTER;
	*ppVmr9 = NULL;

	// Create the VMR and add it to the filter graph.
	HRESULT hr = CoCreateInstance(CLSID_VideoMixingRenderer9, NULL,
		CLSCTX_INPROC, IID_IBaseFilter, (void**)&pVmr);

	if (SUCCEEDED(hr))
	{
		hr = pGB->AddFilter(pVmr, L"Video Mixing Renderer 9");
		if (SUCCEEDED(hr))
		{
			// Set the rendering mode and number of streams
			SmartPtr <IVMRFilterConfig9> pConfig;

			MIF(pVmr->QueryInterface(IID_IVMRFilterConfig9, (void**)&pConfig));
			MIF(pConfig->SetRenderingMode(VMR9Mode_Windowless));

			hr = pVmr->QueryInterface(IID_IVMRWindowlessControl9, (void**)&pVW);
			if (SUCCEEDED(hr))
			{
				MIF(pVW->SetVideoClippingWindow(ghApp));
				MIF(pVW->SetBorderColor(RGB(0, 0, 0)));
			}
		}

		// Don't release the pVmr interface because we are copying it into
		// the caller's ppVmr9 pointer
		*ppVmr9 = pVmr;
	}

	return hr;
}

void Msg(TCHAR *szFormat, ...)
{
	TCHAR szBuffer[1024];  // Large buffer for long filenames or URLs
	const size_t NUMCHARS = sizeof(szBuffer) / sizeof(szBuffer[0]);
	const int LASTCHAR = NUMCHARS - 1;

	// Format the input string
	va_list pArgs;
	va_start(pArgs, szFormat);

	// Use a bounded buffer size to prevent buffer overruns.  Limit count to
	// character size minus one to allow for a NULL terminating character.
	(void)StringCchVPrintf(szBuffer, NUMCHARS - 1, szFormat, pArgs);
	va_end(pArgs);

	// Ensure that the formatted string is NULL-terminated
	szBuffer[LASTCHAR] = TEXT('\0');

	// This sample uses a simple message box to convey warning and error
	// messages.   You may want to display a debug string or suppress messages
	// altogether, depending on your application.
	MessageBox(NULL, szBuffer, TEXT("PlayVideo Error"), MB_OK);
}


