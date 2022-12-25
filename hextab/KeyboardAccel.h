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
#include "TLSData.h"


const char KAWindowClass[] = "#32770";
// make sure the title string exactly matches IDD_PROPPAGE's caption.
const char KAWindowTitle[] = "MTHexDumpTab";
const char KAPropName[] = "MTKeyboardAccel";

class KeyboardAccel
{
public:
	KeyboardAccel() = default;

	bool _enabled = false;

	bool open(HWND hwnd, UINT resId)
	{
		_hwnd = hwnd;
		TLSData *tls = TLSData::instance();
		_whook = queryMsgHook();
		if (!_whook)
		{
			_whook = SetWindowsHookEx(WH_GETMESSAGE, getmsgHookProc, NULL, GetCurrentThreadId());
			if (_whook)
			{
				PSXCONTROLPARAMS *cp = (PSXCONTROLPARAMS*)tls->allocData(sizeof(PSXCONTROLPARAMS));
				cp->MsgHook = _whook;
				tls->set(cp);
			}
		}
		if (_whook)
		{
			_haccel = LoadAccelerators(LibInstanceHandle, MAKEINTRESOURCEW(resId));
			if (_haccel)
			{
				tls->addref();
				SetPropA(hwnd, KAPropName, this);
				DBGPRINTF_VERBOSE((L"KeyboardAccel.open: hwnd=%X, hook=%X\n", hwnd, _whook));
				return true;
			}
		}
		return false;
	}
	void close()
	{
		if (!queryMsgHook(true))
			UnhookWindowsHookEx(_whook);
		RemovePropA(_hwnd, KAPropName);
		DBGPRINTF_VERBOSE((L"KeyboardAccel.close: hwnd=%X, hook=%X\n", _hwnd, _whook));
	}

protected:
	HHOOK _whook = NULL;
	HWND _hwnd = NULL;
	HACCEL _haccel = NULL;

	struct PSXCONTROLPARAMS : public TLSData::CONTROLPARAMS
	{
		HHOOK MsgHook;
	};

	static HHOOK queryMsgHook(bool detach = false)
	{
		TLSData *tls = TLSData::instance();
		PSXCONTROLPARAMS *cp = (PSXCONTROLPARAMS*)tls->get();
		if (!cp)
			return NULL;
		HHOOK h = cp->MsgHook;
		if (detach)
		{
			if (tls->release() == 0)
				h = NULL;
		}
		return h;
	}

	static LRESULT CALLBACK getmsgHookProc(int nCode, WPARAM wParam, LPARAM lParam)
	{
		LRESULT lres = 1;
		if (nCode == HC_ACTION && wParam == PM_REMOVE)
		{
			MSG *msg = (MSG*)lParam;
			if (msg->message >= WM_KEYFIRST && msg->message <= WM_KEYLAST)
			{
				if (translateMsg(msg))
				{
					msg->message = WM_NULL;
					lres = 0;
				}
			}
		}
		if (nCode < 0 || lres)
		{
			HHOOK h = queryMsgHook();
			return CallNextHookEx(h, nCode, wParam, lParam);
		}
		return lres;
	}

	static HRESULT testKAWindow(HWND hwnd)
	{
		char buf[256] = { 0 };
		GetClassNameA(hwnd, buf, ARRAYSIZE(buf));
		DBGPRINTF_VERBOSE((L" HWND=%X; WindowClass=%S\n", hwnd, buf));
		if (0 != strcmp(buf, KAWindowClass))
		{
			if (0 == _stricmp(buf, "EDIT"))
				return E_NOTIMPL;
			return E_FAIL;
		}
		GetWindowTextA(hwnd, buf, ARRAYSIZE(buf));
		DBGPRINTF_VERBOSE((L" WindowTitle='%S'\n", buf));
		if (0 != strcmp(buf, KAWindowTitle))
			return S_FALSE;
		return S_OK;
	}

	static bool translateMsg(MSG *msg)
	{
		DBGPRINTF_VERBOSE((L"KeyboardAccel.translateMsg: msg=%X, wp=%X, lp=%X\n", msg->message, msg->wParam, msg->lParam));
		HWND hwnd = msg->hwnd;
		HRESULT hr = testKAWindow(hwnd);
		if (hr != S_OK)
		{
			// don't handle if the source is an edit control.
			if (hr == E_NOTIMPL)
				return false;
			DBGPUTS_VERBOSE((L" FindParent:"));
			HWND hparent = GetParent(hwnd);
			if (!hparent)
				return false;
			hwnd = hparent;
			hr = testKAWindow(hwnd);
			if (hr != S_OK)
			{
				DBGPUTS_VERBOSE((L" FindChild:"));
				HWND hchild = GetWindow(hwnd, GW_CHILD);
				if (!hchild)
					return false;
				hwnd = hchild;
				while (S_OK != (hr = testKAWindow(hwnd)))
				{
					if (!(hchild = GetWindow(hwnd, GW_HWNDNEXT)))
						return false;
					hwnd = hchild;
				}
			}
		}
		KeyboardAccel* pThis = (KeyboardAccel*)GetPropA(hwnd, KAPropName);
		if (!pThis)
			return false;
		return pThis->translateAccel(msg);
	}

	virtual bool translateAccel(MSG *msg)
	{
		if (!_enabled)
			return false;
		return TranslateAcceleratorW(_hwnd, _haccel, msg);
	}
};
