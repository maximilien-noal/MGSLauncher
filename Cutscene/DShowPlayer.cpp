//////////////////////////////////////////////////////////////////////////
// DShowPlayer.cpp: Implements DirectShow playback functionality.
// 
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "EVRPlayer.h"
#include "common/dshowutil.h"

struct EVRSetupInfo
{
    HWND    hwndVideo;
    DWORD   dwStreams;
    GUID    clsidPresenter;    
    GUID    clsidMixer;
};

HRESULT RemoveUnconnectedRenderer(IGraphBuilder *pGraph, IBaseFilter *pRenderer, BOOL *pbRemoved);
HRESULT InitializeEVR(IBaseFilter *pEVR, const EVRSetupInfo& info, IMFVideoDisplayControl **ppDisplay);

//-----------------------------------------------------------------------------
// DShowPlayer constructor.
//-----------------------------------------------------------------------------

DShowPlayer::DShowPlayer(HWND hwndVideo) :
	m_state(STATE_CLOSED),
	m_hwndVideo(hwndVideo),
	m_hwndEvent(NULL),
	m_EventMsg(0),
	m_pGraph(NULL),
	m_pControl(NULL),
	m_pEvent(NULL),
	m_pSeek(NULL),
    m_pDisplay(NULL),
    m_pEVR(NULL),
    m_pMixer(NULL),
    m_pBitmap(NULL),
    m_pMapper(NULL),
	m_seekCaps(0),
    m_fScale(1.0f),
    m_clsidPresenter(GUID_NULL)
{

}

//-----------------------------------------------------------------------------
// DShowPlayer destructor.
//-----------------------------------------------------------------------------

DShowPlayer::~DShowPlayer()
{
	TearDownGraph();
}



//-----------------------------------------------------------------------------
// DShowPlayer::SetEventWindow
// Description: Set the window to receive graph events.
//
// hwnd: Window to receive the events.
// msg: Private window message. The window will receive this message whenever  
//      a graph event occurs. (Must be in the range WM_APP through 0xBFFF.)
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::SetEventWindow(HWND hwnd, UINT msg)
{
	m_hwndEvent = hwnd;
	m_EventMsg = msg;
	return S_OK;
}



//-----------------------------------------------------------------------------
// DShowPlayer::OpenFile
// Description: Open a new file for playback.
//
// sFileName: Name of the file to open.
// clsidPresenter: CLSID of a custom EVR presenter. 
//
// Note: Use GUID_NULL for the default presenter.
// 
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::OpenFile(const WCHAR* sFileName, const CLSID& clsidPresenter)
{
	HRESULT hr = S_OK;

	IBaseFilter *pSource = NULL;

	// Create a new filter graph. (This also closes the old one, if any.)
	CHECK_HR(hr = InitializeGraph());

    m_clsidPresenter = clsidPresenter;
	
	// Add the source filter to the graph.
	CHECK_HR(hr = m_pGraph->AddSourceFilter(sFileName, NULL, &pSource));

	// Try to render the streams.
	CHECK_HR(hr = RenderStreams(pSource));

	// Get the seeking capabilities.
	CHECK_HR(hr = m_pSeek->GetCapabilities(&m_seekCaps));

	// Update our state.
	m_state = STATE_STOPPED;

done:
    SAFE_RELEASE(pSource);
	return hr;
}

//-----------------------------------------------------------------------------
// DShowPlayer::HandleGraphEvent
// Description: Respond to a graph event.
//
// The owning window should call this method when it receives the window
// message that the application specified when it called SetEventWindow.
//
// pCB: Pointer to the GraphEventCallback callback, implemented by 
//      the application. This callback is invoked once for each event
//      in the queue. 
//
// Caution: Do not tear down the graph from inside the callback.
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::HandleGraphEvent(GraphEventCallback *pCB)
{
	if (pCB == NULL)
	{
		return E_POINTER;
	}

	if (!m_pEvent)
	{
		return E_UNEXPECTED;
	}

	long evCode = 0;
	LONG_PTR param1 = 0, param2 = 0;

	HRESULT hr = S_OK;

    // Get the events from the queue.
	while (SUCCEEDED(m_pEvent->GetEvent(&evCode, &param1, &param2, 0)))
	{
        // Invoke the callback.
		pCB->OnGraphEvent(evCode, param1, param2);

        // Free the event data.
		hr = m_pEvent->FreeEventParams(evCode, param1, param2);
		if (FAILED(hr))
		{
			break;
		}
	}

	return hr;
}


// State changes

HRESULT DShowPlayer::Play()
{
	if (m_state != STATE_PAUSED && m_state != STATE_STOPPED)
	{
		return VFW_E_WRONG_STATE;
	}

	assert(m_pGraph); // If state is correct, the graph should exist.

	HRESULT hr = m_pControl->Run();

	if (SUCCEEDED(hr))
	{
		m_state = STATE_RUNNING;
	}

	return hr;
}

HRESULT DShowPlayer::Pause()
{
	if (m_state != STATE_RUNNING)
	{
		return VFW_E_WRONG_STATE;
	}

	assert(m_pGraph); // If state is correct, the graph should exist.

	HRESULT hr = m_pControl->Pause();

	if (SUCCEEDED(hr))
	{
		m_state = STATE_PAUSED;
	}

	return hr;
}

HRESULT DShowPlayer::Stop()
{
	if (m_state != STATE_RUNNING && m_state != STATE_PAUSED)
	{
		return VFW_E_WRONG_STATE;
	}

	assert(m_pGraph); // If state is correct, the graph should exist.

	HRESULT hr = m_pControl->Stop();

	if (SUCCEEDED(hr))
	{
		m_state = STATE_STOPPED;
	}

	return hr;
}

HRESULT DShowPlayer::Step(DWORD dwFrames)
{
    if (m_pGraph == NULL)
    {
        return VFW_E_WRONG_STATE;
    }

    HRESULT hr = S_OK;
    IVideoFrameStep *pStep = NULL;

    CHECK_HR(hr = m_pGraph->QueryInterface(__uuidof(IVideoFrameStep), (void**)&pStep));

    CHECK_HR(hr = pStep->Step(dwFrames, NULL));

    // To step, the Filter Graph Manager first runs the graph. When
    // the step is complete, it pauses the graph. For the application,
    // we can just report our new state as paused.
    m_state = STATE_PAUSED;

done:
    SAFE_RELEASE(pStep);
    return hr;
}


// EVR functionality

//-----------------------------------------------------------------------------
// DShowPlayer::UpdateVideoWindow
// Description: Sets the destination rectangle for the video.
//
// prc: Destination rectangle, as a subrect of the video window's client
//      area. If NULL, the entire client area is used.
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::UpdateVideoWindow(const LPRECT prc)
{
	if (!m_pDisplay)
	{
		return S_OK; // no-op
	}

	if (prc)
	{
		return m_pDisplay->SetVideoPosition(NULL, prc);
	}
	else
	{

		RECT rc;
		GetClientRect(m_hwndVideo, &rc);
		return m_pDisplay->SetVideoPosition(NULL, &rc);
	}
    return S_OK;
}

//-----------------------------------------------------------------------------
// DShowPlayer::Repaint
// Description: Repaints the video.
//
// Call this method when the application receives WM_PAINT.
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::RepaintVideo()
{
	if (m_pDisplay)
	{
		return m_pDisplay->RepaintVideo();
	}
	else
	{
		return S_OK;
	}
}

//-----------------------------------------------------------------------------
// DShowPlayer::EnableStream
// Description: Enable or disable a stream on the EVR.
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::EnableStream(DWORD iPin, BOOL bEnable)
{
    if (m_pEVR == NULL)
    {
        return MF_E_INVALIDREQUEST;
    }

    HRESULT hr = S_OK;

    IPin *pPin = NULL;
    IEVRVideoStreamControl *pStreamControl = NULL;

    CHECK_HR(hr = FindPinByIndex(m_pEVR, PINDIR_INPUT, (UINT)iPin, &pPin));
    CHECK_HR(hr = pPin->QueryInterface(__uuidof(IEVRVideoStreamControl), (void**)&pStreamControl));
    CHECK_HR(hr = pStreamControl->SetStreamActiveState(bEnable));

done:
    SAFE_RELEASE(pPin);
    SAFE_RELEASE(pStreamControl);
    return hr;
}


// Video Mixer functionality

//-----------------------------------------------------------------------------
// SetScale
// Description: Resizes the substream image.
//
// fScale: Size of the substream image as a percentage [0.0 - 1.0].
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::SetScale(float fScale)
{
    if (fScale < 0 || fScale > 1.0)
    {
        return E_INVALIDARG;
    }

    if (fScale == m_fScale)
    {
        return S_OK; // no-op
    }

    if (m_pMixer == NULL)
    {
        return MF_E_INVALIDREQUEST;
    }

    HRESULT hr = S_OK;

    // Get the current position of the substream rectangle.
    MFVideoNormalizedRect rect;
    ZeroMemory(&rect, sizeof(rect));

    CHECK_HR(hr = m_pMixer->GetStreamOutputRect(1, &rect));

    // When this method is called, the substream might be positioned anywhere 
    // within the composition rectangle. To resize it, first we scale the 
    // right/bottom edges up to the maximum, and then scale the left/top edges.
    rect.right = min(rect.left + fScale, 1.0f);
    rect.bottom = min(rect.top + fScale, 1.0f);

    rect.left -= max(fScale - (rect.right - rect.left), 0.0f); 
    rect.top -= max(fScale - (rect.bottom - rect.top), 0.0f);

    // Set the new position.
    CHECK_HR(hr = m_pMixer->SetStreamOutputRect(1, &rect));

    m_fScale = fScale;

done:
    return hr;
}


//-----------------------------------------------------------------------------
// HitTest
// Description: Test whether a window coordinate falls within the boundaries
// of the substream rectangle.
//
// Returns S_OK if there was a hit, or S_FALSE otherwise.
//
// pt: Window coordinates, relative to the video window's client area.
//
// Note: 
// The window coordinates can be negative (ie, they can fall outside the client 
// area). If there is no substream, the method returns S_FALSE.
//
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::HitTest(const POINT& pt)
{
    if (m_pMapper == NULL)
    {
        return MF_E_INVALIDREQUEST;
    }

    HRESULT hr = S_OK;
    BOOL bHit = FALSE;

    float x = -1, y = -1;

    // Normalize the coordinates (ie, calculate them as a percentage of
    // the video window's entire client area).
    RECT rc;
    GetClientRect(m_hwndVideo, &rc);

    float x1 = (float)pt.x / rc.right;
    float y1 = (float)pt.y / rc.bottom;

    // Map these coordinates into the coordinate space of the substream.
    hr = m_pMapper->MapOutputCoordinateToInputStream(
        x1, y1, // Output coordinates
        0,      // Output stream (the mixer only has one)
        1,      // Input stream (1 = substream)
        &x, &y  // Receives the normalized input coordinates.
        );

    // If the mapped coordinates fall within [0-1], it's a hit.
    if (SUCCEEDED(hr))
    {
        bHit = ( (x >= 0) && (x <= 1) && (y >= 0) && (y <= 1) );
    }

    if (bHit)
    {
        // Store the hit point.
        // We adjust the hit point by the substream scaling factor, so that the 
        // hit point is now scaled to the reference stream. 

        // Example: 
        // - The hit point is (0.5,0.5), the center of the image.
        // - The scaling factor is 0.5, so the substream image is 50% the size
        //   of the reference stream.
        // The adjusted hit point is (0.25,0.25) FROM the origin of the substream 
        // rectangle but IN units of the reference stream. See DShowPlayer::Track 
        // for where this is used.

        m_ptHitTrack = PointF(x * m_fScale, y * m_fScale);
    }

    return bHit ? S_OK : S_FALSE;
}

//-----------------------------------------------------------------------------
// SetHilite
// Description: Applies an alpha-blended bitmap to the substream rectangle.
// 
// To remove the bitmap, call EndTrack().
//
// Notes: The client app calls this method if the user clicks on the video 
// window and HitTest() returns S_OK.
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::SetHilite(HBITMAP hBitmap, float fAlpha)
{
    if (m_pBitmap == NULL || m_pMixer == NULL)
    {
        return MF_E_INVALIDREQUEST;
    }

    HRESULT hr = S_OK;
    HDC hdc = NULL;
    HDC hdcBmp = NULL;
    HBITMAP hOld = NULL;

    // Get the current position of the substream rectangle.
    MFVideoNormalizedRect nrcDest;
    ZeroMemory(&nrcDest, sizeof(nrcDest));

    CHECK_HR(hr = m_pMixer->GetStreamOutputRect(1, &nrcDest));

    // Get the device context for the video window.
    hdc = GetDC(m_hwndVideo);

    // Create a compatible DC and select the bitmap into the DC>
    hdcBmp = CreateCompatibleDC(hdc);
    hOld = (HBITMAP)SelectObject(hdcBmp, hBitmap);

    // Fill in the blending parameters.
    MFVideoAlphaBitmap bmpInfo;
    ZeroMemory(&bmpInfo, sizeof(bmpInfo));
    bmpInfo.GetBitmapFromDC = TRUE; // Use a bitmap DC (not a Direct3D surface).
    bmpInfo.bitmap.hdc = hdcBmp;
    bmpInfo.params.dwFlags = MFVideoAlphaBitmap_Alpha | MFVideoAlphaBitmap_DestRect;
    bmpInfo.params.fAlpha = fAlpha;
    bmpInfo.params.nrcDest = nrcDest;

    // Get the bitmap dimensions.
    BITMAP bm;
    GetObject(hBitmap, sizeof(BITMAP), &bm);

    // Set the source rectangle equal to the entire bitmap.
    SetRect(&bmpInfo.params.rcSrc, 0, 0, bm.bmWidth, bm.bmHeight);

    // Set the bitmap.
    CHECK_HR(hr = m_pBitmap->SetAlphaBitmap(&bmpInfo));

done:
    if (hdcBmp != NULL)
    {
        SelectObject(hdcBmp, hOld);
        DeleteDC(hdcBmp);
    }
    if (hdc != NULL)
    {
        ReleaseDC(m_hwndVideo, hdc);
    }

    return hr;
}


//-----------------------------------------------------------------------------
// Track
// Description: Moves the substream rectangle to a new position.
//
// pt: Specifies the mouse position, relative to the client area of the video
//     window. The actual move position is offset by the original hit point,
//     as determined in HitTest().
//
// Before calling this method, the client app must call HitTest() to find 
// the original hit point. If HitTest() returns S_FALSE, do not call Track().
//
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::Track(const POINT& pt)
{
    if (m_pBitmap == NULL || m_pMixer == NULL || m_pMapper == NULL)
    {
        return MF_E_INVALIDREQUEST;
    }

    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(m_hwndVideo, &rc);

    // x, y: Mouse coordinates, normalized relative to the composition rectangle.
    float x = (float)pt.x / rc.right;
    float y = (float)pt.y / rc.bottom;

    // Map the mouse coordinates to the reference stream.
    CHECK_HR(hr = m_pMapper->MapOutputCoordinateToInputStream(
        x, y,       // Output coordinates
        0,          // Output stream (the mixer only has one)
        0,          // Input stream (0 = ref stream)
        &x, &y      // Receives the normalized input coordinates.
        ));

    // Offset by the original hit point.
    x -= m_ptHitTrack.x;
    y -= m_ptHitTrack.y;

    float max_offset = 1.0f - m_fScale; // Maximum left and top positions for the substream.

    MFVideoNormalizedRect nrcDest;

    if (x < 0)
    {
        nrcDest.left = 0;
        nrcDest.right = m_fScale;
    }
    else if (x > max_offset)
    {
        nrcDest.right = 1;
        nrcDest.left = max_offset;
    }
    else
    {
        nrcDest.left = x;
        nrcDest.right = x + m_fScale;
    }

    if (y < 0)
    {
        nrcDest.top = 0;
        nrcDest.bottom = m_fScale;
    }
    else if (y > max_offset)
    {
        nrcDest.bottom = 1;
        nrcDest.top = max_offset;
    }
    else
    {
        nrcDest.top = y;
        nrcDest.bottom = y + m_fScale;
    }

    // Set the new position.
    CHECK_HR(hr = m_pMixer->SetStreamOutputRect(1, &nrcDest));

    // Move the highlight bitmap also.
    MFVideoAlphaBitmapParams params;
    ZeroMemory(&params, sizeof(params));

    params.dwFlags = MFVideoAlphaBitmap_DestRect;
    params.nrcDest = nrcDest;

    CHECK_HR(hr = m_pBitmap->UpdateAlphaBitmapParameters(&params));

done:
    return hr;
}

//-----------------------------------------------------------------------------
// EndTrack
// Description: Removes the alpha-blended bitmap (see SetHilite).
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::EndTrack()
{
    HRESULT hr = S_OK;
    if (m_pBitmap)
    {
        hr = m_pBitmap->ClearAlphaBitmap();
    }
    return hr;
}



// Seeking

//-----------------------------------------------------------------------------
// DShowPlayer::CanSeek
// Description: Returns TRUE if the current file is seekable.
//-----------------------------------------------------------------------------

BOOL DShowPlayer::CanSeek() const
{
	const DWORD caps = AM_SEEKING_CanSeekAbsolute | AM_SEEKING_CanGetDuration;
	return ((m_seekCaps & caps) == caps);
}


//-----------------------------------------------------------------------------
// DShowPlayer::SetPosition
// Description: Seeks to a new position.
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::SetPosition(REFERENCE_TIME pos)
{
	if (m_pControl == NULL || m_pSeek == NULL)
	{
		return E_UNEXPECTED;
	}

	HRESULT hr = S_OK;

	hr = m_pSeek->SetPositions(&pos, AM_SEEKING_AbsolutePositioning,
		NULL, AM_SEEKING_NoPositioning);

	if (SUCCEEDED(hr))
	{
		// If playback is stopped, we need to put the graph into the paused
		// state to update the video renderer with the new frame, and then stop 
		// the graph again. The IMediaControl::StopWhenReady does this.
		if (m_state == STATE_STOPPED)
		{
			hr = m_pControl->StopWhenReady();
		}
	}

	return hr;
}

//-----------------------------------------------------------------------------
// DShowPlayer::GetStopTime
// Description: Gets the stop time(or duration) of the current file.
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::GetStopTime(LONGLONG *pDuration)
{
	if (m_pSeek == NULL)
	{
		return E_UNEXPECTED;
	}

    HRESULT hr = m_pSeek->GetStopPosition(pDuration);

    // If we cannot get the stop time, try to get the duration.
    if (FAILED(hr))
    {
	    hr = m_pSeek->GetDuration(pDuration);
    }
    return hr;
}

//-----------------------------------------------------------------------------
// DShowPlayer::GetCurrentPosition
// Description: Gets the current playback position.
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::GetCurrentPosition(LONGLONG *pTimeNow)
{
	if (m_pSeek == NULL)
	{
		return E_UNEXPECTED;
	}

	return m_pSeek->GetCurrentPosition(pTimeNow);
}



// Graph building

//-----------------------------------------------------------------------------
// DShowPlayer::InitializeGraph
// Description: Create a new filter graph. (Tears down the old graph.) 
//-----------------------------------------------------------------------------

HRESULT DShowPlayer::InitializeGraph()
{
	HRESULT hr = S_OK;

	TearDownGraph();

	// Create the Filter Graph Manager.
	CHECK_HR(hr = CoCreateInstance(
		CLSID_FilterGraph, 
		NULL, 
		CLSCTX_INPROC_SERVER,
		IID_IGraphBuilder,
		(void**)&m_pGraph
		));

	// Query for graph interfaces. (These interfaces are exposed by the graph
    // manager regardless of which filters are in the graph.)
	CHECK_HR(hr = m_pGraph->QueryInterface(IID_IMediaControl, (void**)&m_pControl));

	CHECK_HR(hr = m_pGraph->QueryInterface(IID_IMediaEventEx, (void**)&m_pEvent));

	CHECK_HR(hr = m_pGraph->QueryInterface(IID_IMediaSeeking, (void**)&m_pSeek));

	// Set up event notification.
	CHECK_HR(hr = m_pEvent->SetNotifyWindow((OAHWND)m_hwndEvent, m_EventMsg, NULL));

done:
	return hr;
}

//-----------------------------------------------------------------------------
// DShowPlayer::TearDownGraph
// Description: Tear down the filter graph and release resources. 
//-----------------------------------------------------------------------------

void DShowPlayer::TearDownGraph()
{
	// Stop sending event messages
	if (m_pEvent)
	{
		m_pEvent->SetNotifyWindow((OAHWND)NULL, NULL, NULL);
	}

    if (m_pControl)
    {
        m_pControl->Stop();
    }

	SAFE_RELEASE(m_pDisplay);
    SAFE_RELEASE(m_pMixer);
    SAFE_RELEASE(m_pBitmap);
    SAFE_RELEASE(m_pMapper);
    SAFE_RELEASE(m_pEVR);
	SAFE_RELEASE(m_pGraph);
	SAFE_RELEASE(m_pControl);
	SAFE_RELEASE(m_pEvent);
	SAFE_RELEASE(m_pSeek);

	m_state = STATE_CLOSED;
	m_seekCaps = 0;
}

//-----------------------------------------------------------------------------
// DShowPlayer::RenderStreams
// Description: Render the streams from a source filter. 
//-----------------------------------------------------------------------------

HRESULT	DShowPlayer::RenderStreams(IBaseFilter *pSource)
{
	HRESULT hr = S_OK;

	BOOL bRenderedAudio = FALSE;
    BOOL bRenderedVideo = FALSE;
	BOOL bRemoved = FALSE;

    EVRSetupInfo setupInfo;
    ZeroMemory(&setupInfo, sizeof(setupInfo));

	IEnumPins *pEnum = NULL;
	IBaseFilter *pEVR = NULL;
	IBaseFilter *pAudioRenderer = NULL;
    IBaseFilter *pSplitter = NULL;
	IPin *pPin = NULL;

	// Add the EVR to the graph.
    CHECK_HR(hr = AddFilterByCLSID(m_pGraph, CLSID_EnhancedVideoRenderer, &pEVR, L"EVR"));

    // Configure the EVR with 1 input stream.
    setupInfo.hwndVideo = m_hwndVideo;
    setupInfo.dwStreams = 1;
    setupInfo.clsidPresenter = m_clsidPresenter;
    setupInfo.clsidMixer = GUID_NULL;

    CHECK_HR(hr = InitializeEVR(pEVR, setupInfo, &m_pDisplay));

	// Add the DSound Renderer to the graph.
	CHECK_HR(hr = AddFilterByCLSID(m_pGraph, CLSID_DSoundRender, &pAudioRenderer, L"Audio Renderer"));

    // Enumerate the pins on the source filter.
	CHECK_HR(hr = pSource->EnumPins(&pEnum));

	// Loop through all the pins.
	while (S_OK == pEnum->Next(1, &pPin, NULL))
	{			
		HRESULT hr2 = E_FAIL;
        
        if (!bRenderedVideo)
        {
    		// Try connecting this pin to the EVR.
            hr2 = ConnectFilters(m_pGraph, pSource, pEVR);
		    if (SUCCEEDED(hr2))
		    {
			    bRenderedVideo = TRUE;
		    }
        }

        if (FAILED(hr2) && !bRenderedAudio)
        {
            // Try connecting the pin to the DSound Renderer
            hr2 = ConnectFilters(m_pGraph, pSource, pAudioRenderer);

		    if (SUCCEEDED(hr2))
		    {
			    bRenderedAudio = TRUE;
		    }
        }

        SAFE_RELEASE(pPin);
	}

    // If nothing connected, then we have failed.
    if (!bRenderedAudio && !bRenderedVideo)
    {
        CHECK_HR(hr = VFW_E_CANNOT_RENDER);
    }

    // If one did not connect, then possibly the source is connected to a splitter
    // and we need to connect the splitter. 
    if (!bRenderedAudio)
    {
        HRESULT hr2 = S_OK;

        hr2 = GetNextFilter(pSource, DOWNSTREAM, &pSplitter);

        if (SUCCEEDED(hr2))
        {
            hr2 = ConnectFilters(m_pGraph, pSplitter, pAudioRenderer);
        }
        if (SUCCEEDED(hr2))
        {
            bRenderedAudio = TRUE;
        }
    }

    if (!bRenderedVideo)
    {
        HRESULT hr2 = S_OK;

        hr2 = GetNextFilter(pSource, DOWNSTREAM, &pSplitter);
        if (SUCCEEDED(hr2))
        {
            hr2 = ConnectFilters(m_pGraph, pSplitter, pEVR);
        }
        if (SUCCEEDED(hr2))
        {
	        bRenderedVideo = TRUE;
        }
    }


	// Remove un-used renderers.

    if (!bRenderedVideo)
    {
    	CHECK_HR(hr = RemoveUnconnectedRenderer(m_pGraph, pEVR, &bRemoved));
        assert(bRemoved);
	    // If we removed the EVR, then we also need to release our 
	    // pointer to the EVR display interfaace
		SAFE_RELEASE(m_pDisplay);
	}
    else
    {
        // EVR is still in the graph. Cache the interface pointer.
        assert(pEVR != NULL);
        m_pEVR = pEVR;
        m_pEVR->AddRef();
    }

    if (!bRenderedAudio)
    {
	    bRemoved = FALSE;
	    CHECK_HR(hr = RemoveUnconnectedRenderer(m_pGraph, pAudioRenderer, &bRemoved));
        assert(bRemoved);
    }

done:
	SAFE_RELEASE(pEnum);
	SAFE_RELEASE(pEVR);
	SAFE_RELEASE(pAudioRenderer);
    SAFE_RELEASE(pSplitter);
    SAFE_RELEASE(pPin);

	return hr;
}


//-----------------------------------------------------------------------------
// DShowPlayer::RemoveUnconnectedRenderer
// Description: Remove a renderer filter from the graph if the filter is
//              not connected. 
//-----------------------------------------------------------------------------

HRESULT RemoveUnconnectedRenderer(IGraphBuilder *pGraph, IBaseFilter *pRenderer, BOOL *pbRemoved)
{
	IPin *pPin = NULL;

	BOOL bRemoved = FALSE;

	// Look for a connected input pin on the renderer.

	HRESULT hr = FindConnectedPin(pRenderer, PINDIR_INPUT, &pPin);
	SAFE_RELEASE(pPin);

	// If this function succeeds, the renderer is connected, so we don't remove it.
	// If it fails, it means the renderer is not connected to anything, so
	// we remove it.

	if (FAILED(hr))
	{
		hr = pGraph->RemoveFilter(pRenderer);
		bRemoved = TRUE;
	}

	if (SUCCEEDED(hr))
	{
		*pbRemoved = bRemoved;
	}

	return hr;
}


//-----------------------------------------------------------------------------
// InitializeEVR
// Description: Configures the EVR filter.
//
// pEVR:      Pointer to the EVR.
// info:      Set-up parameters. This structure contains the following items:
//      hwnd:           Handle to the video window.
//      dwStreams:      Number of streams to configure.
//      clsidPresenter: CLSID of a custom presenter, or GUID_NULL.
//      clsidMixer:     CLSID of a custom mixer, or GUID_NULL.
// ppDisplay: Receives a pointer to the IMFVideoDisplayControl interface.
//-----------------------------------------------------------------------------

HRESULT InitializeEVR(IBaseFilter *pEVR, const EVRSetupInfo& info, IMFVideoDisplayControl **ppDisplay)
{
    HRESULT hr = S_OK;
    RECT rcClient;

    IMFVideoRenderer *pRenderer = NULL;
    IMFVideoDisplayControl *pDisplay = NULL;
    IEVRFilterConfig *pConfig = NULL;
    IMFVideoPresenter *pPresenter = NULL;
    IMFTransform *pMixer = NULL;

    // Before doing anything else, set any custom presenter or mixer.

    // Presenter?
    if (info.clsidPresenter != GUID_NULL)
    {
        CHECK_HR(hr = CoCreateInstance(
            info.clsidPresenter, 
            NULL, 
            CLSCTX_INPROC_SERVER,
            __uuidof(IMFVideoPresenter),
            (void**)&pPresenter
            ));
    }

    // Mixer?
    if (info.clsidMixer != GUID_NULL)
    {
        CHECK_HR(hr = CoCreateInstance(
            info.clsidMixer, 
            NULL, 
            CLSCTX_INPROC_SERVER,
            __uuidof(IMFTransform),
            (void**)&pMixer
            ));
    }

    if (pPresenter || pMixer)
    {
        CHECK_HR(hr = pEVR->QueryInterface(__uuidof(IMFVideoRenderer), (void**)&pRenderer));

        CHECK_HR(hr = pRenderer->InitializeRenderer(pMixer, pPresenter));
    }

    // Continue with the rest of the set-up.

    // Set the video window.
    CHECK_HR(hr = MFGetService(pEVR, MR_VIDEO_RENDER_SERVICE, IID_IMFVideoDisplayControl, (void**)&pDisplay));

    // Set the number of streams.
    CHECK_HR(hr = pDisplay->SetVideoWindow(info.hwndVideo));

    if (info.dwStreams > 1)
    {
        CHECK_HR(hr = pEVR->QueryInterface(__uuidof(IEVRFilterConfig), (void**)&pConfig));
        CHECK_HR(hr = pConfig->SetNumberOfStreams(info.dwStreams));
    }

    // Set the display position to the entire window.
    GetClientRect(info.hwndVideo, &rcClient);
    CHECK_HR(hr = pDisplay->SetVideoPosition(NULL, &rcClient));
    
	// Return the IMFVideoDisplayControl pointer to the caller.
	*ppDisplay = pDisplay;
	(*ppDisplay)->AddRef();

done:
    SAFE_RELEASE(pRenderer);
	SAFE_RELEASE(pDisplay);
    SAFE_RELEASE(pConfig);
    SAFE_RELEASE(pPresenter);
    SAFE_RELEASE(pMixer);
	return hr; 
} 


