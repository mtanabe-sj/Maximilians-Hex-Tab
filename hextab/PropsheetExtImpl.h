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
#include "IUnknownImpl.h"
#include "helper.h"
#include "lib.h"
#ifdef APP_USES_HOTKEYS
#include "KeyboardAccel.h"
#endif//#ifdef APP_USES_HOTKEYS
#include "hextabconfig.h"
#include "BinhexDumpWnd.h"

/* PropsheetExtImpl - implements the Shell Propertysheet Extension interface IShellPropSheetExt to communicate with the host of the property sheet, i.e., Windows Explorer, or HEXDUMPDLG, and produce and manage the property page 'tab' UI of HEXDUMPTAB.

A host performs these operations to bind HEXDUMPTAB as a page to a propertysheet.
- call DllRegisterServer to create an instance of PropsheetExtImpl.
- call PropsheetExtImpl::Initialize to pass a pathname of a file to be examined.
- call PropsheetExtImpl::AddPages to bind a PROPSHEETPAGE descriptor PropsheetExtImpl supplies to the propertysheet the host maintains.
- call PropsheetExtImpl::ProppageDlgProc with the lParam set to the PROPSHEETPAGE to establish communication with the host. specifically, ProppageDlgProc takes the instance address of PropsheetExtImpl from the PROPSHEETPAGE, locates the dialog handler of PropsheetExtImpl from it, and binds the handler to the window messaging loop of the host.

for general technical info on IShellPropSheetExt, refer to MS documentation at
http://msdn.microsoft.com/en-us/library/windows/desktop/ms677109(v=vs.85).aspx

HEXDUMPDLG performs additional operations through the private interface, IHexTabConfig, to achieve a tighter integration with PropsheetExtImpl. For the details, refer to the code of HEXDUMPDLG, in particular, the implementation of method PropsheetDlg::createShellPropsheetExt.
*/
class PropsheetExtImpl :
	public UIModalState,
	public SimpleDlg,
	public IShellExtInit,
	public IHexTabConfig,
	public IUnknownImpl<IShellPropSheetExt, &IID_IShellPropSheetExt>
{
public:
	PropsheetExtImpl();
	virtual ~PropsheetExtImpl();

	// IUnknown methods
	STDMETHOD(QueryInterface)(REFIID riid, LPVOID* ppvObj)
	{
		// windows explorer uses this interface to initialize our prop page instance.
		if (IsEqualIID(riid, IID_IShellExtInit))
		{
			AddRef();
			*ppvObj = (LPSHELLEXTINIT)this;
			return S_OK;
		}
		// our hexdumpdlg host requests for this interface for configuration purposes.
		if (IsEqualIID(riid, IID_IHexTabConfig))
		{
			AddRef();
			*ppvObj = (IHexTabConfig*)this;
			return S_OK;
		}
		return IUnknownImpl<IShellPropSheetExt, &IID_IShellPropSheetExt>::QueryInterface(riid, ppvObj);
	}
	STDMETHOD_(ULONG, AddRef)() { return IUnknownImpl<IShellPropSheetExt, &IID_IShellPropSheetExt>::AddRef(); }
	STDMETHOD_(ULONG, Release)() { return IUnknownImpl<IShellPropSheetExt, &IID_IShellPropSheetExt>::Release(); }

	// IHexTabConfig methods
	STDMETHOD(GetParam) (HDCPID paramId, VARIANT *varValue) { return E_NOTIMPL; }
	STDMETHOD(SetParam) (HDCPID paramId, VARIANT *varValue);
	STDMETHOD(SendCommand) (VARIANT *varCommand, VARIANT *varResult);
	STDMETHOD(QueryCommand) (VARIANT *varCommand, HDCQCTYPE queryType, HDCQCRESULT *queryResult);

	// IShellPropSheetExt methods
	STDMETHOD(AddPages) ( /* [annotation][in] */ __in  LPFNSVADDPROPSHEETPAGE pfnAddPage, /* [annotation][in] */ __in  LPARAM lParam);
	STDMETHOD(ReplacePage) ( /* [annotation][in] */ __in  EXPPS uPageID, /* [annotation][in] */ __in  LPFNSVADDPROPSHEETPAGE pfnReplaceWith, /* [annotation][in] */ __in  LPARAM lParam);
	// IShellExtInit method 
	STDMETHOD(Initialize) (LPCITEMIDLIST pidlFolder, LPDATAOBJECT lpdobj, HKEY hKeyProgID);

	// dialog proc AddPages passes to CreatePropertySheetPage API. passes 'this' to SimpleDlg on WM_INITDIALOG so that the latter can save it on DWLP_USER.
	static INT_PTR CALLBACK ProppageDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		if (uMsg == WM_INITDIALOG) {
			lParam = ((PROPSHEETPAGE*)lParam)->lParam;
		}
		return SimpleDlg::DlgProc(hDlg, uMsg, wParam, lParam);
	}
	// callback proc passed to CreatePropertySheetPage. the host (explorer or hexdumpdlg) uses it to maintain a reference to the current instance.
	static UINT CALLBACK PageCallbackProc(HWND hwnd, UINT uMsg, LPPROPSHEETPAGE ppsp) {
		PropsheetExtImpl* pThis = (PropsheetExtImpl*)(SimpleDlg*)ppsp->lParam;
		switch (uMsg) {
		case PSPCB_ADDREF:
			pThis->AddRef();
			break;
		case PSPCB_CREATE:
			return 1;
		case PSPCB_RELEASE:
			pThis->Release();
			break;
		}
		return 0;
	}

	/*
	// use this to notify the property sheet if a change has been made and the Apply button should be enabled.
	static void SetModified(HWND hDlg)
	{
		SendMessage(GetParent(hDlg), PSM_CHANGED, (WPARAM)hDlg, 0);
	}
	*/

	// UIModalState overrides
	virtual void Enter()
	{
#ifdef APP_USES_HOTKEYS
		_kaccel._enabled = false;
#endif//#ifdef APP_USES_HOTKEYS
	}
	virtual void Leave()
	{
#ifdef APP_USES_HOTKEYS
		_kaccel._enabled = true;
#endif//#ifdef APP_USES_HOTKEYS
	}

protected:
	HICON _hicon; // handle to a small (16x16) icon shown on the property page's tab
#ifdef APP_USES_HOTKEYS
	KeyboardAccel _kaccel; // manages a keyboard accelerator
#endif//#ifdef APP_USES_HOTKEYS
	BinhexDumpWnd *_dumpwnd; // main window class managing the hexdump display and operation.

	virtual BOOL OnInitDialog();
	virtual void OnDestroy();
	virtual void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnNotify(LPNMHDR pNmHdr, LRESULT *plRes);
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

};

