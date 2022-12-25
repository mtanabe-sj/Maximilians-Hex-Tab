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
#pragma once
#include "lib.h"
#include "GripCursor.h"

/* the move to BinhexDumpWnd made the frame clipping obsolete.
#define VIEWINFO_SUPPORTS_FRAME_CLIPPING
*/
#define BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
#define BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE

// default value for the transparency parameter passed to the Win32 AlphaBlend API function. should be set to 64 or less (0 for full transparency, or 255 for full opacity).
#define BHV_NORMAL_OPACITY 32
// the maximum value of opacity AlphaBlend accepts.
#define BHV_FULL_OPACITY 255
// default AlphaBlend opacity for GShapes when saving to a file or printing the hex dump. see BinhexMetaPageView::init.
#define BHV_NORMAL_SHAPE_PAGEVIEW_OPACITY 64
// normal-time opacity for shapes
#define BHV_NORMAL_SHAPE_OPACITY 96
// on-continue-drag opacity for shapes
#define BHV_CONTINUEDRAG_SHAPE_OPACITY 192
// on-continue-drag opacity for annotations
#define BHV_CONTINUEDRAG_ANNOT_OPACITY 128


struct VIEWINFO {
	LARGE_INTEGER SelectionOffset;
	UINT CurCol;
	UINT ColWidth;
	UINT ColsPerPage; // number of columns in current view; equals ColWidth or less; it's less if left columns are scrolled out horizontally.
	UINT ColsTotal;
	UINT CurRow;
	UINT RowHeight;
	UINT RowsPerPage;
	UINT RowsShown;
	UINT RowsTotal;
	ULONG BytesPerRow;
	ULONG FontSize;
	int FontHeight;
	int AnnotationFontHeight;
	UINT AnnotationColWidth, AnnotationRowHeight;
	POINT FrameMargin; // defaults to {FRAMEMARGIN_CX, FRAMEMARGIN_CY}
	SIZE FrameSize;
	SIZE VScrollerSize, HScrollerSize;
	UINT OffsetColWidth;
	bool VScrollerVisible, HScrollerVisible;
	SIZE DevicePPI, LogicalPPI;
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
	SIZE FullRowExts;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
};


struct HighDefViewParams
{
	ULONG Flags; // flags on how to interpret the view parameters. currently not used.
	double ScalingFactor; // Fit-to-Page-Width scaling factor from HexdumpPrintDlg::choosePrinter.
	SIZE ScreenPPI; // horizontal and vertical pixel densities in screen pixels.
	SIZE ScreenCharExts; // metrics from a corresponding screen font.
	POINT LogicalPageSize; // paper size on a printer in logical units of thousandths of inches (TOI)
	RECT LogicalMargins; // print margins in TOI
	POINT DeviceOrigin; // a point in device coordinates that the logical origin maps to; it usually corresponds to the left and top margins in device units.
	SIZE LogicalDPI; // logical DPI, scaled to ScalingFactor if ScalingFactor is non-zero.
	RECT ClipRect; // clipping rectangle in device units; usually coincides with the four corners of the printable area on printer paper
	objList<ustring> ImageSources; // list of filenames; the 0th element is the filename of a very file subject to hex dump. non-zero elements are filenames of other image source types (that are yet to be defined).

	HighDefViewParams(double sf, SIZE sppi, POINT psz, RECT mgn) : Flags(0), ScalingFactor(sf), ScreenPPI{ sppi }, LogicalPageSize{ psz }, LogicalMargins{ mgn }, DeviceOrigin{ 0 }, LogicalDPI{ 0 }, ClipRect{ 0 }
	{
	}
};


class ViewContext
{
public:
	ViewContext() : _hw(NULL), _hdc(NULL), _vi{ 0 } {}
	~ViewContext() { releaseVC(); }

	HWND _hw;
	VIEWINFO _vi;
	HDC _hdc;

	virtual HDC setVC(bool clipFrame_ = false)
	{
		_hdc = GetDC(_hw);
		setupViewport();
		if (clipFrame_)
			clipFrame();
		return _hdc;
	}
	virtual void releaseVC()
	{
		if (_hdc)
		{
			if (_hrgnClip)
				unclipFrame();
			ReleaseDC(_hw, _hdc);
			_hdc = NULL;
		}
	}
	bool invalidateInflated(RECT *rect)
	{
		if (IsRectEmpty(rect))
			return false;
		InflateRect(rect, POSTMOUSEFLYOVER_RESTORE_MARGIN, POSTMOUSEFLYOVER_RESTORE_MARGIN);
		InvalidateRect(_hw, rect, TRUE);
		return true;
	}
	void invalidate(RECT *rect = NULL)
	{
		InvalidateRect(_hw, rect, TRUE);
	}
	void update()
	{
		UpdateWindow(_hw);
	}

	bool clipFrame(HDC hdc=NULL, HRGN hrgnUser =NULL, int combineMode=0)
	{
#ifdef VIEWINFO_SUPPORTS_FRAME_CLIPPING
		if (!hdc)
			hdc = _hdc;
		RECT rcFrame = { _vi.FrameMargin.x, _vi.FrameMargin.y, _vi.FrameMargin.x + _vi.FrameSize.cx, _vi.FrameMargin.y + _vi.FrameSize.cy };
		// if a horizontal scrollbar is active, the left edge may have moved out of the display area. if so, it should not be included in the blending.
		if (_vi.CurCol)
			rcFrame.left += _vi.OffsetColWidth;
		/*if (hrgnUser)
			_hrgnClip = hrgnUser;
		else
			_hrgnClip = CreateRectRgn(rcFrame.left, rcFrame.top, rcFrame.right, rcFrame.bottom);*/
		_hrgnClip = CreateRectRgn(rcFrame.left, rcFrame.top, rcFrame.right, rcFrame.bottom);
		if (hrgnUser)
			CombineRgn(_hrgnClip, _hrgnClip, hrgnUser, combineMode);
		int clipres = SelectClipRgn(hdc, _hrgnClip);
		ASSERT(clipres != ERROR); // SIMPLEREGION (2) or COMPLEXREGION (3)
		return true;
#else//#ifdef VIEWINFO_SUPPORTS_FRAME_CLIPPING
		return true;
#endif//#ifdef VIEWINFO_SUPPORTS_FRAME_CLIPPING
	}
	void unclipFrame(HDC hdc=NULL)
	{
		if (!hdc)
			hdc = _hdc;
#ifdef VIEWINFO_SUPPORTS_FRAME_CLIPPING
		SelectClipRgn(hdc, NULL);
#endif//#ifdef VIEWINFO_SUPPORTS_FRAME_CLIPPING
		if (_hrgnClip)
		{
			DeleteObject(_hrgnClip);
			_hrgnClip = NULL;
		}
	}

protected:
	HRGN _hrgnClip;

	virtual void setupViewport()
	{
		ASSERT(_vi.FrameMargin.x == FRAMEMARGIN_CX);
		SetViewportOrgEx(_hdc, _vi.FrameMargin.x, _vi.FrameMargin.y, NULL);
		// this is meaningless unless SetWindowExtEx() too is called. get rid of it.
		//SetViewportExtEx(_hdc, _vi.FrameSize.cx, _vi.FrameSize.cy, NULL);
	}
};


const UINT BINHEXVIEW_REPAINT_RESET_FONT = 1;
const UINT BINHEXVIEW_REPAINT_DUMP_ONLY = 2;
const UINT BINHEXVIEW_REPAINT_NO_SELECTOR = 4;
const UINT BINHEXVIEW_REPAINT_NO_FRAME = 8;
const UINT BINHEXVIEW_REPAINT_ANNOTATION_ON_SIDE = 0x10;
const UINT BINHEXVIEW_REPAINT_NO_DUMP = 0x20;

class BinhexView : public ViewContext
{
public:
	BinhexView() :
		_hbmPaint(NULL),
		_gcursor(NULL),
		_hfontDump(NULL),
		_hfontHeader(NULL),
		_hfontAnnotation(NULL),
		_fri{ 0 },
		_annotOpacity(BHV_NORMAL_OPACITY),
		_annotFadeoutOpacity(0),
		_shapeOpacity(BHV_NORMAL_OPACITY),
		_rowselectorBorderColor(COLORREF_BLUE),
		_rowselectorInteriorColor(COLORREF_YELLOW),
		_AnnotBorder(2),
		_fontPtsPI(72),
		_MetafileId(0),
		_highdefVP(NULL),
		_hrgnClip(NULL),
		_kill(NULL),
		_bhvp(BHVP_SHELL)
	{
		_vi.FontSize = DEFAULT_FONT_SIZE;
		_vi.BytesPerRow = DEFAULT_HEX_BYTES_PER_ROW;
	}
	~BinhexView() { term(); }

	GripCursorVector *_gcursor;
	FILEREADINFO _fri;
	HFONT _hfontDump;
	HFONT _hfontHeader;
	HFONT _hfontAnnotation;
	DWORD _annotOpacity;
	DWORD _annotFadeoutOpacity; // transparency after a page-wise scroll action; used in AnnotationOperation::drawAnnotation().
	DWORD _shapeOpacity;
	HBITMAP _hbmPaint;
	SHORT _AnnotBorder;
	ULONG _MetafileId;
	HighDefViewParams *_highdefVP;
	HANDLE _kill;
	HRGN _hrgnClip;

	enum BINHEXVIEWPROCESS {
		BHVP_SHELL = 0,
		BHVP_PRIVATE_NORMAL,
		BHVP_PRIVATE_LARGE,
	} _bhvp;

	bool init(HWND h);
	void term();

	HRESULT repaint(UINT flags=0, HDC hdc=NULL);
	HRESULT repaint2(UINT flags = 0);
	ULONG drawRowBytes(HDC hdc, LARGE_INTEGER offsetToData, ULONG dataLength);

	virtual HDC setVC(bool clipFrame=false)
	{
		ViewContext::setVC(clipFrame);
		_hf0 = (HFONT)SelectObject(_hdc, _hfontDump);
		return _hdc;
	}
	virtual void releaseVC()
	{
		SelectObject(_hdc, _hf0);
		ViewContext::releaseVC();
	}

	void beginPaint(HDC hdc)
	{
		_hdc = hdc;
		setupViewport();
		_hf0 = (HFONT)SelectObject(_hdc, _hfontDump);
	}
	void endPaint()
	{
		SelectObject(_hdc, _hf0);
		_hdc = NULL;
		_hf0 = NULL;
	}
	virtual void resetView()
	{
		_fri.DataOffset.QuadPart = 0;
	}

	virtual ULONG measureTextDumpRegion(HDC hdc, POINT pt[8], int cx, int cy, LONG hitRow1, LONG hitRow2, LONG hitCol1, LONG hitCol2, int regionInView, LARGE_INTEGER regionStart, ULONG regionLength);
	virtual ULONG measureHexDumpRegion(POINT pt[8], int cx, int cy, LONG hitRow1, LONG hitRow2, LONG hitCol1, LONG hitCol2, int regionInView);

	RECT getContainingRowRectangle(POINT pt);

#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
	virtual int _getHexColCoord(int colPos, int deltaNumerator=0, int deltaDenominator=0);
	virtual int _getAscColCoord(int colPos);
	virtual int _getLogicalXCoord(int screenX);
	virtual int _getLogicalYCoord(int screenY);
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT

	SIZE toDeviceCoords(const SIZE src);
	RECT toDeviceCoords(const RECT *src, bool originAtLT, bool offsetToLT=false);
	virtual double screenToLogical(double x);
	virtual int screenToLogical(int x);

	virtual bool lockObjAccess() { return false; }
	virtual void unlockObjAccess() {}

protected:
	HFONT _hf0;
	COLORREF _rowselectorBorderColor, _rowselectorInteriorColor;
	int _fontPtsPI; // points per inch (=72) for font creation

	virtual bool _init(HDC hdc);
	virtual HRESULT _repaintInternal(HDC hdc, RECT frameRect, UINT flags);

	virtual RECT initViewInfo();
#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
	virtual void _finishDumpLinePartial(HDC hdc, LPSTR linebuf, ULONG bufSize, ULONG asciiLen, ULONG &fiDataOffset, UINT cx, UINT ix, UINT iy);
	virtual void _finishDumpLine(HDC hdc, LPSTR linebuf, ULONG bufSize, ULONG &fiDataOffset, UINT cx, UINT iy);
	void drawHexDump(HDC hdc);
#else//#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
	void drawHexDump(HDC hdc);
#endif//#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
	void drawRowSelector(HDC hdc, LARGE_INTEGER &selectionOffset, COLORREF borderColor, COLORREF interiorColor);
	HRESULT repaintByMemoryDC(UINT flags, HDC hdc);
};


// 1000 thousandths of an inch
#define HEXDUMP_LOGICAL_TOI 1000
// reference high-def character width for 'Courier New' at 10pt on a 600-dpi printer
#define HEXDUMP_LOGICAL_HIGHDEF_CHAR_WIDTH 83.0
// reference high-def character height
#define HEXDUMP_LOGICAL_HIGHDEF_CHAR_HEIGHT 148.0
// high-def character's height w/o internal leading.
#define HEXDUMP_LOGICAL_HIGHDEF_CHAR_HEIGHT_WO_INTERNAL_LEADING 138.0
// high-def character's ratio of full height to the height w/o internal leading.
#define HEXDUMP_LOGICAL_HIGHDEF_CHAR_H2H_RATIO (HEXDUMP_LOGICAL_HIGHDEF_CHAR_HEIGHT/HEXDUMP_LOGICAL_HIGHDEF_CHAR_HEIGHT_WO_INTERNAL_LEADING)
// high-def character's ratio of width to height.
#define HEXDUMP_LOGICAL_HIGHDEF_CHAR_W2H_RATIO (HEXDUMP_LOGICAL_HIGHDEF_CHAR_WIDTH/HEXDUMP_LOGICAL_HIGHDEF_CHAR_HEIGHT)

struct HexdumpLogicalPageViewParams
{
	int FontSize;
	double fCharHeight, fCharWidth;

	HexdumpLogicalPageViewParams(int fs)
	{
		update(fs);
	}
	void update(int fs)
	{
		FontSize = fs;
		double heightWithoutInternalLeading = (double)FontSize * HEXDUMP_LOGICAL_TOI / FONT_PTS_PER_INCH;
		fCharHeight = heightWithoutInternalLeading * HEXDUMP_LOGICAL_HIGHDEF_CHAR_H2H_RATIO;
		fCharWidth = fCharHeight * HEXDUMP_LOGICAL_HIGHDEF_CHAR_W2H_RATIO;
	}
	void fixupViewInfo(VIEWINFO *vi)
	{
		double r = (double)vi->DevicePPI.cy / HEXDUMP_LOGICAL_TOI;
		int cx = (vi->FrameSize.cx - 2 * vi->FrameMargin.x) / vi->ColWidth;
		int cy = (vi->FrameSize.cy - 2 * vi->FrameMargin.y) / vi->RowHeight;
		double fFrameWidth = fCharWidth * (double)cx;
		double fFrameHeight = fCharHeight * (double)cy;
		double fx = r * fFrameWidth;
		double fy = r * fFrameHeight;
		vi->FrameSize.cx = lrint(fx) + 2 * vi->FrameMargin.x;
		vi->FrameSize.cy = lrint(fy) + 2 * vi->FrameMargin.y;
	}
	SIZE fixupViewExtents(VIEWINFO *vi, SIZE viewExtents, SIZE logicalExtents)
	{
		double rx = (double)viewExtents.cx / (double)logicalExtents.cx;
		double ry = (double)viewExtents.cy / (double)logicalExtents.cy;
		int cx = (vi->FrameSize.cx - 2 * vi->FrameMargin.x) / vi->ColWidth;
		int cy = (vi->FrameSize.cy - 2 * vi->FrameMargin.y) / vi->RowHeight;
		double fFrameWidth = fCharWidth * (double)cx;
		double fFrameHeight = fCharHeight * (double)cy;
		/* do not limit the frame dimensions to the logical extents. doing so would cause the print preview to appear squeezed.
		if (fFrameWidth > (double)logicalExtents.cx)
			fFrameWidth = (double)logicalExtents.cx;
		if (fFrameHeight > (double)logicalExtents.cy)
			fFrameHeight = (double)logicalExtents.cy;
		*/
		double fx = rx * fFrameWidth;
		double fy = ry * fFrameHeight;
		SIZE vex;
		vex.cx = lrint(fx);
		vex.cy = lrint(fy);
		return vex;
	}
	SIZE fixupViewExtents(VIEWINFO *vi, SIZE viewExtents, SIZE logicalExtents, RECT rcDump, RECT rcPage)
	{
		SIZE vex1 = fixupViewExtents(vi, viewExtents, logicalExtents);
		double rx = (double)(rcPage.right - rcPage.left)/(double)(rcDump.right-rcDump.left);
		double ry = (double)(rcPage.bottom - rcPage.top) / (double)(rcDump.bottom - rcDump.top);
		SIZE vex2;
		vex2.cx = lrint(rx * (double)vex1.cx);
		vex2.cy = lrint(ry * (double)vex1.cy);
		return vex2;
	}
	double scaleCharWidth(int devicePPI)
	{
		return fCharWidth * devicePPI / HEXDUMP_LOGICAL_TOI;
	}
	double scaleCharHeight(int devicePPI)
	{
		return fCharHeight * devicePPI / HEXDUMP_LOGICAL_TOI;
	}
};

