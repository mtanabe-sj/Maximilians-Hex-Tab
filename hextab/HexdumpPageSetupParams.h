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
#include "BinhexView.h"

//#define HEXDUMPPRINT_REDUCES_MARGINS_IN_32BPR

// HexdumpPrintDlg can print up to this many copies.
#define HEXDUMP_PRINT_MAX_COPIES 16
// the up-down controls in HexdumpPrintDlg changes a margin by a 100th of an inch a click.
#define HEXDUMP_MARGIN_DELTA 100


const UINT HDPSP_FLAG_CHANGED = 0x00000001;
const UINT HDPSP_FLAG_PRINTBKGDINGRAY_NOT = 0x00010000;
const UINT HDPSP_FLAG_PRINTBKGDINCOLOR = 0x00020000;
const UINT HDPSP_FLAG_PRINTTEXTINCOLOR = 0x00080000;

// fit the dump rendition to the page width
const UINT HDPSP_FLAG_FITTOWIDTH = 0x00000010;
// group annotations in a column outside the dumped bytes
const UINT HDPSP_FLAG_GROUPNOTES = 0x00000020;


struct HexdumpPageSetupParams
{
	// params from struct PAGESETUPDLG
	POINT ptPaperSize; // paper dimensions in 1/1000" or 1/100mm (PSD_INTHOUSANDTHSOFINCHES or PSD_INHUNDREDTHSOFMILLIMETERS)
	RECT rtMinMargin; // minimum margins in 1/1000" or 1/100mm (PSD_INTHOUSANDTHSOFINCHES or PSD_INHUNDREDTHSOFMILLIMETERS)
	RECT rtMargin; // regular margins in 1/1000" or 1/100mm (PSD_INTHOUSANDTHSOFINCHES or PSD_INHUNDREDTHSOFMILLIMETERS)
	// DEVMODE and DEVNAMES allocated by PageSetupDlg().
	HANDLE hDevMode;
	HANDLE hDevNames;
	// select params from DEVMODE
	WCHAR dmDeviceName[CCHDEVICENAME]; // printer name, e.g., "PCL/HP LaserJet"
	WCHAR dmFormName[CCHFORMNAME]; // paper type, e.g., "Letter"
	short dmOrientation; // DMORIENT_PORTRAIT (1) or DMORIENT_LANDSCAPE (2)
	short dmPaperSize; // DMPAPER_LETTER (1), DMPAPER_A4 (9)
	short dmPaperLength; // paper length (height) in 1/10 mm
	short dmPaperWidth; // paper width in 1/10 mm
	// internal params
	int startLineNum, endLineNum;
	int curPage, maxPage, linesPerPage, colsPerPage;
	int requiredCols;
	int fontSize; // font size in points
	short screenColWidth, screenRowHeight;
	SIZE charSz; // character width and height in inches
	SIZE charPelSz; // character width and height in screen pixels
	SIZE printableSz; // printable dimensions of paper minus margins in inches
	SIZE screenPPI; // typically 96 ppi
	UINT flags; // HDPSP_FLAG_*
	int fontSizeFit; // alternate font size for fitting to page width
	double scaleFit; // scaling factor to be used when fitting to page width
	double PV_annotsChars; // how many chars can fit in the annotation column in grouping mode
#ifdef _DEBUG
	// layout parameters from BinhexMetaPageView
	POINT PV_origPt;
	RECT PV_pageRect;
	RECT PV_dumpRect;
	RECT PV_annotsRect;
#endif//#ifdef _DEBUG

	HexdumpPageSetupParams(PRINTDLG *pd = NULL)
	{
		clear();

		if (pd)
		{
			if (pd->hDevNames)
				hDevNames = _cloneGMem(pd->hDevNames);
			if (pd->hDevMode)
			{
				hDevMode = _cloneGMem(pd->hDevMode);
				updateByDevMode(hDevMode);
			}
		}
	}

	void detachTo(HexdumpPageSetupParams &dest)
	{
		dest.clear();
		CopyMemory(&dest, this, sizeof(HexdumpPageSetupParams));
		this->hDevMode = NULL;
		this->hDevNames = NULL;
	}
	void clear()
	{
		ZeroMemory(this, sizeof(HexdumpPageSetupParams));
	}
	void free()
	{
		if (hDevMode)
		{
			GlobalFree(hDevMode);
			hDevMode = NULL;
		}
		if (hDevNames)
		{
			GlobalFree(hDevNames);
			hDevNames = NULL;
		}
		clear();
	}

	void setViewInfo(BinhexMetaView *bmv)
	{
		if (dmOrientation == 0)
		{
			dmOrientation = DMORIENT_PORTRAIT;
			dmPaperSize = DMPAPER_LETTER;
			ptPaperSize = { 8500,11000 }; // 8.5" x 11" for Letter size paper
			rtMargin = { 1000,1000,1000,1000 }; // 1" margin on all 4 sides
			rtMinMargin = { 250,250,250,250 }; // 0.25" margin on all 4 sides
		}

		HDC hdcScrn = GetDC(bmv->_hw);
		screenPPI = { GetDeviceCaps(hdcScrn, LOGPIXELSX), GetDeviceCaps(hdcScrn, LOGPIXELSY) };
		ReleaseDC(bmv->_hw, hdcScrn);

		screenColWidth = bmv->_vi.ColWidth;
		screenRowHeight = bmv->_vi.RowHeight;

		updateViewInfo(&bmv->_vi, false, false);

		if (hDevMode == NULL)
			hDevMode = _initDevMode();
	}
	void updateViewInfo(VIEWINFO *vi, bool fit, bool group, HexdumpLogicalPageViewParams *lp = NULL, int *margins = NULL, int *minMargins = NULL)
	{
		if (fit)
			flags |= HDPSP_FLAG_FITTOWIDTH;
		else
			flags &= ~HDPSP_FLAG_FITTOWIDTH;
		if (group)
			flags |= HDPSP_FLAG_GROUPNOTES;
		else
			flags &= ~HDPSP_FLAG_GROUPNOTES;

		fontSize = vi->FontSize;

		if (margins)
		{
			rtMargin.top = margins[0];
			rtMargin.left = margins[1];
			rtMargin.right = margins[2];
			rtMargin.bottom = margins[3];
		}
		else if (rtMargin.left == 0)
		{
#ifdef HEXDUMPPRINT_REDUCES_MARGINS_IN_32BPR
			if (vi->BytesPerRow < MAX_HEX_BYTES_PER_ROW)
				rtMargin = { 1000,1000,1000,1000 }; // 1" margin on all 4 sides
			else
				rtMargin = { 500,1000,300,1000 }; // 1" margin on all 4 sides
#else//#ifdef HEXDUMPPRINT_REDUCES_MARGINS_IN_32BPR
			rtMargin = { 1000,1000,1000,1000 }; // 1" margin on all 4 sides
#endif//#ifdef HEXDUMPPRINT_REDUCES_MARGINS_IN_32BPR
		}
		if (minMargins)
		{
			rtMinMargin.top = minMargins[0];
			rtMinMargin.left = minMargins[1];
			rtMinMargin.right = minMargins[2];
			rtMinMargin.bottom = minMargins[3];
		}
		else if (rtMinMargin.left == 0)
		{
			rtMinMargin = { 250,250,250,250 }; // 0.25" margin on all 4 sides
		}
		requiredCols = OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + RIGHT_SPACING_COL_LEN + 4 * vi->BytesPerRow;
		printableSz.cy = ptPaperSize.y - rtMargin.top - rtMargin.bottom;
		printableSz.cx = ptPaperSize.x - rtMargin.left - rtMargin.right;
		// RowHeight and ColWidth are in screen pixels. to translate them into lengths in inches, you need a coefficient in inches per pixel. one pixel is 1/96 inch wide provided that the screen resolution is 96 dpi.
		charPelSz.cx = vi->ColWidth;
		charPelSz.cy = vi->RowHeight;
		charSz.cx = MulDiv(vi->ColWidth, HEXDUMP_LOGICAL_TOI, screenPPI.cy);
		charSz.cy = MulDiv(vi->RowHeight, HEXDUMP_LOGICAL_TOI, screenPPI.cx);
		colsPerPage = printableSz.cx / charSz.cx;
		linesPerPage = printableSz.cy / charSz.cy;
		if (lp)
		{
			charPelSz.cx = lrint(lp->scaleCharWidth(screenPPI.cx));
			charPelSz.cy = lrint(lp->scaleCharHeight(screenPPI.cy));
			charSz.cx = lrint(lp->fCharWidth);
			charSz.cy = lrint(lp->fCharHeight);
			colsPerPage = lrint((double)printableSz.cx / lp->fCharWidth);
			linesPerPage = lrint((double)printableSz.cy / lp->fCharHeight);
		}
		/*if (fit)
		{
			scale = (double)colsPerPage / (double)requiredCols;
			linesPerPage = (int)((double)linesPerPage / scale);
		}
		else
			scale = 1.0;*/
		curPage = 1 + vi->CurRow / linesPerPage;
		maxPage = 1 + vi->RowsTotal / linesPerPage;
		// starting and ending lines of current page
		startLineNum = vi->CurRow;
		endLineNum = vi->CurRow+vi->RowsPerPage;
	}

	void update(PAGESETUPDLG &psd)
	{
		ptPaperSize = psd.ptPaperSize;
		rtMargin = psd.rtMargin;
		rtMinMargin = psd.rtMinMargin;
		attachDevInfo(psd.hDevMode, psd.hDevNames);
	}
	void update(PRINTDLG *pd)
	{
		attachDevInfo(pd->hDevMode, pd->hDevNames);

		// query the printer for the dimensions in mm
		SIZE devSz = { GetDeviceCaps(pd->hDC, HORZSIZE), GetDeviceCaps(pd->hDC, VERTSIZE) };
		if (ptPaperSize.x == 0 && ptPaperSize.y == 0)
		{
			// translate to 1/1000"
			ptPaperSize = { MulDiv(100000,devSz.cx,254),MulDiv(100000,devSz.cy,254) };
		}

		// printable paper width in throusandths of an inch
		printableSz.cx = ptPaperSize.x - rtMargin.left - rtMargin.right;
		printableSz.cy = ptPaperSize.y - rtMargin.top - rtMargin.bottom;
	}
	void updateByDevMode(HANDLE hMode)
	{
		DEVMODE *devmode = (DEVMODE *)GlobalLock(hMode);
		wcscpy(dmDeviceName, devmode->dmDeviceName);
		wcscpy(dmFormName, devmode->dmFormName);
		if (devmode->dmFields & DM_PAPERSIZE)
		{
			dmPaperSize = devmode->dmPaperSize;
			if (dmPaperSize == DMPAPER_LETTER)
				ptPaperSize = { 8500, 11000 };
			else if (dmPaperSize == DMPAPER_LEGAL)
				ptPaperSize = { 8500, 14000 };
			else if (dmPaperSize == DMPAPER_A4) // translate mm to 1/1000"
				ptPaperSize = { 8268, 11693 }; // { 210.0*1000.0 / 25.4, 297 * 1000.0 / 25.4 }
			else if (dmPaperSize == DMPAPER_B4) // 250 x 354 mm
				ptPaperSize = { 9843, 13937 }; // { 250.0*1000.0 / 25.4, 354 * 1000.0 / 25.4 }
		}
		if (devmode->dmFields & DM_PAPERLENGTH)
			dmPaperLength = devmode->dmPaperLength;
		if (devmode->dmFields & DM_PAPERWIDTH)
			dmPaperWidth = devmode->dmPaperWidth;
		if (devmode->dmFields & DM_ORIENTATION)
			dmOrientation = devmode->dmOrientation;
		GlobalUnlock(hMode);
	}
	void attachDevInfo(HANDLE &hMode, HANDLE &hNames)
	{
		if (hMode)
		{
			updateByDevMode(hMode);
			if (hDevMode)
				GlobalFree(hDevMode);
			hDevMode = hMode;
			hMode = NULL;
		}
		if (hNames)
		{
			if (hDevNames)
				GlobalFree(hDevNames);
			hDevNames = hNames;
			hNames = NULL;
		}
		flags |= HDPSP_FLAG_CHANGED;
	}
	void savePageLayout(BinhexMetaPageView &pv)
	{
		PV_annotsChars = (double)(pv._annotsRect.right - pv._annotsRect.left) / (double)pv._vi.ColWidth; // non-zero if HDPSP_FLAG_GROUPNOTES is on.
#ifdef _DEBUG
		PV_origPt = pv._origPt;
		PV_pageRect = pv._pageRect;
		PV_dumpRect = pv._dumpRect;
		PV_annotsRect = pv._annotsRect;
#endif//#ifdef _DEBUG

		// fontSizeFit is meaningful when HDPSP_FLAG_FITTOWIDTH is enabled.
		double Nh = requiredCols;
		double Nc = Nh + PV_annotsChars;
		double X = printableSz.cx; // in TOI
		double xc = X / Nc;
		double r = HEXDUMP_LOGICAL_HIGHDEF_CHAR_W2H_RATIO;
		double yc = xc / r; // logical font height in TOI
		double fs = yc * FONT_PTS_PER_INCH / HEXDUMP_LOGICAL_TOI; // font size in pt
		fontSizeFit = lrint(fs);
	}

	void dumpDevInfo()
	{
		if (hDevNames)
		{
			DEVNAMES *dn = (DEVNAMES*)GlobalLock(hDevNames);
			LPCWSTR p = (LPCWSTR)dn;
			DBGPRINTF((L"DevNames: Def=%d; Dev=%s (%s); Driver=%s\n", dn->wDefault, p + dn->wDeviceOffset, p + dn->wOutputOffset, p + dn->wDriverOffset));
			GlobalUnlock(hDevNames);
		}
		if (hDevMode)
		{
			DEVMODE *dm = (DEVMODE*)GlobalLock(hDevMode);
			DBGPRINTF((L"DevMode: Fields=%X; PaperSize=%d (%d x %d); '%s'\n", dm->dmFields, dm->dmPaperSize, dm->dmPaperWidth, dm->dmPaperLength, dm->dmDeviceName));
			GlobalUnlock(hDevMode);
		}
	}

protected:
	/* _initDevMode
	prepares a DEVMODE structure for a custom printer configuration.
	currently, only paper orientation is supported.
	*/
	HGLOBAL _initDevMode()
	{
		HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DEVMODE));
		DEVMODE* pdm = (DEVMODE*)GlobalLock(hg);
		pdm->dmSize = sizeof(DEVMODE);
		pdm->dmFields = DM_ORIENTATION | DM_PAPERSIZE;// | DM_PAPERWIDTH | DM_PAPERLENGTH;
		pdm->dmOrientation = DMORIENT_PORTRAIT;
		pdm->dmPaperSize = DMPAPER_LETTER; // or DMPAPER_A4
		//pdm->dmPaperWidth = 8500;
		//pdm->dmPaperLength = 11000;
		GlobalUnlock(hg);
		return hg;
	}

	HGLOBAL _cloneGMem(HGLOBAL hsrc)
	{
		SIZE_T len = GlobalSize(hsrc);
		HGLOBAL hdest = GlobalAlloc(GPTR, len);
		LPBYTE psrc = (LPBYTE)GlobalLock(hsrc);
		LPBYTE pdest = (LPBYTE)GlobalLock(hdest);
		CopyMemory(pdest, psrc, len);
		GlobalUnlock(hdest);
		GlobalUnlock(psrc);
		return hdest;
	}
};

