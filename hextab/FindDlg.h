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
#include "PropsheetExtImpl.h"
#include "resource.h"

#define _FINDCONFIG BinhexDumpWnd::FINDCONFIG


class FindDlg : public SimpleDlg
{
public:
	FindDlg(_FINDCONFIG* pFindConfig, HWND hwndParent) :
		SimpleDlg(IDD_FINDDATA, hwndParent),
		_fc(pFindConfig)
	{}

protected:
	_FINDCONFIG *_fc;
	RECT _frameRect;
	SIZE _frameSize;
	HWND _hwndData, _hwndFrame;
	ustring _searchToken;

	virtual BOOL OnInitDialog()
	{
		SimpleDlg::OnInitDialog();
		_hwndData = GetDlgItem(_hdlg, IDC_SEARCHSTRING);
		_hwndFrame = GetDlgItem(_hdlg, IDC_HEXIMAGE_FRAME);
		GetWindowRect(_hwndFrame, &_frameRect);
		ScreenToClient(_hdlg, (LPPOINT)&_frameRect.left);
		ScreenToClient(_hdlg, (LPPOINT)&_frameRect.right);
		InflateRect(&_frameRect, -1, -1);
		_frameRect.left += 3;
		_frameRect.top += 7;
		_frameSize.cx = _frameRect.right - _frameRect.left;
		_frameSize.cy = _frameRect.bottom - _frameRect.top;
		UINT idc;
		if (_fc->Flags & FINDCONFIGFLAG_ENCODING_UTF16)
			idc = IDC_RADIO_UTF16;
		else if (_fc->Flags & FINDCONFIGFLAG_ENCODING_RAW)
			idc = IDC_RADIO_HEX;
		else
			idc = IDC_RADIO_UTF8;
		CheckRadioButton(_hdlg, IDC_RADIO_UTF8, IDC_RADIO_HEX, idc);
		if (_fc->Flags & FINDCONFIGFLAG_DIR_BACKWARD)
			idc = IDC_RADIO_BACKWARD;
		else
			idc = IDC_RADIO_FORWARD;
		CheckRadioButton(_hdlg, IDC_RADIO_FORWARD, IDC_RADIO_BACKWARD, idc);
		_searchToken.assign(_fc->DisplayString);
		SetWindowText(_hwndData, (LPCWSTR)_searchToken);
		return TRUE;
	}
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam)
	{
		if (HIWORD(wParam) == EN_CHANGE)
		{
			int cc = GetWindowTextLength(_hwndData) + 1;
			_searchToken.alloc(cc);
			_searchToken._length = GetWindowText(_hwndData, _searchToken.alloc(cc), cc);
			updateSearchConfig();
			InvalidateRect(_hdlg, &_frameRect, TRUE);
		}
		else if (HIWORD(wParam) == BN_CLICKED)
		{
			updateSearchConfig();
			InvalidateRect(_hdlg, &_frameRect, TRUE);
		}
		return SimpleDlg::OnCommand(wParam, lParam);
	}
	virtual BOOL OnOK()
	{
		if (_searchToken.empty())
			return FALSE;
		updateSearchConfig();
		_fc->DisplayString.assign(_searchToken);
		return SimpleDlg::OnOK();
	}

	void updateSearchConfig()
	{
		_fc->Flags &= ~(FINDCONFIGFLAG_ENCODING_MASK | FINDCONFIGFLAG_DIR_MASK);
		if (IsDlgButtonChecked(_hdlg, IDC_RADIO_HEX))
			_fc->Flags |= FINDCONFIGFLAG_ENCODING_RAW;
		else if (IsDlgButtonChecked(_hdlg, IDC_RADIO_UTF16))
			_fc->Flags |= FINDCONFIGFLAG_ENCODING_UTF16;
		if (IsDlgButtonChecked(_hdlg, IDC_RADIO_BACKWARD))
			_fc->Flags |= FINDCONFIGFLAG_DIR_BACKWARD;

		int len;
		if (_fc->Flags & FINDCONFIGFLAG_ENCODING_UTF16)
		{
			len = _searchToken._length * sizeof(WCHAR);
			CopyMemory(_fc->SearchString.alloc(len), _searchToken._buffer, len);
			_fc->SearchString._length = len;
		}
		else if (_fc->Flags & FINDCONFIGFLAG_ENCODING_RAW)
		{
			strvalenum sve(_searchToken.contains(L",")!=-1? ',':' ', _searchToken);
			len = sve.getCount();
			LPBYTE dest = (LPBYTE)_fc->SearchString.alloc(len);
			size_t i = 0;
			LPVOID pos = sve.getHeadPosition();
			while (pos)
			{
				LPCWSTR next = sve.getNext(pos);
				dest[i] = LOBYTE(wcstol(next, NULL, 16));
				i++;
			}
			_fc->SearchString._length = len;
		}
		else // UTF-8
			_fc->SearchString.assignCp(CP_UTF8, _searchToken);
	}

	virtual BOOL OnPaint() {
		RECT rcDraw;

		PAINTSTRUCT	ps;
		BeginPaint(_hdlg, (LPPAINTSTRUCT)&ps);

		LOGFONT lf = { 0 };
		lf.lfHeight = -MulDiv(DEFAULT_FONT_SIZE, GetDeviceCaps(ps.hdc, LOGPIXELSY), 72);
		lf.lfPitchAndFamily = FIXED_PITCH;
		wcscpy_s(lf.lfFaceName, ARRAYSIZE(lf.lfFaceName), SEARCH_FONT_FACE);
		HFONT hfont = CreateFontIndirect(&lf);
		SelectObject(ps.hdc, hfont);

		SetViewportOrgEx(ps.hdc, _frameRect.left, _frameRect.top, NULL);
		SetViewportExtEx(ps.hdc, _frameSize.cx, _frameSize.cy, NULL);

		TEXTMETRIC tm;
		GetTextMetrics(ps.hdc, &tm);
		int ColWidth = tm.tmAveCharWidth;
		int RowHeight = -lf.lfHeight + tm.tmInternalLeading;
		int ColsPerPage = _frameSize.cx / ColWidth;
		int RowsPerPage = _frameSize.cy / RowHeight;
		int BytesPerRow = 16;

		int i, j, ix, iy;
		int cx, cy, cxLast, cyLast;
		char hexbuf[10];
		ULONG bufSize = 3 * BytesPerRow + 1;
		LPSTR linebuf = (LPSTR)malloc(bufSize);

		cy = _fc->SearchString._length / BytesPerRow;
		cyLast = cy;
		cxLast = _fc->SearchString._length % BytesPerRow;
		if (cxLast)
			cy++;
		if (RowsPerPage && RowsPerPage < cyLast) {
			cy = RowsPerPage;
			cxLast = BytesPerRow;
		}

		i = 0;
		for (iy = 0; iy < cy; iy++) {
			ZeroMemory(linebuf, bufSize);
			cx = BytesPerRow;
			if (iy == cyLast)
				cx = cxLast;
			j = i;
			for (ix = 0; ix < cx; ix++) {
				sprintf_s(hexbuf, sizeof(hexbuf), "%02X ", MAKEWORD((BYTE)_fc->SearchString._buffer[i++],0));
				strcat_s(linebuf, bufSize, hexbuf);
			}

			rcDraw.left = 0;
			rcDraw.top = iy * RowHeight;
			rcDraw.right = _frameSize.cx;
			rcDraw.bottom = rcDraw.top + RowHeight;
			if (rcDraw.bottom > _frameSize.cy)
				rcDraw.bottom = _frameSize.cy;

			DrawTextA(ps.hdc, linebuf, (int)strlen(linebuf), &rcDraw, 0);
		}
		free(linebuf);

		DeleteObject(hfont);

		EndPaint(_hdlg, (LPPAINTSTRUCT)&ps);
		return TRUE;
	}
};

