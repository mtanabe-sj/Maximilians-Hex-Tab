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
#include <Windows.h>


//#define SIMPLEDLG_HANDLES_KEYBOARD


class SimpleDlg
{
public:
	SimpleDlg(int idd, HWND hwndParent = NULL)
		: _idd(idd), _hwndParent(hwndParent), _hdlg(NULL) {}
	virtual ~SimpleDlg() {}

	BOOL Create()
	{
		if (::CreateDialogParam(
			LibInstanceHandle,
			MAKEINTRESOURCE(_idd),
			_hwndParent,
			(DLGPROC)DlgProc,
			(LPARAM)this))
			return TRUE;
		return FALSE;
	}
	void Destroy()
	{
		if (_hdlg)
			::DestroyWindow(_hdlg);
	}
	INT_PTR DoModal()
	{
		return DialogBoxParam(
			LibInstanceHandle,
			MAKEINTRESOURCE(_idd),
			_hwndParent,
			DlgProc,
			(LPARAM)(LPVOID)this);
	}

protected:
	int _idd;
	HWND _hdlg, _hwndParent;

	virtual BOOL OnInitDialog() { return TRUE; }
	virtual void OnDestroy() { _hdlg = NULL; }
	virtual BOOL OnOK() { EndDialog(_hdlg, IDOK); return TRUE; }
	virtual BOOL OnCancel() { EndDialog(_hdlg, IDCANCEL); return TRUE; }
	virtual BOOL OnSysCommand(WPARAM wp_, LPARAM lp_) { return FALSE; }
	virtual BOOL OnSetCursor(HWND hwnd, UINT nHitTest, UINT message) { return FALSE; }
	virtual BOOL OnCommand(WPARAM wp_, LPARAM lp_)
	{
		if (LOWORD(wp_) == IDOK)
			return OnOK();
		else if (LOWORD(wp_) == IDCANCEL)
			return OnCancel();
		return FALSE;
	}
	virtual BOOL OnNotify(LPNMHDR pNmHdr, LRESULT *plRes) { return FALSE; }
	virtual void OnContextMenu(HWND hwnd, int x, int y) {}
	virtual void OnTimer(WPARAM nIDEvent) {}
	virtual void OnSize(UINT nType, int cx, int cy) {}
	virtual BOOL OnPaint() { return FALSE; }
	virtual BOOL OnHScroll(int scrollAction, int scrollPos, HWND hCtrl, LRESULT *pResult) { return FALSE; }
	virtual BOOL OnVScroll(int scrollAction, int scrollPos, HWND hCtrl, LRESULT *pResult) { return FALSE; }
	virtual BOOL OnLButtoneDown(UINT state, SHORT x, SHORT y) { return FALSE; }
	virtual BOOL OnLButtoneUp(UINT state, SHORT x, SHORT y) { return FALSE; }
	virtual BOOL OnMouseMove(UINT state, SHORT x, SHORT y) { return FALSE; }
	virtual BOOL OnMouseWheel(short nButtonType, short nWheelDelta, int x, int y) { return FALSE; }
#ifdef SIMPLEDLG_HANDLES_KEYBOARD
	virtual BOOL OnKeyDown(WPARAM nVKCode, USHORT nRepeats, USHORT nScanCode, BOOL fExtendedKey, BOOL fPrevKeyState) { return FALSE; }
#endif//#ifdef SIMPLEDLG_HANDLES_KEYBOARD
	virtual BOOL OnDrawItem(UINT idCtrl, LPDRAWITEMSTRUCT pDIS) { return FALSE; }
	virtual BOOL OnMeasureItem(UINT CtlID, LPMEASUREITEMSTRUCT pMIS) { return FALSE; }
	virtual BOOL OnActivate(USHORT activated, USHORT minimized, HWND hwnd) { return FALSE; }

protected:
	static INT_PTR CALLBACK DlgProc(HWND h_, UINT m_, WPARAM wp_, LPARAM lp_)
	{
		LRESULT lres = 0;
		SimpleDlg* pThis = (SimpleDlg*)GetWindowLongPtr(h_, DWLP_USER);
		if (m_ == WM_INITDIALOG)
		{
			pThis = (SimpleDlg*)lp_;
			pThis->_hdlg = h_;
			::SetWindowLongPtr(h_, DWLP_USER, (LONG_PTR)(LPVOID)pThis);
			return pThis->OnInitDialog();
		}
		if (!pThis)
			return FALSE;
		if (m_ == WM_DESTROY)
		{
			pThis->OnDestroy();
			SetWindowLongPtr(h_, DWLP_USER, NULL);
			return TRUE;
		}
		else if (m_ == WM_TIMER)
		{
			pThis->OnTimer(wp_);
			return TRUE;
		}
		else if (m_ == WM_SIZE)
		{
			pThis->OnSize((UINT)(ULONG_PTR)wp_, LOWORD(lp_), HIWORD(lp_));
			return TRUE;
		}
		else if (m_ == WM_SYSCOMMAND)
		{
			if (pThis->OnSysCommand(wp_, lp_))
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, TRUE);
				return TRUE;
			}
		}
		else if (m_ == WM_SETCURSOR)
		{
			//MS documentation: If the parent window returns TRUE, further processing is halted. 
			if (pThis->OnSetCursor((HWND)wp_, LOWORD(lp_), HIWORD(lp_)))
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, TRUE);
				return TRUE;
			}
		}
		else if (m_ == WM_COMMAND)
		{
			//return pThis->OnCommand( wp_, lp_ );
			if (pThis->OnCommand(wp_, lp_))
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, TRUE);
				return TRUE;
			}
		}
		else if (m_ == WM_NOTIFY)
		{
			if (pThis->OnNotify((LPNMHDR)lp_, &lres))
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, lres);
				return TRUE;
			}
		}
		else if (m_ == WM_CONTEXTMENU)
		{
			pThis->OnContextMenu((HWND)wp_, GET_X_LPARAM(lp_), GET_Y_LPARAM(lp_));
			return TRUE;
		}
		else if (m_ == WM_LBUTTONDOWN)
		{
			if (pThis->OnLButtoneDown((UINT)wp_, GET_X_LPARAM(lp_), GET_Y_LPARAM(lp_)))
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, 0);
				return TRUE;
			}
		}
		else if (m_ == WM_LBUTTONUP)
		{
			if (pThis->OnLButtoneUp((UINT)wp_, GET_X_LPARAM(lp_), GET_Y_LPARAM(lp_)))
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, 0);
				return TRUE;
			}
		}
		else if (m_ == WM_MOUSEMOVE)
		{
			if (pThis->OnMouseMove((UINT)wp_, GET_X_LPARAM(lp_), GET_Y_LPARAM(lp_)))
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, 0);
				return TRUE;
			}
		}
		else if (m_ == WM_MOUSEWHEEL)
		{
			if (pThis->OnMouseWheel(GET_KEYSTATE_WPARAM(wp_), GET_WHEEL_DELTA_WPARAM(wp_), GET_X_LPARAM(lp_), GET_Y_LPARAM(lp_)))
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, 0);
				return TRUE;
			}
		}
		else if (m_ == WM_PAINT)
		{
			if (pThis->OnPaint())
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, 0);
				return TRUE;
			}
		}
		else if (m_ == WM_MEASUREITEM)
		{
			if (pThis->OnMeasureItem((UINT)wp_, (LPMEASUREITEMSTRUCT)lp_))
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, TRUE);
				return TRUE;
			}
		}
		else if (m_ == WM_DRAWITEM)
		{
			if (pThis->OnDrawItem((UINT)wp_, (LPDRAWITEMSTRUCT)lp_))
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, TRUE);
				return TRUE;
			}
		}
#ifdef SIMPLEDLG_HANDLES_KEYBOARD
		else if (m_ == WM_GETDLGCODE)
		{
			::SetWindowLongPtr(h_, DWLP_MSGRESULT, DLGC_WANTALLKEYS);
			return TRUE;
		}
		else if (m_ == WM_KEYDOWN)
		{
			if (pThis->OnKeyDown(wp_,
				LOWORD(lp_),
				LOBYTE(HIWORD(lp_)),
				(lp_ >> 24) & 1,
				(lp_ >> 30) & 1))
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, 0);
				return TRUE;
			}
		}
#endif//#ifdef SIMPLEDLG_HANDLES_KEYBOARD
		else if (m_ == WM_HSCROLL)
		{
			if (pThis->OnHScroll(LOWORD(wp_), HIWORD(wp_), (HWND)lp_, &lres))
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, lres);
				return TRUE;
			}
		}
		else if (m_ == WM_VSCROLL)
		{
			if (pThis->OnVScroll(LOWORD(wp_), HIWORD(wp_), (HWND)lp_, &lres))
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, lres);
				return TRUE;
			}
		}
		else if (m_ == WM_ACTIVATE)
		{
			if (pThis->OnActivate(LOWORD(wp_), HIWORD(wp_), (HWND)lp_))
			{
				::SetWindowLongPtr(h_, DWLP_MSGRESULT, 0);
				return TRUE;
			}
		}
		return FALSE;
	}
};
