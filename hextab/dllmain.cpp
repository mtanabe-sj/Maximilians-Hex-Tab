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
// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <initguid.h>
#include "PropsheetExtImpl.h"
#include "TLSData.h"
#include "resource.h"
#include "libver.h"
#include "CodecImage.h"
#include "ScaledBitmap.h"


#define REFITBMP_USES_SQUEEZE_HELPER
#define REFITBMP_IMG_TO_VIEWFRAME_RATIO 0.5
#define REFITBMP_IN_VIEWFRAME_IMG_WIDTH 200


ULONG LibRefCount = 0;
ULONG LibLockCount = 0;
HMODULE LibInstanceHandle = NULL;
#ifdef APP_USES_HOTKEYS
TLSData *TLSData::TLSD = NULL;
#endif//#ifdef APP_USES_HOTKEYS

BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		LibInstanceHandle = hModule;
#ifdef APP_USES_HOTKEYS
		TLSData::instance();
#endif//#ifdef APP_USES_HOTKEYS
		break;
	case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
		break;
    case DLL_PROCESS_DETACH:
#ifdef APP_USES_HOTKEYS
		delete TLSData::instance();
#endif//#ifdef APP_USES_HOTKEYS
		break;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// Special entry points required for inproc servers

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
	*ppv = NULL;
	IClassFactory *pClassFactory;
	// propertysheet handlers
	if (IsEqualCLSID(rclsid, CLSID_MyShellPropsheetExt))
		pClassFactory = new IClassFactoryNoAggrImpl<PropsheetExtImpl>;
	else
		return CLASS_E_CLASSNOTAVAILABLE;
	if (pClassFactory == NULL)
		return E_OUTOFMEMORY;
	HRESULT hr = pClassFactory->QueryInterface(riid, ppv);
	pClassFactory->Release();
	return hr;
}

STDAPI DllCanUnloadNow(void)
{
	return (LibRefCount == 0 && LibLockCount == 0) ? S_OK : S_FALSE;
}

DWORD DllGetVersion(void)
{
	return MAKELONG(1, 0);
}

STDAPI DllRegisterServer(void)
{
	long res;
	res = Registry_CreateComClsidKey(WSZ_CLSID_MyShellPropsheetExt, WSZ_NAME_MyShellPropsheetExt);
	if (res != ERROR_SUCCESS)
		return HRESULT_FROM_WIN32(res);

	res = Registry_CreateNameValue(HKEY_CLASSES_ROOT,
		L"*\\ShellEx\\PropertySheetHandlers\\" WSZ_CLSID_MyShellPropsheetExt,
		NULL,
		WSZ_NAME_MyShellPropsheetExt);
	/*
	res = Registry_CreateNameValue( HKEY_CLASSES_ROOT,
		L"Directory\\ShellEx\\PropertySheetHandlers\\" WSZ_CLSID_MyShellPropsheetExt,
		NULL,
		WSZ_NAME_MyShellPropsheetExt );
	*/

	res = Registry_CreateNameValue(HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
		WSZ_CLSID_MyShellPropsheetExt,
		WSZ_NAME_MyShellPropsheetExt);

	return S_OK;
}

STDAPI DllUnregisterServer(void)
{
	Registry_DeleteValue(HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
		WSZ_CLSID_MyShellPropsheetExt);

	Registry_DeleteSubkey(HKEY_CLASSES_ROOT,
		L"*\\ShellEx\\PropertySheetHandlers",
		WSZ_CLSID_MyShellPropsheetExt);

	Registry_DeleteSubkey(HKEY_CLASSES_ROOT,
		L"CLSID",
		WSZ_CLSID_MyShellPropsheetExt);

	return S_OK;
}

//////////////////////////////////////////////////////////////
HDTIMAGETYPE GetImageTypeFromExtension(LPCWSTR imageFilename)
{
	LPCWSTR sep = wcsrchr(imageFilename, '\\');
	LPCWSTR ext = wcsrchr(imageFilename, '.');
	if (ext)
	{
		if (sep && sep < ext)
		{
			if (0 == _wcsicmp(ext, L".png"))
				return HDTIT_PNG;
			else if (0 == _wcsicmp(ext, L".jpg") || 0 == _wcsicmp(ext, L".jpeg") || 0 == _wcsicmp(ext, L".jif"))
				return HDTIT_JPG;
			else if (0 == _wcsicmp(ext, L".gif"))
				return HDTIT_GIF;
		}
	}
	return HDTIT_BMP;
}

/* 
temp - true to request for the temp work folder in %TMP%. false for the permanent work folder in %LocalAppData%.
*/
LPWSTR GetWorkFolder(LPWSTR buf, int bufSize, bool temp)
{
	int len;
	HRESULT hr = S_FALSE;
	if (!temp)
	{
		// caller wants the work folder in %LocalAppData%.
		// build the base path, "%LocalAppData%\mtanabe".
		LPWSTR dirPath;
		hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &dirPath);
		if (hr == S_OK)
		{
			wcscpy_s(buf, bufSize - 1, dirPath);
			len = (int)wcslen(buf);
			if (len > 0 && buf[len - 1] != '\\')
				wcscat_s(buf, bufSize - 1, L"\\");
			wcscat_s(buf, bufSize - 1, L"mtanabe");
			CoTaskMemFree(dirPath);
		}
	}
	if (hr != S_OK) // caller wants the temp folder.
		GetTempPath(bufSize, buf);
	len = (int)wcslen(buf);
	if (len > 0 && buf[len - 1] != '\\')
		wcscat_s(buf, bufSize - 1, L"\\");
	wcscat_s(buf, bufSize - 1, APP_WORK_FOLDER L"\\");
	return buf;
}

HRESULT EnsureWorkFolder(bool temp)
{
	WCHAR buf[MAX_PATH];
	GetWorkFolder(buf, ARRAYSIZE(buf), temp);
	if (CreateDirectory(buf, NULL))
		return S_OK; // it's created
	DWORD errorCode = GetLastError();
	if (errorCode == ERROR_ALREADY_EXISTS)
		return S_FALSE; // the folder already exists.
	DBGPRINTF((L"EnsureWorkFolder failed: error=%d\n", errorCode));
	return HRESULT_FROM_WIN32(errorCode); // os raised an error.
}

DWORD CleanDirectory(LPCWSTR dirPath, bool deleteDir)
{
	DWORD res;
	ustring4 searchPath(dirPath, L"*.*");
	WIN32_FIND_DATA wfd;
	HANDLE hf = FindFirstFile(searchPath, &wfd);
	if (hf == INVALID_HANDLE_VALUE)
	{
		res = GetLastError();
		DBGPRINTF((L"CleanDirectory: FindFirstFile failed: RES=%d; '%s'\n", res, (LPCWSTR)searchPath));
		return res;
	}
	int deletedCount = 0;
	do
	{
		if (!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			ustring4 nextItem(dirPath, wfd.cFileName);
			if (DeleteFile(nextItem))
			{
				deletedCount++;
			}
			else
			{
				res = GetLastError();
				DBGPRINTF((L"CleanDirectory: DeleteFile failed: RES=%d; '%s'\n", res, wfd.cFileName));
			}
		}
	} while (FindNextFile(hf, &wfd));
	FindClose(hf);
	DBGPRINTF((L"CleanDirectory: deleted %d item(s); '%s'\n", deletedCount, dirPath));
	if (deleteDir)
		RemoveDirectory(dirPath);
	return NOERROR;
}

DWORD CopyFolderContent(LPCWSTR srcDir, LPCWSTR destDir)
{
	DWORD res;
	ustring4 searchPath(srcDir, L"*.*");
	WIN32_FIND_DATA wfd;
	HANDLE hf = FindFirstFile(searchPath, &wfd);
	if (hf == INVALID_HANDLE_VALUE)
	{
		res = GetLastError();
		DBGPRINTF((L"CopyFolderContent: FindFirstFile failed: RES=%d; '%s'\n", res, (LPCWSTR)searchPath));
		return res;
	}
	int movedCount = 0;
	do
	{
		if (!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			ustring4 srcItem(srcDir, wfd.cFileName);
			ustring4 destItem(destDir, wfd.cFileName);
			if (CopyFile(srcItem, destItem, FALSE))
				movedCount++;
			else
			{
				res = GetLastError();
				DBGPRINTF((L"CopyFolderContent: MoveOrCopyFile failed: RES=%d; '%s'\n", res, wfd.cFileName));
			}
		}
	} while (FindNextFile(hf, &wfd));
	FindClose(hf);
	DBGPRINTF((L"CopyFolderContent: moved %d item(s); '%s' -> '%s'\n", movedCount, srcDir, destDir));
	return NOERROR;
}

DWORD MoveOrCopyFile(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, bool canKeepSrcIfCopied)
{
	if (MoveFile(lpExistingFileName, lpNewFileName))
		return NOERROR;
	if (!CopyFile(lpExistingFileName, lpNewFileName, FALSE))
		return GetLastError();
	if (!canKeepSrcIfCopied)
		DeleteFile(lpExistingFileName);
	return NOERROR;
}

HRESULT LoadImageBmp(HWND hwnd, LPCWSTR filename, HDTIMAGETYPE imgType, SIZE *scaledSz, HANDLE hKill, HBITMAP *phbmp)
{
	*phbmp = NULL;
	ULONG cbSize = 0;
	HGLOBAL hg = NULL;
	HRESULT hr = E_FAIL;
	HANDLE hf = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hf != INVALID_HANDLE_VALUE)
	{
		cbSize = GetFileSize(hf, NULL);
		hg = GlobalAlloc(GHND, cbSize);
		if (hg)
		{
			LPBYTE pb = (LPBYTE)GlobalLock(hg);
			ULONG cbRead;
			if (ReadFile(hf, pb, cbSize, &cbRead, NULL))
			{
				hr = S_OK;
				if (hKill)
				{
					if (WaitForSingleObject(hKill, 0) != WAIT_TIMEOUT)
						hr = E_ABORT;
				}
			}
			GlobalUnlock(hg);
		}
		else
			hr = E_OUTOFMEMORY;
		CloseHandle(hf);
	}
	else
		hr = HRESULT_FROM_WIN32(GetLastError());

	if (hr == S_OK)
	{
		LPSTREAM pstream;
		hr = CreateStreamOnHGlobal(hg, TRUE, &pstream);
		if (hr == S_OK)
		{
			CodecImage *codec = NULL;
			if (imgType == HDTIT_JPG)
				codec = new CodecJPEG;
			else if (imgType == HDTIT_PNG)
				codec = new CodecPNG;
			else if (imgType == HDTIT_GIF)
				codec = new CodecGIF;
			else //if (imgType == HDTIT_BMP)
				codec = new CodecBMP;

			hr = codec->Load(pstream);
			if (hr == S_OK)
			{
				HDC hdc = GetDC(hwnd);
				HDC hdc2 = CreateCompatibleDC(hdc);
				hr = codec->CreateBitmap(hdc2, phbmp);
				if (hr == S_OK && scaledSz)
				{
#ifdef _DEBUG
					SaveBmp(*phbmp, BMP_SAVE_PATH5, false, hwnd);
#endif//#ifdef _DEBUG
					HBITMAP hbmp2;
					hbmp2 = _FitBitmapToWindowFrame(*phbmp, hwnd, *scaledSz, hKill, FBTWF_FLAG_RELY_ON_FRAME_HEIGHT);
					if (hbmp2)
					{
						BITMAP bm;
						GetObject(hbmp2, sizeof(BITMAP), &bm);
						scaledSz->cx = bm.bmWidth;
						scaledSz->cy = bm.bmHeight;
						*phbmp = hbmp2;
					}
					else if (hKill)
					{
						DeleteObject(*phbmp);
						*phbmp = NULL;
						hr = E_ABORT;
					}
				}
				DeleteDC(hdc2);
				ReleaseDC(hwnd, hdc);
			}
			delete codec;
			pstream->Release();
		}
	}
	else
	{
		DBGPRINTF((L"LoadImageBmp: HR=%X\nPath='%s'\n", hr, filename));
	}
	return hr;
}

HRESULT LoadBmp(HWND hwnd, LPCWSTR filename, HBITMAP *phbmp)
{
	return LoadImageBmp(hwnd, filename, HDTIT_BMP, NULL, NULL, phbmp);
}


/* writes a bitmap image to a .bmp file.

make sure the input bitmap is a 32-bpp DIB. use Win32 CreateDIBSection() to create it.
the code assumes no scanline padding is required and no color palette is needed, meaning that the color depth must be 32-bpp. 
the input filename can contain environment variables, e.g., %TMP%.
*/
HRESULT SaveBmp(HBITMAP hbm, LPCWSTR filename, bool topdownScan/*=false*/, HWND hwnd/*=NULL*/)
{
	BITMAP bm = { 0 };
	GetObject(hbm, sizeof(bm), &bm);

	HRESULT hr = S_OK;
	BITMAPINFO bi = { sizeof(BITMAPINFOHEADER) };
	BITMAPINFOHEADER &bih = bi.bmiHeader;
	/* For uncompressed RGB bitmaps, if biHeight is positive, the bitmap is a bottom-up DIB with the origin at the lower left corner. If biHeight is negative, the bitmap is a top-down DIB with the origin at the upper left corner.
*/
	ASSERT(bm.bmHeight > 0);
	bih.biWidth = bm.bmWidth;
	bih.biHeight = topdownScan ? -bm.bmHeight : bm.bmHeight;
	bih.biBitCount = bm.bmBitsPixel;
	bih.biPlanes = 1;
	bih.biCompression = BI_RGB;
	bih.biSizeImage = bm.bmHeight * bm.bmWidthBytes;

	DBGPRINTF((L"SaveBmp: %d, %d (%d); SizeImage=%d\n Path='%s'\n", bm.bmWidth, bm.bmHeight, bih.biHeight, bih.biSizeImage, filename));

	if (bm.bmBits == NULL)
	{
		if (!hwnd)
			return E_UNEXPECTED;
		HDC hdc = GetDC(hwnd);
		LPBYTE bits=NULL;
		HBITMAP hbm2 = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, 0);
		if (hbm2)
		{
			HDC hdc1 = CreateCompatibleDC(hdc);
			HDC hdc2 = CreateCompatibleDC(hdc);
			HBITMAP hbm10 = (HBITMAP)SelectObject(hdc1, hbm);
			HBITMAP hbm20 = (HBITMAP)SelectObject(hdc2, hbm2);
			BitBlt(hdc2, 0, 0, bm.bmWidth, bm.bmHeight, hdc1, 0, 0, SRCCOPY);
			SelectObject(hdc2, hbm20);
			SelectObject(hdc2, hbm10);
			DeleteDC(hdc2);
			DeleteDC(hdc1);

			hr = SaveBmp(hbm2, filename, topdownScan);
			DeleteObject(hbm2);
		}
		else
			hr = E_OUTOFMEMORY;
		ReleaseDC(hwnd, hdc);
		return hr;
	}

	BITMAPFILEHEADER bfh = { 0x4d42 };
	bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(LPBITMAPINFOHEADER) + bm.bmHeight * bm.bmWidthBytes;
	bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(LPBITMAPINFOHEADER);

	WCHAR buf[MAX_PATH];
	ExpandEnvironmentStrings(filename, buf, ARRAYSIZE(buf));
	HANDLE hf = CreateFile(buf, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (hf == INVALID_HANDLE_VALUE)
		return HRESULT_FROM_WIN32(GetLastError());

	ULONG len;
	if (WriteFile(hf, &bfh, sizeof(bfh), &len, NULL) &&
		WriteFile(hf, &bih, sizeof(bih), &len, NULL))
	{
		len = 0;
		WriteFile(hf, bm.bmBits, bih.biSizeImage, &len, NULL);
		if (len != bih.biSizeImage)
			hr = E_FAIL;
		/*if (bih.biHeight > 0)
		{
			LPBYTE p = (LPBYTE)bm.bmBits + bih.biSizeImage;
			for (int i = 0; i < bm.bmHeight; i++)
			{
				p -= bm.bmWidthBytes;
				len = 0;
				WriteFile(hf, p, bm.bmWidthBytes, &len, NULL);
				if (len != bm.bmWidthBytes)
				{
					hr = E_FAIL;
					break;
				}
			}
		}
		else
		{
			len = 0;
			WriteFile(hf, bm.bmBits, bih.biSizeImage, &len, NULL);
			if (len != bih.biSizeImage)
				hr = E_FAIL;
		}
		*/
	}
	else
		hr = HRESULT_FROM_WIN32(GetLastError());
	CloseHandle(hf);
	return hr;
}

// adjust the output rectangle's dimensions based on the source image's aspect ratio.
void _preserveAspectRatio(int srcWidth, int srcHeight, int destWidth, int destHeight, RECT *correctedDestRect)
{
	int x2 = 0;
	int y2 = 0;
	int cx2 = destWidth;
	int cy2 = destHeight;
	if (srcWidth != destWidth || srcHeight != destHeight)
	{
		double aspratio = (double)srcHeight / (double)srcWidth;
		int cx1 = (int)((double)destHeight / aspratio);
		int cy1 = (int)((double)destWidth * aspratio);
		if (destWidth && cx1 >= destWidth)
		{
			cy2 = cy1;
			y2 = (destHeight - cy2) / 2;
		}
		else if (destHeight && cy1 >= destHeight)
		{
			cx2 = cx1;
			x2 = (destWidth - cx2) / 2;
		}
		else if (destHeight && !destWidth)
		{
			cx2 = cx1;
		}
		else if (destWidth && !destHeight)
		{
			cy2 = cy1;
		}
	}
	correctedDestRect->left = x2;
	correctedDestRect->right = x2 + cx2;
	correctedDestRect->top = y2;
	correctedDestRect->bottom = y2 + cy2;
}

HBITMAP FitBitmapToWindowFrame(HBITMAP hbmSrc, HWND hwnd)
{
	RECT rcFrame;
	GetClientRect(hwnd, &rcFrame);
	return _FitBitmapToWindowFrame(hbmSrc, hwnd, { rcFrame.right, rcFrame.bottom });
}

HBITMAP _FitBitmapToWindowFrame(HBITMAP hbmSrc, HWND hwnd, SIZE frameSize, HANDLE hKill, ULONG flags)
{
	BITMAP bm;
	GetObject(hbmSrc, sizeof(bm), &bm);
	RECT rcFit;
	if ((flags & FBTWF_FLAG_KEEP_SIZE_IF_SMALLER) && (bm.bmWidth <= frameSize.cx && bm.bmHeight <= frameSize.cy))
	{
		// keep the current small bitmap. no need to scale it. just clone it.
		rcFit.left = rcFit.top = 0;
		rcFit.right = bm.bmWidth;
		rcFit.bottom = bm.bmHeight;
		return _CropBitmap(hwnd, hbmSrc, &rcFit);
	}
	if (flags & FBTWF_FLAG_IGNORE_SOURCE_ASPECT)
	{
		rcFit.left = rcFit.top = 0;
		rcFit.right = frameSize.cx;
		rcFit.bottom = frameSize.cy;
	}
	else if (flags & FBTWF_FLAG_RELY_ON_FRAME_HEIGHT)
	{
		_preserveAspectRatio(bm.bmWidth, bm.bmHeight, 0, frameSize.cy, &rcFit);
	}
	else if (flags & FBTWF_FLAG_RELY_ON_FRAME_WIDTH)
	{
		_preserveAspectRatio(bm.bmWidth, bm.bmHeight, frameSize.cx, 0, &rcFit);
	}
	else
	{
		_preserveAspectRatio(bm.bmWidth, bm.bmHeight, frameSize.cx, frameSize.cy, &rcFit);
	}
	int cx = rcFit.right - rcFit.left;
	int cy = rcFit.bottom - rcFit.top;
	DBGPRINTF((L"_FitBitmapToWindowFrame: SrcBmp=%dx%d (%d%%); DestFrame=%dx%d ==> %dx%d (%d%%)\n", bm.bmWidth, bm.bmHeight, MulDiv(100,bm.bmWidth,bm.bmHeight), frameSize.cx, frameSize.cy, cx, cy, MulDiv(100, cx, cy)));

	HDC hdc = GetDC(hwnd);
#ifdef REFITBMP_USES_SQUEEZE_HELPER
	HBITMAP hbmDest = NULL;
	ScaledBitmap sbmp(hKill);
	if (sbmp.SetSourceBitmap(hdc, hbmSrc, &bm, true))
	{
		hbmDest = sbmp.GetScaledBitmap({ cx,cy });
	}
#else//#ifdef REFITBMP_USES_SQUEEZE_HELPER
	// create two memory DC's. one for the source bitmap. another a copied bitmap.
	HDC hdc1 = CreateCompatibleDC(hdc);
	HDC hdc2 = CreateCompatibleDC(hdc);
	HBITMAP hbmp01 = (HBITMAP)SelectObject(hdc1, hbmSrc);
	// create a destination bitmap (a copy)
	HBITMAP hbmDest = CreateCompatibleBitmap(hdc, cx, cy);
	HBITMAP hbmp02 = (HBITMAP)SelectObject(hdc2, hbmDest);
	StretchBlt(hdc2, 0, 0, cx, cy, hdc1, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
	SelectObject(hdc2, hbmp02);
	SelectObject(hdc1, hbmp01);
	DeleteDC(hdc2);
	DeleteDC(hdc1);
#endif//#ifdef REFITBMP_USES_SQUEEZE_HELPER
	ReleaseDC(hwnd, hdc);
	return hbmDest;
}

HBITMAP _CropBitmap(HWND hwnd, HBITMAP hbmSrc, RECT *destRect)
{
	SIZE destSz = { destRect->right - destRect->left,destRect->bottom - destRect->top };
	HDC hdc = GetDC(hwnd);
	HDC hdc1 = CreateCompatibleDC(hdc);
	HDC hdc2 = CreateCompatibleDC(hdc);
	HBITMAP hbm2 = CreateCompatibleBitmap(hdc, destSz.cx, destSz.cy);
	HBITMAP hbm20 = (HBITMAP)SelectObject(hdc2, hbm2);
	HBITMAP hbm10 = (HBITMAP)SelectObject(hdc1, hbmSrc);
	BitBlt(hdc2, 0, 0, destSz.cx, destSz.cy, hdc1, destRect->left, destRect->top, SRCCOPY);
	SelectObject(hdc1, hbm10);
	SelectObject(hdc2, hbm20);
	DeleteDC(hdc2);
	DeleteDC(hdc1);
	ReleaseDC(hwnd, hdc);
	SB_SAVEBMP(hbm2, BMP_SAVE_PATH4, false, hwnd);
	return hbm2;
}

HBITMAP _CropBitmapDIB(HWND hwnd, HBITMAP hbmSrc, RECT *destRect)
{
#ifdef _DEBUG
	BITMAP bm1;
	GetObject(hbmSrc, sizeof(BITMAP), &bm1);
#endif//#ifdef _DEBUG
	SIZE destSz = { destRect->right - destRect->left,destRect->bottom - destRect->top };
	BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = destSz.cx;
	bmi.bmiHeader.biHeight = -destSz.cy;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32; // 32-bit color depth
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = destSz.cx * destSz.cy * sizeof(RGBQUAD);

	HDC hdc = GetDC(hwnd);
	HDC hdc1 = CreateCompatibleDC(hdc);
	HDC hdc2 = CreateCompatibleDC(hdc);

	LPBYTE destData = NULL;
	HBITMAP hbm2 = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (LPVOID*)&destData, NULL, 0);
	if (hbm2)
	{
		HBITMAP hbm20 = (HBITMAP)SelectObject(hdc2, hbm2);
		HBITMAP hbm10 = (HBITMAP)SelectObject(hdc1, hbmSrc);
		BitBlt(hdc2, 0, 0, destSz.cx, destSz.cy, hdc1, destRect->left, destRect->top, SRCCOPY);
		SelectObject(hdc1, hbm10);
		SelectObject(hdc2, hbm20);
	}
	DeleteDC(hdc2);
	DeleteDC(hdc1);
	ReleaseDC(hwnd, hdc);
	return hbm2;
}

HRESULT RefitBitmapToViewFrame(HDC hdc, SIZE viewframe, bool forceTopdown, HBITMAP *hbmpInOut)
{
	HBITMAP hbmp1 = *hbmpInOut;
	BITMAP bm;
	GetObject(hbmp1, sizeof(BITMAP), &bm);
	double bmx = (double)bm.bmWidth;
	double bmy = (double)bm.bmHeight;
#ifdef REFITBMP_BY_FRAME_RATIO
	double imageToFrameRatio = REFITBMP_IMG_TO_VIEWFRAME_RATIO;
	double vfx = imageToFrameRatio * (double)viewframe.cx;
#else//REFITBMP_BY_FRAME_RATIO
	double vfx = REFITBMP_IN_VIEWFRAME_IMG_WIDTH;
	double imageToFrameRatio = vfx / viewframe.cx;
#endif//REFITBMP_BY_FRAME_RATIO
	double vfy = imageToFrameRatio * (double)viewframe.cy;
	// see if the original size fits the view frame.
	if (bmx <= vfx && bmy < vfy)
		return S_FALSE; // no need to reduce the image size.
	// we do need to reduce the image size.
	double rx = vfx / bmx;
	double ry = vfy / bmy;
	double r = rx < ry ? rx : ry;
	SIZE sz2;
	sz2.cx = lrint(r*(double)bm.bmWidth);
	sz2.cy = lrint(r*(double)bm.bmHeight);
#ifdef REFITBMP_USES_SQUEEZE_HELPER
	HBITMAP hbmp2 = NULL;
	ScaledBitmap sbmp;
	if (sbmp.SetSourceBitmap(hdc, hbmp1, &bm, forceTopdown))
	{
		hbmp2 = sbmp.GetScaledBitmap(sz2);
	}
#else//#ifdef REFITBMP_USES_SQUEEZE_HELPER
	HDC hdc2 = CreateCompatibleDC(hdc);
	HBITMAP hbmp10 = (HBITMAP)SelectObject(hdc, hbmp1);
	/*BITMAPINFO bmi2 = { 0 };
	bmi2.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi2.bmiHeader.biWidth = sz2.cx;
	bmi2.bmiHeader.biHeight = -((LONG)sz2.cy);
	bmi2.bmiHeader.biPlanes = 1;
	bmi2.bmiHeader.biBitCount = 32; // 32-bit resolution
	bmi2.bmiHeader.biCompression = BI_RGB;
	bmi2.bmiHeader.biSizeImage = sz2.cx * sz2.cy * sizeof(COLORREF);
	LPBYTE bits=NULL;
	HBITMAP hbmp2 = CreateDIBSection(hdc, &bmi2, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, 0);
	*/
	HBITMAP hbmp2 = CreateCompatibleBitmap(hdc, sz2.cx, sz2.cy);
	HBITMAP hbmp20 = (HBITMAP)SelectObject(hdc2, hbmp2);
	StretchBlt(hdc2, 0, 0, sz2.cx, sz2.cy, hdc, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
	SelectObject(hdc2, hbmp20);
	SelectObject(hdc, hbmp10);
	DeleteDC(hdc2);
#endif//#ifdef REFITBMP_USES_SQUEEZE_HELPER
	*hbmpInOut = hbmp2;
	DeleteObject(hbmp1);
	// successfully refitted the image to the requested dimensions of the view frame.
	return S_OK;
}

HRESULT GetSaveAsPath(HWND hwnd, LPWSTR outBuf, int bufSize)
{
	ZeroMemory(outBuf, bufSize * sizeof(WCHAR));
	LPWSTR pictdir;
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &pictdir);
	if (hr == S_OK)
	{
		ustring2 filter(IDS_SAVEAS_FILTER);
		OPENFILENAME ofn = { sizeof(OPENFILENAME), hwnd, 0 };
		ofn.Flags = OFN_PATHMUSTEXIST;
		ofn.lpstrInitialDir = pictdir;
		ofn.lpstrDefExt = L".png";
		ofn.lpstrFilter = filter; // "Bitmap image files\0.bmp\0All files\0*.*\0\0"
		ofn.lpstrFile = outBuf;
		ofn.nMaxFile = bufSize;
		if (GetSaveFileName(&ofn))
			hr = S_OK;
		else
			hr = E_ABORT;
		CoTaskMemFree(pictdir);
	}
	return hr;
}

HRESULT SaveDataFile(LPCWSTR filename, const void *srcData, ULONG srcLen)
{
	HANDLE hf = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (hf == INVALID_HANDLE_VALUE)
		return HRESULT_FROM_WIN32(GetLastError());
	HRESULT hr=S_OK;
	ULONG len;
	if (!WriteFile(hf, srcData, srcLen, &len, NULL))
		hr = HRESULT_FROM_WIN32(GetLastError());
	CloseHandle(hf);
	return hr;
}

HRESULT LoadTextFile(LPCWSTR filename, ustring *outBuf)
{
	HANDLE hf = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hf == INVALID_HANDLE_VALUE)
		return HRESULT_FROM_WIN32(GetLastError());
	ULONG fsize = GetFileSize(hf, NULL);
	LPWSTR buf = outBuf->alloc(fsize / sizeof(WCHAR));
	HRESULT hr = S_OK;
	ULONG len;
	if (ReadFile(hf, buf, fsize, &len, NULL))
		outBuf->_length = len / sizeof(WCHAR);
	else
		hr = HRESULT_FROM_WIN32(GetLastError());
	CloseHandle(hf);
	return hr;
}

BOOL FileExists(LPCWSTR pathname)
{
	WIN32_FIND_DATA wfd;
	HANDLE hf = FindFirstFile(pathname, &wfd);
	if (hf == INVALID_HANDLE_VALUE)
		return FALSE;
	FindClose(hf);
	return (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)? FALSE:TRUE;
}

BOOL DirectoryExists(LPCWSTR pathname)
{
	WIN32_FIND_DATA wfd;
	HANDLE hf = FindFirstFile(pathname, &wfd);
	if (hf == INVALID_HANDLE_VALUE)
		return FALSE;
	FindClose(hf);
	return (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;
}
