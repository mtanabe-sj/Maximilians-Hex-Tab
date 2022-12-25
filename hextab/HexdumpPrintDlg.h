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
#include <dlgs.h>
#include "SimpleDlg.h"
#include "helper.h"


#define HDPD_FLAG_INITIALIZING 0x00000001
#define HDPD_FLAG_UPDATING_MARGIN 0x00000002
#define HDPD_FLAG_DELAY_PAGENUM_UPDATE 0x00000004

#define HDPD_FONTSIZE_MIN 6
#define HDPD_FONTSIZE_MAX 24

class BinhexMetaView;
class HexdumpPrintJob;
struct HexdumpPageSetupParams;

class HexdumpPrintDlg : public SimpleDlg
{
public:
	HexdumpPrintDlg(HWND h_, BinhexMetaView *b_, HexdumpPageSetupParams *s);
	~HexdumpPrintDlg();

	// holds an instance of win32 PRINTDLG to communicate with Win32 PrintDlg API.
	PRINTDLG *_pd;

	// call choosePrinter to choose a printer from a list and set up pages to print. the method calls Win32 PrintDlg to run a printer selection dialog. it places the selected printer and print configuration in _pd and _setup, respectively.
	HRESULT choosePrinter();
	// after a printer is selected by calling choosePrinter, it's time to call startPrinting. it starts a print job passing _pd and _setup to a HexdumpPrintJob instance. control returns to the caller while the print processing is run in the background.
	HexdumpPrintJob *startPrinting();

protected:
	HexdumpLogicalPageViewParams *_logicalPage; // holds character and page metrics for corrective actions.
	HexdumpPageSetupParams *_setup; // holds a page setup descriptor passed in from the owner BinhexDumpWnd.
	BinhexMetaView *_bmv; // pointer to the owner BinhexDumpWnd instance.
	VIEWINFO _vi[2]; // two view descriptors. one for Line mode, and the other for Page mode.
	FILEREADINFO _fri[2]; // two file descriptors. one for Line mode, and the other for Page mode.
	HWND _hwndVS; // vertical scrollbar control next to the preview pane.
	HWND _hPreview; // window handle of the preview static control.
	UINT _radSel; // radio button in selection; All, Pages, or Lines.
	UINT _editSel; // edit control currently active or last selected.
	UINT _page1, _page2; // starting and ending page numbers in Page mode.
	UINT _line1, _line2; // starting and ending line numbers in Line mode.
	int _margin[4], _minMargin[4], _maxMargin[4]; // margins in TOI current, minimum and maximum.
	bool _groupNotes, _fitToWidth; // print options for grouping annotations and fitting to page width.
	HFONT _hfHeader; // small font for the file title and page number in the page header in the preview.
	ULONG _flags; // dialog handler control flags.

	enum RANGETYPE {
		RNG_LINES = 0,
		RNG_PAGES,
	};
	enum MARGINTYPE {
		MGN_TOP = 0,
		MGN_LEFT,
		MGN_RIGHT,
		MGN_BOTTOM,
	};

	void clean();

	/* https://docs.microsoft.com/en-us/windows/win32/api/commdlg/nc-commdlg-lpprinthookproc

	When you use the PrintDlg function to create a Print dialog box, you can provide a PrintHookProc hook procedure to process messages or notifications intended for the dialog box procedure. To enable the hook procedure, use the PRINTDLG structure that you passed to the dialog creation function. Specify the address of the hook procedure in the lpfnPrintHook member and specify the PD_ENABLEPRINTHOOK flag in the Flags member.

	The default dialog box procedure processes the WM_INITDIALOG message before passing it to the hook procedure. For all other messages, the hook procedure receives the message first. Then, the return value of the hook procedure determines whether the default dialog procedure processes the message or ignores it.

	If the hook procedure processes the WM_CTLCOLORDLG message, it must return a valid brush handle to painting the background of the dialog box. In general, if the hook procedure processes any WM_CTLCOLOR* message, it must return a valid brush handle to painting the background of the specified control.

	Do not call the EndDialog function from the hook procedure. Instead, the hook procedure can call the PostMessage function to post a WM_COMMAND message with the IDABORT value to the dialog box procedure. Posting IDABORT closes the dialog box and causes the dialog box function to return FALSE. If you need to know why the hook procedure closed the dialog box, you must provide your own communication mechanism between the hook procedure and your application.

	You can subclass the standard controls of a common dialog box. However, the dialog box procedure may also subclass the controls. Because of this, you should subclass controls when your hook procedure processes the WM_INITDIALOG message. This ensures that your subclass procedure receives the control-specific messages before the subclass procedure set by the dialog box procedure.
	*/
	static UINT_PTR CALLBACK _hookproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
	{
		if (msg == WM_INITDIALOG)
			lp = ((PRINTDLG*)lp)->lCustData;
		return SimpleDlg::DlgProc(hwnd, msg, wp, lp);
	}

	// override the base class. we cannot let it call EndDialog. see the above documentation from MS on EndDialog. 
	virtual BOOL OnOK()
	{
		if (!canClose())
			return TRUE; // to cancel, indicate to the default proc that the OK click has already been taken care of.
		// we're exiting the dialog.
		return FALSE; // allow the default proc to make parameter check and exit the dialog.
	}
	virtual BOOL OnCancel()
	{
		PostMessage(_hdlg, WM_COMMAND, IDABORT, 0);
		return FALSE;
	}

	// event handlers
	virtual BOOL OnInitDialog();
	virtual BOOL OnCommand(WPARAM wp_, LPARAM lp_);
	virtual BOOL OnNotify(LPNMHDR pNmHdr, LRESULT *plRes);
	virtual BOOL OnDrawItem(UINT idCtrl, LPDRAWITEMSTRUCT pDIS);
	virtual BOOL OnVScroll(int scrollAction, int scrollPos, HWND hCtrl, LRESULT *pResult);

	// operations
	double _getDlgItemDouble(UINT idCtrl);
	void _setDlgItemDouble(UINT idCtrl, double newVal);
	BOOL scrollPreview(RANGETYPE rt, int scrollAction, int scrollPos);
	int getCurrentScrollPos(HWND hwndScroller, UINT fMask);
	ustring getPageNum(RANGETYPE rt, bool prefix = false);
	void updatePageNum(RANGETYPE rt);
	void initMarginControls(HexdumpPageSetupParams &setup);
	bool saveNewMargin(MARGINTYPE m);
	bool canClose();
};

