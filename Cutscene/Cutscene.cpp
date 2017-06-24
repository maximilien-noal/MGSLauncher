// Cutscene.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "Cutscene.h"
#include "DShowPlayer.h"

const UINT WM_GRAPH_EVENT = WM_APP + 1;
const UINT WM_GRAPHNOTIFY = WM_USER + 13;

HWND			gameWindow = 0;
BOOL			hookedWndProc = false;
DShowPlayer		*m_pPlayer = NULL;
void(*graphEventfunctionPtr)(long, LONG_PTR, LONG_PTR);

//
// Function prototypes
//
void Msg(TCHAR *szFormat, ...);

//
// Helper Macros (Jump-If-Failed, Log-If-Failed)
//

#define MIF(x) if (FAILED(hr=(x))) \
    {Msg(TEXT("FAILED(hr=0x%x) in ") TEXT(#x) TEXT("\n\0"), hr); goto CLEANUP;}

HMODULE GetCurrentModuleHandle()
{
	HMODULE hMod = NULL;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&GetCurrentModuleHandle), &hMod);
	return hMod;
}

void OnSize()
{
	if (m_pPlayer->State() != STATE_CLOSED && m_pPlayer->HasVideo())
	{
		RECT rcWindow;
		// Find the client area of the application.
		GetClientRect(gameWindow, &rcWindow);
		//Notify the player.
		m_pPlayer->UpdateVideoWindow(&rcWindow);
	}
}

void OnPaint()
{
	PAINTSTRUCT ps;
	HDC hdc;

	hdc = BeginPaint(gameWindow, &ps);

	if (m_pPlayer->State() != STATE_CLOSED && m_pPlayer->HasVideo())
	{
		// The player has video, so ask the player to repaint. 
		m_pPlayer->Repaint(hdc);
	}

	EndPaint(gameWindow, &ps);
}

void OnStop()
{
	HRESULT hr = m_pPlayer->Stop();

	// Seek back to the start. 
	if (SUCCEEDED(hr))
	{
		if (m_pPlayer->CanSeek())
		{
			hr = m_pPlayer->SetPosition(0);
		}
	}

	//Destroy player
	m_pPlayer->~DShowPlayer();
}

static void OnGraphEvent(long eventCode, LONG_PTR param1, LONG_PTR param2)
{
	switch (eventCode)
	{
	case EC_COMPLETE:
		OnStop();
		break;
	}
}

static LRESULT CALLBACK CutsceneWndProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_SIZE:
			OnSize();
			break;

		case WM_PAINT:
			OnPaint();
			break;

		case WM_MOVE:
			OnPaint();
			break;

		case WM_DISPLAYCHANGE:
			m_pPlayer->DisplayModeChanged();
			break;

		case WM_ERASEBKGND:
			return 1;

		case WM_KEYDOWN:
			OnStop();
			break;

		case WM_GRAPHNOTIFY:
			graphEventfunctionPtr = &OnGraphEvent;
			m_pPlayer->HandleGraphEvent(graphEventfunctionPtr);
			break;

		case WM_GRAPH_EVENT:
			graphEventfunctionPtr = &OnGraphEvent;
			m_pPlayer->HandleGraphEvent(graphEventfunctionPtr);
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

	m_pPlayer = new DShowPlayer(gameWindow);

	MIF(m_pPlayer->SetEventWindow(gameWindow, WM_GRAPH_EVENT));

	MIF(m_pPlayer->OpenFile(szMovie));


	// Invalidate the appliction window, in case there is an old video 
	// frame from the previous file and there is no video now. (eg, the
	// new file is audio only, or we failed to open this file.)
	InvalidateRect(gameWindow, NULL, FALSE);

	// If this file has a video stream, we need to notify 
	// the VMR about the size of the destination rectangle.
	// Invoking our OnSize() handler does this.
	OnSize();

	//We need to paint the video stream for the first time.
	//Invoking our OnPaint() handler does this.
	OnPaint();

	MIF(m_pPlayer->Play());

	while (m_pPlayer->State() == STATE_RUNNING)
	{
		MSG msg;

		// Give system threads time to run (and don't sample user input madly)
		Sleep(100);

		// Check and process window messages (like WM_KEYDOWN)
		while (PeekMessage(&msg, gameWindow, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return hr;

CLEANUP:
	m_pPlayer->~DShowPlayer();
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


