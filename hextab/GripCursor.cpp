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
#include "GripCursor.h"


#define GC_OUTLINE_IN_COLOR

#define COLORREF_BLACK 0
#define COLORREF_WHITE 0x00ffffff
#define COLORREF_GRAY 0x00cccccc
#define COLORREF_BLUEGRAY RGB(209, 242, 235)
#define COLORREF_RED RGB(255,0,0)

void GripCursor::clear()
{
	if (_hc)
	{
		DestroyCursor(_hc);
		_hc = NULL;
	}
	if (_hbmColor2)
	{
		DeleteObject(_hbmColor2);
		_hbmColor2 = NULL;
	}
	if (_hbmMask2)
	{
		DeleteObject(_hbmMask2);
		_hbmMask2 = NULL;
	}
}

HCURSOR GripCursor::getCursor(HWND hwnd, const DUMPSHAPEINFO &shapeInfo, GRIPID gripId, bool ignoreRotation)
{
	short newRotation = ignoreRotation ? 0 : shapeInfo.GeomRotation;
	_resId = NULL;
	switch (gripId)
	{
	case GRIPID_SIZENW: // left top corner (north west)
	case GRIPID_SIZESE: // right bottom corner (south east)
		_resId = L"SIZENWSE";
		break;
	case GRIPID_SIZESW: // left bottom corner (south west)
	case GRIPID_SIZENE: // right top corner (north east)
		_resId = L"SIZENESW";
		break;
	case GRIPID_SIZENORTH: // top midway (north)
	case GRIPID_SIZESOUTH: // bottom midway (south)
		_resId = L"SIZENS";
		break;
	case GRIPID_SIZEWEST: // left midway (west)
	case GRIPID_SIZEEAST: // right midway (east)
		_resId = L"SIZEWE";
		break;
	case GRIPID_CURVEDARROW:
		_resId = L"CURVEDARROW";
		break;
	case GRIPID_PINCH:
		_resId = L"PINCH";
		break;
	case GRIPID_GRIP:
		_resId = L"GRIP";
		break;
	default:
		return NULL;
	}

	if (_hc &&
		(newRotation != _rotationAngle ||
			gripId != _gripId))
	{
		clear();
	}
	if (!_hc)
	{
		_rotationAngle = newRotation;
		_gripId = gripId;

		HDC hdc = GetDC(hwnd);
		bool ret = createMaskBitmaps(hdc);
		ReleaseDC(hwnd, hdc);
		if (!ret)
			return NULL;
	}

	return _hc;
}

#ifdef GRIPCURSOR_USES_INSPECTOR
/* returns a rectangle where the next bitmap image may be shown.
this method assumes that the bitmap is 32 pixels wide and 32 pixels tall.
*/
RECT GripCursorInspector::inspectBitmaps(HDC hdc, HBITMAP hbmColor, HBITMAP hbmMask, int verticalOffset, POINT hotspot, LPCWSTR title)
{
	COLORREF bkc0 = SetBkColor(hdc, GetSysColor(COLOR_BTNFACE));

	RECT frect;
	frect = {
		16 - 2,
		16 - 2 + verticalOffset,
		16 + 32 + 2 + 2 + InspectionLabelWidth,
		3 * (16 + 32) + 2 + 2 + verticalOffset };
	FrameRect(hdc, &frect, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
	TextOut(hdc, frect.left, frect.top - 20, title, (int)wcslen(title));

	frect = {
		16 + InspectionLabelWidth,
		16 + verticalOffset,
		16 + 32 + 2 + InspectionLabelWidth,
		16 + 32 + 2 + verticalOffset };
	HDC hdc1 = CreateCompatibleDC(hdc);
	HBITMAP hbmp10 = (HBITMAP)SelectObject(hdc1, hbmColor);
	BitBlt(hdc, frect.left + 1, frect.top + 1, 32, 32, hdc1, 0, 0, SRCCOPY);
	FrameRect(hdc, &frect, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
	TextOut(hdc, frect.left + 2 - InspectionLabelWidth, frect.top + 4, L"XOR", 3);

	WCHAR buf[40];
	wsprintf(buf, L"{%d:%d}", hotspot.x, hotspot.y);
	TextOut(hdc, frect.right + 10, frect.top + 4, buf, (int)wcslen(buf));

	frect = {
		16 + InspectionLabelWidth,
		4 * 16 + verticalOffset,
		16 + 32 + 2 + InspectionLabelWidth,
		3 * 32 + 2 + verticalOffset };
	HDC hdc2 = CreateCompatibleDC(hdc);
	HBITMAP hbmp20 = (HBITMAP)SelectObject(hdc2, hbmMask);
	SelectObject(hdc2, (HBRUSH)GetStockObject(WHITE_BRUSH));
	BitBlt(hdc, frect.left + 1, frect.top + 1, 32, 32, hdc2, 0, 0, SRCCOPY);
	FrameRect(hdc, &frect, (HBRUSH)GetStockObject(BLACK_BRUSH));
	TextOut(hdc, frect.left + 2 - InspectionLabelWidth, frect.top + 4, L"AND", 3);

	frect = {
		16 + InspectionLabelWidth,
		7 * 16 + verticalOffset,
		16 + 32 + 2 + InspectionLabelWidth,
		16 + 4 * 32 + 2 + verticalOffset };
	TextOut(hdc, frect.left + 2 - InspectionLabelWidth, frect.top + 4, L"CUR", 3);

	SetBkColor(hdc, bkc0);
	SelectObject(hdc2, hbmp20);
	SelectObject(hdc1, hbmp10);
	return frect;
}

void GripCursorInspector::inspectCursor(HDC hdc, HCURSOR hc, POINT hotspot, RECT frect)
{
	FillRect(hdc, &frect, (HBRUSH)GetStockObject(WHITE_BRUSH));
	POINT pt = { frect.left + 1, frect.top + 1 };
	DrawIcon(hdc, pt.x, pt.y, hc);
	pt.x += hotspot.x;
	pt.y += hotspot.y;
	HPEN hpen = CreatePen(PS_SOLID, 0, COLORREF_RED);
	HPEN hpen0 = (HPEN)SelectObject(hdc, hpen);
	Ellipse(hdc, pt.x - 3, pt.y - 3, pt.x + 3, pt.y + 3);
	SelectObject(hdc, hpen0);
	FrameRect(hdc, &frect, (HBRUSH)GetStockObject(BLACK_BRUSH));
}
#endif//#ifdef GRIPCURSOR_USES_INSPECTOR

//////////////////////////////////////////////////////////
// GripCursorVector methods and constants

const int CUSTCUR_ARROW_FULLLEN = 24;
const int CUSTCUR_ARROW_HALFLEN = CUSTCUR_ARROW_FULLLEN / 2;
const int CUSTCUR_ARROW_FULLWIDTH = 2;
const int CUSTCUR_ARROW_HALFWIDTH = CUSTCUR_ARROW_FULLWIDTH / 2;
const int CUSTCUR_ARROW_HEADLEN = 5;
const int CUSTCUR_ARROW_HEADWIDTH = 10;
const int CUSTCUR_ARROW_HEADHALFWIDTH = CUSTCUR_ARROW_HEADWIDTH / 2;

// just a stub to indicate that the coordinate is relative to the image's center; bitmap's real center is at (24,24)
const int CENTER_PT = 0;
// these are the x- and y-coordinates of the center of the bitmap image.
const int CENTER_X = 24;
const int CENTER_Y = 24;

/* these points describe the vertices of a double-headed arrow in a West-East orientation.

 the array index number (0 to 10) maps to the vertices as shown below. index 10 wraps around to index 0.

	 (1)                      (4)
	/ |                        | \
   /  |                        |  \
  /  (2)----------------------(3)  \
(0)                                (5)
  \  (8)----------------------(7)  /
   \  |                        |  /
	\ |                        | /
	 (9)                      (6)

in code comments, parts of the arrow shape are mentioned. here is the correspondence.
element 0 : left arrowhead
1 : upper left barb
2 : upper left base
3 : upper right base
4 : upper right barb
5 : right arrowhead
6 : lower right barb
7 : lower right base
8 : lower left base
9 : lower left barb
*/
POINT GCV_arrow[11] =
{
	/* 0 - L arrowhead */{ CENTER_PT - CUSTCUR_ARROW_HALFLEN, CENTER_PT },
	/* 1 - LT barb */{ CENTER_PT - CUSTCUR_ARROW_HALFLEN + CUSTCUR_ARROW_HEADLEN, CENTER_PT - CUSTCUR_ARROW_HEADHALFWIDTH },
	/* 2 - LT base */{ CENTER_PT - CUSTCUR_ARROW_HALFLEN + CUSTCUR_ARROW_HEADLEN, CENTER_PT - CUSTCUR_ARROW_HALFWIDTH },
	/* 3 - RT base */{ CENTER_PT + CUSTCUR_ARROW_HALFLEN - CUSTCUR_ARROW_HEADLEN, CENTER_PT - CUSTCUR_ARROW_HALFWIDTH },
	/* 4 - RT barb */{ CENTER_PT + CUSTCUR_ARROW_HALFLEN - CUSTCUR_ARROW_HEADLEN, CENTER_PT - CUSTCUR_ARROW_HEADHALFWIDTH },
	/* 5 - R arrowhead */{ CENTER_PT + CUSTCUR_ARROW_HALFLEN, CENTER_PT },
	/* 6 - RB barb */{ CENTER_PT + CUSTCUR_ARROW_HALFLEN - CUSTCUR_ARROW_HEADLEN, CENTER_PT + CUSTCUR_ARROW_HEADHALFWIDTH },
	/* 7 - RB base */{ CENTER_PT + CUSTCUR_ARROW_HALFLEN - CUSTCUR_ARROW_HEADLEN, CENTER_PT + CUSTCUR_ARROW_HALFWIDTH },
	/* 8 - LB base */{ CENTER_PT - CUSTCUR_ARROW_HALFLEN + CUSTCUR_ARROW_HEADLEN, CENTER_PT + CUSTCUR_ARROW_HALFWIDTH },
	/* 9 - LB barb */{ CENTER_PT - CUSTCUR_ARROW_HALFLEN + CUSTCUR_ARROW_HEADLEN, CENTER_PT + CUSTCUR_ARROW_HEADHALFWIDTH },
	/*10 - L arrowhead */{ CENTER_PT - CUSTCUR_ARROW_HALFLEN, CENTER_PT },
};

/* GCMASKFIXINFO - this struct is used for generating an AND mask. it holds adjustments (called nudges) to be made to a specific vertice in the XOR mask.
a nudge of, for example, {-1,+1} pushes a vertice to the left by one pixel and up by one pixel.
the count member variable is typically set to 1, indicating that the vertice is nudged just once.
when set to 2, it instructs the code to duplicate the vertice and nudge it to a different direction based on the second set of offsets.
*/
struct GCMASKFIXINFO
{
	int count;
	POINT nudge[2];
};
/* these AND mask adjustments are used for both the vertice-lookup GCV_arrow and vertice-calculated CURVEDARROW.
the 2 types share a basic double-headed arrow shape.
*/
GCMASKFIXINFO GCV_maskfix[11] =
{
	{2, {{-1,1},  { -1, -1 }}}, // arrowhead on the left -- split to 2 nodes
	{2, {{ -1, -1 }, { 1, -1 }}}, // barb, upper left -- 2 nodes
	{1, {{ 1, -1 }}}, // base, upper left
	{1, {{ -1, -1 }}}, // base, upper right
	{2, {{ -1, -1 }, {1, -1}}}, // barb, upper right -- 2 nodes
	{2, {{ 1, -1 }, {1, 1}}}, // arrowhead on the right -- 2 nodes
	{2, {{1, 1}, { -1, 1 }}}, // barb, lower right -- 2 nodes
	{1, {{ -1, 1 }}}, // base, lower right
	{1, {{ 1, 1 }}}, // base, lower left
	{2, {{ 1, 1 }, {-1, 1}}}, // barb, lower left -- 2 nodes
	{1, {{ -1, 1 }}} // back to left arrowhead
};

/* CURVEDARROW configuration in rest position (rotation angle=0)

-24+------------------+--------------------
   |                <-- ANGLE_SPREAD (-36 degrees)
   |          .  /----
   |        .   /_   /  <-- HAED_ANGLE (+10 degrees from -ANGLE_SPREAD)
   |     .         \ \
   |   .            \ \
   | .               | |
 0 +------RADIUS-----+>|
   | .        (INNER)| |(OUTER)
   |   .            / /
   |      .      _ / /_ <-- HEAD_WIDTH
   |         .   \    /  <-- HAED_ANGLE (-10 degrees from +ANGLE_SPREAD)
   |            . \ /
   |                 <-- ANGLE_SPREAD (+36 degrees)
+24+------------------+--------------------

 below are parameters used in calculating the vertices of GRIPID_CURVEDARROW
*/
const int CUSTCUR_CURVYARROW_RADIUS = 24;
const int CUSTCUR_CURVYARROW_RADIUS_OUTER = CUSTCUR_CURVYARROW_RADIUS + 1;
const int CUSTCUR_CURVYARROW_RADIUS_INNER = CUSTCUR_CURVYARROW_RADIUS - 2;
const int CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD = 36;
const int CUSTCUR_CURVYARROW_HEAD_ANGLE = 14;
const int CUSTCUR_CURVYARROW_HEAD_WIDTH = 10;
const int CUSTCUR_CURVYARROW_HEAD_HALFWIDTH = CUSTCUR_CURVYARROW_HEAD_WIDTH / 2;
const int CUSTCUR_CURVYARROW_ORIGIN_X = 0;
const int CUSTCUR_CURVYARROW_ORIGIN_Y = 24;

enum GCV_PATHSEGTYPE
{
	GCVST_OPENPATH = 0,
	GCVST_LINEABS,
	GCVST_LINEREL,
	GCVST_ARC,
	GCVST_ARCREL,
	GCVST_CLOSEPATH,
};

struct GCV_PATHSEGINFO
{
	GCV_PATHSEGTYPE type;
	POINT pt;
	SHORT radius;
	SHORT angle;
};

class GCVPathSegment
{
public:
	GCVPathSegment(const GCV_PATHSEGINFO &si, POINT ptOld, int angleOld, int infl, const GCMASKFIXINFO *msk) : _si(si), _ptBeforeRotation{ 0 }, _priorPt(ptOld), _priorAngle(angleOld), _inflation(infl), _msk(msk) {}

	// configuration of a line or arc segment
	GCV_PATHSEGINFO _si;
	// point from last iteration.
	POINT _priorPt;
	// angle from last iteration
	int _priorAngle;
	// 1 for engaging the inflation; 0 for not engaging it
	int _inflation;
	// an optional instruction to inflate the shape for an AND mask.
	const GCMASKFIXINFO *_msk;
	// new coordinates before the rotation. renderLineRel() will need it.
	POINT _ptBeforeRotation;

	POINT render(FLTRIG &frot, int &inflationIndex, simpleList<POINT> &pts)
	{
		//DBGPRINTF((L"GCVPathSegment.render[%d/%d]: Type=%d; p0={%d:%d}, p={%d:%d}; r=%d, theta=%d\n", inflationIndex, pts.size(), _si.type, _priorPt.x, _priorPt.y, _si.pt.x, _si.pt.y, _si.radius, _si.angle));
		POINT pt = { 0 };
		switch (_si.type)
		{
		case GCVST_OPENPATH:
			pt = start(frot, inflationIndex++, pts);
			break;
		case GCVST_LINEABS:
			pt = renderLine(frot, inflationIndex++, pts);
			break;
		case GCVST_LINEREL:
			pt = renderLineRel(frot, inflationIndex++, pts);
			break;
		case GCVST_ARC:
			pt = renderArc(frot, pts);
			break;
		case GCVST_ARCREL:
			pt = renderArcRel(frot, pts);
			break;
		case GCVST_CLOSEPATH:
			pt = stop(pts);
			break;
		}
		return pt;
	}

protected:
	/* start a path of points. set the first point.
	*/
	POINT start(FLTRIG &frot, int inflationIndex, simpleList<POINT> &pts)
	{
		/* 	we just call renderLine since the method is currently a mere move-to operation.
		*/
		return renderLine(frot, inflationIndex, pts);
	}
	/* close the path by wrapping the end of it to the beginning.
	*/
	POINT stop(simpleList<POINT> &pts)
	{
		/* finish off by copying the very beginning element and copy it to the end of the array.
		*/
		POINT pt = pts[0];
		pts.add(pt);
		return pt;
	}
	/* draw a straight line from prior point to new. the coordinates of the new are relative to the prior coordinates.
	*/
	POINT renderLineRel(FLTRIG &frot, int inflationIndex, simpleList<POINT> &pts)
	{
		FLPOINT fpt;
		if (_si.radius > 0)
		{
			// the new point is the center of a circle.
			// make the relative coordinates absolute. add the latest.
			// the input angle is always absolute. it is measured clockwise from the positive x-axis.
			FLTRIG flt({ _si.pt.x + _priorPt.x, _si.pt.y + _priorPt.y }, _si.angle);
			// get the coordinates of the point on the circumference at the specified angle.
			fpt = flt.circlePoint(_si.radius);
		}
		else
		{
			// the line runs up to the new point that's relative to the prior.
			fpt = { (double)(_si.pt.x + _priorPt.x), (double)(_si.pt.y + _priorPt.y) };
		}
		// inflate it optionally. rotate and save the new point in pts.
		return _rotateAndInflatePoint(fpt, _inflation, inflationIndex, frot, pts);
	}
	/* draw a straight line from prior point to new point. the new point has absolute coordinates.
	*/
	POINT renderLine(FLTRIG &frot, int inflationIndex, simpleList<POINT> &pts)
	{
		FLPOINT fpt;
		if (_si.radius > 0)
		{
			// the new point is the center of a circle.
			// the line stops at a point on the circle at the input angle.
			// the angle is measured clockwise from the positive x-axis.
			FLTRIG flt({ _si.pt.x, _si.pt.y }, _si.angle);
			// get the coordinates of the point on the circumference at the specified angle.
			fpt = flt.circlePoint(_si.radius);
		}
		else
		{
			// the line runs up to the absolute point.
			fpt = { (double)_si.pt.x, (double)_si.pt.y };
		}
		// inflate it optionally. rotate and save the new point in pts.
		return _rotateAndInflatePoint(fpt, _inflation, inflationIndex, frot, pts);
	}
	/* draw an arc segment starting at prior angle and ending at new angle.
	the method computes points on the arc at one-degree intervals.

	to calculate the rotated vertices of CURVEDARROW, use the circle equations and rotational transform equations.
	here is an example case where an arc we want to draw spans from -40 to +40 degrees. the arc's origin is at (0,24). it's radius is 24 units. then, the arc ranges +18 to +24 units in x, while it ranges +15 to -15 units in y.

	P0 = {0,24}

	x = R * cos(f) + P0.x
	y = R * sin(f) + P0.y

	R = 24
	f = +40 .. -40 deg
	x = +18 .. +24
	y = +15 .. -15
	*/
	POINT renderArc(FLTRIG &frot, simpleList<POINT> &pts)
	{
		POINT pt0 = _priorPt;
		POINT pt = { 0 };
		int j;
		// determine the direction, going clockwise or counter-clockwise.
		if (_priorAngle > _si.angle)
		{
			// advance counter-clockwise by one negative degree per iteration.
			for (j = _priorAngle; j > _si.angle; j--)
			{
				// calculate the arc point at next angle, one degree less than current.
				FLTRIG flt({ _si.pt.x, _si.pt.y }, j);
				FLPOINT fpt = flt.circlePoint(_si.radius - _inflation);
				// save the rest position before rotating the point. renderLineRel() will need it.
				_ptBeforeRotation = { lrint(fpt.x), lrint(fpt.y) };
				// rotate the point.
				pt = frot.transformF2Pt(fpt);
				// don't save redundant points.
				if (pt.x == pt0.x && pt.y == pt0.y)
					continue;
				// save the new point.
				pts.add(pt);
				pt0 = pt;
			}
		}
		else
		{
			// advance clockwise by one positive degree per iteration.
			for (j = _priorAngle; j < _si.angle; j++)
			{
				// calculate the arc point at next angle, one degree more than current.
				FLTRIG flt({ _si.pt.x, _si.pt.y }, j);
				FLPOINT fpt = flt.circlePoint(_si.radius + _inflation);
				// save the rest position before rotating the point. renderLineRel() will need it.
				_ptBeforeRotation = { lrint(fpt.x), lrint(fpt.y) };
				// rotate the point.
				pt = frot.transformF2Pt(fpt);
				// don't save redundant points.
				if (pt.x == pt0.x && pt.y == pt0.y)
					continue;
				// save the new point.
				pts.add(pt);
				pt0 = pt;
			}
		}
		return pt;
	}
	POINT renderArcRel(FLTRIG &frot, simpleList<POINT> &pts)
	{
		FLTRIG flt0((long)_priorAngle);
		_si.pt = flt0.itransformPt(_si.radius, _priorPt);
		return renderArc(frot, pts);
	}

	/* _rotateAndInflatePoint rotates the vertice in rest position by an angle specified in frot. it inflates the vertice if the inflation flag indicates so. finally, it saves the rotated vertice in pts.

	parameters:
	fpt - x- and y-coordinates of a vertice in rest position.
	inflation - 1 to inflate the vertice.
	i - index of a vertice mapping the vertice to an element of GCV_maskfix.
	frot - a rotational tranform for the combined rotational angle (i.e., gripper angle plus rotation angle).
	pts - a list to output the rotated vertice to.
	*/
	/* here is the set of simultaneous equations for rotation transformation.
	x' = x0 + x * cos(t) - y * sin(t)
	y' = y0 + x * sin(t) + y * cos(t)
	*/
	POINT _rotateAndInflatePoint(FLPOINT &fpt, int inflation, int i, FLTRIG &frot, simpleList<POINT> &pts)
	{
		_ptBeforeRotation = { lrint(fpt.x), lrint(fpt.y) };
		POINT pt;
		if (inflation)
		{
			pt = { 0 };
			for (int j = 0; j < _msk[i].count; j++)
			{
				FLPOINT fpt2 = fpt;
				fpt2.x -= _msk[i].nudge[j].y;
				fpt2.y += _msk[i].nudge[j].x;
				pt = frot.transformF2Pt(fpt2);
				pts.add(pt);
			}
		}
		else
		{
			pt = frot.transformF2Pt(fpt);
			pts.add(pt);
		}
		return pt;
	}
};


GCV_PATHSEGINFO GCV_curvedarrow[] =
{
	// lower arrow head (in 4th quadrant)
	{GCVST_OPENPATH, {CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, CUSTCUR_CURVYARROW_RADIUS, -CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD},
	// outer blade of lower arrow barb
	{GCVST_LINEABS, {CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, CUSTCUR_CURVYARROW_RADIUS_OUTER + CUSTCUR_CURVYARROW_HEAD_HALFWIDTH, -CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD + CUSTCUR_CURVYARROW_HEAD_ANGLE},
	// base of lower arrow head
	{GCVST_LINEABS, {CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, CUSTCUR_CURVYARROW_RADIUS_OUTER, -CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD + CUSTCUR_CURVYARROW_HEAD_ANGLE},
	// outer curve of the arrow shaft up to the base of the upper arrowhead (in 1st quadrant)
	{GCVST_ARC, {CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y}, CUSTCUR_CURVYARROW_RADIUS_OUTER, CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD - CUSTCUR_CURVYARROW_HEAD_ANGLE },
	// base of the upper arrowhead (in 1st quadrant)
	{GCVST_LINEABS, {CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y}, CUSTCUR_CURVYARROW_RADIUS_OUTER, CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD - CUSTCUR_CURVYARROW_HEAD_ANGLE },
	// outer blade of the upper arrow barb (in 1st quadrant)
	{GCVST_LINEABS, {CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, CUSTCUR_CURVYARROW_RADIUS_OUTER + CUSTCUR_CURVYARROW_HEAD_HALFWIDTH, CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD - CUSTCUR_CURVYARROW_HEAD_ANGLE},
	// endpoint of the upper arrow head (in 1st quadrant)
	{GCVST_LINEABS, {CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, CUSTCUR_CURVYARROW_RADIUS, CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD},
	// inner blade of upper arrow barb
	{GCVST_LINEABS, {CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, CUSTCUR_CURVYARROW_RADIUS_INNER - CUSTCUR_CURVYARROW_HEAD_HALFWIDTH, CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD - CUSTCUR_CURVYARROW_HEAD_ANGLE},
	// inner base of upper arrow barb
	{GCVST_LINEABS, {CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, CUSTCUR_CURVYARROW_RADIUS_INNER, CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD - CUSTCUR_CURVYARROW_HEAD_ANGLE},
	// inner curve of the arrow shaft up to the base of the lower arrowhead (in 4th quadrant)
	{GCVST_ARC, {CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y}, CUSTCUR_CURVYARROW_RADIUS_INNER, -CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD + CUSTCUR_CURVYARROW_HEAD_ANGLE },
	// base of the lower arrowhead (in 4th quadrant)
	{GCVST_LINEABS, {CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y}, CUSTCUR_CURVYARROW_RADIUS_INNER, -CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD + CUSTCUR_CURVYARROW_HEAD_ANGLE },
	// inner blade of the lower barb (in 4th quadrant)
	{GCVST_LINEABS, { CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, CUSTCUR_CURVYARROW_RADIUS_INNER - CUSTCUR_CURVYARROW_HEAD_HALFWIDTH, -CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD + CUSTCUR_CURVYARROW_HEAD_ANGLE},
	// back to where we started, the endpoint of the lower arrowhead (in 4th quadrant)
	{GCVST_CLOSEPATH}
};


const int CUSTCUR_GRIP_HOTSPOT_X = 12;
const int CUSTCUR_GRIP_HOTSPOT_Y = 20;

const int CUSTCUR_GRIP_ORIGIN_X = 23;
const int CUSTCUR_GRIP_ORIGIN_Y = 25;
const int CUSTCUR_GRIP_PALM_RADIUS = 12;
const int CUSTCUR_GRIP_PALM_RADIUS2 = 11;
const int CUSTCUR_GRIP_KNUCKLE_RADIUS = 2;
const int CUSTCUR_GRIP_KNUCKLE_RADIUS2 = 1;
const int CUSTCUR_GRIP_KNUCKLE_INDEX_ORIGIN_X = 17;
const int CUSTCUR_GRIP_KNUCKLE_INDEX_ORIGIN_Y = 13;
const int CUSTCUR_GRIP_KNUCKLE_MIDDLE_ORIGIN_X = 22;
const int CUSTCUR_GRIP_KNUCKLE_MIDDLE_ORIGIN_Y = 12;
const int CUSTCUR_GRIP_KNUCKLE_RING_ORIGIN_X = 27;
const int CUSTCUR_GRIP_KNUCKLE_RING_ORIGIN_Y = 13;
const int CUSTCUR_GRIP_KNUCKLE_PINKY_ORIGIN_X = 32;
const int CUSTCUR_GRIP_KNUCKLE_PINKY_ORIGIN_Y = 16;

GCV_PATHSEGINFO GCV_grip[] =
{
	// start the arc outlining the palm section in 1st quadrant
	{GCVST_OPENPATH, {CUSTCUR_GRIP_ORIGIN_X, CUSTCUR_GRIP_ORIGIN_Y }, CUSTCUR_GRIP_PALM_RADIUS, 5},
	// stop the plam arc with the thumb tucked in 3rd quadrant
	{GCVST_ARC, {CUSTCUR_GRIP_ORIGIN_X, CUSTCUR_GRIP_ORIGIN_Y}, CUSTCUR_GRIP_PALM_RADIUS, 180 + 50},
	// lower endpoint of left side of the index finger in 3rd quadrant.
	{GCVST_LINEREL, {0, 5}, 0, 0},
	// upper endpoint of left side of the index finger
	{GCVST_LINEREL, {0, -9}, 0, 180},
	// knuckle of index finger
	{GCVST_ARC, {CUSTCUR_GRIP_KNUCKLE_INDEX_ORIGIN_X, CUSTCUR_GRIP_KNUCKLE_INDEX_ORIGIN_Y}, CUSTCUR_GRIP_KNUCKLE_RADIUS, 360},
	// lower endpoint of right side of index finger
	{GCVST_LINEREL, {0, 1}, 0, 0},
	// upper endpoint of left side of middle finger
	{GCVST_LINEREL, {0, -2}, 0, 180},
	// knuckle of index finger
	{GCVST_ARC, {CUSTCUR_GRIP_KNUCKLE_MIDDLE_ORIGIN_X, CUSTCUR_GRIP_KNUCKLE_MIDDLE_ORIGIN_Y}, CUSTCUR_GRIP_KNUCKLE_RADIUS, 360},
	// lower endpoint of right side of middle finger
	{GCVST_LINEREL, {0, 2}, 0, 0},
	// upper endpoint of left side of ring finger
	{GCVST_LINEREL, {0, -1}, 0, 180},
	// GCVST_ARC of ring finger
	{GCVST_ARC, {CUSTCUR_GRIP_KNUCKLE_RING_ORIGIN_X, CUSTCUR_GRIP_KNUCKLE_RING_ORIGIN_Y}, CUSTCUR_GRIP_KNUCKLE_RADIUS, 360},
	// lower endpoint of right side of ring finger
	{GCVST_LINEREL, {0, 3}, 0, 0},
	// upper endpoint of left side of pinky finger
	{GCVST_LINEREL, {0, -1}, 0, 180},
	// knuckle of pinky finger
	{GCVST_ARC, {CUSTCUR_GRIP_KNUCKLE_PINKY_ORIGIN_X, CUSTCUR_GRIP_KNUCKLE_PINKY_ORIGIN_Y}, CUSTCUR_GRIP_KNUCKLE_RADIUS, 360},
	// back to where we started, the starting endpoint of the palm arc
	{GCVST_CLOSEPATH}
};

const int CUSTCUR_PINCH_HOTSPOT_X = 5;
const int CUSTCUR_PINCH_HOTSPOT_Y = 13;

const int CUSTCUR_PINCH_ORIGIN_X = 25;
const int CUSTCUR_PINCH_ORIGIN_Y = 25;
const int CUSTCUR_PINCH_PALM_RADIUS = 12;
const int CUSTCUR_PINCH_PALM_RADIUS2 = 11;
const int CUSTCUR_PINCH_INDEX_FINGERTIP_RADIUS = 2; // 1.5
const int CUSTCUR_PINCH_RING_FINGERTIP_RADIUS = 11;
const int CUSTCUR_PINCH_RING_FINGERTIP_RADIUS2 = 12;
const int CUSTCUR_PINCH_THUMBCURVE_RADIUS = 4;
const int CUSTCUR_PINCH_THUMBTIP_RADIUS = 3;

GCV_PATHSEGINFO GCV_pinch[] =
{
	// start the arc outlining the palm section in 1st quadrant
	{GCVST_OPENPATH, {CUSTCUR_PINCH_ORIGIN_X, CUSTCUR_PINCH_ORIGIN_Y }, CUSTCUR_PINCH_PALM_RADIUS, 5},
	// stop the plam arc at the base of thumb in 2nd quadrant
	{GCVST_ARC, {CUSTCUR_PINCH_ORIGIN_X, CUSTCUR_PINCH_ORIGIN_Y}, CUSTCUR_PINCH_PALM_RADIUS, 180},
	// upper endpoint of left side of the thumb
	{GCVST_LINEREL, {0, -8}, 0, 180},
	// round tip of thumb
	{GCVST_ARCREL, {0, 0}, CUSTCUR_PINCH_THUMBTIP_RADIUS, 360},
	// lower endpoint of right side of thumb
	{GCVST_LINEREL, {0, 3}, 0, 180},
	// part 1 of palm curve between thumb and index finger
	{GCVST_ARCREL, {0, 0}, CUSTCUR_PINCH_THUMBCURVE_RADIUS, 0},
	// part 2 of palm curve between thumb and index finger
	{GCVST_ARCREL, {0, 0}, CUSTCUR_PINCH_THUMBCURVE_RADIUS, 290 - 360},

	// upper endpoint of left side of index finger
	{GCVST_LINEREL, {-5, -3}, 0, 0},
	{GCVST_LINEREL, {-3, 0}, 0, 90},
	// tip of index finger
	{GCVST_ARCREL, {0, 0}, CUSTCUR_PINCH_INDEX_FINGERTIP_RADIUS, 270},
	// quarter point of right side of index finger
	{GCVST_LINEREL, {2, 0}, 0, 0},
	// middle point of right side of index finger
	//{GCVST_LINEREL, {4, 1}, 0, 0},
	// endpoint of right side of index finger
	{GCVST_LINEREL, {8, 2}, 0, 0},
	// reverse direction
	{GCVST_LINEREL, {-4, -1}, 0, 270 - 20},

	// right side of ring fingertip
	{GCVST_ARCREL, {0, 0}, CUSTCUR_PINCH_RING_FINGERTIP_RADIUS, 270 + 30},
	{GCVST_ARCREL, {0, 0}, CUSTCUR_PINCH_RING_FINGERTIP_RADIUS2, 360},
	// back to where we started, the starting endpoint of the palm arc
	{GCVST_CLOSEPATH}
};

/* createMaskBitmaps generates a 32x32 cursor of an arrow shape rotated or not.
- prepare and rotate XOR and AND masks.
- show the interim bitmaps and final combined image as a cursor in an inspection pane.
- save the bitmap handles in class member variables. setCursor of base class will use them to generate an HCURSOR and pass it to Win32 SetCursor.
*/
bool GripCursorVector::createMaskBitmaps(HDC hdc)
{
	BITMAP bmColor = { 0, 32, 32, SCANLINELENGTH_32BPP(N_mat), 1, 32, NULL };
	BITMAP bmMask = { 0, 32, 32, SCANLINELENGTH_1BPP(N_mat), 1, 1, NULL };
	HBITMAP hbmColor2 = rotateBitmap(hdc, bmColor, 0);
	HBITMAP hbmMask2 = rotateBitmap(hdc, bmMask, 1);

#ifdef GRIPCURSOR_USES_INSPECTOR
	RECT frect = inspectBitmaps(hdc, hbmColor2, hbmMask2, 12, _hotspot2, L"Type: Vector");
#endif//#ifdef GRIPCURSOR_USES_INSPECTOR

	ICONINFO ii2 = { FALSE, (DWORD)_hotspot2.x, (DWORD)_hotspot2.y, hbmMask2, hbmColor2 };
	_hc = CreateIconIndirect(&ii2);
	/*
	BITMAP bmColor2, bmMask2;
	GetObject(hbmColor2, sizeof(bmColor2), &bmColor2);
	GetObject(hbmMask2, sizeof(bmMask2), &bmMask2);
	_hc = CreateCursor(AppInstanceHandle, _hotspot2.x, _hotspot2.y, bmColor2.bmWidth, bmColor2.bmHeight, bmMask2.bmBits, bmColor2.bmBits);
	*/
#ifdef GRIPCURSOR_USES_INSPECTOR
	inspectCursor(hdc, _hc, _hotspot2, frect);
#endif//#ifdef GRIPCURSOR_USES_INSPECTOR

	_hbmColor2 = hbmColor2;
	_hbmMask2 = hbmMask2;
	return true;
}

#ifdef GCV_LONGHAND
/* _rotateAndInflatePoint rotates the vertice in rest position by an angle specified in frot. it inflates the vertice if the inflation flag indicates so. finally, it saves the rotated vertice in pts.

parameters:
fpt - x- and y-coordinates of a vertice in rest position.
inflation - 1 to inflate the vertice.
i - index of a vertice mapping the vertice to an element of GCV_maskfix.
frot - a rotational tranform for the combined rotational angle (i.e., gripper angle plus rotation angle).
pts - a list to output the rotated vertice to.
*/
POINT GripCursorVector::_rotateAndInflatePoint(FLPOINT &fpt, int inflation, int i, FLTRIG frot, simpleList<POINT> &pts)
{
	POINT pt;
	if (inflation)
	{
		pt = { 0 };
		for (int j = 0; j < GCV_maskfix[i].count; j++)
		{
			FLPOINT fpt2 = fpt;
			fpt2.x -= GCV_maskfix[i].nudge[j].y;
			fpt2.y += GCV_maskfix[i].nudge[j].x;
			pt = frot.transformF2Pt(fpt2);
			pts.add(pt);
		}
	}
	else
	{
		pt = frot.transformF2Pt(fpt);
		pts.add(pt);
	}
	return pt;
}
#endif//#ifdef GCV_LONGHAND

/* _moveShapeToLT moves back the vertices of a shape in pts toward the upper-left corner of the cursor frame. it adjusts the offsetting amount if the shape is inflated (to make an AND mask). finally, it updates the hotspot to the new location.
*/
POINT GripCursorVector::_moveShapeToLT(int inflation, int rotationAngle, simpleList<POINT> &pts)
{
	int x_min = N_mat, y_min = N_mat;
	int x_max = 0, y_max = 0;
	int i;
	for (i = 0; i < (int)pts.size(); i++)
	{
		if (pts[i].x < x_min)
			x_min = pts[i].x;
		if (pts[i].y < y_min)
			y_min = pts[i].y;

		if (pts[i].x > x_max)
			x_max = pts[i].x;
		if (pts[i].y > y_max)
			y_max = pts[i].y;
	}
	if (!inflation)
	{
		int delta = rotationAngle == 0 ? 2 : 1;
		x_min -= delta;
		y_min -= delta;
	}
	for (i = 0; i < (int)pts.size(); i++)
	{
		pts[i].x -= x_min;
		pts[i].y -= y_min;
	}
	_hotspot2.x -= x_min;
	_hotspot2.y -= y_min;
	DBGPRINTF((L"_moveShapeToLT: rotation=%d; inflation=%d; min=(%d:%d); hotspot=(%d:%d)==>(%d:%d)\n", rotationAngle, inflation, x_min, y_min, _hotspot.x, _hotspot.y, _hotspot2.x, _hotspot2.y));
	return { x_min,y_min };
}

/* rotateBitmap generates and rotates a raster image based on a predefined vector-based shape drawing configuration. currently, the method can handle two types of shapes.
1) straight arrow with two arrowheads, one on the left, and the other on the right.
2) curved arrow with two arrowheads, one heading north, and the other south.
*/
HBITMAP GripCursorVector::rotateBitmap(HDC hdc, BITMAP &bm, int inflation)
{
	SIZE sz = { bm.bmWidth,bm.bmHeight };

	// theta0 will hold a built-in angle associated with a gripper the mouse sits on.
	short theta0;
	switch (_gripId)
	{
	case GRIPID_SIZENW: // left top corner (north west)
	case GRIPID_SIZESE: // right bottom corner (south east)
		theta0 = +45;
		break;
	case GRIPID_SIZESW: // left bottom corner (south west)
	case GRIPID_SIZENE: // right top corner (north east)
		theta0 = -45;
		break;
	case GRIPID_SIZENORTH: // top midway (north)
	case GRIPID_SIZESOUTH: // bottom midway (south)
		theta0 = +90;
		break;
		/*case GRIPID_SIZEWEST: // left midway (west)
		case GRIPID_SIZEEAST: // right midway (east)
		case GRIPID_CURVEDARROW:
		case GRIPID_GRIP:*/
	default:
		theta0 = 0;
		break;
	}

	_hotspot = _hotspot2 = { CENTER_X,CENTER_Y };

	short theta = theta0 + _rotationAngle;

	int i, j;
	POINT pt = { 0 };
	simpleList<POINT> pts;

	if (_gripId == GRIPID_GRIP)
	{
		// i indexes GCV_maskfix.
		i = 0;
		// define a transform for the rotated space.
		FLTRIG frot({ 0, 0 }, (long)theta);

		for (j = 0; j < ARRAYSIZE(GCV_grip); j++)
		{
			GCVPathSegment shape(GCV_grip[j], pt, j == 0 ? 0 : GCV_grip[j - 1].angle, 0, NULL);
			pt = shape.render(frot, i, pts);
			if (j < ARRAYSIZE(GCV_grip) - 1 && (GCV_grip[j + 1].type == GCVST_LINEREL || GCV_grip[j + 1].type == GCVST_ARCREL))
				pt = shape._ptBeforeRotation;
		}

		FLTRIG fhotspot({ CUSTCUR_GRIP_HOTSPOT_X, CUSTCUR_GRIP_HOTSPOT_Y }, 0);
		FLPOINT fpt = fhotspot.circlePoint(CUSTCUR_GRIP_PALM_RADIUS);
		_hotspot2 = frot.transformF2Pt(fpt);
		_moveShapeToLT(0, theta, pts);
	}
	else if (_gripId == GRIPID_PINCH)
	{
		// i indexes GCV_maskfix.
		i = 0;
		// define a transform for the rotated space.
		FLTRIG frot({ 0, 0 }, (long)theta);

		for (j = 0; j < ARRAYSIZE(GCV_pinch); j++)
		{
			GCVPathSegment shape(GCV_pinch[j], pt, j == 0 ? 0 : GCV_pinch[j - 1].angle, 0, NULL);
			pt = shape.render(frot, i, pts);
			if (j < ARRAYSIZE(GCV_pinch) - 1 && (GCV_pinch[j + 1].type == GCVST_LINEREL || GCV_pinch[j + 1].type == GCVST_ARCREL))
				pt = shape._ptBeforeRotation;
		}

		FLTRIG fhotspot({ CUSTCUR_PINCH_HOTSPOT_X, CUSTCUR_PINCH_HOTSPOT_Y }, 0);
		FLPOINT fpt = fhotspot.circlePoint(CUSTCUR_PINCH_PALM_RADIUS);
		_hotspot2 = frot.transformF2Pt(fpt);
		_moveShapeToLT(0, theta, pts);
	}
	else if (_gripId == GRIPID_CURVEDARROW)
	{
		/* calculate the rotated vertices of CURVEDARROW using the circle equations and rotational transform equations.

		P0 = {0,24}

		x = R * cos(f) + P0.x
		y = R * sin(f) + P0.y

		R = 24
		f = +40 .. -40 deg
		x = +18 .. +24
		y = +15 .. -15
		*/
		/*
		x' = x0 + x * cos(t) - y * sin(t)
		y' = y0 + x * sin(t) + y * cos(t)
		*/
		// i indexes GCV_maskfix.
		i = 0;
		// define a transform for the rotated space.
		FLTRIG frot({ 0, 0 }, (long)theta);
#ifdef GCV_LONGHAND
		// define a transform for the lower arrow head (in 4th quadrant).
		FLTRIG fheadLo({ CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, -CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD);
		// get the arrowhead's coordinates.
		FLPOINT fpt = fheadLo.circlePoint(CUSTCUR_CURVYARROW_RADIUS);
		// if requested inflate it. rotate and save the result point in pts.
		_rotateAndInflatePoint(fpt, inflation, i++, frot, pts);
		// rotate and save the outer blade of lower arrow barb.
		FLTRIG fbarbLo({ CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, -CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD + CUSTCUR_CURVYARROW_HEAD_ANGLE);
		fpt = fbarbLo.circlePoint(CUSTCUR_CURVYARROW_RADIUS_OUTER + CUSTCUR_CURVYARROW_HEAD_HALFWIDTH);
		_rotateAndInflatePoint(fpt, inflation, i++, frot, pts);
		// move to the base of lower arrow head.
		fpt = fbarbLo.circlePoint(CUSTCUR_CURVYARROW_RADIUS_OUTER);
		POINT pt0 = _rotateAndInflatePoint(fpt, inflation, i++, frot, pts);

		// calculate and save rotated locations of pixels of the outer curve of the arrow shaft.
		for (j = -CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD + CUSTCUR_CURVYARROW_HEAD_ANGLE + 1; j < CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD - CUSTCUR_CURVYARROW_HEAD_ANGLE; j++)
		{
			FLTRIG flt({ CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, j);
			fpt = flt.circlePoint(CUSTCUR_CURVYARROW_RADIUS_OUTER + inflation);
			pt = frot.transformF2Pt(fpt);
			// avoid redundant points
			if (pt.x == pt0.x && pt.y == pt0.y)
				continue;
			pts.add(pt);
			pt0 = pt;
		}

		// rotate and save the base of the upper arrow head (in 1st quadrant).
		FLTRIG fbarbHi({ CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD - CUSTCUR_CURVYARROW_HEAD_ANGLE);
		fpt = fbarbHi.circlePoint(CUSTCUR_CURVYARROW_RADIUS_OUTER);
		_rotateAndInflatePoint(fpt, inflation, i++, frot, pts);
		// go to the outer blade of the upper arrow barb.
		fpt = fbarbHi.circlePoint(CUSTCUR_CURVYARROW_RADIUS_OUTER + CUSTCUR_CURVYARROW_HEAD_HALFWIDTH);
		_rotateAndInflatePoint(fpt, inflation, i++, frot, pts);
		// to the endpoint of the upper arrow head.
		FLTRIG fheadHi({ CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD);
		fpt = fheadHi.circlePoint(CUSTCUR_CURVYARROW_RADIUS);
		_rotateAndInflatePoint(fpt, inflation, i++, frot, pts);
		// to the inner blade of upper arrow barb.
		fpt = fbarbHi.circlePoint(CUSTCUR_CURVYARROW_RADIUS_INNER - CUSTCUR_CURVYARROW_HEAD_HALFWIDTH);
		_rotateAndInflatePoint(fpt, inflation, i++, frot, pts);
		// to the inner base of upper arrow barb.
		fpt = fbarbHi.circlePoint(CUSTCUR_CURVYARROW_RADIUS_INNER);
		pt0 = _rotateAndInflatePoint(fpt, inflation, i++, frot, pts);

		// do the inner curve of the arrow shaft.
		for (j = CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD - CUSTCUR_CURVYARROW_HEAD_ANGLE - 1; j > -CUSTCUR_CURVYARROW_HALF_ANGLE_SPREAD + CUSTCUR_CURVYARROW_HEAD_ANGLE; j--)
		{
			FLTRIG flt({ CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, j);
			fpt = flt.circlePoint(CUSTCUR_CURVYARROW_RADIUS_INNER - inflation);
			pt = frot.transformF2Pt(fpt);
			if (pt.x == pt0.x && pt.y == pt0.y)
				continue;
			pts.add(pt);
			pt0 = pt;
		}

		// back to the base of the lower arrowhead.
		fpt = fbarbLo.circlePoint(CUSTCUR_CURVYARROW_RADIUS_INNER);
		_rotateAndInflatePoint(fpt, inflation, i++, frot, pts);
		// and to the inner blade of the lower barb.
		fpt = fbarbLo.circlePoint(CUSTCUR_CURVYARROW_RADIUS_INNER - CUSTCUR_CURVYARROW_HEAD_HALFWIDTH);
		_rotateAndInflatePoint(fpt, inflation, i++, frot, pts);

		// back to where we started, the endpoint of the lower arrowhead.
		pt = pts[0];
		pts.add(pt);
#else
		for (j = 0; j < ARRAYSIZE(GCV_curvedarrow); j++)
		{
			GCVPathSegment shape(GCV_curvedarrow[j], pt, j == 0 ? 0 : GCV_curvedarrow[j - 1].angle, inflation, GCV_maskfix);
			pt = shape.render(frot, i, pts);
		}
#endif

		FLTRIG fhotspot({ CUSTCUR_CURVYARROW_ORIGIN_X, CUSTCUR_CURVYARROW_ORIGIN_Y }, 0);
		FLPOINT fpt = fhotspot.circlePoint(CUSTCUR_CURVYARROW_RADIUS);
		_hotspot2 = frot.transformF2Pt(fpt);
		_moveShapeToLT(inflation, theta + 90, pts);
	}
	else if (theta != 0)
	{
		/*
		x' = x0 + x * cos(t) - y * sin(t)
		y' = y0 + x * sin(t) + y * cos(t)
		*/
		FLTRIG frot({ CENTER_X, CENTER_Y }, (long)theta);
		for (i = 0; i < ARRAYSIZE(GCV_arrow); i++)
		{
			pt = GCV_arrow[i];
			if (!inflation)
			{
				pt = frot.transformPt(pt);
				pts.add(pt);
			}
			else
			{
				for (j = 0; j < GCV_maskfix[i].count; j++)
				{
					pt = GCV_arrow[i];
					pt.x += GCV_maskfix[i].nudge[j].x;
					pt.y += GCV_maskfix[i].nudge[j].y;
					pt = frot.transformPt(pt);
					pts.add(pt);
				}
			}
		}
		_moveShapeToLT(inflation, theta, pts);
	}
	else
	{
		for (i = 0; i < ARRAYSIZE(GCV_arrow); i++)
		{
			pt.x = GCV_arrow[i].x + CENTER_X;
			pt.y = GCV_arrow[i].y + CENTER_Y;
			if (!inflation)
			{
				pts.add(pt);
			}
			else
			{
				for (j = 0; j < GCV_maskfix[i].count; j++)
				{
					pt.x += GCV_maskfix[i].nudge[j].x;
					pt.y += GCV_maskfix[i].nudge[j].y;
					pts.add(pt);
				}
			}
		}
		_moveShapeToLT(inflation, theta, pts);
	}

#ifdef GRIPCURSOR_USES_INSPECTOR
	COLORREF bkc0 = SetBkColor(hdc, GetSysColor(COLOR_BTNFACE));
	// draw the prepared bitmap in an inspection frame.
	RECT rcdbg = { 10, 220, 10 + InspectionLabelWidth + N_mat + 4, 220 + 2 * N_mat + 16 + 8 };
	// 1st call generates an XOR mask. show it in top half.
	// 2nd call generates an AND mask. show it in bottom half.
	if (bm.bmBitsPixel > 1)
	{
		// for 1st call, do extra.
		// print a title indicating this is debug stuff.
		TextOut(hdc, rcdbg.left, rcdbg.top - 20, L"Matrix", 6);
		// draw a frame.
		FrameRect(hdc, &rcdbg, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
		// print a label for the image.
		TextOut(hdc, rcdbg.left + 4, rcdbg.top + 8, L"XOR", 3);
	}
	else
	{
		// 2nd call draws the AND mask. place it below the XOR mask.
		rcdbg.top += N_mat + 16;
		TextOut(hdc, rcdbg.left + 4, rcdbg.top + 8, L"AND", 3);
	}
	// define a rectangle where the mask bitmap is drawn.
	int x0 = rcdbg.left + InspectionLabelWidth;
	int y0 = rcdbg.top + 4;
	// clear inside the frame to get rid of a previous image.
	rcdbg = { x0 - 1, y0 - 1, x0 + N_mat + 1,y0 + N_mat + 1 };
	FillRect(hdc, &rcdbg, (HBRUSH)GetStockObject(WHITE_BRUSH));
	// make a copy of the vector points of the generated image.
	// each point is offset so it moves into the inspection area.
	simpleList<POINT> dbgpts(pts);
	for (i = 0; i < (int)dbgpts.size(); i++)
	{
		dbgpts[i].x += x0;
		dbgpts[i].y += y0;
	}
	// paint the generated image.
	Polygon(hdc, dbgpts.data(), (int)dbgpts.size());
	// draw a gray frame around it.
	FrameRect(hdc, &rcdbg, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
	SetBkColor(hdc, bkc0);
#endif//#ifdef GRIPCURSOR_USES_INSPECTOR

	HDC hdcDest = CreateCompatibleDC(hdc);

	BITMAPINFO bmi2 = { 0 };
	bmi2.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi2.bmiHeader.biWidth = bm.bmWidth;
	bmi2.bmiHeader.biHeight = bm.bmHeight;
	bmi2.bmiHeader.biPlanes = bm.bmPlanes;
	bmi2.bmiHeader.biBitCount = bm.bmBitsPixel;
	bmi2.bmiHeader.biCompression = BI_RGB;
	bmi2.bmiHeader.biSizeImage = bm.bmHeight * bm.bmWidthBytes;
	LPBYTE pix;
	HBITMAP hbmDest = CreateDIBSection(hdcDest, &bmi2, DIB_RGB_COLORS, (LPVOID*)&pix, NULL, 0);

	HBITMAP hbmDest0 = (HBITMAP)SelectObject(hdcDest, hbmDest);

	// destination is filled with zeroes (black)
	int brFore = WHITE_BRUSH;
	int brBkgd = BLACK_BRUSH;
	COLORREF clrBkgd = COLORREF_BLACK;
	COLORREF clrFore = COLORREF_WHITE;
#ifdef GC_OUTLINE_IN_COLOR
	bool fatPen = (_gripId == GRIPID_GRIP || _gripId == GRIPID_PINCH);
	COLORREF clrOutline = fatPen ? COLORREF_BLACK : COLORREF_GRAY;
	int thicknessOutline = 0;
#endif//#ifdef GC_OUTLINE_IN_COLOR
	if (bm.bmBitsPixel == 1)
	{
		// AND mask uses white as background
		brFore = BLACK_BRUSH;
		brBkgd = WHITE_BRUSH;
		clrBkgd = COLORREF_WHITE;
		clrFore = COLORREF_BLACK;
#ifdef GC_OUTLINE_IN_COLOR
		clrOutline = COLORREF_BLACK;
		if (fatPen)
			thicknessOutline = 3;
#endif//#ifdef GC_OUTLINE_IN_COLOR
	}

	/* fill the and mask in white before drawing the rotated cursor shape.
	*/
	if (bm.bmBitsPixel == 1)
	{
		RECT frc = { 0,0,bm.bmWidth, bm.bmHeight };
		FillRect(hdcDest, &frc, (HBRUSH)GetStockObject(brBkgd));
	}

	HBRUSH hbr = (HBRUSH)GetStockObject(brFore);
	HBRUSH hbr0 = (HBRUSH)SelectObject(hdcDest, hbr);
#ifdef GC_OUTLINE_IN_COLOR
	HPEN hpen = CreatePen(PS_SOLID, thicknessOutline, clrOutline);
	HPEN hpen0 = (HPEN)SelectObject(hdcDest, hpen);
#endif//#ifdef GC_OUTLINE_IN_COLOR

	Polygon(hdcDest, pts.data(), (int)pts.size());

	SelectObject(hdcDest, hbmDest0);
	SelectObject(hdcDest, hbr0);
#ifdef GC_OUTLINE_IN_COLOR
	SelectObject(hdcDest, hpen0);
	DeleteObject(hpen);
#endif//#ifdef GC_OUTLINE_IN_COLOR
	DeleteDC(hdcDest);

	return hbmDest;
}

