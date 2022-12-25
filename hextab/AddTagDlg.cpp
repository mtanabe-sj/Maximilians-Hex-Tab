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
#include "AddTagDlg.h"
#include "BinhexMetaView.h"


// color tables defined in CRegionOperation.cpp
extern COLORREF s_regionInteriorColor[MAX_DUMPREGIONCOLORS];
extern COLORREF s_regionBorderColor[MAX_DUMPREGIONCOLORS];


AddTagDlg::AddTagDlg(BinhexMetaView *bhv) : SimpleDlg(IDD_ADD_TAG, bhv->_hw), _bhv(bhv), _srcOffset{0}, _srcLength(0), _textInGray(false)
{
}


BOOL AddTagDlg::OnInitDialog()
{
	SimpleDlg::OnInitDialog();

	// get a default color index. it's the remainder from a division of the annotation count by the maximum number of colors available.
	int iColor = _bhv->_regions.size() % MAX_DUMPREGIONCOLORS;
	_clrOutline = s_regionBorderColor[iColor];
	_clrInterior = s_regionInteriorColor[iColor];

	// calculate the color sample display rectangle.
	RECT rc;
	GetWindowRect(GetDlgItem(_hdlg, IDC_COLOR_SAMPLE), &rc);
	ScreenToClient(_hdlg, (LPPOINT)&rc.left);
	ScreenToClient(_hdlg, (LPPOINT)&rc.right);
	_rcSample.left = (rc.right + rc.left + _bhv->_vi.ColWidth) / 2;
	_rcSample.top = (rc.bottom + rc.top - _bhv->_vi.RowHeight) / 2 - 2;
	_rcSample.right = rc.right - _bhv->_vi.ColWidth;
	_rcSample.bottom = _rcSample.top + _bhv->_vi.RowHeight + 2;
	_ptSample.x = _rcSample.left + _bhv->_vi.ColWidth;
	_ptSample.y = _rcSample.top;

	// initialze the source data range (offset and length).
	_srcLength = _bhv->queryCurSel(&_srcOffset);
	ustring2 ofs(L"0x%I64X", _srcOffset.QuadPart);
	SetDlgItemText(_hdlg, IDC_EDIT_OFFSET, ofs);
	SetDlgItemInt(_hdlg, IDC_EDIT_LENGTH, _srcLength, FALSE);

	_hNote = GetDlgItem(_hdlg, IDC_EDIT_NOTE);

	LRESULT lres;
	WCHAR buf[16] = { 0 };
	int i;
	for (i = 0; i < MAX_DUMPREGIONCOLORS; i++)
	{
		wsprintf(buf, L"(%X)", s_regionBorderColor[i]);
		lres = SendDlgItemMessage(_hdlg, IDC_COMBO_OUTLINE_COLOR, CB_ADDSTRING, 0, (LPARAM)buf);
		if (lres == CB_ERR)
		{
			::EndDialog(_hdlg, IDABORT);
			return FALSE;
		}
		SendDlgItemMessage(_hdlg, IDC_COMBO_OUTLINE_COLOR, CB_SETITEMDATA, lres, (LPARAM)i);

		wsprintf(buf, L"(%X)", s_regionInteriorColor[i]);
		lres = SendDlgItemMessage(_hdlg, IDC_COMBO_INTERIOR_COLOR, CB_ADDSTRING, 0, (LPARAM)buf);
		if (lres == CB_ERR)
		{
			::EndDialog(_hdlg, IDABORT);
			return FALSE;
		}
		SendDlgItemMessage(_hdlg, IDC_COMBO_INTERIOR_COLOR, CB_SETITEMDATA, lres, (LPARAM)i);
	}
	// i==MAX_DUMPREGIONCOLORS for transparent interior
	lres = SendDlgItemMessage(_hdlg, IDC_COMBO_INTERIOR_COLOR, CB_ADDSTRING, 0, (LPARAM)L"(FFFFFFFF)");
	SendDlgItemMessage(_hdlg, IDC_COMBO_INTERIOR_COLOR, CB_SETITEMDATA, lres, (LPARAM)i);

	SendDlgItemMessage(_hdlg, IDC_COMBO_OUTLINE_COLOR, CB_SETITEMDATA, lres, (LPARAM)i);
	SendDlgItemMessage(_hdlg, IDC_COMBO_OUTLINE_COLOR, CB_SETCURSEL, iColor, 0);
	SendDlgItemMessage(_hdlg, IDC_COMBO_INTERIOR_COLOR, CB_SETCURSEL, iColor, 0);

	return TRUE;
}

BOOL AddTagDlg::OnCommand(WPARAM wp_, LPARAM lp_)
{
	if (LOWORD(wp_) == IDC_CHECK_GRAYTEXT)
	{
		_textInGray = IsDlgButtonChecked(_hdlg, IDC_CHECK_GRAYTEXT);
		InvalidateRect(_hdlg, &_rcSample, TRUE);
		return TRUE;
	}
	if (HIWORD(wp_) == EN_CHANGE)
	{
		int n;
		if (LOWORD(wp_) == IDC_EDIT_NOTE)
		{
			n = Edit_GetTextLength(_hNote);
			if (n)
				_note._length = Edit_GetText(_hNote, _note.alloc(n), n + 1);
			else
				_note.clear();
			return TRUE;
		}
		BOOL x;
		WCHAR buf[40];
		n = GetDlgItemText(_hdlg, LOWORD(wp_), buf, ARRAYSIZE(buf));
		if (LOWORD(wp_) == IDC_EDIT_OFFSET)
		{
			if (n)
				_srcOffset.QuadPart = _wcstoi64(buf, NULL, 0);
			else
				_srcOffset.QuadPart = 0;
		}
		else if (LOWORD(wp_) == IDC_EDIT_LENGTH)
		{
			if (n)
				_srcLength = wcstoul(buf, NULL, 0);
			else
				_srcLength = 0;
		}
		x = (0 <= _srcOffset.QuadPart && _srcOffset.QuadPart + _srcLength < (LONGLONG)_bhv->_fri.FileSize.QuadPart);
		EnableWindow(GetDlgItem(_hdlg, IDOK), x);
		return TRUE;
	}
	if (HIWORD(wp_) == CBN_SELCHANGE)
	{
		LRESULT lres = SendMessage((HWND)lp_, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
		if (lres != CB_ERR)
		{
			lres = SendMessage((HWND)lp_, (UINT)CB_GETITEMDATA, (WPARAM)lres, (LPARAM)0);
			if (LOWORD(wp_) == IDC_COMBO_INTERIOR_COLOR)
			{
				if (lres < MAX_DUMPREGIONCOLORS)
					_clrInterior = s_regionInteriorColor[lres];
				else
					_clrInterior = 0xFFFFFFFF;
			}
			else if (lres < MAX_DUMPREGIONCOLORS)
				_clrOutline = s_regionBorderColor[lres];
			InvalidateRect(_hdlg, &_rcSample, TRUE);
		}
		return TRUE;
	}
	return SimpleDlg::OnCommand(wp_, lp_);
}

/* https://learn.microsoft.com/en-us/windows/win32/controls/wm-measureitem

When the owner window receives the WM_MEASUREITEM message, the owner fills in the MEASUREITEMSTRUCT structure pointed to by the lParam parameter of the message and returns; this informs the system of the dimensions of the control. If a list box or combo box is created with the LBS_OWNERDRAWVARIABLE or CBS_OWNERDRAWVARIABLE style, this message is sent to the owner for each item in the control; otherwise, this message is sent once.

The system sends the WM_MEASUREITEM message to the owner window of combo boxes and list boxes created with the OWNERDRAWFIXED style before sending the WM_INITDIALOG message. As a result, when the owner receives this message, the system has not yet determined the height and width of the font used in the control; function calls and calculations requiring these values should occur in the main function of the application or library.
*/
BOOL AddTagDlg::OnMeasureItem(UINT idCtrl, LPMEASUREITEMSTRUCT pMIS)
{
	pMIS->itemHeight = _bhv->_vi.RowHeight+2;
	pMIS->itemWidth = _bhv->_vi.ColWidth * MAX_DUMPREGIONCOLORS;
	return TRUE;
}

BOOL AddTagDlg::OnDrawItem(UINT idCtrl, LPDRAWITEMSTRUCT pDIS)
{
	if (pDIS->itemID == -1)
		return TRUE;
	int iColor = (int)pDIS->itemData;

	RECT rc = pDIS->rcItem;
	HBRUSH hb1 = CreateSolidBrush(GetSysColor(pDIS->itemState & ODS_SELECTED ? COLOR_HIGHLIGHT: COLOR_WINDOW));
	HBRUSH hb0 = (HBRUSH)SelectObject(pDIS->hDC, hb1);
	FillRect(pDIS->hDC, &rc, hb1);

	HBRUSH hb2;
	COLORREF clr;
	if (iColor < MAX_DUMPREGIONCOLORS)
	{
		clr = idCtrl == IDC_COMBO_INTERIOR_COLOR ? s_regionInteriorColor[iColor] : s_regionBorderColor[iColor];
		hb2 = CreateSolidBrush(clr);
	}
	else
	{
		clr = COLORREF_DKGRAY;
		hb2 = GetStockBrush(NULL_BRUSH);
	}
	SelectObject(pDIS->hDC, hb2);
	HPEN hp = CreatePen(PS_SOLID, 0, clr);
	HPEN hp0 = (HPEN)SelectObject(pDIS->hDC, hp);
	Rectangle(pDIS->hDC, rc.left+4, rc.top+2, rc.right-4, rc.bottom-2);
	SelectObject(pDIS->hDC, hb0);
	SelectObject(pDIS->hDC, hp0);
	DeleteObject(hp);
	DeleteObject(hb2);
	DeleteObject(hb1);

	return TRUE;
}

BOOL AddTagDlg::OnPaint()
{
	ustring2 label(IDS_COLOR_SAMPLE_TEXT);

	PAINTSTRUCT	ps;
	BeginPaint(_hdlg, (LPPAINTSTRUCT)&ps);
	HFONT hf0 = (HFONT)SelectObject(ps.hdc, _bhv->_hfontDump);

	int mode0;
	COLORREF clrText0, clrBack0;
	clrText0 = SetTextColor(ps.hdc, _textInGray ? COLORREF_DKGRAY : COLORREF_BLACK);
	if (_clrInterior & 0xFF000000)
	{
		mode0 = SetBkMode(ps.hdc, TRANSPARENT);
		clrBack0 = COLORREF_WHITE;
	}
	else
	{
		mode0 = SetBkMode(ps.hdc, OPAQUE);
		clrBack0 = SetBkColor(ps.hdc, _clrInterior);
	}
	//SetTextAlign(ps.hdc, TA_CENTER);
	ExtTextOut(ps.hdc, _ptSample.x, _ptSample.y, ETO_CLIPPED | ETO_OPAQUE, &_rcSample, label, label._length, NULL);
	SetBkColor(ps.hdc, clrBack0);
	SetTextColor(ps.hdc, clrText0);

	HBRUSH hbr = CreateSolidBrush(_clrOutline);
	FrameRect(ps.hdc, &_rcSample, hbr);
	DeleteObject(hbr);

	SelectObject(ps.hdc, hf0);
	SetBkMode(ps.hdc, mode0);
	EndPaint(_hdlg, (LPPAINTSTRUCT)&ps);

	return TRUE;
}
