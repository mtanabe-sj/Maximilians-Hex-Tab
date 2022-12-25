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
#include "Resource.h"


#define _CHILDWND_CLASS_CBWNDEXTRA		0
#define _CHILDWND_CLASS_ICON			0
#define _CHILDWND_CLASS_ICONSM			0
#define _CHILDWND_CLASS_MENU			NULL
#define _CHILDWND_CLASS_STYLE			(CS_HREDRAW|CS_VREDRAW) //|CS_DBLCLKS
#define _CHILDWND_CLASS_BACKGROUND		COLOR_WINDOW //COLOR_APPWORKSPACE

class _Window
{
public:
	_Window() : _hwnd(NULL), _pos{ 0 }, _size{ 0 } {}
	~_Window() {}

	SIZE _size;
	POINT _pos;

	operator HWND() const { return _hwnd; }
	BOOL PostMessage(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) { return ::PostMessage(_hwnd, message, wParam, lParam); }
	LRESULT SendMessage(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) { return ::SendMessage(_hwnd, message, wParam, lParam); }

	BOOL DestroyWindow() { BOOL bDestroyed = (_hwnd && ::DestroyWindow(_hwnd)); _hwnd = NULL; return bDestroyed; }
	void ScreenToClient(LPRECT lpRect) { ::ScreenToClient(_hwnd, (LPPOINT)&(lpRect->left)); ::ScreenToClient(_hwnd, (LPPOINT)&(lpRect->right)); }
	int MessageBox(LPCTSTR lpszText, LPCTSTR lpszCaption = NULL, UINT nType = MB_OK) { return ::MessageBox(_hwnd, lpszText, lpszCaption, nType); }
	void Invalidate(BOOL bErase = TRUE) { ::InvalidateRect(_hwnd, NULL, bErase); }

	SIZE GetExtents()
	{
		RECT rc;
		GetClientRect(_hwnd, &rc);
		return { rc.right, rc.bottom };
	}

protected:
	HWND _hwnd;
};

class _ChildWindow : public _Window
{
public:
	_ChildWindow(LPCWSTR classname) : _hparent(NULL), _classname(classname), _regError(0)
	{
	}
	virtual ~_ChildWindow() {
	}

	virtual BOOL Create(HWND hparent, int childId, int style, int exstyle, int x, int y, int cx, int cy, bool &isRegistered) {
		if (!Register(isRegistered))
			return FALSE;
		if (!::CreateWindowEx(
			exstyle,
			_classname,
			NULL,
			style | WS_CHILD,
			x, y,
			cx, cy,
			hparent,
			(HMENU)(UINT_PTR)childId,
			AppInstanceHandle,
			this))
			return FALSE;
		_hparent = hparent;
		return TRUE;
	}
	virtual void Destroy() {
		DestroyWindow();
		ASSERT(_hwnd == NULL);
	}

protected:
	HWND _hparent;
	LPCWSTR _classname;
	DWORD _regError;

	virtual BOOL OnCreate() { return TRUE; }
	virtual void OnDestroy() {
		_hwnd = NULL;
	}
	virtual void OnSetFocus() {}
	virtual void OnKillFocus() {}
	virtual BOOL OnSetCursor(HWND hwnd, UINT nHitTest, UINT message) { return FALSE; }
	virtual void OnSize(UINT_PTR nType, int cx, int cy) { _size.cx = cx, _size.cy = cy; }
	virtual void OnMove(int x, int y) { _pos.x = x, _pos.y = y; }
	virtual BOOL OnPaint() { return FALSE; } // return TRUE to indicate 'painted'
	virtual BOOL OnEraseBkgnd(HDC hdc) { return FALSE; }
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam) { return FALSE; }
	virtual BOOL OnNotify(LPNMHDR pNmHdr, LRESULT& lResult) { return FALSE; }
	virtual BOOL OnTimer(UINT_PTR nTimerId) { return FALSE; }
	virtual void OnContextMenu(HWND hwnd, int x, int y) {}
	virtual BOOL OnKeyDown(WPARAM nVKCode, USHORT nRepeats, USHORT nScanCode, BOOL fExtendedKey, BOOL fPrevKeyState) { return FALSE; }
	virtual BOOL OnLButtonDown(INT_PTR nButtonType, int x, int y) { return FALSE; }
	virtual BOOL OnLButtonUp(INT_PTR nButtonType, int x, int y) { return FALSE; }
	virtual BOOL OnLButtonDblClk(INT_PTR nButtonType, int x, int y) { return FALSE; }
	virtual BOOL OnMouseMove(UINT state, SHORT x, SHORT y) { return FALSE; }
#if _WIN32_WINNT >= 0x0400
	virtual BOOL OnMouseWheel(short nButtonType, short nWheelDelta, int x, int y) { return FALSE; }
#endif//#if _WIN32_WINNT >= 0x0400
	virtual BOOL OnHScroll(int scrollAction, int scrollPos, LRESULT *pResult) { return FALSE; }
	virtual BOOL OnVScroll(int scrollAction, int scrollPos, LRESULT *pResult) { return FALSE; }
	virtual BOOL OnMeasureItem(UINT CtlID, LPMEASUREITEMSTRUCT pMIS, LRESULT *pResult) { return FALSE; }
	virtual BOOL OnDrawItem(UINT idCtrl, LPDRAWITEMSTRUCT pDIS, LRESULT *pResult) { return FALSE; }
	virtual BOOL OnAppMessage(UINT msg, WPARAM wParam, LPARAM lParam) { return FALSE; }

private:
	BOOL Register(bool &isRegistered) {
		if (isRegistered)
			return TRUE;
		isRegistered = true;
		WNDCLASSEX wcex = { 0 };
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = _CHILDWND_CLASS_STYLE;
		wcex.lpfnWndProc = WndProc;
		wcex.lpszClassName = _classname;
		wcex.hInstance = AppInstanceHandle;
		wcex.cbWndExtra = _CHILDWND_CLASS_CBWNDEXTRA;
		wcex.hbrBackground = (HBRUSH)(_CHILDWND_CLASS_BACKGROUND + 1);
		wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
		if (!::RegisterClassEx(&wcex))
		{
			// the api can fail with a class-already-exists error. in that case, we can ignore the error and return a success code.
			_regError = GetLastError();
			if (_regError != ERROR_CLASS_ALREADY_EXISTS) // 1410
				return FALSE;
		}
		return TRUE;
	}

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		if (msg == WM_CREATE) {
			_ChildWindow* pThis = (_ChildWindow*)((LPCREATESTRUCT)lParam)->lpCreateParams;
			ASSERT(!IsBadReadPtr(pThis, sizeof(_ChildWindow)));
			::SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)(LPVOID)pThis);
			pThis->_hwnd = hwnd;
			if (!pThis->OnCreate())
				return -1;
			return 0;
		}
		LRESULT lRes = 0;
		_ChildWindow* pThis = (_ChildWindow*)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (pThis) {
			switch (msg) {
			case WM_SETFOCUS:
				pThis->OnSetFocus();
				break;
			case WM_KILLFOCUS:
				pThis->OnKillFocus();
				break;
			case WM_SIZE:
				pThis->OnSize(wParam, LOWORD(lParam), HIWORD(lParam));
				break;
			case WM_MOVE:
				pThis->OnMove(LOWORD(lParam), HIWORD(lParam));
				break;
			case WM_SETCURSOR:
				//MS documentation: If the parent window returns TRUE, further processing is halted. 
				if (pThis->OnSetCursor((HWND)wParam, LOWORD(lParam), HIWORD(lParam)))
					return TRUE;
				break;
			case WM_PAINT:
				if (pThis->OnPaint())
					return 0;
				break;
			case WM_ERASEBKGND:
				if (pThis->OnEraseBkgnd((HDC)wParam))
					return 1;
				break;
			case WM_DESTROY: // DestroyWindow got called
				pThis->OnDestroy();
				::SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
				break;
			case WM_NOTIFY:
				if (pThis->OnNotify((LPNMHDR)lParam, lRes))
					return lRes;
				break;
			case WM_CONTEXTMENU:
				pThis->OnContextMenu((HWND)wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				return 0;
			case WM_COMMAND:
				if (pThis->OnCommand(wParam, lParam))
					return 0;
				break;
			case WM_TIMER:
				if (lParam == 0) {
					if (pThis->OnTimer(wParam))
						return 0;
				}
				break;
			case WM_KEYDOWN:
				if (pThis->OnKeyDown(wParam,
					LOWORD(lParam),
					LOBYTE(HIWORD(lParam)),
					(lParam >> 24) & 1,
					(lParam >> 30) & 1))
					return 0;
				break;
			case WM_LBUTTONDOWN:
				if (pThis->OnLButtonDown(wParam, LOWORD(lParam), HIWORD(lParam)))
					return lRes;
				break;
			case WM_LBUTTONUP:
				if (pThis->OnLButtonUp(wParam, LOWORD(lParam), HIWORD(lParam)))
					return lRes;
				break;
			case WM_LBUTTONDBLCLK:
				if (pThis->OnLButtonDblClk(wParam, LOWORD(lParam), HIWORD(lParam)))
					return lRes;
				break;
			case WM_MOUSEMOVE:
				if (pThis->OnMouseMove((UINT)(UINT_PTR)wParam, LOWORD(lParam), HIWORD(lParam)))
					return lRes;
				break;
#if _WIN32_WINNT >= 0x0400
			case WM_MOUSEWHEEL:
				if (pThis->OnMouseWheel(LOWORD(wParam), HIWORD(wParam), LOWORD(lParam), HIWORD(lParam)))
					return lRes;
				break;
#endif//#if _WIN32_WINNT >= 0x0400
			case WM_HSCROLL:
				if (pThis->OnHScroll(LOWORD(wParam), HIWORD(wParam), &lRes))
					return lRes;
				break;
			case WM_VSCROLL:
				if (pThis->OnVScroll(LOWORD(wParam), HIWORD(wParam), &lRes))
					return lRes;
				break;
			case WM_MEASUREITEM:
				if (pThis->OnMeasureItem((UINT)wParam, (LPMEASUREITEMSTRUCT)lParam, &lRes))
					return lRes;
				break;
			case WM_DRAWITEM:
				if (pThis->OnDrawItem((UINT)wParam, (LPDRAWITEMSTRUCT)lParam, &lRes))
					return lRes;
				break;
			default:
				if (WM_APP <= msg && msg < 0xBFFF)
				{
					if (pThis->OnAppMessage(msg, wParam, lParam))
						return 0;
				}
				break;
			}
		}
		return ::DefWindowProc(hwnd, msg, wParam, lParam);
	}
};

