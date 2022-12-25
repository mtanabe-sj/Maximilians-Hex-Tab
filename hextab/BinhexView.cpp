/*
  Copyright (c) 2022 Makoto Tanabe <mtanabe.sj@outlook.com>
  Licensed under the MIT License.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/
#include "stdafx.h"
#include "libver.h"
#include "BinhexView.h"


/* init - initialzes the view generator instance with a window handle. 
*/
bool BinhexView::init(HWND h)
{
	_hw = h;
	HDC hdc = GetDC(_hw);
	_vi.LogicalPPI.cx = _vi.DevicePPI.cx = GetDeviceCaps(hdc, LOGPIXELSX);
	_vi.LogicalPPI.cy = _vi.DevicePPI.cy = GetDeviceCaps(hdc, LOGPIXELSY);
	bool ret = _init(hdc);
	ReleaseDC(_hw, hdc);
	return ret;
}

/* _init - performs internal initialization by creating fonts, collecting text metrics, and allocating a file read buffer.
*/
bool BinhexView::_init(HDC hdc)
{
	_gcursor = new GripCursorVector;
#ifdef GRIPCURSOR_USES_INSPECTOR
	_gcursor->setInspectionPane(h);
#endif//#ifdef GRIPCURSOR_USES_INSPECTOR

	_vi.FrameMargin = { FRAMEMARGIN_CX, FRAMEMARGIN_CY };

	ASSERT(_vi.LogicalPPI.cx != 0 && _vi.LogicalPPI.cy != 0);

	LOGFONT lf = { 0 };
	// font for the dumped bytes
	lf.lfHeight = -MulDiv(_vi.FontSize, _vi.LogicalPPI.cy, FONT_PTS_PER_INCH);
	lf.lfWeight = FW_NORMAL;
	lf.lfPitchAndFamily = FIXED_PITCH;
	lf.lfItalic = FALSE;
	wcscpy_s(lf.lfFaceName, ARRAYSIZE(lf.lfFaceName), DUMP_FONT_FACE);
	_hfontDump = CreateFontIndirect(&lf);
	_vi.FontHeight = lf.lfHeight;
	HFONT hf0 = (HFONT)SelectObject(hdc, _hfontDump);
	// character dimensions are collected only once
	TEXTMETRIC tm;
	GetTextMetrics(hdc, &tm);
	_vi.ColWidth = tm.tmAveCharWidth;
	_vi.RowHeight = tm.tmHeight;// +tm.tmExternalLeading;
	// font for headers
	//lf.lfWeight = FW_BOLD;
	lf.lfItalic = TRUE;
	_hfontHeader = CreateFontIndirect(&lf);
	// font for annotation
	lf.lfHeight = -MulDiv(_vi.FontSize, _vi.LogicalPPI.cy, FONT_PTS_PER_INCH);
	lf.lfWeight = FW_NORMAL;
	lf.lfPitchAndFamily = DEFAULT_PITCH;
	lf.lfItalic = FALSE;
	wcscpy_s(lf.lfFaceName, ARRAYSIZE(lf.lfFaceName), ANNOTATION_FONT_FACE);
	_hfontAnnotation = CreateFontIndirect(&lf);
	_vi.AnnotationFontHeight = lf.lfHeight;
	SelectObject(hdc, _hfontAnnotation);
	// character dimensions are collected only once
	GetTextMetrics(hdc, &tm);
	_vi.AnnotationColWidth = tm.tmAveCharWidth;
	_vi.AnnotationRowHeight = tm.tmHeight; // +tm.tmExternalLeading;
	SelectObject(hdc, hf0);

	_fri.BufferSize = READFILE_BUFSIZE;
	_fri.DataBuffer = (LPBYTE)malloc(READFILE_BUFSIZE);
	if (_fri.DataBuffer)
		return true;
	return false;
}

/* term - prepares the view generator instance for termination by freeing objects so far allocated.
*/
void BinhexView::term()
{
	if (_hrgnClip)
	{
		DeleteObject(_hrgnClip);
		_hrgnClip = NULL;
	}
	if (_highdefVP)
	{
		delete _highdefVP;
		_highdefVP = NULL;
	}
#ifdef BINHEXVIEW_SAVES_PAINTED_PAGE
	if (_hbmPaint)
	{
		DeleteObject(_hbmPaint);
		_hbmPaint = NULL;
	}
#endif//BINHEXVIEW_SAVES_PAINTED_PAGE
	if (_gcursor)
	{
		delete _gcursor;
		_gcursor = NULL;
	}
	if (_hfontAnnotation)
	{
		DeleteObject(_hfontAnnotation);
		_hfontAnnotation = NULL;
	}
	if (_hfontDump)
	{
		DeleteObject(_hfontDump);
		_hfontDump = NULL;
	}
	if (_hfontHeader)
	{
		DeleteObject(_hfontHeader);
		_hfontHeader = NULL;
	}
	if (_fri.DataBuffer)
	{
		free(_fri.DataBuffer);
		_fri.DataBuffer = NULL;
	}
	if (_hw)
		_hw = NULL;
}

/* initViewInfo - initializes controlling parameters of the hexdump view generator.
*/
RECT BinhexView::initViewInfo()
{
	SIZE frameSize = _vi.FrameSize;

	_vi.RowsTotal = (UINT)_fri.FileSize.QuadPart / _vi.BytesPerRow;
	_vi.ColsTotal = (COL_LEN_PER_DUMP_BYTE+1) * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN;

	_vi.OffsetColWidth = _vi.ColWidth*(OFFSET_COL_LEN + LEFT_SPACING_COL_LEN);
	_vi.ColsPerPage = frameSize.cx / _vi.ColWidth - (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN);
	_vi.RowsPerPage = frameSize.cy / _vi.RowHeight;
	_vi.RowsShown = 0;
	return { 0,0,_vi.FrameSize.cx,_vi.FrameSize.cy };
}

/* repaintByMemoryDC - avoids screen jitters (affecting a page overcrowded with regions and annotations) by first rendering the hexdump on a memory dc and bitblt'ing it to a destination dc.

parameters:
flags - [in] set to 0 or a combo of BINHEXVIEW_REPAINT_* bitwise flags.
hdc - [in] handle to a destination device context.
*/
HRESULT BinhexView::repaintByMemoryDC(UINT flags, HDC hdc)
{
	HDC hdc2 = CreateCompatibleDC(hdc);
	SIZE sz = { _vi.FrameSize.cx, _vi.FrameSize.cy };
	HBITMAP hbmp2 = CreateCompatibleBitmap(hdc, sz.cx, sz.cy);
	HBITMAP hbmp0 = (HBITMAP)SelectObject(hdc2, hbmp2);
	HRESULT hr = repaint(flags | BINHEXVIEW_REPAINT_RESET_FONT, hdc2);
	if (hr == S_OK)
	{
		if (!BitBlt(hdc, 0, 0, sz.cx, sz.cy, hdc2, 0, 0, SRCCOPY))
		{
			DWORD errorCode = ::GetLastError();
			DBGPRINTF((L"repaintByMemoryDC: BitBlt failed (error %d)\n", errorCode));
			hr = HRESULT_FROM_WIN32(errorCode);
		}
	}
	SelectObject(hdc2, hbmp0);
	DeleteObject(hbmp2);
	DeleteDC(hdc2);
	return hr;
}

/* repaint2 - uses repaintByMemoryDC to repaint the hexdump jitter-free on a device context (_hdc) belonging to a pre-assigned window handle (_hw).
*/
HRESULT BinhexView::repaint2(UINT flags)
{
	setVC(); // gets a device context for the window of _hw.
	HRESULT hr = repaintByMemoryDC(flags, _hdc); // do a jitter-free repaint.
	releaseVC(); // release the dc.
	return hr;
}

/* repaint - a top-level method for generating a hexdump view.

parameters:
flags - [in] set to 0 or a combo of BINHEXVIEW_REPAINT_* bitwise flags.
hdc - [in,optional] NULL or a device context handle. if it's NULL, the class member _hdc is adopted as the device context to output to.
*/
HRESULT BinhexView::repaint(UINT flags, HDC hdc)
{
	HRESULT hr;

	if (hdc == NULL)
		hdc = _hdc;

	RECT frameRect = initViewInfo();

	HFONT hf0 = NULL;
	if (flags & BINHEXVIEW_REPAINT_RESET_FONT)
	{
		hf0 = (HFONT)SelectObject(hdc, _hfontDump);
		FillRect(hdc, &frameRect, GetSysColorBrush(COLOR_WINDOW));
		SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
	}

	hr = _repaintInternal(hdc, frameRect, flags);

	if (!(flags & BINHEXVIEW_REPAINT_NO_FRAME))
	{
		if (_vi.CurCol > 0 && _vi.BytesPerRow > DEFAULT_HEX_BYTES_PER_ROW)
		{
			MoveToEx(hdc, _vi.OffsetColWidth, 0, NULL);
			LineTo(hdc, _vi.OffsetColWidth, _vi.FrameSize.cy);
		}
		FrameRect(hdc, &frameRect, (HBRUSH)GetStockObject(GRAY_BRUSH));
	}

	if (hf0)
		SelectObject(hdc, hf0);
	return hr;
}

/* _repaintInternal - a callback for repaint to generate a hexdump view. optionally, draws a row selector triangle.

parameters:
hdc - [in] device context handle.
frameRect - [in] view frame rectangle.
flags - [in] repaint flags

subclass BinhexMetaView defines an override.
*/
HRESULT BinhexView::_repaintInternal(HDC hdc, RECT frameRect, UINT flags)
{
	if (!_fri.HasData)
		return E_UNEXPECTED;

	if (!(flags & BINHEXVIEW_REPAINT_NO_DUMP))
	{
		// draw the rows of hex dump.
		drawHexDump(hdc);
	}

	if (!(flags & BINHEXVIEW_REPAINT_NO_SELECTOR))
	{
		// paint a triangular-shaped row selector.
		if ((_fri.DataOffset.QuadPart <= _vi.SelectionOffset.QuadPart)
			&& (_vi.SelectionOffset.QuadPart < _fri.DataOffset.QuadPart + (LONGLONG)_fri.DataLength))
		{
			drawRowSelector(hdc, _vi.SelectionOffset, _rowselectorBorderColor, _rowselectorInteriorColor);
			return S_OK;
		}
	}
	return S_FALSE;
}

/* drawHexDump - formats and outputs data bytes as a row of hex digits and a row of ASCII text placed side by side, and repeats until the viewport is filled with the dump rows.
*/
void BinhexView::drawHexDump(HDC hdc)
{
	ULONG i, j, ix, iy;
	UINT cx, cy, cxLast, cyLast, mx;
	char hexbuf[10];
	/* LINE BUFFER: needs to be large enough to hold these bytes, 8+2+3*BytesPerRow+2+BytesPerRow+2
		if BytesPerRow is set to 8, this is the data layout in the buffer.
		[_OFFSET_]__[11]_[22]_[33]_[44]-[55]_[66]_[77]_[88]__[12345678][Z]
	*/
	ULONG bufSize = OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN + _vi.BytesPerRow + 2;
	LPSTR linebuf = (LPSTR)malloc(bufSize);

	cy = _fri.DataLength / _vi.BytesPerRow;
	cyLast = cy;
	cxLast = _fri.DataLength % _vi.BytesPerRow;
	if (cxLast)
		cy++;
	_vi.RowsTotal = (UINT)_fri.FileSize.QuadPart / _vi.BytesPerRow;
	if (_vi.RowsPerPage && _vi.RowsPerPage < cyLast) {
		cy = _vi.RowsPerPage;
		cxLast = _vi.BytesPerRow;
	}

	// i=0 corresponds to the first data byte in _fri.DataBuffer. that byte is at _fri.DataOffset on the source file.
	i = 0;
	for (iy = 0; iy < cy; iy++) {
		ASSERT(i < _fri.DataLength);
		cx = _vi.BytesPerRow;
		mx = cx/2-1;
		sprintf_s(linebuf, bufSize, OFFSET_COL_FORMAT, i + _fri.DataOffset.LowPart);
		if (iy == cyLast)
			cx = cxLast;
		j = i;
		for (ix = 0; ix < cx; ix++) {
			sprintf_s(hexbuf, sizeof(hexbuf), "%02X ", _fri.DataBuffer[i++]);
			if (ix == mx)
				hexbuf[2] = '-';
			strcat_s(linebuf, bufSize, hexbuf);
		}
		for (; ix < _vi.BytesPerRow; ix++) {
			strcat_s(linebuf, bufSize, "   ");
		}
		strcat_s(linebuf, bufSize, " ");
		i = j;
#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
		_finishDumpLine(hdc, linebuf, bufSize, i, cx, iy);
#else//#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
		for (ix = 0; ix < cx; ix++) {
			char ci[] = ".";
			if (' ' <= _fri.DataBuffer[i] && _fri.DataBuffer[i] <= 0x7F)
				*ci = (char)_fri.DataBuffer[i];
			//else if (_fri.DataBuffer[i] != 0x0A && _fri.DataBuffer[i] != 0x0D && _fri.DataBuffer[i] != 0x08 && _fri.DataBuffer[i] != 0x09 && _fri.DataBuffer[i] != 0x00)
			//	*ci = _fri.DataBuffer[i];
			strcat_s(linebuf, bufSize, ci);
			i++;
		}
		_vi.RowsShown++;

		RECT rcDraw;
		rcDraw.left = DRAWTEXT_MARGIN_CX;
		rcDraw.top = DRAWTEXT_MARGIN_CY + iy * _vi.RowHeight;
		rcDraw.right = _vi.FrameSize.cx - _vi.VScrollerSize.cx;
		rcDraw.bottom = rcDraw.top + _vi.RowHeight;
		if (rcDraw.bottom > _vi.FrameSize.cy)
			rcDraw.bottom = _vi.FrameSize.cy;

		int cc = (int)strlen(linebuf);
		if (_vi.CurCol > 0 && _vi.BytesPerRow > DEFAULT_HEX_BYTES_PER_ROW)
		{
			DrawTextA(hdc, linebuf, OFFSET_COL_LEN, &rcDraw, 0);
			if (((OFFSET_COL_LEN + LEFT_SPACING_COL_LEN) + _vi.CurCol) < (UINT)cc)
			{
				rcDraw.left += _vi.OffsetColWidth;
				DrawTextA(hdc,
					linebuf + (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN) + _vi.CurCol,
					cc - (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN) - _vi.CurCol,
					&rcDraw,
					0);
			}
		}
		else
		{
			DrawTextA(hdc, linebuf, cc, &rcDraw, 0);
		}
#endif//#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
	}
	free(linebuf);
}

#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
/* measureHexDumpRegion - calculates the corner coordinates of a region that surrounds a block of dump bytes as seen in the hex dump area.

return value:
number of elements in pt set to corner coordinates of the region. the maximum is 8.

parameters:
hdc - [in] device context handle
pt - [in,out] positions in client coordinates of the corners of a text region.
cx - [in] horizontal offset in client coordinate of the front of the dump bytes in the hex dump area.
cy - [in] vertical offset of the front byte.
hitRow1 - [in] 0-based row number of the region's front measured from the top of the page in view.
hitRow2 - [in] row number of the region's back.
hitCol1 - [in] column number of the region's front.
hitCol2 - [in] column number of the region's back.
regionInView - [in] indicates the region's visibility. bits 1, 2, and/or 4 can be turned on.
  1=region's front is visible.
  2=region's back is visible.
  4=region's middle is visible but the front and back are outside current view.
*/
ULONG BinhexView::measureHexDumpRegion(POINT pt[8], int cx, int cy, LONG hitRow1, LONG hitRow2, LONG hitCol1, LONG hitCol2, int regionInView)
{
	// matched data is in the current view.
	ULONG np = 0;
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
	int cx1 = _getHexColCoord(0);
	int cx2 = _getHexColCoord(_vi.BytesPerRow, -1);
	int hx1 = _getHexColCoord(hitCol1);
	int hx2 = _getHexColCoord(hitCol2, -1);
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
	// draw a perimeter line around the found data in the hex values column.
	if (hitRow1 == hitRow2)
	{
		// a special case with a 4-cornered rectangular region.
		// each group of 2 hex digits is followed by one white space,
		// giving a multiplication factor of COL_LEN_PER_DUMP_BYTE (3).
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = hx1;
		pt[np++].y = cy + hitRow1 * _vi.RowHeight;
		/*1*/pt[np].x = hx2;
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = cx + COL_LEN_PER_DUMP_BYTE * hitCol1*_vi.ColWidth;
		pt[np++].y = cy + hitRow1 * _vi.RowHeight;
		/*1*/pt[np].x = cx + COL_LEN_PER_DUMP_BYTE * hitCol2*_vi.ColWidth - _vi.ColWidth;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		pt[np++].y = pt[0].y;
		/*2*/pt[np].x = pt[1].x;
		pt[np++].y = pt[1].y + _vi.RowHeight;
		/*3*/pt[np].x = pt[0].x;
		pt[np++].y = pt[2].y;
	}
	else if (hitCol2 == 0)
	{
		// a special case - the region is 6 cornered.
		// starting corner given by {hitCol1, hitRow1}.
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = hx1;
		pt[np++].y = cy + hitRow1 * _vi.RowHeight;
		// right-top corner
		/*1*/pt[np].x = cx2;
		pt[np++].y = pt[0].y;
		// right-bottom corner
		/*2*/pt[np].x = pt[1].x;
		pt[np++].y = cy + hitRow2 * _vi.RowHeight;
		// left-bottom corner
		/*3*/pt[np].x = cx;
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = cx + COL_LEN_PER_DUMP_BYTE * hitCol1*_vi.ColWidth;
		pt[np++].y = cy + hitRow1 * _vi.RowHeight;
		// right-top corner
		/*1*/pt[np].x = cx + COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow*_vi.ColWidth - _vi.ColWidth;
		pt[np++].y = pt[0].y;
		// right-bottom corner
		/*2*/pt[np].x = pt[1].x;
		pt[np++].y = cy + hitRow2 * _vi.RowHeight;
		// left-bottom corner
		/*3*/pt[np].x = cx;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		pt[np++].y = pt[2].y;
		// left-top(-1) corner
		/*4*/pt[np].x = pt[3].x;
		pt[np++].y = pt[0].y + _vi.RowHeight;
		// move toward starting corner and stay on a row just below it
		/*7*/pt[np].x = pt[0].x;
		pt[np++].y = pt[4].y;
	}
	else if (regionInView & 4)
	{
		// top and bottom borderlines are not visible. show the right and left borderlines only.
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = cx1;
		pt[np++].y = -FRAMEMARGIN_CY;
		/*1*/pt[np].x = cx2;
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = cx;
		pt[np++].y = -FRAMEMARGIN_CY;
		/*1*/pt[np].x = cx + COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow*_vi.ColWidth - _vi.ColWidth;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		pt[np++].y = pt[0].y;
		/*2*/pt[np].x = pt[1].x;
		pt[np++].y = _vi.FrameSize.cy + FRAMEMARGIN_CY;
		/*3*/pt[np].x = pt[0].x;
		pt[np++].y = pt[2].y;
	}
	else
	{
		// general case with a 8-cornered region
		// the ending byte of the region sits between beginning and ending columns.
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = hx1;
		pt[np++].y = cy + hitRow1 * _vi.RowHeight;
		/*1*/pt[np].x = cx2;
		pt[np++].y = pt[0].y;
		/*2*/pt[np].x = pt[1].x;
		pt[np++].y = cy + hitRow2 * _vi.RowHeight;
		/*3*/pt[np].x = hx2;
		pt[np++].y = pt[2].y;
		/*4*/pt[np].x = pt[3].x;
		pt[np++].y = pt[3].y + _vi.RowHeight;
		/*5*/pt[np].x = cx1;
		pt[np++].y = pt[4].y;
		/*6*/pt[np].x = cx1;
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = cx + COL_LEN_PER_DUMP_BYTE * hitCol1*_vi.ColWidth;
		pt[np++].y = cy + hitRow1 * _vi.RowHeight;
		/*1*/pt[np].x = cx + COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow*_vi.ColWidth - _vi.ColWidth;
		pt[np++].y = pt[0].y;
		/*2*/pt[np].x = pt[1].x;
		pt[np++].y = cy + hitRow2 * _vi.RowHeight;
		/*3*/pt[np].x = cx + COL_LEN_PER_DUMP_BYTE * hitCol2*_vi.ColWidth - _vi.ColWidth;
		pt[np++].y = pt[2].y;
		/*4*/pt[np].x = pt[3].x;
		pt[np++].y = pt[3].y + _vi.RowHeight;
		/*5*/pt[np].x = cx;
		pt[np++].y = pt[4].y;
		/*6*/pt[np].x = cx;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		pt[np++].y = pt[0].y + _vi.RowHeight;
		/*7*/pt[np].x = pt[0].x;
		pt[np++].y = pt[6].y;
	}
	return np;
}

/* measureTextDumpRegion - calculates the coordinates of a block of dump bytes (region) as seen in the 'ASCII' text dump area.

return value:
number of elements in pt set to corner coordinates of the region.

parameters:
hdc - [in] device context handle
pt - [out] positions in client coordinates of the corners of a text region.
cx - [in] horizontal offset in client coordinate of the front of the dump bytes in the ASCII text area.
cy - [in] vertical offset of the front byte.
hitRow1 - [in] 0-based row number of the region's front measured from the top of the page in view.
hitRow2 - [in] row number of the region's back.
hitCol1 - [in] column number of the region's front.
hitCol2 - [in] column number of the region's back.
regionInView - [in] indicates the region's visibility. bits 1, 2, and/or 4 can be set to 1. 1=region's front is visible. 2=region's back is visible. 4=region's middle is visible but the front and back are outside current view.
regionStart - [in] byte offset from the top of the data block buffered in the ROFile buffer, as given by (_ri->DataOffset - _vc->_fri.DataOffset).
regionLength - [in] byte length of the region.
*/
ULONG BinhexView::measureTextDumpRegion(HDC hdc, POINT pt[8], int cx, int cy, LONG hitRow1, LONG hitRow2, LONG hitCol1, LONG hitCol2, int regionInView, LARGE_INTEGER regionStart, ULONG regionLength)
{
	// draw a perimeter line around the found data in the ASCII text column.
	ULONG np = 0;
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
	int cx0 = _getAscColCoord(0);
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
	int cx2 = (COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN)*_vi.ColWidth;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
	if (hitRow1 == hitRow2)
	{
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = cx0 + hitCol1 * _vi.ColWidth;
		pt[np++].y = cy + hitRow1 * _vi.RowHeight;
		/*1*/pt[np].x = cx0 + hitCol2 * _vi.ColWidth;
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = cx + cx2 + hitCol1 * _vi.ColWidth;
		pt[np++].y = cy + hitRow1 * _vi.RowHeight;
		/*1*/pt[np].x = cx + cx2 + hitCol2 * _vi.ColWidth;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		pt[np++].y = pt[0].y;
		/*2*/pt[np].x = pt[1].x;
		pt[np++].y = pt[1].y + _vi.RowHeight;
		/*3*/pt[np].x = pt[0].x;
		pt[np++].y = pt[2].y;
	}
	else if (hitCol2 == 0)
	{
		// a special case - the region is 6 cornered
		// starting corner given by {hitCol1, hitRow1}.
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = cx0 + hitCol1 * _vi.ColWidth;
		pt[np++].y = cy + hitRow1 * _vi.RowHeight;
		// right-top corner
		/*1*/pt[np].x = cx0 + _vi.BytesPerRow*_vi.ColWidth;
		pt[np++].y = pt[0].y;
		// right-bottom corner
		/*2*/pt[np].x = pt[1].x;
		pt[np++].y = cy + hitRow2 * _vi.RowHeight;
		// left-bottom corner
		/*3*/pt[np].x = cx0;
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = cx + cx2 + hitCol1 * _vi.ColWidth;
		pt[np++].y = cy + hitRow1 * _vi.RowHeight;
		// right-top corner
		/*1*/pt[np].x = cx + cx2 + _vi.BytesPerRow*_vi.ColWidth;
		pt[np++].y = pt[0].y;
		// right-bottom corner
		/*2*/pt[np].x = pt[1].x;
		pt[np++].y = cy + hitRow2 * _vi.RowHeight;
		// left-bottom corner
		/*3*/pt[np].x = cx + cx2;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		pt[np++].y = pt[2].y;
		// left-top(-1) corner
		/*4*/pt[np].x = pt[3].x;
		pt[np++].y = pt[0].y + _vi.RowHeight;
		// move toward starting corner and stay on a row just below it
		/*7*/pt[np].x = pt[0].x;
		pt[np++].y = pt[4].y;
	}
	else if (regionInView & 4)
	{
		// top and bottom borderlines are not visible. show the right and left borderlines only.
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = cx0;
		pt[np++].y = -FRAMEMARGIN_CY;
		/*1*/pt[np].x = cx0 + _vi.BytesPerRow * _vi.ColWidth;
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = cx + cx2;
		pt[np++].y = -FRAMEMARGIN_CY;
		/*1*/pt[np].x = cx + cx2 + _vi.BytesPerRow * _vi.ColWidth;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		pt[np++].y = pt[0].y;
		/*2*/pt[np].x = pt[1].x;
		pt[np++].y = _vi.FrameSize.cy + FRAMEMARGIN_CY;
		/*3*/pt[np].x = pt[0].x;
		pt[np++].y = pt[2].y;
	}
	else
	{
		// general case with a 8-cornered region
		// the ending byte of the region sits between beginning and ending columns.
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = cx0 + hitCol1 * _vi.ColWidth;
		pt[np++].y = cy + hitRow1 * _vi.RowHeight;
		/*1*/pt[np].x = cx0 + _vi.BytesPerRow*_vi.ColWidth;
		pt[np++].y = pt[0].y;
		/*2*/pt[np].x = pt[1].x;
		pt[np++].y = cy + hitRow2 * _vi.RowHeight;
		/*3*/pt[np].x = cx0 + hitCol2 * _vi.ColWidth;
		pt[np++].y = pt[2].y;
		/*4*/pt[np].x = pt[3].x;
		pt[np++].y = pt[3].y + _vi.RowHeight;
		/*5*/pt[np].x = cx0;
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		/*0*/pt[np].x = cx + cx2 + hitCol1 * _vi.ColWidth;
		pt[np++].y = cy + hitRow1 * _vi.RowHeight;
		/*1*/pt[np].x = cx + cx2 + _vi.BytesPerRow*_vi.ColWidth;
		pt[np++].y = pt[0].y;
		/*2*/pt[np].x = pt[1].x;
		pt[np++].y = cy + hitRow2 * _vi.RowHeight;
		/*3*/pt[np].x = cx + cx2 + hitCol2 * _vi.ColWidth;
		pt[np++].y = pt[2].y;
		/*4*/pt[np].x = pt[3].x;
		pt[np++].y = pt[3].y + _vi.RowHeight;
		/*5*/pt[np].x = cx + cx2;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		pt[np++].y = pt[4].y;
		/*6*/pt[np].x = pt[5].x;
		pt[np++].y = pt[0].y + _vi.RowHeight;
		/*7*/pt[np].x = pt[0].x;
		pt[np++].y = pt[6].y;
	}
	return np;
}

/* _finishDumpLinePartial - draws a text segment in a line buffer in the ASCII dump area.
the method is called by drawRowBytes to draw the text bytes in the ASCII dump area after drawRowBytes finishes drawing the bytes in the hex dump area.

parameters:
hdc - [in] device context handle.
linebuf - [in] contains a text segment to dump.
bufSize - [in] size in bytes of linebuf.
asciiLen - [in] byte length of the text string in the line buffer.
fiDataOffset - [in,out] offset to the front of a text segment accessible with the file read buffer of _fri.
cx - [in] number of bytes the text segment consists of.
ix - [in] column index of the front of the text segment.
iy - [in] row index of the text segment.

note that the method currently does not use fiDataOffset but an override in a subclass does. see BinhexMetaView::_finishDumpLinePartial.
*/
void BinhexView::_finishDumpLinePartial(HDC hdc, LPSTR linebuf, ULONG bufSize, ULONG asciiLen, ULONG &fiDataOffset, UINT cx, UINT ix, UINT iy)
{
	LPCSTR src = linebuf + COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN + ix;

	RECT rcDraw;
	rcDraw.top = DRAWTEXT_MARGIN_CY + iy * _vi.RowHeight;
	rcDraw.bottom = rcDraw.top + _vi.RowHeight;
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
	rcDraw.left = DRAWTEXT_MARGIN_CX + _getAscColCoord(ix);
	rcDraw.right = DRAWTEXT_MARGIN_CX + _getAscColCoord(cx);
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
	rcDraw.left = DRAWTEXT_MARGIN_CX + (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN + ix) * _vi.ColWidth;
	rcDraw.right = DRAWTEXT_MARGIN_CX + (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN + cx) * _vi.ColWidth;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
	if (rcDraw.right > _vi.FrameSize.cx + DRAWTEXT_MARGIN_CX)
		rcDraw.right = _vi.FrameSize.cx + DRAWTEXT_MARGIN_CX;
	if (_vi.CurCol)
		rcDraw.left -= _vi.ColWidth*_vi.CurCol;
	if (rcDraw.left < _vi.FrameSize.cx)
	{
		DrawTextA(hdc, src, asciiLen, &rcDraw, 0);
	}
}

/* _finishDumpLine - finalizes a line buffer already containing a string of hex digits by formatting a string of dump bytes as ASCII characters meant for display in the ASCII text dump area and adding it to the line buffer. outputs the completed line consisting of a row of hex dump digits and a row of ASCII characters side by side.

parameters:
hdc - [in] device context handle.
linebuf - [in] a line buffer containing source data bytes to be dumped.
bufSize - [in] line buffer size in bytes.
fiDataOffset - [in,out] offset to the front of the source bytes.
cx - [in] number of columns to fill in the ASCII dump area.
iy - [in] row number of the output row.
*/
void BinhexView::_finishDumpLine(HDC hdc, LPSTR linebuf, ULONG bufSize, ULONG &fiDataOffset, UINT cx, UINT iy)
{
	for (UINT ix = 0; ix < cx; ix++) {
		char ci[] = ".";
		if (' ' <= _fri.DataBuffer[fiDataOffset] && _fri.DataBuffer[fiDataOffset] <= 0x7F)
			*ci = (char)_fri.DataBuffer[fiDataOffset];
		//else if (_fri.DataBuffer[fiDataOffset] != 0x0A && _fri.DataBuffer[fiDataOffset] != 0x0D && _fri.DataBuffer[fiDataOffset] != 0x08 && _fri.DataBuffer[fiDataOffset] != 0x09 && _fri.DataBuffer[fiDataOffset] != 0x00)
		//	*ci = _fri.DataBuffer[fiDataOffset];
		strcat_s(linebuf, bufSize, ci);
		fiDataOffset++;
	}
	_vi.RowsShown++;

	RECT rcDraw;
	rcDraw.left = DRAWTEXT_MARGIN_CX;
	rcDraw.top = DRAWTEXT_MARGIN_CY + iy * _vi.RowHeight;
	rcDraw.right = _vi.FrameSize.cx;
	rcDraw.bottom = rcDraw.top + _vi.RowHeight;
	if (rcDraw.bottom > _vi.FrameSize.cy)
		rcDraw.bottom = _vi.FrameSize.cy;

	int cc = (int)strlen(linebuf);
	if (_vi.CurCol > 0 && _vi.BytesPerRow > DEFAULT_HEX_BYTES_PER_ROW)
	{
		DrawTextA(hdc, linebuf, OFFSET_COL_LEN, &rcDraw, 0);
		if (((OFFSET_COL_LEN + LEFT_SPACING_COL_LEN) + _vi.CurCol) < (UINT)cc)
		{
			rcDraw.left += _vi.OffsetColWidth;
			DrawTextA(hdc,
				linebuf + (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN) + _vi.CurCol,
				cc - (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN) - _vi.CurCol,
				&rcDraw,
				0);
		}
	}
	else
	{
		DrawTextA(hdc, linebuf, cc, &rcDraw, 0);
		//DBGPRINTF((L"_finishDumpLine: cc=%d; FontSize=%d, ColWidth=%d; rcDraw={%d,%d,%d,%d}; FrameSize={%d,%d}\n", cc, _vi.FontSize, _vi.ColWidth, rcDraw.left, rcDraw.top, rcDraw.right, rcDraw.bottom, _vi.FrameSize.cx, _vi.FrameSize.cy));
	}
}
#endif//#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE

/* drawRowBytes - formats and outputs a short range (one or two rows) of data bytes as rows of hex digits and rows of ASCII text placed side by side.

CRegionOperation::draw uses the method to render a range of dump bytes as a meta region with the outline and interior drawn in color.
*/
ULONG BinhexView::drawRowBytes(HDC hdc, LARGE_INTEGER offsetToData, ULONG dataLength)
{
	RECT rcDraw;
	ULONG i, ix;
	UINT cx, cy, cxLast, cyLast;
	char hexbuf[10];
	/* LINE BUFFER: needs to be large enough to hold these bytes, 3*BytesPerRow+2+BytesPerRow+2
		if BytesPerRow is set to 8, this is the data layout in the buffer.
		[11]_[22]_[33]_[44]_[55]_[66]_[77]_[88]__[12345678][Z]
	*/
	ULONG bufSize = COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN + _vi.BytesPerRow + 2;
	LPSTR linebuf = (LPSTR)malloc(bufSize);
	ZeroMemory(linebuf, bufSize);

	cy = _fri.DataLength / _vi.BytesPerRow;
	cyLast = cy;
	cxLast = _fri.DataLength % _vi.BytesPerRow;
	if (cxLast)
		cy++;
	_vi.RowsTotal = (UINT)_fri.FileSize.QuadPart / _vi.BytesPerRow;
	if (_vi.RowsPerPage && _vi.RowsPerPage < cyLast) {
		cy = _vi.RowsPerPage;
		cxLast = _vi.BytesPerRow;
	}

	ULONG remainingLen = 0;
	ULONG ki, kx, ky;
	ki = offsetToData.LowPart;
	ky = ki / _vi.BytesPerRow;
	kx = ki % _vi.BytesPerRow;
	cx = _vi.BytesPerRow;
	ULONG cols = (ky == cyLast) ? cxLast - kx : _vi.BytesPerRow - kx;
	if (cols == 0)
		return 0;
	if (dataLength > cols)
		remainingLen = dataLength - cols;
	if (dataLength < cols)
	{
		cx = kx + dataLength;
		cols = dataLength;
	}

	i = ki;

	for (ix = 0; ix < kx; ix++)
		strcat_s(linebuf, bufSize, "   ");
	for (; ix < cx; ix++)
	{
		sprintf_s(hexbuf, sizeof(hexbuf), "%02X ", _fri.DataBuffer[i++]);
		strcat_s(linebuf, bufSize, hexbuf);
	}
	for (; ix < _vi.BytesPerRow; ix++)
		strcat_s(linebuf, bufSize, "   ");

	strcat_s(linebuf, bufSize, " ");

	i = ki;
	for (ix = 0; ix < kx; ix++)
		strcat_s(linebuf, bufSize, " ");
	for (; ix < cx; ix++)
	{
		char ci[] = ".";
		if (' ' <= _fri.DataBuffer[i] && _fri.DataBuffer[i] <= 0x7F)
			*ci = (char)_fri.DataBuffer[i];
		//else if (_fri.DataBuffer[i] != 0x0A && _fri.DataBuffer[i] != 0x0D && _fri.DataBuffer[i] != 0x08 && _fri.DataBuffer[i] != 0x09 && _fri.DataBuffer[i] != 0x00)
		//	*ci = _fri.DataBuffer[i];
		strcat_s(linebuf, bufSize, ci);
		i++;
	}
	/*
	_vi.RowsShown++;
	*/

	// draw the hex digit row
	LPCSTR src = linebuf + COL_LEN_PER_DUMP_BYTE * kx;
	int cc = COL_LEN_PER_DUMP_BYTE * cols;

#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
	rcDraw.left = DRAWTEXT_MARGIN_CX + _getHexColCoord(kx);
	rcDraw.right = DRAWTEXT_MARGIN_CX + _getHexColCoord(cx, -1);
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
	rcDraw.left = DRAWTEXT_MARGIN_CX + (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * kx) * _vi.ColWidth;
	rcDraw.right = DRAWTEXT_MARGIN_CX + (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * cx - 1) * _vi.ColWidth;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
	rcDraw.top = DRAWTEXT_MARGIN_CY + ky * _vi.RowHeight;
	rcDraw.bottom = rcDraw.top + _vi.RowHeight;
	if (rcDraw.bottom > _vi.FrameSize.cy)
		rcDraw.bottom = _vi.FrameSize.cy;
	if (rcDraw.right > _vi.FrameSize.cx + DRAWTEXT_MARGIN_CX)
		rcDraw.right = _vi.FrameSize.cx + DRAWTEXT_MARGIN_CX;

	if (_vi.CurCol > 0 && _vi.BytesPerRow > DEFAULT_HEX_BYTES_PER_ROW)
	{
		if (_vi.CurCol < (UINT)(COL_LEN_PER_DUMP_BYTE * kx + cc))
		{
			rcDraw.left -= _vi.ColWidth*_vi.CurCol;
			if (rcDraw.left < (int)_vi.OffsetColWidth)
			{
				src += _vi.CurCol - COL_LEN_PER_DUMP_BYTE * kx;
				cc -= _vi.CurCol - COL_LEN_PER_DUMP_BYTE * kx;
				rcDraw.left = (int)_vi.OffsetColWidth + 2;
			}
			if (src[cc - 1] == ' ')
				cc--;
			DrawTextA(hdc, src, cc, &rcDraw, 0);
		}
	}
	else
	{
		DrawTextA(hdc, src, cc, &rcDraw, 0);
	}

	// draw the ascii row
#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
	_finishDumpLinePartial(hdc, linebuf, bufSize, cols, ki, cx, kx, ky);
#else//#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
	src = linebuf + 3 * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN + kx;
	cc = cols;

	rcDraw.left = DRAWTEXT_MARGIN_CX + (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + 3 * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN + kx) * _vi.ColWidth;
	rcDraw.right = DRAWTEXT_MARGIN_CX + (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + 3 * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN + cx) * _vi.ColWidth;
	if (rcDraw.right > _vi.FrameSize.cx - DRAWTEXT_MARGIN_CX)
		rcDraw.right = _vi.FrameSize.cx - DRAWTEXT_MARGIN_CX;
	rcDraw.left -= _vi.ColWidth*_vi.CurCol;
	if (rcDraw.left < _vi.FrameSize.cx)
	{
		DrawTextA(hdc, src, cc, &rcDraw, 0);
	}
#endif//#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
	free(linebuf);
	return remainingLen;
}

#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
/* _getHexColCoord - translates a column position in the hex dump area to an x-coordinate in logical units.

return value:
x-coordinate in logical units of the hexdump column.

parameters:
colPos - [in] a column position to convert.
deltaNumerator - [in,optional] numerator part of a fraction of a column space to add to the colPos column position, if deltaDenominator is non-zero; or a number of column spaces to add to colPos.
deltaDenominator - [in,optional] denominator part of the fraction.
*/
int BinhexView::_getHexColCoord(int colPos, int deltaNumerator, int deltaDenominator)
{
	if (deltaDenominator)
	{
		return (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * colPos)*_vi.ColWidth + MulDiv(_vi.ColWidth, deltaNumerator, deltaDenominator);
	}
	return (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * colPos + deltaNumerator)*_vi.ColWidth;
}

/* _getHexColCoord - translates a column position in the ASCII dump area to an x-coordinate in logical units.

return value:
x-coordinate in logical units of the ASCII dump column.

parameters:
colPos - [in] a column position to convert.
*/
int BinhexView::_getAscColCoord(int colPos)
{
	return (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN + colPos)*_vi.ColWidth;
}

/* _getLogicalXCoord - translates an x-coordinate in screen units to a coordinate in logical units.
*/
int BinhexView::_getLogicalXCoord(int screenX)
{
	if (!_highdefVP)
		return screenX;
	return MulDiv(screenX, _vi.LogicalPPI.cx, _highdefVP->ScreenPPI.cx);
}

/* _getLogicalYCoord - translates an y-coordinate in screen units to a coordinate in logical units.
*/
int BinhexView::_getLogicalYCoord(int screenY)
{
	if (!_highdefVP)
		return screenY;
	return MulDiv(screenY, _vi.LogicalPPI.cy, _highdefVP->ScreenPPI.cy);
}
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT

// helper methods of coordinate translation

int BinhexView::screenToLogical(int x)
{
	if (_highdefVP)
		x = lrint(screenToLogical((double)x));
	return x;
}

double BinhexView::screenToLogical(double x)
{
	if (_highdefVP)
	{
		// in printing, convert wavelen to a logical dpi.
		x = x * (double)_highdefVP->LogicalDPI.cx / (double)_highdefVP->ScreenPPI.cx;
		// apply the scaling factor in fit to page width mode.
		if (_highdefVP->ScalingFactor != 0)
			x /= _highdefVP->ScalingFactor;
	}
	return x;
}

SIZE BinhexView::toDeviceCoords(const SIZE src)
{
	SIZE dest;
	dest.cx = MulDiv(src.cx, _vi.DevicePPI.cx, _vi.LogicalPPI.cx);
	dest.cy = MulDiv(src.cy, _vi.DevicePPI.cy, _vi.LogicalPPI.cy);
	if (_highdefVP && _highdefVP->ScalingFactor != 0)
	{
		dest.cx = lrint((double)dest.cx / _highdefVP->ScalingFactor);
		dest.cy = lrint((double)dest.cy / _highdefVP->ScalingFactor);
	}
	return dest;
}

RECT BinhexView::toDeviceCoords(const RECT *src, bool originAtLT, bool offsetToLT)
{
	SIZE extDev = toDeviceCoords({ src->right - src->left, src->bottom - src->top });
	RECT rcDev;
	if (originAtLT)
	{
		// left-top corner of shapeRect is the origin of the device context in hdc
		// use offsetToLT if the output rect will be operated on by a GDI drawing function like Ellipse and be passed to PathToRegion to create a HRGN.
		if (offsetToLT)
		{
			// move the origin to LT corner.
			rcDev.left = src->left;
			rcDev.top = src->top;
			rcDev.right = rcDev.left + extDev.cx;
			rcDev.bottom = rcDev.top + extDev.cy;
		}
		else
		{
			// move LT corner to the coordinate origin.
			rcDev.left = 0;
			rcDev.top = 0;
			rcDev.right = extDev.cx;
			rcDev.bottom = extDev.cy;
		}
	}
	else
	{
		// the device origin is at 0. need to translate LT of shapeRect to device coordinates.
		rcDev.left = MulDiv(src->left, _vi.DevicePPI.cx, _vi.LogicalPPI.cx);
		rcDev.top = MulDiv(src->top, _vi.DevicePPI.cy, _vi.LogicalPPI.cy);
		// add the device extents to get RB of shapeRect in device coordinates.
		rcDev.right = rcDev.left + extDev.cx;
		rcDev.bottom = rcDev.top + extDev.cy;
	}
	return rcDev;
}

/* drawRowSelector - draws a row selector triangle.

parameters:
hdc - [in] device context handle.
selectionOffset - [in] content offset of a row in selection.
borderColor - [in] color to use to outline the triangle.
interiorColor - [in] color to fill the triangle in.
*/
void BinhexView::drawRowSelector(HDC hdc, LARGE_INTEGER &selectionOffset, COLORREF borderColor, COLORREF interiorColor)
{
	UINT j, cx, cy;
	POINT pt[3] = { 0 };
	ULONG np; // number of points
	LARGE_INTEGER delta;
	ULONG hitRow1;

	cx = DRAWTEXT_MARGIN_CX + _vi.OffsetColWidth;
	cy = DRAWTEXT_MARGIN_CY;
	delta.QuadPart = selectionOffset.QuadPart - _fri.DataOffset.QuadPart;
	ASSERT(delta.HighPart == 0);
	hitRow1 = delta.LowPart / _vi.BytesPerRow;
	np = 0;
	/*0*/pt[np].x = cx - _vi.ColWidth - _vi.ColWidth / 2;
	pt[np++].y = cy + hitRow1 * _vi.RowHeight;
	/*1*/pt[np].x = pt[0].x + _vi.ColWidth;
	pt[np++].y = pt[0].y + _vi.RowHeight / 2;
	/*2*/pt[np].x = pt[0].x;
	pt[np++].y = pt[0].y + _vi.RowHeight;

	if (interiorColor != 0 || borderColor != 0)
	{
		HBRUSH hbrIn = CreateSolidBrush(interiorColor);
		HBRUSH hbrOut = CreateSolidBrush(borderColor);
		//HBRUSH hbr0 = (HBRUSH)SelectObject(hdc, hbrIn);
		HRGN hrgn = CreatePolygonRgn(pt, 3, ALTERNATE);

		FillRgn(hdc, hrgn, hbrIn);
		FrameRgn(hdc, hrgn, hbrOut, 1, 1);

		//SelectObject(hdc, hbr0);
		DeleteObject(hrgn);
		DeleteObject(hbrOut);
		DeleteObject(hbrIn);
	}
	else
	{
		//HPEN hp = CreatePen(PS_SOLID, 0, borderColor);
		//HPEN hp0 = (HPEN)SelectObject(hdc, hp);

		MoveToEx(hdc, pt[0].x, pt[0].y, NULL);
		for (j = 1; j < np; j++)
		{
			LineTo(hdc, pt[j].x, pt[j].y);
		}
		LineTo(hdc, pt[0].x, pt[0].y);

		//SelectObject(hdc, hp0);
		//DeleteObject(hp);
	}
}

/* getContainingRowRectangle - calculates a rectangle that contains a dump byte at a specified point in client coordinates. BinhexDumpWnd::OnLButtonUp calls the method to get a container rectangle for a character the user clicks in the ASCII dump area.
*/
RECT BinhexView::getContainingRowRectangle(POINT pt)
{
	int iy = (pt.y- _vi.FrameMargin.y) / _vi.RowHeight;
	RECT rc;
	rc.left = _vi.OffsetColWidth- _vi.FrameMargin.x;
	rc.right = _vi.FrameSize.cx - _vi.FrameMargin.x;
	rc.top = iy * _vi.RowHeight + _vi.FrameMargin.y + DRAWTEXT_MARGIN_CY;
	rc.bottom = (iy+1) * _vi.RowHeight + _vi.FrameMargin.y + 2 * DRAWTEXT_MARGIN_CY;
	return rc;
}

