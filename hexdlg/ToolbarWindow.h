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
#include "ChildWnd.h"
#include "..\hextab\resource.h"
#include "hextabconfig.h"


#define IDM_MIN IDM_FINDDATA
#define IDM_MAX (IDM_GOTO_REGION0+264)

#define IDC_HDVTOOLBARWND 201

#define TBWND_MARGIN_X 4
#define TBWND_MARGIN_Y 1

#define TBWND_BUTTON_WIDTH 54
#define TBWND_BUTTON_HEIGHT 26

#define TBWND_IMAGELIST_ID 0
#define TBWND_NUM_BUTTONS 8


class ToolbarWindow : public _ChildWindow
{
public:
	ToolbarWindow() : _ChildWindow(L"hdvtoolbarwnd"), _hwndTB(NULL), _hbmAdd(NULL), _hbmScan(NULL), _bhc(NULL)
	{
	}
	~ToolbarWindow()
	{
		if (_bhc)
			_bhc->Release();
	}
	static bool _IsRegistered;
	IHexTabConfig *_bhc;

	BOOL Create(HWND hwndParent, IShellPropSheetExt *spsx)
	{
		spsx->QueryInterface(IID_IHexTabConfig, (LPVOID*)&_bhc);
		RECT rcParent;
		GetClientRect(hwndParent, &rcParent);
		SIZE szTB = { TBWND_BUTTON_WIDTH * TBWND_NUM_BUTTONS, TBWND_BUTTON_HEIGHT };
		return _ChildWindow::Create(hwndParent, IDC_HDVTOOLBARWND, WS_VISIBLE | WS_CHILD | WS_BORDER, 0, rcParent.right - szTB.cx - TBWND_MARGIN_X, TBWND_MARGIN_Y, szTB.cx, szTB.cy, _IsRegistered);
	}

protected:
	HWND _hwndTB;
	HBITMAP _hbmAdd, _hbmScan;

	virtual BOOL OnCreate()
	{
		if (!_ChildWindow::OnCreate())
			return FALSE;

		_hwndTB = ::CreateWindowEx(0, TOOLBARCLASSNAME, NULL, WS_CHILD | TBSTYLE_LIST, 0, 0, 0, 0, _hwnd, NULL, AppInstanceHandle, NULL);
		if (!_hwndTB)
			return FALSE;
		::SendMessage(_hwndTB, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);
		
		TBADDBITMAP tbadHist = { HINST_COMMCTRL,IDB_HIST_SMALL_COLOR };
		int indxHist = (int)::SendMessage(_hwndTB, TB_ADDBITMAP, 0, (LPARAM)&tbadHist);
		TBADDBITMAP tbadStd = { HINST_COMMCTRL,IDB_STD_SMALL_COLOR };
		int indxStd = (int)::SendMessage(_hwndTB, TB_ADDBITMAP, 0, (LPARAM)&tbadStd);

		COLORMAP cmap;
		cmap.from = RGB(0, 0, 0);
		cmap.to = GetSysColor(COLOR_BTNFACE);
		_hbmAdd = CreateMappedBitmap(AppInstanceHandle, IDB_TOOLBAR_BUTTON_ADD, 0, &cmap, 1);
		TBADDBITMAP tbadAdd = { NULL, (UINT_PTR)_hbmAdd };
		int indxCustom = (int)::SendMessage(_hwndTB, TB_ADDBITMAP, 0, (LPARAM)&tbadAdd);
		_hbmScan = CreateMappedBitmap(AppInstanceHandle, IDB_TOOLBAR_BUTTON_SCAN, 0, &cmap, 1);
		TBADDBITMAP tbadScan = { NULL, (UINT_PTR)_hbmScan };
		int indxCustom2 = (int)::SendMessage(_hwndTB, TB_ADDBITMAP, 0, (LPARAM)&tbadScan);
		ASSERT(indxCustom == indxCustom2);

		TBBUTTON tbButtons[TBWND_NUM_BUTTONS] =
		{
			{ indxCustom+1, IDM_SCANTAG_START, 0, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Scan" },
			{ indxHist + HIST_VIEWTREE, IDM_LIST_SCANNED_TAGS, 0, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Tags" },
			{ indxHist + HIST_BACK, IDM_FINDPREV_METAITEM, 0, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Prior" },
			{ indxHist + HIST_FORWARD, IDM_FINDNEXT_METAITEM, 0, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Next" },
			{ indxStd + STD_FIND, IDM_FINDDATA, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Find"},
			{ indxStd + STD_PRINT, IDM_PRINTDATA, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Print" },
			{ indxStd + STD_FILESAVE, IDM_SAVEDATA, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Save"},
			{ indxCustom+0, IDM_ADD, TBSTATE_ENABLED, BTNS_WHOLEDROPDOWN, {0}, 0, (INT_PTR)L"Add"},
		};
		::SendMessage(_hwndTB, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
		::SendMessage(_hwndTB, TB_ADDBUTTONS, (WPARAM)ARRAYSIZE(tbButtons), (LPARAM)&tbButtons);
		//::SendMessage(_hwndTB, TB_AUTOSIZE, 0, 0);
		::ShowWindow(_hwndTB, TRUE);

		SetTimer(_hwnd, 1, 1000, NULL);
		return TRUE;
	}
	virtual void OnDestroy()
	{
		if (_hbmScan)
			::DeleteObject(_hbmScan);
		if (_hbmAdd)
			::DeleteObject(_hbmAdd);
		_ChildWindow::OnDestroy();
	}
	virtual BOOL OnTimer(UINT_PTR nTimerId)
	{
		if (_bhc)
		{
			UINT buttons[] =
			{
				IDM_SCANTAG_START,
				IDM_LIST_SCANNED_TAGS,
				IDM_FINDPREV_METAITEM,
				IDM_FINDNEXT_METAITEM,
				IDM_FINDDATA,
				IDM_PRINTDATA,
				IDM_SAVEDATA,
			};
			for (int i = 0; i < ARRAYSIZE(buttons); i++)
			{
				VARIANT cmd;
				cmd.vt = VT_I2;
				cmd.iVal = buttons[i];
				HDCQCRESULT res;
				if (S_OK == _bhc->QueryCommand(&cmd, HDCQCTYPE_STATE, &res))
				{
					LRESULT enabled = ::SendMessage(_hwndTB, TB_ISBUTTONENABLED, buttons[i], 0);
					if (enabled && res != HDCQCRESULT_AVAIL)
						::SendMessage(_hwndTB, TB_ENABLEBUTTON, buttons[i], FALSE);
					else if (!enabled && res == HDCQCRESULT_AVAIL)
						::SendMessage(_hwndTB, TB_ENABLEBUTTON, buttons[i], TRUE);
				}
			}
			return TRUE;
		}
		return FALSE;
	}
	virtual BOOL OnNotify(LPNMHDR pNmHdr, LRESULT& lResult)
	{
		if (pNmHdr->code == TBN_DROPDOWN)
		{
			LPNMTOOLBAR pnmTB = (LPNMTOOLBAR)pNmHdr;
			RECT rc;
			// get the button's rectange in screen coordinates.            
			::SendMessage(pnmTB->hdr.hwndFrom, TB_GETRECT, (WPARAM)pnmTB->iItem, (LPARAM)&rc);
			::MapWindowPoints(pnmTB->hdr.hwndFrom, HWND_DESKTOP, (LPPOINT)&rc, 2);
			HMENU hmenu = ::LoadMenu(AppInstanceHandle, MAKEINTRESOURCE(IDR_POPUP_TB_ADD));
			HMENU hpopup = ::GetSubMenu(hmenu, 0);
			UINT mitems[] =
			{
				IDM_ANNOTATE,
				IDM_DRAW_LINE,
				IDM_DRAW_RECTANGLE,
				IDM_DRAW_CIRCLE,
			};
			for (int i = 0; i < ARRAYSIZE(mitems); i++)
			{
				VARIANT cmd;
				cmd.vt = VT_I2;
				cmd.iVal = mitems[i];
				HDCQCRESULT res;
				if (S_OK == _bhc->QueryCommand(&cmd, HDCQCTYPE_STATE, &res))
				{
					if (res != HDCQCRESULT_AVAIL)
						::EnableMenuItem(hpopup, mitems[i], MF_BYCOMMAND | MF_GRAYED);
				}
			}
			TPMPARAMS tpm = { sizeof(TPMPARAMS), rc };
			::TrackPopupMenuEx(hpopup, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, rc.left, rc.bottom, _hwnd, &tpm);
			::DestroyMenu(hmenu);
			return TRUE;
		}
		return FALSE;
	}
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam)
	{
		if (LOWORD(wParam) == IDM_RECEIVE_HEXDUMP_NOTIF)
		{
			if (HIWORD(wParam) == 0)
			{
				HDCPrintTask printTask = (HDCPrintTask)LOWORD(lParam);
				USHORT printPage = HIWORD(lParam);
				//::SendMessage(_hwndTB, TB_ENABLEBUTTON, IDM_PRINTDATA, printTask>= TskFinishJob? TRUE:FALSE);
			}
			return TRUE;
		}
		if (LOWORD(wParam) == IDM_LIST_SCANNED_TAGS)
		{
			onListScannedTags();
			return TRUE;
		}
		if (IDM_MIN <= LOWORD(wParam) && LOWORD(wParam) <= IDM_MAX)
		{
			if (_bhc)
			{
				VARIANT varg;
				varg.vt = VT_I2;
				varg.iVal = LOWORD(wParam);
				_bhc->SendCommand(&varg, NULL);
			}
			return TRUE;
		}
		return _ChildWindow::OnCommand(wParam, lParam);
	}

	void onListScannedTags()
	{
		VARIANT vres;
		VariantInit(&vres);
		VARIANT varg;
		varg.vt = VT_I2;
		varg.iVal = IDM_LIST_SCANNED_TAGS;
		if (S_OK == _bhc->SendCommand(&varg, &vres))
		{
			if (vres.vt == (VT_ARRAY | VT_BSTR))
			{
				// the button drops down a popup menu populated with tag labels as menu items.
				RECT rcTB;
				::GetWindowRect(_hwndTB, &rcTB);
				// get the coordinates of the menu's upper left corner.
				POINT pt = {rcTB.left+ TBWND_BUTTON_WIDTH, rcTB.bottom-4};
				HMENU hpopup = CreatePopupMenu();

				SAFEARRAY* psa = vres.parray;
				SafeArrayLock(psa);
				UINT numTags = psa->rgsabound[0].cElements;
				for (UINT i = 0; i < numTags; ++i)
				{
					BSTR bi = ((BSTR*)psa->pvData)[i];
					USHORT cmdId = bi[0]; // 0th char is the command id.
					LPCWSTR label = bi + 1; // the label text follows the id.
					AppendMenu(hpopup, MF_ENABLED | MF_STRING, cmdId, label);
				}
				SafeArrayUnlock(psa);

				int nCmd = ::TrackPopupMenu(
					hpopup,
					TPM_LEFTALIGN | TPM_RIGHTBUTTON,
					pt.x, pt.y,
					0,
					_hwnd,
					NULL);
				DestroyMenu(hpopup);
			}
		}
		VariantClear(&vres);
	}
};

