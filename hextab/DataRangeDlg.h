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
#include "BinhexMetaView.h"


class DataRangeDlg : public SimpleDlg
{
public:
	DataRangeDlg(BinhexMetaView *bmv, DATARANGEINFO *dri, HWND hwndParent) :
		SimpleDlg(IDD_DUMPRANGE, hwndParent), _bmv(bmv), _dri(dri), _hbm(NULL), _hbmSized(NULL), _ok(false), _ready(false), _saveDest(SAVEDEST_CLIPBOARD) {}
	~DataRangeDlg() { clean(); }

	enum SAVEDEST {
		SAVEDEST_CLIPBOARD,
		SAVEDEST_FILE
	};
	SAVEDEST _saveDest;

	HBITMAP detachViewBitmap()
	{
		HBITMAP h = _hbm;
		_hbm = NULL;
		return h;
	}

protected:
	BinhexMetaView *_bmv;
	DATARANGEINFO *_dri;
	HBITMAP _hbm, _hbmSized;
	bool _ok, _ready;

	const UINT_PTR TimerId = 1;
	const UINT TimerIntervalShort = 100;
	const UINT TimerIntervalLong = 2000;

	virtual BOOL OnInitDialog()
	{
		SimpleDlg::OnInitDialog();
		if (_dri->GroupAnnots)
			CheckDlgButton(_hdlg, IDC_CHECK_SEGREGATE_ANNOTS, BST_CHECKED);
		CheckRadioButton(_hdlg, IDC_RADIO_DATABYTES, IDC_RADIO_DATALINES, _dri->ByLineCount? IDC_RADIO_DATALINES: IDC_RADIO_DATABYTES);
		CheckRadioButton(_hdlg, IDC_RADIO_COPY, IDC_RADIO_SAVE, IDC_RADIO_COPY);
		SetDlgItemInt(_hdlg, IDC_EDIT_RANGESTART, _dri->StartOffset.LowPart, FALSE);
		SetDlgItemInt(_hdlg, IDC_EDIT_RANGEBYTES, _dri->RangeBytes, FALSE);
		SetDlgItemInt(_hdlg, IDC_EDIT_RANGELINES, _dri->RangeLines, FALSE);
		EnableWindow(GetDlgItem(_hdlg, IDC_EDIT_RANGESTART), FALSE);
		EnableWindow(GetDlgItem(_hdlg, _dri->ByLineCount ? IDC_EDIT_RANGEBYTES : IDC_EDIT_RANGELINES), FALSE);
		SetTimer(_hdlg, TimerId, TimerIntervalShort, NULL);
		_ready = true;
		return TRUE;
	}
	virtual void OnTimer(WPARAM nIDEvent)
	{
		KillTimer(_hdlg, nIDEvent);

		_ready = false;
		SetCursor(LoadCursor(NULL, IDC_WAIT));
		clean();
		HWND hctl = GetDlgItem(_hdlg, IDC_STATIC_PREVIEW);
		BinhexMetaPageView bmv(_bmv);
		bmv.init(hctl, _dri);
		_hbm = bmv.paintToBitmap(_dri->GroupAnnots? BINHEXVIEW_REPAINT_NO_SELECTOR |  BINHEXVIEW_REPAINT_ANNOTATION_ON_SIDE : BINHEXVIEW_REPAINT_NO_SELECTOR);
		_hbmSized = FitBitmapToWindowFrame(_hbm, hctl);
		/* https://docs.microsoft.com/en-us/windows/win32/controls/stm-setimage
		In version 6 of the Microsoft Win32 controls, a bitmap passed to a static control using the STM_SETIMAGE message was the same bitmap returned by a subsequent STM_SETIMAGE message. The client is responsible for deleting any bitmap sent to a static control.
		With Windows XP, if the bitmap passed in the STM_SETIMAGE message contains pixels with nonzero alpha, the static control takes a copy of the bitmap. This copied bitmap is returned by the next STM_SETIMAGE message. The client code may independently track the bitmaps passed to the static control, but if it does not check and release the bitmaps returned from STM_SETIMAGE messages, the bitmaps are leaked. */
		SendDlgItemMessage(_hdlg, IDC_STATIC_PREVIEW, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(LPVOID)_hbmSized);
		_ready = true;
	}
	virtual BOOL OnSetCursor(HWND hwnd, UINT nHitTest, UINT message)
	{
		if (_ready)
			return FALSE;
		SetCursor(LoadCursor(NULL, IDC_WAIT));
		return TRUE;
	}
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam)
	{
		if (!_ready)
			return SimpleDlg::OnCommand(wParam, lParam);

		if (HIWORD(wParam) == EN_CHANGE)
		{
			if (LOWORD(wParam) == IDC_EDIT_RANGEBYTES || LOWORD(wParam) == IDC_EDIT_RANGELINES)
			{
				BOOL xlated = FALSE;
				UINT val = GetDlgItemInt(_hdlg, LOWORD(wParam), &xlated, FALSE);
				if (xlated)
				{
					if (LOWORD(wParam) == IDC_EDIT_RANGEBYTES)
					{
						_dri->RangeBytes = val;
						_dri->RangeLines = (val + _bmv->_vi.BytesPerRow - 1) / _bmv->_vi.BytesPerRow;
						_dri->ByLineCount = false;
						_ready = false;
						SetDlgItemInt(_hdlg, IDC_EDIT_RANGELINES, _dri->RangeLines, FALSE);
						_ready = true;
					}
					else
					{
						_dri->RangeLines = val;
						_dri->RangeBytes = val * _bmv->_vi.BytesPerRow;
						_dri->ByLineCount = true;
						_ready = false;
						SetDlgItemInt(_hdlg, IDC_EDIT_RANGEBYTES, _dri->RangeBytes, FALSE);
						_ready = true;
					}
					SetTimer(_hdlg, TimerId, TimerIntervalLong, NULL);
				}
			}
		}
		else if (HIWORD(wParam) == BN_CLICKED)
		{
			if (LOWORD(wParam) == IDC_RADIO_DATALINES || LOWORD(wParam) == IDC_RADIO_DATABYTES)
			{
				if (IsDlgButtonChecked(_hdlg, IDC_RADIO_DATALINES))
				{
					_dri->ByLineCount = true;
					EnableWindow(GetDlgItem(_hdlg, IDC_EDIT_RANGEBYTES), FALSE);
					EnableWindow(GetDlgItem(_hdlg, IDC_EDIT_RANGELINES), TRUE);
				}
				else //if (IsDlgButtonChecked(_hdlg, IDC_RADIO_DATABYTES))
				{
					_dri->ByLineCount = false;
					EnableWindow(GetDlgItem(_hdlg, IDC_EDIT_RANGEBYTES), TRUE);
					EnableWindow(GetDlgItem(_hdlg, IDC_EDIT_RANGELINES), FALSE);
				}
			}
			else if (LOWORD(wParam) == IDC_CHECK_SEGREGATE_ANNOTS)
			{
				_dri->GroupAnnots = IsDlgButtonChecked(_hdlg, IDC_CHECK_SEGREGATE_ANNOTS) == BST_CHECKED ? true:false;
				SetTimer(_hdlg, TimerId, TimerIntervalShort, NULL);
			}
		}
		return SimpleDlg::OnCommand(wParam, lParam);
	}
	virtual BOOL OnOK()
	{
		if (IsDlgButtonChecked(_hdlg, IDC_RADIO_SAVE))
			_saveDest = SAVEDEST_FILE;
		_ok = true;
		return SimpleDlg::OnOK();
	}

	void clean()
	{
		if (_hbmSized)
		{
			DeleteObject(_hbmSized);
			_hbmSized = NULL;
		}
		if (_hbm)
		{
			DeleteObject(_hbm);
			_hbm = NULL;
		}
	}
};
