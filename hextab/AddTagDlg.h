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
#include "SimpleDlg.h"


class AddTagDlg : public SimpleDlg
{
public:
	AddTagDlg(class BinhexMetaView *bhv);
	~AddTagDlg() {}

	LARGE_INTEGER _srcOffset;
	ULONG _srcLength;
	ustring _note;
	COLORREF _clrOutline, _clrInterior;
	bool _textInGray;

protected:
	BinhexMetaView *_bhv;
	RECT _rcSample;
	POINT _ptSample;
	HWND _hNote;

	virtual BOOL OnInitDialog();
	virtual BOOL OnCommand(WPARAM wp_, LPARAM lp_);
	virtual BOOL OnMeasureItem(UINT idCtrl, LPMEASUREITEMSTRUCT pMIS);
	virtual BOOL OnDrawItem(UINT idCtrl, LPDRAWITEMSTRUCT pDIS);
	virtual BOOL OnPaint();
};

