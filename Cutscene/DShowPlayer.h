#pragma once
//////////////////////////////////////////////////////////////////////////
// DShowPlayer.h: Implements DirectShow playback functionality.
// 
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////

#pragma once

// Windows Header Files:
#include <windows.h>
#include <strsafe.h>
#include <dshow.h>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) { if (x) { x->Release(); x = NULL; } }
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(x) { delete x; x = NULL; }
#endif


#include "video.h"

const long MIN_VOLUME = -10000;
const long MAX_VOLUME = 0;

enum PlaybackState
{
	STATE_RUNNING,
	STATE_PAUSED,
	STATE_STOPPED,
	STATE_CLOSED
};

struct GraphEventCallback
{
	virtual void OnGraphEvent(long eventCode, LONG_PTR param1, LONG_PTR param2) = 0;
};


class DShowPlayer
{
public:

	DShowPlayer(HWND hwndVideo);
	~DShowPlayer();

	HRESULT SetEventWindow(HWND hwnd, UINT msg);

	PlaybackState State() const { return m_state; }

	HRESULT OpenFile(const WCHAR* sFileName);

	// Streaming
	HRESULT Play();
	HRESULT Pause();
	HRESULT Stop();

	// VMR functionality
	BOOL    HasVideo() const;
	HRESULT UpdateVideoWindow(const LPRECT prc);
	HRESULT Repaint(HDC hdc);
	HRESULT DisplayModeChanged();

	// events
	HRESULT HandleGraphEvent(void(*pCB)(long, LONG_PTR, LONG_PTR));

	// seeking
	BOOL	CanSeek() const;
	HRESULT SetPosition(REFERENCE_TIME pos);
	HRESULT GetDuration(LONGLONG *pDuration);
	HRESULT GetCurrentPosition(LONGLONG *pTimeNow);

	// Audio
	HRESULT	Mute(BOOL bMute);
	BOOL	IsMuted() const { return m_bMute; }
	HRESULT	SetVolume(long lVolume);
	long	GetVolume() const { return m_lVolume; }

private:
	HRESULT InitializeGraph();
	void	TearDownGraph();
	HRESULT CreateVideoRenderer();
	HRESULT	RenderStreams(IBaseFilter *pSource);
	HRESULT UpdateVolume();

	PlaybackState	m_state;

	HWND			m_hwndVideo;	// Video clipping window
	HWND			m_hwndEvent;	// Window to receive events
	UINT			m_EventMsg;		// Windows message for graph events

	DWORD			m_seekCaps;		// Caps bits for IMediaSeeking

									// Audio
	BOOL            m_bAudioStream; // Is there an audio stream?
	long			m_lVolume;		// Current volume (unless muted)
	BOOL			m_bMute;		// Volume muted?		

	IGraphBuilder	*m_pGraph;
	IMediaControl	*m_pControl;
	IMediaEventEx	*m_pEvent;
	IMediaSeeking	*m_pSeek;
	IBasicAudio		*m_pAudio;

	BaseVideoRenderer   *m_pVideo;

};

HRESULT RemoveUnconnectedRenderer(IGraphBuilder *pGraph, IBaseFilter *pRenderer, BOOL *pbRemoved);
