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

#include <dshow.h>
#include "EVRPlayer.h"

struct PointF
{
    float   x;
    float   y;

    PointF() : x(0), y(0) {}
    PointF(float x, float y) : x(x), y(y) {}        
};


enum PlaybackState
{
	STATE_RUNNING,
	STATE_PAUSED,
	STATE_STOPPED,
	STATE_CLOSED
};

// GraphEventCallback: 
// Defines a callback for the application to handle filter graph events.
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

	HRESULT OpenFile(const WCHAR* sFileName, const CLSID& clsidPresenter = GUID_NULL);
	
	// Streaming
	HRESULT Play();
	HRESULT Pause();
	HRESULT Stop();
    HRESULT Step(DWORD dwFrames);

	// Video functionality
	BOOL    HasVideo() const { return m_pDisplay != NULL; }
	HRESULT UpdateVideoWindow(const LPRECT prc);
	HRESULT RepaintVideo();
    HRESULT EnableStream(DWORD iPin, BOOL bEnable);

    // Subpicture stream
    BOOL    HasSubstream() const { return m_pMixer != NULL; }
    HRESULT SetScale(float fScale);
    HRESULT HitTest(const POINT& pt);
    HRESULT SetHilite(HBITMAP hBitmap, float fAlpha);
    HRESULT Track(const POINT& pt);
    HRESULT EndTrack();

	// Filter graph events
	HRESULT HandleGraphEvent(GraphEventCallback *pCB);

	// Seeking
	BOOL	CanSeek() const;
	HRESULT SetPosition(REFERENCE_TIME pos);
	HRESULT GetStopTime(LONGLONG *pDuration);
	HRESULT GetCurrentPosition(LONGLONG *pTimeNow);

private:
	HRESULT InitializeGraph();
	void	TearDownGraph();
	HRESULT	RenderStreams(IBaseFilter *pSource);

	PlaybackState	m_state;

	HWND			m_hwndVideo;	// Video clipping window
	HWND			m_hwndEvent;	// Window to receive events
	UINT			m_EventMsg;		// Windows message for graph events

	DWORD			m_seekCaps;		// Caps bits for IMediaSeeking

    GUID            m_clsidPresenter;   // CLSID of a custom presenter.

    // Substream
    float           m_fScale;
    PointF          m_ptHitTrack;

    // Filter graph interfaces.
	IGraphBuilder	*m_pGraph;
	IMediaControl	*m_pControl;
	IMediaEventEx	*m_pEvent;
	IMediaSeeking	*m_pSeek;

    // EVR filter
    IBaseFilter             *m_pEVR;
    IMFVideoDisplayControl  *m_pDisplay;
    IMFVideoMixerControl    *m_pMixer;
    IMFVideoMixerBitmap     *m_pBitmap;
    IMFVideoPositionMapper  *m_pMapper;
};

