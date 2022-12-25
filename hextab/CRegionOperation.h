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
#include "lib.h"
#include "BinhexView.h"


struct DUMPREGIONCAPTURE
{
	ULONG StartTick;
	POINT Pt1, Pt2;
	LARGE_INTEGER StartOffset, EndOffset;
	LARGE_INTEGER PriorDataOffset;
	ULONG PriorDataLength;
	bool Captured;
	bool Endpt1Backward, Endpt2Backward;
};

struct DUMPREGIONGEOMETRY
{
	int regionInView;
	RECT enclosingHexRect;
	RECT enclosingAscRect;
	LONG hexRow1, hexRow2;
	LONG hexCol1, hexCol2;
	LONG ascRow1, ascRow2;
	LONG ascCol1, ascCol2;

	void setRect(POINT *pt, ULONG np, SIZE frameSize, RECT &rect)
	{
		rect.left = frameSize.cx;
		rect.top = frameSize.cy;
		ULONG j;
		for (j = 0; j < np; j++)
		{
			if (pt[j].x && pt[j].x < rect.left)
				rect.left = pt[j].x;
			if (pt[j].y && pt[j].y < rect.top)
				rect.top = pt[j].y;
		}
		rect.right = rect.left;
		rect.bottom = rect.top;
		for (j = 0; j < np; j++)
		{
			if (pt[j].x > rect.right)
				rect.right = pt[j].x;
			if (pt[j].y > rect.bottom)
				rect.bottom = pt[j].y;
		}
	}
};


/* CRegionOperation - performs creation and display operations of a single meta region.
*/
class CRegionOperation
{
public:
	CRegionOperation(BinhexView *vc) : _vc(vc), _ri(NULL) {}
	CRegionOperation(DUMPREGIONINFO *ri, BinhexView *vc) : _vc(vc), _ri(ri) {}
	CRegionOperation(const CRegionOperation *ro) : _vc(ro->_vc), _ri(ro->_ri) {}
	virtual ~CRegionOperation() { clear(); }

	operator DUMPREGIONINFO *() const { return _ri; }

	virtual void draw(HDC hdc, bool saveGeometry = false);
	virtual void clear() {}

	static void setRegion(int index, LONGLONG dataOffset, ULONG dataLength, LPBYTE srcData, DUMPREGIONINFO *ri);

protected:
	BinhexView *_vc;
	DUMPREGIONINFO *_ri;
	DUMPREGIONGEOMETRY _regionGeo;
};

/* CRegionSelect - extends CRegionOperation by adding drag and drop support.
*/
class CRegionSelect : public CRegionOperation
{
public:
	CRegionSelect(simpleList<DUMPREGIONINFO> *regions, int index, BinhexView *vc) : CRegionOperation(vc), _regionCap{0}, _ascii(false)
	{
		if (index < 0)
		{
			_index = (int)regions->size();
			_ri = regions->alloc();
			if (index == -1) // the new region is nested in an existing read-only region.
				_ri->Flags |= DUMPREGIONINFO_FLAG_NESTED;
			return;
		}
		_index = index;
		_ri = &regions->at(index);
	}
	int _index;

	bool startOp(POINT pt, FILEREADINFO &gi);
	bool continueOp(POINT pt, FILEREADINFO &gi);
	bool stopOp(POINT pt, FILEREADINFO &gi);

protected:
	// this flag is true if a click in ascii dump area starts the selection process.
	bool _ascii;

	struct DUMPREGIONCAPTURE
	{
		ULONG StartTick;
		POINT Pt1, Pt2;
		LARGE_INTEGER StartOffset, EndOffset;
		LARGE_INTEGER PriorDataOffset;
		ULONG PriorDataLength;
		bool Captured;
		bool Endpt1Backward, Endpt2Backward;
	} _regionCap;

	int renderCapturedRegion(POINT pt);
	bool empty()
	{
		if (_ri && _ri->DataLength == 0)
			return true;
		return false;
	}
};

/* CRegionCollection - creates and manages meta regions.
*/
class CRegionCollection : public simpleList<DUMPREGIONINFO>
{
public:
	CRegionCollection() : _roSel(NULL), _regionHere(-1), _itemVisited(-1) {}

	CRegionSelect *_roSel;
	int _regionHere;
	int _itemVisited;

	// methods for responding mouse down, move, and up events.
	bool startSelection(POINT pt, FILEREADINFO &fri, LARGE_INTEGER _contentAtCursor, BinhexView *vc)
	{
		clearRegionVisited();
		ASSERT(_roSel == NULL);
		_regionHere = queryRegion(_contentAtCursor, true);
		// note if _regionHere==-1, the cursor is on an existing read-only region. so, we are creating a nested region within a read-only region.
		_roSel = new CRegionSelect((simpleList<DUMPREGIONINFO>*)this, _regionHere, vc);
		return _roSel->startOp(pt, fri);
	}
	bool continueSelection(POINT pt, FILEREADINFO &fri)
	{
		if (!_roSel)
			return false;
		_roSel->continueOp(pt, fri);
		return true;
	}
	bool stopSelection(POINT pt, FILEREADINFO &fri)
	{
		if (!_roSel)
			return false;
		bool ret = _roSel->stopOp(pt, fri);
		if (!ret)
			simpleList<DUMPREGIONINFO>::remove(_roSel->_index);
		else
			_itemVisited = _roSel->_index;
		delete _roSel;
		_roSel = NULL;
		return ret;
	}
	bool clearSelection()
	{
		if (!_roSel)
			return false;
		delete _roSel;
		_roSel = NULL;
		return true;
	}
	void clearRegionVisited()
	{
		if (_itemVisited != -1)
			_itemVisited = -1; // clear the goto start region.
	}

	DUMPREGIONINFO *gotoNextItem(LARGE_INTEGER contentPos, ULARGE_INTEGER fileSize, bool goBack);
	int queryNextItem(LARGE_INTEGER contentPos, ULARGE_INTEGER fileSize, bool goBack);
	DUMPREGIONINFO *gotoItem(int itemIndex);

	// paint routine to redraw all regions.
	void redrawAll(HDC hdc, BinhexView *vc);
	// find a region containing a content.
	int queryRegion(LARGE_INTEGER contentPos, bool excludeIfReadonly=false);
	// find and remove a region under cursor.
	bool removeHere()
	{
		if (_regionHere >= 0)
		{
			if (at(_regionHere).Flags & DUMPREGIONINFO_FLAG_READONLY)
			{
				_regionHere = -1;
				// return a true to signify that the caller's request has been processed. the object itself has not been removed since it's readonly.
				return true;
			}
			if (remove(_regionHere) >= 0)
			{
				_regionHere = -1;
				return true;
			}
		}
		return false;
	}
	bool changeColorHere(int colorIndex);
	// clean up.
	void clear(class AnnotationCollection &annots);
	void clearWritableOnly(class AnnotationCollection &annots);
};

