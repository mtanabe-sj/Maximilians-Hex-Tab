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
#include "CRegionOperation.h"
#include "AnnotationOperation.h"


/* this is no longer necessary. since we've moved to use of BinhexDumpWnd, clipping at view frame is automatic.
#define CREGIONOP_CLIPS_REGION
*/
#define LARGER_THAN_PAGEFUL_REGION_DRAWS_SIDE_BORDERLINES_TOO


// colors suitable for background with about 75% luminosity
COLORREF s_regionInteriorColor[MAX_DUMPREGIONCOLORS] = {
	0xE6B0AA, // red
	0xF1948A,
	0xC39BD3, // purple
	0xD2B4DE,
	0xA9CCE3, // blue
	0x81D4FA,
	0xA3E4D7, // green
	0xA2D9CE,
	0xA9DFBF,
	0xABEBC6,
	0xF9E79F, // yellow
	0xFAD7A0, // orange
	0xF5CBA7,
	0xEDBB99,
	0xD7CCC8, // brown
	0xABB2B9, // gray
};

// colors suitable for foreground with about 40% luminosity
COLORREF s_regionBorderColor[MAX_DUMPREGIONCOLORS] = {
	0xB71C1C, // red
	0x880E4F,
	0x4A148C, // purple
	0x283593,
	0x1A237E, // blue
	0x0D47A1,
	0x01579B,
	0x006064, // green
	0x004D40,
	0x1B5E20,
	0x33691E,
	0x827717, // ocher
	0xE65100, // orange
	0xBF360C, // burnt siena
	0x3E2723, // brown
	0x263238, // dark gray
};


bool CRegionSelect::startOp(POINT pt, FILEREADINFO &fri)
{
	DBGPRINTF((L"CRegionSelect::startOp(%d): pt=(%d:%d); DataOffset=%d\n", _index, pt.x, pt.y, fri.DataOffset.LowPart));
	/* this method assumes that the caller has made these adjustments.
	pt.y -= _vc->_vi.FrameMargin.y;
	pt.x -= _vc->_vi.FrameMargin.x;
	*/
	int col, row;
	col = (pt.x - _vc->_vi.OffsetColWidth) / _vc->_vi.ColWidth / COL_LEN_PER_DUMP_BYTE;
	row = pt.y / _vc->_vi.RowHeight;
	if (col < 0)
		return false;
	if (col >= (int)_vc->_vi.BytesPerRow)
	{
		// a click in the ascii dump area.
		// re-calculate the column position. 1st endpoint is on an ascii column.
		col = (pt.x - _vc->_vi.OffsetColWidth - COL_LEN_PER_DUMP_BYTE *_vc->_vi.BytesPerRow*_vc->_vi.ColWidth - _vc->_vi.ColWidth)/ _vc->_vi.ColWidth;
		// check the range.
		if (col > (int)_vc->_vi.BytesPerRow)
			return false;
		if (col < 0)
			col = 0;
		// raise the _ascii flag. 2nd endpoint that OnLButtonUp detects will fall on an ascii column, too. 
		_ascii = true;
	}
	// a click in the hex dump area.
	if (_ri->DataLength > 0)
	{
		// an existing region
		_regionCap.PriorDataOffset = _ri->DataOffset;
		_regionCap.PriorDataLength = _ri->DataLength;
		_regionCap.StartOffset = _regionCap.EndOffset = _ri->DataOffset;
		LARGE_INTEGER offset, delta1, delta2;
		offset.QuadPart = fri.DataOffset.QuadPart + row * _vc->_vi.BytesPerRow + col;
		delta1.QuadPart = offset.QuadPart - _ri->DataOffset.QuadPart;
		delta2.QuadPart = _ri->DataOffset.QuadPart - offset.QuadPart + _ri->DataLength;
		if (delta1.QuadPart <= delta2.QuadPart)
		{
			// cursor is closer to region's starting location.
			// move the start offset to the ending location. keep the end offset at starting location.
			_regionCap.StartOffset.QuadPart += _ri->DataLength;
			row = (int)((LONG)(_regionCap.StartOffset.QuadPart - fri.DataOffset.QuadPart) / _vc->_vi.BytesPerRow);
			col = (int)((LONG)(_regionCap.StartOffset.QuadPart - fri.DataOffset.QuadPart) % _vc->_vi.BytesPerRow);
		}
		else
		{
			// cursor is closer to region's ending location.
			// keep the start offset at starting location. move the end offset to ending location.
			_regionCap.EndOffset.QuadPart += _ri->DataLength;
			row = (int)((LONG)(_regionCap.StartOffset.QuadPart - fri.DataOffset.QuadPart) / _vc->_vi.BytesPerRow);
			col = (int)((LONG)(_regionCap.StartOffset.QuadPart - fri.DataOffset.QuadPart) % _vc->_vi.BytesPerRow);
		}
		_regionCap.Pt1 = { col*(int)_vc->_vi.ColWidth / COL_LEN_PER_DUMP_BYTE + (int)_vc->_vi.OffsetColWidth, row*(int)_vc->_vi.RowHeight };
		_regionCap.StartTick = GetTickCount();
		_regionCap.Captured = true;
	}
	else
	{
		// a new region
		_regionCap.Pt1 = pt;
		_regionCap.StartOffset.QuadPart = fri.DataOffset.QuadPart + row * _vc->_vi.BytesPerRow + col;
		_regionCap.StartTick = GetTickCount();
		_regionCap.Captured = false;
	}
	_vc->_vi.SelectionOffset.QuadPart = (LONGLONG)(row + _vc->_vi.CurRow)*_vc->_vi.BytesPerRow;
	_vc->invalidate();
	DBGPRINTF((L"StartCapture: Captured=%d, Asc=%d; {%d,%d}=>[%d,%d]; ofs=%I64d~%I64d, tick=%X; flags=%X\n", _regionCap.Captured, _ascii, pt.x, pt.y, row, col, _regionCap.StartOffset.QuadPart, _regionCap.EndOffset.QuadPart, _regionCap.StartTick, _ri->Flags));
	return true;
}

bool CRegionSelect::continueOp(POINT pt, FILEREADINFO &fri)
{
	if (!_regionCap.StartTick)
		return false;
	if (!_regionCap.Captured)
	{
		LONG dt = (LONG)GetTickCount() - _regionCap.StartTick;
		if (dt < 2 * 50)
		{
			DBGPUTS((L"OnMouseMove: idling"));
			return false;
		}
		//SetCapture(_vc->_hwnd);
		_regionCap.Captured = true;
	}
	renderCapturedRegion(pt);
	return true;
}

/* watch out! srcData may be NULL.
*/
void CRegionOperation::setRegion(int index, LONGLONG dataOffset, ULONG dataLength, LPBYTE srcData, DUMPREGIONINFO *ri)
{
	ZeroMemory(ri, FIELD_OFFSET(DUMPREGIONINFO, RegionType));
	size_t colorIndex = index % MAX_DUMPREGIONCOLORS;
	ri->DataOffset.QuadPart = dataOffset;
	ri->DataLength = dataLength;
	ri->BackColor = s_regionInteriorColor[colorIndex];
	ri->BorderColor = s_regionBorderColor[colorIndex];
	ri->NeedBorder = true;
/*#ifdef _DEBUG
	if (srcData)
		CopyMemory(ri->Data, srcData, min(ri->DataLength, sizeof(ri->Data)));
#endif//_DEBUG*/
}

bool CRegionSelect::stopOp(POINT pt, FILEREADINFO &fri)
{
	if (!_regionCap.Captured)
		return false;
	//ReleaseCapture();
	_regionCap.Captured = false;
	if (renderCapturedRegion(pt) == -1)
		return false;
	
	setRegion(_index, _regionCap.PriorDataOffset.QuadPart, _regionCap.PriorDataLength, fri.DataBuffer + _ri->DataOffset.QuadPart - fri.DataOffset.QuadPart, _ri);
	DBGPRINTF((L"CRegionSelect::stopOp(%d): Flags=%X; pt=(%d:%d); DataOffset=%d\n", _index, _ri->Flags, pt.x, pt.y, fri.DataOffset.LowPart));
	_vc->invalidate();
	return !empty();
}

int CRegionSelect::renderCapturedRegion(POINT pt)
{
	ULONG dt = GetTickCount() - _regionCap.StartTick;
	pt.y -= _vc->_vi.FrameMargin.y;
	pt.x -= _vc->_vi.FrameMargin.x;
	int col = (pt.x - _vc->_vi.OffsetColWidth) / _vc->_vi.ColWidth / COL_LEN_PER_DUMP_BYTE;
	int row = pt.y / _vc->_vi.RowHeight;
	if (col < 0)
	{
		DBGPRINTF((L"NotCapturing.1: {%d,%d}=>[%d,%d]; deltaT=%d\n", pt.x, pt.y, row, col, dt));
		return -1;
	}
	if (col >= (int)_vc->_vi.BytesPerRow)
	{
		if (!_ascii)
		{
			DBGPRINTF((L"NotCapturing.2: {%d,%d}=>[%d,%d]; deltaT=%d\n", pt.x, pt.y, row, col, dt));
			return -1;
		}
		// re-calculate the column position. 1st endpoint is on an ascii column.
		col = (pt.x - _vc->_vi.OffsetColWidth - COL_LEN_PER_DUMP_BYTE * _vc->_vi.BytesPerRow*_vc->_vi.ColWidth - _vc->_vi.ColWidth) / _vc->_vi.ColWidth;
		// check the range.
		if (col > (int)_vc->_vi.BytesPerRow)
		{
			DBGPRINTF((L"NotCapturing.3: {%d,%d}=>[%d,%d]; deltaT=%d\n", pt.x, pt.y, row, col, dt));
			return -1;
		}
		if (col < 0)
			col = 0;
		// fall through to calculate a corresponding content position.
	}

	_regionCap.Pt2 = pt;
	_regionCap.EndOffset.QuadPart = _vc->_fri.DataOffset.QuadPart + row * _vc->_vi.BytesPerRow + col;

	ZeroMemory(_ri, FIELD_OFFSET(DUMPREGIONINFO, RegionType));
	if (_regionCap.EndOffset.QuadPart != _regionCap.StartOffset.QuadPart)
	{
		if (_regionCap.EndOffset.QuadPart > _regionCap.StartOffset.QuadPart)
		{
			_ri->DataOffset = _regionCap.StartOffset;
			_ri->DataLength = (ULONG)(_regionCap.EndOffset.QuadPart - _regionCap.StartOffset.QuadPart);
		}
		else
		{
			_ri->DataOffset = _regionCap.EndOffset;
			_ri->DataLength = (ULONG)(_regionCap.StartOffset.QuadPart - _regionCap.EndOffset.QuadPart);
		}
		_ri->BackColor = s_regionInteriorColor[0];
		_ri->BorderColor = s_regionBorderColor[3];
		_ri->NeedBorder = true;
/*#ifdef _DEBUG
		CopyMemory(_ri->Data, _vc->_fri.DataBuffer + _ri->DataOffset.QuadPart, min(_ri->DataLength, sizeof(_ri->Data)));
#endif//#ifdef _DEBUG*/

		bool dirChanged = false;
		bool endpt1Backward = false;
		bool endpt2Backward = false;
		if (_ri->DataOffset.QuadPart < _regionCap.PriorDataOffset.QuadPart)
		{
			if (!_regionCap.Endpt1Backward)
				dirChanged = true;
			endpt1Backward = true;
		}
		else if (_regionCap.Endpt1Backward)
		{
			dirChanged = true;
		}
		if (_ri->DataOffset.QuadPart + _ri->DataLength < _regionCap.PriorDataOffset.QuadPart + _regionCap.PriorDataLength)
		{
			if (!_regionCap.Endpt2Backward)
				dirChanged = true;
			endpt2Backward = true;
		}
		else if (_regionCap.Endpt2Backward)
		{
			dirChanged = true;
		}
		if (dirChanged)
		{
			RECT rc1 = _regionGeo.enclosingHexRect;
			RECT rc2 = _regionGeo.enclosingAscRect;
			InflateRect(&rc1, 8, 8);
			InflateRect(&rc2, 8, 8);
			DBGPRINTF((L"ErasingRect: {%d,%d,%d,%d} <== {%d,%d,%d,%d}\n", rc1.left, rc1.top, rc1.right, rc1.bottom, _regionGeo.enclosingHexRect.left, _regionGeo.enclosingHexRect.top, _regionGeo.enclosingHexRect.right, _regionGeo.enclosingHexRect.bottom));
			_vc->invalidate(&rc1);
			_vc->invalidate(&rc2);
			_vc->update();
			/* this causes background to white out.
			RedrawWindow(_vc->_hwnd, &rc, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
			*/
		}

		_vc->setVC();
		draw(_vc->_hdc, true);
		_vc->releaseVC();

		_regionCap.PriorDataOffset = _ri->DataOffset;
		_regionCap.PriorDataLength = _ri->DataLength;
		_regionCap.Endpt1Backward = endpt1Backward;
		_regionCap.Endpt2Backward = endpt2Backward;
	}
	/*DBGPRINTF((L"Capturing: {%d,%d}=>[%d,%d]; '%S'; ofs=%I64d, len=%d; dT=%d; %I64d~%I64d; %X\n", pt.x, pt.y, row, col, _ri->Data, _ri->DataOffset.QuadPart, _ri->DataLength, dt, _regionCap.StartOffset.QuadPart, _regionCap.EndOffset.QuadPart, _ri->Flags));*/
	return (int)_ri->DataLength;
}

void CRegionOperation::draw(HDC hdc, bool saveGeometry)
{
	if (!_ri)
		return;

	UINT j;
	int cx, cy, cx2;
	LARGE_INTEGER regionStart, regionEnd, dataEnd;
	ULONG regionLength;
	HPEN hp, hp0;
	POINT pt[8] = { 0 };
	ULONG np; // number of points
	LONG hitRow1, hitRow2;
	LONG hitCol1, hitCol2;
#ifdef CREGIONOP_CLIPS_REGION
	HRGN hrgn = NULL;
#endif//#ifdef CREGIONOP_CLIPS_REGION

	if (saveGeometry)
		ZeroMemory(&_regionGeo, sizeof(_regionGeo));

	dataEnd.QuadPart = (LONGLONG)_vc->_fri.DataLength;

	regionStart.QuadPart = regionEnd.QuadPart = _ri->DataOffset.QuadPart - _vc->_fri.DataOffset.QuadPart;
	regionEnd.QuadPart += _ri->DataLength;
	regionLength = _ri->DataLength;

	int regionInView = 0;
	if ((regionStart.QuadPart >= 0) && (regionStart.QuadPart < dataEnd.QuadPart))
	{
		// beginning part of the region is visible.
		regionInView |= 1;
		if (regionEnd.QuadPart < dataEnd.QuadPart)
		{
			// trailling part is visible too.
			regionInView |= 2;
		}
	}
	else if ((regionStart.QuadPart < 0) &&
		(regionEnd.QuadPart >= 0 && regionEnd.QuadPart <= dataEnd.QuadPart))
	{
		// only the trailling part of the region is visible.
		// trim the region by cutting off its beginning portion.
		if (regionLength)
			regionInView |= 2;
	}
#ifdef LARGER_THAN_PAGEFUL_REGION_DRAWS_SIDE_BORDERLINES_TOO
	else if (regionStart.QuadPart < 0 && regionEnd.QuadPart > dataEnd.QuadPart)
	{
		// neither top or bottom edge of the region is visible. but the current display page belongs to the middle part of the region.
		regionInView |= 4;
	}
#endif//#ifdef LARGER_THAN_PAGEFUL_REGION_DRAWS_SIDE_BORDERLINES_TOO

	if (regionInView)
	{
		cx = _vc->_vi.OffsetColWidth;
		cy = 0;
		if (regionStart.QuadPart < 0)
		{
			hitRow1 = (LONG)((regionStart.QuadPart - _vc->_vi.BytesPerRow - 1) / _vc->_vi.BytesPerRow);
			hitCol1 = (LONG)(_vc->_vi.BytesPerRow + (regionStart.QuadPart % _vc->_vi.BytesPerRow));
		}
		else
		{
			hitRow1 = (LONG)(regionStart.QuadPart / _vc->_vi.BytesPerRow);
			hitCol1 = (LONG)(regionStart.QuadPart % _vc->_vi.BytesPerRow);
		}
		hitRow2 = (LONG)(regionEnd.QuadPart / _vc->_vi.BytesPerRow);
		hitCol2 = (LONG)(regionEnd.QuadPart % _vc->_vi.BytesPerRow);
#ifdef LARGER_THAN_PAGEFUL_REGION_DRAWS_SIDE_BORDERLINES_TOO
		if ((hitRow1 < (LONG)_vc->_vi.RowsShown) || (regionInView & 4))
#else//#ifdef LARGER_THAN_PAGEFUL_REGION_DRAWS_SIDE_BORDERLINES_TOO
		if (hitRow1 < (LONG)_vc->_vi.RowsShown)
#endif//#ifdef LARGER_THAN_PAGEFUL_REGION_DRAWS_SIDE_BORDERLINES_TOO
		{
#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
			np = _vc->measureHexDumpRegion(pt, cx, cy, hitRow1, hitRow2, hitCol1, hitCol2, regionInView);
#else//#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
			// matched data is in the current view.
			np = 0;
			// draw a perimeter line around the found data in the hex values column.
			if (hitRow1 == hitRow2)
			{
				// a special case with a 4-cornered rectangular region
				// each group of 2 hex digits is followed by one white space,
				// giving rise to the multiplication factor of 3.
				/*0*/pt[np].x = cx + 3 * hitCol1*_vc->_vi.ColWidth;
				pt[np++].y = cy + hitRow1 * _vc->_vi.RowHeight;
				/*1*/pt[np].x = cx + 3 * hitCol2*_vc->_vi.ColWidth - _vc->_vi.ColWidth;
				pt[np++].y = pt[0].y;
				/*2*/pt[np].x = pt[1].x;
				pt[np++].y = pt[1].y + _vc->_vi.RowHeight;
				/*3*/pt[np].x = pt[0].x;
				pt[np++].y = pt[2].y;
			}
			else if (hitCol2 == 0)
			{
				// a special case - the region is 6 cornered
				// starting corner given by {hitCol1, hitRow1}.
				/*0*/pt[np].x = cx + 3 * hitCol1*_vc->_vi.ColWidth;
				pt[np++].y = cy + hitRow1 * _vc->_vi.RowHeight;
				// right-top corner
				/*1*/pt[np].x = cx + 3 * _vc->_vi.BytesPerRow*_vc->_vi.ColWidth - _vc->_vi.ColWidth;
				pt[np++].y = pt[0].y;
				// right-bottom corner
				/*2*/pt[np].x = pt[1].x;
				pt[np++].y = cy + hitRow2 * _vc->_vi.RowHeight;
				// left-bottom corner
				/*3*/pt[np].x = cx;
				pt[np++].y = pt[2].y;
				// left-top(-1) corner
				/*4*/pt[np].x = pt[3].x;
				pt[np++].y = pt[0].y + _vc->_vi.RowHeight;
				// move toward starting corner and stay on a row just below it
				/*7*/pt[np].x = pt[0].x;
				pt[np++].y = pt[4].y;
			}
#ifdef LARGER_THAN_PAGEFUL_REGION_DRAWS_SIDE_BORDERLINES_TOO
			else if (regionInView & 4)
			{
				/*0*/pt[np].x = cx;
				pt[np++].y = -FRAMEMARGIN_CY;
				/*1*/pt[np].x = cx + 3 * _vc->_vi.BytesPerRow*_vc->_vi.ColWidth - _vc->_vi.ColWidth;
				pt[np++].y = pt[0].y;
				/*2*/pt[np].x = pt[1].x;
				pt[np++].y = _vc->_vi.FrameSize.cy + FRAMEMARGIN_CY;
				/*3*/pt[np].x = pt[0].x;
				pt[np++].y = pt[2].y;
			}
#endif//#ifdef LARGER_THAN_PAGEFUL_REGION_DRAWS_SIDE_BORDERLINES_TOO
			else
			{
				// general case with a 8-cornered region
				// the ending byte of the region sits between beginning and ending columns.
				/*0*/pt[np].x = cx + 3 * hitCol1*_vc->_vi.ColWidth;
				pt[np++].y = cy + hitRow1 * _vc->_vi.RowHeight;
				/*1*/pt[np].x = cx + 3 * _vc->_vi.BytesPerRow*_vc->_vi.ColWidth - _vc->_vi.ColWidth;
				pt[np++].y = pt[0].y;
				/*2*/pt[np].x = pt[1].x;
				pt[np++].y = cy + hitRow2 * _vc->_vi.RowHeight;
				/*3*/pt[np].x = cx + 3 * hitCol2*_vc->_vi.ColWidth - _vc->_vi.ColWidth;
				pt[np++].y = pt[2].y;
				/*4*/pt[np].x = pt[3].x;
				pt[np++].y = pt[3].y + _vc->_vi.RowHeight;
				/*5*/pt[np].x = cx;
				pt[np++].y = pt[4].y;
				/*6*/pt[np].x = cx;
				pt[np++].y = pt[0].y + _vc->_vi.RowHeight;
				/*7*/pt[np].x = pt[0].x;
				pt[np++].y = pt[6].y;
			}
#endif//#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE

			if (_vc->_vi.CurCol > 0 && _vc->_vi.BytesPerRow > DEFAULT_HEX_BYTES_PER_ROW)
			{
				cx2 = cx + (COL_LEN_PER_DUMP_BYTE * _vc->_vi.BytesPerRow - _vc->_vi.CurCol)*_vc->_vi.ColWidth - _vc->_vi.ColWidth;
				if (cx2 > _vc->_vi.FrameSize.cx - 1)
					cx2 = _vc->_vi.FrameSize.cx - 1;
				for (j = 0; j < np; j++)
				{
					pt[j].x -= _vc->_vi.CurCol*_vc->_vi.ColWidth;
					if (pt[j].x < (int)cx)
						pt[j].x = cx;
					else if (pt[j].x > cx2)
						pt[j].x = cx2;
				}
			}
			else
			{
				cx2 = _vc->_vi.FrameSize.cx - 1;
				for (j = 0; j < np; j++)
				{
					if (pt[j].x > cx2)
						pt[j].x = cx2;
				}
			}

			for (j = 0; j < np; j++)
			{
				pt[j].x += DRAWTEXT_MARGIN_CX;
				pt[j].y += DRAWTEXT_MARGIN_CY;
			}

			if (_ri->BackColor || _ri->ForeColor)
			{
				HFONT hf0 = NULL;
				if (_ri->RenderAsHeader)
					hf0 = (HFONT)SelectObject(hdc, _vc->_hfontHeader);
				COLORREF bc = GetBkColor(hdc);
				COLORREF fc = GetTextColor(hdc);
				if (_ri->BackColor)
					SetBkColor(hdc, _ri->BackColor);
				if (_ri->ForeColor)
					SetTextColor(hdc, _ri->ForeColor);

				ULONG nextLen;
				ULONG len = regionLength;
				LARGE_INTEGER offset = regionStart;
				if (!(regionInView & 1))
				{
					len = regionEnd.LowPart;
					offset.QuadPart = 0;
				}
				while (len)
				{
					nextLen = _vc->drawRowBytes(hdc, offset, len);
					offset.QuadPart += len - nextLen;
					len = nextLen;
				}

				SetBkColor(hdc, bc);
				SetTextColor(hdc, fc);
				if (hf0)
					SelectObject(hdc, hf0);
			}

			if (_ri->NeedBorder)
			{
#ifdef CREGIONOP_CLIPS_REGION
				RECT fr = { 0, 0,_vc->_vi.FrameSize.cx,_vc->_vi.FrameSize.cy };
				/*
				POINT origPt;
				GetWindowOrgEx(hdc, &origPt);
				OffsetRect(&fr, origPt.x, origPt.y);
				*/
				hrgn = CreateRectRgn(fr.left, fr.top, fr.right, fr.bottom);
				int clipres = SelectClipRgn(hdc, hrgn);
#endif//#ifdef CREGIONOP_CLIPS_REGION

				if (_ri->BorderColor)
				{
					hp = CreatePen(PS_SOLID, 0, _ri->BorderColor);
					hp0 = (HPEN)SelectObject(hdc, hp);
				}
				else
					hp = hp0 = NULL;

				MoveToEx(hdc, pt[0].x, pt[0].y, NULL);
				for (j = 1; j < np; j++)
				{
					LineTo(hdc, pt[j].x, pt[j].y);
				}
				LineTo(hdc, pt[0].x, pt[0].y);

				if (hp0)
					SelectObject(hdc, hp0);
				if (hp)
					DeleteObject(hp);
			}

			if (saveGeometry)
			{
				_regionGeo.regionInView = regionInView;
				_regionGeo.hexRow1 = hitRow1;
				_regionGeo.hexRow2 = hitRow2;
				_regionGeo.hexCol1 = hitCol1;
				_regionGeo.hexCol2 = hitCol2;
				_regionGeo.setRect(pt, np, _vc->_vi.FrameSize, _regionGeo.enclosingHexRect);
				DBGPRINTF((L"EnclosingHexRect: {%d,%d,%d,%d}\n", _regionGeo.enclosingHexRect.left, _regionGeo.enclosingHexRect.top, _regionGeo.enclosingHexRect.right, _regionGeo.enclosingHexRect.bottom));
			}

			if (((int)cx + (COL_LEN_PER_DUMP_BYTE * (int)_vc->_vi.BytesPerRow - (int)_vc->_vi.CurCol)*(int)_vc->_vi.ColWidth) < _vc->_vi.FrameSize.cx)
			{
#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
				np = _vc->measureTextDumpRegion(hdc, pt, cx, cy, hitRow1, hitRow2, hitCol1, hitCol2, regionInView, regionStart, regionLength);
#else//#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE
				// draw a perimeter line around the found data in the ASCII text column.
				np = 0;
				cx2 = (COL_LEN_PER_DUMP_BYTE * _vc->_vi.BytesPerRow + RIGHT_SPACING_COL_LEN)*_vc->_vi.ColWidth;
				if (hitRow1 == hitRow2)
				{
					/*0*/pt[np].x = cx + cx2 + hitCol1 * _vc->_vi.ColWidth;
					pt[np++].y = cy + hitRow1 * _vc->_vi.RowHeight;
					/*1*/pt[np].x = cx + cx2 + hitCol2 * _vc->_vi.ColWidth;
					pt[np++].y = pt[0].y;
					/*2*/pt[np].x = pt[1].x;
					pt[np++].y = pt[1].y + _vc->_vi.RowHeight;
					/*3*/pt[np].x = pt[0].x;
					pt[np++].y = pt[2].y;
				}
				else if (hitCol2 == 0)
				{
					// a special case - the region is 6 cornered
					// starting corner given by {hitCol1, hitRow1}.
					/*0*/pt[np].x = cx + cx2 + hitCol1 * _vc->_vi.ColWidth;
					pt[np++].y = cy + hitRow1 * _vc->_vi.RowHeight;
					// right-top corner
					/*1*/pt[np].x = cx + cx2 + _vc->_vi.BytesPerRow*_vc->_vi.ColWidth;
					pt[np++].y = pt[0].y;
					// right-bottom corner
					/*2*/pt[np].x = pt[1].x;
					pt[np++].y = cy + hitRow2 * _vc->_vi.RowHeight;
					// left-bottom corner
					/*3*/pt[np].x = cx + cx2;
					pt[np++].y = pt[2].y;
					// left-top(-1) corner
					/*4*/pt[np].x = pt[3].x;
					pt[np++].y = pt[0].y + _vc->_vi.RowHeight;
					// move toward starting corner and stay on a row just below it
					/*7*/pt[np].x = pt[0].x;
					pt[np++].y = pt[4].y;
				}
#ifdef LARGER_THAN_PAGEFUL_REGION_DRAWS_SIDE_BORDERLINES_TOO
				else if (regionInView & 4)
				{
					/*0*/pt[np].x = cx + cx2;
					pt[np++].y = -FRAMEMARGIN_CY;
					/*1*/pt[np].x = cx + cx2 + _vc->_vi.BytesPerRow * _vc->_vi.ColWidth;
					pt[np++].y = pt[0].y;
					/*2*/pt[np].x = pt[1].x;
					pt[np++].y = _vc->_vi.FrameSize.cy + FRAMEMARGIN_CY;
					/*3*/pt[np].x = pt[0].x;
					pt[np++].y = pt[2].y;
				}
#endif//#ifdef LARGER_THAN_PAGEFUL_REGION_DRAWS_SIDE_BORDERLINES_TOO
				else
				{
					/*0*/pt[np].x = cx + cx2 + hitCol1 * _vc->_vi.ColWidth;
					pt[np++].y = cy + hitRow1 * _vc->_vi.RowHeight;
					/*1*/pt[np].x = cx + cx2 + _vc->_vi.BytesPerRow*_vc->_vi.ColWidth;
					pt[np++].y = pt[0].y;
					/*2*/pt[np].x = pt[1].x;
					pt[np++].y = cy + hitRow2 * _vc->_vi.RowHeight;
					/*3*/pt[np].x = cx + cx2 + hitCol2 * _vc->_vi.ColWidth;
					pt[np++].y = pt[2].y;
					/*4*/pt[np].x = pt[3].x;
					pt[np++].y = pt[3].y + _vc->_vi.RowHeight;
					/*5*/pt[np].x = cx + cx2;
					pt[np++].y = pt[4].y;
					/*6*/pt[np].x = pt[5].x;
					pt[np++].y = pt[0].y + _vc->_vi.RowHeight;
					/*7*/pt[np].x = pt[0].x;
					pt[np++].y = pt[6].y;
				}
#endif//#ifdef BINHEXVIEW_SUPPORTS_TEXTDUMP_OVERRIDE

				if (_vc->_vi.CurCol > 0 && _vc->_vi.BytesPerRow > DEFAULT_HEX_BYTES_PER_ROW)
				{
					for (j = 0; j < np; j++)
					{
						pt[j].x -= _vc->_vi.CurCol*_vc->_vi.ColWidth;
						if (pt[j].x < (int)cx)
							pt[j].x = cx;
					}
				}
				cx2 = _vc->_vi.FrameSize.cx - 1;
				for (j = 0; j < np; j++)
				{
					if (pt[j].x > cx2)
						pt[j].x = cx2;
				}

				for (j = 0; j < np; j++)
				{
					pt[j].x += DRAWTEXT_MARGIN_CX;
					pt[j].y += DRAWTEXT_MARGIN_CY;
				}

				if (_ri->NeedBorder)
				{
					if (_ri->BorderColor)
					{
						hp = CreatePen(PS_SOLID, 0, _ri->BorderColor);
						hp0 = (HPEN)SelectObject(hdc, hp);
					}
					else
						hp = hp0 = NULL;

					MoveToEx(hdc, pt[0].x, pt[0].y, NULL);
					for (j = 1; j < np; j++)
					{
						LineTo(hdc, pt[j].x, pt[j].y);
					}
					LineTo(hdc, pt[0].x, pt[0].y);

					if (hp0)
						SelectObject(hdc, hp0);
					if (hp)
						DeleteObject(hp);
				}

				if (saveGeometry)
				{
					_regionGeo.ascRow1 = hitRow1;
					_regionGeo.ascRow2 = hitRow2;
					_regionGeo.ascCol1 = hitCol1;
					_regionGeo.ascCol2 = hitCol2;
					_regionGeo.setRect(pt, np, _vc->_vi.FrameSize, _regionGeo.enclosingAscRect);
					//DBGPRINTF((L"EnclosingAscRect: {%d,%d,%d,%d}\n", _regionGeo.enclosingAscRect.left, _regionGeo.enclosingAscRect.top, _regionGeo.enclosingAscRect.right, _regionGeo.enclosingAscRect.bottom));
				}
			}
		}
		else
			hitRow1 = 0;

#ifdef CREGIONOP_CLIPS_REGION
		if (hrgn)
		{
			SelectClipRgn(hdc, _vc->_hrgnClip);
			DeleteObject(hrgn);
		}
#endif//#ifdef CREGIONOP_CLIPS_REGION
	}
}


void CRegionCollection::redrawAll(HDC hdc, BinhexView *vc)
{
	for (size_t i = 0; i < _n; i++)
	{
		CRegionOperation(&at(i), vc).draw(hdc);
	}
}

int CRegionCollection::queryNextItem(LARGE_INTEGER contentPos, ULARGE_INTEGER fileSize, bool goBack)
{
	DUMPREGIONINFO *ri;
	LARGE_INTEGER offset0 = contentPos;
	if (0 <= _itemVisited && _itemVisited < (int)_n)
	{
		ri = &at(_itemVisited);
		if (!goBack)
		{
			offset0 = ri->DataOffset;
			offset0.QuadPart += ri->DataLength;
		}
		else if (offset0.QuadPart > ri->DataOffset.QuadPart)
			offset0 = ri->DataOffset;
	}
	LARGE_INTEGER offset2;
	LONGLONG delta0 = fileSize.QuadPart, delta;
	int i0 = -1;
	int i = 0;
	for (; i < (int)_n; i++)
	{
		if (i == _itemVisited)
			continue;
		ri = &at(i);
		offset2 = ri->DataOffset;
		offset2.QuadPart += ri->DataLength;
		if (ri->DataOffset.QuadPart <= offset0.QuadPart && offset0.QuadPart < offset2.QuadPart)
			continue;
		if (goBack)
			delta = offset0.QuadPart - ri->DataOffset.QuadPart;
		else
			delta = ri->DataOffset.QuadPart - offset0.QuadPart;
		if (delta < 0)
			delta += fileSize.QuadPart;
		if (delta < delta0)
		{
			delta0 = delta;
			i0 = i;
		}
	}
	return i0;
}

DUMPREGIONINFO *CRegionCollection::gotoItem(int itemIndex)
{
	_itemVisited = itemIndex;
	if (itemIndex != -1)
	{
		return &at(itemIndex);
	}
	return NULL;
}

DUMPREGIONINFO *CRegionCollection::gotoNextItem(LARGE_INTEGER contentPos, ULARGE_INTEGER fileSize, bool goBack)
{
	_itemVisited = queryNextItem(contentPos, fileSize, goBack);
	if (_itemVisited != -1)
	{
		return &at(_itemVisited);
	}
	return NULL;
}

/*
return value:
0..N : index of a region found here.
-1: a read-only region is at contentPos but not reported.
-2: no exising region is found here.
-3: mouse cursor is outside the hex display area.
*/
int CRegionCollection::queryRegion(LARGE_INTEGER contentPos, bool excludeIfReadonly)
{
	_regionHere = -3;
	if (contentPos.QuadPart != -1LL)
	{
		_regionHere = -2;
		int i;
		for (i = (int)_n-1; i >= 0; i--)
		{
			DUMPREGIONINFO &ai = at(i);
			if (!(ai.Flags & DUMPREGIONINFO_FLAG_NESTED))
				continue;
			if (ai.DataOffset.QuadPart <= contentPos.QuadPart && contentPos.QuadPart < (ai.DataOffset.QuadPart + (LONGLONG)ai.DataLength))
			{
				_regionHere = (int)i;
				DBGPRINTF((L"queryRegion(%I64d): index=%d (NESTED)\n", contentPos.QuadPart, i));
				return _regionHere;
			}
		}
		for (i = (int)_n - 1; i >= 0; i--)
		{
			DUMPREGIONINFO &ai = at(i);
			if (ai.DataOffset.QuadPart <= contentPos.QuadPart && contentPos.QuadPart < (ai.DataOffset.QuadPart + (LONGLONG)ai.DataLength))
			{
				if (excludeIfReadonly)
				{
					if (ai.Flags & DUMPREGIONINFO_FLAG_READONLY)
					{
						_regionHere = -1;
						break;
					}
				}
				_regionHere = (int)i;
				DBGPRINTF_VERBOSE((L"queryRegion(%I64d): index=%d\n", contentPos.QuadPart, i));
				break;
			}
		}
	}
	return _regionHere;
}

void CRegionCollection::clearWritableOnly(AnnotationCollection &annots)
{
	//simpleList<DUMPREGIONINFO>::clear();
	for (size_t i = _n; i > 0; i--)
	{
		DUMPREGIONINFO &ri = at(i-1);
		if (ri.Flags & DUMPREGIONINFO_FLAG_READONLY)
			continue;
		if (ri.AnnotCtrlId)
		{
			// update the associated annotation. clear the region association.
			int annotId = annots.queryByCtrlId(ri.AnnotCtrlId);
			if (annotId != -1)
				annots.at(annotId).RegionId = 0;
		}
		remove(i);
	}
	_regionHere = -1;
}

void CRegionCollection::clear(AnnotationCollection &annots)
{
	for (size_t i = _n; i > 0; i--)
	{
		DUMPREGIONINFO &ri = at(i - 1);
		if (ri.AnnotCtrlId)
		{
			// remove the associated annotation.
			int annotId = annots.queryByCtrlId(ri.AnnotCtrlId);
			if (annotId != -1)
				annots.remove(annotId);
		}
		remove(i);
	}
	_regionHere = -1;
}

bool CRegionCollection::changeColorHere(int colorIndex)
{
	if (_regionHere == -1)
		return false;
	if (colorIndex < 0 || colorIndex >= MAX_DUMPREGIONCOLORS)
		return false;
	DUMPREGIONINFO &ri = at(_regionHere);
	ri.BackColor = s_regionInteriorColor[colorIndex];
	ri.BorderColor = s_regionBorderColor[colorIndex];
	return true;
}
