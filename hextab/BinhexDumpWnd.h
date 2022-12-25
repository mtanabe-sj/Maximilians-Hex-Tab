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
#include "helper.h"
#include "lib.h"
#include "BinhexMetaView.h"
#include "RWFile.h"
#include "FyiDlg.h"
#include "HexdumpPrintJob.h"
#include "hextabconfig.h"


// this is a special LPARAM value used by callers of SendCommand of IHexTabConfig to identify the call as one originating in an IHexTabConfig client.
#define LPARAM_FROM_SENDCOMMAND 1

// behavior controlling flags of BinhexDumpWnd. assigned to member variable _flags of the class.
#define BHDW_FLAG_UIBUSY 0x0001
#define BHDW_FLAG_CONTEXTMENU_OPENED 0x0002
#define BHDW_FLAG_SCAN_AVAILABLE 0x0004
#define BHDW_FLAG_SCANAVAIL_CHECKED 0x0008
#define BHDW_FLAG_OPENING_NEW_WINDOW 0x0010


/* BinhexDumpWnd - the main window of hexdumptab providing the hexdump view, managing the hexdump operation, and interacting with the user.
rudiments of the window management are handled by base class _ChildWindow.
hexdump view generation is handled by base class BinhexMetaView.
*/
class BinhexDumpWnd :
	public _ChildWindow,
	public BinhexMetaView
{
public:
	BinhexDumpWnd(UIModalState *ms);
	~BinhexDumpWnd();

	static bool _IsRegistered; // _ChildWindow uses this for the Win32 class registration.
	HexdumpPrintJob *_printJob; // maintains a print job in progress.

	// configuration parameters of the 'Find Data' dialog
	struct FINDCONFIG {
		ULONG Flags; // FINDCONFIGFLAG_* flags
		LARGE_INTEGER HitOffset;
		astring SearchString;
		ustring DisplayString;
	};
	// specifies the location of a byte clicked in the ASCII dump area. the byte is enclosed in a rectangle frame drawn in color red. see OnLButtonUp for updating _t2h.
	struct TEXT2HEXMAP {
		ULONG Flags;
		ULONG DataLength;
		LARGE_INTEGER DataOffset;
		RECT RowRect;
		BYTE Text[4];
	};

	BOOL Create(HWND hparent, int cx, int cy);

	bool canScanTag();
	bool canListTags();
	UINT dragQueryFile(HDROP hdrop);
	UINT queryDlgTemplate();
	BOOL propsheetQueryCancel();
	void propsheetApplyNotify(bool appTerminating);
	BOOL processCommand(WPARAM wParam, LPARAM lParam, VARIANT *varResult=NULL)
	{
		_varResult = varResult;
		BOOL res = OnCommand(wParam, lParam);
		_varResult = NULL;
		return res;
	}

	HRESULT HDConfig_SetParam(HDCPID paramId, VARIANT *varValue);

	virtual ULONG queryCurSel(LARGE_INTEGER *selOffset);
	bool getState(ULONG bhdwFlag)
	{
		return (_flags & bhdwFlag);
	}

protected:
	UIModalState *_ms;
	WIN32_FILE_ATTRIBUTE_DATA _fad;
	bool _menuOnDisplay;
	bool _oneFontSizeForAllDisplays;
	LARGE_INTEGER _contentAtCursor;
	FINDCONFIG _fc;
	TEXT2HEXMAP _t2h;
	class ScanTag *_sreg;
	BOMFile *_bomf;
	FyiDlg *_fyi;
	HexdumpPageSetupParams *_printSetup;
	ustring _paramController;
	ULONG _paramMetafileId;
	int _paramBPR;
	HWND _notifHwnd;
	UINT _notifCmdId;
	USHORT _flags; // internal flags of BHDW_FLAG_*
	ULONG _pmflags; // flags passed to persistMetafile::save().
	VARIANT *_varResult; // used by processCommand

	// win32 message handlers
	virtual BOOL OnCreate();
	virtual void OnDestroy();
	virtual void OnSize(UINT_PTR nType, int cx, int cy);
	//virtual BOOL OnNotify(LPNMHDR pNmHdr, LRESULT& lResult);
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	virtual void OnContextMenu(HWND hwnd, int x, int y);
	virtual BOOL OnHScroll(int scrollAction, int scrollPos, LRESULT *pResult);
	virtual BOOL OnVScroll(int scrollAction, int scrollPos, LRESULT *pResult);
	virtual BOOL OnTimer(UINT_PTR nTimerId);
	virtual BOOL OnLButtonDown(INT_PTR nButtonType, int x, int y);
	virtual BOOL OnLButtonUp(INT_PTR nButtonType, int x, int y);
	virtual BOOL OnMouseMove(UINT state, SHORT x, SHORT y);
	virtual BOOL OnSetCursor(HWND hwnd, UINT nHitTest, UINT message);
	virtual BOOL OnMouseWheel(short nButtonType, short nWheelDelta, int x, int y);
	virtual BOOL OnPaint();
	virtual BOOL OnMeasureItem(UINT CtlID, LPMEASUREITEMSTRUCT pMIS, LRESULT *pResult);
	virtual BOOL OnDrawItem(UINT idCtrl, LPDRAWITEMSTRUCT pDIS, LRESULT *pResult);

	// operations
	virtual HRESULT _repaintInternal(HDC hdc, RECT frameRect, UINT flags);
	virtual void resetView()
	{
		BinhexView::resetView();
		scrollToFileOffset();
	}
	SIZE GetViewExtents();
	void SetViewExtents(int cx, int cy);

	bool makeSearch(bool goBackward);
	HRESULT searchAndShow(bool goBackward);
	HRESULT addScannedTagsAsSubmenu(HMENU hpopup);

	bool configScrollbars(int cx, int cy, bool updateUI = true);
	void initScrollbarsPos();
	void scrollToFileOffset();
	int getCurrentScrollPos(int nBar, UINT fMask = SIF_POS);

	int countMetafiles();
	void OnScanTagFinish(USHORT status);
	int locateContent(POINT pt, LARGE_INTEGER &outOffset, bool ptInScreenCoords = true, int *verticalCorrection = NULL);
	int locateContentAtCursor(LARGE_INTEGER &outPos);

	void OnDrawShape(UINT cmdId, BYTE shapeType, bool askWhere); // DUMPSHAPEINFO_SHAPE_*
	void OnAddTag();
	void OnAnnotate(bool askWhere);
	void OnRemoveMetadata();
	void OnRemoveAllTags(bool confirm);
	void OnCleanAll(bool confirm);
	void OnFindData();
	void OnSaveData();
	void OnPrintData();
	void OnScanTagStart();
	void OnSelectBPR(int bytesPerRow);
	void OnOpenNewWindow(int bytesPerRow = 0);

	void OnLineThickness(int thickness);
	void OnLineType(short lineType);
	void OnShapeOutlineColor(int iColor);
	void OnShapeInteriorColor(int iColor);
	void OnShapeInteriorTransparent();

	void OnGotoNextMetaItem(bool goBack);
	void OnGotoRegion(int regionIndex);

	void OnListScannedTags(VARIANT *varResult);
};

