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


#define SHAPE_ROTATION_USES_FLTRIG
//#define BLENDOUTLINESHAPE_TESTS_SIMPLELINESHAPE
#define RENDERCIRCLE_USES_GDI_ELLIPSE
#define BLENDSHAPE_BLENDS_CONNECTOR_TOO
//#define BLENDANNOTATION_BLENDS_CONNECTOR_TOO


/* GShapeOperation - performs creation and display operations of a single meta shape.
*/
class GShapeOperation
{
public:
	GShapeOperation(DUMPSHAPEINFO &si, BinhexView *vc) : _si(si), _index(-1), _vc(vc) {}
	GShapeOperation(simpleList<DUMPSHAPEINFO> &shapes, int shapeIndex, BinhexView *vc) : _si(shapes[shapeIndex]), _index(shapeIndex), _vc(vc), _gripId(GRIPID_INVALID) {}
	GShapeOperation(const GShapeOperation *so) : _si(so->_si), _index(so->_index), _vc(so->_vc), _gripId(so->_gripId) {}
	virtual ~GShapeOperation() { clear(); }

	int _index;
	GRIPID _gripId;

	operator DUMPSHAPEINFO &() const { return _si; }

	virtual bool startOp(POINT pt, DUMPSHAPGRIPINFO &gi) { return false; }
	virtual bool continueOp(POINT pt, DUMPSHAPGRIPINFO &gi) { return false; }
	virtual bool stopOp(POINT pt, DUMPSHAPGRIPINFO &gi) { return false; }

	virtual bool highlight() { return false; }
	virtual void clear() {}

	void drawShape(HDC hdc);
	bool containsPt(POINT pt);
	bool hasGripperAt(POINT pt);

protected:
	DUMPSHAPEINFO &_si;
	BinhexView *_vc;

	bool shapeInScreenFrame(RECT &outShapeRect, POINT *outOriginPt = NULL);
	bool shapeInFrame(RECT &outShapeRect, POINT *outOriginPt = NULL, bool grabbed = false);
	POINT _getGripCoords(GRIPID gripId, RECT &shapeRect);
	bool _hasGripperAt(POINT pt, RECT shapeRect);
	bool _clearRotatedShapeRegion();
	void _clearConnectorRegion();
	bool _drawOriginConnector(HDC hdc, POINT originPt, POINT shapePt);
	void _drawGripHandles(HDC hdc, POINT mid[4], POINT corner[4], POINT rotpt);

	void blendShape(HDC hdc, RECT shapeRect, POINT originPt, DWORD transparency);

	const UINT RSFLAG_DRAW_GRIPPERS = 1;
	const UINT RSFLAG_DRAW_ORIGIN = 2;
	const UINT RSFLAG_OUTLINE_ONLY = 4;
	const UINT RSFLAG_LT_AT_ORIGIN = 8;
	const UINT RSFLAG_REPAINT_BKGD = 0x10;
	HRGN renderShape(HDC hdc, RECT shapeRect, POINT originPt, UINT flags = 0, HBITMAP hbmBkgd = NULL);
	HRGN _renderShapeRotated(HDC hdc, RECT shapeRect, POINT originPt, UINT flags, HBITMAP hbmBkgd);
	HRGN _renderShapeRotatedHD(HDC hdc, RECT shapeRect, POINT originPt, UINT flags);
	HRGN _renderLineShapeRotated(HDC hdc, RECT shapeRect, POINT originPt, UINT flags);

	short _calcRotationAngle(POINT v);

#ifndef RENDERCIRCLE_USES_GDI_ELLIPSE
	void _generateCirclePts(RECT shapeRect, simpleList<POINT> &pts)
#endif//#ifndef RENDERCIRCLE_USES_GDI_ELLIPSE
};

/* GShapeSelect - extends GShapeOperation with hit testing and highlighting support.
*/
class GShapeSelect : public GShapeOperation
{
public:
	GShapeSelect(DUMPSHAPEINFO &si, BinhexView *vc) :GShapeOperation(si, vc) {}
	GShapeSelect(simpleList<DUMPSHAPEINFO> &shapes, int shapeIndex, BinhexView *vc) :GShapeOperation(shapes, shapeIndex, vc) {}
	virtual ~GShapeSelect() { clear(); }

	virtual bool highlight(HBITMAP hbmBkgd);
	virtual void clear();

	RECT _hitRect;
};

/* GShapeMove - extends GShapeOperation with support for drag and drop to relocate a shape object.
*/
class GShapeMove : public GShapeOperation
{
public:
	GShapeMove(DUMPSHAPEINFO &si, BinhexView *vc) :GShapeOperation(si, vc) {}
	GShapeMove(simpleList<DUMPSHAPEINFO> &shapes, int shapeIndex, BinhexView *vc) :GShapeOperation(shapes, shapeIndex, vc) {}
	GShapeMove(const GShapeOperation *so) : GShapeOperation(so) {}

	virtual bool startOp(POINT pt, DUMPSHAPGRIPINFO &gi);
	virtual bool continueOp(POINT pt, DUMPSHAPGRIPINFO &gi);
	virtual bool stopOp(POINT pt, DUMPSHAPGRIPINFO &gi);
};

/* GShapeMove - extends GShapeOperation with support for drag and drop to stretch or rotate a shape object.
*/
class GShapeStretch : public GShapeOperation
{
public:
	GShapeStretch(DUMPSHAPEINFO &si, BinhexView *vc) :GShapeOperation(si, vc) {}
	GShapeStretch(simpleList<DUMPSHAPEINFO> &shapes, int shapeIndex, BinhexView *vc) :GShapeOperation(shapes, shapeIndex, vc) {}
	GShapeStretch(const GShapeOperation *so) : GShapeOperation(so) {}

	virtual bool startOp(POINT pt, DUMPSHAPGRIPINFO &gi);
	virtual bool continueOp(POINT pt, DUMPSHAPGRIPINFO &gi);
	virtual bool stopOp(POINT pt, DUMPSHAPGRIPINFO &gi);

protected:
	bool stretchShape(POINT stretchPt, POINT &originPt, RECT &shapeRect);
	HRGN _setupClippingRegion(POINT srcPath[4], POINT offset);
};

// flags for GShapeCollection::clear.
const UINT GSHAPECOLL_CLEAR_SOPDRAG = 1;
const UINT GSHAPECOLL_CLEAR_SOPSEL = 2;
const UINT GSHAPECOLL_CLEAR_SOPGRIP = 4;
const UINT GSHAPECOLL_CLEAR_SOPCURS = 8;

/* GShapeCollection - creates and manages meta shapes.
*/
class GShapeCollection : public simpleList<DUMPSHAPEINFO>
{
public:
	GShapeCollection(BinhexView *vc) : _vc(vc), _sopDrag(NULL), _sopGrip(NULL), _sopSel(NULL), _sopUnderCursor(NULL), _shapeHere(-1), _itemVisited(-1), _shapeGrip{ GRIPID_INVALID }, _hitRect{ 0 } {}
	~GShapeCollection() { clear(); }

	int _shapeHere;
	DUMPSHAPGRIPINFO _shapeGrip;

	void reinit(BinhexView *vc2);

	bool startDrag(POINT pt);
	bool continueDrag(POINT pt);
	bool stopDrag(POINT pt);

	HCURSOR queryCursor();
	DUMPSHAPEINFO *queryHere();
	DUMPSHAPEINFO *gotoItem(int itemIndex);
	int queryNextItem(LARGE_INTEGER contentPos, ULARGE_INTEGER fileSize, bool goBack);
	int queryShapeAt(POINT pt, bool screenCoords = false);
	int queryShapeGripperAt(POINT pt);
	bool highlightShape(size_t shapeId);
	bool removeHere();
	void redrawAll(HDC hdc);

	void clear(UINT flags=-1);

protected:
	BinhexView *_vc;
	GShapeOperation *_sopDrag;
	GShapeOperation *_sopGrip;
	GShapeSelect *_sopSel;
	const GShapeSelect *_sopUnderCursor;
	RECT _hitRect;
	int _itemVisited;
};

