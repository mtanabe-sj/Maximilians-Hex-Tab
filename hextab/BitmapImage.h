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
#include "CodecImage.h"


/* BitmapImage - saves an bitmap image of png or bmp in a file.
*/
class BitmapImage
{
public:
	BitmapImage(HBITMAP hbm) : _hbm(hbm) {}
	~BitmapImage() {}

	HRESULT SaveAs(LPCWSTR filename)
	{
		HRESULT hr = S_OK;
		if (_IsFileType(filename, L".png"))
			return SaveImageInPng(filename);
		return SaveImageInBmp(filename);
	}

protected:
	HBITMAP _hbm;

	// a helper for comparing file extension names
	BOOL _IsFileType(LPCWSTR filePath, LPCWSTR fileExtension)
	{
		LPCWSTR test = wcsrchr(filePath, '.');
		if (test)
		{
			if (0 == lstrcmpi(test, fileExtension))
				return TRUE;
		}
		return FALSE;
	}

	HRESULT SavePictdescAsFile(PICTDESC *pictdesc, LPCWSTR filePath)
	{
		HRESULT hr;
		IPicture* pict;
		hr = OleCreatePictureIndirect(pictdesc, IID_IPicture, FALSE, (LPVOID*)&pict);
		if (hr == S_OK) {
			LPSTREAM pictstream;
			hr = CreateStreamOnHGlobal(NULL, TRUE, &pictstream);
			if (hr == S_OK) {
				long cb = 0;
				hr = pict->SaveAsFile(pictstream, TRUE, &cb);
				if (hr == S_OK) {
					hr = pictstream->Commit(0);
					HGLOBAL hmem;
					hr = GetHGlobalFromStream(pictstream, &hmem);
					if (hr == S_OK) {
						LPBYTE pictdata = (LPBYTE)GlobalLock(hmem);
						HANDLE hf = CreateFile(filePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
						if (hf != INVALID_HANDLE_VALUE) {
							ULONG cb2;
							if (WriteFile(hf, pictdata, cb, &cb2, NULL))
								hr = S_OK;
							else
								hr = HRESULT_FROM_WIN32(GetLastError());
							CloseHandle(hf);
						}
						else
							hr = HRESULT_FROM_WIN32(GetLastError());
						GlobalUnlock(hmem);
					}
				}
				pictstream->Release();
			}
			pict->Release();
		}
		return hr;
	}
	HRESULT SavePictdescAsStream(PICTDESC *pictdesc, LPSTREAM *pps)
	{
		HRESULT hr;
		IPicture* pict;
		hr = OleCreatePictureIndirect(pictdesc, IID_IPicture, FALSE, (LPVOID*)&pict);
		if (hr == S_OK) {
			LPSTREAM pictstream;
			hr = CreateStreamOnHGlobal(NULL, TRUE, &pictstream);
			if (hr == S_OK) {
				long cb = 0;
				hr = pict->SaveAsFile(pictstream, TRUE, &cb);
				if (hr == S_OK) {
					hr = pictstream->Commit(0);
					if (hr == S_OK)
						*pps = pictstream;
				}
				if (hr != S_OK)
					pictstream->Release();
			}
			pict->Release();
		}
		return hr;
	}

	HRESULT SaveImageInBmp(LPCWSTR filePath)
	{
		if (!_hbm)
			return E_UNEXPECTED;
		PICTDESC pictdesc = { sizeof(PICTDESC), PICTYPE_BITMAP };
		pictdesc.bmp.hbitmap = _hbm;
		return SavePictdescAsFile(&pictdesc, filePath);
	}
	HRESULT SaveImageInPng(LPCWSTR filePath)
	{
		if (!_hbm)
			return E_UNEXPECTED;
		PICTDESC pictdesc = { sizeof(PICTDESC), PICTYPE_BITMAP };
		pictdesc.bmp.hbitmap = _hbm;
		LPSTREAM psBmp;
		HRESULT hr = SavePictdescAsStream(&pictdesc, &psBmp);
		if (hr == S_OK)
		{
			CodecPNG png;
			hr = png.SetSourceBitmap(psBmp);
			if (hr == S_OK)
			{
				LPSTREAM psPng;
				hr = CreateStreamOnHGlobal(NULL, TRUE, &psPng);
				if (hr == S_OK)
				{
					hr = png.Save(psPng);
					if (hr == S_OK)
					{
						hr = psPng->Commit(0);
						if (hr == S_OK)
						{
							STATSTG stg = { 0 };
							hr = psPng->Stat(&stg, STATFLAG_NONAME);
							if (hr == S_OK)
							{
								HGLOBAL hmem;
								hr = GetHGlobalFromStream(psPng, &hmem);
								if (hr == S_OK)
								{
									LPBYTE pictdata = (LPBYTE)GlobalLock(hmem);
									HANDLE hf = CreateFile(filePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
									if (hf != INVALID_HANDLE_VALUE) {
										ULONG cb2;
										if (WriteFile(hf, pictdata, stg.cbSize.LowPart, &cb2, NULL))
											hr = S_OK;
										else
											hr = HRESULT_FROM_WIN32(GetLastError());
										CloseHandle(hf);
									}
									else
										hr = HRESULT_FROM_WIN32(GetLastError());
									GlobalUnlock(hmem);
								}
							}
						}
					}
					psPng->Release();
				}
			}
			psBmp->Release();
		}
		return hr;
	}
};

