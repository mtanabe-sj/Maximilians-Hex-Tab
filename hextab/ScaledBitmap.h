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

#ifdef _DEBUG
#define SB_SAVEBMP(h,f,t,w) SaveBmp(h,f,t,w)
#else//#ifdef _DEBUG
#define SB_SAVEBMP(h,f,t,w)
#endif//#ifdef _DEBUG


#define MAX_8BPP_RGB_COLORS 0x100


class ScaledBitmapPixels32bpp
{
public:
	ScaledBitmapPixels32bpp(SIZE s, SIZE d) : _srcSz { s }, _destSz{ d } {}

	SIZE _srcSz, _destSz; // width and height pairs of the source and destination bitmaps.

	virtual COLORREF getPixel(LPVOID dataSrc, int x, int y)
	{
		int i = x + y * _srcSz.cx;
		COLORREF *p = (COLORREF *)dataSrc + i;
		return *p;
	}
	virtual void setPixel(LPVOID dataDest, int x, int y, COLORREF rgb)
	{
		int i = x + y * _destSz.cx;
		COLORREF *p = (COLORREF *)dataDest + i;
		*p = rgb;
	}

	/* squeezes source pixels into a destination pixel. the source image is larger in size than the destination
	image. the two images has the same color depth of 32 bpp.

	because the source image is larger, multiple pixels of the source image map to a single pixel
	in the destination image.

	let's consider how a pixel squeeze is done by examining an example case.
	the illustration below indicates how a group of 4x4 pixels map to a single pixel in the destination
	the shaded inner rectangle represents a single destination pixel. a destination pixel consists of
	1) a subgroup of partial source pixels a, b, c and d, that form the destination pixel's 4 corners,
	2) a subgroup of partial source pixels, A, B, C, and D, that form the destination's edges, and
	3) a subroup of whole source pixels of E that fill the destination's pixel.

			  +-----+------+------+------+
			  |  +--+------+------+----+ |
			  |  |/a|////A/|////A/|//b/| |
			  +--+--+------+------+----+-+
			  |  |//|//////|//////|////| |
			  |  |/C|////E/|////E/|//D/| |
			  +--+--+------+------+----+-+
			  |  |//|//////|//////|////| |
			  |  |/C|////E/|////E/|//D/| |
			  +--+--+------+------+----+-+
			  |  |/c|////B/|////B/|//d/| |
			  |  +--+------+------+----+ |
			  +-----+------+------+------+

	the squeeze function calculates the source pixel's relative area that maps to the destination
	pixel, and determines how much of its color value contributes to the destination pixel's color.
	for example, if the partial pixel of part 'a' occupies a 25% of the pixel's whole area,
	the pixel's color contribution too is 25% of its RGB. if part 'A' is 60%, it contributes 60% of
	its RGB. part 'E' is 100%. so, it contributes 100% of its RGB. the RGBs of all the contributing
	whole and partial pixels are summed and divided by the number of the pixels, yeilding an
	average RGB. that's the color we assign to the destination pixel.

	the function performs the color calculations for the participating source pixels in this order.
	1) partial source pixel 'a' that maps to the upper-left hand corner of the destination pixel,
	2) partial source pixel 'b' that maps to the upper-right hand corner of the destination,
	3) partial source pixel(s) 'A', that map to the upper edge of the destination,
	4) partial source pixel 'c' that maps to the lower-left hand corner,
	5) partial source pixel 'd' that maps to the lower-right hand corner,
	6) partial source pixel(s) 'B' that map to the lower edge,
	7) partial source pixel(s) 'C' that map to the left edge,
	8) partial source pixel(s) 'D' that map to the right edge, and
	9) whole source pixel(s) 'E' that map to the interior of the destination.
	*/
	COLORREF squeeze(int xDest, int yDest, RECT *rcDest, LPVOID dataSrc)
	{
		COLORREF rgb;
		int x1, y1, x2, y2, x, y;
		double rx, ry, fx, fy, dfx, dfy, ddf;
		double R, G, B;
		double r = 0, g = 0, b = 0, f = 0;
		int i, j;
#ifdef _DEBUG_PIXELSQUEEZE
		double r0 = 0, g0 = 0, b0 = 0;
		int n = 0;
#endif//#ifdef _DEBUG_PIXELSQUEEZE

		// work out the top row of partial source pixels, 'a', 'b', and 'A' in that order.
		// i tracks the scanline for destination image.
		i = yDest;
		// first, do part 'a'.
		// j tracks the destination pixel's position within the scanline.
		j = xDest;
		// get relative distances from the top and left edges in the destination.
		ry = _ratioRowIndexToImageHeight(i, rcDest);
		rx = _ratioColumnIndexToImageWidth(j, rcDest);
		// based on the destination's relative distance, get the source's left-most pixel.
		fy = (double)_srcSz.cy * ry;
		fx = (double)_srcSz.cx * rx;
		// round it down. the coordinates refer to left-most source pixel 'A'.
		// it's not the 'a', though.
		y = (int)fy;
		x = (int)fx;
		// isolate the part of the left-most pixel 'a' that sticks out to the left of the
		// destination pixel.
		dfy = fy - (double)y;
		dfx = fx - (double)x;
		//if (dfy > 0 && dfx > 0)
		{
			// take a compliment. that's the area that forms the left-top corner of destination.
			dfy = 1.0 - dfy;
			dfx = 1.0 - dfx;
			// get the relative area of the part to the whole (which is a unit area).
			ddf = dfy * dfx;
			// pick the color of partial source pixel 'a'
			rgb = getPixel(dataSrc, x, y);
			// add it to the sum.
			R = (double)(int)GetRValue(rgb);
			G = (double)(int)GetGValue(rgb);
			B = (double)(int)GetBValue(rgb);
			r += ddf * R;
			g += ddf * G;
			b += ddf * B;
			f += ddf;
#ifdef _DEBUG_PIXELSQUEEZE
			r0 += R;
			g0 += G;
			b0 += B;
			n++;
#endif//#ifdef _DEBUG_PIXELSQUEEZE
		}

		// keep the horizontal and vertical coordinates of the left-most source pixel.
		// we will use them for the pixel 'A' loop below.
		x1 = x + 1;
		y1 = y + 1;

		// go to source pixel 'b'.
		// move right to the adjacent pixel in destination.
		j++;
		// get the relative horizontal distance. note that the vertical is not moved.
		rx = _ratioColumnIndexToImageWidth(j, rcDest);
		// apply it to the source to get the horizontal distance in source image.
		fx = (double)_srcSz.cx * rx;
		// round it down. this is the left edge of pixel 'b'.
		x = (int)fx;
		// get the source partial area that forms the right-top corner of destination.
		dfx = fx - (double)x;
		// watch out. the top and/or right edges of the destination can fall exactly
		// on the edges of the source pixel. if so, pixel 'b' makes no contribution.
		if (dfy > 0 && dfx > 0)
		{
			// this is the ratio of the contributing area to the whole (1)
			ddf = dfy * dfx;
			// pick the color from pixel 'b'.
			rgb = getPixel(dataSrc, x, y);
			// add it to the averaging sum.
			R = (double)(int)GetRValue(rgb);
			G = (double)(int)GetGValue(rgb);
			B = (double)(int)GetBValue(rgb);
			r += ddf * R;
			g += ddf * G;
			b += ddf * B;
			f += ddf;
#ifdef _DEBUG_PIXELSQUEEZE
			r0 += R;
			g0 += G;
			b0 += B;
			n++;
#endif//#ifdef _DEBUG_PIXELSQUEEZE
		}

		// save the horizontal position of the right-most pixel in source image.
		x2 = x;
		// work on pixels 'A'.
		for (j = x1; j < x2; j++)
		{
			// the relative area has a unit width and depends only on the vertical extent.
			ddf = dfy;
			// pick the color from pixel 'A'.
			rgb = getPixel(dataSrc, j, y);
			// add the color contribution to the averaging sum.
			R = (double)(int)GetRValue(rgb);
			G = (double)(int)GetGValue(rgb);
			B = (double)(int)GetBValue(rgb);
			r += ddf * R;
			g += ddf * G;
			b += ddf * B;
			f += ddf;
#ifdef _DEBUG_PIXELSQUEEZE
			r0 += R;
			g0 += G;
			b0 += B;
			n++;
#endif//#ifdef _DEBUG_PIXELSQUEEZE
		}

		// work on pixel 'c'.
		// move down a scanline.
		i++;
		// go back to the left edge.
		j = xDest;
		ry = _ratioRowIndexToImageHeight(i, rcDest);
		rx = _ratioColumnIndexToImageWidth(j, rcDest);
		// get the coordinates of the source pixel that corresponds to the destination's left-bottom corner.
		fy = (double)_srcSz.cy * ry;
		fx = (double)_srcSz.cx * rx;
		// round down. this points to pixel 'c' in source.
		y = (int)fy;
		x = (int)fx;
		// calculate the partial area of pixel 'c'. it forms the destination's left-bottom corner.
		dfy = fy - (double)y;
		dfx = fx - (double)x;
		if (dfy > 0 && dfx > 0)
		{
			// take a compliment for the horizontal distance. that's the one that contributes.
			dfx = 1.0 - dfx;
			// get the contributing area of pixel 'c'.
			ddf = dfy * dfx;
			// pick the color from pixel 'c'.
			rgb = getPixel(dataSrc, x, y);
			// add it to the averaging sum.
			R = (double)(int)GetRValue(rgb);
			G = (double)(int)GetGValue(rgb);
			B = (double)(int)GetBValue(rgb);
			r += ddf * R;
			g += ddf * G;
			b += ddf * B;
			f += ddf;
#ifdef _DEBUG_PIXELSQUEEZE
			r0 += R;
			g0 += G;
			b0 += B;
			n++;
#endif//#ifdef _DEBUG_PIXELSQUEEZE
		}
		// save the horizontal index of pixel 'B' which is adjacent to pixel 'c'.
		x1 = x + 1;

		// work on pixel 'd'. it forms the destination's right-bottom corner.
		j++;
		// the vertical (y) does not change. 
		rx = _ratioColumnIndexToImageWidth(j, rcDest);
		// here is the right edge of pixel 'd'.
		fx = (double)_srcSz.cx * rx;
		// round down. it's the left edge of pixel 'd'.
		x = (int)fx;
		// isolate the fraction. it's how much the destination sticks out to the right of pixels 'B'.
		dfx = fx - (double)x;
		// watch out. pixel 'd' may not contribute after all.
		if (dfy > 0 && dfx > 0)
		{
			// get the contributing area.
			ddf = dfy * dfx;
			// pick the color from pixel 'd'.
			rgb = getPixel(dataSrc, x, y);
			// add it to the averaging sum.
			R = (double)(int)GetRValue(rgb);
			G = (double)(int)GetGValue(rgb);
			B = (double)(int)GetBValue(rgb);
			r += ddf * R;
			g += ddf * G;
			b += ddf * B;
			f += ddf;
#ifdef _DEBUG_PIXELSQUEEZE
			r0 += R;
			g0 += G;
			b0 += B;
			n++;
#endif//#ifdef _DEBUG_PIXELSQUEEZE
		}
		// keep the right-top coordinates of right-most pixel 'B'.
		x2 = x;
		y2 = y;

		if (y < _srcSz.cy)
		{
			// work on pixels 'B'. there can be one or more of them.
			for (j = x1; j < x2; j++)
			{
				// the width is unity. the area only depends on contributing height of pixel B.
				ddf = dfy;
				// pick the color from pixel B.
				rgb = getPixel(dataSrc, j, y);
				// add it to the averaging sum.
				R = (double)(int)GetRValue(rgb);
				G = (double)(int)GetGValue(rgb);
				B = (double)(int)GetBValue(rgb);
				r += ddf * R;
				g += ddf * G;
				b += ddf * B;
				f += ddf;
#ifdef _DEBUG_PIXELSQUEEZE
				r0 += R;
				g0 += G;
				b0 += B;
				n++;
#endif//#ifdef _DEBUG_PIXELSQUEEZE
			}
		}

		// finally, work on pixels 'E'. each contributes to the destination pixel 100%.
		for (i = y1; i < y2; i++)
		{
			for (j = x1; j < x2; j++)
			{
				// pick the color from next pixel 'E'.
				rgb = getPixel(dataSrc, j, i);
				// add it to the averaging sum.
				R = (double)(int)GetRValue(rgb);
				G = (double)(int)GetGValue(rgb);
				B = (double)(int)GetBValue(rgb);
				r += R;
				g += G;
				b += B;
				f += 1.0;
#ifdef _DEBUG_PIXELSQUEEZE
				r0 += R;
				g0 += G;
				b0 += B;
				n++;
#endif//#ifdef _DEBUG_PIXELSQUEEZE
			}
		}

		// get the average color value for destination pixel, and we're done!
		r /= f;
		g /= f;
		b /= f;
		rgb = RGB(lrint(r), lrint(g), lrint(b));

#ifdef _DEBUG_PIXELSQUEEZE
		r0 /= n;
		g0 /= n;
		b0 /= n;
		COLORREF rgb0 = RGB(lrint(r0), lrint(g0), lrint(b0));
		DBGPRINTF(("Squeeze: (%d:%d)-(%d:%d); RGB=%06X/%06X; %.4g/%.4g/%.4g @ %.2g/%d\n", x1, y1, x2, y2, rgb, rgb0, r, g, b, f, n));
#endif//#ifdef _DEBUG_PIXELSQUEEZE

		return rgb;
	}

protected:
	double _ratioRowIndexToImageHeight(int i, RECT *rcDest)
	{
		//return (double)(i - rcDest->top) / (double)pbmiDest->bmiHeader.biHeight;
		return (double)(i - rcDest->top) / (double)(rcDest->bottom - rcDest->top);
	}

	double _ratioColumnIndexToImageWidth(int j, RECT *rcDest)
	{
		//(double)(j - rcDest->left) / (double)pbmiDest->bmiHeader.biWidth;
		return (double)(j - rcDest->left) / (double)(rcDest->right - rcDest->left);
	}

};

class ScaledBitmapPixels24bpp : public ScaledBitmapPixels32bpp
{
public:
	ScaledBitmapPixels24bpp(SIZE s, SIZE d) : ScaledBitmapPixels32bpp(s,d) {}

	virtual COLORREF getPixel(LPVOID dataSrc, int x, int y)
	{
		int i = x + y * _srcSz.cx;
		RGBTRIPLE *p = (RGBTRIPLE *)dataSrc + i;
		COLORREF c = 0xFFFFFFFF;
		memcpy(&c, p, sizeof(RGBTRIPLE));
		/*
		RGBQUAD *p2 = (RGBQUAD *)&c;
		p2->rgbBlue = p->rgbtBlue;
		p2->rgbGreen = p->rgbtGreen;
		p2->rgbRed = p->rgbtRed;
		*/
		return c;
	}
	virtual void setPixel(LPVOID dataDest, int x, int y, COLORREF rgb)
	{
		int i = x + y * _destSz.cx;
		RGBTRIPLE *p = (RGBTRIPLE *)dataDest + i;
		memcpy(p, &rgb, sizeof(RGBTRIPLE));
		/*
		RGBQUAD *p1 = (RGBQUAD *)&rgb;
		p->rgbtBlue = p1->rgbBlue;
		p->rgbtGreen = p1->rgbGreen;
		p->rgbtRed = p1->rgbRed;
		*/
	}

protected:
};

class ScaledBitmapPixels8bpp : public ScaledBitmapPixels24bpp
{
public:
	ScaledBitmapPixels8bpp(SIZE s, SIZE d, BITMAPINFO *b) : ScaledBitmapPixels24bpp(s, d), _bmiSrc(b) {
		ASSERT(b->bmiHeader.biBitCount == 8);
	}

	virtual COLORREF getPixel(LPVOID dataSrc, int x, int y)
	{
		int s = SCANLINELENGTH_8BPP(_srcSz.cx);
		int z = x + y * s;
		short i = MAKEWORD(*((LPBYTE)dataSrc + z), 0);
		return *(COLORREF*)(_bmiSrc->bmiColors+i);
	}

protected:
	BITMAPINFO *_bmiSrc;
};

class ScaledBitmapPixels1bpp : public ScaledBitmapPixels24bpp
{
public:
	ScaledBitmapPixels1bpp(SIZE s, SIZE d, BITMAPINFO *b) : ScaledBitmapPixels24bpp(s, d), _bmiSrc(b) {
		ASSERT(b->bmiHeader.biBitCount == 1);
		ASSERT(b->bmiHeader.biClrUsed == 2);
	}

	virtual COLORREF getPixel(LPVOID dataSrc, int x, int y)
	{
		int s = SCANLINELENGTH_1BPP(_srcSz.cx);

		// get the color byte index.
		int k = x / 8;
		// get the intra-byte bit index.
		int q = x % 8;
		// lowest of bit indices 0-7 corresponds to highest mask bit 0x80.
		int qmask = 0x80 >> q;
		// read the color index.
		int z = k + y * s;
		short i = MAKEWORD(*((LPBYTE)dataSrc + z), 0);
		// apply the mask to isolate the pixel bit. the color index can only be 1 or 0.
		i = (i & qmask) ? 1 : 0;
		// load the color from the palette.
		return *(COLORREF*)(_bmiSrc->bmiColors + i);
	}

protected:
	BITMAPINFO *_bmiSrc;
};


class ScaledBitmapBaseImpl
{
public:
	ScaledBitmapBaseImpl(HANDLE k_) : _kill(k_), _hdest(NULL), _hsrc(NULL), _forceTopdown(false), _srcSz{ 0 }, _destSz{ 0 }, _destOrig{ 0 }, _srcData(NULL), _bmiSrc(NULL), _bmiLen(0) {}
	~ScaledBitmapBaseImpl()
	{
		if (_bmiSrc)
			free(_bmiSrc);
		if (_hsrc)
			DeleteObject(_hsrc);
		if (_hdest)
			DeleteObject(_hdest);
	}

protected:
	HANDLE _kill;
	HBITMAP _hsrc, _hdest; // handles to the source and destination bitmaps.
	SIZE _srcSz, _destSz; // width and height pairs of the source and destination bitmaps.
	POINT _destOrig; // destination origin; x- and y-coordinates of the upper left corner of the scaled bitmap in the destination area.
	BITMAPINFO *_bmiSrc; // a header describing the bitmap of handle _hsrc.
	int _bmiLen;
	LPBYTE _srcData; // associated with the bitmap of handle _hsrc; will be automatically freed when _hsrc is deleted.
	bool _forceTopdown;

	HBITMAP _copyBitmap(HDC hdc, HBITMAP hbmp, BITMAP *bm, UINT *crcSrc = NULL)
	{
		_bmiLen = sizeof(BITMAPINFO);
		if(bm->bmBitsPixel < 24)
			_bmiLen += sizeof(COLORREF) * MAX_8BPP_RGB_COLORS;
		LPBITMAPINFO pbmi2 = (LPBITMAPINFO)malloc(_bmiLen);
		ZeroMemory(pbmi2, _bmiLen);
		pbmi2->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		HBITMAP hbmp0 = (HBITMAP)SelectObject(hdc, hbmp);
		int res = GetDIBits(hdc, hbmp, 0, 1, NULL, pbmi2, DIB_RGB_COLORS);
		if (res == 0)
			return NULL;
		if (pbmi2->bmiHeader.biClrUsed)
		{
			res = GetDIBColorTable(hdc, 0, pbmi2->bmiHeader.biClrUsed, pbmi2->bmiColors);
		}
		SelectObject(hdc, hbmp0);

		if (_forceTopdown)
			pbmi2->bmiHeader.biHeight = -bm->bmHeight;

		if (pbmi2->bmiHeader.biCompression != BI_RGB) // BI_BITFIELDS (3), BI_JPEG (4) or BI_PNG (5)
			pbmi2->bmiHeader.biCompression = BI_RGB;

		HBITMAP hbmp2 = CreateDIBSection(hdc, pbmi2, DIB_RGB_COLORS, (LPVOID*)&_srcData, NULL, 0);
		if (hbmp2)
		{
			if (bm->bmBits)
			{
				HDC hdc2 = CreateCompatibleDC(hdc);
				HBITMAP hbmp20 = (HBITMAP)SelectObject(hdc2, hbmp2);
				memcpy(_srcData, bm->bmBits, pbmi2->bmiHeader.biSizeImage);
				SetDIBits(hdc2, hbmp2, 0, bm->bmHeight, _srcData, pbmi2, DIB_RGB_COLORS);
				SelectObject(hdc2, hbmp20);
				DeleteDC(hdc2);
			}
			else
			{
				HDC hdc1 = CreateCompatibleDC(hdc);
				HDC hdc2 = CreateCompatibleDC(hdc);
				HBITMAP hbmp10 = (HBITMAP)SelectObject(hdc1, hbmp);
				HBITMAP hbmp20 = (HBITMAP)SelectObject(hdc2, hbmp2);
				BitBlt(hdc2, 0, 0, _srcSz.cx, _srcSz.cy, hdc1, 0, 0, SRCCOPY);
				SelectObject(hdc2, hbmp20);
				SelectObject(hdc1, hbmp10);
				DeleteDC(hdc2);
				DeleteDC(hdc1);
			}

			if (crcSrc)
			{
				CRC32 crc;
				crc.feedData(_srcData, pbmi2->bmiHeader.biSizeImage);
				*crcSrc = crc.value();
			}

			_bmiSrc = pbmi2;
			pbmi2 = NULL;
		}

		if (pbmi2)
			free(pbmi2);

		return hbmp2;
	}

	DWORD _rescaleBitmap(HDC hdc)
	{
		DWORD ret = ERROR_SUCCESS;

		LPBITMAPINFO pbmi = (LPBITMAPINFO)malloc(_bmiLen);
		memcpy(pbmi, _bmiSrc, _bmiLen);

		pbmi->bmiHeader.biWidth = _destSz.cx;
		pbmi->bmiHeader.biHeight = _forceTopdown? -_destSz.cy : _destSz.cy;
		pbmi->bmiHeader.biSizeImage = _destSz.cy;
		if (pbmi->bmiHeader.biBitCount == 32)
			pbmi->bmiHeader.biSizeImage *= SCANLINELENGTH_32BPP(_destSz.cx);
		else if (pbmi->bmiHeader.biBitCount == 24)
			pbmi->bmiHeader.biSizeImage *= SCANLINELENGTH_24BPP(_destSz.cx);
		else
		{
			pbmi->bmiHeader.biBitCount = 24;
			pbmi->bmiHeader.biClrUsed = pbmi->bmiHeader.biClrImportant = 0;
			pbmi->bmiHeader.biSizeImage *= SCANLINELENGTH_24BPP(_destSz.cx);
		}

		HDC hdc1 = CreateCompatibleDC(hdc);
		HBITMAP hbmp10 = (HBITMAP)SelectObject(hdc1, _hsrc);

		LPBYTE destData;
		HBITMAP hbmp2 = CreateDIBSection(hdc, pbmi, DIB_RGB_COLORS, (LPVOID*)&destData, NULL, 0);
		if (hbmp2)
		{
			// the bitmap allocated in destData will remain valid until DeleteObject is called on hbmp2 (or _hdest).
			if (_squeezeBitmap(destData, _srcData))
			{
				HDC hdc2 = CreateCompatibleDC(hdc);
				HBITMAP hbmp20 = (HBITMAP)SelectObject(hdc2, hbmp2);
				_hdest = hbmp2;
				SelectObject(hdc2, hbmp20);
				DeleteDC(hdc2);
				SB_SAVEBMP(_hdest, BMP_SAVE_PATH3, _forceTopdown, NULL);
				ret = ERROR_SUCCESS;
			}
			else
				ret = ERROR_CANCELLED;
		}
		else
			ret = ERROR_OUTOFMEMORY;
		SelectObject(hdc1, hbmp10);
		DeleteDC(hdc1);

		free(pbmi);
		return ret;
	}

	virtual bool _squeezeBitmap(LPVOID dataDest, LPVOID dataSrc)
	{
		DBGPRINTF((L"SqueezeBitmap starts: %d bpp; Dest={%d, %d, %d, %d}, Src={%d, %d}\n", _bmiSrc->bmiHeader.biBitCount, _destOrig.x, _destOrig.y, _destSz.cx, _destSz.cy, _srcSz.cx, _srcSz.cy));
		COLORREF rgb;
		RECT rcDest = { _destOrig.x,_destOrig.y,_destOrig.x + _destSz.cx,_destOrig.y + _destSz.cy };
		ScaledBitmapPixels32bpp *sbp = NULL;
		if (_bmiSrc->bmiHeader.biBitCount == 32)
			sbp = new ScaledBitmapPixels32bpp(_srcSz, _destSz);
		else if (_bmiSrc->bmiHeader.biBitCount == 24)
			sbp = new ScaledBitmapPixels24bpp(_srcSz, _destSz);
		else if (_bmiSrc->bmiHeader.biBitCount == 8)
			sbp = new ScaledBitmapPixels8bpp(_srcSz, _destSz, _bmiSrc);
		else if (_bmiSrc->bmiHeader.biBitCount == 1)
			sbp = new ScaledBitmapPixels1bpp(_srcSz, _destSz, _bmiSrc);
		else
			return false;

		int i, j;
		for (i = 0; i < _destSz.cy; i++)
		{
			if (i < rcDest.top || i > rcDest.bottom)
				continue;
			if (_kill && WaitForSingleObject(_kill, 0) != WAIT_TIMEOUT)
			{
				DBGPRINTF((L"SqueezeBitmap aborted (%d/%d)\n", i, _destSz.cy));
				return false;
			}
			for (j = 0; j < _destSz.cx; j++)
			{
				if (j < rcDest.left || j > rcDest.right)
					continue;
				rgb = sbp->squeeze(j, i, &rcDest, dataSrc);
				sbp->setPixel(dataDest, j, i, rgb);
			}
		}
		delete sbp;
		DBGPUTS((L"SqueezeBitmap stops"));
		return true;
	}

};


/* ScaledBitmap - reduces the size of a source bitmap. Can block if the source image is large as it performs the scaling operation synchornously in the calling thread.

here is sample code for scaling down a bitmap image from source size sizeSrc to reduced size sizeDest.

	HBITMAP hbmDest = NULL;
	ScaledBitmap sbmp;
	if (sbmp.SetSourceBitmap(hdc, hbmSrc, sizeSrc))
	{
		hbmDest = sbmp.GetScaledBitmap(sizeDest);
	}
*/
class ScaledBitmap : public ScaledBitmapBaseImpl
{
public:
	ScaledBitmap(HANDLE hKill = NULL) : ScaledBitmapBaseImpl(hKill), _hdc(NULL), _releaseDC(false) {}
	~ScaledBitmap()
	{
		if (_releaseDC)
			ReleaseDC(NULL, _hdc);
	}

	bool SetSourceBitmap(HDC hdc, HBITMAP hbmp, BITMAP *bm, bool forceTopdown)
	{
		_forceTopdown = forceTopdown;
		_srcSz = {bm->bmWidth, bm->bmHeight};
		if (!hdc)
		{
			_hdc = GetDC(NULL);
			_releaseDC = true;
		}
		else
			_hdc = hdc;
		if ((_hsrc = _copyBitmap(hdc, hbmp, bm)))
			return true;
		return false;
	}
	HBITMAP GetScaledBitmap(SIZE destSize)
	{
		_destSz = destSize;
		if (ERROR_SUCCESS == _rescaleBitmap(_hdc))
		{
			return (HBITMAP)InterlockedExchangePointer((LPVOID*)&_hdest, NULL);
		}
		return NULL;
	}

protected:
	HDC _hdc; // a handle to a device context where both the source and destination (rescaled) bitmaps are rendered.
	bool _releaseDC; // set to true if the ScaledBitmap owns the DC handle in _hdc and the class destructor will release the handle.
};

