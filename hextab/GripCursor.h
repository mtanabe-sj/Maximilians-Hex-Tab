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


//#define GRIPCURSOR_USES_INSPECTOR


struct FLPOINT
{
	double x;
	double y;
};

struct FLTRIG
{
	POINT X0;
	double cos_theta, sin_theta, theta;

	FLTRIG(POINT origPt, long deg)
	{
		X0 = origPt;
		theta = M_PI * (double)deg / 180.0;
		cos_theta = cos(theta);
		sin_theta = sin(theta);
	}
	FLTRIG(long deg)
	{
		X0 = { 0 };
		theta = M_PI * (double)deg / 180.0;
		cos_theta = cos(theta);
		sin_theta = sin(theta);
	}

	FLPOINT transformFPt(FLPOINT fpt)
	{
		FLPOINT fpt2;
		fpt2.x = fpt.x*cos_theta - fpt.y*sin_theta + (double)X0.x;
		fpt2.y = fpt.x*sin_theta + fpt.y*cos_theta + (double)X0.y;
		return fpt2;
	}
	POINT transformF2Pt(FLPOINT fpt)
	{
		FLPOINT fpt2 = transformFPt(fpt);
		return { lrint(fpt2.x), lrint(fpt2.y) };
	}
	POINT transformPt(POINT pt)
	{
		FLPOINT fpt = { (double)pt.x, (double)pt.y };
		return transformF2Pt(fpt);
	}
	POINT itransformPt(int radius, POINT pt)
	{
		double x, y, R;
		x = pt.x;
		y = pt.y;
		R = (double)radius;
		double x0 = x - R * cos_theta;
		double y0 = y - R * sin_theta;
		X0.x = lrint(x0);
		X0.y = lrint(y0);
		return X0;
	}

	POINT decomposeX(double X)
	{
		double x2 = X * cos_theta;
		double y2 = X * sin_theta;
		return { lrint(x2), lrint(y2) };
	}
	POINT decomposeIX(long X)
	{
		return decomposeX((double)X);
	}
	POINT decomposeY(double Y, POINT *offset = NULL)
	{
		double x2 = Y * sin_theta;
		double y2 = Y * cos_theta;
		POINT v = { lrint(x2), lrint(y2) };
		if (offset)
		{
			v.x += offset->x;
			v.y += offset->y;
		}
		return v;
	}
	POINT decomposeIY(long Y, POINT *offset = NULL)
	{
		return decomposeY((double)Y, offset);
	}
	POINT decomposeHalfIY(long Y)
	{
		return decomposeY(0.5 * (double)Y);
	}

	POINT transformX(double X)
	{
		POINT pt = decomposeX(X);
		return { pt.x + X0.x, pt.y + X0.y };
	}
	POINT transformIX(long X)
	{
		return transformX((double)X);
	}

	FLPOINT circlePoint(int radius)
	{
		FLPOINT fpt;
		fpt.x = radius * cos_theta + X0.x;
		fpt.y = radius * sin_theta + X0.y;
		return fpt;
	}
};

////////////////////////////////////////////////////////////////

const int N_mat = 48;
const int InspectionLabelWidth = 40;

////////////////////////////////////////////////////////////////

enum GC_CURSORTYPE
{
	GCCT_NONE = 0,
	GCCT_VECTOR,
	GCCT_RASTER,
	GCCT_RASTER2,
};

class GripCursor
{
public:
	GripCursor() : _hc(NULL), _hbmColor2(NULL), _hbmMask2(NULL), _rotationAngle(0), _gripId(-1), _cursorType(GCCT_NONE) {}
	~GripCursor() { clear(); }

	HCURSOR getCursor(HWND hwnd, const DUMPSHAPEINFO &shapeInfo, GRIPID gripId, bool ignoreRotation = false);

	short _rotationAngle;
	POINT _hotspot, _hotspot2;
	LPCWSTR _resId;
	GC_CURSORTYPE _cursorType;

protected:
	HCURSOR _hc;
	int _gripId;
	HBITMAP _hbmColor2, _hbmMask2;

	virtual bool createMaskBitmaps(HDC hdc) { return false; }

	void clear();
};

#ifdef GRIPCURSOR_USES_INSPECTOR
class GripCursorInspector : public GripCursor
{
public:
	GripCursorInspector() : _hwndInspect(NULL) {}

	void setInspectionPane(HWND hwnd) { _hwndInspect = hwnd; }

protected:
	HWND _hwndInspect;

	RECT inspectBitmaps(HDC hdc, HBITMAP hbmColor, HBITMAP hbmMask, int verticalOffset, POINT hotspot, LPCWSTR title);
	void inspectCursor(HDC hdc, HCURSOR hc, POINT hotspot, RECT frect);
};
#endif//#ifdef GRIPCURSOR_USES_INSPECTOR

class GripCursorVector :
#ifdef GRIPCURSOR_USES_INSPECTOR
	public GripCursorInspector
#else//#ifdef GRIPCURSOR_USES_INSPECTOR
	public GripCursor
#endif//#ifdef GRIPCURSOR_USES_INSPECTOR
{
public:
	GripCursorVector() { _cursorType = GCCT_VECTOR; }

protected:
	virtual bool createMaskBitmaps(HDC hdc);
private:
	HBITMAP rotateBitmap(HDC hdc, BITMAP &bm, int inflation);
	POINT _moveShapeToLT(int inflation, int rotationAngle, simpleList<POINT> &pts);
#ifdef GCV_LONGHAND
	POINT _rotateAndInflatePoint(FLPOINT &fpt, int inflation, int i, FLTRIG frot, simpleList<POINT> &pts);
#endif//#ifdef GCV_LONGHAND
};

