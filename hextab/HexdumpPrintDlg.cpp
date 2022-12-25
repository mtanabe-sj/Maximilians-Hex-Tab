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
#include "BinhexMetaView.h"
#include "HexdumpPrintDlg.h"
#include "HexdumpPageSetupParams.h"
#include "HexdumpPrintJob.h"


HexdumpPrintDlg::HexdumpPrintDlg(HWND h_, BinhexMetaView *b_, HexdumpPageSetupParams *s_) : SimpleDlg(PRINTDLGORD, h_), _setup(s_), _bmv(b_), _vi{ 0 }, _fri{ 0 }, _pd(NULL), _hfHeader(NULL), _logicalPage(NULL), _editSel(0), _flags(0)
{
}
HexdumpPrintDlg::~HexdumpPrintDlg()
{
	clean();
}

HRESULT HexdumpPrintDlg::choosePrinter()
{
	HRESULT hr = E_ABORT;

	_pd = (PRINTDLG*)GlobalAlloc(LPTR, sizeof(PRINTDLG));
	//  Initialize the PRINTDLG structure.
	_pd->lStructSize = sizeof(PRINTDLG);
	_pd->hwndOwner = _hwndParent;
	_pd->hDevMode = _setup->hDevMode;
	_pd->hDevNames = _setup->hDevNames;
	_pd->hDC = NULL;
	_pd->Flags = PD_RETURNDC | PD_COLLATE | PD_ENABLEPRINTTEMPLATE | PD_ENABLEPRINTHOOK | PD_SELECTION;
	_pd->nFromPage = _pd->nToPage = _setup->curPage;
	_pd->nMinPage = 1;
	_pd->nMaxPage = _setup->maxPage;
	_pd->nCopies = 1;
	_pd->hInstance = LibInstanceHandle;
	_pd->lpPrintTemplateName = MAKEINTRESOURCE(_idd);
	_pd->lpfnPrintHook = _hookproc;
	_pd->lCustData = (LPARAM)(ULONG_PTR)this;

	//  run the printer selection dialog.
	if (PrintDlg(_pd))
	{
		if (_pd->hDC)
		{
			// update the page setup based on selected printer.
			_setup->update(_pd);
			// caller should immediately call startPrinting to create a print job.
			// the hDC and page range(s) will be internally passed to the print job instance.
			hr = S_OK;
		}
		else
			hr = E_UNEXPECTED;
	}
	return hr;
}

HexdumpPrintJob *HexdumpPrintDlg::startPrinting()
{
	HexdumpPrintJob *pjob = new HexdumpPrintJob(_pd, _setup, _bmv);
	if (pjob->start())
		return pjob;
	delete pjob;
	return NULL;
}

void HexdumpPrintDlg::clean()
{
	ASSERT(_fri[0].DataBuffer == NULL && _fri[1].DataBuffer == NULL);
	HFONT hf = (HFONT)InterlockedExchangePointer((LPVOID*)&_hfHeader, NULL);
	if (hf)
		DeleteFont(hf);
	HexdumpLogicalPageViewParams *p = (HexdumpLogicalPageViewParams*)InterlockedExchangePointer((LPVOID*)&_logicalPage, NULL);
	if (p)
		delete p;
	PRINTDLG *pd = (PRINTDLG*)InterlockedExchangePointer((LPVOID*)&_pd, NULL);
	if (pd)
	{
		if (pd->hDC != NULL)
			DeleteDC(pd->hDC);
		GlobalFree((HGLOBAL)pd);
	}
}

double HexdumpPrintDlg::_getDlgItemDouble(UINT idCtrl)
{
	WCHAR buf[40] = { 0 };
	GetDlgItemText(_hdlg, idCtrl, buf, ARRAYSIZE(buf));
	return _wtof(buf);
}

void HexdumpPrintDlg::_setDlgItemDouble(UINT idCtrl, double newVal)
{
	WCHAR buf[40] = { 0 };
	_swprintf_c(buf, ARRAYSIZE(buf), L"%.02g", newVal);
	SetDlgItemText(_hdlg, idCtrl, buf);
}

BOOL HexdumpPrintDlg::OnInitDialog()
{
	SimpleDlg::OnInitDialog();

	_flags |= HDPD_FLAG_INITIALIZING;

	_hwndVS = GetDlgItem(_hdlg, IDC_VSCROLLBAR);
	_hPreview = GetDlgItem(_hdlg, IDC_STATIC_PAGE1);

#ifdef _DEBUG
	RECT rc = { 0,0,90,108 };
	MapDialogRect(_hdlg, &rc);
	DBGPRINTF((L"MapDialogRect: Width = 90-->%d; Height = 108-->%d\n", rc.right, rc.bottom));
#endif//#ifdef _DEBUG
	_vi[RNG_LINES] = _vi[RNG_PAGES] = _bmv->_vi;
	if (_bmv->_vi.BytesPerRow < MAX_HEX_BYTES_PER_ROW)
		_vi[RNG_LINES].FontSize = _vi[RNG_PAGES].FontSize = DEFAULT_PRINTER_FONT_SIZE;

	_fri[RNG_LINES] = _fri[RNG_PAGES] = _bmv->_fri;
	_fri[RNG_LINES].DataBuffer = _fri[RNG_PAGES].DataBuffer = NULL;
	_fri[RNG_LINES].HasData = _fri[RNG_PAGES].HasData = false; // clearing this flag is very important. failure likely corrupts the preview display.

	_groupNotes = _fitToWidth = false;

	_logicalPage = new HexdumpLogicalPageViewParams(_vi[RNG_LINES].FontSize);

	HexdumpPageSetupParams &setup = *_setup;
	setup.dumpDevInfo();
	setup.updateViewInfo(_vi + RNG_LINES, _fitToWidth, _groupNotes, _logicalPage);

	// configure the Page mode view parameters.
	// zero-based current page number
	int curPage = setup.curPage - 1;
	// fix the file offset and starting row for Page mode.
	_fri[RNG_PAGES].DataOffset.QuadPart = curPage*setup.linesPerPage*_bmv->_vi.BytesPerRow;
	_vi[RNG_PAGES].CurRow = curPage * setup.linesPerPage;

	// configure the Line mode view parameters.
	_vi[RNG_LINES].RowsTotal = _vi[RNG_LINES].CurRow + _bmv->_vi.RowsShown;
	_vi[RNG_LINES].RowsPerPage = _vi[RNG_PAGES].RowsPerPage = setup.linesPerPage;
	_vi[RNG_LINES].RowsShown = _vi[RNG_PAGES].RowsShown = 0;
	_vi[RNG_LINES].VScrollerVisible = false;
	_vi[RNG_PAGES].VScrollerVisible = true;
	_vi[RNG_LINES].HScrollerVisible = _vi[RNG_PAGES].HScrollerVisible = false;

	_margin[MGN_TOP] = setup.rtMargin.top;
	_margin[MGN_LEFT] = setup.rtMargin.left;
	_margin[MGN_RIGHT] = setup.rtMargin.right;
	_margin[MGN_BOTTOM] = setup.rtMargin.bottom;
	_minMargin[MGN_TOP] = setup.rtMinMargin.top;
	_minMargin[MGN_LEFT] = setup.rtMinMargin.left;
	_minMargin[MGN_RIGHT] = setup.rtMinMargin.right;
	_minMargin[MGN_BOTTOM] = setup.rtMinMargin.bottom;
	_maxMargin[MGN_TOP] = 2 * setup.rtMargin.top;
	_maxMargin[MGN_LEFT] = 2 * setup.rtMargin.left;
	_maxMargin[MGN_RIGHT] = 2 * setup.rtMargin.right;
	_maxMargin[MGN_BOTTOM] = 2 * setup.rtMargin.bottom;

	SendDlgItemMessage(_hdlg, IDC_SPIN_TOP, UDM_SETPOS, 0, _margin[MGN_TOP] / HEXDUMP_MARGIN_DELTA);
	SendDlgItemMessage(_hdlg, IDC_SPIN_LEFT, UDM_SETPOS, 0, _margin[MGN_LEFT] / HEXDUMP_MARGIN_DELTA);
	SendDlgItemMessage(_hdlg, IDC_SPIN_RIGHT, UDM_SETPOS, 0, _margin[MGN_RIGHT] / HEXDUMP_MARGIN_DELTA);
	SendDlgItemMessage(_hdlg, IDC_SPIN_BOTTOM, UDM_SETPOS, 0, _margin[MGN_BOTTOM] / HEXDUMP_MARGIN_DELTA);
	SendDlgItemMessage(_hdlg, IDC_SPIN_TOP, UDM_SETRANGE, 0, MAKELONG(_maxMargin[MGN_TOP] / HEXDUMP_MARGIN_DELTA, _minMargin[MGN_TOP] / HEXDUMP_MARGIN_DELTA));
	SendDlgItemMessage(_hdlg, IDC_SPIN_LEFT, UDM_SETRANGE, 0, MAKELONG(_maxMargin[MGN_LEFT] / HEXDUMP_MARGIN_DELTA, _minMargin[MGN_LEFT] / HEXDUMP_MARGIN_DELTA));
	SendDlgItemMessage(_hdlg, IDC_SPIN_RIGHT, UDM_SETRANGE, 0, MAKELONG(_maxMargin[MGN_RIGHT] / HEXDUMP_MARGIN_DELTA, _minMargin[MGN_RIGHT] / HEXDUMP_MARGIN_DELTA));
	SendDlgItemMessage(_hdlg, IDC_SPIN_BOTTOM, UDM_SETRANGE, 0, MAKELONG(_maxMargin[MGN_BOTTOM] / HEXDUMP_MARGIN_DELTA, _minMargin[MGN_BOTTOM] / HEXDUMP_MARGIN_DELTA));

	SetDlgItemText(_hdlg, IDC_EDIT_FONTNAME, DUMP_FONT_FACE);
	SendDlgItemMessage(_hdlg, IDC_SPIN_FONTSIZE, UDM_SETPOS, 0, _vi[RNG_LINES].FontSize);
	SendDlgItemMessage(_hdlg, IDC_SPIN_FONTSIZE, UDM_SETRANGE, 0, MAKELONG(HDPD_FONTSIZE_MAX, HDPD_FONTSIZE_MIN));

	// note that the range of lines to be printed in Line mode is in _bmv->_vi.RowsPerPage, not _vi[RNG_LINES].RowsPerPage and not setup.linesPerPage either.
	_line1 = _line2 = _vi[RNG_LINES].CurRow + 1;
	_line2 += _bmv->_vi.RowsPerPage;
	SetDlgItemInt(_hdlg, IDC_EDIT_LINENUM1, _line1, FALSE);
	SetDlgItemInt(_hdlg, IDC_EDIT_LINENUM2, _line2, FALSE);

	_page1 = _page2 = setup.curPage;
	SetDlgItemInt(_hdlg, edt1, _page1, FALSE);
	SetDlgItemInt(_hdlg, edt2, _page2, FALSE);

	initMarginControls(setup);

	if (_pd->Flags & PD_SELECTION)
	{
		_radSel = rad2;
		updatePageNum(RNG_LINES);
		ShowWindow(_hwndVS, SW_HIDE);
	}
	else
	{
		_radSel = rad3;
		updatePageNum(RNG_PAGES);
	}
	CheckRadioButton(_hdlg, rad1, rad3, _radSel);

	SendDlgItemMessage(_hdlg, IDC_EDIT_LINENUM1, EM_SETREADONLY, TRUE, FALSE);
	SendDlgItemMessage(_hdlg, IDC_EDIT_LINENUM2, EM_SETREADONLY, TRUE, FALSE);

	_flags &= ~HDPD_FLAG_INITIALIZING;
	return TRUE;
}

BOOL HexdumpPrintDlg::OnCommand(WPARAM wp_, LPARAM lp_)
{
	DBGPRINTF((L"HexdumpPrintDlg::OnCommand: wp=%p, lp=%p\n", wp_, lp_));
	if (_flags & HDPD_FLAG_INITIALIZING)
		return SimpleDlg::OnCommand(wp_, lp_);
	if (wp_ == rad1 || wp_ == rad2 || wp_ == rad3)
	{
		_radSel = LOWORD(wp_); // rad1=All, rad2=Pages, rad3=Lines
		CheckRadioButton(_hdlg, rad1, rad3, _radSel);
		if (_radSel == rad1)
		{
			// All
			ShowWindow(_hwndVS, SW_SHOW);
		}
		else if (_radSel == rad3)
		{
			// Pages
			ShowWindow(_hwndVS, SW_SHOW);
		}
		else if (_radSel == rad2)
		{
			// Lines
			ShowWindow(_hwndVS, SW_HIDE);
		}
		InvalidateRect(_hPreview, NULL, TRUE);
		_flags |= HDPD_FLAG_DELAY_PAGENUM_UPDATE;
		return TRUE;
	}
	if (wp_ == IDC_CHECK_GROUPNOTES || wp_ == IDC_CHECK_FITTOPAGEWIDTH)
	{
		if (wp_ == IDC_CHECK_GROUPNOTES)
		{
			_groupNotes = IsDlgButtonChecked(_hdlg, IDC_CHECK_GROUPNOTES);
		}
		else //if (wp_ == IDC_CHECK_FITTOPAGEWIDTH)
		{
			_fitToWidth = IsDlgButtonChecked(_hdlg, IDC_CHECK_FITTOPAGEWIDTH);
			EnableWindow(GetDlgItem(_hdlg, IDC_EDIT_FONTSIZE), !_fitToWidth);
			EnableWindow(GetDlgItem(_hdlg, IDC_SPIN_FONTSIZE), !_fitToWidth);
		}
		InvalidateRect(_hPreview, NULL, TRUE);
		_flags |= HDPD_FLAG_DELAY_PAGENUM_UPDATE;
		return TRUE;
	}
	if (wp_ == IDM_UPDATE_PAGENUM)
	{
		updatePageNum((RANGETYPE)lp_);
		return TRUE;
	}
	if (HIWORD(wp_) == EN_SETFOCUS)
	{
		_editSel = LOWORD(wp_);
		return TRUE;
	}
	if (HIWORD(wp_) == EN_CHANGE)
	{
		BOOL x;
		UINT v;
		if (LOWORD(wp_) == edt1 || LOWORD(wp_) == edt2)
		{
			// v holds a one-based page number
			v = GetDlgItemInt(_hdlg, LOWORD(wp_), &x, FALSE);
			if (x)
			{
				_editSel = LOWORD(wp_);
				if (LOWORD(wp_) == edt1)
				{
					_page1 = v;
					if (v > _page2)
					{
						_page2 = v; // ending page number must be equal to or greater than starting page number.
						_flags |= HDPD_FLAG_INITIALIZING;
						SetDlgItemInt(_hdlg, edt2, v, FALSE);
						_flags &= ~HDPD_FLAG_INITIALIZING;
					}
				}
				else
				{
					_page2 = v;
					if (v < _page1)
					{
						_page1 = v; // starting page number must be equal to or less than ending page number.
						_flags |= HDPD_FLAG_INITIALIZING;
						SetDlgItemInt(_hdlg, edt1, v, FALSE);
						_flags &= ~HDPD_FLAG_INITIALIZING;
					}
				}
				if (_radSel != rad2)
				{
					//HexdumpPageSetupParams setup(_pd);
					HexdumpPageSetupParams &setup = *_setup;
					setup.updateViewInfo(_vi + RNG_PAGES, _fitToWidth, _groupNotes, _logicalPage, _margin, _minMargin);
					v = (v - 1)*setup.linesPerPage;
					scrollPreview(RNG_PAGES, SB_THUMBPOSITION, v);
				}
			}
		}
		else if (LOWORD(wp_) == IDC_EDIT_LINENUM1 || LOWORD(wp_) == IDC_EDIT_LINENUM2)
		{
			v = GetDlgItemInt(_hdlg, LOWORD(wp_), &x, FALSE);
			if (x)
			{
				_editSel = LOWORD(wp_);
				if (LOWORD(wp_) == IDC_EDIT_LINENUM2)
				{
					_line2 = v;
					_vi[RNG_LINES].RowsTotal = v - 1;
				}
				else
					_line1 = v;
			}
		}
		else if (IDC_EDIT_MARGIN_TOP <= LOWORD(wp_) && LOWORD(wp_) <= IDC_EDIT_MARGIN_BOTTOM)
		{
			if (_flags & HDPD_FLAG_UPDATING_MARGIN)
				return TRUE;
			saveNewMargin((MARGINTYPE)(LOWORD(wp_) - IDC_EDIT_MARGIN_TOP));
		}
		return TRUE;
	}
	return SimpleDlg::OnCommand(wp_, lp_);
}

BOOL HexdumpPrintDlg::OnNotify(LPNMHDR pNmHdr, LRESULT *plRes)
{
	if (pNmHdr->code == UDN_DELTAPOS)
	{
		if (_flags & HDPD_FLAG_INITIALIZING)
		{
			*plRes = 1; // not ready.
			return TRUE;
		}
		LPNMUPDOWN pNmUpd = (LPNMUPDOWN)pNmHdr;
		int x;
		if (pNmHdr->idFrom == IDC_SPIN_FONTSIZE)
		{
			x = pNmUpd->iPos + pNmUpd->iDelta;
			if (!(HDPD_FONTSIZE_MIN <= x && x <= HDPD_FONTSIZE_MAX))
			{
				*plRes = 1; // will go out of the allowed range. don't allow the change.
				return TRUE;
			}
			_vi[RNG_LINES].FontSize = _vi[RNG_PAGES].FontSize = x;
			InvalidateRect(_hPreview, NULL, TRUE);
			_flags |= HDPD_FLAG_DELAY_PAGENUM_UPDATE;
			*plRes = 0;
			return TRUE;
		}
		if (_flags & HDPD_FLAG_UPDATING_MARGIN)
		{
			*plRes = 1; // don't allow the change.
			return TRUE;
		}
		MARGINTYPE m = (MARGINTYPE)(pNmHdr->idFrom - IDC_SPIN_TOP);
		ASSERT(MGN_TOP <= m && m <= MGN_BOTTOM);
		x = (pNmUpd->iPos + pNmUpd->iDelta)*HEXDUMP_MARGIN_DELTA;
		if (!(_minMargin[m] <= x && x <= _maxMargin[m]))
		{
			*plRes = 1; // will go out of the allowed range. don't allow the change.
			return TRUE;
		}
		double f = (double)x / 1000.0;
		_flags |= HDPD_FLAG_UPDATING_MARGIN;
		_setDlgItemDouble(IDC_EDIT_MARGIN_TOP + (UINT)m, f);
		_flags &= ~HDPD_FLAG_UPDATING_MARGIN;
		_margin[m] = x;
		InvalidateRect(_hPreview, NULL, TRUE);
		*plRes = 0;
		return TRUE;
	}
	return FALSE;
}

BOOL HexdumpPrintDlg::OnDrawItem(UINT idCtrl, LPDRAWITEMSTRUCT pDIS)
{
	if (idCtrl != IDC_STATIC_PAGE1)
		return FALSE;

	ustring headerText = _bmv->getFileTitle();

	RECT rcPreview;
	GetClientRect(_hPreview, &rcPreview);

	RANGETYPE rangeType = _radSel == rad2 ? RNG_LINES : RNG_PAGES;
	_logicalPage->update(_vi[rangeType].FontSize);

	HexdumpPageSetupParams &setup = *_setup;
	setup.dumpDevInfo();
	setup.updateViewInfo(_vi + rangeType, _fitToWidth, _groupNotes, _logicalPage, _margin, _minMargin);

	_vi[rangeType].ColWidth = setup.charPelSz.cx;
	_vi[rangeType].RowHeight = setup.charPelSz.cy;
	_vi[rangeType].RowsPerPage = setup.linesPerPage;
	int rowsRemaining = _vi[rangeType].RowsTotal - _vi[rangeType].CurRow;
	/* repaint() will reset RowsShown, and then, increment it as it loops through the rows being output. so, do not set RowsShown to anything here. it will be overwritten.
	if (rowsRemaining < setup.linesPerPage)
		_vi[rangeType].RowsShown = rowsRemaining;
	else
		_vi[rangeType].RowsShown = setup.linesPerPage;
		*/

	DATARANGEINFO dri = { _fri[rangeType].FileSize, _fri[rangeType].DataOffset };
	LARGE_INTEGER remainingBytes;
	remainingBytes.QuadPart = (LONGLONG)_fri[rangeType].FileSize.QuadPart - _fri[rangeType].DataOffset.QuadPart;

	int printRows;
	if (rangeType == RNG_LINES)
	{
		// lines
		printRows = _line2 - _line1;
		if (printRows <= 0)
			return TRUE; // invalid line number
		if (printRows > setup.linesPerPage)
			printRows = setup.linesPerPage;
		// fix the ending line number
		setup.endLineNum = setup.startLineNum + printRows;
	}
	else // pages
		printRows = setup.linesPerPage;
	dri.RangeBytes = _vi[rangeType].BytesPerRow*printRows;
	if (dri.RangeBytes > remainingBytes.LowPart)
		dri.RangeBytes = remainingBytes.LowPart;
	dri.RangeLines = (dri.RangeBytes + _vi[rangeType].BytesPerRow - 1) / _vi[rangeType].BytesPerRow;
	dri.GroupAnnots = _groupNotes;

	SIZE printSz = { rcPreview.right, rcPreview.bottom };
	RECT printMargin = { 0 };
	RECT printMinMargin = { 0 };
	double rx, ry;
	if (setup.rtMargin.left)
	{
		rx = (double)setup.rtMargin.left / (double)setup.ptPaperSize.x;
		ry = (double)setup.rtMargin.top / (double)setup.ptPaperSize.y;
		printMargin.left = (int)(rcPreview.right*rx);
		printMargin.top = (int)(rcPreview.bottom*ry);
		rx = (double)setup.rtMargin.right / (double)setup.ptPaperSize.x;
		ry = (double)setup.rtMargin.bottom / (double)setup.ptPaperSize.y;
		printMargin.right = (int)(rcPreview.right*rx);
		printMargin.bottom = (int)(rcPreview.bottom*ry);
		printSz.cx -= printMargin.left + printMargin.right;
		printSz.cy -= printMargin.top + printMargin.bottom;

		rx = (double)setup.rtMinMargin.left / (double)setup.ptPaperSize.x;
		ry = (double)setup.rtMinMargin.top / (double)setup.ptPaperSize.y;
		printMinMargin.left = (int)(rcPreview.right*rx);
		printMinMargin.top = (int)(rcPreview.bottom*ry);
		rx = (double)setup.rtMinMargin.right / (double)setup.ptPaperSize.x;
		ry = (double)setup.rtMinMargin.bottom / (double)setup.ptPaperSize.y;
		printMinMargin.right = (int)(rcPreview.right*rx);
		printMinMargin.bottom = (int)(rcPreview.bottom*ry);
	}
	SIZE printSz2 = printSz;

	DBGPRINTF((L"OnDrawItem: Font=%d (%dx%d); Page=%dx%d (%dx%d) showing %d lines; Preview=%dx%d\n", setup.fontSize, setup.charPelSz.cx, setup.charPelSz.cy, setup.colsPerPage, setup.linesPerPage, _vi[rangeType].ColsPerPage, _vi[rangeType].RowsPerPage, rowsRemaining, rcPreview.right, rcPreview.bottom));

	BinhexMetaPageView pageview(_bmv, _vi + rangeType, _fri + rangeType);
	pageview.init(_hPreview, &dri, setup.fontSize);
	HBITMAP hbm = pageview.paintToBitmap(dri.GroupAnnots ? BINHEXVIEW_REPAINT_NO_SELECTOR | BINHEXVIEW_REPAINT_ANNOTATION_ON_SIDE : BINHEXVIEW_REPAINT_NO_SELECTOR);

	setup.savePageLayout(pageview);

	ULONG fbFlags = 0;
	if (_fitToWidth)
	{
		fbFlags |= FBTWF_FLAG_RELY_ON_FRAME_WIDTH;
	}
	else
	{
		// if the grouping option is on (with the fit-to-width option off), and if the annotations column extends more than the dump area does, the bitmap will have to be cropped. 
		if (_groupNotes)
		{
			RECT rc2 = pageview._dumpRect;
			rc2.right = pageview._pageRect.right;
			if (_CompareRectsByHeight(pageview._annotsRect, pageview._dumpRect) > 0)
			{
				HBITMAP hbm2 = _CropBitmap(_hPreview, hbm, &rc2);
				DeleteObject(hbm);
				if (!hbm2)
					return TRUE;
				hbm = hbm2;
			}
			printSz2 = _logicalPage->fixupViewExtents(&pageview._vi, printSz, setup.printableSz, pageview._dumpRect, rc2);
			fbFlags |= FBTWF_FLAG_IGNORE_SOURCE_ASPECT;
		}
		else
		{
			printSz2 = _logicalPage->fixupViewExtents(&pageview._vi, printSz, setup.printableSz);
			fbFlags |= FBTWF_FLAG_IGNORE_SOURCE_ASPECT;
		}
	}

	HBITMAP hbmSized = _FitBitmapToWindowFrame(hbm, _hPreview, printSz2, NULL, fbFlags);
	DeleteObject(hbm);

	BITMAP bm;
	GetObject(hbmSized, sizeof(BITMAP), &bm);

	// erase entire background.
	HDC hdc = GetDC(_hPreview);
	HBRUSH hbrBkgd = GetSysColorBrush(COLOR_3DHIGHLIGHT);
	FillRect(hdc, &rcPreview, hbrBkgd);

	if (!_hfHeader)
		_hfHeader = CreateFont(-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, L"Arial");

	HDC hdc0 = CreateCompatibleDC(hdc);
	HBITMAP hbm0 = (HBITMAP)SelectObject(hdc0, hbmSized);

	BitBlt(hdc, printMargin.left, printMargin.top, printSz.cx, printSz.cy, hdc0, 0, 0, SRCCOPY);

	RECT rcHeader = { printMinMargin.left, printMinMargin.top, rcPreview.right - printMinMargin.right, printMargin.top };

	HFONT hf0 = (HFONT)SelectObject(hdc, _hfHeader);
	DrawText(hdc, headerText, headerText._length, &rcHeader, DT_CENTER);
	if (rangeType == RNG_PAGES)
	{
		ustring pn = getPageNum(rangeType);
		DrawText(hdc, pn, pn._length, &rcHeader, DT_RIGHT);
	}
	SelectObject(hdc, hf0);

	HPEN hpen = CreatePen(PS_DOT, 0, COLORREF_LTLTGRAY);
	HPEN hpen0 = (HPEN)SelectObject(hdc, hpen);
	MoveToEx(hdc, 2, printMargin.top, NULL);
	LineTo(hdc, rcPreview.right - 2, printMargin.top);
	MoveToEx(hdc, 2, rcPreview.bottom - printMargin.bottom, NULL);
	LineTo(hdc, rcPreview.right - 2, rcPreview.bottom - printMargin.bottom);
	MoveToEx(hdc, printMargin.left, 2, NULL);
	LineTo(hdc, printMargin.left, rcPreview.bottom - 2);
	MoveToEx(hdc, rcPreview.right - printMargin.right, 2, NULL);
	LineTo(hdc, rcPreview.right - printMargin.right, rcPreview.bottom - 2);
	SelectObject(hdc, hpen0);
	DeleteObject(hpen);

	SelectObject(hdc0, hbm0);
	DeleteObject(hbmSized);
	DeleteDC(hdc0);
	ReleaseDC(_hPreview, hdc);

	if (_flags & HDPD_FLAG_DELAY_PAGENUM_UPDATE)
	{
		_flags &= ~HDPD_FLAG_DELAY_PAGENUM_UPDATE;
		PostMessage(_hdlg, WM_COMMAND, IDM_UPDATE_PAGENUM, rangeType);
	}
	return TRUE;
}

BOOL HexdumpPrintDlg::OnVScroll(int scrollAction, int scrollPos, HWND hCtrl, LRESULT *pResult)
{
	// the up-down controls for the margin settings fire WM_VSCROLL too. so, screen them out. we want to handle preview events only.
	if (hCtrl != _hwndVS)
		return FALSE;
	RANGETYPE rt = _radSel == rad2 ? RNG_LINES : RNG_PAGES;
	BOOL ret = scrollPreview(rt, scrollAction, scrollPos);
	if (ret && _radSel == rad3)
	{
		int pageNum = _vi[rt].CurRow / _vi[rt].RowsPerPage + 1;
		if (_editSel == edt1 && pageNum != _page1)
		{
			_page1 = pageNum;
			_flags |= HDPD_FLAG_INITIALIZING;
			SetDlgItemInt(_hdlg, edt1, _page1, FALSE);
			_flags &= ~HDPD_FLAG_INITIALIZING;
		}
		else if (_editSel == edt2 && pageNum != _page2)
		{
			_page2 = pageNum;
			_flags |= HDPD_FLAG_INITIALIZING;
			SetDlgItemInt(_hdlg, edt2, _page2, FALSE);
			_flags &= ~HDPD_FLAG_INITIALIZING;
		}
	}
	return ret;
}

BOOL HexdumpPrintDlg::scrollPreview(RANGETYPE rt, int scrollAction, int scrollPos)
{
	SCROLLINFO si = { /*cbSize=*/sizeof(SCROLLINFO) };

	switch (scrollAction)
	{
	case SB_TOP:
		si.nPos = 0;
		break;
	case SB_LINEUP:
		si.nPos = (int)_vi[rt].CurRow - 1;
		if (si.nPos < 0)
			si.nPos = 0;
		break;
	case SB_LINEDOWN:
		si.nPos = (int)_vi[rt].CurRow + 1;
		if (si.nPos > (int)_vi[rt].RowsTotal)
			si.nPos = (int)_vi[rt].RowsTotal;
		break;
	case SB_PAGEUP:
		si.nPos = (int)_vi[rt].CurRow - (int)_vi[rt].RowsPerPage;
		if (si.nPos < 0)
			si.nPos = 0;
		break;
	case SB_PAGEDOWN:
		si.nPos = (int)_vi[rt].CurRow + (int)_vi[rt].RowsPerPage;
		if (si.nPos > (int)_vi[rt].RowsTotal)
			si.nPos = (int)_vi[rt].RowsTotal;
		break;
	case SB_BOTTOM:
		si.nPos = (int)_vi[rt].RowsTotal;
		break;
	case SB_THUMBPOSITION:
		si.nPos = scrollPos;
		break;
	case SB_THUMBTRACK:
		si.nPos = scrollPos;
		break;
	default:
		return FALSE;
	}
	if ((int)_vi[rt].CurRow == si.nPos)
		return FALSE;

	si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
	si.nPage = (int)_vi[rt].RowsPerPage;
	si.nMax = (int)_vi[rt].RowsTotal + 1;
	SetScrollInfo(_hwndVS, SB_CTL, &si, TRUE); // note it's SB_CTL, not SB_VERT. SB_VERT would not update the scroll bar control we are using in this project.
	_vi[rt].CurRow = (UINT)si.nPos;
	_fri[rt].HasData = false;
	_fri[rt].DataOffset.LowPart = (ULONG)_vi[rt].CurRow*_vi[rt].BytesPerRow;
	// repaint the preview page.
	InvalidateRect(_hPreview, NULL, TRUE);
	updatePageNum(rt);
	return TRUE;
}

int HexdumpPrintDlg::getCurrentScrollPos(HWND hwndScroller, UINT fMask)
{
	SCROLLINFO si = { sizeof(SCROLLINFO) };
	si.fMask = fMask;
	if (GetScrollInfo(hwndScroller, SB_CTL, &si))
	{
		if (fMask == SIF_TRACKPOS)
			return si.nTrackPos;
		return si.nPos;
	}
	return 0;
}

ustring HexdumpPrintDlg::getPageNum(RANGETYPE rt, bool prefix)
{
	int pageNum = _vi[rt].CurRow / _vi[rt].RowsPerPage + 1;
	int pageTot = _vi[rt].RowsTotal / _vi[rt].RowsPerPage + 1;
	ustring s;
	if (prefix)
	{
		if (_radSel == rad2)
		{
			// in Line mode, add a 'Page' prefix.
			s.format(L"Lines %d - %d\n(Page %d)", _line1, _line2, pageNum);
		}
		else
		{
			// in Page mode, add a 'Page' prefix. also show the page number followed by the total.
			s.format(L"Page %d / %d", pageNum, pageTot);
		}
	}
	else // print the page number only
		s.format(L"%d", pageNum);
	return s;
}

void HexdumpPrintDlg::updatePageNum(RANGETYPE rt)
{
	// don't show the total page count for RNG_LINES. OnInitDialog has set RowsTotal to CurRow+RowsPerPage for the Line mode. The total would always equal pageNum.
	SetDlgItemText(_hdlg, IDC_STATIC_PAGENUM1, getPageNum(rt, true));
}

void HexdumpPrintDlg::initMarginControls(HexdumpPageSetupParams &setup)
{
	ustring s;
	double x;
	_flags |= HDPD_FLAG_UPDATING_MARGIN;
	x = (double)setup.rtMargin.top / 1000.0;
	s.format(L"%.02g", x);
	SetDlgItemText(_hdlg, IDC_EDIT_MARGIN_TOP, s);
	x = (double)setup.rtMargin.left / 1000.0;
	s.format(L"%.02g", x);
	SetDlgItemText(_hdlg, IDC_EDIT_MARGIN_LEFT, s);
	x = (double)setup.rtMargin.right / 1000.0;
	s.format(L"%.02g", x);
	SetDlgItemText(_hdlg, IDC_EDIT_MARGIN_RIGHT, s);
	x = (double)setup.rtMargin.bottom / 1000.0;
	s.format(L"%.02g", x);
	SetDlgItemText(_hdlg, IDC_EDIT_MARGIN_BOTTOM, s);
	_flags &= ~HDPD_FLAG_UPDATING_MARGIN;
}

bool HexdumpPrintDlg::saveNewMargin(MARGINTYPE m)
{
	if (_flags & HDPD_FLAG_UPDATING_MARGIN)
		return false;
	bool ret = false;
	_flags |= HDPD_FLAG_UPDATING_MARGIN;
	double f;
	f = _getDlgItemDouble(IDC_EDIT_MARGIN_TOP + (UINT)m)*1000.0;
	if (f >= (double)_minMargin[m] && f < (double)_maxMargin[m])
	{
		_margin[m] = (int)f;
		InvalidateRect(_hPreview, NULL, TRUE);
		ret = true;
	}
	else
		_setDlgItemDouble(IDC_EDIT_MARGIN_TOP + (UINT)m, (double)_margin[m] / 1000.0);
	_flags &= ~HDPD_FLAG_UPDATING_MARGIN;
	return ret;
}

bool HexdumpPrintDlg::canClose()
{
	/* actually, PrintDlg itself makes a range check on the starting and ending page numbers. so, our second test on the page range is unnecessary.
	*/
	if (_radSel == rad2 && _line1 > _line2)
		return false;
	if (_radSel == rad3 && _page1 > _page2)
		return false;
	return true;
}
