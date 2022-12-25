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
#include <initguid.h>
#include "PropsheetDlg.h"
#include "Resource.h"
#include "SimpleDlg.h"
#include "..\hextab\libver.h"
#include "RegistryHelper.h"
#include "hextabconfig.h"
#include "hextabscan.h"


/* refer to this on property sheet usage:
https://docs.microsoft.com/en-us/windows/win32/controls/property-sheets
*/

#include <pshpack1.h>
typedef struct {
	WORD      dlgVer;
	WORD      signature;
	DWORD     helpID;
	DWORD     exStyle;
	DWORD     style;
	WORD      cDlgItems;
	short     x;
	short     y;
	short     cx;
	short     cy;
	/*sz_Or_Ord menu;
	sz_Or_Ord windowClass;
	WCHAR     title[titleLen];
	WORD      pointsize;
	WORD      weight;
	BYTE      italic;
	BYTE      charset;
	WCHAR     typeface[stringLen];*/
} __DLGTEMPLATEEX;
#include <pshpack1.h>


bool ToolbarWindow::_IsRegistered = false;


HRESULT PropsheetDlg::subscribeToNotification()
{
	IHexTabConfig *bhc;
	HRESULT hr = _spsx->QueryInterface(IID_IHexTabConfig, (LPVOID*)&bhc);
	ASSERT(hr == S_OK);

	VARIANT varg;
	varg.vt = VT_UI4;
	varg.ulVal = (ULONG)(ULONG_PTR)(HWND)_tbwnd;
	hr = bhc->SetParam(HDCPID_HOST_HWND, &varg);
	varg.vt = VT_UI4;
	varg.ulVal = IDM_RECEIVE_HEXDUMP_NOTIF;
	hr = bhc->SetParam(HDCPID_HOST_NOTIF_COMMAND_ID, &varg);
	bhc->Release();
	return hr;
}

HRESULT PropsheetDlg::createShellPropsheetExt()
{
	HRESULT hr;
	AutoComRel<IShellPropSheetExt> spsx;
	hr = CoCreateInstance(CLSID_MyShellPropsheetExt, NULL, CLSCTX_INPROC_SERVER, IID_IShellPropSheetExt, (LPVOID*)&spsx);
	if (hr != S_OK)
		return hr;
	IShellExtInit *sxi;
	hr = spsx->QueryInterface(IID_IShellExtInit, (LPVOID*)&sxi);
	ASSERT(hr == S_OK);
	PIDLIST_ABSOLUTE pidl;
	hr = SHILCreateFromPath(_fpath, &pidl, NULL);
	ASSERT(hr == S_OK);
	IShellItem *psi;
	hr = SHCreateShellItem(NULL, NULL, pidl, &psi);
	ASSERT(hr == S_OK);
	IDataObject *dobj;
	hr = psi->BindToHandler(NULL, BHID_DataObject, IID_IDataObject, (LPVOID*)&dobj);
	ASSERT(hr == S_OK);
	hr = sxi->Initialize(NULL, dobj, NULL);
	dobj->Release();
	psi->Release();
	sxi->Release();
	CoTaskMemFree(pidl);
	if (hr == S_OK)
	{
		IHexTabConfig *bhc;
		hr = spsx->QueryInterface(IID_IHexTabConfig, (LPVOID*)&bhc);
		ASSERT(hr == S_OK);

		VARIANT varg;
		varg.vt = VT_BSTR;
		varg.bstrVal = SysAllocString(L"hexdumpdlg");
		hr = bhc->SetParam(HDCPID_CONTROLLER, &varg);
		ASSERT(hr == S_OK);
		VariantClear(&varg);
		varg.vt = VT_I4;
		varg.lVal = _fbpr;
		hr = bhc->SetParam(HDCPID_BPR, &varg);
		ASSERT(hr == S_OK);
		varg.vt = VT_UI4;
		varg.ulVal = _fmetaId;
		hr = bhc->SetParam(HDCPID_METAFILEID, &varg);
		ASSERT(hr == S_OK);
		bhc->Release();

		hr = spsx->AddPages(addPropPage, (LPARAM)(ULONG_PTR)this);
		if (hr == S_OK)
			_spsx = spsx.detach();
	}
	DBGPRINTF((L"createShellPropsheetExt: HR=%X\n", hr));
	return hr;
}

HRESULT PropsheetDlg::deleteShellPropsheetExt()
{
	HPROPSHEETPAGE h = (HPROPSHEETPAGE)InterlockedExchangePointer((LPVOID*)&_hpage, NULL);
	if (h)
		PropSheet_RemovePage(_hdlg, 0, h);
	IShellPropSheetExt *x = (IShellPropSheetExt*)InterlockedExchangePointer((LPVOID*)&_spsx, NULL);
	if (x)
		x->Release();
	return S_OK;
}

BOOL PropsheetDlg::_addPropPage(HPROPSHEETPAGE hPage)
{
	_hpage = hPage;
	return TRUE;
}

int CALLBACK PropsheetDlg::propsheetCB(HWND hwnd, UINT message, LPARAM lParam)
{
	DBGPRINTF((L"propsheetCB: hwnd=%p, msg=%d, param=%p\n", hwnd, message, lParam));
	switch (message)
	{
	case PSCB_PRECREATE:
		DBGPUTS((L"PropsheetCB - PSCB_PRECREATE\n"));
		if (lParam)
		{
			__DLGTEMPLATEEX *tmplx = (__DLGTEMPLATEEX*)lParam;
			/*if (tmplx->signature == 0xFFFF)
			{
				tmplx->cx *= 2;
				tmplx->cy *= 2;
			}
			else
			{
				DLGTEMPLATE *tmpl = (DLGTEMPLATE*)lParam;
				tmpl->cx *= 2;
				tmpl->cy *= 2;
			}
			*/
		}
		return 0;

	case PSCB_INITIALIZED:
		DBGPRINTF((L"PropsheetCB - PSCB_INITIALIZED(%p)\n", hwnd));
		return 0;

	case PSCB_BUTTONPRESSED:
		DBGPRINTF((L"PropsheetCB - PSCB_BUTTONPRESSED(%d)\n", lParam));
		// lParam: PSBTN_FINISH(Close), PSBTN_OK, PSBTN_APPLYNOW, or PSBTN_CANCEL
	}

	return 0;
}

LPCWSTR _getFileFilterString(ustring &filter)
{
	UINT ids1[] = { IDS_FILEBROWSE_FILTER_IMAGE1, IDS_FILEBROWSE_FILTER_TEXT1, IDS_FILEBROWSE_FILTER_DATA1, IDS_FILEBROWSE_FILTER_ALL1 };
	UINT ids2[] = { IDS_FILEBROWSE_FILTER_IMAGE2, IDS_FILEBROWSE_FILTER_TEXT2, IDS_FILEBROWSE_FILTER_DATA2, IDS_FILEBROWSE_FILTER_ALL2 };
	LPCWSTR groups[] = { L"image", L"text", L"data", L"all" };

	int i;
	LONG res;
	ustring2 flt1[4];
	ustring2 flt2[4];
	for (i = 0; i < ARRAYSIZE(groups); i++)
	{
		flt1[i].loadString(ids1[i]);
		flt2[i].loadString(ids2[i]);

		WCHAR fext[16] = { 0 };
		while (ERROR_SUCCESS == (res = Registry_FindEntryByValue(HKEY_LOCAL_MACHINE, SCANSERVER_REGKEY L"\\ExtGroups", groups[i], fext, ARRAYSIZE(fext))))
		{
			flt2[i] += L";*";
			flt2[i] += fext;
		}
	}

	int len = 2;
	for (i = 0; i < ARRAYSIZE(groups); i++)
	{
		len += flt1[i]._length + flt2[i]._length + 2;
	}
	LPWSTR p = filter.alloc(len);
	for (i = 0; i < ARRAYSIZE(groups); i++)
	{
		CopyMemory(p, flt1[i]._buffer, flt1[i]._length * sizeof(WCHAR));
		p += flt1[i]._length + 1;
		CopyMemory(p, flt2[i]._buffer, flt2[i]._length * sizeof(WCHAR));
		p += flt2[i]._length + 1;
	}
	return filter;
}

BOOL PropsheetDlg::_getFile()
{
	ustring2 caption(IDS_FILE_OPEN_DLG);
	ustring filter;
	WCHAR path[_MAX_PATH] = { 0 };
	OPENFILENAME ofn = { sizeof(OPENFILENAME), 0 };
	ofn.lpstrTitle = caption;
	ofn.hwndOwner = _hdlg;
	ofn.Flags = OFN_FILEMUSTEXIST;
	ofn.lpstrFilter = _getFileFilterString(filter);
	//ofn.lpstrDefExt = L".jpg";
	ofn.lpstrFile = path;
	ofn.nMaxFile = ARRAYSIZE(path);
	if (!GetOpenFileName(&ofn))
	{
		DWORD cderr = CommDlgExtendedError(); // CDERR_DIALOGFAILURE, etc.
		if (cderr != 0) // 0 if user has canceled.
			MessageBox(_hdlg, ustring(ustring2(IDS_CDERR), cderr), ustring2(IDS_APP_TITLE), MB_OK | MB_ICONEXCLAMATION);
		// user canceled
		return FALSE;
	}
	_fpath = path;
	return TRUE;
}

LPCWSTR _getFileTitlePtr(LPCWSTR fpath)
{
	LPCWSTR p = wcsrchr(fpath, '\\');
	if (p)
		return p + 1;
	return fpath;
}

BOOL PropsheetDlg::Open(objList<ustring> &args)
{
#ifdef _DEBUG_x
	if (IDCANCEL == MessageBox(NULL, L"Ready to execute", L"Debug Stop @ PropsheetDlg::Open", MB_OKCANCEL))
		return FALSE;
#endif//_DEBUG
	if (args.size() > 0)
	{
		_fpath = *args[0];
		if (args.size() > 1)
		{
			_fbpr = _wtoi(*args[1]);
			if (args.size() > 2)
				_fmetaId = (ULONG)_wtoi(*args[2]);
		}
	}
	if (_fpath._length == 0)
	{
		if (!_getFile())
			return FALSE;
	}

	if (S_OK != createShellPropsheetExt())
		return FALSE;

	ustring2 caption(L"%s - %s", (LPCWSTR)ustring2(IDS_APP_TITLE), _getFileTitlePtr(_fpath));

	_psh.dwSize = sizeof(PROPSHEETHEADER);
	_psh.dwFlags |= PSH_MODELESS | PSH_USECALLBACK | PSH_NOCONTEXTHELP | PSH_USEICONID | PSH_NOAPPLYNOW;
	_psh.pfnCallback = propsheetCB;
	_psh.hInstance = AppInstanceHandle;
	_psh.pszIcon = MAKEINTRESOURCE(IDI_APP);
	_psh.pszCaption = caption;
	_psh.phpage = &_hpage;
	_psh.nPages = 1;
	INT_PTR res = PropertySheet(&_psh);
	if (res <= 0)
	{
		DWORD errorCode = GetLastError();
		DBGPRINTF((L"PropertySheet failed (res=%d; error=%x)\n", res, errorCode));
		return FALSE;
	}
	_hdlg = (HWND)res;
	_tbwnd.Create(_hdlg, _spsx);
	// add 'about this app' menu command to the system menu.
	HMENU hsysmenu = GetSystemMenu(_hdlg, FALSE);
	if (hsysmenu)
	{
		InsertMenu(hsysmenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
		InsertMenu(hsysmenu, -1, MF_BYPOSITION | MF_STRING, IDM_ABOUT, (LPCWSTR)ustring2(IDS_ABOUTBOX));
	}
	DWORD val;
	if (ERROR_SUCCESS == Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY2, L"dlg.pos", &val))
		SetWindowPos(_hdlg, NULL, LOWORD(val), HIWORD(val), 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	subscribeToNotification();
	return TRUE;
}

/*
For a modeless property sheet, your message loop should use PSM_ISDIALOGMESSAGE to pass messages to the property sheet dialog box. Your message loop should use PSM_GETCURRENTPAGEHWND to determine when to destroy the dialog box. When the user clicks the OK or Cancel button, PSM_GETCURRENTPAGEHWND returns NULL. You can then use the DestroyWindow function to destroy the dialog box.
*/
void PropsheetDlg::Run()
{
	//HACCEL hacc = ::LoadAccelerators(AppResourceHandle, MAKEINTRESOURCE(IDR_ACCELERATOR));
	HWND hwnd;
	BOOL b;
	MSG msg;
	while ((b = ::GetMessage(&msg, NULL, 0, 0)) != 0) {
		if (b == -1) {
			ASSERT(FALSE);
			break; // fatal error
		}
		//DBGPRINTF((L"Run(%p): hwnd=%p, msg=%X, wp=%X, lp=%X\n", _hdlg, msg.hwnd, msg.message, msg.wParam, msg.lParam));
		if (PropSheet_IsDialogMessage(_hdlg, &msg))
		{
			hwnd = PropSheet_GetCurrentPageHwnd(_hdlg);
			if (!hwnd)
				break;
			if (msg.message == WM_SYSCOMMAND && msg.wParam == IDM_ABOUT && msg.hwnd == _hdlg)
			{
				//b = PeekMessage(&msg, NULL, WM_SYSCOMMAND, WM_SYSCOMMAND, PM_REMOVE);
				SimpleDlg dlg(IDD_ABOUTBOX, _hdlg);
				dlg.DoModal();
				continue;
			}
			continue;
		}
		//if (::TranslateAccelerator(_hdlg, hacc, &msg))
		//	continue;
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}
}

void PropsheetDlg::Close()
{
	if (!_hdlg)
		return;
	RECT rc;
	GetWindowRect(_hdlg, &rc);
	DWORD val = MAKELONG(rc.left, rc.top);
	Registry_SetDwordValue(HKEY_CURRENT_USER, LIBREGKEY2, L"dlg.pos", val);

	_tbwnd.Destroy();
	deleteShellPropsheetExt();
	DestroyWindow(_hdlg);
}
