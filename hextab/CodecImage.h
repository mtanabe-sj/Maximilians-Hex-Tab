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
#include <wincodec.h>

// api reference:
// https://docs.microsoft.com/en-us/windows/win32/api/wincodec/nf-wincodec-wicconvertbitmapsource

// add this to the linker input list.
// windowscodecs.lib


class CodecImage
{
public:
	CodecImage()
	{
		_wbsrc = NULL;
	}
	~CodecImage()
	{
		Free();
	}
	IWICBitmapSource *_wbsrc;

	HRESULT Load(LPSTREAM ps)
	{
		Free();
		IWICBitmapDecoder * decoder;
		HRESULT hr = createDecoder(&decoder);
		if (hr == S_OK)
		{
			hr = decoder->Initialize(ps, WICDecodeMetadataCacheOnLoad);
			if (hr == S_OK)
			{
				IWICBitmapFrameDecode *frame;
				hr = decoder->GetFrame(0, &frame); // only interested in frame 0
				if (hr == S_OK)
				{
					hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, frame, &_wbsrc);
					frame->Release();
				}
			}
			decoder->Release();
		}
		return hr;
	}

	HRESULT Save(LPSTREAM ps)
	{
		if (!_wbsrc) // make sure SetSourceBitmap() is used to supply source bitmap data.
			return E_POINTER;
		IWICBitmapEncoder *encoder;
		HRESULT hr = createEncoder(&encoder);
		if (hr == S_OK)
		{
			hr = encoder->Initialize(ps, WICBitmapEncoderNoCache);
			if (hr == S_OK)
			{
				IWICBitmapFrameEncode *frame;
				hr = encoder->CreateNewFrame(&frame, NULL);
				if (hr == S_OK)
				{
					hr = frame->Initialize(NULL);
					if (hr == S_OK)
					{
						GUID pixguid = GUID_WICPixelFormat32bppBGRA;
						UINT cx, cy;
						_wbsrc->GetSize(&cx, &cy);
						hr = frame->SetSize(cx, cy);
						if (hr == S_OK)
							hr = frame->SetPixelFormat(&pixguid);
						if (hr == S_OK)
						{
							hr = frame->WriteSource(_wbsrc, NULL);
							if (hr == S_OK)
							{
								hr = frame->Commit();
								if (hr == S_OK)
									encoder->Commit();
							}
						}
					}
					frame->Release();
				}
			}
			encoder->Release();
		}
		return hr;
	}

	HRESULT SetSourceBitmap(LPSTREAM psBmp)
	{
		Free();
		//return E_NOTIMPL;
		IWICBitmapDecoder * decoder;
		HRESULT hr = CoCreateInstance(CLSID_WICBmpDecoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(decoder), reinterpret_cast<void**>(&decoder));
		if (hr == S_OK)
		{
			hr = decoder->Initialize(psBmp, WICDecodeMetadataCacheOnLoad);
			if (hr == S_OK)
			{
				IWICBitmapFrameDecode *frame;
				hr = decoder->GetFrame(0, &frame); // only interested in frame 0
				if (hr == S_OK)
				{
					hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppBGRA, frame, &_wbsrc); //GUID_WICPixelFormat32bppBGR
					frame->Release();
				}
			}
			decoder->Release();
		}
		return hr;
	}

	HRESULT CreateBitmap(HDC hdc, HBITMAP *phbmp, bool topdownScan=true)
	{
		if (!_wbsrc)
			return E_POINTER;
		UINT cx, cy;
		HRESULT hr = _wbsrc->GetSize(&cx, &cy);
		if (hr != S_OK)
			return hr;
		if (cx == 0 || cy == 0)
			return E_UNEXPECTED;

		ASSERT((LONG)cy > 0);
		BITMAPINFO bmi = { 0 };
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = cx;
		bmi.bmiHeader.biHeight = topdownScan? -((LONG)cy) : cy;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32; // 32-bit color depth
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biSizeImage = cx * cy * sizeof(RGBQUAD);
		DBGPRINTF((L"CodecImage::CreateBitmap: %d, %d (%d); SizeImage=%d\n", cx, cy, bmi.bmiHeader.biHeight, bmi.bmiHeader.biSizeImage));

		LPBYTE bits;
		HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, 0);
		if (!hbmp)
			return E_FAIL;

		hr = copyPixels(cx, cy, bits);
		if (hr == S_OK)
			*phbmp = hbmp;
		else
			DeleteObject(hbmp);
		return hr;
	}

protected:
	void Free()
	{
		if (_wbsrc)
		{
			_wbsrc->Release();
			_wbsrc = NULL;
		}
	}

	virtual HRESULT createDecoder(IWICBitmapDecoder ** decoder) { return E_NOTIMPL; }
	virtual HRESULT createEncoder(IWICBitmapEncoder **encoder) { return E_NOTIMPL; }
	virtual HRESULT copyPixels(UINT cx, UINT cy, LPBYTE bits)
	{
		UINT stride = sizeof(RGBQUAD) * cx; // 4 bytes per pixel (since it's 32bpp)
		return _wbsrc->CopyPixels(NULL, stride, stride*cy, bits);
	}
};


class CodecBMP : public CodecImage
{
public:
	CodecBMP() {}

protected:
	virtual HRESULT createDecoder(IWICBitmapDecoder ** decoder)
	{
		return CoCreateInstance(CLSID_WICBmpDecoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(*decoder), reinterpret_cast<void**>(decoder));
	}
	virtual HRESULT createEncoder(IWICBitmapEncoder **encoder)
	{
		return CoCreateInstance(CLSID_WICBmpEncoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(*encoder), reinterpret_cast<void**>(encoder));
	}
};


class CodecPNG : public CodecImage
{
public:
	CodecPNG(int cf=2) : _colorType(cf) {}
	int _colorType; // PNG_COLOR_TYPE_GRAY (0), PNG_COLOR_TYPE_RGB (2), PNG_COLOR_TYPE_GRAY_ALPHA (4), PNG_COLOR_TYPE_RGB_ALPHA (6)

protected:
	virtual HRESULT createDecoder(IWICBitmapDecoder ** decoder)
	{
		return CoCreateInstance(CLSID_WICPngDecoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(*decoder), reinterpret_cast<void**>(decoder));
	}
	virtual HRESULT createEncoder(IWICBitmapEncoder **encoder)
	{
		return CoCreateInstance(CLSID_WICPngEncoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(*encoder), reinterpret_cast<void**>(encoder));
	}
	virtual HRESULT copyPixels(UINT cx, UINT cy, LPBYTE dest)
	{
		if (_colorType == 4) // PNG_COLOR_TYPE_GRAY_ALPHA
		{
			UINT stride = sizeof(RGBQUAD) * cx; // 4 bytes per pixel (since it's 32bpp)
			UINT imgLen = stride * cy;
			LPBYTE src = (LPBYTE)malloc(imgLen);
			HRESULT hr = _wbsrc->CopyPixels(NULL, stride, imgLen, src);
			if (hr == S_OK)
			{
				RGBQUAD *s = (RGBQUAD*)src;
				RGBQUAD *d = (RGBQUAD*)dest;
				for (UINT i = 0; i < cy; i++)
				{
					for (UINT j = 0; j < cx; j++)
					{
						RGBQUAD rgb = *s++;
						rgb.rgbRed = rgb.rgbGreen = rgb.rgbBlue = rgb.rgbReserved ^ 0xff;
						*d++ = rgb;
					}
				}

			}
			free(src);
			return hr;
		}
		return CodecImage::copyPixels(cx, cy, dest);
	}
};


class CodecJPEG : public CodecImage
{
public:
	CodecJPEG() {}

protected:
	virtual HRESULT createDecoder(IWICBitmapDecoder ** decoder)
	{
		return CoCreateInstance(CLSID_WICJpegDecoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(*decoder), reinterpret_cast<void**>(decoder));
	}
	virtual HRESULT createEncoder(IWICBitmapEncoder **encoder)
	{
		return CoCreateInstance(CLSID_WICJpegEncoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(*encoder), reinterpret_cast<void**>(encoder));
	}
};


class CodecGIF : public CodecImage
{
public:
	CodecGIF() {}

protected:
	virtual HRESULT createDecoder(IWICBitmapDecoder ** decoder)
	{
		return CoCreateInstance(CLSID_WICGifDecoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(*decoder), reinterpret_cast<void**>(decoder));
	}
	virtual HRESULT createEncoder(IWICBitmapEncoder **encoder)
	{
		return CoCreateInstance(CLSID_WICGifEncoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(*encoder), reinterpret_cast<void**>(encoder));
	}
};

/*
class CodecPNG
{
public:
	CodecPNG()
	{
		_wbsrc = NULL;
	}
	~CodecPNG()
	{
		Free();
	}
	IWICBitmapSource *_wbsrc;

	HRESULT Load(LPSTREAM ps)
	{
		Free();
		IWICBitmapDecoder * decoder;
		HRESULT hr = CoCreateInstance(CLSID_WICPngDecoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(decoder), reinterpret_cast<void**>(&decoder));
		if (hr == S_OK)
		{
			hr = decoder->Initialize(ps, WICDecodeMetadataCacheOnLoad);
			if (hr == S_OK)
			{
				IWICBitmapFrameDecode *frame;
				hr = decoder->GetFrame(0, &frame); // only interested in frame 0
				if (hr == S_OK)
				{
					hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, frame, &_wbsrc);
					frame->Release();
				}
			}
			decoder->Release();
		}
		return hr;
	}

	HRESULT Save(LPSTREAM ps)
	{
		if (!_wbsrc) // make sure SetSourceBitmap() is used to supply source bitmap data.
			return E_POINTER;
		IWICBitmapEncoder *encoder;
		HRESULT hr = CoCreateInstance(CLSID_WICPngEncoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(encoder), reinterpret_cast<void**>(&encoder));
		if (hr == S_OK)
		{
			hr = encoder->Initialize(ps, WICBitmapEncoderNoCache);
			if (hr == S_OK)
			{
				IWICBitmapFrameEncode *frame;
				hr = encoder->CreateNewFrame(&frame, NULL);
				if (hr == S_OK)
				{
					hr = frame->Initialize(NULL);
					if (hr == S_OK)
					{
						GUID pixguid = GUID_WICPixelFormat32bppBGRA;
						UINT cx, cy;
						_wbsrc->GetSize(&cx, &cy);
						hr = frame->SetSize(cx, cy);
						if (hr == S_OK)
							hr = frame->SetPixelFormat(&pixguid);
						if (hr == S_OK)
						{
							hr = frame->WriteSource(_wbsrc, NULL);
							if (hr == S_OK)
							{
								hr = frame->Commit();
								if (hr == S_OK)
									encoder->Commit();
							}
						}
					}
					frame->Release();
				}
			}
			encoder->Release();
		}
		return hr;
	}

	HRESULT SetSourceBitmap(LPSTREAM psBmp)
	{
		Free();
		//return E_NOTIMPL;
		IWICBitmapDecoder * decoder;
		HRESULT hr = CoCreateInstance(CLSID_WICBmpDecoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(decoder), reinterpret_cast<void**>(&decoder));
		if (hr == S_OK)
		{
			hr = decoder->Initialize(psBmp, WICDecodeMetadataCacheOnLoad);
			if (hr == S_OK)
			{
				IWICBitmapFrameDecode *frame;
				hr = decoder->GetFrame(0, &frame); // only interested in frame 0
				if (hr == S_OK)
				{
					hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppBGRA, frame, &_wbsrc); //GUID_WICPixelFormat32bppBGR
					frame->Release();
				}
			}
			decoder->Release();
		}
		return hr;
	}

	HRESULT CreateBitmap(HDC hdc, HBITMAP *phbmp)
	{
		if (!_wbsrc)
			return E_POINTER;
		UINT cx, cy;
		HRESULT hr = _wbsrc->GetSize(&cx, &cy);
		if (hr != S_OK)
			return hr;
		if (cx == 0 || cy == 0)
			return E_UNEXPECTED;

		BITMAPINFO bmi = { 0 };
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = cx;
		bmi.bmiHeader.biHeight = -((LONG)cy);
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32; // 32-bit resolution
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biSizeImage = cx * cy * 4;

		LPBYTE bits;
		HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, 0);
		if (!hbmp)
			return E_FAIL;

		UINT stride = 4 * cx; // 4 bytes per pixel (since it's 32-bit resolution)
		hr = _wbsrc->CopyPixels(NULL, stride, stride*cy, bits);
		if (hr == S_OK)
			*phbmp = hbmp;
		else
			DeleteObject(hbmp);
		return hr;
	}

	void Free()
	{
		if (_wbsrc)
		{
			_wbsrc->Release();
			_wbsrc = NULL;
		}
	}
};
*/
