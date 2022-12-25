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
#include "PropsheetExtImpl.h"
#include "FindDlg.h"
#include "DataRangeDlg.h"
#include "persistMetafile.h"
#include "BitmapImage.h"
#include "libver.h"
#include "ScanTag.h"
#include "HexdumpPrintDlg.h"
#include "HexdumpPageSetupParams.h"


#ifdef _DEBUG
// process-wide event logging support.
EventLog evlog;
#endif//#ifdef _DEBUG

////////////////////////////////////////////////////////////////

PropsheetExtImpl::PropsheetExtImpl() :
	SimpleDlg(IDS_PROPPAGE),
	_dumpwnd(NULL),
	_hicon(NULL)
{
}

PropsheetExtImpl::~PropsheetExtImpl()
{
	if (_dumpwnd)
		delete _dumpwnd;
	if (_hicon)
		DestroyIcon(_hicon);
}

/* method QueryCommand of interface IHexTabConfig - passes back availability of a command.
the host app (hexdumpdlg) calls this interface method to find out about the availability of a command.
*/
STDMETHODIMP PropsheetExtImpl::QueryCommand(VARIANT *varCommand, HDCQCTYPE queryType, HDCQCRESULT *queryResult)
{
	if (varCommand->vt != VT_I2)
		return E_INVALIDARG;
	switch(varCommand->iVal)
	{
	case IDM_SCANTAG_START:
		*queryResult = _dumpwnd->canScanTag() ? HDCQCRESULT_AVAIL : HDCQCRESULT_UNAVAIL;
		return S_OK;
	case IDM_FINDPREV_METAITEM:
	case IDM_FINDNEXT_METAITEM:
		*queryResult = (_dumpwnd->_regions.size() || _dumpwnd->_annotations.querySize(true) || _dumpwnd->_shapes.size()) ? HDCQCRESULT_AVAIL : HDCQCRESULT_UNAVAIL;
		return S_OK;
	case IDM_FINDDATA:
	case IDM_PRINTDATA:
		if (_dumpwnd->_printJob)
		{
			*queryResult = HDCQCRESULT_UNAVAIL;
			return S_OK;
		}
		// fall through to pass back an AVAILABLE result
	case IDM_SAVEDATA:
		*queryResult = HDCQCRESULT_AVAIL;
		return S_OK;
	case IDM_LIST_SCANNED_TAGS:
		*queryResult = _dumpwnd->canListTags()? HDCQCRESULT_AVAIL : HDCQCRESULT_UNAVAIL;
		return S_OK;
	};
	return E_INVALIDARG;
}

/* method SendCommand of interface IHexTabConfig - responds to a command invocation by the host (hexdumpdlg). delegates the command processing to BinhexDumpWnd.
*/
STDMETHODIMP PropsheetExtImpl::SendCommand(VARIANT *varCommand, VARIANT *varResult)
{
	if (varCommand->vt == VT_I2)
	{
		if (_dumpwnd->processCommand(MAKEWPARAM(varCommand->iVal, BN_CLICKED), LPARAM_FROM_SENDCOMMAND, varResult))
			return S_OK;
		else
			return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
	}
	return E_INVALIDARG;
}

/* method SetParam of interface IHexTabConfig - controls aspects of the hexdumptab behavior. delegates actual configuration to BinhexDumpWnd.
*/
STDMETHODIMP PropsheetExtImpl::SetParam(HDCPID paramId, VARIANT *varValue)
{
	return _dumpwnd->HDConfig_SetParam(paramId, varValue);
}

/* method AddPages of interface IShellPropSheetExt - the host app (explorer or hexdumpdlg) calls this method to add hexdumptab as a property page to the hosting property sheet window.
*/
STDMETHODIMP PropsheetExtImpl::AddPages ( /* [annotation][in] */ __in  LPFNSVADDPROPSHEETPAGE pfnAddPage, /* [annotation][in] */ __in  LPARAM lParam)
{
	DBGPUTS((L"PropsheetExtImpl::AddPages"));
	_hicon = (HICON)LoadImage(LibResourceHandle, MAKEINTRESOURCE(IDI_PROPPAGE), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
	PROPSHEETPAGE	psp = { 0 };
	HPROPSHEETPAGE hPage;
	psp.dwSize = sizeof(psp);
	psp.dwFlags = PSP_USEREFPARENT | PSP_USETITLE | PSP_USEHICON | PSP_USECALLBACK;
	psp.hInstance = LibResourceHandle;
	psp.hIcon = _hicon;
	psp.pszTitle = MAKEINTRESOURCE(IDS_PROPPAGE);
	//psp.pcRefParent = &this->_ref;
	psp.pfnCallback = PageCallbackProc;
	psp.lParam = (LPARAM)(SimpleDlg*)this;
	psp.pszTemplate = MAKEINTRESOURCE(_dumpwnd->queryDlgTemplate());
	psp.pfnDlgProc = (DLGPROC)ProppageDlgProc;
	hPage = CreatePropertySheetPage(&psp);
	if (!hPage)
		return E_OUTOFMEMORY;
	if (pfnAddPage(hPage, lParam))
		return S_OK; // success.
	DestroyPropertySheetPage(hPage);
	return E_FAIL; // our page did not get added.
}

/* method ReplacePage of interface IShellPropSheetExt - (not implemented)
windows explorer never calls ReplacePage.
*/
STDMETHODIMP PropsheetExtImpl::ReplacePage( /* [annotation][in] */ __in  EXPPS uPageID, /* [annotation][in] */ __in  LPFNSVADDPROPSHEETPAGE pfnReplaceWith, /* [annotation][in] */ __in  LPARAM lParam)
{
	return E_NOTIMPL;
}

/* method Initialize of interface IShellExtInit - the host app (explorer or hexdumpdlg) calls this method to pass a pathname of a file for property examination and display.
the path data is passed in as an HDROP handle.
this is the first call the host makes after the current PropsheetExtImpl instance is created by DllGetClassObject. so, we take it as an opportunity to instantiate our main frame class, BinhexDumpWnd. BinhexDumpWnd saves the file's pathname for later use. note, though, that the frame window is not created yet. that is deferred to OnInitDialog.
*/
STDMETHODIMP PropsheetExtImpl::Initialize(LPCITEMIDLIST pidlFolder, LPDATAOBJECT lpdobj, HKEY hKeyProgID)
{
	DBGPUTS((L"PropsheetExtImpl::Initialize"));
	if (pidlFolder)
		return E_NOTIMPL; // folder not supported
	HRESULT hr = E_INVALIDARG;
	FORMATETC fmtetc = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM smedium;
	if (S_OK == lpdobj->GetData(&fmtetc, &smedium))
	{
		HDROP hdrop = (HDROP)GlobalLock(smedium.hGlobal);
		if (hdrop)
		{
			if (!_dumpwnd)
				_dumpwnd = new BinhexDumpWnd(this);
			if (_dumpwnd->dragQueryFile(hdrop) == 1)
				hr = S_OK;
			GlobalUnlock(smedium.hGlobal);
		}
		ReleaseStgMedium(&smedium);
	}
	return hr;
}

///////////////////////////////////////////////////////
// property page dialog handlers

/* OnInitDialog - initializes the property page of hexdumptab by performing these tasks.
- create work folders if they don't exist,
- create the main window that performs hexdump, displays dump view, responds to commands, and interacts with a user.
- starts a keyboard accelerator.
*/
BOOL PropsheetExtImpl::OnInitDialog()
{
	DBGPUTS((L"PropsheetExtImpl::OnInitDialog"));
	SimpleDlg::OnInitDialog();

	EnsureWorkFolder(true); // temp folder: %TMP%\HexDumpTab
	EnsureWorkFolder(false); // work folder: %AppLocalData%\mtanabe\HexDumpTab

	RECT rc;
	GetClientRect(_hdlg, &rc);
	ASSERT(_dumpwnd); // has Initialize() been called?
	_dumpwnd->Create(_hdlg, rc.right, rc.bottom);

#ifdef APP_USES_HOTKEYS
	_kaccel.open(_hdlg, IDR_ACCEL);
#endif//#ifdef APP_USES_HOTKEYS
	return TRUE;
}

/* OnDestroy - prepares the property page for termination.
it performs these tasks.
- close the keyboard accelerator.
*/
void PropsheetExtImpl::OnDestroy()
{
	DBGPUTS((L"PropsheetExtImpl::OnDestroy"));
#ifdef APP_USES_HOTKEYS
	_kaccel.close();
#endif//#ifdef APP_USES_HOTKEYS
	SimpleDlg::OnDestroy();
}

/* OnSize - responds to a change made to the size of the property page.
you'd think handling WM_SIZE would not be necessary since it's a fixed-frame dialog. not so. at least for explorer being our host, the prop page is sized once after the creation. if the size of _dumpwnd was not updated, it would not fill the inside of the dialog box.
*/
void PropsheetExtImpl::OnSize(UINT nType, int cx, int cy)
{
	if (nType == SIZE_MINIMIZED)
		return;

	if (_dumpwnd)
		SetWindowPos(*_dumpwnd, NULL, 0, 0, cx, cy, SWP_NOZORDER | SWP_NOMOVE);
}

/* OnNotify - responds to property page notifications.
- PSN_APPLY: serializes meta tags in a file in per-user data folder in %LocalAppData%.
- PSN_QUERYCANCEL: prepares to exit by checking up on a print job in progress and cleaning work files in %TMP%.
- PSN_SETACTIVE and PSN_KILLACTIVE: enables and disables the keyboard accelerator.
*/
BOOL PropsheetExtImpl::OnNotify(LPNMHDR pNmHdr, LRESULT *plRes)
{
	if (pNmHdr->code == PSN_APPLY)
	{
		// user clicked Apply or OK.
		_dumpwnd->propsheetApplyNotify(true);
		*plRes = PSNRET_NOERROR;
		return TRUE;
	}
	// PSN_QUERYCANCEL is raised if Cancel is clicked.
	if (pNmHdr->code == PSN_QUERYCANCEL)
	{
		if (!_dumpwnd->propsheetQueryCancel())
		{
			/* https://docs.microsoft.com/en-us/windows/win32/controls/psn-querycancel
			Returns TRUE to prevent the cancel operation, or FALSE to allow it.
			*/
			*plRes = TRUE;
			return TRUE;
		}
	}
#ifdef APP_USES_HOTKEYS
	if (pNmHdr->code == PSN_SETACTIVE)
		_kaccel._enabled = true;
	else if (pNmHdr->code == PSN_KILLACTIVE)
		_kaccel._enabled = false;
#endif//#ifdef APP_USES_HOTKEYS
	return SimpleDlg::OnNotify(pNmHdr, plRes);
}

/* OnCommand - processes commands.
it delegates command processing to the main window BinhexDumpWnd.
*/
BOOL PropsheetExtImpl::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if (_dumpwnd->processCommand(wParam, lParam))
		return TRUE;
	return SimpleDlg::OnCommand(wParam, lParam);
}

