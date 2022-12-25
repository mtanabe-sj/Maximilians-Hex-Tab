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
#include "BinhexDumpWnd.h"
#include "FindDlg.h"
#include "DataRangeDlg.h"
#include "persistMetafile.h"
#include "BitmapImage.h"
#include "libver.h"
#include "ScanTag.h"
#include "HexdumpPrintDlg.h"
#include "HexdumpPageSetupParams.h"
#include "ScanTagExt.h"
#include "AddTagDlg.h"


#ifdef _DEBUG
//#define DEBUG_CREGION_SELECT
#endif//_DEBUG

#define SHAPE_ROTATION_USES_FLTRIG

//#define BLENDOUTLINESHAPE_TESTS_SIMPLELINESHAPE
#define RENDERCIRCLE_USES_GDI_ELLIPSE
#define BLENDSHAPE_BLENDS_CONNECTOR_TOO
//#define BLENDANNOTATION_BLENDS_CONNECTOR_TOO

#define MENUITEMPOS_COLORTAGS 5
#define MENUITEMPOS_DRAW 6
#define MENUITEMPOS_DRAW_PROPERTIES 4
#define MENUITEMPOS_DRAW_PROPERTIES_LINE 0
#define MENUITEMPOS_DRAW_PROPERTIES_INTERIORCOLOR 1
#define MENUITEMPOS_DRAW_PROPERTIES_INTERIORCOLOR_START 2
#define MENUITEMPOS_DRAW_PROPERTIES_OUTLINECOLOR 2
#define MENUITEMPOS_DRAW_PROPERTIES_OUTLINECOLOR_START 0


// color tables defined in CRegionOperation.cpp
extern COLORREF s_regionInteriorColor[MAX_DUMPREGIONCOLORS];
extern COLORREF s_regionBorderColor[MAX_DUMPREGIONCOLORS];

// color tables for GShape
COLORREF s_gshapeColor[MAX_DUMPGSHAPECOLORS] = {
COLORREF_PURPLE,
COLORREF_VIOLET,
COLORREF_BLUE,
COLORREF_LTBLUE,
COLORREF_TURQUOISE,
COLORREF_GREEN,
COLORREF_RED,
COLORREF_PINK,
COLORREF_ORANGE,
COLORREF_YELLOW,
COLORREF_LIME,
COLORREF_LTGRAY,
COLORREF_GRAY,
COLORREF_DKGRAY,
COLORREF_BLACK,
COLORREF_WHITE
};


LPCWSTR ScannableFileExtensions[] = { L".png", L".bmp", L".gif", L".jpg", L".jpeg", L"jif", L".ico", L".txt", L".xml", L".htm", L".html" };

static BYTE s_bom_utf7a[] = { 0x2B,0x2F,0x76,0x38 };
static BYTE s_bom_utf7b[] = { 0x2B,0x2F,0x76,0x39 };
static BYTE s_bom_utf7c[] = { 0x2B,0x2F,0x76,0x2B };
static BYTE s_bom_utf7d[] = { 0x2B,0x2F,0x76,0x2F };
static BYTE s_bom_utf7e[] = { 0x2B,0x2F,0x76,0x38,0x2D };
static BYTE s_bom_utf8[] = { 0xEF,0xBB,0xBF };
static BYTE s_bom_utf16[] = { 0xFF,0xFE };
static BYTE s_bom_utf16_be[] = { 0xFE,0xFF }; // big endian
static BYTE s_bom_utf32[] = { 0xFF, 0xFE, 0, 0 };
static BYTE s_bom_utf32_be[] = { 0, 0, 0xFE,0xFF }; // big endian

BYTEORDERMARK_DESC s_bomdesc[] = {
	{IDM_BOM_UTF7_A, TEXT_UTF7_A, false, false, sizeof(s_bom_utf7a), s_bom_utf7a, "UTF-7 (+/v8)"},
	{IDM_BOM_UTF7_B, TEXT_UTF7_B, false, false, sizeof(s_bom_utf7b), s_bom_utf7b, "UTF-7 (+/vo)"},
	{IDM_BOM_UTF7_C, TEXT_UTF7_C, false, false, sizeof(s_bom_utf7c), s_bom_utf7c, "UTF-7 (+/v+)"},
	{IDM_BOM_UTF7_D, TEXT_UTF7_D, false, false, sizeof(s_bom_utf7d), s_bom_utf7d, "UTF-7 (+/v/)"},
	{IDM_BOM_UTF7_E, TEXT_UTF7_E, false, false, sizeof(s_bom_utf7e), s_bom_utf7e, "UTF-7 (+/v8-)"},
	{IDM_BOM_UTF8, TEXT_UTF8, true, true, sizeof(s_bom_utf8), s_bom_utf8, "UTF-8"},
	{IDM_BOM_UTF16, TEXT_UTF16, true, true, sizeof(s_bom_utf16), s_bom_utf16, "UTF-16"},
	{IDM_BOM_UTF16_BE, TEXT_UTF16_BE, true, true, sizeof(s_bom_utf16_be), s_bom_utf16_be, "UTF-16 (big endian)"},
	{IDM_BOM_UTF32, TEXT_UTF32, true, true, sizeof(s_bom_utf32), s_bom_utf32_be, "UTF-32"},
	{IDM_BOM_UTF32_BE, TEXT_UTF32_BE, true, true, sizeof(s_bom_utf32_be), s_bom_utf32_be, "UTF-32 (big endian)"},
};

bool BinhexDumpWnd::_IsRegistered = false;

BinhexDumpWnd::BinhexDumpWnd(UIModalState *ms) :
	_ChildWindow(L"binhexdump"),
	_ms(ms),
	_sreg(NULL),
	_fyi{ 0 },
	_menuOnDisplay(false),
	_oneFontSizeForAllDisplays(false),
	_fc{ 0 },
	_t2h{ 0 },
	_fad{ 0 },
	_flags(0),
	_pmflags(0),
	_paramMetafileId(0),
	_paramBPR(0),
	_notifCmdId(0),
	_notifHwnd(NULL),
	_printJob(NULL),
	_printSetup(NULL)
{
}

BinhexDumpWnd::~BinhexDumpWnd()
{
	if (_fyi)
		delete _fyi;
}

UINT BinhexDumpWnd::dragQueryFile(HDROP hdrop)
{
	UINT cFiles = DragQueryFile(hdrop, 0xFFFFFFFF, _filename, ARRAYSIZE(_filename));
	if (cFiles == 1) {
		// we support single file selections only.
		DragQueryFile(hdrop, 0, _filename, ARRAYSIZE(_filename));
	}
	return cFiles;
}

HRESULT BinhexDumpWnd::HDConfig_SetParam(HDCPID paramId, VARIANT *varValue)
{
	if (paramId == HDCPID_CONTROLLER && varValue->vt == VT_BSTR)
	{
		_paramController = varValue->bstrVal;
		DBGPRINTF((L"HDConfig_SetParam: HDCPID_CONTROLLER=%s\n", (LPCWSTR)_paramController));
		return S_OK;
	}
	if (paramId == HDCPID_METAFILEID && varValue->vt == VT_UI4)
	{
		_paramMetafileId = varValue->ulVal;
		DBGPRINTF((L"HDConfig_SetParam: HDCPID_METAFILEID=0x%X\n", _paramMetafileId));
		return S_OK;
	}
	if (paramId == HDCPID_BPR && varValue->vt == VT_I4)
	{
		_paramBPR = varValue->lVal;
		DBGPRINTF((L"HDConfig_SetParam: HDCPID_BPR=%d\n", _paramBPR));
		return S_OK;
	}
	if (paramId == HDCPID_HOST_HWND && varValue->vt == VT_UI4)
	{
		_notifHwnd = (HWND)(ULONG_PTR)varValue->ulVal;
		DBGPRINTF((L"HDConfig_SetParam: HDCPID_HOST_HWND=%p\n", _notifHwnd));
		return S_OK;
	}
	if (paramId == HDCPID_HOST_NOTIF_COMMAND_ID && varValue->vt == VT_UI4)
	{
		_notifCmdId = varValue->ulVal;
		DBGPRINTF((L"HDConfig_SetParam: HDCPID_HOST_NOTIF_COMMAND_ID=%d\n", _notifCmdId));
		return S_OK;
	}
	return E_INVALIDARG;
}

UINT BinhexDumpWnd::queryDlgTemplate()
{
	UINT idd = 0;
	if (0 == _wcsicmp(_paramController, L"hexdumpdlg"))
	{
		_bhvp = BHVP_PRIVATE_NORMAL;
		idd = IDD_PROPPAGE_MEDIUM2; // default to 16 columns per row

		DWORD val = _paramBPR;
		_paramBPR = 0;
		if (val == 0)
			Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY2, L"BytesPerRow", &val);
		if (val > 16)
		{
			_bhvp = BHVP_PRIVATE_LARGE;
			idd = IDD_PROPPAGE_LARGE2; // MAX_HEX_BYTES_PER_ROW columns per row
		}
	}
	else
		idd = IDD_PROPPAGE2; // 8 columns per row
	return idd;
}

SIZE BinhexDumpWnd::GetViewExtents()
{
	SIZE vx = _ChildWindow::GetExtents();
	vx.cx -= 2 * _vi.FrameMargin.x;
	vx.cy -= 2 * _vi.FrameMargin.y;
	return vx;
}

void BinhexDumpWnd::SetViewExtents(int cx, int cy)
{
	_vi.FrameSize.cx = cx - 2 * _vi.FrameMargin.x;
	_vi.FrameSize.cy = cy - 2 * _vi.FrameMargin.y;
}

/* Create - creates a Win32 window of class 'binhexdump'.
the window serves as the main view of the hexdumptab property page.
*/
BOOL BinhexDumpWnd::Create(HWND hparent, int cx, int cy)
{
	return _ChildWindow::Create(hparent, IDC_HEXDUMPWND, WS_VISIBLE | WS_BORDER | WS_VSCROLL, 0, 0, 0, cx, cy, _IsRegistered);
}

/* OnCreate - a callback to respond to the window creation of CreateWindowEx started by a call to the Create method.
- reads per-user configuration settings from the registry.
- initializes the hexdump view generator (BinhexMetaView).
- generates a meta file id based on the pathname of the user file to be hex-dumped.
- sets up the scrollbars of the main window.
- initializes the data search and creates a byte-order-mark detector.
- loads meta tags deserialized from a data file in %LocalAppData% (or %TMP%).
*/
BOOL BinhexDumpWnd::OnCreate()
{
	_ChildWindow::OnCreate();

	DWORD val;
	if (ERROR_SUCCESS == Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY, L"Shape.Opacity", &val))
		_shapeOpacity = val;
	if (ERROR_SUCCESS == Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY, L"Annotation.Opacity", &val))
		_annotOpacity = val;
	if (ERROR_SUCCESS == Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY, L"RowSelector.BorderColor", &val))
		_rowselectorBorderColor = val;
	if (ERROR_SUCCESS == Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY, L"RowSelector.InteriorColor", &val))
		_rowselectorInteriorColor = val;
	if (_bhvp == BHVP_SHELL)
	{
		if (ERROR_SUCCESS == Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY, L"BytesPerRow", &val) && val)
			_vi.BytesPerRow = val;
		if (ERROR_SUCCESS == Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY, L"FontSize", &val) && val)
			_vi.FontSize = val;
		if (ERROR_SUCCESS == Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY, L"FixFontSize", &val))
			_oneFontSizeForAllDisplays = val ? true : false;
	}
	else
	{
		_vi.BytesPerRow = _bhvp == BHVP_PRIVATE_LARGE ? MAX_HEX_BYTES_PER_ROW : 16;
		_vi.FontSize = DEFAULT_FONT_SIZE;
		_oneFontSizeForAllDisplays = true;
	}

	// generate a work file id for serializing meta tags and other resources.
	// for one thing, the id is used to name a work folder in %TMP% where the resources are kept.
	_MetafileId = ROFile::getFileId();

	// initialize the view generator and scrollbars.
	BinhexMetaView::init(_hwnd);

	_vi.FrameSize = GetViewExtents();

	if (configScrollbars(_vi.FrameSize.cx, _vi.FrameSize.cy, false))
	{
		SetTimer(_hwnd, TIMERID_DELAYCONFIGSCROLLBARS, 1000, NULL);
	}

	initScrollbarsPos();

	// reset the Find Data parameters.
	_fc.HitOffset.QuadPart = 0;
	_fc.Flags = FINDCONFIGFLAG_ENCODING_UTF8 | FINDCONFIGFLAG_DIR_FORWARD;

	// need a byte-order-mark detector.
	_bomf = new BOMFile(this);

	DBGPRINTF((L"OnCreate: _paramMetafileId=%X\n", _paramMetafileId));
	// load up meta tags from a data file in %LocalAppData% or %TMP%.
	persistMetafile pmf(_paramMetafileId!=0, _filename, _paramMetafileId);
	if (S_OK == pmf.load(_regions, _annotations, _shapes, &_fad, &_pmflags))
	{
		_pmflags |= PMFLAG_APP_SCANNED; // turn on the flag to gray out the Scan button and menu command.
		// keep the ID of the meta tags file. it tells us if 'Scan' command should be made available or not.
		if (_paramMetafileId == 0)
			_paramMetafileId = pmf.getFileId();
		// to show the loaded meta tags, start a view refresh with a fadeout effect.
		::PostMessage(_hwnd, WM_COMMAND, MAKEWPARAM(IDM_FADEOUT_ANNOTATIONS, BN_CLICKED), 0);
	}
	return TRUE;
}

/* OnDestroy - prepares the class object for termination.
- stops any print job and scan job,
- saves modified preferences,
- cleans up resources used by the view generator.
*/
void BinhexDumpWnd::OnDestroy()
{
	// stop a print job regardless of its status (e.g., in progress).
	HexdumpPrintJob* pj;
	lockObjAccess();
	pj = (HexdumpPrintJob*)InterlockedExchangePointer((LPVOID*)&_printJob, NULL);
	unlockObjAccess();
	if (pj)
	{
		pj->stop();
		delete pj;
	}
	// close the byte-order-mark detector.
	delete _bomf;

	// stop any active scan.
	OnScanTagFinish(2);
	// save user preference settings that might have been changed.
	if (_bhvp == BHVP_SHELL)
	{
		Registry_SetDwordValue(HKEY_CURRENT_USER, LIBREGKEY, L"FixFontSize", _oneFontSizeForAllDisplays ? TRUE : FALSE);
		Registry_SetDwordValue(HKEY_CURRENT_USER, LIBREGKEY, L"FontSize", _vi.FontSize);
		Registry_SetDwordValue(HKEY_CURRENT_USER, LIBREGKEY, L"BytesPerRow", _vi.BytesPerRow);
	}
	else
	{
		Registry_SetDwordValue(HKEY_CURRENT_USER, LIBREGKEY2, L"BytesPerRow", _vi.BytesPerRow);
	}
	BinhexView::term();
	_ChildWindow::OnDestroy();
}

/* OnSize - responds to a window resize. saves the new window dimensions. adjusts the scrollbar layout if that's necessary.
*/
void BinhexDumpWnd::OnSize(UINT_PTR nType, int cx, int cy)
{
	_ChildWindow::OnSize(nType, cx, cy);

	if (nType == SIZE_MINIMIZED)
		return;

	if (_flags & BHDW_FLAG_UIBUSY)
		return;

	_vi.FrameSize.cx = cx;
	_vi.FrameSize.cy = cy;

	// raise the busy flag. configScrollbars will forego a scroll api call which if issued would cause a recursive call and cause a stack overflow.
	_flags |= BHDW_FLAG_UIBUSY;
	if (configScrollbars(cx, cy))
		SetTimer(_hwnd, TIMERID_DELAYCONFIGSCROLLBARS, 1000, NULL);
	_flags &= ~BHDW_FLAG_UIBUSY;
}

/*
BOOL BinhexDumpWnd::OnNotify(LPNMHDR pNmHdr, LRESULT& lResult)
{
	return _ChildWindow::OnNotify(pNmHdr, lResult);
}
*/

/* propsheetApplyNotify - responds to a click on OK or Apply in the property sheet.
- saves the meta tags and other meta resources in a data file in %LocalAppData% (or %TMP%).
*/
void BinhexDumpWnd::propsheetApplyNotify(bool appTerminating)
{
	// save metadata (regions, etc.) the user added to his file.
	if (appTerminating)
		_flags |= PMFLAG_APP_TERMINATING;
	persistMetafile pmf(!appTerminating, _filename);
	HRESULT hr = pmf.save(_regions, _annotations, _shapes, &_fad, _pmflags);
	if (hr == S_OK)
		_paramMetafileId = pmf.getFileId();
	/* if a print job is in progress, don't worry. allow the termination to initiate. our OnDestroy will wait for the print job to finish.
	if (_printJob)
	{
	}
	*/
}

/* propsheetQueryCancel - responds to a click on Cancel in the property sheet.
- posts a message box asking the user if it's okay to stop a print job if one is still running.
- deletes work files created in annotation operations.
*/
BOOL BinhexDumpWnd::propsheetQueryCancel()
{
	lockObjAccess();
	bool res = (!_printJob);
	unlockObjAccess();
	if (!res)
	{
		// no print job is in progress. ask the user if it's okay to exit.
		if (IDYES != MessageBox(ustring2(IDS_ASK_CANCEL_PRINT), ustring2(IDS_PROPPAGE), MB_YESNO))
			return FALSE; // don't let user close the app.
		// user says it's ok to cancel the print job.
		lockObjAccess();
		if (_printJob)
			_printJob->stop(true);
		unlockObjAccess();
	}
	if (!(_flags & BHDW_FLAG_OPENING_NEW_WINDOW))
	{
		// do cleanup.
		_annotations.cleanUp();
		if (_paramMetafileId)
		{
			ustring pmf = persistMetafile::queryMetafile(_paramMetafileId, true);
			DeleteFile(pmf);
		}
	}
	return TRUE; // ok to close the app.
}

/* OnCommand - processes commands issued by the user or generated programmatically.
note that the commands of IDM_VKEY_* are from the keyboard accelerator.
*/
BOOL BinhexDumpWnd::OnCommand(WPARAM wParam, LPARAM lParam)
{
	LPCWSTR resultMsg = NULL;
	switch (LOWORD(wParam))
	{
#ifdef APP_USES_HOTKEYS
	case IDM_VKEY_LEFT:
		OnHScroll(SB_LINEUP, 0, NULL);
		return TRUE;
	case IDM_VKEY_RIGHT:
		OnHScroll(SB_LINEDOWN, 0, NULL);
		return TRUE;
	case IDM_VKEY_PRIOR:
		if (OnVScroll(SB_PAGEUP, 0, NULL))
		{
			_vi.SelectionOffset.QuadPart -= _vi.BytesPerRow*_vi.RowsPerPage;
			if (_vi.SelectionOffset.QuadPart < 0)
				_vi.SelectionOffset.QuadPart = 0;
		}
		return TRUE;
	case IDM_VKEY_NEXT:
		if (OnVScroll(SB_PAGEDOWN, 0, NULL))
		{
			_vi.SelectionOffset.QuadPart += _vi.BytesPerRow*_vi.RowsPerPage;
			if (_vi.SelectionOffset.QuadPart > (LONGLONG)_fri.FileSize.QuadPart)
				_vi.SelectionOffset.QuadPart = (LONGLONG)(_fri.FileSize.QuadPart - _vi.BytesPerRow);
		}
		return TRUE;
	case IDM_VKEY_UP:
		if (OnVScroll(SB_LINEUP, 0, NULL))
		{
			_vi.SelectionOffset.QuadPart -= _vi.BytesPerRow;
			if (_vi.SelectionOffset.QuadPart < 0)
				_vi.SelectionOffset.QuadPart = 0;
		}
		return TRUE;
	case IDM_VKEY_DOWN:
		if (OnVScroll(SB_LINEDOWN, 0, NULL))
		{
			_vi.SelectionOffset.QuadPart += _vi.BytesPerRow;
			if (_vi.SelectionOffset.QuadPart > (LONGLONG)_fri.FileSize.QuadPart)
				_vi.SelectionOffset.QuadPart -= _vi.BytesPerRow;
		}
		return TRUE;
	case IDM_VKEY_HOME:
		if (OnVScroll(SB_TOP, 0, NULL))
			_vi.SelectionOffset.QuadPart = 0;
		return TRUE;
	case IDM_VKEY_END:
		if (OnVScroll(SB_BOTTOM, 0, NULL))
		{
			if (_vi.RowsTotal != 0)
				_vi.SelectionOffset.QuadPart = (_vi.RowsTotal - 1)*_vi.BytesPerRow;
			else
				_vi.SelectionOffset.QuadPart = 0;
		}
		return TRUE;
#endif//#ifdef APP_USES_HOTKEYS

	case IDM_SCANTAG_START:
		OnScanTagStart();
		return TRUE;
	case IDM_SCANTAG_FINISH:
		// HIWORD has a status code. it's zero if the scan was successful.
		OnScanTagFinish(HIWORD(wParam));
		invalidate();
		return TRUE;
	case IDM_FINDPREV_METAITEM:
	case IDM_FINDNEXT_METAITEM:
		OnGotoNextMetaItem(LOWORD(wParam) == IDM_FINDPREV_METAITEM);
		return TRUE;

	case IDM_PRINT_EVENT_NOTIF:
		if (_printJob)
		{
			HexdumpPrintJob *pj;
			ustring s;
			HDCPrintTask tsk = (HDCPrintTask)LOWORD(lParam);
			if (_notifHwnd)
				::PostMessage(_notifHwnd, WM_COMMAND, _notifCmdId, lParam);
			switch (tsk)
			{
			case TskStartPage:
				// HIWORD(lParam) has the page number of the page being printed.
				DBGPRINTF((L"PrintJob starting page %d\n", HIWORD(lParam)));
				break;
			case TskFinishJob:
			case TskAbortJob:
				pj = (HexdumpPrintJob*)InterlockedExchangePointer((LPVOID*)&_printJob, NULL);
				if (pj)
				{
					pj->stop();
					delete pj;
					DBGPUTS((tsk == TskFinishJob ? L"PrintJob finished successfully." : L"PrintJob aborted."));
				}
				break;
			}
		}
		return TRUE;
	case IDM_PRINTDATA:
		OnPrintData();
		return TRUE;
	case IDM_SAVEDATA:
		OnSaveData();
		return TRUE;
	case IDM_FINDDATA:
		OnFindData();
		return TRUE;
	case IDM_FINDPREV:
		if (_fc.SearchString._length == 0)
		{
			_fc.Flags |= FINDCONFIGFLAG_DIR_BACKWARD;
			OnFindData();
		}
		else
			searchAndShow(true);
		return TRUE;
	case IDM_FINDNEXT:
		if (_fc.SearchString._length == 0)
		{
			_fc.Flags &= ~FINDCONFIGFLAG_DIR_BACKWARD;
			OnFindData();
		}
		else
			searchAndShow(false);
		return TRUE;

	case IDM_OPEN_NEW_WINDOW:
		OnOpenNewWindow();
		if (_bhvp == BHVP_SHELL)
		{
			// issue a cancel and close the propsheet. we've started our own hexdumpdlg app to take over.
			// turn this flag on to skip _annotations.cleanUp in propsheetQueryCancel.
			_flags |= BHDW_FLAG_OPENING_NEW_WINDOW;
			//TODO: should stop this if another tab (that belongs to someone else) has made a change and has a need for asking the user if it's okay to cancel?
			::PropSheet_PressButton(GetParent(_hparent), PSBTN_CANCEL);
		}
		return TRUE;
	case IDM_SAVEMETA:
		propsheetApplyNotify(false);
		return TRUE;

	case IDM_BPL_8:
		OnSelectBPR(8);
		return TRUE;
	case IDM_BPL_16:
		OnSelectBPR(16);
		return TRUE;
	case IDM_BPL_32:
		OnSelectBPR(MAX_HEX_BYTES_PER_ROW);
		return TRUE;
	case IDM_SAME_FONTSIZE:
		if (_oneFontSizeForAllDisplays)
			_oneFontSizeForAllDisplays = false;
		else
			_oneFontSizeForAllDisplays = true;
		OnSelectBPR(_vi.BytesPerRow);
		return TRUE;

	case IDM_FADEOUT_ANNOTATIONS:
		invalidate();
		_annotations.startFadeoutDelay();
		return TRUE;

	case IDM_ADD_TAG:
		OnAddTag();
		return TRUE;
	case IDM_ANNOTATE:
		OnAnnotate(lParam == LPARAM_FROM_SENDCOMMAND);
		return TRUE;
	case IDM_DRAW_LINE:
		OnDrawShape(LOWORD(wParam), DUMPSHAPEINFO_SHAPE_LINE, lParam == LPARAM_FROM_SENDCOMMAND);
		return TRUE;
	case IDM_DRAW_IMAGE:
		OnDrawShape(LOWORD(wParam), DUMPSHAPEINFO_SHAPE_IMAGE, lParam == LPARAM_FROM_SENDCOMMAND);
		return TRUE;
	case IDM_DRAW_RECTANGLE:
		OnDrawShape(LOWORD(wParam), DUMPSHAPEINFO_SHAPE_RECTANGLE, lParam == LPARAM_FROM_SENDCOMMAND);
		return TRUE;
	case IDM_DRAW_CIRCLE:
		OnDrawShape(LOWORD(wParam), DUMPSHAPEINFO_SHAPE_ELLIPSE, lParam == LPARAM_FROM_SENDCOMMAND);
		return TRUE;
	case IDM_REMOVE:
		OnRemoveMetadata();
		return TRUE;
	case IDM_REMOVE_ALL:
		OnRemoveAllTags(true);
		return TRUE;
	case IDM_CLEAN_ALL:
		OnCleanAll(true);
		return TRUE;

	case IDM_LINE_THICK1:
		OnLineThickness(1);
		return TRUE;
	case IDM_LINE_THICK2:
		OnLineThickness(2);
		return TRUE;
	case IDM_LINE_THICK4:
		OnLineThickness(4);
		return TRUE;
	case IDM_LINE_STRAIGHT:
		OnLineType(0);
		return TRUE;
	case IDM_LINE_WAVY:
		OnLineType(DUMPSHAPEINFO_OPTION_WAVY);
		return TRUE;
	case IDM_LINE_ARROW:
		OnLineType(DUMPSHAPEINFO_OPTION_ARROW);
		return TRUE;
	case IDM_LINE_ARROW2:
		OnLineType(DUMPSHAPEINFO_OPTION_ARROW2);
		return TRUE;

	case IDM_COLOR_IN_TRANSPARENT:
		OnShapeInteriorTransparent();
		return TRUE;

	case IDM_LIST_SCANNED_TAGS:
		OnListScannedTags(lParam == LPARAM_FROM_SENDCOMMAND? _varResult:NULL);
		return TRUE;

	default:
		if (HIWORD(wParam) == EN_CHANGE)
		{
			if (IDC_ANNOTATION_EDITCTRL0 <= LOWORD(wParam) && LOWORD(wParam) < IDC_ANNOTATION_EDITCTRL0 + _annotations.size())
			{
				_annotations.OnAnnotationEnChange(LOWORD(wParam));
				return TRUE;
			}
		}
		else if (IDM_COLOR_TAG0 <= wParam && wParam <= IDM_COLOR_TAG0 + MAX_DUMPREGIONCOLORS)
		{
			if (_regions.changeColorHere((int)wParam - IDM_COLOR_TAG0))
				invalidate();
			return TRUE;
		}
		else if (IDM_COLOR_OUT0 <= wParam && wParam <= IDM_COLOR_OUT0 + MAX_DUMPGSHAPECOLORS)
		{
			OnShapeOutlineColor((int)wParam - IDM_COLOR_OUT0);
			return TRUE;
		}
		else if (IDM_COLOR_IN0 <= wParam && wParam <= IDM_COLOR_IN0 + MAX_DUMPGSHAPECOLORS)
		{
			OnShapeInteriorColor((int)wParam - IDM_COLOR_IN0);
			return TRUE;
		}
		else if (IDM_GOTO_REGION0 <= wParam && wParam <= IDM_GOTO_REGION0+_regions.size())
		{
			OnGotoRegion((int)wParam - IDM_GOTO_REGION0);
			return TRUE;
		}
		break;
	}
	return _ChildWindow::OnCommand(wParam, lParam);
}

/* OnContextMenu - responds to a left mouse click in the main window by generating and displaying a menu appropriate for the current operating context.
*/
void BinhexDumpWnd::OnContextMenu(HWND hwnd, int x, int y)
{
	HMENU hmenu = LoadMenu(LibResourceHandle, MAKEINTRESOURCE(IDR_VIEWPOPUP));
	HMENU hpopup = GetSubMenu(hmenu, 0);

	// place a check mark on a BPR choice in selection.
	if (_vi.BytesPerRow == 4 * DEFAULT_HEX_BYTES_PER_ROW)
		CheckMenuItem(hpopup, IDM_BPL_32, MF_CHECKED | MF_BYCOMMAND);
	else if (_vi.BytesPerRow == 2 * DEFAULT_HEX_BYTES_PER_ROW)
		CheckMenuItem(hpopup, IDM_BPL_16, MF_CHECKED | MF_BYCOMMAND);
	else // DEFAULT_HEX_BYTES_PER_ROW
		CheckMenuItem(hpopup, IDM_BPL_8, MF_CHECKED | MF_BYCOMMAND);

	// check it if the one-font-for-all-BPRs option is enabled.
	if (_oneFontSizeForAllDisplays)
		CheckMenuItem(hpopup, IDM_SAME_FONTSIZE, MF_CHECKED | MF_BYCOMMAND);

	if (_bhvp == BHVP_SHELL)
	{
		// disable the Print menu command when running in Windows Explorer.
		EnableMenuItem(hpopup, IDM_PRINTDATA, MF_BYCOMMAND | MF_GRAYED);
	}
	else
	{
		DeleteMenu(hpopup, 0, MF_BYPOSITION); // delete the top menu item, IDM_OPEN_NEW_WINDOW
		DeleteMenu(hpopup, 0, MF_BYPOSITION); // delete the first SEPARATOR
		EnableMenuItem(hpopup, IDM_BPL_8, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(hpopup, IDM_SAME_FONTSIZE, MF_BYCOMMAND | MF_GRAYED);
	}
	// remove the problematic metadata cleanup command, unless a Shift key is down.
	if (!(GetKeyState(VK_SHIFT) & 0x8000))
		DeleteMenu(hpopup, IDM_CLEAN_ALL, MF_BYCOMMAND);

	// cannot enable Print if a print job is already running.
	if (_printJob)
		EnableMenuItem(hpopup, IDM_PRINTDATA, MF_BYCOMMAND | MF_GRAYED);

	// locate the mouse cursor and translate its position to an offset into the file content.
	locateContent({ x,y }, _contentAtCursor);
	// raise a flag so that the event handler that responds to the command or an event that the command generates can distinguish it from others as one from the context menu.
	_flags |= BHDW_FLAG_CONTEXTMENU_OPENED;

	// see if the content location maps to a meta object.
	int _shapeHere = _shapes.queryShapeAt({ x, y }, true);
	int _annotationHere = _annotations.queryAnnotationAt({ x, y }, true, true);
	int _regionHere = _regions.queryRegion(_contentAtCursor, false);
	// adjust the menu depending on the meta object association.
	if (_regionHere >= 0 || _annotationHere >= 0 || _shapeHere >= 0)
	{
		// is the cursor over an annotation meta object?
		if (_annotationHere >= 0)
		{
			ustring2 newLabel(IDS_MENULABEL_REMOVECOMMENT);
			MENUITEMINFO mii = { sizeof(MENUITEMINFO), MIIM_STRING | MIIM_ID, MFT_STRING, 0, IDM_REMOVE };
			mii.dwTypeData = newLabel._buffer;
			SetMenuItemInfo(hpopup, IDM_REMOVE, FALSE, &mii);
		}
		// is the cursor on a shape meta object?
		else if (_shapeHere >= 0)
		{
			ustring2 newLabel(IDS_MENULABEL_REMOVEDRAWING);
			MENUITEMINFO mii = { sizeof(MENUITEMINFO), MIIM_STRING | MIIM_ID, MFT_STRING, 0, IDM_REMOVE };
			mii.dwTypeData = newLabel._buffer;
			SetMenuItemInfo(hpopup, IDM_REMOVE, FALSE, &mii);

			DUMPSHAPEINFO &shapeInfo = _shapes[_shapeHere];

			// get the Draw submenu.
			HMENU hsubmenuDraw = GetSubMenu(hpopup, MENUITEMPOS_DRAW);
			if (hsubmenuDraw)
			{
				// enable the Properties submenu.
				EnableMenuItem(hsubmenuDraw, MENUITEMPOS_DRAW_PROPERTIES, MF_BYPOSITION | MF_ENABLED);
				// get a handle to the Properties submenu.
				HMENU hsubmenuProps = GetSubMenu(hsubmenuDraw, MENUITEMPOS_DRAW_PROPERTIES);
				if (shapeInfo.Shape == DUMPSHAPEINFO_SHAPE_LINE)
				{
					// check 'Line' on the Draw submenu.
					CheckMenuItem(hsubmenuDraw, IDM_DRAW_LINE, MF_BYCOMMAND | MF_CHECKED);
					// disable the Interior Color submenu under Properties.
					EnableMenuItem(hsubmenuProps, MENUITEMPOS_DRAW_PROPERTIES_INTERIORCOLOR, MF_BYPOSITION | MF_GRAYED);
					HMENU hsubmenuLine = GetSubMenu(hsubmenuProps, MENUITEMPOS_DRAW_PROPERTIES_LINE);
					if (shapeInfo.StrokeThickness == 4)
						CheckMenuItem(hsubmenuLine, IDM_LINE_THICK4, MF_BYCOMMAND | MF_CHECKED);
					else if (shapeInfo.StrokeThickness == 2)
						CheckMenuItem(hsubmenuLine, IDM_LINE_THICK2, MF_BYCOMMAND | MF_CHECKED);
					else
						CheckMenuItem(hsubmenuLine, IDM_LINE_THICK1, MF_BYCOMMAND | MF_CHECKED);

					if (shapeInfo.GeomOptions == DUMPSHAPEINFO_OPTION_WAVY)
						CheckMenuItem(hsubmenuLine, IDM_LINE_WAVY, MF_BYCOMMAND | MF_CHECKED);
					else if (shapeInfo.GeomOptions == DUMPSHAPEINFO_OPTION_ARROW)
						CheckMenuItem(hsubmenuLine, IDM_LINE_ARROW, MF_BYCOMMAND | MF_CHECKED);
					else if (shapeInfo.GeomOptions == DUMPSHAPEINFO_OPTION_ARROW2)
						CheckMenuItem(hsubmenuLine, IDM_LINE_ARROW2, MF_BYCOMMAND | MF_CHECKED);
					else
						CheckMenuItem(hsubmenuLine, IDM_LINE_STRAIGHT, MF_BYCOMMAND | MF_CHECKED);
				}
				else
				{
					// check 'Rectangle' or 'Circle' on the Draw submenu.
					if (shapeInfo.Shape == DUMPSHAPEINFO_SHAPE_RECTANGLE)
						CheckMenuItem(hsubmenuDraw, IDM_DRAW_RECTANGLE, MF_BYCOMMAND | MF_CHECKED);
					else if (shapeInfo.Shape == DUMPSHAPEINFO_SHAPE_ELLIPSE)
						CheckMenuItem(hsubmenuDraw, IDM_DRAW_CIRCLE, MF_BYCOMMAND | MF_CHECKED);
					// disable the 'Line' submenu under Properties.
					EnableMenuItem(hsubmenuProps, MENUITEMPOS_DRAW_PROPERTIES_LINE, MF_BYPOSITION | MF_GRAYED);
					// get a handle to the 'Interior Color' submenu
					HMENU hsubmenuInClr = GetSubMenu(hsubmenuProps, MENUITEMPOS_DRAW_PROPERTIES_INTERIORCOLOR);
					if (hsubmenuInClr)
					{
						if (!(shapeInfo.Flags & DUMPSHAPEINFO_FLAG_FILLED))
							CheckMenuItem(hsubmenuInClr, IDM_COLOR_IN_TRANSPARENT, MF_BYCOMMAND | MF_CHECKED);

						// remove the '(none)' top menu item.
						DeleteMenu(hsubmenuInClr, MENUITEMPOS_DRAW_PROPERTIES_INTERIORCOLOR_START, MF_BYPOSITION);
						// insert menu items each showing a color the gshape can take.
						for (int i = 0; i < MAX_DUMPGSHAPECOLORS; i++)
						{
							USHORT curColor = (shapeInfo.InteriorColor == s_gshapeColor[i]) ? 1 : 0;
							MENUITEMINFO mi = { sizeof(MENUITEMINFO) };
							mi.fMask = MIIM_ID | MIIM_FTYPE | MIIM_DATA | MIIM_FTYPE;
							mi.fType = MFT_OWNERDRAW;
							mi.wID = i + IDM_COLOR_IN0;
							mi.dwItemData = MAKELONG(i, curColor);
							InsertMenuItem(hsubmenuInClr, MENUITEMPOS_DRAW_PROPERTIES_INTERIORCOLOR_START, TRUE, &mi);
						}
					}
				}
				HMENU hsubmenuOutClr = GetSubMenu(hsubmenuProps, MENUITEMPOS_DRAW_PROPERTIES_OUTLINECOLOR); // 'Outline Color' submenu
				if (hsubmenuOutClr)
				{
					// remove the '(none)' top menu item.
					DeleteMenu(hsubmenuOutClr, MENUITEMPOS_DRAW_PROPERTIES_OUTLINECOLOR_START, MF_BYPOSITION);
					// insert menu items each showing a color the tag can be shown in.
					for (int i = 0; i < MAX_DUMPGSHAPECOLORS; i++)
					{
						USHORT curColor = (shapeInfo.OutlineColor == s_gshapeColor[i]) ? 1 : 0;
						MENUITEMINFO mi = { sizeof(MENUITEMINFO) };
						mi.fMask = MIIM_ID | MIIM_FTYPE | MIIM_DATA | MIIM_FTYPE;
						mi.fType = MFT_OWNERDRAW;
						mi.wID = i + IDM_COLOR_OUT0;
						mi.dwItemData = MAKELONG(i, curColor);
						InsertMenuItem(hsubmenuOutClr, MENUITEMPOS_DRAW_PROPERTIES_OUTLINECOLOR_START, TRUE, &mi);
					}
				}
				// disable 'Line', 'Rectangle', and 'Circle' on the Draw submenu.
				EnableMenuItem(hsubmenuDraw, IDM_DRAW_LINE, MF_BYCOMMAND | MF_GRAYED);
				EnableMenuItem(hsubmenuDraw, IDM_DRAW_RECTANGLE, MF_BYCOMMAND | MF_GRAYED);
				EnableMenuItem(hsubmenuDraw, IDM_DRAW_CIRCLE, MF_BYCOMMAND | MF_GRAYED);
			}
		}
		// is the cursor over a region meta object?
		else if (_regionHere > 0)
		{
			DUMPREGIONINFO &ri = _regions[_regionHere];
			HMENU hsubmenuClrTags = GetSubMenu(hpopup, MENUITEMPOS_COLORTAGS); // 'Color Tags' submenu
			if (hsubmenuClrTags)
			{
				// remove the '(none)' top menu item.
				DeleteMenu(hsubmenuClrTags, 0, MF_BYPOSITION);
				// insert menu items each showing a color the tag can be shown in.
				for (int i = 0; i < MAX_DUMPREGIONCOLORS; i++)
				{
					USHORT curColor = (ri.BackColor == s_regionInteriorColor[i]) ? 1 : 0;
					MENUITEMINFO mi = { sizeof(MENUITEMINFO) };
					mi.fMask = MIIM_ID | MIIM_FTYPE | MIIM_DATA | MIIM_FTYPE;
					mi.fType = MFT_OWNERDRAW;
					mi.wID = i + IDM_COLOR_TAG0;
					mi.dwItemData = MAKELONG(i, curColor);
					InsertMenuItem(hsubmenuClrTags, 0, TRUE, &mi);
				}
			}
		}
		// can enable the Remove command.
		EnableMenuItem(hpopup, IDM_REMOVE, MF_ENABLED | MF_BYCOMMAND);
	}
	// even if the cursor is not at a meta object, so long as there is a meta object somewhere, we can enable the Remove All command.
	if (_regions.size() > 0 || _annotations.size() > 0 || _shapes.size() > 0)
		EnableMenuItem(hpopup, IDM_REMOVE_ALL, MF_ENABLED | MF_BYCOMMAND);
	// enable the Clean All command if there is one or more work files in %LocalAppData%.
	// note that earlier in this routine, the menu item is deleted unless the user presses a shift key.
	if (countMetafiles() > 0)
		EnableMenuItem(hpopup, IDM_CLEAN_ALL, MF_ENABLED | MF_BYCOMMAND);

	// enable the Scan command if no tag scan has been run. otherwise, replace it with the Scanned Tags command instead. the latter lists up scanned tags in a submenu so that the user can head straight to a desired tag.
	if (canScanTag())
		EnableMenuItem(hpopup, IDM_SCANTAG_START, MF_ENABLED | MF_BYCOMMAND);
	else
		addScannedTagsAsSubmenu(hpopup);

	// run the menu. OnCommand will respond to the selection.
	_menuOnDisplay = true;
	int iResult = TrackPopupMenu(
		hpopup,
		TPM_RIGHTALIGN | TPM_RIGHTBUTTON,
		x, y,
		0,
		hwnd,
		NULL);
	DestroyMenu(hmenu);
	_menuOnDisplay = false;
}

/* addScannedTagsAsSubmenu - generates a submenu populated by meta tags found in a previous scan. 
- hpopup - specifies a parent menu. the submenu is added to the bottom of it.
*/
HRESULT BinhexDumpWnd::addScannedTagsAsSubmenu(HMENU hpopup)
{
	int pos = GetMenuItemCount(hpopup);

	HMENU hsubmenu = CreateMenu();

	for (int i = 0; i < _regions.size(); i++)
	{
		DUMPREGIONINFO &ri = _regions[i];
		if ((ri.Flags & DUMPREGIONINFO_FLAG_TAG))
		{
			int annotId = _annotations.queryByCtrlId(ri.AnnotCtrlId);
			if (annotId != -1)
			{
				ustring label;
				if (AnnotationOperation(_annotations[annotId], &_annotations).formatAsMenuItemLabel(label))
				{
					AppendMenu(hsubmenu, MF_ENABLED | MF_ENABLED, IDM_GOTO_REGION0+i, label._buffer);
				}
			}
		}
	}

	InsertMenu(hpopup, pos, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT_PTR)hsubmenu, ustring2(IDS_SCANNED_TAGS));
	return S_OK;
}

/* canListTags - returns true if at least one meta region is tagged.
*/
bool BinhexDumpWnd::canListTags()
{
	for (int i = 0; i < _regions.size(); i++)
	{
		DUMPREGIONINFO &ri = _regions[i];
		if ((ri.Flags & DUMPREGIONINFO_FLAG_TAG))
			return true;
	}
	return false;
}

/* OnListScannedTags - handles command IDM_LIST_SCANNED_TAGS by generating an automaton-ready list of tags in BSTR mapped to integer command IDs.
this method is invoked when the host hexdumpdlg app issues a IDM_LIST_SCANNED_TAGS to get a listing of meta tags it can show on a dropdown menu from the Tags toolbar button. see onListScannedTags in ToolbarWindow.h of project hexdumpdlg.
*/
void BinhexDumpWnd::OnListScannedTags(VARIANT *varResult)
{
	if (!varResult)
		return;

	// collect meta tags (usually generated in a scan).
	objList<bstring> tags;
	for (int i = 0; i < _regions.size(); i++)
	{
		DUMPREGIONINFO &ri = _regions[i];
		if ((ri.Flags & DUMPREGIONINFO_FLAG_TAG))
		{
			int annotId = _annotations.queryByCtrlId(ri.AnnotCtrlId);
			if (annotId != -1)
			{
				ustring label;
				if (AnnotationOperation(_annotations[annotId], &_annotations).formatAsMenuItemLabel(label))
				{
					bstring *p = new bstring;
					// note we're adding one word to the space allocation to accommodate an associated command id.
					BSTR b = p->alloc(label._length+1);
					// 0th word has the command id.
					b[0] = IDM_GOTO_REGION0 + i;
					// follow the command id with the tag label.
					wcscpy(b + 1, label);
					// add the command+label to the list of tags.
					tags.add(p);
				}
			}
		}
	}

	// turn tags into a safe array of BSTRs compatible with win32 automation.
	SAFEARRAYBOUND rgsabound[1];
	rgsabound[0].lLbound = 0;
	rgsabound[0].cElements = (ULONG)tags.size();
	SAFEARRAY* psa = SafeArrayCreate(VT_BSTR, 1, rgsabound);
	if (!psa)
		return ;
	for (int i = 0; i < (int)tags.size(); ++i)
	{
		((BSTR*)psa->pvData)[i] = tags[i]->detach();
	}
	varResult->vt = VT_ARRAY | VT_BSTR;
	varResult->parray = psa;
}

/* OnHScroll - responds to horizontal scrollbar events.
*/
BOOL BinhexDumpWnd::OnHScroll(int scrollAction, int scrollPos, LRESULT *)
{
	_annotations.abortAnnotationEdit();

	SCROLLINFO si = { /*cbSize=*/sizeof(SCROLLINFO) };

	if (_vi.ColsPerPage >= _vi.ColsTotal)
		return FALSE;

	switch (scrollAction)
	{
	case SB_TOP:
		si.nPos = 0;
		break;
	case SB_LINEUP:
		si.nPos = _vi.CurCol - 1;
		if (si.nPos < 0)
			si.nPos = 0;
		break;
	case SB_LINEDOWN:
		si.nPos = (int)_vi.CurCol + 1;
		if (si.nPos > (int)(_vi.ColsTotal - _vi.ColsPerPage + 2))
			si.nPos = (int)(_vi.ColsTotal - _vi.ColsPerPage + 2);
		break;
	case SB_PAGEUP:
		si.nPos = (int)_vi.CurCol - (int)_vi.ColsPerPage;
		if (si.nPos < 0)
			si.nPos = 0;
		break;
	case SB_PAGEDOWN:
		si.nPos = _vi.CurCol + _vi.ColsPerPage;
		if (si.nPos > (int)(_vi.ColsTotal - _vi.ColsPerPage + 2))
			si.nPos = (int)(_vi.ColsTotal - _vi.ColsPerPage + 2);
		break;
	case SB_BOTTOM:
		si.nPos = (_vi.ColsTotal - _vi.ColsPerPage);
		break;
	case SB_THUMBPOSITION:
		si.nPos = scrollPos;
		//si.nPos = getCurrentScrollPos(SB_HORZ);
		break;
	case SB_THUMBTRACK:
		si.nPos = scrollPos;
		//si.nPos = getCurrentScrollPos(SB_HORZ, SIF_TRACKPOS);
		break;
	default:
		return TRUE;
	}

	si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
	si.nPage = _vi.ColsPerPage;
	si.nMax = _vi.ColsTotal + 1;
	SetScrollInfo(_hwnd, SB_HORZ, &si, TRUE);
	_vi.CurCol = si.nPos;

	InvalidateRect(_hwnd, NULL, TRUE);
	return TRUE;
}

/* OnVScroll - responds to vertical scrollbar events.
*/
BOOL BinhexDumpWnd::OnVScroll(int scrollAction, int scrollPos, LRESULT *)
{
	_annotations.abortAnnotationEdit();

	SCROLLINFO si = { /*cbSize=*/sizeof(SCROLLINFO) };

	switch (scrollAction)
	{
	case SB_TOP:
		si.nPos = 0;
		break;
	case SB_LINEUP:
		si.nPos = _vi.CurRow - 1;
		if (si.nPos < 0)
			si.nPos = 0;
		break;
	case SB_LINEDOWN:
		si.nPos = (int)_vi.CurRow + 1;
		if (si.nPos > (int)_vi.RowsTotal)
			si.nPos = (int)_vi.RowsTotal;
		break;
	case SB_PAGEUP:
		si.nPos = (int)_vi.CurRow - (int)_vi.RowsPerPage;
		if (si.nPos < 0)
			si.nPos = 0;
		break;
	case SB_PAGEDOWN:
		si.nPos = _vi.CurRow + _vi.RowsPerPage;
		if (si.nPos > (int)_vi.RowsTotal)
			si.nPos = (int)_vi.RowsTotal;
		break;
	case SB_BOTTOM:
		si.nPos = _vi.RowsTotal;
		break;
	case SB_THUMBPOSITION:
		//si.nPos = scrollPos;
		si.nPos = getCurrentScrollPos(SB_VERT);
		break;
	case SB_THUMBTRACK:
		//si.nPos = scrollPos;
		si.nPos = getCurrentScrollPos(SB_VERT, SIF_TRACKPOS);
		break;
	default:
		return FALSE;
	}
	if (_vi.CurRow == si.nPos)
		return FALSE;

	si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
	si.nPage = _vi.RowsPerPage;
	si.nMax = _vi.RowsTotal + 1;
	SetScrollInfo(_hwnd, SB_VERT, &si, TRUE); // note it's SB_CTL, not SB_VERT. SB_VERT would not update the scroll bar control we are using in this project.
	_vi.CurRow = si.nPos;
	_fri.HasData = false;
	_fri.DataOffset.LowPart = (ULONG)si.nPos*_vi.BytesPerRow;

	// we don't use ScrollWindow/UpdateWindow.
	// we just clear the view area for simplicity's sake. OnPaint will redraw the entire view.
	InvalidateRect(_hwnd, NULL, TRUE);
	_regions.clearRegionVisited();
	if (scrollAction == SB_PAGEUP || scrollAction == SB_PAGEDOWN || scrollAction == SB_TOP)
		_annotations.startFadeoutDelay();
	return TRUE;
}

/* OnTimer - responds to timer events.
*/
BOOL BinhexDumpWnd::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TIMERID_DELAYTRANSPARENTANNOTATION)
	{
		// show the annotations with a fadeout effect, i.e., by gradually making them disappear.
		_annotations.onFadeoutDelayTimer();
		return TRUE;
	}
	if (nIDEvent == TIMERID_DELAYCONFIGSCROLLBARS)
	{
		// adjust the scrollbars in a delayed fashion to avoid a race condition.
		KillTimer(_hwnd, nIDEvent);
		configScrollbars(_vi.FrameSize.cx, _vi.FrameSize.cy);
		return TRUE;
	}
	if (nIDEvent == TIMERID_DELAYSHOWANNOT)
	{
		// re-select an annotation under the cursor in a delayed fashion.
		KillTimer(_hwnd, nIDEvent);
		_annotations.reselectHere();
		return TRUE;
	}
	if (nIDEvent == TIMERID_DELAYPERSISTANNOT)
	{
		// highlight an annotation marked for persistence in a delayed fashion.
		KillTimer(_hwnd, nIDEvent);
		_annotations.highlightAnnotationByCtrlId(0);
		return TRUE;
	}
	return _ChildWindow::OnTimer(nIDEvent);
}

/* OnLButtonDown - responds to left-button down mouse events.
*/
BOOL BinhexDumpWnd::OnLButtonDown(INT_PTR nButtonType, int x, int y)
{
	_annotations.abortAnnotationEdit();

	_shapes._shapeGrip.Gripped = false;

	// first, the prior selection of an ASCII char gets cleared.
	if (_t2h.Flags & TEXT2HEXMAP_FLAG_CHAR_SELECTED)
	{
		_t2h.Flags &= ~TEXT2HEXMAP_FLAG_CHAR_SELECTED;
		invalidate(&_t2h.RowRect); // limit the clearing to the char's rectangle.
	}
	// if the context menu has been opened, clear the indicator flag.
	if (_flags & BHDW_FLAG_CONTEXTMENU_OPENED)
		_flags &= ~BHDW_FLAG_CONTEXTMENU_OPENED;

	POINT pt = { x, y };

	// get the upper left corner of the view frame in client coordinates.
	RECT rcFrame;
	GetClientRect(_hwnd, &rcFrame);
	InflateRect(&rcFrame, -_vi.FrameMargin.x, -_vi.FrameMargin.y);

	// if the mouse cursor is inside the view frame, 
	// - remove the for-your-info help message, if shown, covering a part of the view, and
	// - clear incomplete selection of a region.
	if (!PtInRect(&rcFrame, pt))
	{
		if (_fyi)
		{
			delete _fyi;
			_fyi = NULL;
		}
		_regions.clearSelection();
		return FALSE;
	}
	// make the cursor position relative to the upper left corner of the view frame.
	pt.y -= rcFrame.top;
	pt.x -= rcFrame.left;
	// see if the help message is being shown.
	if (_fyi)
	{
		// yes, find the content offset corresponding to the cursor position.
		locateContent({ x,y }, _contentAtCursor, false);
		// simulate a button click to issue a pre-assigned command (e.g., IDM_ANNOTATE).
		::PostMessage(_hwnd, WM_COMMAND, MAKEWPARAM(_fyi->_cmdId, BN_CLICKED), 0);
		delete _fyi;
		_fyi = NULL;
		return TRUE;
	}

#ifndef DEBUG_CREGION_SELECT
	// check starting of a drag operation on a shape object.
	if (_shapes.startDrag(pt))
		return TRUE;
	// check grabbing of an annotation object.
	if (_annotations.selectAt(pt))
	{
		SetTimer(_hwnd, TIMERID_DELAYSHOWANNOT, 500, NULL);
		return TRUE;
	}
#endif//#ifndef DEBUG_CREGION_SELECT

	// selecting a region of dump bytes. find the content offset of the cursor. region selection needs it.
	locateContent({ x,y }, _contentAtCursor, false);
	lockObjAccess();
	_regions.startSelection(pt, _fri, _contentAtCursor, this);
	unlockObjAccess();

	return TRUE;
}

/* OnLButtonUp - responds to left-button up mouse events.
- stop an ongoing drag operation on a shape object, annotation object, or region object, or
- draw a box around a character selected in the ASCII dump area.
*/
BOOL BinhexDumpWnd::OnLButtonUp(INT_PTR nButtonType, int x, int y)
{
	// the mouse button is lifted up ending a drag operation and starting a drop.
#ifndef DEBUG_CREGION_SELECT
	// stop the drag operation for a shape object if that's ongoing.
	if (_shapes.stopDrag({ x - _vi.FrameMargin.x, y - _vi.FrameMargin.y }))
	{
		// a shape object has been relocated or stretched. refresh the view.
		invalidate();
		return TRUE;
	}
	// stop it now if the drag operation was for an annotation.
	if (_annotations.stopDrag({ x - _vi.FrameMargin.x, y - _vi.FrameMargin.y }))
		return TRUE; // an annotation has been moved to the new location.
#endif//#ifndef DEBUG_CREGION_SELECT

	POINT pt = { x,y };
	// lock region access in case a print or scan job is in progress. they run on worker threads.
	// stop it if the drag operation is for a region.
	lockObjAccess();
	bool regionSelected = _regions.stopSelection(pt, _fri);
	unlockObjAccess();
	// is no region being dragged?
	if (!regionSelected)
	{
		// no region has been dragged.
		// if the cursor is on an ASCII char in the text dump area, highlight a corresponding two-hex digits in the hex dump area.
		int dy = 0;
		if (2 == locateContent(pt, _contentAtCursor, false, &dy))
		{
			// save the character and its content offset in a member variable. next time the mouse button is clicked, the box drawn around the character is erased.
			_t2h.DataOffset = _contentAtCursor;
			ZeroMemory(_t2h.Text, sizeof(_t2h.Text));
			// translate the content offset to an index into the dump data kept in the block buffer. use the index to read the character value.
			ULONG i = (ULONG)(_t2h.DataOffset.QuadPart - _fri.DataOffset.QuadPart);
			// content in UTF-8 and Unicode requires special handling. a character can consist of multiple bytes. all of the constituent bytes must be picked up.
			if (_bom == TEXT_UTF8 || _bom == TEXT_UTF8_NOBOM)
			{
				// UTF-8
				int bytes1, bytes2;
				int utf8Len = _getUTF8CharSequence(i, _t2h.Text, &bytes1, &bytes2);
				_t2h.DataOffset.QuadPart -= bytes1;
				_t2h.DataLength = utf8Len;
			}
			else if (_bom == TEXT_UTF16 || _bom == TEXT_UTF16_NOBOM)
			{
				// Unicode
				if (i & 1)
				{
					i--;
					_t2h.DataOffset.QuadPart--;
				}
				_t2h.Text[0] = _fri.DataBuffer[i];
				_t2h.Text[1] = _fri.DataBuffer[i + 1];
				_t2h.DataLength = sizeof(WCHAR);
			}
			else
			{
				// ASCII
				_t2h.Text[0] = _fri.DataBuffer[i];
				_t2h.DataLength = 1;
			}

			_t2h.Flags |= TEXT2HEXMAP_FLAG_CHAR_SELECTED;
			// calculate and save the enclosing rectangle. we will use it to draw a red box around the character.
			_t2h.RowRect = getContainingRowRectangle(pt);
			if (dy < 0)
				_t2h.RowRect.top += dy;
			// draw a box around the character.
			invalidate(&_t2h.RowRect);
		}
	}
	DBGPUTS_VERBOSE((L"StopCapture"));
	return TRUE;
}

/* OnMouseMove - responds to mouse cursor movements.
*/
BOOL BinhexDumpWnd::OnMouseMove(UINT state, SHORT x, SHORT y)
{
	POINT pt = { x - _vi.FrameMargin.x, y - _vi.FrameMargin.y };
#ifndef DEBUG_CREGION_SELECT
	// continue with the dragging of a shape object.
	if (_shapes.continueDrag(pt))
		return TRUE;
	//DBGDUMP(("ANNOTATIONS", _annotations.data(), sizeof(DUMPANNOTATIONINFO)*(int)_annotations.size()));
	// continue dragging an annotation object.
	if (_annotations.continueDrag(pt))
		return TRUE;
	// will need to do more if the mouse button is not held down, i.e., only the cursor has been moved to a new location and no object is being dragged around.
	if (!(state & MK_LBUTTON))
	{
		DBGPUTS_VERBOSE((L"OnMouseMove: LBUTTON off"));
		// if the cursor is at one of the grip points, we're done.
		int objId = _shapes.queryShapeGripperAt(pt);
		if (objId >= 0)
		{
			// cursor is at a grip point of a shape object.
			return TRUE;
		}
		// run a hit test for shape or annotation.
		objId = _shapes.queryShapeAt(pt);
		if (objId >= 0)
		{
			// cursor is on a shape object.
			if (_shapes.highlightShape(objId))
				return TRUE;
		}
		// quit if an annotation is being edited by the user.
		if (_annotations.editInProgress())
		{
			DBGPUTS_VERBOSE((L"OnMouseMove: Annotation edit in progress"));
			return TRUE;
		}
		bool res = false;
		// lock annotation access.
		lockObjAccess();
		// is there an annotation directly under the cusor?
		res = _annotations.highlightAnnotation(pt);
		if (!res)
		{
			// no. then, highlight an annotation that belongs to a region the cursor is on.
			if (locateContent(pt, _contentAtCursor, false))
			{
				if ((objId = _regions.queryRegion(_contentAtCursor)) > -1)
					res = _annotations.highlightAnnotationByCtrlId(_regions[objId].AnnotCtrlId);
			}
		}
		unlockObjAccess();
		if (res)
			return TRUE;
		_annotations.clearHighlight();
		_shapes.clear(GSHAPECOLL_CLEAR_SOPSEL);
		return TRUE;
	}
#endif//#ifndef DEBUG_CREGION_SELECT
	// continue with the stretching or shrinking of a region object.
	lockObjAccess();
	_regions.continueSelection({ x,y }, _fri);
	unlockObjAccess();
	return TRUE;
}

/* OnSetCursor - responds to cursor queries.
*/
BOOL BinhexDumpWnd::OnSetCursor(HWND hwnd, UINT nHitTest, UINT message)
{
	DBGPUTS_VERBOSE((L"OnSetCursor"));
	HCURSOR hc = NULL;
	if (IsIconic(_hwnd))
		return FALSE;
	if (_sreg || _printJob)
	{
		// busy scanning for tags or working out a printout.
		hc = LoadCursor(NULL, IDC_WAIT);
	}
	else if (!_menuOnDisplay)
	{
		// if the mouse is on a shape object, let it generate an appropriate cursor shape.
		hc = _shapes.queryCursor();
		// otherwise, if the mouse is on an annotation, get a cursor shape from that.
		if (!hc)
			hc = _annotations.queryCursor();
	}
	// if no special cursor shape is available, use the default arrow shape.
	if (!hc)
		hc = LoadCursor(NULL, IDC_ARROW);
	// update the cursor.
	SetCursor(hc);
	return TRUE;
}

/* OnMeasureItem - responds to WM_MEAREITEM messages coming from the submenu of color samples. see OnContextMenu on how the color submenu is added to the context menu.
- sets the dimensions of each custom drawn menu item to preset dimensions.
*/
BOOL BinhexDumpWnd::OnMeasureItem(UINT CtlID, LPMEASUREITEMSTRUCT pMIS, LRESULT *pResult)
{
	if (pMIS->CtlType == ODT_MENU)
	{
		// does the item belong to the menu range of colors made available to region objects?
		if (IDM_COLOR_TAG0 <= pMIS->itemID && pMIS->itemID <= IDM_COLOR_TAG0 + MAX_DUMPREGIONCOLORS)
		{
			// yes, the menu item is for a color in s_regionBorderColor or s_regionInteriorColor.
			pMIS->itemWidth = _vi.ColWidth * MAX_DUMPREGIONCOLORNAME;
			pMIS->itemHeight = _vi.RowHeight;
			*pResult = TRUE;
			return TRUE;
		}
		// is the menu item a color sample of shape objects?
		if ((IDM_COLOR_IN0 <= pMIS->itemID && pMIS->itemID <= IDM_COLOR_IN0 + MAX_DUMPGSHAPECOLORS) || (IDM_COLOR_OUT0 <= pMIS->itemID && pMIS->itemID <= IDM_COLOR_OUT0 + MAX_DUMPGSHAPECOLORS))
		{
			// yes, the menu item is for a color in s_gshapeColor.
			pMIS->itemWidth = _vi.ColWidth * MAX_DUMPGSHAPECOLORNAME;
			pMIS->itemHeight = _vi.RowHeight;
			*pResult = TRUE;
			return TRUE;
		}
	}
	return FALSE;
}

/* OnDrawItem - responds to WM_DRAWITEM for custom drawn menus.
- draws color samples as menu items of either the region color menu or shape color menu.
*/
BOOL BinhexDumpWnd::OnDrawItem(UINT idCtrl, LPDRAWITEMSTRUCT pDIS, LRESULT *pResult)
{
	if (pDIS->CtlType != ODT_MENU)
		return FALSE;
	// the low part of itemData is the color index of the menu item being drawn.
	USHORT iColor = LOWORD(pDIS->itemData);
	// high part of itemData is 1 if the color is in current use.
	USHORT curColor = HIWORD(pDIS->itemData);

	COLORREF clrBorder, clrInterior;
	ustring label;
	HFONT hfont;
	if (IDM_COLOR_TAG0 <= pDIS->itemID && pDIS->itemID <= IDM_COLOR_TAG0 + MAX_DUMPREGIONCOLORS)
	{
		// the menu item is a color sample for CRegion.
		clrBorder = s_regionBorderColor[iColor];
		clrInterior = s_regionInteriorColor[iColor];
		// generate a label for the menu item. it's the hex digits of the color in RGB the menu item represents.
		label.format(L"(%X)", clrInterior);
		// use a fixed-pitch font for the label.
		hfont = _hfontDump;
	}
	else if ((IDM_COLOR_IN0 <= pDIS->itemID && pDIS->itemID <= IDM_COLOR_IN0 + MAX_DUMPGSHAPECOLORS) || (IDM_COLOR_OUT0 <= pDIS->itemID && pDIS->itemID <= IDM_COLOR_OUT0 + MAX_DUMPGSHAPECOLORS))
	{
		// the menu item is a color sample for GShape.
		// use the same border and interior colors.
		clrBorder = s_gshapeColor[iColor];
		clrInterior = s_gshapeColor[iColor];
		// pick a color name on a list from a string resource. use it as the menu label.
		objList<ustring> *colors = ustring2(IDS_GSHAPE_COLORS).split(',');
		ASSERT(colors->size() == MAX_DUMPGSHAPECOLORS);
		label = *(*colors)[iColor];
		delete colors;
		// use a variable-pitch font for the label.
		hfont = _hfontAnnotation;
	}
	else
		return FALSE;

	int dx = GetSystemMetrics(SM_CXMENUCHECK);
	RECT rc = pDIS->rcItem;
	rc.left += dx;
	rc.right = rc.left + (rc.right - rc.left) / 2;
	COLORREF crText, crBkg;
	if (pDIS->itemState & ODS_SELECTED)
	{
		crText = SetTextColor(pDIS->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
		crBkg = SetBkColor(pDIS->hDC, GetSysColor(COLOR_HIGHLIGHT));
	}
	else
	{
		crText = SetTextColor(pDIS->hDC, GetSysColor(COLOR_MENUTEXT));
		crBkg = SetBkColor(pDIS->hDC, GetSysColor(COLOR_MENU));
	}
	UINT align0 = SetTextAlign(pDIS->hDC, TA_LEFT);
	HFONT hf0 = (HFONT)SelectObject(pDIS->hDC, hfont);
	ExtTextOut(pDIS->hDC, rc.right+_vi.ColWidth/2, rc.top, ETO_OPAQUE, &pDIS->rcItem, label, label._length, NULL);
	// if the menu item is the current color, place a bullet symbol before the color sample strip.
	if (curColor)
	{
		RECT rc2 = pDIS->rcItem;
		rc2.right = rc2.left + dx;
		int ddx = (dx - _vi.ColWidth)/2;
		WCHAR bullet[2];
		bullet[0] = WCHAR_COLORSAMPLE_BULLET; // black square
		bullet[1] = 0;
		ExtTextOut(pDIS->hDC, rc2.left + ddx, rc2.top, ETO_OPAQUE, &rc2, bullet, 1, NULL);
	}
	SelectObject(pDIS->hDC, hf0);
	SetTextColor(pDIS->hDC, crText);
	SetBkColor(pDIS->hDC, crBkg);
	SetTextAlign(pDIS->hDC, align0);

	HPEN hp = CreatePen(PS_SOLID, 1, clrBorder);
	HBRUSH hb = CreateSolidBrush(clrInterior);
	HPEN hp0 = (HPEN)SelectObject(pDIS->hDC, hp);
	HBRUSH hb0 = (HBRUSH)SelectObject(pDIS->hDC, hb);
	Rectangle(pDIS->hDC, rc.left, rc.top + 2, rc.right, rc.bottom - 2);
	SelectObject(pDIS->hDC, hb0);
	SelectObject(pDIS->hDC, hp0);

	*pResult = TRUE;
	return TRUE;
}

/* OnMouseWheel - responds to mouse wheel events by translating the events to corresponding scrollbar events and scrolling the dump page in view.
*/
BOOL BinhexDumpWnd::OnMouseWheel(short nButtonType, short nWheelDelta, int x, int y)
{
	LRESULT lres = 0;
	if (nWheelDelta > 0)
	{
		// wheel is moving forward; scroll up
		while (nWheelDelta > 0)
		{
			OnVScroll(SB_LINEUP, 0, &lres);
			nWheelDelta -= WHEEL_DELTA;
		}
	}
	else if (nWheelDelta < 0)
	{
		while (nWheelDelta < 0)
		{
			OnVScroll(SB_LINEDOWN, 0, &lres);
			nWheelDelta += WHEEL_DELTA;
		}
	}
	return TRUE;
}

/* OnPaint - responds to WM_PAINT view refresh requests.
- delegates the view generation to the BinhexMetaView. the base class invokes the top-level _repaintInternal which directs specific repaint tasks.
*/
BOOL BinhexDumpWnd::OnPaint()
{
	PAINTSTRUCT	ps;
	BeginPaint(_hwnd, (LPPAINTSTRUCT)&ps);

	lockObjAccess();
	BinhexView::beginPaint(ps.hdc);
	// execute a repaint. don't draw a borderline. a borderline has already been drawn since BinhexDumpWnd::Create specified WS_BORDER.
	repaint(BINHEXVIEW_REPAINT_NO_FRAME);
	BinhexView::endPaint();
	unlockObjAccess();

	EndPaint(_hwnd, (LPPAINTSTRUCT)&ps);
	return TRUE;
}

/* _repaintInternal - an override called by base class BinhexMetaView when the latter's repaint is called.
- delegates standard view generation to the base class method of the same name.
- paints in a green color a region of dump bytes found by a prior search.
- draws a red rectangle around a character clicked in the ASCII dump area
*/
HRESULT BinhexDumpWnd::_repaintInternal(HDC hdc, RECT frameRect, UINT flags)
{
	HRESULT hr = BinhexMetaView::_repaintInternal(hdc, frameRect, flags);
	if (SUCCEEDED(hr))
	{
		// draw borderlines around a dump content that's turned up in the latest search.
		if (_fc.Flags & FINDCONFIGFLAG_MATCHFOUND)
		{
			DUMPREGIONINFO regionInfo = { _fc.HitOffset, _fc.SearchString._length, 0, 0xA3E4D7, 0x2E86C1, true };
			CRegionOperation(&regionInfo, this).draw(hdc);
		}
		// draw a red box around a ASCII character clicked by the user.
		if (_t2h.Flags & TEXT2HEXMAP_FLAG_CHAR_SELECTED)
		{
			DUMPREGIONINFO regionInfo = { _t2h.DataOffset, _t2h.DataLength, 0, 0, COLORREF_ORANGE, true };
			CRegionOperation(&regionInfo, this).draw(hdc);
		}
	}
	else
	{
		// an error occurred in the file read
		ShowScrollBar(_hwnd, SB_BOTH, FALSE);
		ustring msg;
		ustring2 errorDescription;
		errorDescription.formatOSMessage(_fri.ErrorCode);
		msg.format(L"FILE COULD NOT BE READ (ERROR CODE %d)", _fri.ErrorCode);
		RECT rc = frameRect;
		InflateRect(&rc, -4, -4);
		DrawText(hdc, msg, msg._length, &rc, 0);
		rc.top += 2 * _vi.RowHeight;
		DrawText(hdc, errorDescription, errorDescription._length, &rc, 0);
	}
	return hr;
}

/* OnRemoveAllTags - processes command IDM_REMOVE_ALL.
- asks the user if it's okay to delete, if confirm is true.
- delete all tags (regions and annotations).
- refresh the view.
*/
void BinhexDumpWnd::OnRemoveAllTags(bool confirm)
{
	if (confirm && IDYES != MessageBox(ustring2(IDS_CONFIRM_REMOVEALL), ustring2(IDS_PROPPAGE), MB_YESNO | MB_DEFBUTTON2))
		return;
	_pmflags &= ~PMFLAG_APP_SCANNED;
	_regions.clear(_annotations);
	InvalidateRect(_hwnd, NULL, TRUE);
}

/* countMetafiles - counts meta files (data files containing meta tag definitions) in %LocalAppData%.
- actually counts the registry entries in the key,
	[HKEY_CURRENT_USER\Software\mtanabe\HexDumpTab\MetaFiles]

a registry entry maps a hexdumped file to a meta file.

example:
"C:\Src\TestData\test20.JPG" = dword:0000da1f

note that the meta file in the example case is named "BITDA1F.tmp" because the the JPG is mapped to file ID 0x0000DA1F.
*/
int BinhexDumpWnd::countMetafiles()
{
	simpleList<propNameValue> list;
	if (ERROR_SUCCESS != Registry_EnumPropEntries(HKEY_CURRENT_USER, LIBREGKEY L"\\MetaFiles", list, 1))
		return -1;
	return (int)list.size();
}

/* locateContent - locates the cursor position in terms of an offset into the content data of the dumped file.

pt - [in] screen coordinates of mouse cursor.
outOffset - [out] the content offset the cursor position corresponds to.
ptInScreenCoords - [optional, in] (defaults to true) the input pt is in screen coordinates. set this to false if pt is in client coordinates.
verticalCorrection - [optional, out] a y-axis correction necessary to locate the starting byte of a group of bytes that represents the character at position pt. if the shown character at pt is encoded in Unicode or UTF-8, there can be multiple source bytes, and the front byte may occur in the prior row. in that case, this parameter will contain a value of negative _vi.RowHeight.

return value:
0 - pt is outside the dump areas.
1 - pt belongs to the hex dump area.
2 - pt belongs to the ascii dump area.
*/
int BinhexDumpWnd::locateContent(POINT pt, LARGE_INTEGER &outOffset, bool ptInScreenCoords, int *verticalCorrection)
{
	if (ptInScreenCoords)
		::ScreenToClient(_hwnd, &pt);
	RECT rcFrame = { 0,0,_size.cx,_size.cy };
	//GetClientRect(_hwnd, &rcFrame);
	InflateRect(&rcFrame, -_vi.FrameMargin.x, -_vi.FrameMargin.y);
	if (PtInRect(&rcFrame, pt))
	{
		pt.y -= rcFrame.top;
		pt.x -= rcFrame.left;
		int col = pt.x - (int)_vi.OffsetColWidth;
		if (col >= 0)
		{
			col /= COL_LEN_PER_DUMP_BYTE * _vi.ColWidth;
			if (_vi.ColsPerPage < _vi.ColsTotal)
			{
				int col2 = (COL_LEN_PER_DUMP_BYTE * col + _vi.CurCol) - (_vi.BytesPerRow * COL_LEN_PER_DUMP_BYTE);
				if (col2 > 0)
					col = col2 + _vi.BytesPerRow;
			}
			int row = pt.y / _vi.RowHeight;
			// does pt belong to the hex dump area?
			if (0 <= col && col < (int)_vi.BytesPerRow)
			{
				outOffset.QuadPart = _fri.DataOffset.QuadPart + row * _vi.BytesPerRow + col;
				DBGPRINTF_VERBOSE((L"locateContent.1: {%d,%d}=>[%d,%d]; offset=%I64d\n", pt.x, pt.y, row, col, outOffset.QuadPart));
				return 1;
			}
			// check if pt belongs to the ascii dump area.
			if (col >= (int)_vi.BytesPerRow)
			{
				// subtract RIGHT_SPACING_COL_LEN + (COL_LEN_PER_DUMP_BYTE- DIGITS_PER_DUMP_BYTE) (= 2) from _vi.CurCol. There is a gap between the hex column and ascii column consisting of 2 space chars. so, the position in the ascii row must be reduced by 2.
				int x = pt.x - _vi.OffsetColWidth + ((_vi.CurCol - RIGHT_SPACING_COL_LEN - (COL_LEN_PER_DUMP_BYTE - DIGITS_PER_DUMP_BYTE)) - COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN)*_vi.ColWidth;
				if (x < 0)
					x = 0;
				if (x > 0)
				{
					col = x / _vi.ColWidth;
					if (col >= (int)_vi.BytesPerRow)
						col = (int)_vi.BytesPerRow - RIGHT_SPACING_COL_LEN;
				}
				else
					col = 0;
				ULONG relativeOffset = row * _vi.BytesPerRow;
				ULONG offset0 = _fri.DataOffset.LowPart + relativeOffset;
				outOffset.QuadPart = offset0 + col;
				SIZE sz = { 0 };
#ifdef _DEBUG
				ustring leadingText;
				int stopIndex = -1;
#endif//_DEBUG
				// content in utf-8 or unicode requires special handling.
				if (_bom == TEXT_UTF8 || _bom == TEXT_UTF8_NOBOM)
				{
					if (col == 0)
					{
						int bytes1, bytes2;
						BYTE utf8[4];
						if (_getUTF8CharSequence(relativeOffset, utf8, &bytes1, &bytes2))
							offset0 -= bytes1;
						outOffset.QuadPart = offset0;
						if (verticalCorrection)
							*verticalCorrection = bytes1 ? -(int)_vi.RowHeight : 0;
#ifdef _DEBUG
						leadingText.assignCp(CP_UTF8, (LPCSTR)utf8);
						stopIndex = -bytes1;
#endif//_DEBUG
					}
					else
					{
						HDC hdc = GetDC(_hwnd);
						HFONT _hf0 = (HFONT)SelectObject(hdc, _hfontDump);
						SetViewportOrgEx(hdc, _vi.FrameMargin.x, _vi.FrameMargin.y, NULL);
						int imax = _vi.BytesPerRow;
						if (offset0 + imax > _fri.FileSize.LowPart)
							imax = _fri.FileSize.LowPart - offset0;
						int prevLen = 0;
						for (int i = 0; i < imax; i++)
						{
							int utf8Len;
							ustring src = _findStartByteUTF8(_fri, relativeOffset, i + 1, &utf8Len);
							if (utf8Len >= (int)_vi.BytesPerRow)
							{
								// new char makes it exceed the row length. it gets wrapped to next row. we cannot add it to current row. we have to quit at the prior utf-8 length.
								outOffset.LowPart = offset0 + prevLen;
#ifdef _DEBUG
								leadingText.assign(src);
								stopIndex = i;
#endif//_DEBUG
								break;
							}
							GetTextExtentPoint32(hdc, src, src._length, &sz);
							if (sz.cx >= x)
							{
								int bytes1, bytes2;
								BYTE utf8[4];
								if (!_getUTF8CharSequence(relativeOffset + prevLen, utf8, &bytes1, &bytes2))
									bytes1 = 0;
								int delta = prevLen - bytes1;
								outOffset.LowPart = offset0 + delta;
								if (verticalCorrection)
									*verticalCorrection = delta < 0 ? -(int)_vi.RowHeight : 0;
#ifdef _DEBUG
								leadingText.assign(src);
								stopIndex = i;
#endif//_DEBUG
								break;
							}
							if (utf8Len > prevLen)
								prevLen = utf8Len;
						}
						SelectObject(hdc, _hf0);
						ReleaseDC(_hwnd, hdc);
					}
				}
				else if (_bom == TEXT_UTF16 || _bom == TEXT_UTF16_NOBOM)
				{
					HDC hdc = GetDC(_hwnd);
					HFONT _hf0 = (HFONT)SelectObject(hdc, _hfontDump);
					SetViewportOrgEx(hdc, _vi.FrameMargin.x, _vi.FrameMargin.y, NULL);
					int imax = _vi.BytesPerRow;
					if (offset0 + imax > _fri.FileSize.LowPart)
						imax = _fri.FileSize.LowPart - offset0;
					int prevLen = 0;
					for (int i = 0; i < imax; i += sizeof(WCHAR))
					{
						ustring src = _findStartByteUnicode(_fri, relativeOffset, i + sizeof(WCHAR));
						GetTextExtentPoint32(hdc, src, src._length, &sz);
						if (sz.cx >= x)
						{
							outOffset.LowPart = offset0 + i;
							if (verticalCorrection)
								*verticalCorrection = 0;
#ifdef _DEBUG
							leadingText.assign(src);
							stopIndex = i;
#endif//_DEBUG
							break;
						}
					}
					SelectObject(hdc, _hf0);
					ReleaseDC(_hwnd, hdc);
				}
				DBGPRINTF_VERBOSE((L"locateContent.2: {%d,%d}=>[%d,%d]; x=%d; offset=%I64X, stop=%d; '%s'; cx=%d\n", pt.x, pt.y, row, col, x, outOffset.QuadPart, stopIndex, (LPCWSTR)leadingText, sz.cx));
				return 2;
			}
		}
	}
	outOffset.QuadPart = -1LL;
	return 0;
}

/* locateContentAtCursor - calculates the content offset of a current cursor.
*/
int BinhexDumpWnd::locateContentAtCursor(LARGE_INTEGER &outOffset)
{
	POINT pt;
	if (!GetCursorPos(&pt))
		return 0;
	return locateContent(pt, outOffset);
}

/* OnCleanAll - cleans the per-user work folder of meta files.
*/
void BinhexDumpWnd::OnCleanAll(bool confirm)
{
	if (confirm && IDYES != MessageBox(ustring2(IDS_CONFIRM_CLEANALL), ustring2(IDS_PROPPAGE), MB_YESNO | MB_DEFBUTTON2))
		return;
	//OnRemoveAllTags(false);
	simpleList<propNameValue> list;
	if (ERROR_SUCCESS != Registry_EnumPropEntries(HKEY_CURRENT_USER, LIBREGKEY L"\\MetaFiles", list))
		return;
	for (size_t i = 0; i < list.size(); i++)
	{
		Registry_DeleteValue(HKEY_CURRENT_USER, LIBREGKEY L"\\MetaFiles", list[i].Name);
		ustring metafile = persistMetafile::queryMetafile(list[i].Value, false);
		DeleteFile(metafile);
	}
}

/* OnOpenNewWindow - processes IDM_OPEN_NEW_WINDOW by running a new instance of hexdumpdlg.exe for the same dumped file.
- builds a pathname for hexdumpdlg.exe.
- builds a command line for starting the exe with configuration settings.
- passes it to ShellExecute.
*/
void BinhexDumpWnd::OnOpenNewWindow(int bytesPerRow)
{
	// assumes that the exe is in the same directory where hexdumptab.dll is.
	WCHAR path[MAX_PATH];
	GetModuleFileName(LibInstanceHandle, path, ARRAYSIZE(path));
	DBGPRINTF((L"OnOpenNewWindow: '%s'\n", path));
	LPWSTR pszTitle = wcsrchr(path, '\\');
	if (!pszTitle++)
		return;
	*pszTitle = 0;
	wcscpy_s(pszTitle, ARRAYSIZE(path) - (int)(INT_PTR)(pszTitle - path), L"hexdumpdlg.exe");
#ifdef _DEBUG
	wcscpy(path, L"C:\\Src\\projects\\cpp\\shell\\hexdumptab\\x64\\Debug\\hexdumpdlg.exe");
#endif//_DEBUG
	ustring2 args(L"\"%s\" %d %d", _filename, bytesPerRow, _paramMetafileId);
	DBGPRINTF((L"ShellExecute Args: %s\n", (LPCWSTR)args));
	ShellExecute(NULL, L"open", path, args, NULL, SW_NORMAL);
}

/* OnSelectBPR - processes commands IDM_BPL_<N>.
*/
void BinhexDumpWnd::OnSelectBPR(int bytesPerRow)
{
	// if the host app is not explorer (meaning it's hexdumpdlg), we need to close the current instance and start a new instance of hexdumpdlg. we tell the new instance to switch to the new BPR in the launch command line.
	if (_bhvp != BHVP_SHELL)
	{
		// save metadata (regions, etc.) the user added to his file.
		persistMetafile mfile(true, _filename);
		if (S_OK == mfile.save(_regions, _annotations, _shapes, &_fad, _pmflags | PMFLAG_APP_TERMINATING))
			_paramMetafileId = mfile.getFileId();
		// start our own property sheet dialog. it will reload us for the new BPL.
		OnOpenNewWindow(bytesPerRow);
		// turn this flag on to skip _annotations.cleanUp in propsheetQueryCancel.
		_flags |= BHDW_FLAG_OPENING_NEW_WINDOW;
		// close the current property sheet.
		::PropSheet_PressButton(GetParent(_hparent), PSBTN_CANCEL);
		return;
	}

	// the host is windows explorer.
	// adopt the new BPR by adjustting display control parameters.
	ULONG oldFontSize = _vi.FontSize;
	ASSERT(bytesPerRow == 8 || bytesPerRow == 16 || bytesPerRow == MAX_HEX_BYTES_PER_ROW);
	switch (bytesPerRow)
	{
	case 8: // DEFAULT_HEX_BYTES_PER_ROW
		_vi.FontSize = DEFAULT_FONT_SIZE;
		break;
	case 16:
		_vi.FontSize = _oneFontSizeForAllDisplays ? DEFAULT_FONT_SIZE : 5;
		break;
	case MAX_HEX_BYTES_PER_ROW:
		_vi.FontSize = _oneFontSizeForAllDisplays ? DEFAULT_FONT_SIZE : 3;
		break;
	default:
		ASSERT(FALSE);
		return;
	}

	term();
	init(_hwnd);

	_vi.BytesPerRow = bytesPerRow;
	_vi.RowsTotal = (UINT)(_fri.FileSize.QuadPart / _vi.BytesPerRow);
	_vi.CurRow = (UINT)(_fri.DataOffset.QuadPart / _vi.BytesPerRow);
	_vi.CurCol = 0;
	_fri.DataOffset.QuadPart = _vi.CurRow*_vi.BytesPerRow;

	if (oldFontSize != _vi.FontSize)
		_annotations.OnFontChange();

	scrollToFileOffset();
}

/* OnDrawShape - processes commands IDM_DRAW_LINE, IDM_DRAW_RECTANGLE, and IDM_DRAW_CIRCLE by creating a requested shape object rendered to a preset format and color configuration.
*/
void BinhexDumpWnd::OnDrawShape(UINT cmdId, BYTE shapeType, bool askWhere)
{
	if (askWhere)
	{
		_fyi = new FyiDlg(_hwnd, cmdId);
		_fyi->Create();
		return;
	}
	if (_contentAtCursor.QuadPart == -1LL)
		return;
	DUMPSHAPEINFO di = { 0 };
	di.Shape = shapeType; // DUMPSHAPEINFO_SHAPE_LINE, etc.
	di.DataOffset = _contentAtCursor;
	di.StrokeThickness = (_vi.FontSize < DEFAULT_FONT_SIZE) ? 1 : 2;

	switch (shapeType)
	{
	case DUMPSHAPEINFO_SHAPE_RECTANGLE:
		di.GeomRadius = _vi.ColWidth - 2; // arc radius for rounded corner or zero for no rounding
		di.GeomSize.cx = 3 * 4 * _vi.ColWidth;
		di.GeomSize.cy = 2 * _vi.RowHeight;
		di.Flags |= DUMPSHAPEINFO_FLAG_FILLED;
		di.OutlineColor = COLORREF_BLUE;
		di.InteriorColor = COLORREF_LTBLUE;
		di.GeomOffset.x = -(int)_vi.ColWidth;
		di.GeomOffset.y = -(int)(_vi.RowHeight >> 1);
		di.GeomPts[0] = { -(int)_vi.ColWidth,-(int)_vi.RowHeight }; // left-top corner
		di.GeomPts[1] = { di.GeomSize.cx,di.GeomSize.cy }; // right-bottom corner
		break;
	case DUMPSHAPEINFO_SHAPE_ELLIPSE:
		di.GeomOffset.x = -(int)_vi.ColWidth;
		di.GeomOffset.y = -(int)(_vi.RowHeight >> 1);
		di.GeomPts[0] = { 0,0 }; // origin
		di.GeomRadius = 2 * _vi.RowHeight; // radius for circle; major axis for ellipse
		di.GeomRadius2 = di.GeomRadius / 2; // minor axis
		di.GeomRotation = 0; // angle in degrees of axis inclination
		di.GeomSize.cx = 3 * 4 * _vi.ColWidth;
		di.GeomSize.cy = 2 * _vi.RowHeight;
		di.Flags |= DUMPSHAPEINFO_FLAG_FILLED;
		di.OutlineColor = COLORREF_RED;
		di.InteriorColor = COLORREF_PINK;
		break;
	case DUMPSHAPEINFO_SHAPE_LINE:
		di.OutlineColor = COLORREF_ORANGE;
		di.GeomSize.cx = 3 * 4 * _vi.ColWidth;
		di.GeomSize.cy = _vi.RowHeight / 2;
		di.GeomOffset.x = (int)(_vi.ColWidth >> 2);
		di.GeomOffset.y = (int)(_vi.RowHeight >> 2);
		di.GeomPts[0] = { 0,0 }; // endpoint in north west
		di.GeomPts[1] = { di.GeomSize.cx,di.GeomSize.cy }; // endpoint in south east
		break;
	case DUMPSHAPEINFO_SHAPE_IMAGE:
		return;
	default:
		ASSERT(FALSE);
		return;
	}

	_shapes.add(di);
	InvalidateRect(_hwnd, NULL, TRUE);
}

/* OnAddTag - processes command IDM_ADD_TAG by interactively collecting layout and descriptive info and creating a pair of colored region and annotation.
*/
void BinhexDumpWnd::OnAddTag()
{
	AddTagDlg dlg(this);
	if (dlg.DoModal() != IDOK)
		return;
	ULONG flags = BMVADDANNOT_FLAG_MODIFIABLE;
	if (dlg._textInGray)
		flags |= BMVADDANNOT_FLAG_GRAYTEXT;
	if (dlg._clrInterior & 0xFF000000)
		flags |= BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND;
	addTaggedRegion(flags, dlg._srcOffset, dlg._srcLength, dlg._clrOutline, dlg._clrInterior,  dlg._note);
	InvalidateRect(_hwnd, NULL, TRUE);
}

/* OnAnnotate - processes command IDM_ANNOTATE.
- posts a for-your-info help message if askWhere is true.
- opens a blank annotation so that the user can enter text.
*/
void BinhexDumpWnd::OnAnnotate(bool askWhere)
{
	if (askWhere)
	{
		_fyi = new FyiDlg(_hwnd, IDM_ANNOTATE);
		_fyi->Create();
		return;
	}
	_annotations.addAnnotation(_contentAtCursor);
}

/* OnRemoveMetadata - processes command IDM_REMOVE.
the actual meta object deleted is the object that the cursor was sitting on before the context menu was opened.
*/
void BinhexDumpWnd::OnRemoveMetadata()
{
	// delete a shape object at the cursor?
	if (_shapes.removeHere())
	{
		InvalidateRect(_hwnd, NULL, TRUE);
		return;
	}
	// delete an annotation at the cursor?
	if (_annotations.removeHere())
	{
		InvalidateRect(_hwnd, NULL, TRUE);
		return;
	}
	// delete a region at the cursor?
	if (_regions.removeHere())
		InvalidateRect(_hwnd, NULL, TRUE);
}

/* canScanTag - finds out if the file content can be scanned for tags.
*/
bool BinhexDumpWnd::canScanTag()
{
	// if the file has already been scanned, it may not be scanned again.
	if (_pmflags & PMFLAG_APP_SCANNED)
		return false;
	// if the check has already been run, just use that result.
	if (_flags & BHDW_FLAG_SCANAVAIL_CHECKED)
		return _flags & BHDW_FLAG_SCAN_AVAILABLE ? true : false;
	// turn on the availability-checked flag. also assume scan will be available. it will be reversed if the opposite turns out to be true.
	_flags |= BHDW_FLAG_SCANAVAIL_CHECKED | BHDW_FLAG_SCAN_AVAILABLE;

	// check the availability with external scan servers first.
	if (ScanTagExt::CanScanFile(this))
		return true; // there is a scan server that can service this file.

	// see if the built-in scan service can handle the file. the file must belong with one of the file types supported by the service.
	LPCWSTR fext = getFileExtension();
	if (fext)
	{
		for (int i = 0; i < ARRAYSIZE(ScannableFileExtensions); i++)
		{
			if (0 == _wcsicmp(fext, ScannableFileExtensions[i]))
				return true; // scan is available.
		}
	}
	// the built-in byte-order-mark analyzer can scan simple text files. see if the target file is such.
	UINT resourceId = _bomf->AnalyzeFileType(s_bomdesc, ARRAYSIZE(s_bomdesc));
	if (resourceId)
		return true; // it is a simple text file, and can be scanned for the byte-order-mark and control characters like line feed.

	// the built-in scan service does not support the file. turn off the availability flag.
	_flags &= ~BHDW_FLAG_SCAN_AVAILABLE;
	return false;
}

/* OnScanTagStart - processes command IDM_SCANTAG_START.
- makes sure that there is a scan service, built-in or external, available for the file.
- starts a worker thread and run the scan on the thread.
*/
void BinhexDumpWnd::OnScanTagStart()
{
	ScanTag *sreg = NULL;
	// see if there is an external scan server that can handle this file.
	if (ScanTagExt::CanScanFile(this))
	{
		// an external scan server is available. use it.
		sreg = new ScanTagExt(this);
	}
	else
	{
		// choose a scan service appropriate for the file depending on the file type.
		LPCWSTR fext = getFileExtension();
		if (!fext)
			return;
		if (0 == _wcsicmp(fext, L".png"))
			sreg = new ScanTagPng(this);
		else if (0 == _wcsicmp(fext, L".bmp"))
			sreg = new ScanTagBmp(this);
		else if (0 == _wcsicmp(fext, L".gif"))
			sreg = new ScanTagGif(this);
		else if (0 == _wcsicmp(fext, L".jpg") || 0 == _wcsicmp(fext, L".jpeg") || 0 == _wcsicmp(fext, L".jif"))
			sreg = new ScanTagJpg(this);
		else if (0 == _wcsicmp(fext, L".ico"))
			sreg = new ScanTagIco(this);
		else
		{
			DWORD val = 0;
			Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY L"\\Scan", L"Text.DontWantLF", &val);
			sreg = new ScanTagBOM(this, s_bomdesc, ARRAYSIZE(s_bomdesc), val ? false : true);
		}
	}

	// start scanning the file for tags.
	if (sreg->start())
		_sreg = sreg;
	else
		delete sreg;
}

/* OnScanTagFinish - finishes a scan service started by OnScanTagStart.
*/
void BinhexDumpWnd::OnScanTagFinish(USHORT status)
{
	if (_sreg)
	{
		// 0=success; 1=failed; 2=aborted (see toShortScanStatus)
		if (status != 2)
		{
			ustring msg;
			msg.attach(_sreg->_msg);
			if (msg._length)
				MessageBox(msg, ustring2(IDS_PROPPAGE), status == 0 ? MB_OK : MB_OK | MB_ICONERROR);
		}
		// stop the scan worker.
		_sreg->stop();
		_sreg = NULL;

		if (status == 0)
		{
			// serialize the collected meta tags in a data file in %LocalAppData%.
			propsheetApplyNotify(false);
			// refresh the dump view with a fadeout effect.
			invalidate();
			_annotations.startFadeoutDelay();
			// raise the already-scanned flag to prevent a second.
			_pmflags |= PMFLAG_APP_SCANNED;
		}
	}
}

/* OnPrintData - processes command IDM_PRINTDATA by interactively selecting a printer with the user and starting a scan service.
*/
void BinhexDumpWnd::OnPrintData()
{
	if (_printJob)
		return; // a print job is already in progress. come back after that's done.

	_ms->Enter(); // disable the keyboard accelerator.

	// for the first-time printing, use a default page setup.
	if (!_printSetup)
		_printSetup = new HexdumpPageSetupParams;
	_printSetup->setViewInfo(this);

	// run a print dialog and ask the user to choose a printer and select pages to print.
	HexdumpPrintDlg dlg(_hwnd, this, _printSetup);
	if (S_OK == dlg.choosePrinter())
		_printJob = dlg.startPrinting(); // start printing. keep a reference to the print job.

	_ms->Leave(); // restore the accelerator.
}

/* OnSaveData - processes command IDM_SAVEDATA by either saving the dump page in current view as a bitmap image in a file or copying it as a bitmap image to the clipboard.
- runs a modal dialog with a preview of the currently shown dump page.
- lets the user choose the system clipboard or a file on a disk as the save destination.
*/
void BinhexDumpWnd::OnSaveData()
{
	_ms->Enter();

	DATARANGEINFO dri = { _fri.FileSize, _fri.DataOffset };
	LARGE_INTEGER remainingBytes;
	remainingBytes.QuadPart = (LONGLONG)_fri.FileSize.QuadPart - _fri.DataOffset.QuadPart;
	dri.RangeBytes = _vi.BytesPerRow*_vi.RowsPerPage;
	if (dri.RangeBytes > remainingBytes.LowPart)
		dri.RangeBytes = remainingBytes.LowPart;
	dri.RangeLines = (dri.RangeBytes + _vi.BytesPerRow - 1) / _vi.BytesPerRow;
	//dri.GroupAnnots = true;

	DataRangeDlg dlg(this, &dri, _hwnd);
	if (IDOK == dlg.DoModal())
	{
		HBITMAP hbm = dlg.detachViewBitmap();
		if (dlg._saveDest == DataRangeDlg::SAVEDEST_CLIPBOARD)
		{
			// copy the page as a bitmap image to the clipboard.
			if (OpenClipboard(_hwnd))
			{
				EmptyClipboard();
				SetClipboardData(CF_BITMAP, hbm);
				CloseClipboard();
			}
		}
		else // DataRangeDlg::SAVEDEST_FILE
		{
			// save the page as a bitmap image file.
			WCHAR buf[MAX_PATH];
			if (S_OK == GetSaveAsPath(_hwnd, buf, ARRAYSIZE(buf)))
			{
				BitmapImage bi(hbm);
				if (FAILED(bi.SaveAs(buf)))
					MessageBox(ustring2(IDS_SAVEAS_FAIL), ustring2(IDS_PROPPAGE), MB_OK | MB_ICONERROR);
			}
		}
		DeleteObject(hbm);
	}

	_ms->Leave();
}

/* OnGotoRegion - processes commands IDM_GOTO_REGION0 plus something.
- locates a specified region in the file content.
- scrolls to a page containing the target region.
- show an annotation attached to the region exclusively for a few seconds so that the user can easily see that he has arrived at the requested tag.
*/
void BinhexDumpWnd::OnGotoRegion(int iRegion)
{
	ASSERT(0 <= iRegion && iRegion < _regions.size());
	DUMPREGIONINFO *ri = _regions.gotoItem(iRegion);
	if (!ri)
		return;
	// move the view port to the found region.
	_fri.DataOffset.QuadPart = (ri->DataOffset.QuadPart / _vi.BytesPerRow)*(LONGLONG)_vi.BytesPerRow;
	// update the current position.
	_contentAtCursor.QuadPart = _fri.DataOffset.QuadPart;
	// update the current row number.
	_vi.CurRow = (UINT)(_fri.DataOffset.QuadPart / _vi.BytesPerRow);
	// scroll the view to the new row.
	scrollToFileOffset();
	// show the region and an associated annotation if there is one.
	_annotations.startPersistentAnnotation(ri->AnnotCtrlId, 5);
	SetTimer(_hwnd, TIMERID_DELAYPERSISTANNOT, 500, NULL);
}

/* OnGotoNextMetaItem - responds to requests of IDM_FINDNEXT_METAITEM (goBack==false) and IDM_FINDPREV_METAITEM (goBack==true). brings the next metadata item (e.g., region) into view.
*/
void BinhexDumpWnd::OnGotoNextMetaItem(bool goBack)
{
	// find the region closest to the current location in _contentAtCursor. do the same for the annotations and shapes.
	int iRegion = _regions.queryNextItem(_contentAtCursor, _fri.FileSize, goBack);
	int iAnnot = _annotations.queryNextItem(_contentAtCursor, _fri.FileSize, goBack);
	int iShape = _shapes.queryNextItem(_contentAtCursor, _fri.FileSize, goBack);

	// compute the distance between the closest item and the current content location for each of the three metadata types.
	DUMPREGIONINFO *ri = NULL;
	DUMPANNOTATIONINFO *ai = NULL;
	DUMPSHAPEINFO *si = NULL;
	LONG dxRegion = 0, dxAnnot = 0, dxShape = 0;
	if (iRegion >= 0)
		dxRegion = (LONG)(_regions[iRegion].DataOffset.QuadPart - _contentAtCursor.QuadPart);
	if (iAnnot >= 0)
		dxAnnot = (LONG)(_annotations[iAnnot].DataOffset.QuadPart - _contentAtCursor.QuadPart);
	if (iShape >= 0)
		dxShape = (LONG)(_shapes[iShape].DataOffset.QuadPart - _contentAtCursor.QuadPart);

	// compare the three and see which one is the closest.
	if (goBack)
	{
		// find the previous item closest to the current item.
		// see if that's a region.
		// case 1: the separation between the current position and the immediately prior region is negative. it's the distance relative to the current position. for it to be the closest to the current position, the region's distance must be shorter than those of the other two. note that the test uses a 'greater than' comparison. that's because distance dxRegion is negative. for dxRegion to be shortest (or placing the region closest to the current item), dxRegion must be greater than both dxAnnot and dxShape so long as dxAnnot and dxShape are negative, too. also note that dxRegion is shorter than dxAnnot if dxAnnot is positive. A positive distance means the object is not behind (or 'prior to') the current item but is ahead of (or 'next from') it.
		if ((dxRegion < 0) && (dxRegion >= dxAnnot || dxAnnot >= 0) && (dxRegion >= dxShape || dxShape >= 0))
			ri = _regions.gotoItem(iRegion);
		// case 2: the distance is positive. this is a special case. the current position is at or near the beginning of the file while the prior region is at or near the end of the file.
		else if ((dxRegion > 0) && (dxRegion >= dxAnnot && dxAnnot >= 0) && (dxRegion >= dxShape && dxShape >= 0))
			ri = _regions.gotoItem(iRegion);
		// see if it's an annotation that's closest.
		if ((dxAnnot < 0) && (dxAnnot >= dxRegion || dxRegion >= 0) && (dxAnnot >= dxShape || dxShape >= 0))
			ai = _annotations.gotoItem(iAnnot);
		else if ((dxAnnot > 0) && (dxAnnot >= dxRegion && dxRegion >= 0) && (dxAnnot >= dxShape && dxShape >= 0))
			ai = _annotations.gotoItem(iAnnot);
		// see if it's a shape that's closest.
		if ((dxShape < 0) && (dxShape >= dxRegion || dxRegion >= 0) && (dxShape >= dxAnnot || dxAnnot >= 0))
			si = _shapes.gotoItem(iShape);
		else if ((dxShape > 0) && (dxShape >= dxRegion && dxRegion >= 0) && (dxShape >= dxAnnot && dxAnnot >= 0))
			si = _shapes.gotoItem(iShape);
	}
	else
	{
		// find the next item closest to the current item.
		// see if the closest is the next region.
		// case 1: the distance from the current item to the next region is dxRegion. the distance to the next annotation is dxAnnot. the distance to the next shape is dxShape. if dxRegion is smaller than the other two, the region is closest.
		if ((dxRegion > 0) && (dxRegion <= dxAnnot || dxAnnot <= 0) && (dxRegion <= dxShape || dxShape <= 0))
			ri = _regions.gotoItem(iRegion);
		// case 2: dxRegion is negative. this is a special case. there is no more region ahead of the current item. the current item is the one nearest to the end of the file. the 'next' region is the first one at or near the beginning of the file.
		else if ((dxRegion < 0) && (dxRegion <= dxAnnot && dxAnnot <= 0) && (dxRegion <= dxShape && dxShape <= 0))
			ri = _regions.gotoItem(iRegion);
		// see if the closest is the next annotation.
		if ((dxAnnot > 0) && (dxAnnot <= dxRegion || dxRegion <= 0) && (dxAnnot <= dxShape || dxShape <= 0))
			ai = _annotations.gotoItem(iAnnot);
		else if ((dxAnnot < 0) && (dxAnnot <= dxRegion && dxRegion <= 0) && (dxAnnot <= dxShape && dxShape <= 0))
			ai = _annotations.gotoItem(iAnnot);
		// is the next shape the closest?
		if ((dxShape > 0) && (dxShape <= dxRegion || dxRegion <= 0) && (dxShape <= dxAnnot || dxAnnot <= 0))
			si = _shapes.gotoItem(iShape);
		else if ((dxShape < 0) && (dxShape <= dxRegion && dxRegion <= 0) && (dxShape <= dxAnnot && dxAnnot <= 0))
			si = _shapes.gotoItem(iShape);
	}

	// move the view port to the found metadata item.
	if (ri)
	{
		// the next region is closest. will show it in the first display row.
		_fri.DataOffset.QuadPart = (ri->DataOffset.QuadPart / _vi.BytesPerRow)*(LONGLONG)_vi.BytesPerRow;
	}
	else if (ai)
	{
		// the next annotation is closest.
		_fri.DataOffset.QuadPart = (ai->DataOffset.QuadPart / _vi.BytesPerRow)*(LONGLONG)_vi.BytesPerRow;
	}
	else if (si)
	{
		// the next shape is closest.
		_fri.DataOffset.QuadPart = (si->DataOffset.QuadPart / _vi.BytesPerRow)*(LONGLONG)_vi.BytesPerRow;
	}
	else
		return;

	// update the current position.
	_contentAtCursor.QuadPart = _fri.DataOffset.QuadPart;
	// update the current row number.
	_vi.CurRow = (UINT)(_fri.DataOffset.QuadPart / _vi.BytesPerRow);
	// scroll the view to the new row.
	scrollToFileOffset();
	if (ri)
	{
		// show the region and an associated annotation if there is one.
		_annotations.startPersistentAnnotation(ri->AnnotCtrlId, 5);
		SetTimer(_hwnd, TIMERID_DELAYPERSISTANNOT, 500, NULL);
	}
	else
	{
		// bring all annotations that belong to the new page to full opacity.
		_annotations.startFadeoutDelay();
	}
}

/* OnFindData - processes command IDM_FINDDATA.
- interactively collects a sequence of bytes as a keyword to search for.
- searches the file content for the sequence of bytes.
*/
void BinhexDumpWnd::OnFindData()
{
	_ms->Enter();

	// run a search dialog and ask the user to enter a search keyword (a sequence of bytes).
	FindDlg dlg(&_fc, _hwnd);
	if (IDOK == dlg.DoModal())
	{
		// do the search forward or backward.
		searchAndShow(_fc.Flags & FINDCONFIGFLAG_DIR_BACKWARD);
	}

	_ms->Leave();
}

/* searchAndShow - runs a search forward or backward and scrolls to a page containing the found keyword.
*/
HRESULT BinhexDumpWnd::searchAndShow(bool goBackward)
{
	if (!makeSearch(goBackward))
		return E_FAIL;
	ASSERT(_fc.Flags & FINDCONFIGFLAG_MATCHFOUND);
	_vi.SelectionOffset.QuadPart = _fc.HitOffset.QuadPart;
	if ((_fri.DataOffset.QuadPart <= _fc.HitOffset.QuadPart)
		&& (_fc.HitOffset.QuadPart < _fri.DataOffset.QuadPart + (LONGLONG)_fri.DataLength))
	{
		LARGE_INTEGER delta;
		delta.QuadPart = _fc.HitOffset.QuadPart - _fri.DataOffset.QuadPart;
		ASSERT(delta.HighPart == 0);
		ULONG hitRow = delta.LowPart / _vi.BytesPerRow;
		if (hitRow < _vi.RowsShown)
		{
			// matched data is in the current view.
			InvalidateRect(_hwnd, NULL, TRUE);
			return S_OK;
		}
	}
	_fri.DataOffset.QuadPart = (_fc.HitOffset.QuadPart / _vi.BytesPerRow)*(LONGLONG)_vi.BytesPerRow;
	_vi.CurRow = (UINT)(_fri.DataOffset.QuadPart / _vi.BytesPerRow);
	scrollToFileOffset();
	return S_FALSE;
}

/* makeSearch - searches for a keyword (a sequence of bytes) and puts the result in _fc.
*/
bool BinhexDumpWnd::makeSearch(bool goBackward)
{
	HRESULT hr;
	FILEREADINFO fi = { 0 };

	if (ERROR_SUCCESS != queryFileSize(&fi.FileSize) ||
		fi.FileSize.QuadPart != _fri.FileSize.QuadPart)
	{
		MessageBox(ustring2(IDS_MSG_SEARCHFAIL_FILEMODIFIED), NULL, MB_OK | MB_ICONEXCLAMATION);
		return false;
	}
	fi.DataBuffer = (LPBYTE)malloc(READFILE_BUFSIZE);
	fi.BufferSize = READFILE_BUFSIZE;
	ZeroMemory(fi.DataBuffer, READFILE_BUFSIZE);
	fi.DataOffset = _fri.DataOffset;

	/* move the search starting point to past the current selection, if the previous search turned up a hit. or if it's a backward search, move it to the start of the selection. */
	if (((_fc.Flags & FINDCONFIGFLAG_MATCHFOUND) || goBackward) &&
		(_fri.DataOffset.QuadPart <= _vi.SelectionOffset.QuadPart) &&
		(_vi.SelectionOffset.QuadPart < _fri.DataOffset.QuadPart + (LONGLONG)_fri.DataLength))
	{
		if (goBackward)
			fi.DataOffset.QuadPart = _vi.SelectionOffset.QuadPart;
		else
			fi.DataOffset.QuadPart = _vi.SelectionOffset.QuadPart + _fc.SearchString._length;
	}

	_fc.Flags &= ~FINDCONFIGFLAG_MATCHFOUND;
	fi.Backward = goBackward;

	hr = search(&fi, (LPBYTE)_fc.SearchString._buffer, _fc.SearchString._length, &_fc.HitOffset);

	free(fi.DataBuffer);

	if (hr == S_OK)
	{
		_fc.Flags |= FINDCONFIGFLAG_MATCHFOUND;
		return true;
	}
	if (hr == S_FALSE)
	{
		MessageBox(ustring2(IDS_MSG_SEARCHFAIL_NOMATCH), ustring2(IDS_CAPTION_FINDDATA), MB_OK | MB_ICONEXCLAMATION);
		return false;
	}
	ustring2 errorDescription;
	ustring msg;
	if (hr == E_ACCESSDENIED)
	{
		// win32 CreateFile failed
		ustring2 osError;
		osError.formatOSMessage(fi.ErrorCode);
		errorDescription.format(ustring2(IDS_MSGFMT_ERRORCODE), fi.ErrorCode, (LPCWSTR)osError);
	}
	else
	{
		// search failed due to a win32 FileRead error
		errorDescription.formatOSMessage(GetLastError());
	}
	msg.format(ustring2(IDS_MSGFMT_SEARCHFAIL), (LPCWSTR)errorDescription);
	MessageBox(msg, NULL, MB_OK | MB_ICONEXCLAMATION);
	return false;
}

/* initScrollbarsPos - initializes the vertical and horizontal scrollbars.
*/
void BinhexDumpWnd::initScrollbarsPos()
{
	if (_fri.FileSize.QuadPart == 0)
		queryFileSize(&_fri.FileSize);

	initViewInfo();

	SCROLLINFO si = { /*cbSize=*/sizeof(SCROLLINFO) };
	si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
	si.nPos = _vi.CurRow;
	si.nPage = _vi.RowsPerPage;
	si.nMax = _vi.RowsTotal + 1;
	SetScrollInfo(_hwnd, SB_VERT, &si, TRUE);

	if (_vi.HScrollerVisible)
	{
		si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
		si.nPos = _vi.CurCol;
		si.nPage = _vi.ColsPerPage;
		si.nMax = _vi.ColsTotal + 1;
		SetScrollInfo(_hwnd, SB_HORZ, &si, TRUE);
	}
}

/* scrollToFileOffset - scrolls to a page specified by _vi.CurRow.
*/
void BinhexDumpWnd::scrollToFileOffset()
{
	configScrollbars(_vi.FrameSize.cx, _vi.FrameSize.cy);
	_annotations.stopPersistentAnnotation();

	SCROLLINFO si = { /*cbSize=*/sizeof(SCROLLINFO) };
	si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
	si.nPos = _vi.CurRow;
	si.nPage = _vi.RowsPerPage;
	si.nMax = _vi.RowsTotal + 1;
	SetScrollInfo(_hwnd, SB_VERT, &si, TRUE);

	_fri.HasData = false;
	InvalidateRect(_hwnd, NULL, TRUE);
}

/* configScrollbars - configures the two scrollbars.
*/
bool BinhexDumpWnd::configScrollbars(int cx, int cy, bool updateUI)
{
	SCROLLBARINFO sbiV = { sizeof(SCROLLBARINFO) };
	GetScrollBarInfo(_hwnd, OBJID_VSCROLL, &sbiV);
	SCROLLBARINFO sbiH = { sizeof(SCROLLBARINFO) };
	GetScrollBarInfo(_hwnd, OBJID_HSCROLL, &sbiH);
	_vi.VScrollerVisible = (sbiV.rgstate[0] & STATE_SYSTEM_INVISIBLE) ? false : true;
	_vi.HScrollerVisible = (sbiH.rgstate[0] & STATE_SYSTEM_INVISIBLE) ? false : true;
	bool showVScroll = false;
	bool showHScroll = false;
	bool hideVScroll = false;
	bool hideHScroll = false;
	_vi.VScrollerSize.cx = sbiV.rcScrollBar.right - sbiV.rcScrollBar.left;
	_vi.VScrollerSize.cy = cy;
	_vi.HScrollerSize.cx = cx;
	_vi.HScrollerSize.cy = sbiH.rcScrollBar.bottom - sbiH.rcScrollBar.top;
	// see if a horizontal scrollbar should be shown.
	_vi.ColsPerPage = _vi.FrameSize.cx / _vi.ColWidth - (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN);
	_vi.ColsTotal = 4 * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN;

	if (_vi.ColsPerPage != 0 && _vi.ColsTotal > _vi.ColsPerPage)
		showHScroll = !_vi.HScrollerVisible;
	else if (_vi.HScrollerVisible)
		hideHScroll = true;

	if (_vi.RowsPerPage == 0 || _vi.RowsTotal > _vi.RowsPerPage)
		showVScroll = !_vi.VScrollerVisible;
	else if (_vi.VScrollerVisible)
		hideVScroll = true;

	if (!updateUI)
		return showHScroll | hideVScroll;

	/* when called during a WM_SIZE response, do not call ShowScrollBar. doing so would cause recursive calls leading to a stack overflow. */
	if (_flags & BHDW_FLAG_UIBUSY)
		return showHScroll | showVScroll | hideVScroll | hideHScroll;

	if (showHScroll)
		ShowScrollBar(_hwnd, SB_HORZ, TRUE);
	else if (hideHScroll)
		ShowScrollBar(_hwnd, SB_HORZ, FALSE);

	if (showVScroll)
		ShowScrollBar(_hwnd, SB_VERT, TRUE);
	else if (hideVScroll)
		ShowScrollBar(_hwnd, SB_VERT, FALSE);

	if (showHScroll)
	{
		SCROLLINFO si = { /*cbSize=*/sizeof(SCROLLINFO) };
		si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
		si.nPage = _vi.ColsPerPage;
		si.nMax = _vi.ColsTotal + 1;
		SetScrollInfo(_hwnd, SB_HORZ, &si, TRUE);
	}

	return false;
}

/* getCurrentScrollPos - returns the current scroll position. this is a wrapper around Win32 API function GetScrollInfo.

nBar - [in] scrollbar type; set to SB_HORZ or SB_VERT. SB_VERT is not valid.
fMask - [in] scrollbar parameter to read; should be set to SIF_POS or SIF_TRACKPOS.
*/
int BinhexDumpWnd::getCurrentScrollPos(int nBar, UINT fMask/*=SIF_POS*/)
{
	SCROLLINFO si = { sizeof(SCROLLINFO) };
	si.fMask = fMask;
	if (GetScrollInfo(_hwnd, nBar, &si))
	{
		if (fMask == SIF_TRACKPOS)
			return si.nTrackPos;
		return si.nPos;
	}
	return 0;
}

/* OnLineThickness - changes a shape object's stroke thickness and updates the view.

thickness - [in] stroke thickness in pixels.
*/
void BinhexDumpWnd::OnLineThickness(int thickness)
{
	DUMPSHAPEINFO *si = _shapes.queryHere();
	if (!si)
		return;
	si->StrokeThickness = thickness;
	invalidate();
}

/* OnLineType - changes a line shape object's line type and updates the view.

lineType - [in] set to 0, DUMPSHAPEINFO_OPTION_ARROW, DUMPSHAPEINFO_OPTION_ARROW2, or DUMPSHAPEINFO_OPTION_WAVY.
*/
void BinhexDumpWnd::OnLineType(short lineType)
{
	DUMPSHAPEINFO *si = _shapes.queryHere();
	if (!si)
		return;
	if (si->Shape != DUMPSHAPEINFO_SHAPE_LINE)
		return;
	si->GeomOptions = lineType;
	invalidate();
}

/* OnShapeOutlineColor - changes a shape object's outline color and updates the view.

iColor - [in] 0-based index to an RGB entry in the shape color palette s_gshapeColor.
*/
void BinhexDumpWnd::OnShapeOutlineColor(int iColor)
{
	DUMPSHAPEINFO *si = _shapes.queryHere();
	if (!si)
		return;
	ASSERT(0 <= iColor && iColor < ARRAYSIZE(s_gshapeColor));
	si->OutlineColor = s_gshapeColor[iColor];
	invalidate();
}

/* OnShapeInteriorColor - changes a shape object's interior color and updates the view.

iColor - [in] 0-based index to an RGB entry in the shape color palette s_gshapeColor.
*/
void BinhexDumpWnd::OnShapeInteriorColor(int iColor)
{
	DUMPSHAPEINFO *si = _shapes.queryHere();
	if (!si)
		return;
	ASSERT(0 <= iColor && iColor < ARRAYSIZE(s_gshapeColor));
	si->Flags |= DUMPSHAPEINFO_FLAG_FILLED;
	si->InteriorColor = s_gshapeColor[iColor];
	invalidate();
}

/* OnShapeInteriorTransparent - makes a shape object's interior transparent and updates the view.
making an object transparent means that the object is outlined only, and that its interior is not filled with color allowing the background (i.e., dump digits) to show through.
*/
void BinhexDumpWnd::OnShapeInteriorTransparent()
{
	DUMPSHAPEINFO *si = _shapes.queryHere();
	if (!si)
		return;
	si->Flags &= ~DUMPSHAPEINFO_FLAG_FILLED;
	invalidate();
}

/* queryCurSel - checks with internal flags to see which selectable item is actually in selection. returns the byte length of data turned up in a previous search, or returns 0 if no data is 'in selection'.

selOffset - [out] file offset to the sequence of bytes that matched a keyword in a previous search (i.e., the IDM_FINDDATA command); or file offset to an ASCII dump byte (registered with _t2h) that was previously click selected by the user. if neither condition applies, and if a context menu has just been opened, the content offset to the dump byte at the cursor position before the context menu was opened is passed back to the caller through this parameter. if no context menu has been opened, the content offset to the start of the block of dump bytes in the currently shown page is set to this parameter.
*/
ULONG BinhexDumpWnd::queryCurSel(LARGE_INTEGER *selOffset)
{
	if (_fc.Flags & FINDCONFIGFLAG_MATCHFOUND)
	{
		if (_fc.HitOffset.QuadPart <= _contentAtCursor.QuadPart && _contentAtCursor.QuadPart < _fc.HitOffset.QuadPart + _fc.SearchString._length)
		{
			*selOffset = _fc.HitOffset;
			return _fc.SearchString._length;
		}
	}
	if (_t2h.Flags & TEXT2HEXMAP_FLAG_CHAR_SELECTED)
	{
		*selOffset = _t2h.DataOffset;
		return _t2h.DataLength;
	}
	if (_flags & BHDW_FLAG_CONTEXTMENU_OPENED)
	{
		*selOffset = _contentAtCursor;
		return 0;
	}
	*selOffset = _fri.DataOffset;
	return 0;
}
