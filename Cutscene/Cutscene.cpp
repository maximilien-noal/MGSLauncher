// Cutscene.cpp : Defines the exported functions for the DLL application.
//
#include <Windows.h>
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

IGraphBuilder	*pGB = NULL;
IMediaControl	*pMC = NULL;
IMediaEventEx	*pME = NULL;
IBasicAudio		*pBA = NULL;
RECT			g_rcDest = { 0 };

// VMR9 interfaces
IVMRWindowlessControl9 *pWC = NULL;

DWORD		g_dwGraphRegister = 0;
HWND		gameWindow = 0;
PLAYSTATE	g_psCurrent = Stopped;
BOOL		hookedWndProc = false;

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

void OnPaint(HWND hwnd)
{
	HRESULT hr;
	PAINTSTRUCT ps;
	HDC         hdc;
	RECT        rcClient;

	GetClientRect(hwnd, &rcClient);
	hdc = BeginPaint(hwnd, &ps);

	if (pWC)
	{
		// When using VMR Windowless mode, you must explicitly tell the
		// renderer when to repaint the video in response to WM_PAINT
		// messages.  This is most important when the video is stopped
		// or paused, since the VMR won't be automatically updating the
		// window as the video plays.
		hr = pWC->RepaintVideo(hwnd, hdc);
	}
	else  // No video image. Just paint the whole client area.
	{
		FillRect(hdc, &rcClient, (HBRUSH)(COLOR_BTNFACE + 1));
	}

	EndPaint(hwnd, &ps);
}

void MoveVideoWindow(void)
{
	HRESULT hr;

	// Track the movement of the container window and resize as needed
	if (pWC)
	{
		GetClientRect(gameWindow, &g_rcDest);
		hr = pWC->SetVideoPosition(NULL, &g_rcDest);
	}
}

HRESULT HandleGraphEvent(void)
{
	LONG evCode;
	LONG_PTR evParam1, evParam2;
	HRESULT hr = S_OK;

	// Make sure that we don't access the media event interface
	// after it has already been released.
	if (!pME)
		return S_OK;

	// Process all queued events
	while (SUCCEEDED(pME->GetEvent(&evCode, &evParam1, &evParam2, 0)))
	{
		// Free memory associated with callback, since we're not using it
		hr = pME->FreeEventParams(evCode, evParam1, evParam2);
	}

	return hr;
}

static LRESULT CALLBACK CutsceneWndProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_PAINT:
			OnPaint(gameWindow);
			break;

		case WM_DISPLAYCHANGE:
			if (pWC)
				pWC->DisplayModeChanged();
			break;

		// Resize the video when the window changes
		case WM_MOVE:
		case WM_SIZE:
			MoveVideoWindow();
			break;

		case WM_KEYDOWN:
			CloseClip();
			break;

		case WM_GRAPHNOTIFY:
			HandleGraphEvent();
			break;
		default:
			return CallWindowProc((WNDPROC)GetWindowLongPtr(gameWindow, GWLP_WNDPROC), gameWindow, message, wParam, lParam);
	}

	return CallWindowProc((WNDPROC)GetWindowLongPtr(gameWindow, GWLP_WNDPROC), gameWindow, message, wParam, lParam);
}



void ErrorExit(LPTSTR lpszFunction)
{
	// Retrieve the system error message for the last-error code

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	// Display the error message and exit the process

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"),
		lpszFunction, dw, lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(dw);
}

HMODULE GetCurrentModuleHandle()
{
	HMODULE hMod = NULL;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&GetCurrentModuleHandle), &hMod);
	return hMod;
}


HRESULT PlayVideo(LPTSTR szMovie, HINSTANCE processHandle, HWND gameWindow)
{
	HRESULT hr;
	gameWindow = gameWindow;

	if (hookedWndProc == false)
	{
		HHOOK hook = SetWindowsHookEx(WH_CALLWNDPROC, (HOOKPROC)&CutsceneWndProc, GetCurrentModuleHandle(), 0);
		if (hook == NULL)
		{
			ErrorExit(L"SetWindowsHookEx");
		}
		hookedWndProc = true;
	}

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
		JIF(pME->SetNotifyWindow((OAHWND)gameWindow, WM_GRAPHNOTIFY, 0));

		// Complete initialization
		ShowWindow(gameWindow, SW_SHOWNORMAL);
		UpdateWindow(gameWindow);
		SetForegroundWindow(gameWindow);
		SetFocus(gameWindow);

		hr = AddGraphToRot(pGB, &g_dwGraphRegister);
		if (FAILED(hr))
		{
			Msg(TEXT("Failed to register filter graph with ROT!  hr=0x%x"), hr);
			g_dwGraphRegister = 0;
		}

		// Run the graph to play the media file
		JIF(pMC->Run());
		g_psCurrent = Running;

		SetFocus(gameWindow);
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
	GetClientRect(gameWindow, &rect);
	InvalidateRect(gameWindow, &rect, TRUE);
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

			hr = pVmr->QueryInterface(IID_IVMRWindowlessControl9, (void**)&pWC);
			if (SUCCEEDED(hr))
			{
				MIF(pWC->SetVideoClippingWindow(gameWindow));
				MIF(pWC->SetBorderColor(RGB(0, 0, 0)));
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

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)L"") + 40) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%d: %s"),
		L"", dw, lpMsgBuf);

	// This sample uses a simple message box to convey warning and error
	// messages.   You may want to display a debug string or suppress messages
	// altogether, depending on your application.
	MessageBox(NULL, szBuffer, (LPCWSTR)lpDisplayBuf, MB_OK);
}


