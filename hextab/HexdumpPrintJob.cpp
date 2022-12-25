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
#include "resource.h"
#include "HexdumpPrintJob.h"
#include "HexdumpPageSetupParams.h"


HexdumpPrintJob::HexdumpPrintJob(PRINTDLG *p_, HexdumpPageSetupParams *s_, BinhexMetaView *v_) : _hwnd(p_->hwndOwner), _hfHeader(NULL), _setup(NULL), BinhexMetaView(v_), _docId(0), _printedPages(0)
{
	_setup = new HexdumpPageSetupParams;
	s_->detachTo(*_setup);

	// PD_ALLPAGES, PD_COLLATE, PD_PAGENUMS, PD_PAGENUMS
	_printdlgFlags = p_->Flags;
	if (_printdlgFlags & PD_SELECTION)
	{
		_startPage = 1 + _vi.CurRow / _setup->linesPerPage;
		_endPage = 1 + (_vi.CurRow+_vi.RowsPerPage) / _setup->linesPerPage;
		_pageCount = (_vi.RowsPerPage + _setup->linesPerPage - 1) / _setup->linesPerPage;
	}
	else
	{
		// min and max pages are 1-based.
		_startPage = p_->nFromPage;
		_endPage = p_->nToPage;
		_pageCount = _endPage - _startPage + 1;
	}
	_pageNum = 0;
	_copies = p_->nCopies;
	if (_copies <= 0 || _copies > HEXDUMP_PRINT_MAX_COPIES)
		_copies = 1;

	// print job owns the device context. so clear it from the source struct.
	_hdc = (HDC)InterlockedExchangePointer((LPVOID*)&p_->hDC, NULL);
}

HexdumpPrintJob::~HexdumpPrintJob()
{
	if (_hfHeader)
		DeleteFont(_hfHeader);
	if (_hdc)
		DeleteDC(_hdc);
	if (_setup)
		delete _setup;
}

bool HexdumpPrintJob::start()
{
	DBGPUTS((L"HexdumpPrintJob.start"));
	// run a parameter check.
	if (!_hdc)
		return false;
	if (_setup->printableSz.cx == 0 || _setup->printableSz.cy == 0)
		return false;
	// determine starting page.
	_pageNum = _startPage - 1;
	if (_pageNum < 0)
		return false;
	if (_pageCount <= 0)
		return false;

	// start a worker thread.
	_kill = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	_thread = ::CreateThread(NULL, 0, _runWorker, this, 0, NULL);
	if (_thread == NULL)
		return false;
	return !_terminated;
}

void HexdumpPrintJob::stop(bool cancelJob)
{
	HANDLE hThread = InterlockedExchangePointer(&_thread, NULL);
	if (hThread)
	{
		if (cancelJob)
		{
			DBGPUTS((L"Killing print job..."));
			SetEvent(_kill);
		}
		if (WAIT_OBJECT_0 != ::WaitForSingleObject(hThread, INFINITE))
		{
			ASSERT(FALSE);
		}
		CloseHandle(hThread);
		DBGPUTS((L"HexdumpPrintJob.stop"));
	}
	HANDLE hkill = InterlockedExchangePointer(&_kill, NULL);
	if (hkill)
		CloseHandle(hkill);
}

DWORD HexdumpPrintJob::runWorker()
{
	HWND hwnd = _hwnd;
	DWORD status;
	int copiesLeft = _copies;
	HDCPrintTask task = TskStartJob;
	DOCINFO docInfo = { sizeof(DOCINFO) };
	HANDLE events[] = { _kill };
	while (WAIT_TIMEOUT == (status = WaitForMultipleObjects(ARRAYSIZE(events), events, FALSE, 0)))
	{
		if (task == TskStartJob)
		{
			DBGPUTS((L"HexdumpPrintJob.runWorker: TskStartJob"));
			task = TskStartDoc;
		}
		else if (task == TskStartDoc)
		{
			if (!startDoc(&docInfo))
				break;
			task = TskStartPage;
		}
		else if (task == TskStartPage)
		{
			if (!startPage())
				break;
			task = TskPrintPage;
		}
		else if (task == TskPrintPage)
		{
			if (!printPage())
				break;
			task = TskFinishPage;
		}
		else if (task == TskFinishPage)
		{
			finishPage();
			if (--_pageCount > 0)
			{
				// go to next page
				_pageNum++;
				task = TskStartPage;
				continue;
			}
			// done with current doc (one copy)
			task = TskFinishDoc;
		}
		else if (task == TskFinishDoc)
		{
			finishDoc();
			if (--copiesLeft <= 0)
			{
				task = TskFinishJob;
				break;
			}
		}
		PostMessage(hwnd, WM_COMMAND, IDM_PRINT_EVENT_NOTIF, MAKELPARAM(task, _pageNum));
	}
	if (task == TskFinishJob)
	{
		DBGPUTS((L"HexdumpPrintJob.runWorker: TskFinishJob"));
		status = ERROR_SUCCESS;
	}
	else
	{
		DBGPUTS((L"HexdumpPrintJob.runWorker: AbortDoc"));
		if (task >= TskStartPage)
			::AbortDoc(_hdc);
		task = TskAbortJob;
	}
	PostMessage(hwnd, WM_COMMAND, IDM_PRINT_EVENT_NOTIF, MAKELPARAM(task, _pageNum));
	_terminated = true;
	return status;
}

bool HexdumpPrintJob::startDoc(DOCINFO *docInfo)
{
	DBGPUTS((L"HexdumpPrintJob.startDoc"));

	_docId = ::StartDoc(_hdc, docInfo);
	if (_docId <= 0)
		return false;

	if (!_init(_hdc))
		return false;

	// make sure _highdefVP is allocated and initialized before beginPaint is called. the latter invokes setupViewport which in turn calls setViewMapping. setViewMapping dereferences _highdefVP.
	_highdefVP = new HighDefViewParams(_setup->scaleFit, _setup->screenPPI, _setup->ptPaperSize, _setup->rtMargin);
	_highdefVP->ImageSources.add(new ustring(ROFile::getFilename()));

	beginPaint(_hdc);

	return true;
}

void HexdumpPrintJob::finishDoc()
{
	DBGPUTS((L"HexdumpPrintJob.finishDoc"));
	::EndDoc(_hdc);
	endPaint();
}

bool HexdumpPrintJob::startPage()
{
	DBGPUTS((L"HexdumpPrintJob.startPage"));

	// move the file pointer to the beginning of the data bytes being printed
	if (_printdlgFlags & PD_SELECTION)
	{
		int n = _setup->startLineNum + (_pageNum - _startPage + 1)* _setup->linesPerPage;
		_fri.DataOffset.QuadPart = _vi.BytesPerRow*n;
	}
	else
	{
		_fri.DataOffset.QuadPart = _pageNum * _vi.BytesPerRow*_setup->linesPerPage;
	}
	_fri.HasData = false;

	_dri = { _fri.FileSize, _fri.DataOffset };

	LARGE_INTEGER remainingBytes;
	remainingBytes.QuadPart = (LONGLONG)_fri.FileSize.QuadPart - _fri.DataOffset.QuadPart;

	int printRows = _setup->linesPerPage;
	if (_printdlgFlags & PD_SELECTION)
	{
		if ((int)_vi.RowsPerPage < printRows)
			printRows = _vi.RowsPerPage;
	}
	_dri.RangeBytes = _vi.BytesPerRow*printRows;
	if (_dri.RangeBytes > remainingBytes.LowPart)
		_dri.RangeBytes = remainingBytes.LowPart;
	_dri.RangeLines = (_dri.RangeBytes + _vi.BytesPerRow - 1) / _vi.BytesPerRow;
	_dri.GroupAnnots = (_setup->flags & HDPSP_FLAG_GROUPNOTES);

	SIZE printSz = _setup->printableSz;
	RECT printMargin = _setup->rtMargin;
	RECT printMinMargin = _setup->rtMinMargin;

	//_vi.FontSize = DEFAULT_PRINTER_FONT_SIZE;
	_vi.RowsPerPage = _dri.RangeLines;
	_vi.RowsShown = 0;

	_vi.OffsetColWidth = _vi.ColWidth*(OFFSET_COL_LEN + LEFT_SPACING_COL_LEN);
	_vi.ColsPerPage = _vi.ColsTotal = (COL_LEN_PER_DUMP_BYTE + 1)* _vi.BytesPerRow + RIGHT_SPACING_COL_LEN;
	_vi.VScrollerSize.cx = _vi.VScrollerSize.cy = 0;
	_vi.HScrollerSize.cx = _vi.HScrollerSize.cy = 0;
	_vi.CurRow = (UINT)(_fri.DataOffset.QuadPart / _vi.BytesPerRow);

	_vi.FrameSize.cx = _vi.OffsetColWidth + _vi.ColsPerPage*_vi.ColWidth;
	_vi.FrameSize.cy = _vi.RowsPerPage*_vi.RowHeight;

	_annotOpacity = BHV_FULL_OPACITY;
	_annotFadeoutOpacity = 0; // this is not used in page view. so clear it.
	_shapeOpacity = BHV_NORMAL_SHAPE_PAGEVIEW_OPACITY;

	_annotations.reinit(this, DUMPANNOTATION_FLAG_SHOWCONNECTOR | DUMPANNOTATION_FLAG_DONTDELETEIMAGE);
	_shapes.reinit(this);

	POINT origPt = { 0 }; // don't set it to _vi.FrameMargin
	RECT dumpRect = { 0,0,_vi.FrameSize.cx,_vi.FrameSize.cy };
	RECT pageRect = dumpRect;
	RECT annotsRect = { 0 };

	if (_dri.GroupAnnots)
	{
		if (_regions.size() && _annotations.size())
		{
			SIZE pane = _annotations.getAnnotationsPane(&_regions);
			int dy = dumpRect.bottom - pane.cy;
			int dy2 = dy / 2;

			annotsRect.left = dumpRect.right + _vi.ColWidth;
			annotsRect.right = annotsRect.left + pane.cx;
			pageRect.right += pane.cx;
			if (dy >= 0)
			{
				// the annotation pane is shorter in height than the dump pane.
				annotsRect.top = dy2;
				annotsRect.bottom = dy2 + pane.cy;
			}
			else
			{
				// the annotation pane is taller than the dump pane.
				// lower the dump pane.
				origPt.y -= dy2;
				dumpRect.top -= dy2;
				dumpRect.bottom -= dy2;
				annotsRect.bottom = pane.cy;
				pageRect.bottom = dumpRect.bottom - dy2;
			}
		}
	}

	setViewMapping(origPt, { pageRect.right, pageRect.bottom });

	if (::StartPage(_hdc) <= 0)
		return false;

	return true;
}

void HexdumpPrintJob::finishPage()
{
	DBGPUTS((L"HexdumpPrintJob.finishPage"));
	resetViewport();

	printHeader();

#ifdef _DEBUG
	RECT rc = { _setup->rtMargin.left, _setup->rtMargin.top, _setup->rtMargin.left + _setup->printableSz.cx, _setup->rtMargin.top + _setup->printableSz.cy };
	FrameRect(_hdc, &rc, GetStockBrush(GRAY_BRUSH));

	rc.left = _setup->rtMinMargin.left;
	rc.top = _setup->rtMinMargin.top;
	rc.right = _setup->ptPaperSize.x - _setup->rtMinMargin.right;
	rc.bottom = _setup->ptPaperSize.y - _setup->rtMinMargin.bottom;
	FrameRect(_hdc, &rc, GetStockBrush(GRAY_BRUSH));
#endif//#ifdef _DEBUG

	::EndPage(_hdc);

	_printedPages++;
}

bool HexdumpPrintJob::printPage()
{
	DBGPUTS((L"HexdumpPrintJob.printPage"));

	repaint(_dri.GroupAnnots ? BINHEXVIEW_REPAINT_NO_SELECTOR | BINHEXVIEW_REPAINT_NO_FRAME | BINHEXVIEW_REPAINT_ANNOTATION_ON_SIDE : BINHEXVIEW_REPAINT_NO_SELECTOR | BINHEXVIEW_REPAINT_NO_FRAME);

	//FrameRect(_hdc, &_pageRect, GetStockBrush(DKGRAY_BRUSH));
	return true;
}

void HexdumpPrintJob::printHeader()
{
	if (!_hfHeader)
		return;

	ustring headerText = ROFile::getFileTitle();

	RECT rcHeader = { _setup->rtMinMargin.left, _setup->rtMinMargin.top, _setup->ptPaperSize.x - _setup->rtMinMargin.right, _setup->rtMargin.top };

	COLORREF cf0 = SetTextColor(_hdc, COLORREF_GRAY);
	HFONT hf0 = (HFONT)SelectObject(_hdc, _hfHeader);
	DrawText(_hdc, headerText, headerText._length, &rcHeader, DT_CENTER);
	if (!(_printdlgFlags & PD_SELECTION))
	{
		ustring pn;
		pn.format(L"%d", _pageNum + 1);
		DrawText(_hdc, pn, pn._length, &rcHeader, DT_RIGHT);
	}
	SelectObject(_hdc, hf0);
	SetTextColor(_hdc, cf0);
}

bool HexdumpPrintJob::clipView(RECT *viewRect)
{

	if (!IsRectEmpty(viewRect))
	{
		ASSERT(_hrgnClip==NULL);
		_hrgnClip = CreateRectRgn(viewRect->left, viewRect->top, viewRect->right, viewRect->bottom);
	}
	if (_hrgnClip)
	{
		/* https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-selectcliprgn
		Only a copy of the selected region is used. The region itself can be selected for any number of other device contexts or it can be deleted.
		The SelectClipRgn function assumes that the coordinates for a region are specified in device units.
		To remove a device-context's clipping region, specify a NULL region handle.
		*/
		int clipres = SelectClipRgn(_hdc, _hrgnClip);
		DBGPRINTF((L"SelectClipRgn: %s; CY=%d\n", (clipres == SIMPLEREGION) ? L"SIMPLEREGION" : (clipres == COMPLEXREGION) ? L"COMPLEXREGION" : (clipres == NULLREGION) ? L"NULLREGION" : L"RGN_ERROR", _setup->printableSz.cy));
		if (clipres == ERROR)
			return false;
	}
	return true;
}

void HexdumpPrintJob::releaseClipRegion()
{
	if (_hrgnClip)
	{
		SelectClipRgn(_hdc, NULL);
		DeleteObject(_hrgnClip);
		_hrgnClip = NULL;
	}
}

bool HexdumpPrintJob::_init(HDC hdc)
{
	_vi.FontSize = _setup->fontSize;
	if (_setup->flags & HDPSP_FLAG_FITTOWIDTH)
		_vi.FontSize = _setup->fontSizeFit;

	_vi.LogicalPPI.cx = HEXDUMP_LOGICAL_TOI;
	_vi.LogicalPPI.cy = HEXDUMP_LOGICAL_TOI;
	_vi.DevicePPI.cx = GetDeviceCaps(hdc, LOGPIXELSX);
	_vi.DevicePPI.cy = GetDeviceCaps(hdc, LOGPIXELSY);

	if (!BinhexMetaView::_init(hdc))
		return false;

	int cy = MulDiv(8, _vi.LogicalPPI.cy, FONT_PTS_PER_INCH);
	_hfHeader = CreateFont(-cy, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, L"Arial");

	_AnnotBorder = MulDiv(1, _vi.LogicalPPI.cx, _setup->screenPPI.cx);

	// convert these in ppi to 1/1000"
	_vi.FrameMargin =
	{
		MulDiv(_vi.FrameMargin.x, _vi.LogicalPPI.cx, _setup->screenPPI.cx),
		MulDiv(_vi.FrameMargin.y, _vi.LogicalPPI.cy, _setup->screenPPI.cy)
	};

	if (_setup->flags & HDPSP_FLAG_FITTOWIDTH)
	{
		double X = ((double)_setup->requiredCols + _setup->PV_annotsChars) *(double)_vi.ColWidth;
		_setup->scaleFit = X / (double)_setup->printableSz.cx;
	}
	return true;
}

void HexdumpPrintJob::setupViewport()
{
	// map a logical inch (1000 TOI units) to printer (device) dpi (e.g., 600 dpi)".
	SetMapMode(_hdc, MM_ISOTROPIC);
	setViewMapping({ 0,0 }, { 0,0 });

	// find the full extents of the dump rectangle.
	ASSERT(_setup->requiredCols == (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + RIGHT_SPACING_COL_LEN + (COL_LEN_PER_DUMP_BYTE + 1) * _vi.BytesPerRow));
	// generate a text string that spans the full dump width. since it's a fixed pitch font, any character will do. use 'X'.
	astring src;
	src.fill(_setup->requiredCols, 'X');
	// select the font we use in hex dump to the printer device context.
	HFONT hf0 = (HFONT)SelectObject(_hdc, _hfontDump);
	// ask gdi to measure the full width.
	GetTextExtentExPointA(_hdc, src, src._length, 0, NULL, NULL, &_vi.FullRowExts);
	SelectObject(_hdc, hf0);
}

void HexdumpPrintJob::resetViewport()
{
	releaseClipRegion();

	_highdefVP->ClipRect = { 0 };
	_highdefVP->DeviceOrigin = { 0 };
	_highdefVP->LogicalDPI = _vi.LogicalPPI;

	// logical origin and extents
	SetWindowOrgEx(_hdc, 0, 0, NULL);
	SetWindowExtEx(_hdc, _vi.LogicalPPI.cx, _vi.LogicalPPI.cy, NULL);
	// device origin and extents
	SetViewportOrgEx(_hdc, 0, 0, NULL);
	SetViewportExtEx(_hdc, _vi.DevicePPI.cx, _vi.DevicePPI.cy, NULL);
}

bool HexdumpPrintJob::_atEndingPage()
{
	// total rows: _vi.RowsTotal may not be reliable. so, use FileSize to recompute the total number of rows.
	ULONG rows = _fri.FileSize.LowPart / _vi.BytesPerRow;
	int pages = rows / _setup->linesPerPage;
	// current row: use DataOffset to recompute the current row number. _vi.CurRow may not be reliable.
	ULONG r = _fri.DataOffset.LowPart / _vi.BytesPerRow;
	// point to the bottom row on the page.
	r += _vi.RowsPerPage;
	// get the page number that the bottom row belongs to.
	int p = r / _setup->linesPerPage;
	// if the current page is less than the total, we're not at the ending page.
	if (p < pages)
		return false;
	// we're at the final page.
	return true;
}

void HexdumpPrintJob::setViewMapping(POINT orig, SIZE exts)
{
	// logical origin and extents (in TOI or 1/1000")
	// device origin and extents (in ppi or dpi)
	int cx, cy;
	if ((_setup->flags & HDPSP_FLAG_FITTOWIDTH) && _setup->scaleFit != 1.0)
	{
		cx = lrint((double)_vi.LogicalPPI.cx * _setup->scaleFit);
		cy = lrint((double)_vi.LogicalPPI.cy * _setup->scaleFit);
	}
	else
	{
		cx = _vi.LogicalPPI.cx;
		cy = _vi.LogicalPPI.cy;
	}

	int mx = MulDiv(_setup->rtMargin.left + orig.x, _vi.DevicePPI.cx, _vi.LogicalPPI.cx);
	int my = MulDiv(_setup->rtMargin.top + orig.y, _vi.DevicePPI.cy, _vi.LogicalPPI.cy);

	if (_highdefVP->DeviceOrigin.x != mx || _highdefVP->DeviceOrigin.y != my)
	{
		SetWindowOrgEx(_hdc, 0, 0, NULL);
		SetViewportOrgEx(_hdc, mx, my, NULL);
		_highdefVP->DeviceOrigin.x = mx;
		_highdefVP->DeviceOrigin.y = my;
	}
	if (_highdefVP->LogicalDPI.cx != cx || _highdefVP->LogicalDPI.cy != cy)
	{
		SetWindowExtEx(_hdc, cx, cy, NULL);
		SetViewportExtEx(_hdc, _vi.DevicePPI.cx, _vi.DevicePPI.cy, NULL);
		_highdefVP->LogicalDPI.cx = cx;
		_highdefVP->LogicalDPI.cy = cy;
		_highdefVP->ScreenCharExts.cx = _setup->screenColWidth;
		_highdefVP->ScreenCharExts.cy = _setup->screenRowHeight;
	}

	if (exts.cx && exts.cy)
	{
		// define a clipping region in device units for the printable rectangle. it's to trim anything sticking out into the margins.
		// if orig is non-zero, recalc the offset to the coordinate origin.
		if (orig.x)
			mx = MulDiv(_setup->rtMargin.left, _vi.DevicePPI.cx, _vi.LogicalPPI.cx);
		if (orig.y)
			my = MulDiv(_setup->rtMargin.top, _vi.DevicePPI.cy, _vi.LogicalPPI.cy);
		RECT rc = { mx,my,mx,my };
		int cx0 = _setup->printableSz.cx;
		int cy0 = _setup->printableSz.cy;
		if (((_setup->flags & HDPSP_FLAG_GROUPNOTES) || (_printdlgFlags & PD_SELECTION)) && !_atEndingPage())
		{
			int cy2 = exts.cy;
			if ((_setup->flags & HDPSP_FLAG_FITTOWIDTH) && _setup->scaleFit != 1.0)
			{
				cy2 = lrint((double)cy2 / _setup->scaleFit);
			}
			if (cy0 > cy2)
				cy0 = cy2;
		}
		rc.right += MulDiv(cx0, _vi.DevicePPI.cx, _vi.LogicalPPI.cx);
		rc.bottom += MulDiv(cy0, _vi.DevicePPI.cy, _vi.LogicalPPI.cy);
		if (clipView(&rc))
			_highdefVP->ClipRect = rc;
	}
}

#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
/* _getLogicalXCoord - translates an x-coordinate in screen units to a coordinate in logical units. overrides the base class method to apply a correction based on a high-def metrics from the printer.
*/
int HexdumpPrintJob::_getLogicalXCoord(int screenX)
{
	if (_vi.FullRowExts.cx != 0)
	{
		double charWidth = (double)_setup->screenColWidth;
		double Nc = (double)screenX/charWidth;
		double x = Nc*(double)_vi.FullRowExts.cx / (double)_setup->requiredCols;
		return lrint(x);
	}
	return BinhexMetaView::_getLogicalXCoord(screenX);
}

/* _getLogicalYCoord - translates an y-coordinate in screen units to a coordinate in logical units. overrides the base class method to apply a correction based on a high-def metrics from the printer.
*/
int HexdumpPrintJob::_getLogicalYCoord(int screenY)
{
	if (_vi.FullRowExts.cy != 0)
	{
		double charHeight = (double)_setup->screenRowHeight;
		double Nc = (double)screenY / charHeight;
		double y = Nc * (double)_vi.FullRowExts.cy;
		return lrint(y);
	}
	return BinhexMetaView::_getLogicalYCoord(screenY);
}

/* _getHexColCoord - translates a column position in the hex dump area to an x-coordinate in logical units. overrides the base class method to apply a correction based on a high-def metrics from the printer.

return value:
x-coordinate in logical units of the hexdump column.

parameters:
colPos - [in] a column position to convert.
deltaNumerator - [in,optional] numerator part of a fraction of a column space to add to the colPos column position, if deltaDenominator is non-zero; or a number of column spaces to add to colPos.
deltaDenominator - [in,optional] denominator part of the fraction.
*/
int HexdumpPrintJob::_getHexColCoord(int colPos, int deltaNumerator, int deltaDenominator)
{
	if ((_setup->flags & HDPSP_FLAG_FITTOWIDTH) && _vi.FullRowExts.cx != 0)
	{
		int x;
		int p2 = OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * colPos;
		if (deltaDenominator)
		{
			p2 *= deltaDenominator;
			p2 += deltaNumerator;
			x = MulDiv(_vi.FullRowExts.cx, p2, _setup->requiredCols*deltaDenominator);
		}
		else
		{
			p2 += deltaNumerator;
			x = MulDiv(_vi.FullRowExts.cx, p2, _setup->requiredCols);
		}
		return x;
	}
	return BinhexMetaView::_getHexColCoord(colPos, deltaNumerator);
}

/* _getHexColCoord - translates a column position in the ASCII dump area to an x-coordinate in logical units. overrides the base class method to apply a correction based on a high-def metrics from the printer.

return value:
x-coordinate in logical units of the ASCII dump column.

parameters:
colPos - [in] a column position to convert.
*/
int HexdumpPrintJob::_getAscColCoord(int colPos)
{
	if ((_setup->flags & HDPSP_FLAG_FITTOWIDTH) && _vi.FullRowExts.cx != 0)
	{
		int p2 = OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + RIGHT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + colPos;
		int x = MulDiv(_vi.FullRowExts.cx, p2, _setup->requiredCols);
		return x;
	}
	return BinhexMetaView::_getAscColCoord(colPos);
}
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
