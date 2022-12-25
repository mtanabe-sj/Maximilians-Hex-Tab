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
#include "GShapeOperation.h"
#include "GripCursor.h"

#define GSHAPEOP_CLIPS_REGION


void GShapeOperation::drawShape(HDC hdc)
{
	DBGPRINTF((L"drawShape(%d)\n", _si.Shape));
	POINT originPt;
	RECT shapeRect;
	if (!shapeInFrame(shapeRect, &originPt))
		return;

	blendShape(hdc, shapeRect, originPt, _vc->_shapeOpacity);
}

bool GShapeOperation::shapeInScreenFrame(RECT &outShapeRect, POINT *outOriginPt)
{
	ASSERT(_vc->_highdefVP && _vc->_highdefVP->ScreenCharExts.cx && _vc->_highdefVP->ScreenCharExts.cy); // has HexdumpPrintJob::setViewMapping been called? it initializes _highdefVP->ScreenCharExts.
	BinhexView bv;
	CopyMemory(&bv._vi, &_vc->_vi, sizeof(VIEWINFO));
	bv._fri.DataOffset = _vc->_fri.DataOffset;
	bv._fri.FileSize = _vc->_fri.FileSize;
	BinhexView *vc_ = _vc;
	_vc = &bv;
	_vc->_vi.ColWidth = vc_->_highdefVP->ScreenCharExts.cx; // e.g., 7 pel for 10pt font
	_vc->_vi.RowHeight = vc_->_highdefVP->ScreenCharExts.cy; // 14 pel for 10pt
	_vc->_vi.OffsetColWidth = _vc->_vi.ColWidth*(OFFSET_COL_LEN + LEFT_SPACING_COL_LEN);
	bool ret = shapeInFrame(outShapeRect, outOriginPt);
	_vc = vc_;
	return ret;
}

bool GShapeOperation::shapeInFrame(RECT &outShapeRect, POINT *outOriginPt, bool grabbed)
{
	LARGE_INTEGER shapeStart, shapeEnd, dataEnd;
	dataEnd.QuadPart = 0;// (LONGLONG)_fri.DataLength;

	shapeStart.QuadPart = shapeEnd.QuadPart = _si.DataOffset.QuadPart - _vc->_fri.DataOffset.QuadPart;
	shapeEnd.QuadPart += 2;

	LONG hitRow1, hitRow2;
	LONG hitCol1, hitCol2;

	if (shapeStart.QuadPart < 0)
	{
		hitRow1 = (LONG)((shapeStart.QuadPart - _vc->_vi.BytesPerRow - 1) / _vc->_vi.BytesPerRow);
		hitCol1 = (LONG)(_vc->_vi.BytesPerRow + (shapeStart.QuadPart % _vc->_vi.BytesPerRow));
	}
	else
	{
		hitRow1 = (LONG)(shapeStart.QuadPart / _vc->_vi.BytesPerRow);
		hitCol1 = (LONG)(shapeStart.QuadPart % _vc->_vi.BytesPerRow);
	}
	hitRow2 = (LONG)(shapeEnd.QuadPart / _vc->_vi.BytesPerRow);
	hitCol2 = (LONG)(shapeEnd.QuadPart % _vc->_vi.BytesPerRow);

	POINT originPt;
	originPt.x = _vc->_getHexColCoord(hitCol1, 1, 2) + DRAWTEXT_MARGIN_CX;
	originPt.y = _vc->_vi.RowHeight / 2 + hitRow1 * _vc->_vi.RowHeight + DRAWTEXT_MARGIN_CY;
	if (_vc->_vi.CurCol)
		originPt.x -= _vc->_vi.CurCol*_vc->_vi.ColWidth;
	/*if (originPt.x < (int)_vc->_vi.OffsetColWidth)
	{
		//if (grabbed)
		//	originPt.x = originPt.x;
		if (!grabbed)
			originPt.x = _vc->_vi.OffsetColWidth;
	}*/

	RECT shapeRect;
	shapeRect.left = originPt.x + _vc->_getLogicalXCoord(_si.GeomOffset.x);
	shapeRect.top = originPt.y + _vc->_getLogicalYCoord(_si.GeomOffset.y);
	shapeRect.right = shapeRect.left + _vc->_getLogicalXCoord(_si.GeomSize.cx);
	shapeRect.bottom = shapeRect.top + _vc->_getLogicalYCoord(_si.GeomSize.cy);

	DBGPRINTF_VERBOSE((L"shapeInFrame: shape=%d, flags=%X; rect={%d,%d,%d,%d}, pt={%d,%d}, sz={%d,%d}\n", _si.Shape, _si.Flags, shapeRect.left, shapeRect.top, shapeRect.right, shapeRect.bottom, originPt.x, originPt.y, _si.GeomSize.cx, _si.GeomSize.cy));

	outShapeRect = shapeRect;
	if (outOriginPt)
		*outOriginPt = originPt;

	RECT frameRect = { 0,0,_vc->_vi.FrameSize.cx,_vc->_vi.FrameSize.cy };
	if (PtInRect(&frameRect, *(LPPOINT)&shapeRect.left) || PtInRect(&frameRect, *(LPPOINT)&shapeRect.right))
		return true;
	return false;
}

bool GShapeSelect::highlight(HBITMAP hbmBkgd)
{
	DBGPUTS((L"highlight"));
	POINT originPt;
	RECT shapeRect;
	if (!shapeInFrame(shapeRect, &originPt))
		return false;
	if (!EqualRect(&_hitRect, &shapeRect))
		_vc->invalidateInflated(&_hitRect);
	if (_si.GeomRotation != 0)
		SetRectEmpty(&_hitRect);
	else
		_hitRect = shapeRect;

	HDC hdc = _vc->setVC();
	HRGN hrgn = renderShape(hdc, shapeRect, originPt,
		(_si.Flags & DUMPSHAPEINFO_FLAG_FILLED) ? RSFLAG_DRAW_ORIGIN | RSFLAG_REPAINT_BKGD | RSFLAG_DRAW_GRIPPERS : RSFLAG_DRAW_ORIGIN | RSFLAG_REPAINT_BKGD | RSFLAG_DRAW_GRIPPERS | RSFLAG_OUTLINE_ONLY, hbmBkgd);
	if (hrgn)
		DeleteObject(hrgn);
	_vc->releaseVC();
	return true;
}

bool GShapeOperation::containsPt(POINT pt)
{
	if (_si.GeomRotation != 0)
	{
		HRGN hrgn = CreatePolygonRgn(_si.GeomOuterPts, 4, ALTERNATE);
		BOOL b = PtInRegion(hrgn, pt.x, pt.y);
		DeleteObject(hrgn);
		if (b)
		{
			DBGPRINTF((L"GShapeOperation::containsPt: rotation=%d; res=%d\n", _si.GeomRotation, _index));
			return true;
		}
		return false;
	}
	RECT shapeRect;
	if (!shapeInFrame(shapeRect))
		return false;
	InflateRect(&shapeRect, SHAPEGRIPPER_HALF2 - 1, SHAPEGRIPPER_HALF2 - 1);
	if (PtInRect(&shapeRect, pt))
	{
		DBGPRINTF((L"containsPt: shape=%d\n", _index));
		return true;
	}
	return false;
}

bool GShapeOperation::_hasGripperAt(POINT pt, RECT shapeRect)
{
	int i = GRIPID_GRIP;
	if (_si.Shape == DUMPSHAPEINFO_SHAPE_LINE)
	{
		// skip all corners. line does not support gripping a corner.
		i = START_LINE_SHAPE_GRIP;
	}
	for (; i < MAX_SHAPE_GRIPS; i++)
	{
		// the western handle is actually a rotational pivot. it's not a gripper.
		if (i == GRIPID_SIZEWEST)
			continue;
		POINT gripPt = _getGripCoords((GRIPID)i, shapeRect);
		RECT rc = { gripPt.x, gripPt.y, gripPt.x, gripPt.y };
		InflateRect(&rc, SHAPEGRIPPER_HALF2, SHAPEGRIPPER_HALF2);
		if (PtInRect(&rc, pt))
		{
			_gripId = (GRIPID)i;
			DBGPRINTF((L"queryGripperAt: gripId=%d\n", i));
			return true;
		}
	}
	_gripId = GRIPID_INVALID;
	return false;
}

bool GShapeOperation::hasGripperAt(POINT pt)
{
	RECT shapeRect;
	if (!shapeInFrame(/*_shapes[i], */shapeRect))
		return false;
	return _hasGripperAt(pt, shapeRect);
}

void GShapeSelect::clear()
{
	if (_vc->invalidateInflated(&_hitRect))
		SetRectEmpty(&_hitRect);
	if (_si.GeomRotation)
		_clearRotatedShapeRegion();
	_clearConnectorRegion();
	GShapeOperation::clear();
}

bool GShapeOperation::_clearRotatedShapeRegion()
{
	POINT pts[4];
	CopyMemory(pts, _si.GeomOuterPts, sizeof(pts));
	// compensate for viewport offset OnPaint removes.
	for (int i = 0; i < 4; i++)
	{
		pts[i].x += _vc->_vi.FrameMargin.x; // left top corner
		pts[i].y += _vc->_vi.FrameMargin.y;
	}
	HRGN hrgn = CreatePolygonRgn(pts, 4, ALTERNATE);
	if (!hrgn)
		return false;
	InvalidateRgn(_vc->_hw, hrgn, TRUE);
	DeleteObject(hrgn);
	return true;
}

void GShapeOperation::_clearConnectorRegion()
{
	if (!(_si.Flags & DUMPSHAPEINFO_FLAG_CONNECTOR))
		return;

	_si.Flags &= ~DUMPSHAPEINFO_FLAG_CONNECTOR;

	POINT pts[4];
	pts[0] = pts[1] = _si.GeomConnectorPts[0]; // origin
	pts[2] = pts[3] = _si.GeomConnectorPts[1]; // shape's left edge, mid-point
	if (pts[0].x <= pts[2].x)
	{
		if (pts[0].y <= pts[2].y)
		{
			pts[0].x += SHAPE_CONNECTOR_HALF_WIDTH;
			pts[0].y -= SHAPE_CONNECTOR_HALF_WIDTH;
			pts[1].x -= SHAPE_CONNECTOR_HALF_WIDTH;
			pts[1].y -= SHAPE_CONNECTOR_HALF_WIDTH;
			pts[2].x -= SHAPE_CONNECTOR_HALF_WIDTH;
			pts[2].y += SHAPE_CONNECTOR_HALF_WIDTH;
			pts[3].x += SHAPE_CONNECTOR_HALF_WIDTH;
			pts[3].y += SHAPE_CONNECTOR_HALF_WIDTH;
		}
		else
		{
			pts[0].x -= SHAPE_CONNECTOR_HALF_WIDTH;
			pts[0].y += SHAPE_CONNECTOR_HALF_WIDTH;
			pts[1].x += SHAPE_CONNECTOR_HALF_WIDTH;
			pts[1].y += SHAPE_CONNECTOR_HALF_WIDTH;
			pts[2].x += SHAPE_CONNECTOR_HALF_WIDTH;
			pts[2].y -= SHAPE_CONNECTOR_HALF_WIDTH;
			pts[3].x -= SHAPE_CONNECTOR_HALF_WIDTH;
			pts[3].y -= SHAPE_CONNECTOR_HALF_WIDTH;
		}
	}
	else
	{
		if (pts[0].y <= pts[2].y)
		{
			pts[0].x -= SHAPE_CONNECTOR_HALF_WIDTH;
			pts[0].y -= SHAPE_CONNECTOR_HALF_WIDTH;
			pts[1].x += SHAPE_CONNECTOR_HALF_WIDTH;
			pts[1].y -= SHAPE_CONNECTOR_HALF_WIDTH;
			pts[2].x += SHAPE_CONNECTOR_HALF_WIDTH;
			pts[2].y += SHAPE_CONNECTOR_HALF_WIDTH;
			pts[3].x -= SHAPE_CONNECTOR_HALF_WIDTH;
			pts[3].y += SHAPE_CONNECTOR_HALF_WIDTH;
		}
		else
		{
			pts[0].x -= SHAPE_CONNECTOR_HALF_WIDTH;
			pts[0].y += SHAPE_CONNECTOR_HALF_WIDTH;
			pts[1].x += SHAPE_CONNECTOR_HALF_WIDTH;
			pts[1].y += SHAPE_CONNECTOR_HALF_WIDTH;
			pts[2].x += SHAPE_CONNECTOR_HALF_WIDTH;
			pts[2].y -= SHAPE_CONNECTOR_HALF_WIDTH;
			pts[3].x -= SHAPE_CONNECTOR_HALF_WIDTH;
			pts[3].y -= SHAPE_CONNECTOR_HALF_WIDTH;
		}
	}
	// compensate for viewport offset OnPaint removes.
	for (int i = 0; i < 4; i++)
	{
		pts[i].x += _vc->_vi.FrameMargin.x; // left top corner
		pts[i].y += _vc->_vi.FrameMargin.y;
	}
	HRGN hrgn = CreatePolygonRgn(pts, 4, ALTERNATE);
	if (hrgn)
	{
		InvalidateRgn(_vc->_hw, hrgn, TRUE);
		DeleteObject(hrgn);
	}
	RECT rc = { _si.GeomConnectorPts[0].x - SHAPEGRIPPER_HALF2, _si.GeomConnectorPts[0].y - SHAPEGRIPPER_HALF2, _si.GeomConnectorPts[0].x + SHAPEGRIPPER_HALF3, _si.GeomConnectorPts[0].y + SHAPEGRIPPER_HALF3 };
	InvalidateRect(_vc->_hw, &rc, TRUE);
}

POINT GShapeOperation::_getGripCoords(GRIPID gripId, RECT &shapeRect)
{
	int halfWidth, halfHeight;
	if (_si.GeomRotation != 0)
	{
		// GeomCornerPts: [0]=LT, [1]=LB, [2]=RB, [3]=RT
		if (gripId == GRIPID_SIZENW) // left top corner
			return { _si.GeomCornerPts[0].x, _si.GeomCornerPts[0].y };
		if (gripId == GRIPID_SIZESW) // left bottom corner
			return { _si.GeomCornerPts[1].x, _si.GeomCornerPts[1].y };
		if (gripId == GRIPID_SIZESE) // right bottom corner
			return { _si.GeomCornerPts[2].x, _si.GeomCornerPts[2].y };
		if (gripId == GRIPID_SIZENE) // right top corner
			return { _si.GeomCornerPts[3].x, _si.GeomCornerPts[3].y };

		// GeomMidPts: [0] = left, [1] = top, [2] = right, [3] = bottom
		if (gripId == GRIPID_SIZENORTH) // top midway
			return { _si.GeomMidPts[1].x, _si.GeomMidPts[1].y };
		if (gripId == GRIPID_SIZEWEST) // left midway
			return { _si.GeomMidPts[0].x, _si.GeomMidPts[0].y };
		if (gripId == GRIPID_SIZESOUTH) // bottom midway
			return { _si.GeomMidPts[3].x, _si.GeomMidPts[3].y };
		if (gripId == GRIPID_SIZEEAST) // right midway
			return { _si.GeomMidPts[2].x, _si.GeomMidPts[2].y };

		// rotation pivot at quarter length from right edge
		if (gripId == GRIPID_CURVEDARROW)
		{
			halfWidth = (_si.GeomMidPts[2].x - _si.GeomMidPts[0].x) / 2;
			halfHeight = (_si.GeomMidPts[2].y - _si.GeomMidPts[0].y) / 2;
			int x = _si.GeomMidPts[0].x + halfWidth + halfWidth / 2;
			int y = _si.GeomMidPts[0].y + halfHeight + halfHeight / 2;
			return { x, y };
		}
	}
	else
	{
		if (gripId == GRIPID_SIZENW) // left top corner
			return { shapeRect.left, shapeRect.top };
		if (gripId == GRIPID_SIZESW) // left bottom corner
			return { shapeRect.left, shapeRect.bottom };
		if (gripId == GRIPID_SIZESE) // right bottom corner
			return { shapeRect.right, shapeRect.bottom };
		if (gripId == GRIPID_SIZENE) // right top corner
			return { shapeRect.right, shapeRect.top };

		halfWidth = (shapeRect.right - shapeRect.left) / 2;
		halfHeight = (shapeRect.bottom - shapeRect.top) / 2;
		if (gripId == GRIPID_SIZENORTH) // top midway
			return { shapeRect.left + halfWidth, shapeRect.top };
		if (gripId == GRIPID_SIZEWEST) // left midway
			return { shapeRect.left, shapeRect.top + halfHeight };
		if (gripId == GRIPID_SIZESOUTH) // bottom midway
			return { shapeRect.left + halfWidth, shapeRect.bottom };
		if (gripId == GRIPID_SIZEEAST) // right midway
			return { shapeRect.right, shapeRect.top + halfHeight };

		// rotation pivot at quarter length from right edge
		if (gripId == GRIPID_CURVEDARROW)
			return { shapeRect.right - halfWidth / 2, shapeRect.top + halfHeight };
	}
	return { 0 };
}

#ifndef RENDERCIRCLE_USES_GDI_ELLIPSE
void GShapeOperation::_generateCirclePts(RECT shapeRect, simpleList<POINT> &pts)
{
	double radius = _si.GeomRadius;
	double x0 = shapeRect.left + (shapeRect.right - shapeRect.left) / 2;
	double y0 = shapeRect.top + (shapeRect.bottom - shapeRect.top) / 2;
	double theta;
	POINT pt, pt0 = { -1,-1 };
	for (size_t i = 1; i < 360; i++)
	{
		if (i == 90)
		{
			pt.x = lrint(x0);
			pt.y = lrint(y0 - radius);
		}
		else if (i == 180)
		{
			pt.x = lrint(x0 - radius);
			pt.y = lrint(y0);
		}
		else if (i == 270)
		{
			pt.x = lrint(x0);
			pt.y = lrint(y0 + radius);
		}
		else
		{
			theta = M_PI * (double)i / 180.0;
			pt.x = lrint(x0 + radius * cos(theta));
			pt.y = lrint(y0 - radius * sin(theta));
		}
		if (pt.x == pt0.x && pt.y == pt0.y)
			continue;
		pts.add(pt);
		pt0 = pt;
	}
}
#endif//#ifndef RENDERCIRCLE_USES_GDI_ELLIPSE

HRGN GShapeOperation::_renderLineShapeRotated(HDC hdc, RECT shapeRect, POINT originPt, UINT flags)
{
	HRGN hrgn = NULL;
	HBRUSH hbr = NULL, hbr0 = NULL;

	HPEN hpen = CreatePen(PS_SOLID, _vc->screenToLogical(_si.StrokeThickness), _si.OutlineColor);
	HPEN hpen0 = (HPEN)SelectObject(hdc, hpen);

	int dx2 = shapeRect.right - shapeRect.left;
	int dy2 = shapeRect.bottom - shapeRect.top;
	int dx = dx2 / 2;
	int dy = dy2 / 2;

	POINT cornerRotation;
	POINT mid[4]; // will hold midway points. [0]=left, [1]=top, [2]=right, [3]=bottom
	POINT corner[4]; // will hold corner points. [0]=LT, [1]=LB, [2]=RB, [3]=RT

	/*
	shapeRect before rotation:
	
   LT                  dx
    +------------------+------------------+
    +                                     + dy
    +------------------+------------------+
	                                     RB

	shapeRect after rotation:

                                     .\ mid[2]
                                 .     \
                             .         . RB
                         .         .
                     .         . 
                 .         .
             .         .
    LT   .         .   \
     . +-------.---    GeomRotation   -----+
     \ +   .            |                  +  GeomSize.cy
mid[0]\.-----------------------------------+
	               GeomSize.cx

	*/
	mid[0] = { shapeRect.left, shapeRect.top + dy };
#ifdef SHAPE_ROTATION_USES_FLTRIG
	FLTRIG flt(mid[0], (long)_si.GeomRotation);
	mid[2] = flt.transformIX(dx2);
	if (_vc->_highdefVP)
	{
		// compensate the y-coordinate in mid[2]. it's the y-component of a vector of length of GeomSize.cx rotated to angle GeomRotation. compensation is necessary because it's essentially a screen length. use _getLogicalYCoord to translate it to a re-calibrated logical length.
#ifdef _DEBUG
		int dmy1 = mid[2].y - mid[0].y;
#endif//#ifdef _DEBUG
		POINT pt2 = flt.decomposeIX(_si.GeomSize.cx);
		int dmy2 = _vc->_getLogicalYCoord(pt2.y);
		mid[2].y = mid[0].y+dmy2;
	}
	cornerRotation = flt.decomposeHalfIY(dy2);
	//POINT cornerRotation, gripExtension;
	//gripExtension = flt.decomposeIY(_si.GeomSize.cy + 2 * SHAPEGRIPPER_HALF, &cornerRotation);
#else//#ifdef SHAPE_ROTATION_USES_FLTRIG
	double theta = M_PI * (double)(long)_si.GeomRotation / 180.0;
	double cos_theta = cos(theta);
	double sin_theta = sin(theta);
	double r = shapeRect.right - shapeRect.left;
	double x2 = r * cos_theta;
	double y2 = r * sin_theta;
	mid[2].x = lrint(x2) + mid[0].x;
	mid[2].y = lrint(y2) + mid[0].y;
	double theta2 = theta + 0.5*M_PI;
	y2 = 0.5 * (double)_si.GeomSize.cy * cos_theta;
	x2 = 0.5 * (double)_si.GeomSize.cy * sin_theta;
	cornerRotation.cx = lrint(x2);
	cornerRotation.cy = lrint(y2);
	double extendedLen = _si.GeomSize.cy + 2 * SHAPEGRIPPER_HALF;
	y2 = extendedLen * cos_theta;
	x2 = extendedLen * sin_theta;
	gripExtension.cx = cornerRotation.cx + lrint(x2);
	gripExtension.cy = cornerRotation.cy + lrint(y2);
#endif//#ifdef SHAPE_ROTATION_USES_FLTRIG

	if (_si.GeomOptions == DUMPSHAPEINFO_OPTION_ARROW
		|| _si.GeomOptions == DUMPSHAPEINFO_OPTION_ARROW2)
	{
		MoveToEx(hdc, mid[0].x, mid[0].y, NULL);
		LineTo(hdc, mid[2].x, mid[2].y);

#ifdef SHAPE_ROTATION_USES_FLTRIG
		long bladeLength = 3 * dy;
		POINT blade = flt.decomposeIX(bladeLength);
		POINT barb = flt.decomposeIY(dy);
#else//#ifdef SHAPE_ROTATION_USES_FLTRIG
		double bladeLength = 3.0 * dy;
		POINT blade = { lrint(bladeLength * cos_theta), lrint(bladeLength * sin_theta) };
		POINT barb = { lrint(dy * sin_theta), lrint(dy * cos_theta) };
#endif//#ifdef SHAPE_ROTATION_USES_FLTRIG

		HBRUSH hbr2 = CreateSolidBrush(_si.OutlineColor);
		HPEN hpen2 = CreatePen(PS_SOLID, 0, _si.OutlineColor);
		SelectObject(hdc, hbr2);
		SelectObject(hdc, hpen2);
		POINT arrowhead[3];
		arrowhead[0].x = mid[2].x;
		arrowhead[0].y = mid[2].y;
		arrowhead[1].x = mid[2].x - blade.x + barb.x;
		arrowhead[1].y = mid[2].y - blade.y - barb.y;
		arrowhead[2].x = mid[2].x - blade.x - barb.x;
		arrowhead[2].y = mid[2].y - blade.y + barb.y;
		Polygon(hdc, arrowhead, 3);
		if (_si.GeomOptions == DUMPSHAPEINFO_OPTION_ARROW2)
		{
			arrowhead[0].x = mid[0].x;
			arrowhead[0].y = mid[0].y;
			arrowhead[1].x = mid[0].x + blade.x + barb.x;
			arrowhead[1].y = mid[0].y + blade.y - barb.y;
			arrowhead[2].x = mid[0].x + blade.x - barb.x;
			arrowhead[2].y = mid[0].y + blade.y + barb.y;
			Polygon(hdc, arrowhead, 3);
		}
		DeleteObject(hpen2);
		DeleteObject(hbr2);
	}
	else if (_si.GeomOptions == DUMPSHAPEINFO_OPTION_WAVY)
	{
		// draw a wavy line
		// sinusoidal wave: A = a0 * sin(theta)
		double wavelen = _vc->screenToLogical(16.0); // one cycle per 16 pixels
		double freq = 1.0 / wavelen; // one cycle per 16 pixels
		double a0 = (double)dy;
		simpleList<POINT> fpts;
		for (long t = mid[0].x; t < mid[2].x; t++)
		{
			double dt = (double)(t - mid[0].x);
			double phi = 2.0*M_PI*freq*dt;
			double A = a0 * sin(phi);
#ifdef SHAPE_ROTATION_USES_FLTRIG
			POINT pT = flt.transformX(dt);
			POINT pA = flt.decomposeY(A);
#else//#ifdef SHAPE_ROTATION_USES_FLTRIG
			double tx = dt * cos_theta;
			double ty = dt * sin_theta;
			POINT pT = { lrint(tx) + mid[0].x, lrint(ty) + mid[0].y };
			double Ax = a0 * sin_theta;
			double Ay = a0 * cos_theta;
			POINT pA = { lrint(Ax) + mid[0].x, lrint(Ay) + mid[0].y };
#endif//#ifdef SHAPE_ROTATION_USES_FLTRIG
			POINT fpt = { pT.x + pA.x, pT.y - pA.y };
			fpts.add(fpt);
		}
		MoveToEx(hdc, fpts[0].x, fpts[0].y, NULL);
		for (size_t i = 1; i < fpts.size(); i++)
			LineTo(hdc, fpts[i].x, fpts[i].y);
	}
	else // a straight line
	{
		MoveToEx(hdc, mid[0].x, mid[0].y, NULL);
		LineTo(hdc, mid[2].x, mid[2].y);
	}

	mid[1].x = mid[0].x + (mid[2].x - mid[0].x) / 2;
	mid[1].y = mid[0].y + (mid[2].y - mid[0].y) / 2;
	mid[3] = mid[1];
	mid[1].x += cornerRotation.x; // top midway
	mid[1].y -= cornerRotation.y;
	mid[3].x -= cornerRotation.x; // bottom midway
	mid[3].y += cornerRotation.y;

	corner[0] = corner[1] = mid[0];
	corner[2] = corner[3] = mid[2];
	corner[0].x += cornerRotation.x; // left top corner
	corner[0].y -= cornerRotation.y;
	corner[1].x -= cornerRotation.x; // left bottom corner
	corner[1].y += cornerRotation.y;
	corner[2].x -= cornerRotation.x; // right bottom corner
	corner[2].y += cornerRotation.y;
	corner[3].x += cornerRotation.x; // right top corner
	corner[3].y -= cornerRotation.y;

	if (_si.Flags & DUMPSHAPEINFO_FLAG_FILLED)
	{
		hrgn = CreatePolygonRgn(corner, 4, ALTERNATE);
	}

	if (hbr0)
		SelectObject(hdc, hbr0);
	if (hbr)
		DeleteObject(hbr);
	if (hpen0)
		SelectObject(hdc, hpen0);
	if (hpen)
		DeleteObject(hpen);

	if (flags & RSFLAG_DRAW_ORIGIN)
		_drawOriginConnector(hdc, originPt, mid[0]);

	if (flags & RSFLAG_DRAW_GRIPPERS)
		_drawGripHandles(hdc, mid, corner, _getGripCoords(GRIPID_CURVEDARROW, shapeRect));
	/*{
		hpen = (HPEN)GetStockObject(BLACK_PEN);
		hpen0 = (HPEN)SelectObject(hdc, hpen);
		hbr = (HBRUSH)GetStockObject(WHITE_BRUSH);
		hbr0 = (HBRUSH)SelectObject(hdc, hbr);
		RECT rc;
		if (_si.Shape != DUMPSHAPEINFO_SHAPE_LINE)
		{
			// left top corner
			rc = { corner[0].x - SHAPEGRIPPER_HALF, corner[0].y - SHAPEGRIPPER_HALF, corner[0].x + SHAPEGRIPPER_HALF, corner[0].y + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
			// left bottom corner
			rc = { corner[1].x - SHAPEGRIPPER_HALF, corner[1].y - SHAPEGRIPPER_HALF, corner[1].x + SHAPEGRIPPER_HALF, corner[1].y + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
			// right bottom corner
			rc = { corner[2].x - SHAPEGRIPPER_HALF, corner[2].y - SHAPEGRIPPER_HALF, corner[2].x + SHAPEGRIPPER_HALF, corner[2].y + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
			// right top corner
			rc = { corner[3].x - SHAPEGRIPPER_HALF, corner[3].y - SHAPEGRIPPER_HALF, corner[3].x + SHAPEGRIPPER_HALF, corner[3].y + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

			// top midway
			rc = { mid[1].x - SHAPEGRIPPER_HALF, mid[1].y - SHAPEGRIPPER_HALF, mid[1].x + SHAPEGRIPPER_HALF, mid[1].y + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
			// bottom midway
			rc = { mid[3].x - SHAPEGRIPPER_HALF, mid[3].y - SHAPEGRIPPER_HALF, mid[3].x + SHAPEGRIPPER_HALF, mid[3].y + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
		}
		// right midway
		rc = { mid[2].x - SHAPEGRIPPER_HALF, mid[2].y - SHAPEGRIPPER_HALF, mid[2].x + SHAPEGRIPPER_HALF, mid[2].y + SHAPEGRIPPER_HALF };
		Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
		// left midway
		hbr = CreateSolidBrush(COLORREF_LTBLUE);
		hbr0 = (HBRUSH)SelectObject(hdc, hbr);
		rc = { mid[0].x - SHAPEGRIPPER_HALF1, mid[0].y - SHAPEGRIPPER_HALF1, mid[0].x + SHAPEGRIPPER_HALF1, mid[0].y + SHAPEGRIPPER_HALF1 };
		Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);
		SelectObject(hdc, hbr0);
		DeleteObject(hbr);
		// rotation pivot point
		POINT rotpt = _getGripCoords(GRIPID_CURVEDARROW, shapeRect);
		rc = { rotpt.x - SHAPEGRIPPER_HALF1, rotpt.y - SHAPEGRIPPER_HALF1,rotpt.x + SHAPEGRIPPER_HALF1,rotpt.y + SHAPEGRIPPER_HALF1 };
		Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);

		SelectObject(hdc, hpen0);
	}*/

	CopyMemory(_si.GeomMidPts, mid, sizeof(mid));
	CopyMemory(_si.GeomCornerPts, corner, sizeof(corner));
	DBGPRINTF_VERBOSE((L"_renderLineShapeRotated.1: rotate=%d; corner={%d:%d,%d:%d,%d:%d,%d:%d}\n", _si.GeomRotation, corner[0].x, corner[0].y, corner[1].x, corner[1].y, corner[2].x, corner[2].y, corner[3].x, corner[3].y));

	/*
	// 4 corners outside the grippers. swap x, y corner adjustments
	SIZE gripSize = { gripExtension.x,gripExtension.y };
	corner[0].x -= gripSize.cx; // left top corner
	corner[0].y -= gripSize.cy;
	corner[1].x -= gripSize.cx; // left bottom corner
	corner[1].y += gripSize.cy;
	corner[2].x += gripSize.cx; // right bottom corner
	corner[2].y += gripSize.cy;
	corner[3].x += gripSize.cx; // right top corner
	corner[3].y -= gripSize.cy;
	*/
	// note: initially, SHAPEGRIPPER_HALF was used instead of SHAPEGRIPPER_HALF2. latter was adopted later to clean after the grib handles. farthest vertices of the handle boxes would end up being left behind as artifacts.
	POINT corner2[4];
	corner2[0] = flt.transformPt({ -SHAPEGRIPPER_HALF2, -_si.GeomSize.cy / 2 - SHAPEGRIPPER_HALF2 }); // left top corner
	corner2[1] = flt.transformPt({ -SHAPEGRIPPER_HALF2, _si.GeomSize.cy / 2 + SHAPEGRIPPER_HALF2 }); // left bottom corner
	corner2[2] = flt.transformPt({ _si.GeomSize.cx + SHAPEGRIPPER_HALF2, _si.GeomSize.cy / 2 + SHAPEGRIPPER_HALF2 });  // right bottom corner
	corner2[3] = flt.transformPt({ _si.GeomSize.cx + SHAPEGRIPPER_HALF2, -_si.GeomSize.cy / 2 - SHAPEGRIPPER_HALF2 }); // right top corner
	CopyMemory(_si.GeomOuterPts, corner2, sizeof(corner2));

	DBGPRINTF_VERBOSE((L"_renderLineShapeRotated.2: gripSize={%d:%d}; outer={%d:%d,%d:%d,%d:%d,%d:%d}\n", SHAPEGRIPPER_HALF2, SHAPEGRIPPER_HALF2, corner2[0].x, corner2[0].y, corner2[1].x, corner2[1].y, corner2[2].x, corner2[2].y, corner2[3].x, corner2[3].y));

	return hrgn;
}

RECT _polygonToRect(POINT *pts, int len)
{
	RECT rc = { 10000, 10000, 0, 0 };
	for (int i = 0; i < len; i++)
	{
		if (pts[i].x < rc.left)
			rc.left = pts[i].x;
		if (pts[i].y < rc.top)
			rc.top = pts[i].y;
	}
	for (int i = 0; i < len; i++)
	{
		if (pts[i].x > rc.right)
			rc.right = pts[i].x;
		if (pts[i].y > rc.bottom)
			rc.bottom = pts[i].y;
	}
	return rc;
}

short GShapeOperation::_calcRotationAngle(POINT v)
{
	short rot = 0;
	if (v.x == 0)
	{
		// use the current angle as a hint
		if (0 <= _si.GeomRotation && _si.GeomRotation < 180)
			rot = 90;
		else //if (180 <= _si.GeomRotation && _si.GeomRotation < 360)
			rot = 270;
	}
	else if (v.y == 0)
	{
		if (90 < _si.GeomRotation && _si.GeomRotation <= 270)
			rot = 180;
		else //if ((0 <= _si.GeomRotation && _si.GeomRotation <= 90) || (270 < _si.GeomRotation && _si.GeomRotation <= 360))
			rot = 0;
	}
	else
	{
		double m = (double)v.y / (double)v.x;
		double theta = atan(m);
		if (v.x < 0)
			theta = M_PI + theta; // 2nd or 3rd quadrant
		rot = LOWORD(lrint(180.0*theta / M_PI));
	}
	return rot;
}

HRGN GShapeOperation::_renderShapeRotatedHD(HDC hdc, RECT shapeRect, POINT originPt, UINT flags)
{
	ASSERT(!(flags & (RSFLAG_REPAINT_BKGD | RSFLAG_REPAINT_BKGD)));
	ASSERT(_si.Shape == DUMPSHAPEINFO_SHAPE_RECTANGLE || _si.Shape == DUMPSHAPEINFO_SHAPE_ELLIPSE);

	RECT scrnShapeRect = { 0 };
	shapeInScreenFrame(scrnShapeRect);
	int cx1 = scrnShapeRect.right - scrnShapeRect.left;
	int cy1 = scrnShapeRect.bottom - scrnShapeRect.top;
	POINT halfPt1 = { cx1 / 2, cy1 / 2 };
	FLTRIG flt1({ scrnShapeRect.left, scrnShapeRect.top + halfPt1.y }, (long)_si.GeomRotation);
	POINT pt1 = flt1.decomposeIX(1000);
	POINT pt2 =
	{
		_vc->_getLogicalXCoord(pt1.x),
		_vc->_getLogicalYCoord(pt1.y),
	};
	short rot2 = _calcRotationAngle(pt2);

	RECT shapeRect2 =
	{
		_vc->_getLogicalXCoord(scrnShapeRect.left),
		_vc->_getLogicalYCoord(scrnShapeRect.top),
		_vc->_getLogicalXCoord(scrnShapeRect.right),
		_vc->_getLogicalYCoord(scrnShapeRect.bottom)
	};
	int cx2 = shapeRect2.right - shapeRect2.left;
	int cy2 = shapeRect2.bottom - shapeRect2.top;

	POINT halfPt2 = { cx2 / 2, cy2 / 2 };
	FLTRIG flt({ shapeRect2.left, shapeRect2.top + halfPt2.y }, (long)rot2);

	POINT rotationOffset = flt.decomposeIY(halfPt2.y);

	int gmode0 = GetGraphicsMode(hdc);
	SetGraphicsMode(hdc, GM_ADVANCED);

	XFORM xf = { 0 };
	xf.eM11 = (FLOAT)flt.cos_theta;
	xf.eM12 = (FLOAT)flt.sin_theta;
	xf.eM21 = (FLOAT)-flt.sin_theta;
	xf.eM22 = (FLOAT)flt.cos_theta;
	xf.eDx = (FLOAT)shapeRect2.left + rotationOffset.x;
	xf.eDy = (FLOAT)shapeRect2.top + halfPt2.y - rotationOffset.y;
	SetWorldTransform(hdc, &xf);

	HRGN hrgn = NULL;
	HBRUSH hbr = NULL, hbr0 = NULL;
	HPEN hpen = NULL, hpen0 = NULL;

	hpen = CreatePen(PS_SOLID, _vc->screenToLogical(_si.StrokeThickness), _si.OutlineColor);
	hpen0 = (HPEN)SelectObject(hdc, hpen);

	if (_si.Flags & DUMPSHAPEINFO_FLAG_FILLED)
	{
		hbr = CreateSolidBrush(_si.InteriorColor);
		hbr0 = (HBRUSH)SelectObject(hdc, hbr);
	}

	if (_si.Shape == DUMPSHAPEINFO_SHAPE_ELLIPSE)
		Ellipse(hdc, 0, 0, cx2, cy2);
	else //if (_si.Shape == DUMPSHAPEINFO_SHAPE_RECTANGLE)
		Rectangle(hdc, 0, 0, cx2, cy2);

	xf.eM11 = 1.0;
	xf.eM12 = 0;
	xf.eM21 = 0;
	xf.eM22 = 1.0;
	xf.eDx = 0;
	xf.eDy = 0;
	SetWorldTransform(hdc, &xf);

	SetGraphicsMode(hdc, gmode0);

	if (hbr0)
		SelectObject(hdc, hbr0);
	if (hbr)
		DeleteObject(hbr);
	if (hpen0)
		SelectObject(hdc, hpen0);
	if (hpen)
		DeleteObject(hpen);
	return hrgn;
}

HRGN GShapeOperation::_renderShapeRotated(HDC hdc, RECT shapeRect, POINT originPt, UINT flags, HBITMAP hbmBkgd)
{
	int cx = shapeRect.right - shapeRect.left;
	int cy = shapeRect.bottom - shapeRect.top;

	POINT halfPt = { cx / 2, cy / 2 };

	RECT rc;
	POINT cornerRotation;
	POINT mid[4]; // will hold midway points. [0]=left, [1]=top, [2]=right, [3]=bottom
	POINT corner[4]; // will hold corner points. [0]=LT, [1]=LB, [2]=RB, [3]=RT

	mid[0] = { shapeRect.left, shapeRect.top + halfPt.y };
	FLTRIG flt(mid[0], (long)_si.GeomRotation);
	mid[2] = flt.transformIX(cx);
	cornerRotation = flt.decomposeHalfIY(cy);

	mid[1].x = mid[0].x + (mid[2].x - mid[0].x) / 2;
	mid[1].y = mid[0].y + (mid[2].y - mid[0].y) / 2;
	mid[3] = mid[1];
	mid[1].x += cornerRotation.x; // top midway
	mid[1].y -= cornerRotation.y;
	mid[3].x -= cornerRotation.x; // bottom midway
	mid[3].y += cornerRotation.y;

	corner[0] = corner[1] = mid[0];
	corner[2] = corner[3] = mid[2];
	corner[0].x += cornerRotation.x; // left top corner
	corner[0].y -= cornerRotation.y;
	corner[1].x -= cornerRotation.x; // left bottom corner
	corner[1].y += cornerRotation.y;
	corner[2].x -= cornerRotation.x; // right bottom corner
	corner[2].y += cornerRotation.y;
	corner[3].x += cornerRotation.x; // right top corner
	corner[3].y -= cornerRotation.y;

	if (flags & RSFLAG_REPAINT_BKGD)
	{
		rc = _polygonToRect(corner, 4);
		HDC hdc1 = CreateCompatibleDC(hdc);
		if (hbmBkgd)
		{
			HBITMAP hbm10 = (HBITMAP)SelectObject(hdc1, hbmBkgd);
			BitBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hdc1, rc.left, rc.top, SRCCOPY);
			SelectObject(hdc1, hbm10);
		}
		else
		{
			HBITMAP hbm1 = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
			HBITMAP hbm10 = (HBITMAP)SelectObject(hdc1, hbm1);
			_vc->repaint(BINHEXVIEW_REPAINT_RESET_FONT | BINHEXVIEW_REPAINT_DUMP_ONLY, hdc1);
			BitBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hdc1, rc.left, rc.top, SRCCOPY);
			SelectObject(hdc1, hbm10);
			DeleteObject(hbm1);
		}
		DeleteDC(hdc1);
	}

	int gmode0 = GetGraphicsMode(hdc);
	SetGraphicsMode(hdc, GM_ADVANCED);

	POINT rotationOffset = flt.decomposeIY(halfPt.y);

	XFORM xf = { 0 };
	xf.eM11 = (FLOAT)flt.cos_theta;
	xf.eM12 = (FLOAT)flt.sin_theta;
	xf.eM21 = (FLOAT)-flt.sin_theta;
	xf.eM22 = (FLOAT)flt.cos_theta;
	xf.eDx = (FLOAT)shapeRect.left + rotationOffset.x;
	xf.eDy = (FLOAT)shapeRect.top + halfPt.y - rotationOffset.y;
	SetWorldTransform(hdc, &xf);

	HRGN hrgn = NULL;
	HBRUSH hbr = NULL, hbr0 = NULL;
	HPEN hpen = NULL, hpen0 = NULL;

	hpen = CreatePen(PS_SOLID, _si.StrokeThickness, _si.OutlineColor);
	hpen0 = (HPEN)SelectObject(hdc, hpen);

	if (_si.Flags & DUMPSHAPEINFO_FLAG_FILLED)
	{
		hbr = CreateSolidBrush(_si.InteriorColor);
		hbr0 = (HBRUSH)SelectObject(hdc, hbr);
	}

	if (flags & RSFLAG_REPAINT_BKGD)
	{
		HDC hdc2 = CreateCompatibleDC(hdc);
		HBITMAP hbm2 = CreateCompatibleBitmap(hdc, cx, cy);
		HBITMAP hbm20 = (HBITMAP)SelectObject(hdc2, hbm2);
		HBRUSH hbr20 = (HBRUSH)SelectObject(hdc2, hbr);
		HPEN hpen20 = (HPEN)SelectObject(hdc2, hpen);

		RECT rc2 = { 0,0,cx,cy };
		FillRect(hdc2, &rc2, GetStockBrush(WHITE_BRUSH));

		if (_si.Shape == DUMPSHAPEINFO_SHAPE_RECTANGLE)
		{
			Rectangle(hdc2, 0, 0, cx, cy);
		}
		else if (_si.Shape == DUMPSHAPEINFO_SHAPE_ELLIPSE)
		{
			Ellipse(hdc2, 0, 0, cx, cy);
		}

		BLENDFUNCTION bf = { AC_SRC_OVER, 0, BHV_NORMAL_SHAPE_OPACITY, 0 };
		AlphaBlend(hdc, 0, 0, cx, cy, hdc2, 0, 0, cx, cy, bf);

		SelectObject(hdc2, hpen20);
		SelectObject(hdc2, hbr20);
		SelectObject(hdc2, hbm20);
		DeleteDC(hdc2);
		DeleteObject(hbm2);
	}
	else
	{
		if (_si.Shape == DUMPSHAPEINFO_SHAPE_RECTANGLE)
			Rectangle(hdc, 0, 0, cx, cy);
		else if (_si.Shape == DUMPSHAPEINFO_SHAPE_ELLIPSE)
			Ellipse(hdc, 0, 0, cx, cy);
		else { ASSERT(FALSE); }
	}

	if (_si.Flags & DUMPSHAPEINFO_FLAG_FILLED)
	{
		BeginPath(hdc);
		if (_si.Shape == DUMPSHAPEINFO_SHAPE_RECTANGLE)
			Rectangle(hdc, -1, -1, cx + 2, cy + 2);
		else if (_si.Shape == DUMPSHAPEINFO_SHAPE_ELLIPSE)
			Ellipse(hdc, -1, -1, cx + 2, cy + 2);
		EndPath(hdc);
		hrgn = PathToRegion(hdc);
	}

	xf.eM11 = 1.0;
	xf.eM12 = 0;
	xf.eM21 = 0;
	xf.eM22 = 1.0;
	xf.eDx = 0;
	xf.eDy = 0;
	SetWorldTransform(hdc, &xf);

	SetGraphicsMode(hdc, gmode0);

	if (hbr0)
		SelectObject(hdc, hbr0);
	if (hbr)
		DeleteObject(hbr);
	if (hpen0)
		SelectObject(hdc, hpen0);
	if (hpen)
		DeleteObject(hpen);

	if (flags & RSFLAG_DRAW_ORIGIN)
		_drawOriginConnector(hdc, originPt, mid[0]);

	if (flags & RSFLAG_DRAW_GRIPPERS)
		_drawGripHandles(hdc, mid, corner, _getGripCoords(GRIPID_CURVEDARROW, shapeRect));
	/*
	{
		hpen = (HPEN)GetStockObject(BLACK_PEN);
		hpen0 = (HPEN)SelectObject(hdc, hpen);
		hbr = (HBRUSH)GetStockObject(WHITE_BRUSH);
		hbr0 = (HBRUSH)SelectObject(hdc, hbr);

		if (_si.Shape != DUMPSHAPEINFO_SHAPE_LINE)
		{
			// left top corner
			rc = { corner[0].x - SHAPEGRIPPER_HALF, corner[0].y - SHAPEGRIPPER_HALF, corner[0].x + SHAPEGRIPPER_HALF, corner[0].y + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
			// left bottom corner
			rc = { corner[1].x - SHAPEGRIPPER_HALF, corner[1].y - SHAPEGRIPPER_HALF, corner[1].x + SHAPEGRIPPER_HALF, corner[1].y + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
			// right bottom corner
			rc = { corner[2].x - SHAPEGRIPPER_HALF, corner[2].y - SHAPEGRIPPER_HALF, corner[2].x + SHAPEGRIPPER_HALF, corner[2].y + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
			// right top corner
			rc = { corner[3].x - SHAPEGRIPPER_HALF, corner[3].y - SHAPEGRIPPER_HALF, corner[3].x + SHAPEGRIPPER_HALF, corner[3].y + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

			// top midway
			rc = { mid[1].x - SHAPEGRIPPER_HALF, mid[1].y - SHAPEGRIPPER_HALF, mid[1].x + SHAPEGRIPPER_HALF, mid[1].y + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
			// bottom midway
			rc = { mid[3].x - SHAPEGRIPPER_HALF, mid[3].y - SHAPEGRIPPER_HALF, mid[3].x + SHAPEGRIPPER_HALF, mid[3].y + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
		}
		// right midway
		rc = { mid[2].x - SHAPEGRIPPER_HALF, mid[2].y - SHAPEGRIPPER_HALF, mid[2].x + SHAPEGRIPPER_HALF, mid[2].y + SHAPEGRIPPER_HALF };
		Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
		// left midway
		hbr = CreateSolidBrush(COLORREF_LTBLUE);
		hbr0 = (HBRUSH)SelectObject(hdc, hbr);
		rc = { mid[0].x - SHAPEGRIPPER_HALF1, mid[0].y - SHAPEGRIPPER_HALF1, mid[0].x + SHAPEGRIPPER_HALF1, mid[0].y + SHAPEGRIPPER_HALF1 };
		Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);
		SelectObject(hdc, hbr0);
		DeleteObject(hbr);
		// rotation pivot point
		POINT rotpt = _getGripCoords(GRIPID_CURVEDARROW, shapeRect);
		rc = { rotpt.x - SHAPEGRIPPER_HALF1, rotpt.y - SHAPEGRIPPER_HALF1,rotpt.x + SHAPEGRIPPER_HALF1,rotpt.y + SHAPEGRIPPER_HALF1 };
		Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);

		SelectObject(hdc, hpen0);
	}*/

	CopyMemory(_si.GeomMidPts, mid, sizeof(mid));
	CopyMemory(_si.GeomCornerPts, corner, sizeof(corner));

	DBGPRINTF_VERBOSE((L"_renderhapeRotated.1: rotate=%d; corner={%d:%d,%d:%d,%d:%d,%d:%d}\n", _si.GeomRotation, corner[0].x, corner[0].y, corner[1].x, corner[1].y, corner[2].x, corner[2].y, corner[3].x, corner[3].y));

	/*
	// 4 corners outside the grippers. swap x, y corner adjustments
	SIZE gripSize = { gripExtension.x,gripExtension.y };
	corner[0].x -= gripSize.cx; // left top corner
	corner[0].y -= gripSize.cy;
	corner[1].x -= gripSize.cx; // left bottom corner
	corner[1].y += gripSize.cy;
	corner[2].x += gripSize.cx; // right bottom corner
	corner[2].y += gripSize.cy;
	corner[3].x += gripSize.cx; // right top corner
	corner[3].y -= gripSize.cy;
	*/

	POINT corner2[4];
	corner2[0] = flt.transformPt({ -SHAPEGRIPPER_HALF2, -_si.GeomSize.cy / 2 - SHAPEGRIPPER_HALF2 }); // left top corner
	corner2[1] = flt.transformPt({ -SHAPEGRIPPER_HALF2, _si.GeomSize.cy / 2 + SHAPEGRIPPER_HALF2 }); // left bottom corner
	corner2[2] = flt.transformPt({ _si.GeomSize.cx + SHAPEGRIPPER_HALF2, _si.GeomSize.cy / 2 + SHAPEGRIPPER_HALF2 });  // right bottom corner
	corner2[3] = flt.transformPt({ _si.GeomSize.cx + SHAPEGRIPPER_HALF2, -_si.GeomSize.cy / 2 - SHAPEGRIPPER_HALF2 }); // right top corner
	CopyMemory(_si.GeomOuterPts, corner2, sizeof(corner2));

	DBGPRINTF_VERBOSE((L"_renderhapeRotated.2: gripSize={%d:%d}; corner={%d:%d,%d:%d,%d:%d,%d:%d}\n", SHAPEGRIPPER_HALF2, SHAPEGRIPPER_HALF2, corner2[0].x, corner2[0].y, corner2[1].x, corner2[1].y, corner2[2].x, corner2[2].y, corner2[3].x, corner2[3].y));

	return hrgn;
}

void GShapeOperation::_drawGripHandles(HDC hdc, POINT mid[4], POINT corner[4], POINT rotpt)
{
	RECT rc;
	HPEN hpen = (HPEN)GetStockObject(BLACK_PEN);
	HPEN hpen0 = (HPEN)SelectObject(hdc, hpen);
	HBRUSH hbr = (HBRUSH)GetStockObject(WHITE_BRUSH);
	HBRUSH hbr0 = (HBRUSH)SelectObject(hdc, hbr);

	if (_si.Shape != DUMPSHAPEINFO_SHAPE_LINE)
	{
		// left top corner
		rc = { corner[0].x - SHAPEGRIPPER_HALF, corner[0].y - SHAPEGRIPPER_HALF, corner[0].x + SHAPEGRIPPER_HALF, corner[0].y + SHAPEGRIPPER_HALF };
		Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
		// left bottom corner
		rc = { corner[1].x - SHAPEGRIPPER_HALF, corner[1].y - SHAPEGRIPPER_HALF, corner[1].x + SHAPEGRIPPER_HALF, corner[1].y + SHAPEGRIPPER_HALF };
		Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
		// right bottom corner
		rc = { corner[2].x - SHAPEGRIPPER_HALF, corner[2].y - SHAPEGRIPPER_HALF, corner[2].x + SHAPEGRIPPER_HALF, corner[2].y + SHAPEGRIPPER_HALF };
		Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
		// right top corner
		rc = { corner[3].x - SHAPEGRIPPER_HALF, corner[3].y - SHAPEGRIPPER_HALF, corner[3].x + SHAPEGRIPPER_HALF, corner[3].y + SHAPEGRIPPER_HALF };
		Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

		// top midway
		rc = { mid[1].x - SHAPEGRIPPER_HALF, mid[1].y - SHAPEGRIPPER_HALF, mid[1].x + SHAPEGRIPPER_HALF, mid[1].y + SHAPEGRIPPER_HALF };
		Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
		// bottom midway
		rc = { mid[3].x - SHAPEGRIPPER_HALF, mid[3].y - SHAPEGRIPPER_HALF, mid[3].x + SHAPEGRIPPER_HALF, mid[3].y + SHAPEGRIPPER_HALF };
		Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
	}
	// right midway
	rc = { mid[2].x - SHAPEGRIPPER_HALF, mid[2].y - SHAPEGRIPPER_HALF, mid[2].x + SHAPEGRIPPER_HALF, mid[2].y + SHAPEGRIPPER_HALF };
	Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
	// left midway
	hbr = CreateSolidBrush(COLORREF_LTBLUE);
	hbr0 = (HBRUSH)SelectObject(hdc, hbr);
	rc = { mid[0].x - SHAPEGRIPPER_HALF1, mid[0].y - SHAPEGRIPPER_HALF1, mid[0].x + SHAPEGRIPPER_HALF1, mid[0].y + SHAPEGRIPPER_HALF1 };
	Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);
	SelectObject(hdc, hbr0);
	DeleteObject(hbr);
	// rotation pivot point
	rc = { rotpt.x - SHAPEGRIPPER_HALF1, rotpt.y - SHAPEGRIPPER_HALF1,rotpt.x + SHAPEGRIPPER_HALF1,rotpt.y + SHAPEGRIPPER_HALF1 };
	Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);

	SelectObject(hdc, hpen0);
}

bool GShapeOperation::_drawOriginConnector(HDC hdc, POINT originPt, POINT shapePt)
{
	_si.Flags &= ~DUMPSHAPEINFO_FLAG_CONNECTOR;
	int cols = (shapePt.x - originPt.x) / _vc->_vi.ColWidth;
	int rows = (shapePt.y - originPt.y) / _vc->_vi.RowHeight;
	int c2 = cols * cols;
	int r2 = rows * rows;
	if (c2 <= 1 && r2 <= 1)
		return false;

	RECT rc = { originPt.x- SHAPEGRIPPER_HALF, originPt.y- SHAPEGRIPPER_HALF, originPt.x+ SHAPEGRIPPER_HALF, originPt.y+ SHAPEGRIPPER_HALF };

	HPEN hpen = CreatePen(PS_DASH, 2, COLORREF_WHITE);
	HPEN hpen0 = (HPEN)SelectObject(hdc, hpen);
	MoveToEx(hdc, shapePt.x, shapePt.y, NULL);
	LineTo(hdc, originPt.x, originPt.y);
	Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
	SelectObject(hdc, hpen0);
	DeleteObject(hpen);

	_si.GeomConnectorPts[0] = originPt;
	_si.GeomConnectorPts[1] = shapePt;
	_si.Flags |= DUMPSHAPEINFO_FLAG_CONNECTOR;
	return true;
}

HRGN GShapeOperation::renderShape(HDC hdc, RECT shapeRect, POINT originPt, UINT flags, HBITMAP hbmBkgd)
{
	HBRUSH hbr = NULL, hbr0 = NULL;
	if (flags & RSFLAG_OUTLINE_ONLY)
	{
		// for outline rendition, use a transparent brush for the interior.
		hbr = (HBRUSH)GetStockObject(NULL_BRUSH);
		hbr0 = (HBRUSH)SelectObject(hdc, hbr);
		renderShape(hdc, shapeRect, originPt, flags & ~RSFLAG_OUTLINE_ONLY);
		SelectObject(hdc, hbr0);
		return NULL;
	}

	if (_si.GeomRotation != 0)
	{
		if (_si.Shape == DUMPSHAPEINFO_SHAPE_LINE)
			return _renderLineShapeRotated(hdc, shapeRect, originPt, flags);
		if (_vc->_highdefVP)
			return _renderShapeRotatedHD(hdc, shapeRect, originPt, flags);
		return _renderShapeRotated(hdc, shapeRect, originPt, flags, hbmBkgd);
	}

	DBGPRINTF((L"renderShape: shapeRect={%d,%d,%d,%d}, originPt={%d,%d}; flags=%x\n", shapeRect.left, shapeRect.top, shapeRect.right, shapeRect.bottom, originPt.x, originPt.y, flags));

	RECT rc;
	int cx = shapeRect.right - shapeRect.left;
	int cy = shapeRect.bottom - shapeRect.top;
	int dx = cx / 2;
	int dy = cy / 2;

	HRGN hrgn = NULL;

	HPEN hpen = CreatePen(PS_SOLID, _vc->screenToLogical(_si.StrokeThickness), _si.OutlineColor);
	HPEN hpen0 = (HPEN)SelectObject(hdc, hpen);

	if (_si.Shape == DUMPSHAPEINFO_SHAPE_RECTANGLE)
	{
		if (!(_si.Flags & DUMPSHAPEINFO_FLAG_FILLED))
		{
			Rectangle(hdc, shapeRect.left, shapeRect.top, shapeRect.right, shapeRect.bottom);
			goto _checkGrippers;
		}
		hbr = CreateSolidBrush(_si.InteriorColor);
		hbr0 = (HBRUSH)SelectObject(hdc, hbr);
		if (flags & RSFLAG_REPAINT_BKGD)
		{
			HDC hdc1 = CreateCompatibleDC(hdc);
			if (hbmBkgd)
			{
				//SaveBmp(hbmBkgd, BMP_SAVE_PATH2);
				HBITMAP hbm10 = (HBITMAP)SelectObject(hdc1, hbmBkgd);
				BitBlt(hdc, shapeRect.left, shapeRect.top, cx, cy, hdc1, shapeRect.left, shapeRect.top, SRCCOPY);
				SelectObject(hdc1, hbm10);
			}
			else
			{
				HBITMAP hbm1 = CreateCompatibleBitmap(hdc, shapeRect.right, shapeRect.bottom);
				HBITMAP hbm10 = (HBITMAP)SelectObject(hdc1, hbm1);
				_vc->repaint(BINHEXVIEW_REPAINT_RESET_FONT | BINHEXVIEW_REPAINT_DUMP_ONLY, hdc1);
				BitBlt(hdc, shapeRect.left, shapeRect.top, cx, cy, hdc1, shapeRect.left, shapeRect.top, SRCCOPY);
				SelectObject(hdc1, hbm10);
				DeleteObject(hbm1);
			}
			DeleteDC(hdc1);

			HDC hdc2 = CreateCompatibleDC(hdc);
			HBITMAP hbm2 = CreateCompatibleBitmap(hdc, cx, cy);
			HBITMAP hbm20 = (HBITMAP)SelectObject(hdc2, hbm2);
			HBRUSH hbr20 = (HBRUSH)SelectObject(hdc2, hbr);
			HPEN hpen20 = (HPEN)SelectObject(hdc2, hpen);

			Rectangle(hdc2, 0, 0, cx, cy);

			BLENDFUNCTION bf = { AC_SRC_OVER, 0, BHV_NORMAL_SHAPE_OPACITY, 0 };
			AlphaBlend(hdc, shapeRect.left, shapeRect.top, cx, cy, hdc2, 0, 0, cx, cy, bf);

			SelectObject(hdc2, hpen20);
			SelectObject(hdc2, hbr20);
			SelectObject(hdc2, hbm20);
			DeleteDC(hdc2);
			DeleteObject(hbm2);
		}
		else
		{
			Rectangle(hdc, shapeRect.left, shapeRect.top, shapeRect.right, shapeRect.bottom);
		}
		if (_vc->_highdefVP)
			rc = _vc->toDeviceCoords(&shapeRect, (flags & RSFLAG_LT_AT_ORIGIN));
		else
		if (flags & RSFLAG_LT_AT_ORIGIN)
			rc = { 0,0,shapeRect.right - shapeRect.left, shapeRect.bottom - shapeRect.top };
		else
			rc = shapeRect;
		hrgn = CreateRectRgn(rc.left, rc.top, rc.right, rc.bottom);
	}
	else if (_si.Shape == DUMPSHAPEINFO_SHAPE_ELLIPSE)
	{
		if (!(_si.Flags & DUMPSHAPEINFO_FLAG_FILLED))
		{
			Ellipse(hdc, shapeRect.left, shapeRect.top, shapeRect.right, shapeRect.bottom);
			goto _checkGrippers;
		}
		hbr = CreateSolidBrush(_si.InteriorColor);
		hbr0 = (HBRUSH)SelectObject(hdc, hbr);
#ifdef RENDERCIRCLE_USES_GDI_ELLIPSE
		BeginPath(hdc);
		if (_vc->_highdefVP)
		{
			// a GDI region is expressed in device units. shapeRect is in logical units. so, translate shapeRect to x- and y-extents in device units.
			rc = _vc->toDeviceCoords(&shapeRect, (flags & RSFLAG_LT_AT_ORIGIN), true);
			Ellipse(hdc, rc.left - 1, rc.top - 1, rc.right + 1, rc.bottom + 1);
		}
		else
			Ellipse(hdc, shapeRect.left - 1, shapeRect.top - 1, shapeRect.right + 1, shapeRect.bottom + 1);
		EndPath(hdc);
		hrgn = PathToRegion(hdc);

		if (flags & RSFLAG_REPAINT_BKGD)
		{
			HDC hdc1 = CreateCompatibleDC(hdc);
			if (hbmBkgd)
			{
				HBITMAP hbm10 = (HBITMAP)SelectObject(hdc1, hbmBkgd);
				BitBlt(hdc, shapeRect.left, shapeRect.top, cx, cy, hdc1, shapeRect.left, shapeRect.top, SRCCOPY);
				SelectObject(hdc1, hbm10);
			}
			else
			{
				HBITMAP hbm1 = CreateCompatibleBitmap(hdc, shapeRect.right, shapeRect.bottom);
				HBITMAP hbm10 = (HBITMAP)SelectObject(hdc1, hbm1);
				_vc->repaint(BINHEXVIEW_REPAINT_RESET_FONT | BINHEXVIEW_REPAINT_DUMP_ONLY, hdc1);
				BitBlt(hdc, shapeRect.left, shapeRect.top, cx, cy, hdc1, shapeRect.left, shapeRect.top, SRCCOPY);
				SelectObject(hdc1, hbm10);
				DeleteObject(hbm1);
			}
			DeleteDC(hdc1);

			HDC hdc2 = CreateCompatibleDC(hdc);
			HBITMAP hbm2 = CreateCompatibleBitmap(hdc, cx, cy);
			HBITMAP hbm20 = (HBITMAP)SelectObject(hdc2, hbm2);
			HBRUSH hbr20 = (HBRUSH)SelectObject(hdc2, hbr);
			HPEN hpen20 = (HPEN)SelectObject(hdc2, hpen);

			RECT rc2 = { 0,0,cx,cy };
			FillRect(hdc2, &rc2, GetStockBrush(WHITE_BRUSH));
			Ellipse(hdc2, 0, 0, cx, cy);
#ifdef GSHAPEOP_CLIPS_REGION
			int clipres = SelectClipRgn(hdc, hrgn);
#endif//#ifdef GSHAPEOP_CLIPS_REGION

			BLENDFUNCTION bf = { AC_SRC_OVER, 0, BHV_NORMAL_SHAPE_OPACITY, 0 };
			AlphaBlend(hdc, shapeRect.left, shapeRect.top, cx, cy, hdc2, 0, 0, cx, cy, bf);
#ifdef GSHAPEOP_CLIPS_REGION
			SelectClipRgn(hdc, _vc->_hrgnClip);
#endif//#ifdef GSHAPEOP_CLIPS_REGION

			SelectObject(hdc2, hpen20);
			SelectObject(hdc2, hbr20);
			SelectObject(hdc2, hbm20);
			DeleteDC(hdc2);
			DeleteObject(hbm2);
		}
		else
		{
			Ellipse(hdc, shapeRect.left, shapeRect.top, shapeRect.right, shapeRect.bottom);
		}
		/* it's possible to convert the path to a region. but, it's cumbersome, and unfortunately imprecise. so, don't use this.
		simpleList<POINT> pts(GetPath(hdc, NULL, NULL, 0));
		simpleList<BYTE> types(pts.size());
		GetPath(hdc, pts.data(), types.data(), (int)pts.size());
		hrgn = CreatePolygonRgn(pts.data(), (int)pts.size(), ALTERNATE);
		OffsetRgn(hrgn, FRAMEMARGIN_CX, FRAMEMARGIN_CY);
		*/
#else//#ifndef RENDERCIRCLE_USES_GDI_ELLIPSE
		simpleList<POINT> pts;
		_generateCirclePts(_si, shapeRect, pts);
		Polygon(hdc, pts.data(), (int)pts.size());
		for (size_t i = 0; i < pts.size(); i++)
		{
			pts[i].x += FRAMEMARGIN_CX;
			pts[i].y += FRAMEMARGIN_CY;
		}
		hrgn = CreatePolygonRgn(pts.data(), (int)pts.size(), ALTERNATE);
#endif//#ifndef RENDERCIRCLE_USES_GDI_ELLIPSE
	}
	else //if (_si.Shape == DUMPSHAPEINFO_SHAPE_LINE)
	{
		POINT pts[4];
		pts[0] = { shapeRect.left, shapeRect.top };
		pts[1] = { shapeRect.left, shapeRect.bottom };
		pts[2] = { shapeRect.right, shapeRect.bottom };
		pts[3] = { shapeRect.right, shapeRect.top };

		if (_si.GeomOptions == DUMPSHAPEINFO_OPTION_ARROW
			|| _si.GeomOptions == DUMPSHAPEINFO_OPTION_ARROW2)
		{
			int bladeLength = 3 * dy;
			MoveToEx(hdc, pts[0].x, pts[0].y + dy, NULL);
			LineTo(hdc, pts[2].x, pts[2].y - dy);

			HBRUSH hbr2 = CreateSolidBrush(_si.OutlineColor);
			HPEN hpen2 = CreatePen(PS_SOLID, 0, _si.OutlineColor);
			SelectObject(hdc, hbr2);
			SelectObject(hdc, hpen2);
			POINT arrowhead[3];
			arrowhead[0].x = pts[2].x;
			arrowhead[0].y = pts[2].y - dy;
			arrowhead[1].x = pts[2].x - bladeLength;
			arrowhead[1].y = arrowhead[0].y - dy;
			arrowhead[2].x = arrowhead[1].x;
			arrowhead[2].y = arrowhead[0].y + dy;
			Polygon(hdc, arrowhead, 3);
			if (_si.GeomOptions == DUMPSHAPEINFO_OPTION_ARROW2)
			{
				arrowhead[0].x = pts[0].x;
				arrowhead[0].y = pts[0].y + dy;
				arrowhead[1].x = pts[0].x + bladeLength;
				arrowhead[1].y = arrowhead[0].y - dy;
				arrowhead[2].x = arrowhead[1].x;
				arrowhead[2].y = arrowhead[0].y + dy;
				Polygon(hdc, arrowhead, 3);
			}
			DeleteObject(hpen2);
			DeleteObject(hbr2);
		}
		else if (_si.GeomOptions == DUMPSHAPEINFO_OPTION_WAVY)
		{
			// draw a wavy line
			// sinusoidal wave: f = r * sin(phi)
			double wavelen = _vc->screenToLogical(16.0); // one cycle per 16 pixels
			double freq = 1.0 / wavelen;
			double a0 = (double)dy;
			double phi;
			simpleList<POINT> fpts;
			for (int t = pts[0].x; t < pts[2].x; t++)
			{
				phi = 2.0*M_PI*freq*(double)(t - pts[0].x);
				double A = a0 * sin(phi);
				POINT fpt = { t, pts[0].y - lrint(A) + dy };
				fpts.add(fpt);
			}
			MoveToEx(hdc, fpts[0].x, fpts[0].y, NULL);
			for (size_t i = 1; i < fpts.size(); i++)
				LineTo(hdc, fpts[i].x, fpts[i].y);
		}
		else // a straight line
		{
			MoveToEx(hdc, pts[0].x, pts[0].y + dy, NULL);
			LineTo(hdc, pts[2].x, pts[2].y - dy);
		}

		if (!(_si.Flags & DUMPSHAPEINFO_FLAG_FILLED))
			goto _checkGrippers;

		pts[0].x--;
		//pts[0].y++;
		pts[1].x++;
		pts[1].y++;
		pts[2].x++;
		//pts[2].y--;
		pts[3].x--;
		pts[3].y++;
		hrgn = CreatePolygonRgn(pts, 4, ALTERNATE);
	}

_checkGrippers:
	if (hbr0)
		SelectObject(hdc, hbr0);
	if (hbr)
		DeleteObject(hbr);
	if (hpen0)
		SelectObject(hdc, hpen0);
	if (hpen)
		DeleteObject(hpen);

	if (flags & RSFLAG_DRAW_ORIGIN)
		_drawOriginConnector(hdc, originPt, { shapeRect.left , shapeRect.bottom - dy });

	if (flags & RSFLAG_DRAW_GRIPPERS)
	{
		POINT mid[4] =
		{
			{shapeRect.left, shapeRect.top + dy},
			{shapeRect.left + dx, shapeRect.top},
			{shapeRect.right, shapeRect.bottom - dy},
			{shapeRect.right - dx, shapeRect.bottom},
		};
		POINT corner[4] = 
		{
			{shapeRect.left, shapeRect.top},
			{shapeRect.left, shapeRect.bottom},
			{shapeRect.right, shapeRect.bottom},
			{shapeRect.right, shapeRect.top},
		};

		_drawGripHandles(hdc, mid, corner, _getGripCoords(GRIPID_CURVEDARROW, shapeRect));

		/*hpen = (HPEN)GetStockObject(BLACK_PEN);
		hpen0 = (HPEN)SelectObject(hdc, hpen);
		hbr = (HBRUSH)GetStockObject(WHITE_BRUSH);
		hbr0 = (HBRUSH)SelectObject(hdc, hbr);

		if (_si.Shape != DUMPSHAPEINFO_SHAPE_LINE)
		{
			// left top corner
			rc = { shapeRect.left - SHAPEGRIPPER_HALF, shapeRect.top - SHAPEGRIPPER_HALF, shapeRect.left + SHAPEGRIPPER_HALF,shapeRect.top + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
			// left bottom corner
			rc = { shapeRect.left - SHAPEGRIPPER_HALF, shapeRect.bottom - SHAPEGRIPPER_HALF, shapeRect.left + SHAPEGRIPPER_HALF, shapeRect.bottom + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
			// right bottom corner
			rc = { shapeRect.right - SHAPEGRIPPER_HALF, shapeRect.bottom - SHAPEGRIPPER_HALF, shapeRect.right + SHAPEGRIPPER_HALF, shapeRect.bottom + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
			// right top corner
			rc = { shapeRect.right - SHAPEGRIPPER_HALF, shapeRect.top - SHAPEGRIPPER_HALF, shapeRect.right + SHAPEGRIPPER_HALF, shapeRect.top + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
			// top midway
			rc = { shapeRect.left + dx - SHAPEGRIPPER_HALF, shapeRect.top - SHAPEGRIPPER_HALF, shapeRect.left + dx + SHAPEGRIPPER_HALF, shapeRect.top + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
			// bottom midway
			rc = { shapeRect.right - dx - SHAPEGRIPPER_HALF, shapeRect.bottom - SHAPEGRIPPER_HALF, shapeRect.right - dx + SHAPEGRIPPER_HALF, shapeRect.bottom + SHAPEGRIPPER_HALF };
			Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
		}
		// right midway
		rc = { shapeRect.right - SHAPEGRIPPER_HALF, shapeRect.bottom - dy - SHAPEGRIPPER_HALF, shapeRect.right + SHAPEGRIPPER_HALF, shapeRect.bottom - dy + SHAPEGRIPPER_HALF };
		Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
		// left midway
		hbr = CreateSolidBrush(COLORREF_LTBLUE);
		hbr0 = (HBRUSH)SelectObject(hdc, hbr);
		rc = { shapeRect.left - SHAPEGRIPPER_HALF1, shapeRect.bottom - dy - SHAPEGRIPPER_HALF1, shapeRect.left + SHAPEGRIPPER_HALF1, shapeRect.bottom - dy + SHAPEGRIPPER_HALF1 };
		Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);
		SelectObject(hdc, hbr0);
		DeleteObject(hbr);
		// rotation pivot
		POINT rotpt = _getGripCoords(GRIPID_CURVEDARROW, shapeRect);
		rc = { rotpt.x - SHAPEGRIPPER_HALF1, rotpt.y - SHAPEGRIPPER_HALF1,rotpt.x + SHAPEGRIPPER_HALF1,rotpt.y + SHAPEGRIPPER_HALF1 };
		Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);

		SelectObject(hdc, hpen0);*/

		rc = shapeRect;
		rc.left -= SHAPEGRIPPER_HALF2;
		rc.top -= SHAPEGRIPPER_HALF2;
		rc.right += SHAPEGRIPPER_HALF2;
		rc.bottom += SHAPEGRIPPER_HALF2;
		_si.GeomOuterPts[0].x = rc.left;
		_si.GeomOuterPts[0].y = rc.top;
		_si.GeomOuterPts[1].x = rc.left;
		_si.GeomOuterPts[1].y = rc.bottom;
		_si.GeomOuterPts[2].x = rc.right;
		_si.GeomOuterPts[2].y = rc.bottom;
		_si.GeomOuterPts[3].x = rc.right;
		_si.GeomOuterPts[3].y = rc.top;
	}

	_si.GeomCornerPts[0].x = shapeRect.left;
	_si.GeomCornerPts[0].y = shapeRect.top;
	_si.GeomCornerPts[1].x = shapeRect.left;
	_si.GeomCornerPts[1].y = shapeRect.bottom;
	_si.GeomCornerPts[2].x = shapeRect.right;
	_si.GeomCornerPts[2].y = shapeRect.bottom;
	_si.GeomCornerPts[3].x = shapeRect.right;
	_si.GeomCornerPts[3].y = shapeRect.top;
	return hrgn;
}

void GShapeOperation::blendShape(HDC hdc, RECT shapeRect, POINT originPt, DWORD transparency)
{
	if (!(_si.Flags & DUMPSHAPEINFO_FLAG_FILLED))
	{
#ifdef SHAPES_FADE_ON_GOING_OUT_OF_FOCUS
		if (_si.GeomRotation == 0)
		{
			_blendShapeOutline(hdc, shapeRect, originPt);
			return;
		}
#endif//#ifdef SHAPES_FADE_ON_GOING_OUT_OF_FOCUS
		renderShape(hdc, shapeRect, originPt, RSFLAG_OUTLINE_ONLY);
		return;
	}
	
	HDC hdc2;
	HBITMAP hbmp, hbmp0;
	SIZE exts;
	RECT rc;
	HRGN hrgn;
	int clipres = ERROR;
	BLENDFUNCTION bf = { AC_SRC_OVER, 0, LOBYTE(transparency), 0 };

	if (_si.GeomRotation != 0)
	{
		hdc2 = CreateCompatibleDC(hdc);
		hbmp = CreateCompatibleBitmap(hdc, _vc->_vi.FrameSize.cx, _vc->_vi.FrameSize.cy);
		hbmp0 = (HBITMAP)SelectObject(hdc2, hbmp);
		exts = _vc->_vi.FrameSize;
		rc = { 0,0, exts.cx, exts.cy };
		FillRect(hdc2, &rc, GetStockBrush(WHITE_BRUSH));
		hrgn = renderShape(hdc2, shapeRect, originPt);
#ifdef GSHAPEOP_CLIPS_REGION
		if (hrgn)
		{
			GetRgnBox(hrgn, &rc);
			exts.cx = rc.right - rc.left;
			exts.cy = rc.bottom - rc.top;
			OffsetRgn(hrgn, _vc->_vi.FrameMargin.x, _vc->_vi.FrameMargin.y);
			clipres = SelectClipRgn(hdc, hrgn);
			ASSERT(clipres != ERROR); // SIMPLEREGION (2) or COMPLEXREGION (3)
		}
#endif//#ifdef GSHAPEOP_CLIPS_REGION
		AlphaBlend(hdc, rc.left, rc.top, exts.cx, exts.cy, hdc2, rc.left, rc.top, exts.cx, exts.cy, bf);
#ifdef GSHAPEOP_CLIPS_REGION
		SelectClipRgn(hdc, _vc->_hrgnClip);
#endif//#ifdef GSHAPEOP_CLIPS_REGION
		SelectObject(hdc2, hbmp0);
		DeleteObject(hbmp);
		DeleteDC(hdc2);
		DeleteObject(hrgn);
		return;
	}
	exts.cx = shapeRect.right - shapeRect.left;
	exts.cy = shapeRect.bottom - shapeRect.top;
	hdc2 = CreateCompatibleDC(hdc);
	SetWindowOrgEx(hdc2, shapeRect.left, shapeRect.top, NULL);
	SIZE wndexts2 = { 0 }, vpexts2 = { 0 };
	if (_vc->_highdefVP && _vc->_highdefVP->ScalingFactor != 0)
	{
#ifdef _DEBUG
		GetWindowExtEx(hdc2, &wndexts2);
		GetViewportExtEx(hdc2, &vpexts2);
#endif//#ifdef _DEBUG
		wndexts2.cx = lrint((double)_vc->_vi.LogicalPPI.cx * _vc->_highdefVP->ScalingFactor);
		wndexts2.cy = lrint((double)_vc->_vi.LogicalPPI.cy * _vc->_highdefVP->ScalingFactor);
		SetWindowExtEx(hdc2, wndexts2.cx, wndexts2.cy, NULL);
		SetViewportExtEx(hdc2, _vc->_vi.DevicePPI.cx, _vc->_vi.DevicePPI.cy, NULL);
	}
	hbmp = CreateCompatibleBitmap(hdc, exts.cx, exts.cy);
	hbmp0 = (HBITMAP)SelectObject(hdc2, hbmp);
	// render the shape in the memory dc.
	hrgn = renderShape(hdc2, shapeRect, originPt, RSFLAG_LT_AT_ORIGIN);
#ifdef _DEBUG
	RECT rcDbg;
	GetRgnBox(hrgn, &rcDbg);
#endif//#ifdef _DEBUG
	if (_vc->_highdefVP)
	{
		OffsetRgn(hrgn, _vc->_vi.FrameMargin.x, _vc->_vi.FrameMargin.y);
		SIZE LT = _vc->toDeviceCoords({ shapeRect.left, shapeRect.top });
		OffsetRgn(hrgn, LT.cx + _vc->_highdefVP->DeviceOrigin.x, LT.cy + _vc->_highdefVP->DeviceOrigin.y);
	}
	else
	{
		OffsetRgn(hrgn, _vc->_vi.FrameMargin.x, _vc->_vi.FrameMargin.y);
		OffsetRgn(hrgn, shapeRect.left, shapeRect.top);
	}
#ifdef GSHAPEOP_CLIPS_REGION
	clipres = SelectClipRgn(hdc, hrgn);
	ASSERT(clipres != ERROR); // SIMPLEREGION (2) or COMPLEXREGION (3)
#endif//#ifdef GSHAPEOP_CLIPS_REGION
	// now blend the shape into the hex dump.
	AlphaBlend(hdc, shapeRect.left, shapeRect.top, exts.cx, exts.cy, hdc2, shapeRect.left, shapeRect.top, exts.cx, exts.cy, bf);
#ifdef GSHAPEOP_CLIPS_REGION
	SelectClipRgn(hdc, _vc->_hrgnClip);
#endif//#ifdef GSHAPEOP_CLIPS_REGION
	SelectObject(hdc2, hbmp0);
	DeleteObject(hbmp);
	DeleteDC(hdc2);
	DeleteObject(hrgn);
}

/////////////////////////////////////////////////////////
// GShapeMove

bool GShapeMove::startOp(POINT startPt, DUMPSHAPGRIPINFO &gi)
{
	_si.Flags |= DUMPSHAPEINFO_FLAG_CHANGING;

	POINT originPt;
	RECT shapeRect;
	shapeInFrame(shapeRect, &originPt);

	// create a bitmap image of the shape in a memory DC.
	// hold onto it. will alphablend the image as user moves cursor to a new place.
	HDC hdc = _vc->setVC();
	HDC hdc1 = CreateCompatibleDC(hdc);
	HDC hdc2 = CreateCompatibleDC(hdc);
	HBITMAP hbmp1 = CreateCompatibleBitmap(hdc, _vc->_vi.FrameSize.cx, _vc->_vi.FrameSize.cy);
	HBITMAP hbmp01 = (HBITMAP)SelectObject(hdc1, hbmp1);
	RECT frameRect = { 0,0,_vc->_vi.FrameSize.cx, _vc->_vi.FrameSize.cy };
	FillRect(hdc1, &frameRect, GetStockBrush(WHITE_BRUSH));
	HRGN hrgn = renderShape(hdc1, shapeRect, originPt);
	if (!hrgn)
	{
		if (_si.GeomRotation != 0)
			hrgn = CreatePolygonRgn(_si.GeomCornerPts, 4, ALTERNATE);
		else
			hrgn = CreateRectRgn(shapeRect.left, shapeRect.top, shapeRect.right, shapeRect.bottom);
	}
	RECT imageRect;
	GetRgnBox(hrgn, &imageRect);
	InflateRect(&imageRect, 1, 1);
	SIZE imageSize = { imageRect.right - imageRect.left, imageRect.bottom - imageRect.top };
	HBITMAP hbmp2 = CreateCompatibleBitmap(hdc, imageSize.cx, imageSize.cy);
	HBITMAP hbmp02 = (HBITMAP)SelectObject(hdc2, hbmp2);
	BitBlt(hdc2, 0, 0, imageSize.cx, imageSize.cy, hdc1, imageRect.left, imageRect.top, SRCCOPY);
	/*#ifdef _DEBUG
		BitBlt(hdc, 0,0, imageSize.cx, imageSize.cy, hdc2, 0,0, SRCCOPY);
	#endif*/
	SelectObject(hdc1, hbmp01);
	SelectObject(hdc2, hbmp02);
	DeleteDC(hdc1);
	DeleteDC(hdc2);
	_vc->releaseVC();
	if (hrgn)
		DeleteObject(hrgn);

	DeleteObject(hbmp1);

	gi.TransitImage = hbmp2;
	gi.GripId = GRIPID_INVALID; // -1 differentiates moving from gripping.
	gi.ShapeIndex = _index;
	gi.Gripped = true;
	gi.GripPt = startPt;
	gi.ImageRect = imageRect;

	DBGPRINTF((L"GShapeMove.startOp: shapeIndex=%d; GripPt={%d:%d}; GeomSize={%d,%d}; Offset={%d,%d}\n", _index, startPt.x, startPt.y, _si.GeomSize.cx, _si.GeomSize.cy, _si.GeomOffset.x, _si.GeomOffset.y));
	return true;
}

bool GShapeMove::continueOp(POINT newPt, DUMPSHAPGRIPINFO &gi)
{
	POINT movement = { newPt.x - gi.GripPt.x,newPt.y - gi.GripPt.y };

	HRGN hrgnClip = NULL;
	HRGN hrgn;
	if (_si.GeomRotation != 0)
	{
		POINT pts1[4];
		CopyMemory(pts1, _si.GeomCornerPts, sizeof(pts1));
		int i;
		for (i = 0; i < 4; i++)
		{
			pts1[i].x += movement.x;
			pts1[i].y += movement.y;
		}
		hrgn = CreatePolygonRgn(pts1, 4, ALTERNATE);
		// clipping region is for screen DC whose viewport is offset by FRAMEMARGIN.
		// to compensate, we need to add the margins back to the rectangle.
		for (i = 0; i < 4; i++)
		{
			pts1[i].x += _vc->_vi.FrameMargin.x;
			pts1[i].y += _vc->_vi.FrameMargin.y;
		}
		hrgnClip = CreatePolygonRgn(pts1, 4, ALTERNATE);
	}
	else
	{
		RECT shapeRect;
		shapeInFrame(shapeRect);
		/*shapeRect.left += movement.x;
		shapeRect.top += movement.y;
		shapeRect.right += movement.x;
		shapeRect.bottom += movement.y;*/
		OffsetRect(&shapeRect, movement.x, movement.y);
		hrgn = CreateRectRgn(shapeRect.left, shapeRect.top, shapeRect.right, shapeRect.bottom);
		// clipping region is for screen DC whose viewport is offset by FRAMEMARGIN.
		// to compensate, we need to add the margins back to the rectangle.
		/*shapeRect.left += FRAMEMARGIN_CX;
		shapeRect.right += FRAMEMARGIN_CX;
		shapeRect.top += FRAMEMARGIN_CY;
		shapeRect.bottom += FRAMEMARGIN_CY;*/
		OffsetRect(&shapeRect, _vc->_vi.FrameMargin.x, _vc->_vi.FrameMargin.y);
		hrgnClip = CreateRectRgn(shapeRect.left, shapeRect.top, shapeRect.right, shapeRect.bottom);
	}

	HDC hdc = _vc->setVC();
	HDC hdc1;
	HBITMAP hbmp01;

	if (gi.PreReplaceImage)
	{
		hdc1 = CreateCompatibleDC(hdc);
		hbmp01 = (HBITMAP)SelectObject(hdc1, gi.PreReplaceImage);
		BitBlt(hdc, gi.PreReplaceRect.left, gi.PreReplaceRect.top, gi.PreReplaceRect.right - gi.PreReplaceRect.left, gi.PreReplaceRect.bottom - gi.PreReplaceRect.top, hdc1, 0, 0, SRCCOPY);
		SelectObject(hdc1, hbmp01);
		DeleteDC(hdc1);
		DeleteObject(gi.PreReplaceImage);
	}

	RECT rc1;
	GetRgnBox(hrgn, &rc1);
	InflateRect(&rc1, SHAPEGRIPPER_HALF2, SHAPEGRIPPER_HALF2);
	int cx = rc1.right - rc1.left;
	int cy = rc1.bottom - rc1.top;

	hdc1 = CreateCompatibleDC(hdc);
	BITMAPINFO bmi = { sizeof(BITMAPINFOHEADER), cx, cy, 1, 32, BI_RGB, cx*cy * sizeof(COLORREF) };
	UINT32* bits;
	HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, 0);
	hbmp01 = (HBITMAP)SelectObject(hdc1, hbmp);
	BitBlt(hdc1, 0, 0, cx, cy, hdc, rc1.left, rc1.top, SRCCOPY);
	/*#ifdef _DEBUG
		BitBlt(hdc, 0, 0, cx, cy, hdc1, 0, 0, SRCCOPY);
	#endif*/
	SelectObject(hdc1, hbmp01);
	DeleteDC(hdc1);
	gi.PreReplaceImage = hbmp;
	gi.PreReplaceRect = rc1;

	RECT rc2 = gi.ImageRect;
	/*rc2.left += movement.x;
	rc2.right += movement.x;
	rc2.top += movement.y;
	rc2.bottom += movement.y;*/
	OffsetRect(&rc2, movement.x, movement.y);
	cx = rc2.right - rc2.left;
	cy = rc2.bottom - rc2.top;

	HDC hdc2 = CreateCompatibleDC(hdc);
	HBITMAP hbmp02 = (HBITMAP)SelectObject(hdc2, gi.TransitImage);
#ifdef GSHAPEOP_CLIPS_REGION
	int clipres = SelectClipRgn(hdc, hrgnClip);
	ASSERT(clipres != ERROR); // SIMPLEREGION (2) or COMPLEXREGION (3)
#endif//#ifdef GSHAPEOP_CLIPS_REGION
	BLENDFUNCTION bf = { AC_SRC_OVER, 0, BHV_NORMAL_SHAPE_OPACITY, 0 };
	AlphaBlend(hdc, rc2.left, rc2.top, cx, cy, hdc2, 0, 0, cx, cy, bf);
#ifdef GSHAPEOP_CLIPS_REGION
	SelectClipRgn(hdc, _vc->_hrgnClip);
#endif//#ifdef GSHAPEOP_CLIPS_REGION
	SelectObject(hdc2, hbmp02);
	DeleteDC(hdc2);

	DeleteObject(hrgnClip);
	DeleteObject(hrgn);
	_vc->releaseVC();

	DBGPRINTF((L"shapeMoveContinue: newPt={%d,%d}; GripPt={%d,%d}; GeomOffset={%d,%d}==>{%d,%d}\n", newPt.x, newPt.y, gi.GripPt.x, gi.GripPt.y, _si.GeomOffset.x, _si.GeomOffset.y, _si.GeomOffset.x + newPt.x - gi.GripPt.x, _si.GeomOffset.y + newPt.y - gi.GripPt.y));
	return true;
}

bool GShapeMove::stopOp(POINT finishPt, DUMPSHAPGRIPINFO &gi)
{
	_si.GeomOffset.x += finishPt.x - gi.GripPt.x;
	_si.GeomOffset.y += finishPt.y - gi.GripPt.y;

	_si.Flags &= ~DUMPSHAPEINFO_FLAG_CHANGING;

	DBGPRINTF((L"shapeMoveFinish: finishPt={%d,%d}; GripPt={%d,%d}; GeomOffset={%d,%d}\n", finishPt.x, finishPt.y, gi.GripPt.x, gi.GripPt.y, _si.GeomOffset.x, _si.GeomOffset.y));

	if (gi.TransitImage)
		DeleteObject(gi.TransitImage);
	ZeroMemory(&gi, sizeof(DUMPSHAPGRIPINFO));
	return true;
}

///////////////////////////////////////////////////////////////////
// GShapeStretch

bool GShapeStretch::startOp(POINT startPt, DUMPSHAPGRIPINFO &gi)
{
	ASSERT(_gripId != GRIPID_INVALID);
	if (_gripId == GRIPID_SIZEWEST && _si.Shape == DUMPSHAPEINFO_SHAPE_LINE)
		return false;
	_si.Flags |= DUMPSHAPEINFO_FLAG_CHANGING;

	POINT originPt;
	RECT shapeRect;
	shapeInFrame(shapeRect, &originPt);

	// create a bitmap image of the shape in a memory DC.
	// hold onto it. will alphablend the image as user moves cursor to a new place.
	HDC hdc = _vc->setVC();
	HDC hdc1 = CreateCompatibleDC(hdc);
	HDC hdc2 = CreateCompatibleDC(hdc);
	HBITMAP hbmp1 = CreateCompatibleBitmap(hdc, _vc->_vi.FrameSize.cx, _vc->_vi.FrameSize.cy);
	HBITMAP hbmp01 = (HBITMAP)SelectObject(hdc1, hbmp1);
	RECT frameRect = { 0,0,_vc->_vi.FrameSize.cx, _vc->_vi.FrameSize.cy };
	FillRect(hdc1, &frameRect, GetStockBrush(WHITE_BRUSH));
	HRGN hrgn = renderShape(hdc1, shapeRect, originPt);
	if (!hrgn)
	{
		if (_si.GeomRotation != 0)
			hrgn = CreatePolygonRgn(_si.GeomCornerPts, 4, ALTERNATE);
		else
			hrgn = CreateRectRgn(shapeRect.left, shapeRect.top, shapeRect.right, shapeRect.bottom);
	}
	RECT imageRect;
	GetRgnBox(hrgn, &imageRect);
	InflateRect(&imageRect, SHAPEGRIPPER_HALF, SHAPEGRIPPER_HALF);
	SIZE imageSize = { imageRect.right - imageRect.left, imageRect.bottom - imageRect.top };
	HBITMAP hbmp2 = CreateCompatibleBitmap(hdc, imageSize.cx, imageSize.cy);
	HBITMAP hbmp02 = (HBITMAP)SelectObject(hdc2, hbmp2);
	BitBlt(hdc2, 0, 0, imageSize.cx, imageSize.cy, hdc1, imageRect.left, imageRect.top, SRCCOPY);
	/*#ifdef _DEBUG
		BitBlt(hdc, 0,0, imageSize.cx, imageSize.cy, hdc2, 0,0, SRCCOPY);
	#endif*/
	SelectObject(hdc1, hbmp01);
	SelectObject(hdc2, hbmp02);
	DeleteDC(hdc1);
	DeleteDC(hdc2);
	_vc->releaseVC();
	if (hrgn)
		DeleteObject(hrgn);

	DeleteObject(hbmp1);

	gi.TransitImage = hbmp2;
	gi.ShapeIndex = _index;
	gi.GripId = _gripId;
	gi.Gripped = true;
	gi.GripPt = startPt;
	gi.ImageRect = imageRect;

	DBGPRINTF((L"shapeStretchStart: shapeIndex=%d; GridId=%d, GripPt={%d:%d}; GeomRotation=%d, Size={%d,%d}, Offset={%d,%d}\n", _index, _gripId, startPt.x, startPt.y, _si.GeomRotation, _si.GeomSize.cx, _si.GeomSize.cy, _si.GeomOffset.x, _si.GeomOffset.y));
	return true;
}

HRGN GShapeStretch::_setupClippingRegion(POINT srcPath[4], POINT offset)
{
	POINT dest[4];
	// srcPath (actually GeomCornerPts): [0]=LT, [1]=LB, [2]=RB, [3]=RT
	CopyMemory(dest, srcPath, sizeof(dest));
	int i;
	// clipping region is for screen DC whose viewport is offset by FRAMEMARGIN.
	// to compensate, we need to add the margins back to the rectangle.
	for (i = 0; i < 4; i++)
	{
		dest[i].x += offset.x;
		dest[i].y += offset.y;
	}
	return CreatePolygonRgn(dest, 4, ALTERNATE);
}

bool GShapeStretch::continueOp(POINT stretchPt, DUMPSHAPGRIPINFO &gi)
{
	if (_gripId == GRIPID_INVALID)
		return false; // the shape is being relocated

	POINT originPt;
	RECT shapeRect;
	if (!stretchShape(stretchPt, originPt, shapeRect))
		return false;

	HDC hdc = _vc->setVC();
	HDC hdc1, hdc2;
	HBITMAP hbmp1, hbmp01;
	HBITMAP hbmp2, hbmp02;

	if (gi.PreReplaceImage)
	{
		hdc1 = CreateCompatibleDC(hdc);
		hbmp01 = (HBITMAP)SelectObject(hdc1, gi.PreReplaceImage);
		BitBlt(hdc, gi.PreReplaceRect.left, gi.PreReplaceRect.top, gi.PreReplaceRect.right - gi.PreReplaceRect.left, gi.PreReplaceRect.bottom - gi.PreReplaceRect.top, hdc1, 0, 0, SRCCOPY);
		SelectObject(hdc1, hbmp01);
		DeleteDC(hdc1);
		DeleteObject(gi.PreReplaceImage);
	}

	hdc1 = CreateCompatibleDC(hdc);
	hdc2 = CreateCompatibleDC(hdc);
	hbmp1 = CreateCompatibleBitmap(hdc, _vc->_vi.FrameSize.cx, _vc->_vi.FrameSize.cy);
	hbmp01 = (HBITMAP)SelectObject(hdc1, hbmp1);
	RECT frameRect = { 0,0,_vc->_vi.FrameSize.cx, _vc->_vi.FrameSize.cy };
	FillRect(hdc1, &frameRect, GetStockBrush(WHITE_BRUSH));
	HRGN hrgn = renderShape(hdc1,
		shapeRect, originPt,
		(_si.Flags & DUMPSHAPEINFO_FLAG_FILLED) ? RSFLAG_DRAW_GRIPPERS : RSFLAG_DRAW_GRIPPERS | RSFLAG_OUTLINE_ONLY);
	if (!hrgn)
		hrgn = CreatePolygonRgn(_si.GeomCornerPts, 4, ALTERNATE);
	HRGN hrgnClip = _setupClippingRegion(_si.GeomOuterPts, { _vc->_vi.FrameMargin.x, _vc->_vi.FrameMargin.y });

	RECT imageRect;
	GetRgnBox(hrgn, &imageRect);
	InflateRect(&imageRect, SHAPEGRIPPER_HALF2, SHAPEGRIPPER_HALF2);
	if (imageRect.left < 0)
		imageRect.left = 0;
	if (imageRect.top < 0)
		imageRect.top = 0;
	if (imageRect.right > _vc->_vi.FrameSize.cx)
		imageRect.right = _vc->_vi.FrameSize.cx;
	if (imageRect.bottom > _vc->_vi.FrameSize.cy)
		imageRect.bottom = _vc->_vi.FrameSize.cy;
	SIZE imageSize = { imageRect.right - imageRect.left, imageRect.bottom - imageRect.top };

	BITMAPINFO bmi = { sizeof(BITMAPINFOHEADER), imageSize.cx, imageSize.cy, 1, 32, BI_RGB, imageSize.cx*imageSize.cy * sizeof(COLORREF) };
	UINT32* bits;
	hbmp2 = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, 0);
	hbmp02 = (HBITMAP)SelectObject(hdc2, hbmp2);
	BOOL res = BitBlt(hdc2, 0, 0, imageSize.cx, imageSize.cy, hdc, imageRect.left, imageRect.top, SRCCOPY);
	//BitBlt(hdc, imageRect.left, imageRect.top, imageSize.cx, imageSize.cy, hdc1, imageRect.left, imageRect.top, SRCCOPY);
#ifdef GSHAPEOP_CLIPS_REGION
	int clipres = SelectClipRgn(hdc, hrgnClip);
	ASSERT(clipres != ERROR); // SIMPLEREGION (2) or COMPLEXREGION (3)
#endif//#ifdef GSHAPEOP_CLIPS_REGION
	BLENDFUNCTION bf = { AC_SRC_OVER, 0, BHV_CONTINUEDRAG_SHAPE_OPACITY, 0 };
	res = AlphaBlend(hdc, imageRect.left, imageRect.top, imageSize.cx, imageSize.cy, hdc1, imageRect.left, imageRect.top, imageSize.cx, imageSize.cy, bf);
#ifdef GSHAPEOP_CLIPS_REGION
	SelectClipRgn(hdc, _vc->_hrgnClip);
#endif//#ifdef GSHAPEOP_CLIPS_REGION

	SelectObject(hdc1, hbmp01);
	SelectObject(hdc2, hbmp02);
	DeleteDC(hdc1);
	DeleteDC(hdc2);
	DeleteObject(hbmp1);

	DBGPRINTF((L"shapeStretchContinue: AlphaBlend=%d; Hrgn=%X, Clip=%X; imageRect={%d,%d,%d,%d}, size={%d,%d}\n", res, hrgn, hrgnClip, imageRect.left, imageRect.top, imageRect.right, imageRect.bottom, imageSize.cx, imageSize.cy));

	gi.PreReplaceImage = hbmp2;
	gi.PreReplaceRect = imageRect;

	_vc->releaseVC();
	DeleteObject(hrgn);
	DeleteObject(hrgnClip);
	return true;
}

bool GShapeStretch::stretchShape(POINT stretchPt, POINT &originPt, RECT &shapeRect)
{
	if (!shapeInFrame(shapeRect, &originPt, true))
		return false;

	POINT gripPt = _getGripCoords(_gripId, shapeRect);

	int dx = stretchPt.x - gripPt.x;
	int dy = stretchPt.y - gripPt.y;

	switch (_gripId)
	{
	case GRIPID_SIZENW: // left top corner
		_si.GeomOffset.x += dx;
		_si.GeomSize.cx -= dx;
		_si.GeomOffset.y += dy;
		_si.GeomSize.cy -= dy;
		break;
	case GRIPID_SIZESW: // left bottom corner
		_si.GeomOffset.x += dx;
		_si.GeomSize.cx -= dx;
		_si.GeomSize.cy += dy;
		break;
	case GRIPID_SIZESE: // right bottom corner
		_si.GeomSize.cx += dx;
		_si.GeomSize.cy += dy;
		break;
	case GRIPID_SIZENE: // right top corner
		_si.GeomSize.cx += dx;
		_si.GeomOffset.y += dy;
		_si.GeomSize.cy -= dy;
		break;
	case GRIPID_SIZENORTH: // top midway
		_si.GeomOffset.y += dy;
		_si.GeomSize.cy -= dy;
		break;
	case GRIPID_SIZESOUTH: // bottom midway
		_si.GeomSize.cy += dy;
		break;
	case GRIPID_SIZEWEST: // left midway
		_si.GeomOffset.x += dx;
		_si.GeomSize.cx -= dx;
		break;
	case GRIPID_SIZEEAST: // right midway
		_si.GeomSize.cx += dx;
		break;
	case GRIPID_CURVEDARROW:
	{
		short rot0 = _si.GeomRotation;
		POINT piv;
		piv.x = _si.GeomOffset.x + originPt.x;
		piv.y = _si.GeomSize.cy / 2 + _si.GeomOffset.y + originPt.y;
		dx = stretchPt.x - piv.x;
		dy = stretchPt.y - piv.y;
		_si.GeomRotation = _calcRotationAngle({dx,dy});
	}
	break;

	default:
		ASSERT(FALSE);
		return false;
	}
	_si.Flags |= DUMPSHAPEINFO_FLAG_CHANGING;

	DBGPRINTF((L"stretchShape: grip=%d; pt={%d,%d}; ro=%d; cx=%d, cy=%d; ox=%d, oy=%d\n", _gripId, dx, dy, _si.GeomRotation, _si.GeomSize.cx, _si.GeomSize.cy, _si.GeomOffset.x, _si.GeomOffset.y));

	return true;
}

bool GShapeStretch::stopOp(POINT finishPt, DUMPSHAPGRIPINFO &gi)
{
	continueOp(finishPt, gi);
	_si.Flags &= ~DUMPSHAPEINFO_FLAG_CHANGING;

	DBGPRINTF((L"shapeStretchFinish: finishPt={%d,%d}; GripPt={%d,%d}; GeomSize={%d,%d}, Rotation=%d\n", finishPt.x, finishPt.y, gi.GripPt.x, gi.GripPt.y, _si.GeomSize.cx, _si.GeomSize.cy, _si.GeomRotation));

	if (gi.PreReplaceImage)
		DeleteObject(gi.PreReplaceImage);
	if (gi.TransitImage)
		DeleteObject(gi.TransitImage);
	ZeroMemory(&gi, sizeof(DUMPSHAPGRIPINFO));
	return true;
}


/////////////////////////////////////////////////////////////

void GShapeCollection::reinit(BinhexView *vc2)
{
	_vc = vc2;
}

void GShapeCollection::redrawAll(HDC hdc)
{
	for (int i = 0; i < (int)size(); i++)
	{
		GShapeOperation(*this, i, _vc).drawShape(hdc);
	}
}

bool GShapeCollection::startDrag(POINT pt)
{
	ASSERT(_sopDrag == NULL);
	if (_sopGrip)
	{
		_sopDrag = new GShapeStretch(_sopGrip);
	}
	else if (_sopUnderCursor)
	{
		ASSERT(_sopUnderCursor == _sopSel);
		_sopDrag = new GShapeMove(_sopSel);
	}
	else
		return false;
	if (!_sopDrag->startOp(pt, _shapeGrip))
	{
		delete _sopDrag;
		_sopDrag = NULL;
	}
	return true;
}

bool GShapeCollection::continueDrag(POINT pt)
{
	clear(GSHAPECOLL_CLEAR_SOPGRIP | GSHAPECOLL_CLEAR_SOPCURS);
	if (!_sopDrag)
		return false;
	_sopDrag->continueOp(pt, _shapeGrip);
	return true;
}

bool GShapeCollection::stopDrag(POINT pt)
{
	if (!_sopDrag)
		return false;
	_sopDrag->stopOp(pt, _shapeGrip);
	delete _sopDrag;
	_sopDrag = NULL;
	return true;
}

HCURSOR GShapeCollection::queryCursor()
{
	HCURSOR hc = NULL;
	if (_shapeGrip.Gripped)
	{
		if (_shapeGrip.ShapeIndex == (size_t)-1)
			hc = LoadCursor(NULL, IDC_HAND);
		else if (!(hc = _vc->_gcursor->getCursor(_vc->_hw, at(_shapeGrip.ShapeIndex), (_shapeGrip.GripId == GRIPID_SIZEEAST) ? GRIPID_PINCH : GRIPID_GRIP, _shapeGrip.GripId != GRIPID_CURVEDARROW)))
			hc = LoadCursor(NULL, IDC_HAND);
	}
	else if (_sopUnderCursor)
	{
		hc = LoadCursor(NULL, IDC_SIZEALL);
	}
	else if (_sopGrip)
	{
		hc = _vc->_gcursor->getCursor(_vc->_hw, *_sopGrip, _sopGrip->_gripId);
	}
	return hc;
}

bool GShapeCollection::highlightShape(size_t shapeId)
{
	if (shapeId < size())
	{
		if (_sopSel)
		{
			if (_sopSel->highlight(_vc->_hbmPaint))
			{
				_sopUnderCursor = _sopSel;
				return true;
			}
			delete _sopSel;
		}
		_sopSel = new GShapeSelect(*this, (int)shapeId, _vc);
		if (_sopSel->highlight(_vc->_hbmPaint))
		{
			_sopUnderCursor = _sopSel;
			return true;
		}
		delete _sopSel;
		_sopSel = NULL;
	}
	return false;
}

DUMPSHAPEINFO *GShapeCollection::queryHere()
{
	if (_shapeHere < 0)
		return NULL;
	return &at(_shapeHere);
}

int GShapeCollection::queryShapeAt(POINT pt, bool screenCoords)
{
	if (screenCoords)
		::ScreenToClient(_vc->_hw, &pt);

	_shapeHere = -1;
	for (int i = 0; i < (int)size(); i++)
	{
		GShapeOperation so(*this, i, _vc);
		if (so.containsPt(pt))
		{
			_shapeHere = i;
			DBGPRINTF((L"queryShapeAt: index=%d\n", i));
			break;
		}
	}
	return _shapeHere;
}

int GShapeCollection::queryShapeGripperAt(POINT pt)
{
	for (int i = 0; i < (int)size(); i++)
	{
		GShapeOperation so(*this, i, _vc);
		if (so.hasGripperAt(pt))
		{
			_sopGrip = new GShapeOperation(&so);
			return so._gripId;
		}
	}
	return -1;
}

int GShapeCollection::queryNextItem(LARGE_INTEGER contentPos, ULARGE_INTEGER fileSize, bool goBack)
{
	DUMPSHAPEINFO *item;
	LARGE_INTEGER offset0 = contentPos;
	if (0 <= _itemVisited && _itemVisited < (int)size())
	{
		item = &at(_itemVisited);
		offset0 = item->DataOffset;
	}
	LONGLONG delta0 = fileSize.QuadPart, delta;
	int i0 = -1;
	int i = 0;
	for (; i < (int)size(); i++)
	{
		if (i == _itemVisited)
			continue;
		item = &at(i);
		if (item->DataOffset.QuadPart == offset0.QuadPart)
			continue;
		if (goBack)
			delta = offset0.QuadPart - item->DataOffset.QuadPart;
		else
			delta = item->DataOffset.QuadPart - offset0.QuadPart;
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

DUMPSHAPEINFO *GShapeCollection::gotoItem(int itemIndex)
{
	_itemVisited = itemIndex;
	if (itemIndex != -1)
	{
		return &at(itemIndex);
	}
	return NULL;
}

bool GShapeCollection::removeHere()
{
	if (_shapeHere < 0)
		return false;
	if (remove(_shapeHere) < 0)
		return false;
	clear();
	_vc->invalidate();
	return true;
}

void GShapeCollection::clear(UINT flags)
{
	_shapeHere = -1;
	_shapeGrip.GripId = GRIPID_INVALID;
	if (_sopDrag && (flags & GSHAPECOLL_CLEAR_SOPDRAG))
	{
		delete _sopDrag;
		_sopDrag = NULL;
	}
	if (_sopGrip && (flags & GSHAPECOLL_CLEAR_SOPGRIP))
	{
		delete _sopGrip;
		_sopGrip = NULL;
	}
	if (_sopSel && (flags & GSHAPECOLL_CLEAR_SOPSEL))
	{
		delete _sopSel;
		_sopSel = NULL;
	}
	if (_sopUnderCursor && (flags & GSHAPECOLL_CLEAR_SOPCURS))
		_sopUnderCursor = NULL;
}

