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
#include "ROFile.h"
#include "CRegionOperation.h"
#include "AnnotationOperation.h"
#include "GShapeOperation.h"

#define USES_BINHEXMETAVIEW

// addTaggedRegion flags:
// don't fill interior with color.
#define BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND 0x0001
// keep interior half opaque half transparent.
#define BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND 0x0002
// render the text in a header style.
#define BMVADDANNOT_FLAG_HEADER 0x0004
// draw the text in a gray shade.
#define BMVADDANNOT_FLAG_GRAYTEXT 0x0008
// mark the text modifiable.
#define BMVADDANNOT_FLAG_MODIFIABLE 0x0010
// make interior semi-opaque (a bit more transparent than BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND)
#define BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND2 0x0020
// the input text is not a formatting string and contains no variable placeholder.
#define BMVADDANNOT_FLAG_DONT_FORMAT_ANNOT 0x0040
// add the annotation to the summary list of meta tags.
#define BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST 0x0080
// bit mask to isolate the index to a parent annotation.
#define BMVADDANNOT_MASK_PARENTREGION 0xFFFF0000


/* class BinhexMetaView - generates a hexdump view and manages meta objects (e.g., regions and annotations).
*/
class BinhexMetaView : public BinhexView, public ROFile
{
public:
	BinhexMetaView(const BinhexMetaView *bmv);
	BinhexMetaView() : _annotations(this), _shapes(this), _bom(TEXT_UNKNOWN)
	{
		_mutex = CreateMutex(NULL, FALSE, NULL);
	}
	virtual ~BinhexMetaView()
	{
		HANDLE h = InterlockedExchangePointer((LPVOID*)&_mutex, NULL);
		if (h)
			CloseHandle(h);
	}

	TEXTBYTEORDERMARK _bom;
	CRegionCollection _regions;
	AnnotationCollection _annotations;
	GShapeCollection _shapes;

	// helpers for tagging a block of dump bytes with an annotation.
	int addTaggedRegion(ULONG flags, LARGE_INTEGER regionOffset, ULONG regionLen, LPCWSTR annotFormat, ...);
	int addTaggedRegion(ULONG flags, LARGE_INTEGER regionOffset, ULONG regionLen, COLORREF borderColor, COLORREF interiorColor, LPCWSTR annotText);
	int addTaggedRegion(ULONG flags, USHORT labelStart, USHORT labelStop, LARGE_INTEGER regionOffset, ULONG regionLen, LPCWSTR annotText=NULL);
	DUMPANNOTATIONINFO *openAnnotationOfRegion(int regionIndex, bool canCreate=false);

	// locks or unlocks access to meta objects in _regions, _annotations, and _shapes.
	virtual bool lockObjAccess()
	{
		if (!_mutex)
			return false;
		WaitForSingleObject(_mutex, INFINITE);
		return true;
	}
	virtual void unlockObjAccess()
	{
		if (_mutex)
			ReleaseMutex(_mutex);
	}

	// helper override for correcting metrics of Unicode/UTF-8 text in the 'ASCII' text dump area.
	virtual ULONG measureTextDumpRegion(HDC hdc, POINT pt[8], int cx, int cy, LONG hitRow1, LONG hitRow2, LONG hitCol1, LONG hitCol2, int regionInView, LARGE_INTEGER regionStart, ULONG regionLength);
	// stub for allowing code in the class to call an override in a subclass (BinhexDumpWnd).
	virtual ULONG queryCurSel(LARGE_INTEGER *selOffset) { selOffset->QuadPart = 0; return 0; }

protected:
	HANDLE _mutex; // protects _regions and other meta objects from thread contentions.
	astring _utf8; // a buffer of _finishDumpLine to temporarily hold UTF-8 text being processed.

	HRESULT readFileData(ULONG dwReadSize);
	HBITMAP createDIB(HDC hdcSrc = NULL);

	// helpers for view generation.
	virtual HRESULT _repaintInternal(HDC hdc, RECT frameRect, UINT flags);
#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
	virtual void _finishDumpLinePartial(HDC hdc, LPSTR linebuf, ULONG bufSize, ULONG asciiLen, ULONG &ki, UINT cx, UINT ix, UINT iy);
	virtual void _finishDumpLine(HDC hdc, LPSTR linebuf, ULONG bufSize, ULONG &i, UINT cx, UINT iy);
	ULONG _finishDumpLineU(HDC hdc, LPSTR linebuf, ULONG bufSize, WCHAR *src, ULONG fiDataIndex, UINT cx1, UINT cx2, UINT iy);
	// helpers for non-ASCII stream examination.
	ustring _findStartByteUnicode(FILEREADINFO &_fri, ULONG fiDataOffset, ULONG asciiLen);
	ustring _findStartByteUTF8(FILEREADINFO &_fri, ULONG fiDataOffset, ULONG asciiLen, int *utf8Len=NULL);
	int translateUTF8(LPCSTR srcUTF8, int srcLen, LPWSTR destUni, int destSize);
	int _getUTF8CharSequence(LONG fiDataOffset, BYTE utf8seq[4], int *prefxBytes = NULL, int *suffixBytes=NULL);
	WCHAR _getUTF8AsUnicodeChar(LONG fiDataOffset, int *bytesFromBuffer=NULL);
#endif//#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
};


/* BinhexMetaPageView - generates a page view of the hexdump for preview purposes.
*/
class BinhexMetaPageView : public BinhexMetaView
{
public:
	BinhexMetaPageView(const BinhexMetaView *bmv, VIEWINFO *vi2=NULL, FILEREADINFO *fri2=NULL) : BinhexMetaView(bmv)
	{
		if (vi2)
			CopyMemory(&_vi, vi2, sizeof(_vi));
		if (fri2)
			CopyMemory(&_fri, fri2, sizeof(_fri));
	}

	bool init(HWND hwnd, DATARANGEINFO *dri, int fontSize=0);
	HBITMAP paintToBitmap(UINT repaintFlags = BINHEXVIEW_REPAINT_NO_SELECTOR | BINHEXVIEW_REPAINT_NO_FRAME);

	POINT _origPt; // upper left corner of the view frame.
	RECT _pageRect; // frames the whole page which may just be the hexdump rectangle or which may consist of hexdump and an annotation columns.
	RECT _dumpRect; // frames the main hexdump area.
	RECT _annotsRect; // frames right-hand column space where all annotations of the page are stacked up. valid when option 'Group Notes' is enabled.

protected:
	virtual void setupViewport();
	virtual RECT initViewInfo();
};
