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
#include "BinhexMetaView.h"
#include "libver.h"
#include "resource.h"


//#define TRANSLATEUTF8_DELEGATES_TO_WIN32

////////////////////////////////////////////////////////////
// BinhexMetaView methods

BinhexMetaView::BinhexMetaView(const BinhexMetaView *bmv) :
	_regions(bmv->_regions),
	_annotations(bmv->_annotations),
	_shapes(bmv->_shapes),
	_bom(TEXT_UNKNOWN),
	_mutex(NULL)
{
	CopyMemory(_filename, bmv->_filename, sizeof(_filename));
	CopyMemory(&_vi, &bmv->_vi, sizeof(_vi));
	CopyMemory(&_fri, &bmv->_fri, sizeof(_fri));
	_fri.DataBuffer = NULL;
	_fri.HasData = false;

	_MetafileId = bmv->_MetafileId;
	_annotOpacity = bmv->_annotOpacity;
	_annotFadeoutOpacity = bmv->_annotFadeoutOpacity;
	_shapeOpacity = bmv->_shapeOpacity;
	_rowselectorBorderColor = bmv->_rowselectorBorderColor;
	_rowselectorInteriorColor = bmv->_rowselectorInteriorColor;
}

/* createDIB - generates a bitmap image from the currently shown page of dump bytes.

_repaintInternal calls this method to save the screen. GShapeSelect::highlight uses the screen image to speed up processing.
*/
HBITMAP BinhexMetaView::createDIB(HDC hdcSrc)
{
	if (hdcSrc == NULL)
		hdcSrc = _hdc;
	HDC hdcDest = CreateCompatibleDC(hdcSrc);
	/* this does not work. use a dib section instead.
	_hbmPaint = CreateCompatibleBitmap(hdc, _vi.FrameSize.cx, _vi.FrameSize.cy);
	*/
	BITMAPINFO bmi = { sizeof(BITMAPINFOHEADER), _vi.FrameSize.cx, _vi.FrameSize.cy, 1, 32, BI_RGB, _vi.FrameSize.cx * _vi.FrameSize.cy * sizeof(COLORREF) };
	UINT32* bits = NULL;
	HBITMAP hbm = CreateDIBSection(hdcSrc, &bmi, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, 0);
	HBITMAP hbm0 = (HBITMAP)SelectObject(hdcDest, hbm);
	BitBlt(hdcDest, 0, 0, _vi.FrameSize.cx, _vi.FrameSize.cy, hdcSrc, 0, 0, SRCCOPY);
	SelectObject(hdcDest, hbm0);
	DeleteDC(hdcDest);
	return hbm;
}

#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
/* measureTextDumpRegion - calculates the corner coordinates of a region that surrounds a block of dump bytes as seen in the ASCII text dump area. this override makes corrections to the metrics when the text is encoded in Unicode or UTF-8. passes control to the base class if it's ASCII text.

return value:
number of elements in pt set to corner coordinates of the region.

parameters:
hdc - [in] device context handle
pt - [in,out] positions in client coordinates of the corners of a text region.
cx - [in] horizontal offset in client coordinate of the front of the dump bytes in the ASCII text area.
cy - [in] vertical offset of the front byte.
hitRow1 - [in] 0-based row number of the region's front measured from the top of the page in view.
hitRow2 - [in] row number of the region's back.
hitCol1 - [in] column number of the region's front.
hitCol2 - [in] column number of the region's back.
regionInView - [in] indicates the region's visibility. bits 1, 2, and/or 4 can be turned on.
  1=region's front is visible.
  2=region's back is visible.
  4=region's middle is visible but the front and back are outside current view.
regionStart - [in] byte offset from the top of the data block buffered in the ROFile buffer, as given by (_ri->DataOffset - _vc->_fri.DataOffset).
regionLength - [in] byte length of the region.
*/
ULONG BinhexMetaView::measureTextDumpRegion(HDC hdc, POINT pt[8], int cx, int cy, LONG hitRow1, LONG hitRow2, LONG hitCol1, LONG hitCol2, int regionInView, LARGE_INTEGER regionStart, ULONG regionLength)
{
	// top and bottom borderlines are not visible. show the right and left borderlines only.
	if (regionInView & 4)
		return BinhexView::measureTextDumpRegion(hdc, pt, cx, cy, hitRow1, hitRow2, hitCol1, hitCol2, regionInView, regionStart, regionLength);

	if (_bom == TEXT_UTF8 || _bom == TEXT_UTF8_NOBOM)
	{
		ULONG np = 0;
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		int cx0 = _getAscColCoord(0);
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		int cx2 = (COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN)*_vi.ColWidth;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		int cols;
		SIZE sz0 = { 0 }, sz = { 0 };
		if (hitRow1 == hitRow2)
		{
			if (hitCol1 > 0)
			{
				ustring src0 = _findStartByteUTF8(_fri, regionStart.LowPart - hitCol1, hitCol1);
				GetTextExtentPoint32(hdc, src0, src0._length, &sz0);
				DBGPRINTF((L"measureTextDumpRegion(UTF8).1: sz.cx=%d; %s (%d)\n", sz0.cx, src0._buffer, src0._length));
			}
			cols = hitCol2 - hitCol1;
			if (cols > 0)
			{
				ustring src = _findStartByteUTF8(_fri, regionStart.LowPart, min(regionLength, (ULONG)cols));
				GetTextExtentPoint32(hdc, src, src._length, &sz);
				DBGPRINTF((L"measureTextDumpRegion(UTF8).2: sz.cx=%d; %s (%d)\n", sz.cx, src._buffer, src._length));
			}
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			/*0*/pt[np].x = cx0 + sz0.cx;
			pt[np++].y = cy + hitRow1 * _vi.RowHeight;
			/*1*/pt[np].x = cx0 + sz0.cx + sz.cx;
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			/*0*/pt[np].x = cx + cx2 + sz0.cx;
			pt[np++].y = cy + hitRow1 * _vi.RowHeight;
			/*1*/pt[np].x = cx + cx2 + sz0.cx + sz.cx;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			pt[np++].y = pt[0].y;
			/*2*/pt[np].x = pt[1].x;
			pt[np++].y = pt[1].y + _vi.RowHeight;
			/*3*/pt[np].x = pt[0].x;
			pt[np++].y = pt[2].y;
		}
		else if (hitCol2 == 0)
		{
			// a special case - the region is 6 cornered
			// starting corner given by {hitCol1, hitRow1}.
			if (hitCol1 > 0)
			{
				ustring src0 = _findStartByteUTF8(_fri, regionStart.LowPart - hitCol1, hitCol1);
				GetTextExtentPoint32(hdc, src0, src0._length, &sz0);
			}
			cols = _vi.BytesPerRow - hitCol1;
			if (cols > 0)
			{
				ustring src = _findStartByteUTF8(_fri, regionStart.LowPart, min(regionLength, (ULONG)cols));
				GetTextExtentPoint32(hdc, src, src._length, &sz);
			}
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			/*0*/pt[np].x = cx0 + sz0.cx;
			pt[np++].y = cy + hitRow1 * _vi.RowHeight;
			// right-top corner
			/*1*/pt[np].x = cx0 + sz0.cx + sz.cx;
			pt[np++].y = pt[0].y;
			// right-bottom corner
			/*2*/pt[np].x = pt[1].x;
			pt[np++].y = cy + hitRow2 * _vi.RowHeight;
			// left-bottom corner
			/*3*/pt[np].x = cx0;
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			/*0*/pt[np].x = cx + cx2 + sz0.cx;
			pt[np++].y = cy + hitRow1 * _vi.RowHeight;
			// right-top corner
			/*1*/pt[np].x = cx + cx2 + sz0.cx + sz.cx;
			pt[np++].y = pt[0].y;
			// right-bottom corner
			/*2*/pt[np].x = pt[1].x;
			pt[np++].y = cy + hitRow2 * _vi.RowHeight;
			// left-bottom corner
			/*3*/pt[np].x = cx + cx2;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			pt[np++].y = pt[2].y;
			// left-top(-1) corner
			/*4*/pt[np].x = pt[3].x;
			pt[np++].y = pt[0].y + _vi.RowHeight;
			// move toward starting corner and stay on a row just below it
			/*7*/pt[np].x = pt[0].x;
			pt[np++].y = pt[4].y;
		}
		else
		{
			// general case with a 8-cornered region
			// the ending byte of the region sits between beginning and ending columns.
			if (hitCol1 > 0)
			{
				ustring src0 = _findStartByteUTF8(_fri, regionStart.LowPart - hitCol1, hitCol1);
				GetTextExtentPoint32(hdc, src0, src0._length, &sz0);
			}
			cols = _vi.BytesPerRow - hitCol1;
			if (cols > 0)
			{
				ustring src = _findStartByteUTF8(_fri, regionStart.LowPart, min(regionLength, (ULONG)cols));
				GetTextExtentPoint32(hdc, src, src._length, &sz);
			}
			SIZE sz2 = { 0 };
			if (hitCol2 > 0)
			{
				ustring src2 = _findStartByteUTF8(_fri, regionStart.LowPart + regionLength - hitCol2, hitCol2);
				GetTextExtentPoint32(hdc, src2, src2._length, &sz2);
			}
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			/*0*/pt[np].x = cx0 + sz0.cx;
			pt[np++].y = cy + hitRow1 * _vi.RowHeight;
			/*1*/pt[np].x = cx0 + sz0.cx + sz.cx;
			pt[np++].y = pt[0].y;
			/*2*/pt[np].x = pt[1].x;
			pt[np++].y = cy + hitRow2 * _vi.RowHeight;
			/*3*/pt[np].x = cx0 + sz2.cx;
			pt[np++].y = pt[2].y;
			/*4*/pt[np].x = pt[3].x;
			pt[np++].y = pt[3].y + _vi.RowHeight;
			/*5*/pt[np].x = cx0;
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			/*0*/pt[np].x = cx + cx2 + sz0.cx;
			pt[np++].y = cy + hitRow1 * _vi.RowHeight;
			/*1*/pt[np].x = cx + cx2 + sz0.cx + sz.cx;
			pt[np++].y = pt[0].y;
			/*2*/pt[np].x = pt[1].x;
			pt[np++].y = cy + hitRow2 * _vi.RowHeight;
			/*3*/pt[np].x = cx + cx2 + sz2.cx;
			pt[np++].y = pt[2].y;
			/*4*/pt[np].x = pt[3].x;
			pt[np++].y = pt[3].y + _vi.RowHeight;
			/*5*/pt[np].x = cx + cx2;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			pt[np++].y = pt[4].y;
			/*6*/pt[np].x = pt[5].x;
			pt[np++].y = pt[0].y + _vi.RowHeight;
			/*7*/pt[np].x = pt[0].x;
			pt[np++].y = pt[6].y;
		}
		return np;
	}
	if (_bom == TEXT_UTF16 || _bom == TEXT_UTF16_NOBOM)
	{
		ULONG np = 0;
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		int cx0 = _getAscColCoord(0);
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		int cx2 = (COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN)*_vi.ColWidth;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
		int cols;
		SIZE sz0 = { 0 }, sz = { 0 };
		if (hitRow1 == hitRow2)
		{
			if (hitCol1 > 0)
			{
				ustring src0 = _findStartByteUnicode(_fri, regionStart.LowPart - hitCol1, hitCol1);
				GetTextExtentPoint32(hdc, src0, src0._length, &sz0);
			}
			cols = hitCol2 - hitCol1;
			if (cols > 0)
			{
				ustring src = _findStartByteUnicode(_fri, regionStart.LowPart, min(regionLength, (ULONG)cols));
				GetTextExtentPoint32(hdc, src, src._length, &sz);
			}
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			/*0*/pt[np].x = cx0 + sz0.cx;
			pt[np++].y = cy + hitRow1 * _vi.RowHeight;
			/*1*/pt[np].x = cx0 + sz0.cx + sz.cx;
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			/*0*/pt[np].x = cx + cx2 + sz0.cx;
			pt[np++].y = cy + hitRow1 * _vi.RowHeight;
			/*1*/pt[np].x = cx + cx2 + sz0.cx + sz.cx;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			pt[np++].y = pt[0].y;
			/*2*/pt[np].x = pt[1].x;
			pt[np++].y = pt[1].y + _vi.RowHeight;
			/*3*/pt[np].x = pt[0].x;
			pt[np++].y = pt[2].y;
		}
		else if (hitCol2 == 0)
		{
			// a special case - the region is 6 cornered
			// starting corner given by {hitCol1, hitRow1}.
			if (hitCol1 > 0)
			{
				ustring src0 = _findStartByteUnicode(_fri, regionStart.LowPart - hitCol1, hitCol1);
				GetTextExtentPoint32(hdc, src0, src0._length, &sz0);
			}
			cols = _vi.BytesPerRow - hitCol1;
			if (cols > 0)
			{
				ustring src = _findStartByteUnicode(_fri, regionStart.LowPart, min(regionLength, (ULONG)cols));
				GetTextExtentPoint32(hdc, src, src._length, &sz);
			}
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			/*0*/pt[np].x = cx0 + sz0.cx;
			pt[np++].y = cy + hitRow1 * _vi.RowHeight;
			// right-top corner
			/*1*/pt[np].x = cx0 + sz0.cx + sz.cx;
			pt[np++].y = pt[0].y;
			// right-bottom corner
			/*2*/pt[np].x = pt[1].x;
			pt[np++].y = cy + hitRow2 * _vi.RowHeight;
			// left-bottom corner
			/*3*/pt[np].x = cx0;
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			/*0*/pt[np].x = cx + cx2 + sz0.cx;
			pt[np++].y = cy + hitRow1 * _vi.RowHeight;
			// right-top corner
			/*1*/pt[np].x = cx + cx2 + sz0.cx + sz.cx;
			pt[np++].y = pt[0].y;
			// right-bottom corner
			/*2*/pt[np].x = pt[1].x;
			pt[np++].y = cy + hitRow2 * _vi.RowHeight;
			// left-bottom corner
			/*3*/pt[np].x = cx + cx2;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			pt[np++].y = pt[2].y;
			// left-top(-1) corner
			/*4*/pt[np].x = pt[3].x;
			pt[np++].y = pt[0].y + _vi.RowHeight;
			// move toward starting corner and stay on a row just below it
			/*7*/pt[np].x = pt[0].x;
			pt[np++].y = pt[4].y;
		}
		else
		{
			// general case with a 8-cornered region
			// the ending byte of the region sits between beginning and ending columns.
			if (hitCol1 > 0)
			{
				ustring src0 = _findStartByteUnicode(_fri, regionStart.LowPart - hitCol1, hitCol1);
				GetTextExtentPoint32(hdc, src0, src0._length, &sz0);
			}
			cols = _vi.BytesPerRow - hitCol1;
			if (cols > 0)
			{
				ustring src = _findStartByteUnicode(_fri, regionStart.LowPart, min(regionLength, (ULONG)cols));
				GetTextExtentPoint32(hdc, src, src._length, &sz);
			}
			SIZE sz2 = { 0 };
			if (hitCol2 > 0)
			{
				ustring src2 = _findStartByteUnicode(_fri, regionStart.LowPart + regionLength - hitCol2, hitCol2);
				GetTextExtentPoint32(hdc, src2, src2._length, &sz2);
			}
#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			/*0*/pt[np].x = cx0 + sz0.cx;
			pt[np++].y = cy + hitRow1 * _vi.RowHeight;
			/*1*/pt[np].x = cx0 + sz0.cx + sz.cx;
			pt[np++].y = pt[0].y;
			/*2*/pt[np].x = pt[1].x;
			pt[np++].y = cy + hitRow2 * _vi.RowHeight;
			/*3*/pt[np].x = cx0 + sz2.cx;
			pt[np++].y = pt[2].y;
			/*4*/pt[np].x = pt[3].x;
			pt[np++].y = pt[3].y + _vi.RowHeight;
			/*5*/pt[np].x = cx0;
#else//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			/*0*/pt[np].x = cx + cx2 + sz0.cx;
			pt[np++].y = cy + hitRow1 * _vi.RowHeight;
			/*1*/pt[np].x = cx + cx2 + sz0.cx + sz.cx;
			pt[np++].y = pt[0].y;
			/*2*/pt[np].x = pt[1].x;
			pt[np++].y = cy + hitRow2 * _vi.RowHeight;
			/*3*/pt[np].x = cx + cx2 + sz2.cx;
			pt[np++].y = pt[2].y;
			/*4*/pt[np].x = pt[3].x;
			pt[np++].y = pt[3].y + _vi.RowHeight;
			/*5*/pt[np].x = cx + cx2;
#endif//#ifdef BINHEXVIEW_SUPPORTS_HIDEF_PRINTOUT
			pt[np++].y = pt[4].y;
			/*6*/pt[np].x = pt[5].x;
			pt[np++].y = pt[0].y + _vi.RowHeight;
			/*7*/pt[np].x = pt[0].x;
			pt[np++].y = pt[6].y;
		}
		return np;
	}

	return BinhexView::measureTextDumpRegion(hdc, pt, cx, cy, hitRow1, hitRow2, hitCol1, hitCol2, regionInView, regionStart, regionLength);
}

/* _findStartByteUnicode - locates the true starting byte of a Unicode text segment at a specified offset into the FILEREADINFO buffer. a unicode character consists of two bytes. if the offset points to the 2nd byte of a unicode character, the true starting byte is at offset - 1. the method moves the file pointer backward by one byte, reads two consecutive bytes, and returns them as the unicode character.
if the unicode character read falls in the control character range (0..0x1f), the method returns a '.' instead. if the offset points to a byte-order-mark (BOM), e.g., 0xFEFF, the bom is replaced with a '.' too.

return value:
a unicode character found at the specified buffer offset.

parameters:
fri - [in,out] descriptor of a read buffer for the file.
fiBufferOffset - [in] offset to a dump byte.
asciiLen - [in] number of the bytes that trail the byte at the offset.
*/
ustring BinhexMetaView::_findStartByteUnicode(FILEREADINFO &fri, ULONG fiBufferOffset, ULONG asciiLen)
{
	WCHAR wc;
	ustring us;
	ULONG srcLen = asciiLen / 2;
	WCHAR *src = us.alloc(srcLen);
	// check the first byte in the buffer.
	LONG i = (LONG)fiBufferOffset;
	if (i+fri.DataOffset.LowPart > 0)
	{
		BYTE b1 = fri.DataBuffer[i];
		BYTE b2[2] = { 0 };
		if (fiBufferOffset & 1)
		{
			// the byte is at an odd offset. it's the 2nd byte of a unicode char.
			// back up a byte to pick up the 1st byte.
			HRESULT hr = S_FALSE;
			if (i == 0)
			{
				hr = readByteAt(fri.DataOffset.QuadPart-1, b2);
				if (FAILED(hr))
					return us;
			}
			else
			{
				b2[0] = fri.DataBuffer[--i];
				asciiLen--;
			}
			asciiLen--;
		}
		else
		{
			b2[0] = fri.DataBuffer[++i];
			asciiLen -= sizeof(WCHAR);
		}
		wc = MAKEWORD(b1, b2[0]);
		if (wc < 0x20)
			wc = '.';
		*src++ = wc;
		i++;
		us._length++;
	}
	else if (_bom == TEXT_UTF16)
	{
		// offset is 0. there is a bom.
		// replace the 0xFEFF bom with a period.
		*src++ = '.';
		asciiLen -= sizeof(WCHAR);
		i += sizeof(WCHAR);
		us._length++;
	}
	while (asciiLen >= 2)
	{
		// offset is 0. there is no bom.
		wc = MAKEWORD(fri.DataBuffer[i], fri.DataBuffer[i + 1]);
		if (wc < 0x20)
			wc = '.';
		*src++ = wc;
		asciiLen -= sizeof(WCHAR);
		i += sizeof(WCHAR);
		us._length++;
	}
	return us;
}

/* _getUTF8AsUnicodeChar - translates a UTF-8 sequence to a Unicode character. returns the single Unicode character.

parameters:
fiDataOffset - [in] offset to a byte in the read buffer, _fri.
bytesFromBuffer - [out,optional] number of the bytes that follow the byte at the offset.
*/
WCHAR BinhexMetaView::_getUTF8AsUnicodeChar(LONG fiDataOffset, int *bytesFromBuffer)
{
	WCHAR uni = 0;
	BYTE utf8seq[4] = { 0 };
	int len = _getUTF8CharSequence(fiDataOffset, utf8seq, NULL, bytesFromBuffer);
	if (len > 0)
		translateUTF8((LPCSTR)utf8seq, len, &uni, 1);
	return uni;
}

/* _getUTF8CharSequence - identifies a UTF-8 byte sequence that includes the byte at a specified offset.

return value:
number of the bytes a UTF-8 sequence identified as such contains.

parameters:
fiDataOffset - [in] offset to a byte in the read buffer, _fri.
utf8 - [in,out] a 4-byte buffer to hold the identified UTF-8 byte sequence.
prefixBytes - [out,optional] number of the bytes that precede the byte at the input offset.
suffixBytes - [out,optional] number of the bytes that follow the byte.
*/
int BinhexMetaView::_getUTF8CharSequence(LONG fiDataOffset, BYTE utf8[4], int *prefixBytes, int *suffixBytes)
{
	HRESULT hr;
	int bytes1 = 0;
	int bytes2 = 0;
	int len = 0;
	BYTE b1, b2, b3;
	ZeroMemory(utf8, 4);
	// check the first char in the buffer.
	b1 = _fri.DataBuffer[fiDataOffset];
	if (0x80 <= b1 && b1 <= 0xbf)
	{
		// it is likely the 2nd or 3rd byte of a utf-8 encoded sequence. it is so, if the byte is be preceded by a 1st byte of utf-8. the buffer does not have the byte. so we need turn to the file. let's load the byte.
		if (fiDataOffset == 0)
		{
			hr = readByteAt(_fri.DataOffset.QuadPart - 1, &b2);
			if (FAILED(hr))
				return 0;
		}
		else
		{
			b2 = _fri.DataBuffer[fiDataOffset - 1];
		}
		bytes1++;
		// is the prior byte another 2nd utf-8 byte?
		if (0x80 <= b2 && b2 <= 0xbf)
		{
			// yes, then, we need to load a yet prior byte. that byte must be the first utf-8 byte. that will be the first byte of the first utif-8 sequence in row buffer _utf8.
			if (fiDataOffset == 1)
			{
				hr = readByteAt(_fri.DataOffset.QuadPart - 2, &b3);
				if (FAILED(hr))
					return 0;
			}
			else
			{
				b3 = _fri.DataBuffer[fiDataOffset - 2];
			}
			utf8[len++] = b3;
			utf8[len++] = b2;
			utf8[len++] = b1;
			bytes1++;
		}
		else
		{
			utf8[len++] = b2;
			utf8[len++] = b1;
			if (0xe0 <= b2 && b2 <= 0xef)
			{
				utf8[len++] = _fri.DataBuffer[fiDataOffset + 1];
				bytes2++;
			}
		}
		// only one byte has been read from the DataBuffer. note the count does not include the bytes read directly from the file (through a call to readByteAt).
		bytes2++;
	}
	else if (0xc0 <= b1 && b1 <= 0xdf)
	{
		// 2-byte utf-8: unicode range 0x080 .. 0x7ff
		b2 = _fri.DataBuffer[fiDataOffset + 1];
		utf8[len++] = b1;
		utf8[len++] = b2;
		bytes2 = len;
	}
	else if (0xe0 <= b1 && b1 <= 0xef)
	{
		// 3-byte utf-8 unicode range 0x0800 .. 0xffff
		b2 = _fri.DataBuffer[fiDataOffset + 1];
		b3 = _fri.DataBuffer[fiDataOffset + 2];
		utf8[len++] = b1;
		utf8[len++] = b2;
		utf8[len++] = b3;
		bytes2 = len;
	}
	else
	{
		// ascii or extended-ascii character.
		utf8[len++] = b1;
		bytes2 = len;
	}
	if (suffixBytes)
		*suffixBytes = bytes2;
	if (prefixBytes)
		*prefixBytes = bytes1;
	return len;
}

/* _findStartByteUTF8 - locates the true starting byte of a UTF-8 text segment at a specified offset into the FILEREADINFO buffer. a (unicode) character encoded in UTF-8 can consist of one to three bytes. if the offset points to the back byte of a character, the true starting byte is at offset - 2. the method moves the file pointer backward by two bytes, reads 3 consecutive bytes, and translates the sequence to a corresponding unicode character.
if the unicode character read falls in the control character range (0..0x1f), the method returns a '.' instead. if the offset points to a byte-order-mark (BOM), {0xEF,0xBB,0xBF}, the bom is replaced with a '.' too.

return value:
a unicode character translated from a UTF-8 sequence found at the specified buffer offset.

parameters:
fri - [in,out] descriptor of a read buffer for the file.
fiBufferOffset - [in] offset to a dump byte.
asciiLen - [in] number of the bytes that trail the byte at the offset.
utf8Len - [out] number of bytes the found utf-8 sequence is made of.
*/
ustring BinhexMetaView::_findStartByteUTF8(FILEREADINFO &_fri, ULONG fiBufferOffset, ULONG asciiLen, int *utf8Len)
{
	WCHAR wc;
	ustring us;
	int readLen=0;
	int srcLen = asciiLen;
	WCHAR *src = us.alloc(asciiLen);
	LONG i = (LONG)fiBufferOffset;
	if (i + _fri.DataOffset.LowPart > 0)
	{
		wc = _getUTF8AsUnicodeChar(i, &readLen);
		if (readLen == 0)
			return us;
		if (wc < 0x20)
			wc = '.';
		*src++ = wc;
		us._length++;
		srcLen -= readLen;
		i += readLen;
	}
	else if (_bom == TEXT_UTF8)
	{
		// replace the 3-byte bom with a period.
		int jmax = min(srcLen, 3);
		for (int j = 0; j < jmax; j++)
		{
			*src++ = '.';
			us._length++;
			srcLen--;
			i++;
		}
	}
	while (srcLen > 0)
	{
		readLen = 0;
		wc = _getUTF8AsUnicodeChar(i, &readLen);
		if (readLen == 0)
			break;
		if (wc < 0x20)
			wc = '.';
		*src++ = wc;
		us._length++;
		srcLen -= readLen;
		i += readLen;
	}
	if (utf8Len)
		*utf8Len = i - (LONG)fiBufferOffset;
	return us;
}

/* _finishDumpLinePartial - overrides the base method to format a (partial) line of text encoded in UTF-8 or Unicode, and output the text re-encoded in Unicode in the ASCII dump area.
the method is called by drawRowBytes to draw the text bytes in the ASCII dump area after drawRowBytes finishes drawing the bytes in the hex dump area.

parameters:
hdc - [in] device context handle.
linebuf - [in] contains a text segment to dump.
bufSize - [in] size in bytes of linebuf.
asciiLen - [in] byte length of the text string in the line buffer. 
fiDataOffset - [in,out] offset to the front of a text segment accessible with the file read buffer of _fri.
cx - [in] number of bytes the text segment consists of.
ix - [in] column index of the front of the text segment.
iy - [in] row index of the text segment.

note that both this override and the base method currently does not modify the value of fiDataOffset, although it is passed in by reference.
*/
void BinhexMetaView::_finishDumpLinePartial(HDC hdc, LPSTR linebuf, ULONG bufSize, ULONG asciiLen, ULONG &fiDataOffset, UINT cx, UINT ix, UINT iy)
{
	if (_bom == TEXT_UTF8 || _bom == TEXT_UTF8_NOBOM)
	{
		SIZE sz = { 0 };
		SIZE sz0 = { 0 };
		if (ix > 0)
		{
			ustring src0 = _findStartByteUTF8(_fri, fiDataOffset - ix, ix);
			GetTextExtentPoint32(hdc, src0, src0._length, &sz0);
		}
		ustring src = _findStartByteUTF8(_fri, fiDataOffset, asciiLen);
		GetTextExtentPoint32(hdc, src, src._length, &sz);

		RECT rcDraw;
		rcDraw.top = DRAWTEXT_MARGIN_CY + iy * _vi.RowHeight;
		rcDraw.bottom = rcDraw.top + _vi.RowHeight;
		rcDraw.left = DRAWTEXT_MARGIN_CX + (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN) * _vi.ColWidth + sz0.cx;
		rcDraw.right = DRAWTEXT_MARGIN_CX + (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN) * _vi.ColWidth + sz0.cx + sz.cx;
		if (rcDraw.right > _vi.FrameSize.cx - DRAWTEXT_MARGIN_CX)
			rcDraw.right = _vi.FrameSize.cx - DRAWTEXT_MARGIN_CX;
		rcDraw.left -= _vi.ColWidth*_vi.CurCol;
		if (rcDraw.left < _vi.FrameSize.cx)
		{
			DrawTextW(hdc, src, src._length, &rcDraw, 0);
		}
		return;
	}
	if (_bom == TEXT_UTF16 || _bom == TEXT_UTF16_NOBOM)
	{
		SIZE sz = { 0 };
		SIZE sz0 = { 0 };
		if (ix > 0)
		{
			ustring src0 = _findStartByteUnicode(_fri, fiDataOffset - ix, ix);
			GetTextExtentPoint32(hdc, src0, src0._length, &sz0);
		}
		ustring src = _findStartByteUnicode(_fri, fiDataOffset, asciiLen);
		GetTextExtentPoint32(hdc, src, src._length, &sz);

		RECT rcDraw;
		rcDraw.top = DRAWTEXT_MARGIN_CY + iy * _vi.RowHeight;
		rcDraw.bottom = rcDraw.top + _vi.RowHeight;
		rcDraw.left = DRAWTEXT_MARGIN_CX + (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN) * _vi.ColWidth + sz0.cx;
		rcDraw.right = DRAWTEXT_MARGIN_CX + (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN) * _vi.ColWidth + sz0.cx + sz.cx;
		if (rcDraw.right > _vi.FrameSize.cx - DRAWTEXT_MARGIN_CX)
			rcDraw.right = _vi.FrameSize.cx - DRAWTEXT_MARGIN_CX;
		rcDraw.left -= _vi.ColWidth*_vi.CurCol;
		if (rcDraw.left < _vi.FrameSize.cx)
		{
			DrawTextW(hdc, src, src._length, &rcDraw, 0);
		}
		return;
	}
	BinhexView::_finishDumpLinePartial(hdc, linebuf, bufSize, asciiLen, fiDataOffset, cx, ix, iy);
}

/* translateUTF8 - translates a string of UTF-8 sequences into a string of Unicode characters.

return value:
number of unicode characters copied to the destUni buffer.

parameters:
srcUTF8 - [in] a source string in UTF-8.
srcLen - [in] byte length of the source string.
destUni - [in,out] holds a string of Unicode characters translated from the UTF-8 source.
destSize - [in] destination buffer size in wide characters.
*/
int BinhexMetaView::translateUTF8(LPCSTR srcUTF8, int srcLen, LPWSTR destUni, int destSize)
{
#ifdef TRANSLATEUTF8_DELEGATES_TO_WIN32
	return MultiByteToWideChar(CP_UTF8, 0, srcUTF8, srcLen, destUni, destSize);
#else//#ifdef TRANSLATEUTF8_DELEGATES_TO_WIN32
	BYTE b1, b2, b3;
	WCHAR uni;
	int j = 0;
	for (int i = 0; i < srcLen && j < destSize; i++)
	{
		b1 = srcUTF8[i];
		if (0xc0 <= b1 && b1 <= 0xdf)
		{
			// 2-byte utf-8: unicode range 0x080 .. 0x7ff
			// 1st byte: 110abcde
			// 2nd byte: 10fghijk
			// unicode: abcde-fghijk (11 bits)
			b2 = srcUTF8[++i];
			uni = ((b1 & 0x1f) << 6) | (b2 & 0x3f);
		}
		else if (0xe0 <= b1 && b1 <= 0xef)
		{
			// 3-byte utf-8 unicode range 0x0800 .. 0xffff
			// 1st byte: 1110abcd
			// 2nd byte: 10efghij
			// 3rd byte: 10klmnop
			// unicode: abcd-efghij-klmnop (16 bits)
			b2 = srcUTF8[++i];
			b3 = srcUTF8[++i];
			uni = ((b1 & 0x0f) << 12) | ((b2 & 0x3f) << 6) | (b3 & 0x3f);
		}
		else
		{
			// ascii character. just pad a 0.
			uni = MAKEWORD(b1, 0);
		}
		destUni[j++] = uni;
	}
	return j;
#endif//#ifdef TRANSLATEUTF8_DELEGATES_TO_WIN32
}

/* _finishDumpLine - overrides the base method. outputs the hex digits already in a line buffer to the hex dump area, and outputs the source bytes as Unicode characters in the ASCII dump area, if the file is a text file encoded in Unicode or UTF-8. delegates to the base class if the source is not Unicode or UTF-8.

parameters:
hdc - [in] device context handle.
linebuf - [in] a line buffer containing source data bytes to be dumped.
bufSize - [in] line buffer size in bytes.
fiDataOffset - [in,out] offset to the front of the source bytes.
cx - [in] number of columns to fill in the ASCII dump area.
iy - [in] row number of the output row.

this method handles utf-8 and unicode texts. any other text is sent back to the base class method.

these are the utf-8 sequences this function captures and translates into unicode.

unicode range 1: 0x0000 .. 0x007f
  utf-8 1st byte: 0x00 .. 0x7f

unicode range 2: 0x0080 .. 0x07ff
  utf-8 1st byte range: 0xc0 .. 0xdf
  utf-8 2nd byte range: 0x80 .. 0xbf

unicode range 3: 0x0800 .. 0xffff
  utf-8 1st byte range: 0xe0 .. 0xef
  utf-8 2nd byte range: 0x80 .. 0xbf
  utf-8 3rd byte range: 0x80 .. 0xbf
*/
void BinhexMetaView::_finishDumpLine(HDC hdc, LPSTR linebuf, ULONG bufSize, ULONG &fiDataOffset, UINT cx, UINT iy)
{
	if (_bom == TEXT_UTF8 || _bom == TEXT_UTF8_NOBOM)
	{
		// translate utf-8 text into unicode.
		// class member variable _utf8, a text row buffer, keeps bytes from a row that were orphans and could not be processed in a previous call.
		// we append the text bytes of the new row to _utf8. if _utf8 has orphan bytes, appended bytes should make a complete utf-8 sequence and produce successful unicode translation.
		BYTE b1 = 0; // first byte of a utf-8 sequence
		// fiDataOffset is an offset into the dump data buffer.
		if (fiDataOffset == 0)
		{
			// we're at beginning of a dump page.
			// clear any orphan byte from a prior dump.
			_utf8.clear();
			if (_fri.DataOffset.LowPart != 0)
			{
				// check the first char in the buffer.
				b1 = _fri.DataBuffer[0];
				if (0x80 <= b1 && b1 <= 0xbf)
				{
					// it is likely the 2nd or 3rd byte of a utf-8 encoded sequence. it is so, if the byte is be preceded by a 1st byte of utf-8. the buffer does not have the byte. so we need turn to the file. let's load the byte.
					BYTE b2[2] = { 0 };
					if (S_OK == readByteAt(_fri.DataOffset.QuadPart-1, b2))
					{
						// is the prior byte another 2nd utf-8 byte?
						if (0x80 <= b2[0] && b2[0] <= 0xbf)
						{
							// yes, then, we need to load a yet prior byte. that byte must be the first utf-8 byte. that will be the first byte of the first utif-8 sequence in row buffer _utf8.
							BYTE b3[2] = { 0 };
							if (S_OK == readByteAt(_fri.DataOffset.QuadPart-2, b3))
								_utf8.append((LPCSTR)b3);
						}
						// add the prior byte which serves as the 2nd byte of the first utf-8 sequence.
						_utf8.append((LPCSTR)b2);
					}
				}
			}
		}
		// add the text bytes from the new row.
		_utf8.append((LPCSTR)(_fri.DataBuffer + fiDataOffset), cx);
		// analyze the bytes. cx is the row width. we are restricted to processing up to cx bytes.
		// bomLen is used to skip the first few bytes that belong to a bom sequence. if the file has no bom, bomLen is 0.
		ULONG bomLen = _bom == TEXT_UTF8? 3 : 0;
		// i2 counts the bom bytes we skip.
		ULONG i2 = fiDataOffset + _fri.DataOffset.LowPart;
		// utf8Len counts the bytes we will actually translate.
		UINT utf8Len = 0;
		// u16Len counts the unicode chars we expect to generate.
		UINT u16Len = 0;
		// j counts the bytes of utf-8 sequences in the row buffer, _utf8. j can be greater than utf8Len. the difference is the number of the orphan bytes we will carry forward to the next call.
		ULONG j = 0;
		while (utf8Len < _utf8._length)
		{
			b1 = _utf8[j];
			// skip the byte if it's part of the bom.
			if (i2 < bomLen)
			{
				i2++;
				j++;
			}
			// is it the first byte of a 2-byte utf-8 sequence?
			else if (0xc0 <= b1 && b1 <= 0xdf)
				j += 2;
			// or, is it the first byte of a 3-byte utf-8 sequence?
			else if (0xe0 <= b1 && b1 <= 0xef)
				j += 3;
			// no, it is an ascii or extended-ascii byte.
			else
				j++;
			// if the latest utf-8 sequence makes the row too long, break. it will be made the first sequence in the next row.
			if (j > _utf8._length)
				break;
			// we can process this byte as part of the current row.
			utf8Len = j;
			// bump up the (expected) unicode count.
			u16Len++;
		}
		// allocate a unicode buffer. it will receive the wide text translated from the utf-8 sequences in _utf8.
		WCHAR *u16 = (WCHAR *)calloc(u16Len+1, sizeof(WCHAR));
		u16[u16Len] = 0;
		// watch for the skipped bytes of the bom.
		if (i2 && i2==bomLen)
		{
			// we don't want to subject the bom bytes to the utf-8 translation process. they are not part of the user data. so, we cannot translate them. we want to show them as bom bytes or pure numbers.
			for (j = 0; j < bomLen; j++)
			{
				// pad the byte with a zero.
				u16[j] = MAKEWORD(_fri.DataBuffer[j], 0);
			}
			// translate the remaing utf-8 sequence(s) in the row buffer to unicode.
			translateUTF8(_utf8 + bomLen, utf8Len - bomLen, u16 + bomLen, u16Len - bomLen);
		}
		else // translate the utf-8 sequences in the row buffer to unicode.
			translateUTF8(_utf8, utf8Len, u16, u16Len);
		// delete the processed bytes from the row buffer. keep the unprocessed as orphan bytes. they are the leading part of a yet-to-be-loaded utf-8 sequence. it will be used to complete a new utf-8 sequence in the next call.
		astring tmp((LPCSTR)_utf8 + utf8Len);
		_utf8.attach(tmp);
		// print the translated unicode text in the dump column.
		_finishDumpLineU(hdc, linebuf, bufSize, u16, fiDataOffset, 0, sizeof(WCHAR)*u16Len, iy);
		// push the buffer read index to past the processed row of bytes.
		fiDataOffset += cx;
		// clean up
		free(u16);
	}
	else if (_bom == TEXT_UTF16 || _bom == TEXT_UTF16_NOBOM)
	{
		// unicode text.
		// di is 0 if starting offset is even. it's 1 if odd.
		// an odd boundary means the 1st byte in the buffer is actually the second byte of a 2-byte unicode character. we drop the orphan byte. the new row dump starts with the bytes that follow.
		// why dropping the orphan byte? why not reconstruct the broken unicode character by reloading the leading byte from the file and adding the orphan byte to it? we can certainly do that. but, as explained below, we just want a cheap way to stop an unlikely view corruption if it ever has to happen.
		// we don't expect an odd-numbered starting offset thanks to logical constraints imposed by the view parameters we've chosen. the view allows the number of bytes in a dump row to be 8, 16, or 32, all even-numbered. Scrolling the view page should never place a byte of an odd-numbered offset at the beginning of the dump buffer.
		// the di watch is a safeguard to prevent view corruption just in case.
		ULONG di = _fri.DataOffset.LowPart & 1;
		WCHAR *src = (WCHAR*)(_fri.DataBuffer + fiDataOffset + di);
		WCHAR *src2 = NULL;
		if (_bom == TEXT_UTF16)
		{
			// need to skip the bom bytes if the starting offset is 0.
			if (fiDataOffset == 0 && _fri.DataOffset.QuadPart == 0)
			{
				// we cannot use the bytes in DataBuffer since they include non-unicode bom bytes. so, we make a temp buffer and copy the bom and non-bom bytes of the row to it. the bom bytes are expanded so they appear as unicode chars.
				src2 = (WCHAR*)calloc(cx+1, sizeof(WCHAR));
				memset(src2, 0, (cx + 1)* sizeof(WCHAR));
				// copy a unicode double comma char in place of the 2 bom bytes.
				src2[0] = '.';// MAKEWORD(_fri.DataBuffer[0], _fri.DataBuffer[1]);
				// copy the non-bom bytes.
				for (UINT j = 2; j < cx; j += 2)
				{
					src2[j >> 1] = MAKEWORD(_fri.DataBuffer[j], _fri.DataBuffer[j + 1]);
				}
				src = (WCHAR*)src2;
			}
		}
		// use a dedicated method to dump unicode text.
		fiDataOffset = _finishDumpLineU(hdc, linebuf, bufSize, src, fiDataOffset, di, cx, iy);
		if (src2)
			free(src2);
	}
	else // let the base class handle ascii and extended-ascii.
		BinhexView::_finishDumpLine(hdc, linebuf, bufSize, fiDataOffset, cx, iy);
}

/* _finishDumpLineU - converts a string of hex dump digits in ASCII to Unicode, adds a text-dump segment already in Unicode to the digit string, and outputs the combined line of text at a specified row position.

return value:
file offset of the back of the Unicode dump string.

parameters:
hdc - [in] device context handle.
linebuf - [in] a line buffer containing hex digits in ASCII.
bufSize - [in] line buffer size in bytes.
src - [in] a text dump string in Unicode.
fiDataIndex - [in] file offset of the front of the Unicode dump.
*/
ULONG BinhexMetaView::_finishDumpLineU(HDC hdc, LPSTR linebuf, ULONG bufSize, WCHAR *src, ULONG fiDataIndex, UINT cx1, UINT cx2, UINT iy)
{
	UINT lineLen = (UINT)strlen(linebuf);

	ULONG i = fiDataIndex;
	int destSize = lineLen + cx2 - cx1 + 2;
	WCHAR *dest = (WCHAR *)calloc(destSize, sizeof(WCHAR));
	ZeroMemory(dest, destSize);

	UINT ix;
	for (ix = 0; ix < lineLen; ix++)
		dest[ix] = linebuf[ix];

	UINT destLen = lineLen;
	for (ix = cx1; ix < cx2; ix+=sizeof(WCHAR))
	{
		WCHAR ci = *src;
		if (ci < ' ')
			ci = '.';
		dest[destLen++] = ci;
		src++;
		i += sizeof(WCHAR);
	}
	_vi.RowsShown++;

	RECT rcDraw;
	rcDraw.left = DRAWTEXT_MARGIN_CX;
	rcDraw.top = DRAWTEXT_MARGIN_CY + iy * _vi.RowHeight;
	rcDraw.right = _vi.FrameSize.cx;
	rcDraw.bottom = rcDraw.top + _vi.RowHeight;
	if (rcDraw.bottom > _vi.FrameSize.cy)
		rcDraw.bottom = _vi.FrameSize.cy;

	if (_vi.CurCol > 0 && _vi.BytesPerRow > DEFAULT_HEX_BYTES_PER_ROW)
	{
		DrawTextW(hdc, dest, OFFSET_COL_LEN, &rcDraw, 0);
		if (((OFFSET_COL_LEN + LEFT_SPACING_COL_LEN) + _vi.CurCol) < destLen)
		{
			rcDraw.left += _vi.OffsetColWidth;
			DrawTextW(hdc,
				dest + (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN) + _vi.CurCol,
				destLen - (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN) - _vi.CurCol,
				&rcDraw,
				0);
		}
	}
	else
	{
		DrawTextW(hdc, dest, destLen, &rcDraw, 0);
	}
	return i;
}
#endif//#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE

/* _repaintInternal - overrides the base class method to ensure data availability, and repaint regions and other meta objects.
*/
HRESULT BinhexMetaView::_repaintInternal(HDC hdc, RECT frameRect, UINT flags)
{
	HRESULT hr = S_FALSE;
	if (!_fri.HasData)
	{
		hr = readFileData(_vi.RowsPerPage*_vi.BytesPerRow);
		if (FAILED(hr))
			return hr;
	}

	hr = BinhexView::_repaintInternal(hdc, frameRect, flags);

	if (!(flags & BINHEXVIEW_REPAINT_DUMP_ONLY))
	{
		_regions.redrawAll(hdc, this);
		if (flags & BINHEXVIEW_REPAINT_ANNOTATION_ON_SIDE)
			_annotations.redrawAll(hdc, &_regions);
		else
			_annotations.redrawAll(hdc);
		_shapes.redrawAll(hdc);
		hr = S_OK;
	}
#ifdef BINHEXVIEW_SAVES_PAINTED_PAGE
	if (_hbmPaint)
		DeleteObject(_hbmPaint);
	_hbmPaint = createDIB(hdc);
	//SaveBmp(_hbmPaint, BMP_SAVE_PATH1);
#endif//#ifdef BINHEXVIEW_SAVES_PAINTED_PAGE
	return hr;
}

HRESULT BinhexMetaView::readFileData(ULONG dwReadSize)
{
	return readBlock(&_fri, dwReadSize);
}

short _brightenRgbComponent(short c, int brightness)
{
	/*float f = c;
	float df = 255.0 - f;
	float df2 = 0.5*df;
	float f2 = f + df2;
	return (short)f2;*/
	return c + MulDiv(255 - c, brightness, 100);
}

COLORREF _brightenColor(COLORREF src, int brightness)
{
	short r = _brightenRgbComponent(GetRValue(src), brightness);
	short g = _brightenRgbComponent(GetGValue(src), brightness);
	short b = _brightenRgbComponent(GetBValue(src), brightness);
	return RGB(r, g, b);
}

/* addTaggedRegion - adds a region and attaches an annotation.

this annotation helper is thread-safe. can use it in the worker thread. it uses a mutex to protect the region and annotation collections from contentious access.
*/
int BinhexMetaView::addTaggedRegion(ULONG flags, LARGE_INTEGER regionOffset, ULONG regionLen, LPCWSTR annotFormat, ...)
{
	lockObjAccess();
	DUMPREGIONINFO ri = { 0 };
	CRegionOperation::setRegion((int)_regions.size(), regionOffset.QuadPart, regionLen, NULL, &ri);
	if (flags & BMVADDANNOT_MASK_PARENTREGION)
	{
		// inherit colors from specified parent region.
		SHORT parentRegion = HIWORD(flags);
		// region index is one-based. so decrement it to get a real index.
		if (parentRegion-- > 0)
		{
			ri.BorderColor = _regions[parentRegion].BorderColor;
			ri.ForeColor = _regions[parentRegion].ForeColor;
			ri.BackColor = _regions[parentRegion].BackColor;
		}
	}
	if (flags & BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND)
		ri.BackColor = 0;
	else if (flags & BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND)
		ri.BackColor = _brightenColor(ri.BackColor, 50);
	else if (flags & BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND2)
		ri.BackColor = _brightenColor(ri.BackColor, 75);
	if (flags & BMVADDANNOT_FLAG_GRAYTEXT)
		ri.ForeColor = COLORREF_DKGRAY;
	if (flags & BMVADDANNOT_FLAG_HEADER)
		ri.RenderAsHeader = true;
	if (!(flags & BMVADDANNOT_FLAG_MODIFIABLE))
		ri.Flags |= DUMPREGIONINFO_FLAG_READONLY;
	if (flags & BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST)
		ri.Flags |= DUMPREGIONINFO_FLAG_TAG;

	int count = (int)_regions.add(ri);
	DBGPRINTF_VERBOSE((L"addTaggedRegion(flags=%08X, offset=%I64X, len=%d): count=%d\n", flags, regionOffset.QuadPart, regionLen, count));
	if (annotFormat)
	{
		if (flags & BMVADDANNOT_FLAG_DONT_FORMAT_ANNOT)
		{
			_annotations.annotateRegion(&_regions, count, annotFormat);
		}
		else
		{
			va_list args;
			va_start(args, annotFormat);
			ustring s;
			s.vformat(annotFormat, args);
			va_end(args);
			_annotations.annotateRegion(&_regions, count, s);
		}
	}
	unlockObjAccess();
	return count;
}

int BinhexMetaView::addTaggedRegion(ULONG flags, USHORT labelStart, USHORT labelStop, LARGE_INTEGER regionOffset, ULONG regionLen, LPCWSTR annotText)
{
	lockObjAccess();
	DUMPREGIONINFO ri = { 0 };
	CRegionOperation::setRegion((int)_regions.size(), regionOffset.QuadPart, regionLen, NULL, &ri);
	if (flags & BMVADDANNOT_MASK_PARENTREGION)
	{
		// inherit colors from specified parent region.
		SHORT parentRegion = HIWORD(flags);
		// region index is one-based. so decrement it to get a real index.
		if (parentRegion-- > 0)
		{
			ri.BorderColor = _regions[parentRegion].BorderColor;
			ri.ForeColor = _regions[parentRegion].ForeColor;
			ri.BackColor = _regions[parentRegion].BackColor;
		}
	}
	ri.LabelStart = labelStart;
	ri.LabelStop = labelStop;
	ri.Flags |= DUMPREGIONINFO_FLAG_SETLABEL;

	if (flags & BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND)
		ri.BackColor = 0;
	else if (flags & BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND)
		ri.BackColor = _brightenColor(ri.BackColor, 50);
	else if (flags & BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND2)
		ri.BackColor = _brightenColor(ri.BackColor, 75);
	if (flags & BMVADDANNOT_FLAG_GRAYTEXT)
		ri.ForeColor = COLORREF_DKGRAY;
	if (flags & BMVADDANNOT_FLAG_HEADER)
		ri.RenderAsHeader = true;
	if (!(flags & BMVADDANNOT_FLAG_MODIFIABLE))
		ri.Flags |= DUMPREGIONINFO_FLAG_READONLY;
	if (flags & BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST)
		ri.Flags |= DUMPREGIONINFO_FLAG_TAG;

	int count = (int)_regions.add(ri);
	DBGPRINTF_VERBOSE((L"addTaggedRegion(flags=%08X, offset=%I64X, len=%d): count=%d\n", flags, regionOffset.QuadPart, regionLen, count));
	if (annotText)
		_annotations.annotateRegion(&_regions, count, annotText);
	unlockObjAccess();
	return count;
}

int BinhexMetaView::addTaggedRegion(ULONG flags, LARGE_INTEGER regionOffset, ULONG regionLen, COLORREF borderColor, COLORREF interiorColor, LPCWSTR annotText)
{
	lockObjAccess();
	DUMPREGIONINFO ri = { 0 };
	CRegionOperation::setRegion((int)_regions.size(), regionOffset.QuadPart, regionLen, NULL, &ri);
	ri.BorderColor = borderColor;
	ri.BackColor = interiorColor;
	if (flags & BMVADDANNOT_MASK_PARENTREGION)
	{
		// inherit colors from specified parent region.
		SHORT parentRegion = HIWORD(flags);
		// region index is one-based. so decrement it to get a real index.
		if (parentRegion-- > 0)
		{
			ri.BorderColor = _regions[parentRegion].BorderColor;
			ri.ForeColor = _regions[parentRegion].ForeColor;
			ri.BackColor = _regions[parentRegion].BackColor;
		}
	}
	if (flags & BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND)
		ri.BackColor = 0;
	else if (flags & BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND)
		ri.BackColor = _brightenColor(ri.BackColor, 50);
	else if (flags & BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND2)
		ri.BackColor = _brightenColor(ri.BackColor, 75);
	if (flags & BMVADDANNOT_FLAG_GRAYTEXT)
		ri.ForeColor = COLORREF_DKGRAY;
	if (flags & BMVADDANNOT_FLAG_HEADER)
		ri.RenderAsHeader = true;
	if (!(flags & BMVADDANNOT_FLAG_MODIFIABLE))
		ri.Flags |= DUMPREGIONINFO_FLAG_READONLY;
	if (flags & BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST)
		ri.Flags |= DUMPREGIONINFO_FLAG_TAG;

	int count = (int)_regions.add(ri);
	DBGPRINTF_VERBOSE((L"addTaggedRegion(flags=%08X, offset=%I64X, len=%d): count=%d\n", flags, regionOffset.QuadPart, regionLen, count));
	if (annotText)
		_annotations.annotateRegion(&_regions, count, annotText);
	unlockObjAccess();
	return count;
}

DUMPANNOTATIONINFO *BinhexMetaView::openAnnotationOfRegion(int regionIndex, bool canCreate)
{
	DUMPANNOTATIONINFO *ai=NULL;
	lockObjAccess();
	// regionIndex is 1-based.
	if (0 < regionIndex && regionIndex <= _regions.size())
	{
		DUMPREGIONINFO &ri = _regions[regionIndex - 1];
		int i = ri.AnnotCtrlId - IDC_ANNOTATION_EDITCTRL0;
		if (0 <= i && i < _annotations.size())
			ai = &_annotations[i];
		else if (canCreate) // create an empty annotation?
			ai = _annotations.annotateRegion(&_regions, regionIndex, NULL);
	}
	unlockObjAccess();
	return ai;
}

////////////////////////////////////////////////////////////
// BinhexMetaPageView methods

bool BinhexMetaPageView::init(HWND hwnd, DATARANGEINFO *dri, int fontSize)
{
	int rpp = _vi.RowsPerPage;
	_vi.FontSize = fontSize? fontSize:DEFAULT_FONT_SIZE;
	_vi.RowsPerPage = dri->RangeLines;
	_vi.RowsShown = 0;

	if (!BinhexMetaView::init(hwnd))
		return false;

	// use larger margins for the full-page view. overwrite the margins set by the base class which are for a scrollable view meant for BinhexDumpWnd.
	_vi.FrameMargin = { PAGEFRAMEMARGIN_CX, PAGEFRAMEMARGIN_CY };

	_vi.OffsetColWidth = _vi.ColWidth*(OFFSET_COL_LEN + LEFT_SPACING_COL_LEN);
	_vi.ColsPerPage = _vi.ColsTotal = (COL_LEN_PER_DUMP_BYTE+1) * _vi.BytesPerRow + RIGHT_SPACING_COL_LEN;
	_vi.VScrollerSize.cx = _vi.VScrollerSize.cy = 0;
	_vi.HScrollerSize.cx = _vi.HScrollerSize.cy = 0;
	_vi.CurRow = (UINT)(_fri.DataOffset.QuadPart / _vi.BytesPerRow);

	_vi.FrameSize.cx = _vi.OffsetColWidth + _vi.ColsPerPage*_vi.ColWidth + 2 * _vi.FrameMargin.x;
	_vi.FrameSize.cy = rpp*_vi.RowHeight + 2 * _vi.FrameMargin.y;

	_origPt = { 0 };
	_dumpRect = { 0,0,_vi.FrameSize.cx,_vi.FrameSize.cy };
	_pageRect = _dumpRect;
	_annotsRect = { 0 };

	this->_annotOpacity = BHV_FULL_OPACITY;
	this->_annotFadeoutOpacity = 0; // this is not used in page view.
	this->_shapeOpacity = BHV_NORMAL_SHAPE_PAGEVIEW_OPACITY;
	DWORD val;
	if (ERROR_SUCCESS == Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY L"\\PageView", L"Shape.Opacity", &val))
		_shapeOpacity = val;
	if (ERROR_SUCCESS == Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY L"\\PageView", L"Annotation.Opacity", &val))
		_annotOpacity = val;
	_annotations.reinit(this, DUMPANNOTATION_FLAG_SHOWCONNECTOR | DUMPANNOTATION_FLAG_DONTDELETEIMAGE);
	_shapes.reinit(this);

	if (dri->GroupAnnots)
	{
		if (_regions.size() && _annotations.size())
		{
			SIZE pane = _annotations.getAnnotationsPane(&_regions);
			int dy = _dumpRect.bottom - pane.cy;
			int dy2 = dy / 2;

			_annotsRect.left = _dumpRect.right + _vi.ColWidth;
			_annotsRect.right = _annotsRect.left + pane.cx;
			_pageRect.right += pane.cx;
			if (dy >= 0)
			{
				// the annotation pane is shorter than the dump pane.
				_annotsRect.top = dy2;
				_annotsRect.bottom = dy2 + pane.cy;
			}
			else // dy and dy2 are negative
			{
				// the annotation pane is taller than the dump pane.
				// lower the dump pane.
				_origPt.y -= dy2;
				_dumpRect.top -= dy2;
				_dumpRect.bottom -= dy2;
				_annotsRect.bottom = pane.cy;
				_pageRect.bottom = _dumpRect.bottom - dy2;
			}
		}
	}
	return true;
}

RECT BinhexMetaPageView::initViewInfo()
{
	// do not call the base class method. it's meant for the dump view with scrollbars in the main dialog box. it's not for a print preview. the preview initialization has already set up the VIEWINFO parameters. the only thing we need to do is clear RowsShown.
	_vi.RowsShown = 0;
	return { 0,0,_vi.FrameSize.cx,_vi.FrameSize.cy };
}

HBITMAP BinhexMetaPageView::paintToBitmap(UINT repaintFlags)
{
	ASSERT(_vi.RowsShown == 0); // have you called init()?
	HDC hdc = GetDC(_hw);
	HDC hdc2 = CreateCompatibleDC(hdc);
	HBITMAP hbm = CreateCompatibleBitmap(hdc, _pageRect.right, _pageRect.bottom);
	HBITMAP hbm0 = (HBITMAP)SelectObject(hdc2, hbm);

	FillRect(hdc2, &_pageRect, GetStockBrush(WHITE_BRUSH));

	beginPaint(hdc2);
	repaint(repaintFlags);
	endPaint();

	SelectObject(hdc2, hbm0);
	DeleteDC(hdc2);
	ReleaseDC(_hw, hdc);
	return hbm;
}

void BinhexMetaPageView::setupViewport()
{
	SetViewportOrgEx(_hdc, _origPt.x, _origPt.y, NULL);
	// this is meaningless unless SetWindowExtEx() too is called. get rid of it.
	//SetViewportExtEx(_hdc, _vi.FrameSize.cx, _vi.FrameSize.cy, NULL);
}
