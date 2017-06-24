// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the CUTSCENE_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// CUTSCENE_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef CUTSCENE_EXPORTS
#define CUTSCENE_API extern "C" __declspec(dllexport)
#else
#define CUTSCENE_API extern "C" __declspec(dllimport)
#endif

#include <windows.h>

CUTSCENE_API HRESULT PlayVideo(LPTSTR lpszMovie, HINSTANCE processHandle, HWND gameWindow);
