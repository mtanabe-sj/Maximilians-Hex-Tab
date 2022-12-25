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
#include "BinhexMetaView.h"
#include "hextabconfig.h"


struct HexdumpPageSetupParams;

class HexdumpPrintJob : public BinhexMetaView
{
public:
	HexdumpPrintJob(PRINTDLG *p_, HexdumpPageSetupParams *s_, BinhexMetaView *v_);
	~HexdumpPrintJob();

	bool start();
	void stop(bool cancelJob=false);

#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
	virtual int _getHexColCoord(int colPos, int deltaNumerator=0, int deltaDenominator=0);
	virtual int _getAscColCoord(int colPos);
	virtual int _getLogicalXCoord(int screenX);
	virtual int _getLogicalYCoord(int screenY);
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT

protected:
	bool _terminated = false;
	HANDLE _thread = NULL;

	HexdumpPageSetupParams *_setup;
	DATARANGEINFO _dri;

	HWND _hwnd;
	HFONT _hfHeader;
	DWORD _printdlgFlags;
	int _startPage, _endPage, _copies;
	int _docId, _pageNum, _pageCount, _printedPages;
	int _fontScale;

	static DWORD WINAPI _runWorker(LPVOID pThis)
	{
		return ((HexdumpPrintJob*)pThis)->runWorker();
	}

	DWORD runWorker();
	bool startDoc(DOCINFO *docInfo);
	void finishDoc();
	bool startPage();
	bool printPage();
	void finishPage();

	void resetViewport();
	bool clipView(RECT *viewRect);
	void releaseClipRegion();

	// BinhexMetaView overrides
	virtual bool _init(HDC hdc);
	virtual void setupViewport();
	void setViewMapping(POINT orig, SIZE exts);
	void printHeader();
	bool _atEndingPage();
};

