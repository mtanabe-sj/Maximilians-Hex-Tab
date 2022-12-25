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
#include "ScanTag.h"
#include "resource.h"
#include "CodecImage.h"
#include "EZXML.h"


// comment this out to suppress the display of an image type logo by some of the ScanTag subclasses.
#define SCANTAG_ADDS_LOGO

#define SCANREGION_DATABUFFER_SIZE 0x100

#define LONG_ABS(x) (x<0?-x:x)

/////////////////////////////////////////////////////
// byte-order helpers

void _swapBytes(BYTE &b1, BYTE &b2)
{
	BYTE tmp = b1;
	b1 = b2;
	b2 = tmp;
}

void _reverseByteOrder(LPVOID pv, size_t len)
{
	LPBYTE p = (LPBYTE)pv;
	unsigned char *p2 = p + len - 1;
	size_t n = len >> 1;
	while (n--)
	{
		_swapBytes(*p2--, *p++);
	}
}

DWORD _reverseInt32(DWORD val)
{
	DWORD val2 = val;
	_reverseByteOrder(&val2, sizeof(val));
	return val2;
}


class ROMemoryFile : public ROFileBase
{
public:
	ROMemoryFile(LPBYTE basePtr, ULONG baseOffset, int dataOffset, int dataLen)
	{
		ZeroMemory(&_fi, sizeof(_fi));
		_fi.DoubleBuffer = (LPBYTE)malloc((size_t)SCANREGION_DATABUFFER_SIZE * 2);
		ZeroMemory(_fi.DoubleBuffer, SCANREGION_DATABUFFER_SIZE * 2);
		_fi.DataBuffer = _fi.DoubleBuffer + SCANREGION_DATABUFFER_SIZE;
		_fi.BufferSize = SCANREGION_DATABUFFER_SIZE;
		_fi.FileSize.LowPart = (ULONG)dataLen;

		_basePtr = basePtr;
		_dataPtr = basePtr + dataOffset;
		_baseOffset = baseOffset;
		_dataOffset = dataOffset;
		_dataLen = dataLen;
	}
	~ROMemoryFile()
	{
		free(_fi.DoubleBuffer);
	}

	FILEREADINFO2 _fi;
	LPBYTE _basePtr;
	LPBYTE _dataPtr;
	ULONG _baseOffset;
	int _dataOffset, _dataLen;

	virtual DWORD queryFileSize(ULARGE_INTEGER *fileSize) const
	{
		*fileSize = _fi.FileSize;
		return ERROR_SUCCESS;
	}

protected:
	virtual HANDLE openFile(FILEREADINFO *fi) const
	{
		fi->FileSize = _fi.FileSize;
		fi->ErrorCode = 0;
		return NULL;
	}
	virtual HRESULT readFile(HANDLE, FILEREADINFO *fi, LARGE_INTEGER* seekOffset, ULONG readSize) const
	{
		if (seekOffset->QuadPart > (LONGLONG)_fi.FileSize.QuadPart)
		{
			fi->ErrorCode = ERROR_RANGE_NOT_FOUND;
			return E_ACCESSDENIED;
		}
		LPBYTE src = _dataPtr + seekOffset->LowPart;
		int srcLen = _dataLen - (int)seekOffset->LowPart;
		int copyLen = min(srcLen, (int)readSize);

		CopyMemory(fi->DataBuffer, src, readSize);
		fi->DataLength = copyLen;
		fi->HasData = true;
		return S_OK;
	}
};


/////////////////////////////////////////////////////

class MetadataEndian
{
public:
	MetadataEndian(bool bigEndian=false) : _big(bigEndian), _headerLen(0), _baseLen(0), _basePtr(NULL) {}

	bool _big;
	int _headerLen;
	int _baseLen;
	LPBYTE _basePtr;

	USHORT toUSHORT(USHORT *src) const
	{
		_reverseByteOrder(src, sizeof(USHORT));
		return *src;
	}
	ULONG toULONG(ULONG *src) const
	{
		_reverseByteOrder(src, sizeof(ULONG));
		return *src;
	}

	USHORT getUSHORT(LPVOID src) const
	{
		USHORT v = *(USHORT*)src;
		if (_big)
			_reverseByteOrder(&v, sizeof(v));
		return v;
	}
	ULONG getULONG(LPVOID src) const
	{
		ULONG v = *(ULONG*)src;
		if (_big)
			_reverseByteOrder(&v, sizeof(v));
		return v;
	}
	ULONGLONG getULONGLONG(LPVOID src) const
	{
		ULONGLONG v = *(ULONGLONG*)src;
		if (_big)
			_reverseByteOrder(&v, sizeof(v));
		return v;
	}
	float getFLOAT(LPVOID src) const
	{
		float v = *(float*)src;
		if (_big)
			_reverseByteOrder(&v, sizeof(v));
		return v;
	}
	double getDOUBLE(LPVOID src) const
	{
		double v = *(double*)src;
		if (_big)
			_reverseByteOrder(&v, sizeof(v));
		return v;
	}
};


/////////////////////////////////////////////////////

ScanTag::ScanTag(BinhexMetaView *bmv) : ROFileAccessor(bmv), _bmv(bmv), _terminated(false), _thread(NULL), _kill(NULL)
{
}

ScanTag::~ScanTag()
{
	stop();
}

bool ScanTag::start()
{
	if (_bmv->_fri.FileSize.QuadPart == 0)
		return false;
	// start a worker thread.
	_kill = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	_thread = ::CreateThread(NULL, 0, _runWorker, this, 0, NULL);
	if (_thread == NULL)
		return false;
	return !_terminated;
}

void  ScanTag::stop()
{
	HANDLE hThread = InterlockedExchangePointer(&_thread, NULL);
	if (hThread)
	{
		SetEvent(_kill);
		if (WAIT_OBJECT_0 != ::WaitForSingleObject(hThread, INFINITE))
		{
			ASSERT(FALSE);
		}
		CloseHandle(hThread);
	}
	HANDLE hkill = InterlockedExchangePointer(&_kill, NULL);
	if (hkill)
		CloseHandle(hkill);
}

LPCSTR ScanTag::_toTextEncodingName()
{
	if (_bmv->_bom == TEXT_ASCII)
		return "ASCII";
	if (_bmv->_bom == TEXT_GENERIC)
		return "Extended ASCII";
	if (TEXT_UTF7_A <= _bmv->_bom && _bmv->_bom <= TEXT_UTF7_E)
		return "UTF-7";
	if (_bmv->_bom == TEXT_UTF8)
		return "UTF-8";
	if (_bmv->_bom == TEXT_UTF8_NOBOM)
		return "UTF-8 (no BOM)";
	if (_bmv->_bom == TEXT_UTF16)
		return "Unicode";
	if (_bmv->_bom == TEXT_UTF16_NOBOM)
		return "Unicode (no BOM)";
	if (_bmv->_bom == TEXT_UTF16_BE)
		return "Unicode (Big Endian)";
	if (_bmv->_bom == TEXT_UTF32)
		return "UTF-32";
	if (_bmv->_bom == TEXT_UTF32_BE)
		return "UTF-32 (Big Endian)";
	return "(Unknown)";
}

USHORT toShortScanStatus(DWORD status)
{
	if (status == ERROR_SUCCESS)
		return 0;
	if (status == ERROR_OPERATION_ABORTED)
		return 2;
	return 1;
}

DWORD ScanTag::runWorker()
{
	HWND hwnd = _bmv->_hw;
	FILEREADINFO2 fi = {};
	fi.FileSize.QuadPart = _bmv->_fri.FileSize.QuadPart;
	fi.DoubleBuffer = (LPBYTE)malloc((size_t)SCANREGION_DATABUFFER_SIZE * 2);
	fi.DataBuffer = fi.DoubleBuffer + SCANREGION_DATABUFFER_SIZE;
	fi.BufferSize = SCANREGION_DATABUFFER_SIZE;
	ZeroMemory(fi.DoubleBuffer, SCANREGION_DATABUFFER_SIZE * 2);
	DWORD status = runInternal(&fi);
	/* moved to BinhexDumpWnd::OnScanTagFinish.
	if (status == 0)
	{
		if(_bmv->_annotations._imageSrcCount > 0 || _bmv->_annotations._textSrcCount > 0)
			PostMessage(_bmv->_hw, WM_COMMAND, IDM_SAVEMETA, 0);
	}
	*/
	free(fi.DoubleBuffer);
	PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_SCANTAG_FINISH, toShortScanStatus(status)), (LPARAM)(LPVOID)this);
	_terminated = true;
	return status;
}


//////////////////////////////////////////////////////////////

class MetaPngScan
{
public:
	MetaPngScan(ScanTag *sr, FILEREADINFO2 *fi, ULONG maxScan = 0) : _sr(sr), _fi(fi), atIEND(false), _maxScan(maxScan), _ihdr{ 0 }, _initOffset(fi->DataOffset.LowPart + fi->DataUsed) {}
	bool atIEND;

	/////////////////////////////////////////////
	// PNG chunk definition is documented here.
	// https://www.w3.org/TR/PNG-Chunks.html

#define CHUNKS_ID_LEN 4

#pragma pack(1)
	struct CHUNKEMPTY
	{
		DWORD Size;
		BYTE Id[CHUNKS_ID_LEN];
		/*
		BYTE Data[Size]; // for an actual chunk, Data has a byte length of Size.
		*/
		DWORD CRC; // actual location of CRC is offset by Size
	};
	struct CHUNK
	{
		CHUNKEMPTY Info;
		LPBYTE Data;
		LARGE_INTEGER Offset;
		bool Verified;
	};
	struct IHDR
	{
		DWORD Width; //4 bytes
		DWORD Height; //4 bytes
		BYTE BitDepth; //1 byte
		BYTE ColorType; // 1 byte
		BYTE CompressionType; // 1 byte
		BYTE FilterType; // 1 byte
		BYTE InterlaceType; // 1 byte
	};
#pragma pack()
	const BYTE PngSig[8] = { 0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A };
	IHDR _ihdr;

	bool isPNG(bool rewindIfFail=false)
	{
		LPBYTE p = _sr->freadp(sizeof(PngSig), _fi);
		if (!p)
			return false;
		if (memcmp(p, PngSig, sizeof(PngSig)) == 0)
			return true;
		if (rewindIfFail)
			_sr->fseek(_fi, -(int)sizeof(PngSig), ROFSEEK_ORIGIN_CUR);
		_fi->ErrorCode = ERROR_RANGE_NOT_FOUND;
		return false;
	}

	IHDR chunkToIHDR(CHUNK &ck)
	{
		IHDR hdr = *(IHDR*)ck.Data;
		hdr.Width = _reverseInt32(hdr.Width);
		hdr.Height = _reverseInt32(hdr.Height);
		return hdr;
	}

	int collect()
	{
		int chunks = 0;
		_imgdataRegion = 0;

		CHUNK chunk;
		while (getNextChunk(&chunk))
		{
			ustring chunkId(CP_ACP, (LPCSTR)chunk.Info.Id, CHUNKS_ID_LEN);
			int parentRegion = onScanChunkDescriptor(chunk, chunkId);

			LARGE_INTEGER dataOffset;
			dataOffset.QuadPart = chunk.Offset.QuadPart + FIELD_OFFSET(MetaPngScan::CHUNKEMPTY, CRC);
			int regionId = onScanChunkData(parentRegion, dataOffset, chunk, chunkId);

			// watch for the first IDAT chunk. we want to attach a thumbnail picture of the PNG image.
			if (_imgdataRegion == 0 &&
				0 == memcmp(chunk.Info.Id, "IDAT", CHUNKS_ID_LEN))
			{
				_imgdataRegion = regionId;
			}

			dataOffset.QuadPart += chunk.Info.Size;
			onScanChunkCRC(parentRegion, dataOffset, chunk, chunkId);
			chunks++;
		}

		if (_fi->ErrorCode == ERROR_SUCCESS)
			_annotateImage(_imgdataRegion);

		return chunks;
	}

protected:
	ScanTag *_sr;
	FILEREADINFO2 *_fi;
	ULONG _maxScan;
	ULONG _initOffset;
	int _imgdataRegion;

	LPCWSTR _PngColorType(BYTE colorType)
	{
		/*
		1 - palette in use
		2 - colors (RGB) in use
		4 - alpha channel in use
		*/
		switch (colorType)
		{
		case 0:
			return L"Grayscale (0)";
		case 2:
			return L"RGB (2)";
		case 3:
			return L"Palette Index (3)";
		case 4:
			return L"Grayscale w/Alpha (4)";
		case 6:
			return L"RGB w/Alpha (6)";
		}
		// invalid color type
		return L"(Unknown)";
	}

	LPCWSTR _PngCompressionMethod(BYTE comprMethod)
	{
		if (comprMethod == 0)
			return L"Deflate/Inflate (0)";
		return L"(Unknown)";
	}

	LPCWSTR _PngInterlaceMethod(BYTE interlMethod)
	{
		if (interlMethod == 0)
			return L"(No interlace)";
		if (interlMethod == 1)
			return L"Adam7 interlace";
		return L"(Unknown)";
	}

	virtual int onScanChunkDescriptor(CHUNK chunk, LPCWSTR chunkId)
	{
		return _sr->_bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST, chunk.Offset, FIELD_OFFSET(CHUNKEMPTY, CRC), L"PNG Chunk %s:\n Offset 0x%I64X\n Data size %d bytes", chunkId, chunk.Offset.QuadPart, chunk.Info.Size);
	}
	virtual int onScanChunkData(int parentRegion, LARGE_INTEGER dataOffset, CHUNK chunk, LPCWSTR chunkId)
	{
		if (0 == memcmp(chunk.Info.Id, "IHDR", CHUNKS_ID_LEN))
		{
			// expand the IHDR structure
			_ihdr = chunkToIHDR(chunk);

			return _sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND, parentRegion), dataOffset, chunk.Info.Size, L"Data of %s (%d bytes) - Image Header\n Dimensions: %d x %d\n Bit depth: %d BPP\n Color type: %s\n Compression type: %s\n Interlace type: %s", (LPCWSTR)chunkId, chunk.Info.Size, _ihdr.Width, _ihdr.Height, _ihdr.BitDepth, _PngColorType(_ihdr.ColorType), _PngCompressionMethod(_ihdr.CompressionType), _PngInterlaceMethod(_ihdr.InterlaceType));
		}
		if (0 == memcmp(chunk.Info.Id, "IDAT", CHUNKS_ID_LEN))
		{
			USHORT flags = BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND | BMVADDANNOT_FLAG_GRAYTEXT;
			if (_imgdataRegion == 0)
				flags |= BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST;
			return _sr->_bmv->addTaggedRegion(MAKELONG(flags, parentRegion), dataOffset, chunk.Info.Size, L"Image Data of %s (%d bytes)", (LPCWSTR)chunkId, chunk.Info.Size);
		}
		return _sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND | BMVADDANNOT_FLAG_GRAYTEXT, parentRegion), dataOffset, chunk.Info.Size, L"Data of %s (%d bytes)", (LPCWSTR)chunkId, chunk.Info.Size);
	}
	virtual int onScanChunkCRC(int parentRegion, LARGE_INTEGER dataOffset, CHUNK chunk, LPCWSTR chunkId)
	{
		return _sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND | BMVADDANNOT_FLAG_GRAYTEXT, parentRegion), dataOffset, sizeof(DWORD), L"CRC of %s", (LPCWSTR)chunkId);
	}

	// returns true if a chunk starts at the current file pointer.
	// the caller's ck will be filled with a descriptor of the found chunk.
	bool getNextChunk(CHUNK *ck)
	{
		ZeroMemory(ck, sizeof(CHUNK));
		if (atIEND)
			return false; // not an error condition. so, ErrorCode is not set.
		if ((_maxScan != 0) && ((_fi->DataOffset.QuadPart + _fi->DataUsed + FIELD_OFFSET(CHUNKEMPTY, CRC)) > (_initOffset+_maxScan)))
		{
			_fi->ErrorCode = ERROR_HANDLE_EOF;
			return false;
		}

		LPBYTE p;
		if (!(p = _sr->freadp(FIELD_OFFSET(CHUNKEMPTY, CRC), _fi)))
			return false;

		CHUNKEMPTY *pch = (CHUNKEMPTY*)p;
		DWORD chunkSize = _reverseInt32(pch->Size);
		CopyMemory(ck->Info.Id, pch->Id, CHUNKS_ID_LEN);
		ck->Info.Size = chunkSize;
		ck->Offset.QuadPart = _fi->StartOffset.QuadPart;
		CRC32 crc;
		crc.feedData(p + FIELD_OFFSET(CHUNKEMPTY, Id), CHUNKS_ID_LEN);
		if (chunkSize <= SCANREGION_DATABUFFER_SIZE)
		{
			p = _sr->freadp(chunkSize, _fi);
			crc.feedData(p, chunkSize);
			ck->Data = p;
		}
		else
		{
			ULONG len, left = chunkSize;
			do
			{
				if (left < SCANREGION_DATABUFFER_SIZE)
					len = left;
				else
					len = SCANREGION_DATABUFFER_SIZE;
				left -= len;
				p = _sr->freadp(len, _fi);
				if (!p)
					return false;
				crc.feedData(p, len);
			} while (left);
			ck->Data = NULL;
		}
		p = _sr->freadp(sizeof(DWORD), _fi);
		if (!p)
			return false;
		ck->Info.CRC = _reverseInt32(*(DWORD*)p);
		if (crc.value() == ck->Info.CRC)
			ck->Verified = true;
		if (0 == memcmp(ck->Info.Id, "IEND", CHUNKS_ID_LEN))
			atIEND = true;
		return true;
	}

	bool _annotateImage(int regionId)
	{
		if (regionId == 0)
			return false;
		CRegionCollection &rc = _sr->_bmv->_regions;
		AnnotationCollection &ac = _sr->_bmv->_annotations;
		int annotId = ac.queryByCtrlId(rc[regionId - 1].AnnotCtrlId);
		if (annotId == -1)
			return false;
		HBITMAP hbmp;
		if (FAILED(_createImageBitmap(&hbmp)))
			return false;
		AnnotationOperation annot(ac[annotId], &ac);
		annot.attachThumbnail(hbmp);
		return true;
	}

	virtual HRESULT _createImageBitmap(HBITMAP *hbmpOut)
	{
		HRESULT hr;
		LPSTREAM ps;
		hr = CreateStreamOnHGlobal(NULL, TRUE, &ps);
		if (hr == S_OK)
		{
			_sr->fseek(_fi, 0, ROFSEEK_ORIGIN_BEGIN);
			hr = _sr->fcopyTo(ps, _fi);
			if (hr == S_OK)
			{
				CodecPNG img(_ihdr.ColorType);
				hr = img.Load(ps);
				if (hr == S_OK)
				{
					HDC hdc0 = CreateDC(L"DISPLAY", NULL, NULL, NULL);
					HDC hdc = CreateCompatibleDC(hdc0);
					hr = img.CreateBitmap(hdc, hbmpOut); // this generates a top-down bitmap.
					if (hr == S_OK)
						hr = RefitBitmapToViewFrame(hdc, _sr->_bmv->_vi.FrameSize, true, hbmpOut);
					DeleteDC(hdc);
					DeleteDC(hdc0);
				}
			}
			ps->Release();
		}
		return hr;
	}

};

class MetaPngNestedScan : public MetaPngScan
{
public:
	MetaPngNestedScan(ScanTag *sr, FILEREADINFO2 *fi, ULONG maxScan, int baseRegion) : MetaPngScan(sr, fi, maxScan), _baseRegion(baseRegion) {}

protected:
	int _baseRegion;

	virtual int onScanChunkDescriptor(CHUNK chunk, LPCWSTR chunkId)
	{
		return _sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_GRAYTEXT, _baseRegion), chunk.Offset, FIELD_OFFSET(CHUNKEMPTY, CRC), L"PNG Chunk %s:\n Offset 0x%I64X\n Data size %d bytes", chunkId, chunk.Offset.QuadPart, chunk.Info.Size);
	}
	virtual int onScanChunkData(int parentRegion, LARGE_INTEGER dataOffset, CHUNK chunk, LPCWSTR chunkId)
	{
		// disregard the input parentRegion. use the base region.
		return MetaPngScan::onScanChunkData(_baseRegion, dataOffset, chunk, chunkId);
	}
	virtual int onScanChunkCRC(int parentRegion, LARGE_INTEGER dataOffset, CHUNK chunk, LPCWSTR chunkId)
	{
		// disregard the input parentRegion. use the base region.
		return MetaPngScan::onScanChunkCRC(_baseRegion, dataOffset, chunk, chunkId);
	}

	virtual HRESULT _createImageBitmap(HBITMAP *hbmpOut)
	{
		HRESULT hr;
		LPSTREAM ps;
		hr = CreateStreamOnHGlobal(NULL, TRUE, &ps);
		if (hr == S_OK)
		{
			// first off, need to buffer all of the thumbnail image data.
			LPBYTE p = (LPBYTE)malloc(_maxScan);
			_sr->fseek(_fi, _initOffset, ROFSEEK_ORIGIN_BEGIN);
			if (_sr->fread(p, _maxScan, 1, _fi))
			{
				// next, create a memory-mapped file on the buffered image.
				ROMemoryFile mf(p, _initOffset, 0, _maxScan);
				// copy the thumbnail data into the stream.
				ROFileAccessor rofa(&mf);
				rofa.fseek(&mf._fi, 0, ROFSEEK_ORIGIN_BEGIN);
				hr = rofa.fcopyTo(ps, &mf._fi);
				if (hr == S_OK)
				{
					// create a codec for png and use it to convert the thumbnail data to a bitmap image. if the output image is too big for the view frame, shrink it.
					CodecPNG img;
					hr = img.Load(ps);
					if (hr == S_OK)
					{
						HDC hdc0 = CreateDC(L"DISPLAY", NULL, NULL, NULL);
						HDC hdc = CreateCompatibleDC(hdc0);
						hr = img.CreateBitmap(hdc, hbmpOut); // this generates a top-down bitmap.
						if (hr == S_OK)
							hr = RefitBitmapToViewFrame(hdc, _sr->_bmv->_vi.FrameSize, true, hbmpOut);
						DeleteDC(hdc);
						DeleteDC(hdc0);
					}
				}
			}
			else
				hr = HRESULT_FROM_WIN32(_fi->ErrorCode);
			free(p);
			ps->Release();
		}
		return hr;
	}
};

DWORD ScanTagPng::runInternal(FILEREADINFO2 *fi)
{
	MetaPngScan png(this, fi);
	if (png.isPNG())
	{
		int parentRegion = _bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST, { 0 }, sizeof(png.PngSig), L"PNG Signature");
#ifdef SCANTAG_ADDS_LOGO
		DUMPANNOTATIONINFO *ai = _bmv->openAnnotationOfRegion(parentRegion, true);
		if (ai)
		{
			HBITMAP hbmp = LoadBitmap(LibResourceHandle, MAKEINTRESOURCE(IDB_PNG_LOGO));
			_bmv->_annotations.setPicture(ai, hbmp);
			DeleteObject(hbmp);
		}
#endif//#ifdef SCANTAG_ADDS_LOGO

		png.collect();
	}
	return fi->ErrorCode;
}


//////////////////////////////////////////////////////////////

class MetaBmpScan
{
public:
	MetaBmpScan(ScanTag *sr, FILEREADINFO2 *fi, ULONG maxScan = 0) : _sr(sr), _fi(fi), _bmfh{ 0 }, _bmph{ 0 },_imgSize(0), _maxScan(maxScan), _parentRegion(0) {
		_pixdataLabel = L"Pixel Data";
	}

	int _parentRegion;
	BITMAPFILEHEADER _bmfh;
	BITMAPV5HEADER _bmph; // to get correct header type, check bV5Size against sizeof(BITMAPV5HEADER), sizeof(BITMAPV4HEADER), and sizeof(BITMAPINFOHEADER).
	ULONG _imgSize;
	simpleList<COLORREF> _palette;

	bool checkSig()
	{
		BITMAPFILEHEADER *bfh = (BITMAPFILEHEADER *)_sr->freadp(sizeof(BITMAPFILEHEADER), _fi);
		if (bfh)
		{
			if (bfh->bfType == 0x4d42) // 'BM'
			{
				CopyMemory(&_bmfh, bfh, sizeof(BITMAPFILEHEADER));
				_sr->_bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST, { 0 }, sizeof(BITMAPFILEHEADER), L"BITMAP FILE HEADER\n Offset to image data: 0x%X\n File size: %d (0x%X) bytes", _bmfh.bfOffBits, _bmfh.bfSize, _bmfh.bfSize);
				return true;
			}
		}
		_fi->ErrorCode = ERROR_RANGE_NOT_FOUND;
		return false;
	}

	int collect()
	{
		UINT bpp = 0;
		LPBYTE p;
		DWORD *bihSize = (DWORD*)_sr->freadp(sizeof(DWORD), _fi);
		if (*bihSize == sizeof(BITMAPV4HEADER))
		{
			p = _sr->freadp(sizeof(BITMAPV4HEADER) - sizeof(DWORD), _fi);
			BITMAPV4HEADER bih;
			bih.bV4Size = *bihSize;
			CopyMemory(&bih.bV4Width, p, sizeof(BITMAPV4HEADER) - sizeof(DWORD));
			CopyMemory(&_bmph, &bih, sizeof(BITMAPV4HEADER));

			bpp = bih.bV4BitCount;
			_imgSize = bih.bV4SizeImage;
			if (_imgSize == 0)
				_imgSize= _getImageSize(bih.bV4BitCount, bih.bV4Width, bih.bV4Height);

			_sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND, _parentRegion), { _fi->StartOffset.LowPart - sizeof(DWORD) }, sizeof(BITMAPV4HEADER), L"BITMAPV4HEADER\n Image size: %d (0x%X) bytes\n Dimensions: %d x %d\n Bit depth: %d BPP\n Palette colors: %d\n Mask: R=%X, G=%X, B=%X, A=%X\n Gamma: R=%X, G=%X, B=%X", bih.bV4SizeImage, bih.bV4SizeImage, bih.bV4Width, bih.bV4Height, _getImageHeight(), bih.bV4BitCount, bih.bV4ClrUsed, bih.bV4RedMask, bih.bV4GreenMask, bih.bV4BlueMask, bih.bV4AlphaMask, bih.bV4GammaRed, bih.bV4GammaGreen, bih.bV4GammaBlue);
		}
		else if (*bihSize == sizeof(BITMAPV5HEADER))
		{
			p = _sr->freadp(sizeof(BITMAPV5HEADER) - sizeof(DWORD), _fi);
			BITMAPV5HEADER bih;
			bih.bV5Size = *bihSize;
			CopyMemory(&bih.bV5Width, p, sizeof(BITMAPV5HEADER) - sizeof(DWORD));
			CopyMemory(&_bmph, &bih, sizeof(BITMAPV5HEADER));

			bpp = bih.bV5BitCount;
			_imgSize = bih.bV5SizeImage;
			if (_imgSize == 0)
				_imgSize= _getImageSize(bih.bV5BitCount, bih.bV5Width, bih.bV5Height);

			_sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND, _parentRegion), { _fi->StartOffset.LowPart - sizeof(DWORD) }, sizeof(BITMAPV5HEADER), L"BITMAPV5HEADER\n Image size: %d (0x%X) bytes\n Dimensions: %d x %d\n Bit depth: %d BPP\n Palette colors: %d\n Mask: R=%X, G=%X, B=%X, A=%X\n Gamma: R=%X, G=%X, B=%X\n Intent: 0x%X\n Profile data: 0x%X", bih.bV5SizeImage, bih.bV5SizeImage, bih.bV5Width, _getImageHeight(), bih.bV5BitCount, bih.bV5ClrUsed, bih.bV5RedMask, bih.bV5GreenMask, bih.bV5BlueMask, bih.bV5AlphaMask, bih.bV5GammaRed, bih.bV5GammaGreen, bih.bV5GammaBlue, bih.bV5Intent, bih.bV5ProfileData);
		}
		else
		{
			p = _sr->freadp(sizeof(BITMAPINFOHEADER) - sizeof(DWORD), _fi);
			BITMAPINFOHEADER bih;
			bih.biSize = *bihSize;
			CopyMemory(&bih.biWidth, p, sizeof(BITMAPINFOHEADER) - sizeof(DWORD));
			CopyMemory(&_bmph, &bih, sizeof(BITMAPINFOHEADER));

			bpp = bih.biBitCount;
			// disregard bih.biSizeImage. for a bmp in an ico, the size means both the xor and and mask images.
			_imgSize = _getImageSize(bih.biBitCount, bih.biWidth, bih.biHeight);

			int paletteColors = bih.biClrUsed;
			if (paletteColors == 0 && bih.biBitCount <= 8)
				paletteColors = 1 << bih.biBitCount;
			if (paletteColors == 0 && bih.biBitCount == 16 && _bmph.bV5Compression == BI_BITFIELDS)
				paletteColors = 3;

			_sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND, _parentRegion), { _fi->StartOffset.LowPart - sizeof(DWORD) }, sizeof(BITMAPINFOHEADER), L"BITMAPINFOHEADER\n Image size: %d (0x%X) bytes\n Dimensions: %d x %d\n Bit depth: %d BPP\n Palette colors: %d", bih.biSizeImage, bih.biSizeImage, bih.biWidth, LONG_ABS(bih.biHeight), bih.biBitCount, bih.biClrUsed);

			if (paletteColors)
			{
				LARGE_INTEGER paletteOffset = _sr->fpos(_fi);
				COLORREF rgb1, rgb=0;
				for (int i = 0; i < paletteColors; i++)
				{
					_sr->fread(&rgb, sizeof(COLORREF), 1, _fi);
					_palette.add(rgb);
					if (i == 0)
						rgb1 = rgb;
				}
				_sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST | BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND | BMVADDANNOT_FLAG_GRAYTEXT, _parentRegion), paletteOffset, paletteColors*sizeof(COLORREF), L"Color Palette\n Colors used: %d\n 1st RGB(%d,%d,%d)\n Last RGB(%d,%d,%d)", paletteColors, GetRValue(rgb1), GetGValue(rgb1), GetBValue(rgb1), GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
			}
		}

		int regionId = _sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST | BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND | BMVADDANNOT_FLAG_GRAYTEXT, _parentRegion), _getOffsetToPixelData(), _imgSize, L"%s\n Data size: %d bytes\n Dimensions: %d x %d pixels\n Scan line: %d bytes\n Color depth: %d BPP", _pixdataLabel, _imgSize, _bmph.bV5Width, _getImageHeight(), _getScanlineLength(_bmph.bV5BitCount, _bmph.bV5Width), _bmph.bV5BitCount);
		_annotateImage(regionId);
		return 1;
	}

protected:
	ScanTag *_sr;
	FILEREADINFO2 *_fi;
	ULONG _maxScan;
	LPCWSTR _pixdataLabel;

	virtual LARGE_INTEGER _getOffsetToPixelData()
	{
		return { _bmfh.bfOffBits };
	}
	virtual int _getImageHeight()
	{
		return (int)LONG_ABS(_bmph.bV5Height);
	}
	ULONG _getImageSize(WORD biBitCount, LONG biWidth, LONG biHeight)
	{
		return (ULONG)(_getScanlineLength(biBitCount, biWidth)*_getImageHeight());
	}
	LONG _getScanlineLength(WORD biBitCount, LONG biWidth)
	{
		switch (biBitCount)
		{
		case 1:
			return SCANLINELENGTH_1BPP(biWidth);
		case 2:
			return SCANLINELENGTH_2BPP(biWidth);
		case 4:
			return SCANLINELENGTH_4BPP(biWidth);
		case 8:
			return SCANLINELENGTH_8BPP(biWidth);
		case 16:
			/* http://www.ece.ualberta.ca/~elliott/ee552/studentAppNotes/2003_w/misc/bmp_file_format/bmp_file_format.htm

			The bitmap has a maximum of 2^16 colors. If the Compression field of the bitmap file is set to BI_RGB, the Palette field does not contain any entries. Each word in the bitmap array represents a single pixel. The relative intensities of red, green, and blue are represented with 5 bits for each color component. The value for blue is in the least significant 5 bits, followed by 5 bits each for green and red, respectively. The most significant bit is not used.
			If the Compression field of the bitmap file is set to BI_BITFIELDS, the Palette field contains three 4 byte color masks that specify the red, green, and blue components, respectively, of each pixel.  Each 2 bytes in the bitmap array represents a single pixel.
			*/
			return SCANLINELENGTH_16BPP(biWidth);
		case 24:
			return SCANLINELENGTH_24BPP(biWidth);
		case 32:
			return SCANLINELENGTH_32BPP(biWidth);
		}
		return 0;
	}

	bool _annotateImage(int regionId)
	{
		CRegionCollection &r = _sr->_bmv->_regions;
		AnnotationCollection &ac = _sr->_bmv->_annotations;
		int annotId = ac.queryByCtrlId(r[regionId - 1].AnnotCtrlId);
		if (annotId == -1)
			return false;
		LARGE_INTEGER offset = _getOffsetToPixelData();
		if (ERROR_SUCCESS != _sr->fseek(_fi, (LONG)offset.LowPart, ROFSEEK_ORIGIN_BEGIN))
			return false;
		LPBYTE imgData = (LPBYTE)malloc(_imgSize);
		bool ret = _sr->fread(imgData, _imgSize, 1, _fi);
		if (ret)
		{
			HBITMAP hbmp = _createBitmapFromImageData(imgData, _imgSize, _getScanlineLength(_bmph.bV5BitCount, _bmph.bV5Width));
			if (hbmp)
			{
				AnnotationOperation annot(ac[annotId], &ac);
				annot.attachThumbnail(hbmp);
			}
			else
				ret = false;
		}
		free(imgData);
		return ret;
	}
	HBITMAP _createBitmapFromImageData(LPBYTE imgData, ULONG imgLen, ULONG imgScanLen)
	{
		HDC hdc0 = CreateDC(L"DISPLAY", NULL, NULL, NULL);
		HDC hdc = CreateCompatibleDC(hdc0);
		LPBYTE pb = new BYTE[sizeof(BITMAPINFOHEADER) + _palette.dataLength()];
		ZeroMemory(pb, sizeof(BITMAPINFOHEADER) + _palette.dataLength());
		LPBITMAPINFO bmi = (LPBITMAPINFO)pb;
		COLORREF *rgb = (COLORREF*)(pb + sizeof(BITMAPINFOHEADER));

		long cy = _getImageHeight();
		bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi->bmiHeader.biPlanes = 1;
		bmi->bmiHeader.biBitCount = _bmph.bV5BitCount;
		bmi->bmiHeader.biWidth = _bmph.bV5Width;
		bmi->bmiHeader.biHeight = _getImageHeight();// _bmph.bV5Height;
		bmi->bmiHeader.biSizeImage = imgScanLen * cy;
		bmi->bmiHeader.biClrUsed = (DWORD)_palette.size();
		bmi->bmiHeader.biClrImportant = (DWORD)_palette.size();
		if (_bmph.bV5BitCount == 16 && _bmph.bV5Compression != BI_RGB)
		{
			//BI_BITFIELDS?
			bmi->bmiHeader.biCompression = _bmph.bV5Compression;
		}

		DBGPRINTF((L"_createBitmapFromImageData: %d, %d (%d); SizeImage=%d\n", bmi->bmiHeader.biWidth, cy, bmi->bmiHeader.biHeight, bmi->bmiHeader.biSizeImage));

		CopyMemory(bmi->bmiColors, _palette.data(), _palette.dataLength());

		LPBYTE bits;
		HBITMAP hbmp = CreateDIBSection(hdc, bmi, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, 0);
		if (hbmp)
		{
			HBITMAP hbm0 = (HBITMAP)SelectObject(hdc, hbmp);
			CopyMemory(bits, imgData, imgLen);
			SetDIBits(hdc, hbmp, 0, bmi->bmiHeader.biHeight, bits, bmi, DIB_RGB_COLORS);
			RefitBitmapToViewFrame(hdc, _sr->_bmv->_vi.FrameSize, false, &hbmp);
			SelectObject(hdc, hbm0);
		}
		delete[] pb;
		DeleteDC(hdc);
		DeleteDC(hdc0);
		return hbmp;
	}
};


DWORD ScanTagBmp::runInternal(FILEREADINFO2 *fi)
{
	MetaBmpScan bmp(this, fi);
	if (bmp.checkSig())
	{
		if (bmp.collect())
			return ERROR_SUCCESS;
	}
	return fi->ErrorCode;
}


//////////////////////////////////////////////////////////////

const int MAX_GIF_TEXTDATA = 255;

const BYTE GIFBLKID_ImageDescriptor = 0x2c;
const BYTE GIFBLKID_ExtensionIntroducer = 0x21;
const BYTE GIFBLKID_Trailer = 0x3B;

const BYTE GIFEXTID_GraphicControlExtension = 0xF9;
const BYTE GIFEXTID_CommentExtension = 0xFE;
const BYTE GIFEXTID_PlainTextExtension = 0x01;
const BYTE GIFEXTID_ApplicationExtension = 0xFF;


class MetaGifInfo
{
public:
	MetaGifInfo(ScanTag *sr, FILEREADINFO2 *fi) : _sr(sr), _fi(fi), _gcx{ 0 }, _imgdesc{ 0 }, _gifinfo{ 0 } {}
	~MetaGifInfo() {}

#pragma pack(1)
	struct HEADER {
		char Signature[3];
		char Version[3];
	};
	struct SDPACKEDFIELDS {
		BYTE cbGlobalColorTable : 3;
		BYTE bSortFlag : 1;
		BYTE nColorResolution : 3;
		BYTE hasGlobalColorTable : 1;
	};
	struct SREENDESCRIPTOR {
		USHORT ScreenWidth;
		USHORT ScreenHeight;
		SDPACKEDFIELDS PackedFields;
		BYTE BackgroundColorIndex;
		BYTE PixelAspectRatio;
	};
	struct IDPACKEDFIELDS {
		BYTE SizeOfLocalColorTable : 3;
		BYTE Reserved : 2;
		BYTE SortFlag : 1;
		BYTE InterlaceFlag : 1;
		BYTE LocalColorTableFlag : 1;
	};
	struct IMAGEDESCRIPTOR {
		BYTE Separator;
		USHORT ImageLeft;
		USHORT ImageTop;
		USHORT ImageWidth;
		USHORT ImageHeight;
		IDPACKEDFIELDS PackedFields;
	};
	struct GCEPACKEDFIELDS {
		BYTE TransparentColorFlag : 1;
		BYTE UserInputFlag : 1;
		BYTE DisposalMethod : 3;
		BYTE Reserved : 3;
	};
	struct GraphicControlExtension {
		BYTE ExtensionIntroducer;
		BYTE GraphicControlLabel;
		BYTE BlockSize;
		GCEPACKEDFIELDS PackedFields;
		USHORT DelayTime;
		BYTE TransparentColorIndex;
		BYTE BlockTerminator;
	};
	struct PlainTextExtension {
		BYTE ExtensionIntroducer;
		BYTE PlainTextLabel;
		BYTE BlockSize;
		USHORT TextGridLeftPos;
		USHORT TextGridTopPos;
		USHORT TextGridWidth;
		USHORT TextGridHeight;
		BYTE CharCellWidth;
		BYTE CharCellHeight;
		BYTE TextForeColorIndex;
		BYTE TextBackColorIndex;
	};
	struct GifTextData {
		char Data[MAX_GIF_TEXTDATA];
		short Length;
	};
	struct PlainTextExtension2 : public PlainTextExtension {
		GifTextData Text;
	};
	struct ApplicationExtension {
		BYTE ExtensionIntroducer;
		BYTE ExtensionLabel;
		BYTE BlockSize;
		char AppIdentifier[8];
		BYTE AppAuthenticationCode[3];
		BYTE DataBlock[1];
	};
	struct GifAppData {
		BYTE Data[512];
		short Length;
	};
	struct ApplicationExtension2 : public ApplicationExtension {
		GifAppData Bin;
	};
	struct AppExtensionData_Netscape20
	{
		BYTE Loop; //1=enable looped image display
		BYTE Repeats; // 0=infinite loop
		BYTE Reserved;
	};
#pragma pack()

	struct GIFHEADERINFO
	{
		int width, height;
		int scanlineLen;
		int bpp;
		int colors;
		int bkgdIndex;
		int transpIndex;
		bool hasTransparency;
		bool is89a;
	} _gifinfo;

	IMAGEDESCRIPTOR _imgdesc;
	GraphicControlExtension _gcx;
	simpleList<RGBQUAD> _vpal;
	simpleList<RGBQUAD> _vpalGlobal;
	simpleList<PlainTextExtension2> _vptx;
	simpleList<GifTextData> _vcomments;
	simpleList<ApplicationExtension2> _vax;
	AppExtensionData_Netscape20 _axNets20;

	bool checkSig()
	{
		HEADER hdr;
		if (!_sr->fread(&hdr, sizeof(HEADER), 1, _fi))
			return false;
		DBGDUMP(("GIF.Signature", &hdr, sizeof(hdr)));
		if (memcmp(hdr.Signature, "GIF", sizeof(hdr.Signature)) == 0)
		{
			if (memcmp(hdr.Version, "87a", sizeof(hdr.Version)) == 0)
				return true;
			if (memcmp(hdr.Version, "89a", sizeof(hdr.Version)) == 0)
			{
				_gifinfo.is89a = true;
				return true;
			}
		}
		_fi->ErrorCode = ERROR_RANGE_NOT_FOUND;
		return false;
	}

	bool readScreenDesc()
	{
		SREENDESCRIPTOR sdesc;
		if (!_sr->fread(&sdesc, sizeof(sdesc), 1, _fi))
			return false;
		// our implementation depends on presence of a color table.
		if (!sdesc.PackedFields.hasGlobalColorTable)
			return false;
		// color depth can be 1 to 8.
		_gifinfo.bpp = sdesc.PackedFields.cbGlobalColorTable + 1;
		_gifinfo.colors = POWEROF2(_gifinfo.bpp);
		// accept a zero (default) aspect ratio only.
		if (sdesc.PixelAspectRatio != 0)
			return false;
		_gifinfo.width = sdesc.ScreenWidth;
		_gifinfo.height = sdesc.ScreenHeight;
		_gifinfo.bkgdIndex = MAKEWORD(sdesc.BackgroundColorIndex, 0);
		DBGDUMP(("GIF.ScreenDescriptor", &_gifinfo, sizeof(_gifinfo)));
		return true;
	}

	bool readColorTable(int numColors, simpleList<RGBQUAD> &vpal)
	{
		RGBTRIPLE rgbt;
		for (int i = 0; i < numColors; i++)
		{
			if (!_sr->fread(&rgbt, sizeof(rgbt), 1, _fi))
				return false;
			RGBQUAD pe = { rgbt.rgbtRed, rgbt.rgbtGreen, rgbt.rgbtBlue, (BYTE)0xff };
			vpal.add(pe);
		}
		return true;
	}

	bool skipColorTable(int colorCount)
	{
		if (_sr->fseek(_fi, colorCount * sizeof(RGBTRIPLE), ROFSEEK_ORIGIN_CUR) == ERROR_SUCCESS)
			return true;
		return false;
	}

	void applyTransparencyToColorTable()
	{
		if (_gifinfo.hasTransparency)
			_vpal[_gifinfo.transpIndex].rgbReserved = 0;
	}
	void removeTransparencyFromColorTable()
	{
		if (_gifinfo.hasTransparency)
			_vpal[_gifinfo.transpIndex].rgbReserved = 0xff;
	}

	bool readExtensionBlocks()
	{
		// [0] = extension introducer, [1] = graphic control label
		BYTE blockId[2];

		while (_gifinfo.is89a)
		{
			// process next extension block
			if (_sr->fread(blockId, sizeof(blockId), 1, _fi) != 1)
				return false;
			LARGE_INTEGER dataOffset = _fi->PriorStart;
			ULONG dataLen = _fi->PriorLength;
			if (ERROR_SUCCESS != _sr->fseek(_fi, -(int)sizeof(blockId), ROFSEEK_ORIGIN_CUR)) // back up.
				return false;
			// check block start id.
			if (blockId[0] != GIFBLKID_ExtensionIntroducer)
				break; // no more block.

			// handle the extension block.
			if (blockId[1] == GIFEXTID_GraphicControlExtension)
			{
				// graphic control extension. read the whole block.
				if (_sr->fread(&_gcx, sizeof(_gcx), 1, _fi) != 1)
					return false;
				dataOffset = _fi->PriorStart;
				dataLen = _fi->PriorLength;
				if (_gcx.PackedFields.TransparentColorFlag)
				{
					_gifinfo.hasTransparency = true;
					_gifinfo.transpIndex = _gcx.TransparentColorIndex;
				}
				DBGDUMP(("GraphicControlExtension", &_gcx, sizeof(_gcx)));
				onScanGraphicControlExtension(dataOffset, dataLen);
			}
			else if (blockId[1] == GIFEXTID_CommentExtension)
			{
				// comment extension.
				dataOffset = _fi->StartOffset;
				_sr->fseek(_fi, sizeof(blockId), ROFSEEK_ORIGIN_CUR);
				_vcomments = _readTextBlock(MAX_GIF_TEXTDATA);
				dataLen = (ULONG)(LONG)(_fi->StartOffset.QuadPart - dataOffset.QuadPart);
				onScanCommentExtension(dataOffset, dataLen);
			}
			else if (blockId[1] == GIFEXTID_PlainTextExtension)
			{
				// plain text extension.
				PlainTextExtension2 ptx;
				if (_sr->fread(&ptx, sizeof(PlainTextExtension), 1, _fi) != 1)
					return false;
				dataOffset = _fi->StartOffset;
				simpleList<GifTextData> t = _readTextBlock(MAX_GIF_TEXTDATA);
				dataLen = (ULONG)(LONG)(_fi->StartOffset.QuadPart - dataOffset.QuadPart);
				// we can keep 0th element only
				if (t.size())
					CopyMemory(&ptx.Text, &t[0], sizeof(GifTextData));
				_vptx.add(ptx);
				onScanPlainTextExtension(dataOffset, dataLen, ptx);
			}
			else if (blockId[1] == GIFEXTID_ApplicationExtension)
			{
				// application extension.
				// don't keep it. just skip the block.
				ApplicationExtension2 ax;
				if (_sr->fread(&ax, sizeof(ApplicationExtension), 1, _fi) != 1)
					return false;
				dataOffset = _fi->PriorStart;
				dataLen = _fi->PriorLength;
				DBGDUMP(("ApplicationExtension", &ax, FIELD_OFFSET(ApplicationExtension, DataBlock)));
				GifAppData vdata = { 0 };
				// check for a netscape extension which is an iteration directive.
				if (0 == memcmp(ax.AppIdentifier, "NETSCAPE", 8) &&
					0 == memcmp(ax.AppAuthenticationCode, "2.0", 3) &&
					ax.DataBlock[0] == sizeof(AppExtensionData_Netscape20))
				{
					/*
					ApplicationExtension (14 bytes)
					0000: 21 FF 0B 4E 45 54 53 43 41 50 45 32 2E 30       !..NETSCAPE2.0
					ApplicationData (4 bytes)
					0000: 03 01 00 00                                     ....
					*/
					// first byte is the extension data's byte length. it should be 3.
					// read in the netscape extension.
					if (_sr->fread(&_axNets20, sizeof(_axNets20), 1, _fi) != 1)
						return false;
					vdata.Data[vdata.Length++] = ax.DataBlock[0];
					memcpy(vdata.Data + vdata.Length, &_axNets20, sizeof(_axNets20));
					vdata.Length += sizeof(_axNets20);
				}
				else if (0 == memcmp(ax.AppIdentifier, "XMP Data", 8) &&
					0 == memcmp(ax.AppAuthenticationCode, "XMP", 3))
				{
					/*
					ApplicationExtension (14 bytes)
					0000: 21 FF 0B 58 4D 50 20 44 61 74 61 58 4D 50       !..XMP DataXMP
					ApplicationData (1112 bytes)
					0000: 3C 3F 78 70 61 63 6B 65 74 20 62 65 67 69 6E 3D <?xpacket begin=
					0001: 22 EF BB BF 22 20 69 64 3D 22 57 35 4D 30 4D 70 "..." id="W5M0Mp
					0002: 43 65 68 69 48 7A 72 65 53 7A 4E 54 63 7A 6B 63 CehiHzreSzNTczkc
					0003: 39 64 22 3F 3E 20 3C 78 3A 78 6D 70 6D 65 74 61 9d"?> <x:xmpmeta
					0004: 20 78 6D 6C 6E 73 3A 78 3D 22 61 64 6F 62 65 3A  xmlns:x="adobe:
					0005: 6E 73 3A 6D 65 74 61 2F 22 20 78 3A 78 6D 70 74 ns:meta/" x:xmpt
					*/
					while (ax.DataBlock[0] != 0)
					{
						vdata.Data[vdata.Length++] = ax.DataBlock[0];
						if (_sr->fread(ax.DataBlock, 1, 1, _fi) != 1)
							return false;
					}
				}
				else
				{
					// unknown extension: read up to a termination zero (same as XMP).
					while (ax.DataBlock[0] != 0)
					{
						vdata.Data[vdata.Length++] = ax.DataBlock[0];
						if (_sr->fread(ax.DataBlock, 1, 1, _fi) != 1)
							return false;
					}
				}
				DBGDUMP(("ApplicationData", vdata.Data, vdata.Length));
				CopyMemory(&ax.Bin, &vdata, sizeof(vdata));
				_vax.add(ax);
				onScanApplicationExtension(dataOffset, dataLen, ax);
				// read in all trailing zeroes.
				do
				{
					if (_sr->fread(ax.DataBlock, 1, 1, _fi) != 1)
						return false;
				} while (ax.DataBlock[0] == 0);
				_sr->fseek(_fi, -1, ROFSEEK_ORIGIN_CUR); // back up.
			}
			else //it's an extension block we don't recognize...
				return false;
		}
		return true;
	}

	bool readImageDesc()
	{
		if (_sr->fread(&_imgdesc, sizeof(_imgdesc), 1, _fi) != 1)
			return false;
		if (_imgdesc.Separator != GIFBLKID_ImageDescriptor)
			return false; // invalid descriptor.
		// v0.2 adding support for local color table
		if (_imgdesc.PackedFields.LocalColorTableFlag)
		{
			// local table follows immediately after this descriptor.
			int colors = POWEROF2(_imgdesc.PackedFields.SizeOfLocalColorTable + 1);
			_vpal.clear();
			if (!readColorTable(colors, _vpal))
				return false;
		}
		else
		{
			// the image will use the global color table.
			_vpal = _vpalGlobal;
		}
		DBGDUMP(("ImageDescriptor", &_imgdesc, sizeof(_imgdesc)));
		return true;
	}

	bool skipImageDesc()
	{
		if (_sr->fread(&_imgdesc, sizeof(_imgdesc), 1, _fi) != 1)
			return false;
		if (_imgdesc.Separator != GIFBLKID_ImageDescriptor)
			return false; // invalid descriptor.
		// v0.2 adding support for local color table
		if (_imgdesc.PackedFields.LocalColorTableFlag)
		{
			// local table follows immediately after this descriptor.
			int colors = POWEROF2(_imgdesc.PackedFields.SizeOfLocalColorTable + 1);
			if (!skipColorTable(colors))
				return false;
		}
		return true;
	}

	bool atEOF(bool checkBlockTerminator = false)
	{
		BYTE b;
		if (checkBlockTerminator)
		{
			if (!_readByte(&b))
				return true;
			// a block is terminated with a null byte.
			if (b != 0)
				return true;
		}
		if (!_readByte(&b))
			return true;
		// check for the termination code
		if (b == GIFBLKID_Trailer)
			return true;
		// back up
		_sr->fseek(_fi, -1, ROFSEEK_ORIGIN_CUR);
		return false;
	}
	virtual bool readImageFrame()
	{
		// we're effectively skipping the frame data.
		BYTE b;
		if (!_readByte(&b))
			return false;
		// code size = bits per pixel.
		int codeSize = MAKEWORD(b, 0);
		// byte count of LZW data in first subblock.
		if (!_readByte(&b))
			return false;
		int blockLen = MAKEWORD(b, 0) + 1;
		while (blockLen)
		{
			// read in next subblock.
			_sr->fseek(_fi, blockLen - 1, ROFSEEK_ORIGIN_CUR);
			if (!_readByte(&b))
				return false;
			// are we done with the frame?
			if (b == 0)
				break;
			// start a new subblock.
			blockLen = b + 1;
		}
		return true;
	}

protected:
	ScanTag *_sr;
	FILEREADINFO2 *_fi;

	simpleList<GifTextData> _readTextBlock(int maxLen)
	{
		simpleList<GifTextData> vs;
		byte len;
		while (_sr->fread(&len, 1, 1, _fi) == 1
			&& len < maxLen)
		{
			GifTextData s = { 0 };
			char ic;
			int i, imax = MAKEWORD(len, 0);
			for (i = 0; i < imax; i++)
			{
				if (_sr->fread(&ic, 1, 1, _fi) != 1)
					return vs;
				if (ic == 0)
					return vs;
				if (i < sizeof(GifTextData::Data))
					s.Data[i] = ic;
			}
			s.Length = i;
			vs.add(s);
		}
		if (_sr->fread(&len, 1, 1, _fi) != 1 || len != 0)
			vs.clear();
		return vs;
	}
	bool _readByte(LPBYTE buf)
	{
		if (1 == _sr->fread(buf, 1, 1, _fi))
			return true;
		return false;
	}
	bool _peekByte(LPBYTE buf)
	{
		if (1 != _sr->fread(buf, 1, 1, _fi))
			return false;
		_sr->fseek(_fi, -1, ROFSEEK_ORIGIN_CUR);
		return true;
	}
	bool _skipBytes(int len)
	{
		if (_sr->fseek(_fi, len, ROFSEEK_ORIGIN_CUR) == 0)
			return true;
		return false;
	}
	long ftell()
	{
		LARGE_INTEGER pos = _sr->fpos(_fi);
		return (long)pos.LowPart;
	}

	// notification method prototypes
	virtual void onScanGraphicControlExtension(LARGE_INTEGER dataOffset, ULONG dataLen) {}
	virtual void onScanApplicationExtension(LARGE_INTEGER dataOffset, ULONG dataLen, ApplicationExtension2 &ax) {}
	virtual void onScanCommentExtension(LARGE_INTEGER dataOffset, ULONG dataLen) {}
	virtual void onScanPlainTextExtension(LARGE_INTEGER dataOffset, ULONG dataLen, PlainTextExtension2 &ptx) {}
};


class MetaGifDecoder : public MetaGifInfo
{
public:
	MetaGifDecoder(ScanTag *sr, FILEREADINFO2 *fi) :MetaGifInfo(sr, fi) {}

	virtual bool readImageFrame()
	{
		/* DisposalMethod:
		0 - No disposal specified. The decoder is not required to take any action.
		1 - Do not dispose. The graphic is to be left in place.
		2 - Restore to background color. The area used by the graphic must be restored to the background color.
		3 - Restore to previous. The decoder is required to restore the area overwritten by the graphic with what was there prior to rendering the graphic.
		4-7 - To be defined.
		*/
		//cout << "Scanning GIF image frame " << frames << endl;
		int res;
		if (_gcx.GraphicControlLabel == GIFEXTID_GraphicControlExtension &&
			_gcx.PackedFields.DisposalMethod == 1)
		{
			_applyTransparencyToColorTable();
			res = _decompressImageFrame();
			//_removeTransparencyFromColorTable();
		}
		else
		{
			res = _decompressImageFrame();
		}
		if (res != 0)
			return false;
		return true;
	}

protected:
	int _group;
	int _blockLen, _bitPos, _byteCount;
	int _xStart, _yStart, _xEnd, _yEnd, _x, _y;
	simpleList<BYTE> _vpels;

	void _applyTransparencyToColorTable()
	{
		if (_gifinfo.hasTransparency)
		{
			if (_gifinfo.transpIndex >= (int)_vpal.size())
				_vpal.resize(_gifinfo.transpIndex + 1);
			_vpal[_gifinfo.transpIndex].rgbReserved = 0;
		}
	}
	void _removeTransparencyFromColorTable()
	{
		if (_gifinfo.hasTransparency)
			_vpal[_gifinfo.transpIndex].rgbReserved = 0xff;
	}
	int _decompressImageFrame()
	{
		int colorIndex, colorMask;
		int codeSize, initCodeSize;
		int clearCode, stopCode;
		int newCode, priorCode, curCode, freeCode, firstFree, maxCode, firstMax;

		_gifinfo.scanlineLen = SCANLINELENGTH_8BPP(_gifinfo.width);
		size_t imageSize = _gifinfo.scanlineLen*_gifinfo.height;

		// allocate a code table of fixed size 2^12. it consists of entries made 
		// of two parts. for each entry, the 'first' part keeps a LZW code. 
		// the other, 'second', keeps a color-table index.
		simpleList<PairIntInt> vdict(1 << 12);
		// allocate the image output buffer.
		if (_vpels.size() != imageSize)
			_vpels.resize(imageSize);

		_group = 1;
		_xStart = _imgdesc.ImageLeft;
		_yStart = _imgdesc.ImageTop;
		_x = _xStart;
		_y = _yStart;
		_xEnd = _imgdesc.ImageWidth + _xStart - 1;
		_yEnd = _imgdesc.ImageHeight + _yStart - 1;

		BYTE b;
		// read minimum LZW code size.
		if (!_readByte(&b))
			return EBADF;
		// Code size = bits per pixel.
		codeSize = MAKEWORD(b, 0);
		// Clear code = 2 raised to code size.
		clearCode = POWEROF2(codeSize);
		// End-of-image code = Clear code + 1
		stopCode = clearCode + 1;
		// First available code = Clear code + 2
		firstFree = clearCode + 2;
		freeCode = firstFree;
		// Starts at <code size>+1 bits per code
		initCodeSize = ++codeSize;
		maxCode = POWEROF2(codeSize);
		firstMax = maxCode;
		colorMask = POWEROF2(_gifinfo.bpp) - 1;

		// byte count of LZW data in first subblock.
		if (!_readByte(&b))
			return EBADF;
		_blockLen = MAKEWORD(b, 0) + 1;
		_bits = 0;
		_byteCount = 0;
		_bitPos = 8;
		priorCode = 0;

#ifdef _TRACE_GIF_DECODE
		_dbg_frameLen = _dbg_fpos;
		_dbg_fpos = ftell();
		_dbg_frameNum++;
		printf("decompressImageFrame start: frame=%d, fpos=%x; codeSize=%d, blockLen=%d\n", _dbg_frameNum, _dbg_fpos, codeSize, _blockLen);
		_dbg_bytereads = 0;
		_dbg_blockreads = 0;
		_dbg_pels = _vpels.data();
#endif//#ifdef _TRACE_GIF_DECODE

		// enter LZW decoder loop.
		size_t n = 0;
		while (++n)
		{
			newCode = _makeCode(codeSize);
#ifdef _TRACE_GIF_DECODE
			printf("LOOP %d) makeCode1: newCode=%x, freeCode=%x (codeSize=%d)\n", n, newCode, freeCode, codeSize);
#endif//#ifdef _TRACE_GIF_DECODE
			if (newCode == stopCode)
			{
				// make sure that the image block is properly terminated.
				if (_readByte(&b) && b == 0)
				{
					// jump over any trailing zeroes.
					while (_peekByte(&b) && b == 0)
						_skipBytes(1);
					break; // All done.
				}
				// image frame not terminated correctly!
				ASSERT(FALSE);
				break;
			}
			// a LIFO buffer for building and holding a decompressed sequence of
			// color indices before they are sent to the output buffer.
			simpleStack<int> vbuf;

			// check clear code.
			if (newCode == clearCode)
			{
				// time to reset everything.
				// go back to initial code size. clear the code dictionary.
				codeSize = initCodeSize;
				maxCode = firstMax;
				freeCode = firstFree;
				newCode = _makeCode(codeSize);
#ifdef _TRACE_GIF_DECODE
				printf("LOOP %d) clearCode: newCode=%x, freeCode=%x (codeSize=%d)\n", n, newCode, freeCode, codeSize);
#endif//#ifdef _TRACE_GIF_DECODE
				colorIndex = newCode & colorMask;
				_setPixel(colorIndex);
				priorCode = newCode;
			}
			else
			{
				curCode = newCode;
				// check for a special case.
				// sometimes, decoder gets a code it has not generated in the dictionary yet.
				// this happens when the encoder processes a sequence of colors that are
				// represented by most recent code plus the latest color index.
				if (newCode >= freeCode)
				{
					// revert to the latest code plus latest color.
#ifdef _TRACE_GIF_DECODE
					if (newCode > freeCode)
						printf(" code not found in code table: priorCode=%x, colorIndex=%x (outPos=%x)\n", n, priorCode, colorIndex, vbuf.size());
#endif//#ifdef _TRACE_GIF_DECODE
					newCode = priorCode;
					vbuf.push(colorIndex);
				}
				// decompress by following the sequence of codes and picking up 
				// corresponding color indices from the dictionary.
				while (newCode > colorMask)
				{
					if (newCode >= (int)vdict.size())
					{
						/*long fpos = ftell();
						cerr << "Image decoder failed: buffer overrun at file offset 0x" << fpos << endl;*/
						ASSERT(FALSE);
						return EOVERFLOW;
					}
					// get the color index for current code. buffer it for outputting later.
					vbuf.push(vdict[newCode].second);
					// get to the next code in the sequence.
					newCode = vdict[newCode].first;
				}
				colorIndex = (newCode & colorMask);
				vbuf.push(colorIndex);
				// output the expanded color index(es) in reverse order.
				while (vbuf.size())
				{
					_setPixel(vbuf.top());
					vbuf.pop();
				}
				// don't add the very first code to the dictionary.
				// wait until the second one is in. then, it will be added.
				if (n == 1)
				{
					priorCode = curCode;
					continue;
				}
				// dictionary (code table) may not have more than 2^12 codes.
				if (freeCode >= (int)vdict.size())
				{
#ifdef _TRACE_GIF_DECODE
					long fpos = ftell();
					printf(">>> color table exhausted at file offset 0x%x\n", fpos);
#endif//#ifdef _TRACE_GIF_DECODE
					/*if (n % 0x100 == 0)
						cout << "x";*/
					// don't bail out though.
					continue;
				}
				// build up the dictionary. 'first' member is either a prior color index,
				// or a code representing a prior sequence of color indices.
				// 'second' is the latest color index.
				vdict[freeCode] = { priorCode, colorIndex };
				// save the new code as prior one.
				priorCode = curCode;
				// get to the next code. if out of space, increase number of bits by one.
				if (++freeCode >= maxCode)
				{
					// code table is limited to a size of 2^12 (0x1000) elements. don't exceed it.
					if (codeSize < 12)
					{
						// can increase the code size by another bit.
						codeSize++;
						// dictionary is doubled in capacity.
						maxCode *= 2;
#ifdef _TRACE_GIF_DECODE
						printf(">>> decomp info: codeSize increased to %d (maxCode=%d)\n", codeSize, maxCode);
#endif//#ifdef _TRACE_GIF_DECODE
					}
					else
					{
#ifdef _TRACE_GIF_DECODE
						long fpos = ftell();
						printf(">>> decomp warning: at code table edge; file offset 0x%x\n", fpos);
#endif//#ifdef _TRACE_GIF_DECODE
						// the next code must be a clear code.
						// otherwise, the freeCode check above will fail.
					}
				}
				/*if (n % 0x100 == 0)
					cout << ".";*/
			}
		}
#ifdef _TRACE_GIF_DECODE
		_dbg_fpos = ftell();
		_dbg_frameLen = _dbg_fpos - _dbg_frameLen;
		_dbg_blockreads += _dbg_bytereads;
		printf("decompressImageFrame stop: reads=%x/%x\n fpos=%x (%d), frame=%d (compressed=%x | expanded=%x)\n", _dbg_bytereads, _dbg_blockreads, _dbg_fpos, _dbg_fpos, _dbg_frameNum, _dbg_frameLen, _vpels.size());
#endif//#ifdef _TRACE_GIF_DECODE
		/*cout << endl;*/
		return 0;
	}
	/*
	The rows of an Interlaced images are arranged in the following order:
		  Group 1 : Every 8th. row, starting with row 0.              (Pass 1)
		  Group 2 : Every 8th. row, starting with row 4.              (Pass 2)
		  Group 3 : Every 4th. row, starting with row 2.              (Pass 3)
		  Group 4 : Every 2nd. row, starting with row 1.              (Pass 4)
	*/
	int _setPixel(size_t colorIndex)
	{
		ASSERT(colorIndex < _vpal.size());
		LPBYTE dest = (_vpels.data() + _gifinfo.scanlineLen*(_gifinfo.height - _y - 1)) + _x;
		*dest = LOBYTE(colorIndex);

		// Move to the next pixel position.
		_x++;
		if (_x > _xEnd)
		{
			_x = _xStart;
			if (_imgdesc.PackedFields.InterlaceFlag)
			{
				// Interlaced. Skip lines.
				_y += (_group == 1 ? 8 : _group == 2 ? 8 : _group == 3 ? 4 : 2);
				if (_y > _yEnd)
				{
					_group++;
					if (_group > 4)
						_group = 1;
					_y = _yStart + (_group == 2 ? 4 : _group == 3 ? 2 : 1);
				}
			}
			else
				_y++; // Non-interlaced.
		}
		return 0;
	}

	/* gif packs the bits with the LSB first.
	*/
	int _makeCode(int codeSize)
	{
		int code = 0;
		for (int i = 0; i < codeSize; i++)
			code += _getBit() << i;
		return code;
	}

	// this bit buffer is for exclusive use by _getBit() and for a limited access by
	// _decompressImageFrame(). the latter resets/clears the buffer in its variable initialization.
	BYTE _bits;
	// get the next code bit from the encoded lzw code stream.
	int _getBit()
	{
		if (++_bitPos == 9)
		{
			// read the next byte from the file stream.
			_readByte(&_bits);
			_bitPos = 1;
			_byteCount++;
#ifdef _TRACE_GIF_DECODE
			printf("_readByte1: in=%x  @ %d/%d\n", _bits, _byteCount, _blockLen);
#endif//#ifdef _TRACE_GIF_DECODE
			// see if having reached the end of the current block. it's typically 256 (0x100) bytes.
			if (_byteCount == _blockLen)
			{
				// time to start a new block.
				_blockLen = _bits + 1;
#ifdef _TRACE_GIF_DECODE
				_dbg_blockreads += _dbg_bytereads;
				printf("getBit: in=%x; reads=%x/%x; nextBlockLen=%x\n", _bits, _dbg_bytereads, _dbg_blockreads, _blockLen);
				_dbg_bytereads = 0;
#endif//#ifdef _TRACE_GIF_DECODE
				_readByte(&_bits);
				_byteCount = 1;
#ifdef _TRACE_GIF_DECODE
				printf("_readByte2: in=%x  @ %d/%d\n", _bits, _byteCount, _blockLen);
#endif//#ifdef _TRACE_GIF_DECODE
			}
		}
		// isolate the code bit.
		return ((_bits & POWEROF2(_bitPos - 1)) ? 1 : 0);
	}

	HBITMAP createImageBitmap() const
	{
		if (_gifinfo.hasTransparency)
			return _copyToBmp32bppV4();
		else
			return _copyToBmp8bpp();
	}

	HBITMAP _copyToBmp8bpp() const
	{
		// always allocates space for 256 (2^8) colors regardless of the actual number of colors in use (_vpal.size()).
		size_t palLen = sizeof(RGBQUAD) * 256;
		LPBITMAPINFO pbmi = (LPBITMAPINFO)malloc(sizeof(BITMAPINFOHEADER) + palLen);
		memset(pbmi, 0, sizeof(BITMAPINFOHEADER) + palLen);
		pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		pbmi->bmiHeader.biWidth = _gifinfo.width;
		pbmi->bmiHeader.biHeight = _gifinfo.height;
		pbmi->bmiHeader.biBitCount = 8; // always 8 bpp. don't set it to _gifinfo.bpp.
		pbmi->bmiHeader.biClrImportant = _gifinfo.colors; // actual number of colors. gif lets you use a palette of 2, 4, 8, 16, 32, 64, 128, and 256 colors.
		pbmi->bmiHeader.biClrUsed = 256; // set it to the maximum number of colors
		pbmi->bmiHeader.biPlanes = 1;
		pbmi->bmiHeader.biSizeImage = (DWORD)_vpels.size();
		pbmi->bmiHeader.biCompression = BI_RGB;

		DBGPRINTF((L"_copyToBmp8bpp: %d, %d (%d); SizeImage=%d (top-down)\n", _gifinfo.width, _gifinfo.height, pbmi->bmiHeader.biHeight, pbmi->bmiHeader.biSizeImage));

		memcpy(pbmi->bmiColors, _vpal.data(), palLen);

		HDC hdc0 = CreateDC(L"DISPLAY", NULL, NULL, NULL);
		HDC hdc = CreateCompatibleDC(hdc0);
		SetDIBColorTable(hdc, 0, pbmi->bmiHeader.biClrUsed, pbmi->bmiColors);
		LPBYTE bits = NULL;
		HBITMAP hbmp = CreateDIBSection(hdc, pbmi, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, 0);
		if (hbmp)
		{
			HBITMAP hbmp0 = (HBITMAP)SelectObject(hdc, hbmp);
			CopyMemory(bits, _vpels.data(), _vpels.size());
			SetDIBits(hdc, hbmp, 0, pbmi->bmiHeader.biHeight, bits, pbmi, DIB_RGB_COLORS);
			if (hbmp)
				RefitBitmapToViewFrame(hdc, _sr->_bmv->_vi.FrameSize, false, &hbmp);
			SelectObject(hdc, hbmp0);
		}
		DeleteDC(hdc);
		DeleteDC(hdc0);
		return hbmp;
	}
	HBITMAP _copyToBmp32bppV4() const
	{
		int scanlineLen2 = SCANLINELENGTH_32BPP(_gifinfo.width);

		BITMAPV4HEADER bmi = { 0 };
		LPBITMAPINFO pbmi = (LPBITMAPINFO)&bmi;
		bmi.bV4Size = sizeof(BITMAPV4HEADER);
		bmi.bV4Width = _gifinfo.width;
		bmi.bV4Height = _gifinfo.height;
		bmi.bV4Planes = 1;
		bmi.bV4BitCount = 32;
		bmi.bV4V4Compression = BI_BITFIELDS;
		bmi.bV4SizeImage = scanlineLen2 * _gifinfo.height;

		bmi.bV4RedMask = 0x00FF0000;
		bmi.bV4GreenMask = 0x0000FF00;
		bmi.bV4BlueMask = 0x00000FF;
		bmi.bV4AlphaMask = 0xFF000000;

		bmi.bV4CSType = LCS_WINDOWS_COLOR_SPACE;// 0x206e6957 or 'Win '

		DBGPRINTF((L"_copyToBmp32bppV4: %d, %d (%d); SizeImage=%d (top-down)\n", _gifinfo.width, _gifinfo.height, bmi.bV4Height, bmi.bV4SizeImage));

		LPBYTE pels2 = new BYTE[bmi.bV4SizeImage];

		const BYTE *src = _vpels.data();
		BYTE *dest = pels2;

		for (int i = 0; i < _gifinfo.height; i++)
		{
			for (int j = 0; j < _gifinfo.width; j++)
			{
				int k = src[j];
				RGBQUAD rgb = _vpal[k];
				*((RGBQUAD*)dest + j) = rgb;
			}
			src += _gifinfo.scanlineLen;
			dest += scanlineLen2;
		}

		HDC hdc0 = CreateDC(L"DISPLAY", NULL, NULL, NULL);
		HDC hdc = CreateCompatibleDC(hdc0);
		LPBYTE bits = NULL;
		HBITMAP hbmp = CreateDIBSection(hdc, pbmi, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, 0);
		HBITMAP hbmp0 = (HBITMAP)SelectObject(hdc, hbmp);
		RECT rc = { 0,0,_gifinfo.width ,_gifinfo.height };
		FillRect(hdc, &rc, GetStockBrush(WHITE_BRUSH));
		CopyMemory(bits, pels2, bmi.bV4SizeImage);
		SetDIBits(hdc, hbmp, 0, pbmi->bmiHeader.biHeight, bits, pbmi, DIB_RGB_COLORS);
		if (hbmp)
			RefitBitmapToViewFrame(hdc, _sr->_bmv->_vi.FrameSize, false, &hbmp);
		SelectObject(hdc, hbmp0);
		DeleteDC(hdc);
		DeleteDC(hdc0);

		delete[] pels2;
		return hbmp;
	}
	/*
	int _copyToBmp32bpp(BITMAPIMAGE &bi) const
	{
		int scanlineLen2 = SCANLINELENGTH_32BPP(_gifinfo.width);
		bi.pels.resize(scanlineLen2*_gifinfo.height);
		bi.hdr.resize(sizeof(BITMAPINFOHEADER));
		LPBITMAPINFO pbmi = (LPBITMAPINFO)bi.hdr.data();
		pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		pbmi->bmiHeader.biWidth = _gifinfo.width;
		pbmi->bmiHeader.biHeight = _gifinfo.height;
		pbmi->bmiHeader.biBitCount = 32;
		pbmi->bmiHeader.biClrUsed = 0;
		pbmi->bmiHeader.biPlanes = 1;
		pbmi->bmiHeader.biSizeImage = (DWORD)bi.pels.size();
		pbmi->bmiHeader.biCompression = BI_RGB;

		const byte *src = _vpels.data();
		byte *dest = bi.pels.data();

		for (int i = 0; i < _gifinfo.height; i++)
		{
			for (int j = 0; j < _gifinfo.width; j++)
			{
				int k = src[j];
				RGBQUAD rgb = _vpal[k];
				*((RGBQUAD*)dest + j) = rgb;
			}
			src += _gifinfo.scanlineLen;
			dest += scanlineLen2;
		}
		return 0;
	}
	*/
};

class MetaGifScan : public MetaGifDecoder // MetaGifInfo
{
public:
	MetaGifScan(ScanTag *sr, FILEREADINFO2 *fi) : MetaGifDecoder(sr, fi) {}

	int collect()
	{
		if (!checkSig())
			return -1;
		_sr->_bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST, _fi->PriorStart, _fi->PriorLength, L"GIF version %s", _gifinfo.is89a ? L"89a" : L"87a");

		if (!readScreenDesc())
			return -1;
		int regionId = _sr->_bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST, _fi->PriorStart, _fi->PriorLength, L"GIF Image\n Dimensions: %d x %d\n Bit depth: %d BPP (%d colors)\n Background palette index: %d", _gifinfo.width, _gifinfo.height, _gifinfo.bpp, _gifinfo.colors, _gifinfo.bkgdIndex);

		LARGE_INTEGER offset = _fi->StartOffset;
		if (!readColorTable(_gifinfo.colors, _vpalGlobal))
			return -1;
		ULONG len = _fi->StartOffset.LowPart - offset.LowPart;
		_sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND | BMVADDANNOT_FLAG_GRAYTEXT, regionId), offset, len, L"Global Color Palette");

		int frames = 0;
		while (!atEOF())
		{
			if (_sr->isKilled(&_fi->ErrorCode))
				return -1;

			if (!readExtensionBlocks())
				return -1;
			offset = _fi->StartOffset;
			if (!readImageDesc())
				return -1;
			frames++;
			len = _fi->StartOffset.LowPart - offset.LowPart;
			regionId = _sr->_bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST, offset, sizeof(_imgdesc), L"Image Descriptor %d\n Position: Left %d, Top %d\n Dimensions: %d x %d\n LocalColorTable: %d\n Interlace: %d\n Sort: %d", frames, _imgdesc.ImageLeft, _imgdesc.ImageTop, _imgdesc.ImageWidth, _imgdesc.ImageHeight, _imgdesc.PackedFields.LocalColorTableFlag ? POWEROF2(_imgdesc.PackedFields.SizeOfLocalColorTable + 1) : 0, _imgdesc.PackedFields.InterlaceFlag, _imgdesc.PackedFields.SortFlag);
			if (len > sizeof(_imgdesc))
			{
				// local palette exists.
				offset.QuadPart += sizeof(_imgdesc);
				len -= sizeof(_imgdesc);
				_sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND, regionId), offset, len, L"Local Color Palette");
			}

			offset = _fi->StartOffset;
			if (!readImageFrame())
				return -1;
			len = _fi->StartOffset.LowPart - offset.LowPart;
			int regionId = _sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST | BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND | BMVADDANNOT_FLAG_GRAYTEXT, regionId), offset, len, L"Image Frame %d", frames);
			_annotateImage(regionId, frames);

			if (_gcx.PackedFields.DisposalMethod == 1)
				_removeTransparencyFromColorTable();
		}
		_sr->_bmv->addTaggedRegion(0, { _fi->StartOffset.LowPart - 1 }, 1, L"GIF Trailer", frames);
		return frames;
	}

protected:
	bool _annotateImage(int regionId, int frameIndex)
	{
		if (regionId == 0)
			return false;
		CRegionCollection &rc = _sr->_bmv->_regions;
		AnnotationCollection &ac = _sr->_bmv->_annotations;
		int annotId = ac.queryByCtrlId(rc[regionId - 1].AnnotCtrlId);
		if (annotId == -1)
			return false;
		HBITMAP hbmp = createImageBitmap();
		if (!hbmp)
			return false;
		AnnotationOperation annot(ac[annotId], &ac);
		annot.attachThumbnail(hbmp);
		return true;
	}

	LPCWSTR _GifDisposalMethod(BYTE disposalMethod)
	{
		switch (disposalMethod)
		{
		case 0: return L"No action required";
		case 1: return L"Do not dispose";
		case 2: return L"Restore to background color";
		case 3: return L"Restore to previous";
		}
		return L"(Unknown)";
	}
	// notification methods
	virtual void onScanGraphicControlExtension(LARGE_INTEGER dataOffset, ULONG dataLen)
	{
		_sr->_bmv->addTaggedRegion(0, dataOffset, dataLen, L"Graphic Control Extension\n User input: %s\n Disposal method: %s\n Transparency: %s (Palette index %d)", _gcx.PackedFields.UserInputFlag ? L"Yes" : L"No", _GifDisposalMethod(_gcx.PackedFields.DisposalMethod), _gcx.PackedFields.TransparentColorFlag ? L"Yes" : L"No", _gcx.PackedFields.TransparentColorFlag ? _gcx.TransparentColorIndex : 0);
	}
	virtual void onScanApplicationExtension(LARGE_INTEGER dataOffset, ULONG dataLen, ApplicationExtension2 &ax)
	{
		_sr->_bmv->addTaggedRegion(0, dataOffset, dataLen, L"Application Extension\n Identifier: %s\n Auth code: %s\n App data length: %d", (LPCWSTR)ustring(CP_ACP, ax.AppIdentifier, 8), (LPCWSTR)ustring(CP_ACP, (char*)ax.AppAuthenticationCode, 3), ax.Bin.Length);
	}
	virtual void onScanCommentExtension(LARGE_INTEGER dataOffset, ULONG dataLen)
	{
		_sr->_bmv->addTaggedRegion(0, _fi->PriorStart, _fi->PriorLength, L"Comment Extension\n Text: %d bytes", _vcomments[_vcomments.size() - 1].Length);
	}
	virtual void onScanPlainTextExtension(LARGE_INTEGER dataOffset, ULONG dataLen, PlainTextExtension2 &ptx)
	{
		_sr->_bmv->addTaggedRegion(0, _fi->PriorStart, _fi->PriorLength, L"Text Extension\n Grid: L=%d T=%d W=%d H=%d\n Cell: W=%d H=%d\n Text: %d bytes", ptx.TextGridLeftPos, ptx.TextGridTopPos, ptx.TextGridWidth, ptx.TextGridHeight, ptx.CharCellWidth, ptx.CharCellHeight, ptx.Text.Length);
	}
};

DWORD ScanTagGif::runInternal(FILEREADINFO2 *fi)
{
	MetaGifScan gifscan(this, fi);
	gifscan.collect();

	return fi->ErrorCode;
}


//////////////////////////////////////////////////////////////

/* this document gives a summary on jpeg segments and metadata standards
https://dev.exiv2.org/projects/exiv2/wiki/The_Metadata_in_JPEG_files
*/


#pragma pack(1)
struct EXIF_TIFFHEADER
{
	USHORT ByteOrder; // 0x4949=little endian; 0x4D4D=big endian
	USHORT Signature; // always set to 0x002A
	ULONG IFDOffset; // offset to 0th IFD. If the TIFF header is followed immediately by the 0th IFD, it is written as 00000008.
};

// data type values for EXIF_IFD.Type
enum EXIF_IFD_DATATYPE
{
	IFD_UNKNOWN,
	IFD_BYTE,
	IFD_ASCII,
	IFD_USHORT,
	IFD_ULONG,
	IFD_ULONGLONG,
	IFD_CHAR,
	IFD_UNDEFINED,
	IFD_SHORT,
	IFD_LONG,
	IFD_LONGLONG,
	IFD_FLOAT,
	IFD_DOUBLE,
	IFD_MAX
};
struct EXIF_IFD
{
	USHORT Tag;
	USHORT Type; // 1=byte, 2=ascii, 3=short (2 bytes), 4=long (4 bytes), 5=rational (2 longs), 7=undefined, 9=signed long, 10=signed rational (2 slongs).
	ULONG Count; // number of elements of data type Type recorded in either ValueOrOffset or in a memory location pointed to by ValueOrOffset plus the starting address of the TIFFHeader.
	ULONG ValueOrOffset; // This tag records the offset from the start of the TIFF header to the position where the value itself is recorded. In cases where the value fits in 4 bytes, the value itself is recorded.If the value is smaller than 4 bytes, the value is stored in the 4 - byte area starting from the left, i.e., from the lower end of the byte offset area.For example, in big endian format, if the type is SHORT and the value is 1, it is recorded as 00010000.H.
};
#pragma pack()


// exif (exchangeable image file format) metadata parser
// ifd (image file directory)

class MetadataExifIfd;
class MetadataExifIfdAttribute
{
public:
	MetadataExifIfdAttribute(EXIF_IFD* header, MetadataExifIfd* coll);
	~MetadataExifIfdAttribute();

	int parse();
	astring getText() const;

	MetadataExifIfd *_coll;
	int _offset;
	int _headerLen;
	int _valueOffset; // an offset to where the value data is located; relative to _coll->_base->_basePtr. the value's length is in _value._length.
	EXIF_IFD _ifd;
	astring _name;
	simpleList<BYTE> _value;

	LPCSTR _IfdDataType()
	{
		switch (_ifd.Type)
		{
		case IFD_BYTE: return "BYTE";
		case IFD_ASCII: return "ASCII";
		case IFD_CHAR: return "CHAR";
		case IFD_UNDEFINED: return "(UNDEFINED)";
		case IFD_USHORT: return "USHORT";
		case IFD_SHORT: return "SHORT";
		case IFD_ULONG: return "ULONG";
		case IFD_LONG: return "LONG";
		case IFD_FLOAT: return "FLOAT";
		case IFD_ULONGLONG: return "ULONGLONG";
		case IFD_LONGLONG: return "LONGLONG";
		case IFD_DOUBLE: return "DOUBLE";
		}
		return "(UNKNOWN)";
	}

	LPCSTR _IfdTagName()
	{
		switch (_ifd.Tag)
		{
			// primary tags
		case 0x100: return "ImageWidth";
		case 0x101: return "ImageLength";
		case 0x102: return "BitsPerSample";
		case 0x103: return "Compression";
		case 0x106: return "PhotometricInterpretation";
		case 0x10E: return "ImageDescription";
		case 0x10F: return "Make";
		case 0x110: return "Model";
		case 0x111: return "StripOffsets";
		case 0x112: return "Orientation";
		case 0x115: return "SamplesPerPixel";
		case 0x116: return "RowsPerStrip";
		case 0x117: return "StripByteCounts";
		case 0x11A: return "XResolution";
		case 0x11B: return "YResolution";
		case 0x11C: return "PlanarConfiguration";
		case 0x128: return "ResolutionUnit";
		case 0x12D: return "TransferFunction";
		case 0x131: return "Software";
		case 0x132: return "DateTime";
		case 0x13B: return "Artist";
		case 0x13E: return "WhitePoint";
		case 0x13F: return "PrimaryChromaticities";
		case 0x201: return "JPEGInterchangeFormat";
		case 0x202: return "JPEGInterchangeFormatLength";
		case 0x211: return "YCbCrCoefficients";
		case 0x212: return "YCbCrSubSampling";
		case 0x213: return "YCbCrPositioning";
		case 0x214: return "ReferenceBlackWhite";
		case 0x8298: return "Copyright";
		case 0x8769: return "OffsetToExifIFD";
		case 0x8825: return "OffsetToGpsIFD";
			// EXIF tags
		case 0x829A: return "ExposureTime";
		case 0x829D: return "FNumber";
		case 0x8822: return "ExposureProgram";
		case 0x8824: return "SpectralSensitivity";
		case 0x8827: return "ISOSpeedRatings";
		case 0x8828: return "OECF";
		case 0x9000: return "ExifVersion";
		case 0x9003: return "DateTimeOriginal";
		case 0x9004: return "DateTimeDigitized";
		case 0x9101: return "ComponentsConfiguration";
		case 0x9102: return "CompressedBitsPerPixel";
		case 0x9201: return "ShutterSpeedValue";
		case 0x9202: return "ApertureValue";
		case 0x9203: return "BrightnessValue";
		case 0x9204: return "ExposureBiasValue";
		case 0x9205: return "MaxApertureValue";
		case 0x9206: return "SubjectDistance";
		case 0x9207: return "MeteringMode";
		case 0x9208: return "LightSource";
		case 0x9209: return "Flash";
		case 0x920A: return "FocalLength";
		case 0x9214: return "SubjectArea";
		case 0x927C: return "MakerNote";
		case 0x9286: return "UserComment";
		case 0x9290: return "SubsecTime";
		case 0x9291: return "SubsecTimeOriginal";
		case 0x9292: return "SubsecTimeDigitized";
		case 0xA000: return "FlashpixVersion";
		case 0xA001: return "ColorSpace";
		case 0xA002: return "PixelXDimension";
		case 0xA003: return "PixelYDimension";
		case 0xA004: return "RelatedSoundFile";
		case 0xA20B: return "FlashEnergy";
		case 0xA20C: return "SpatialFrequencyResponse";
		case 0xA20E: return "FocalPlaneXResolution";
		case 0xA20F: return "FocalPlaneYResolution";
		case 0xA210: return "FocalPlaneResolutionUnit";
		case 0xA214: return "SubjectLocation";
		case 0xA215: return "ExposureIndex";
		case 0xA217: return "SensingMethod";
		case 0xA300: return "FileSource";
		case 0xA301: return "SceneType";
		case 0xA302: return "CFAPattern";
		case 0xA401: return "CustomRendered";
		case 0xA402: return "ExposureMode";
		case 0xA403: return "WhiteBalance";
		case 0xA404: return "DigitalZoomRatio";
		case 0xA405: return "FocalLengthIn35mmFilm";
		case 0xA406: return "SceneCaptureType";
		case 0xA407: return "GainControl";
		case 0xA408: return "Contrast";
		case 0xA409: return "Saturation";
		case 0xA40A: return "Sharpness";
		case 0xA40B: return "DeviceSettingDescription";
		case 0xA40C: return "SubjectDistanceRange";
		case 0xA420: return "ImageUniqueID";
			// GPS tags
		case 0x0: return "GPSVersionID";
		case 0x1: return "GPSLatitudeRef";
		case 0x2: return "GPSLatitude";
		case 0x3: return "GPSLongitudeRef";
		case 0x4: return "GPSLongitude";
		case 0x5: return "GPSAltitudeRef";
		case 0x6: return "GPSAltitude";
		case 0x7: return "GPSTimestamp";
		case 0x8: return "GPSSatellites";
		case 0x9: return "GPSStatus";
		case 0xA: return "GPSMeasureMode";
		case 0xB: return "GPSDOP";
		case 0xC: return "GPSSpeedRef";
		case 0xD: return "GPSSpeed";
		case 0xE: return "GPSTrackRef";
		case 0xF: return "GPSTrack";
		case 0x10: return "GPSImgDirectionRef";
		case 0x11: return "GPSImgDirection";
		case 0x12: return "GPSMapDatum";
		case 0x13: return "GPSDestLatitudeRef";
		case 0x14: return "GPSDestLatitude";
		case 0x15: return "GPSDestLongitudeRef";
		case 0x16: return "GPSDestLongitude";
		case 0x17: return "GPSDestBearingRef";
		case 0x18: return "GPSDestBearing";
		case 0x19: return "GPSDestDistanceRef";
		case 0x1A: return "GPSDestDistance";
		case 0x1B: return "GPSProcessingMethod";
		case 0x1C: return "GPSAreaInformation";
		case 0x1D: return "GPSDateStamp";
		case 0x1E: return "GPSDifferential";
		}

		static char tagname[16];
		sprintf(tagname, "Tag %d", _ifd.Tag);
		return tagname;
	}

	enum EXIFTAG
	{
		// primary tags
		ImageWidth = 0x100,
		ImageLength = 0x101,
		BitsPerSample = 0x102,
		Compression = 0x103,
		PhotometricInterpretation = 0x106,
		ImageDescription = 0x10E,
		Make = 0x10F,
		Model = 0x110,
		StripOffsets = 0x111,
		Orientation = 0x112,
		SamplesPerPixel = 0x115,
		RowsPerStrip = 0x116,
		StripByteCounts = 0x117,
		XResolution = 0x11A,
		YResolution = 0x11B,
		PlanarConfiguration = 0x11C,
		ResolutionUnit = 0x128,
		TransferFunction = 0x12D,
		Software = 0x131,
		DateTime = 0x132,
		Artist = 0x13B,
		WhitePoint = 0x13E,
		PrimaryChromaticities = 0x13F,
		JPEGInterchangeFormat = 0x201,
		JPEGInterchangeFormatLength = 0x202,
		YCbCrCoefficients = 0x211,
		YCbCrSubSampling = 0x212,
		YCbCrPositioning = 0x213,
		ReferenceBlackWhite = 0x214,
		Copyright = 0x8298,
		OffsetToExifIFD = 0x8769,
		OffsetToGpsIFD = 0x8825,

		// EXIF tags
		ExposureTime = 0x829A,
		FNumber = 0x829D,
		ExposureProgram = 0x8822,
		SpectralSensitivity = 0x8824,
		ISOSpeedRatings = 0x8827,
		OECF = 0x8828,
		ExifVersion = 0x9000,
		DateTimeOriginal = 0x9003,
		DateTimeDigitized = 0x9004,
		ComponentsConfiguration = 0x9101,
		CompressedBitsPerPixel = 0x9102,
		ShutterSpeedValue = 0x9201,
		ApertureValue = 0x9202,
		BrightnessValue = 0x9203,
		ExposureBiasValue = 0x9204,
		MaxApertureValue = 0x9205,
		SubjectDistance = 0x9206,
		MeteringMode = 0x9207,
		LightSource = 0x9208,
		Flash = 0x9209,
		FocalLength = 0x920A,
		SubjectArea = 0x9214,
		MakerNote = 0x927C,
		UserComment = 0x9286,
		SubsecTime = 0x9290,
		SubsecTimeOriginal = 0x9291,
		SubsecTimeDigitized = 0x9292,
		FlashpixVersion = 0xA000,
		ColorSpace = 0xA001,
		PixelXDimension = 0xA002,
		PixelYDimension = 0xA003,
		RelatedSoundFile = 0xA004,
		FlashEnergy = 0xA20B,
		SpatialFrequencyResponse = 0xA20C,
		FocalPlaneXResolution = 0xA20E,
		FocalPlaneYResolution = 0xA20F,
		FocalPlaneResolutionUnit = 0xA210,
		SubjectLocation = 0xA214,
		ExposureIndex = 0xA215,
		SensingMethod = 0xA217,
		FileSource = 0xA300,
		SceneType = 0xA301,
		CFAPattern = 0xA302,
		CustomRendered = 0xA401,
		ExposureMode = 0xA402,
		WhiteBalance = 0xA403,
		DigitalZoomRatio = 0xA404,
		FocalLengthIn35mmFilm = 0xA405,
		SceneCaptureType = 0xA406,
		GainControl = 0xA407,
		Contrast = 0xA408,
		Saturation = 0xA409,
		Sharpness = 0xA40A,
		DeviceSettingDescription = 0xA40B,
		SubjectDistanceRange = 0xA40C,
		ImageUniqueID = 0xA420,

		// GPS tags
		GPSVersionID = 0x0,
		GPSLatitudeRef = 0x1,
		GPSLatitude = 0x2,
		GPSLongitudeRef = 0x3,
		GPSLongitude = 0x4,
		GPSAltitudeRef = 0x5,
		GPSAltitude = 0x6,
		GPSTimestamp = 0x7,
		GPSSatellites = 0x8,
		GPSStatus = 0x9,
		GPSMeasureMode = 0xA,
		GPSDOP = 0xB,
		GPSSpeedRef = 0xC,
		GPSSpeed = 0xD,
		GPSTrackRef = 0xE,
		GPSTrack = 0xF,
		GPSImgDirectionRef = 0x10,
		GPSImgDirection = 0x11,
		GPSMapDatum = 0x12,
		GPSDestLatitudeRef = 0x13,
		GPSDestLatitude = 0x14,
		GPSDestLongitudeRef = 0x15,
		GPSDestLongitude = 0x16,
		GPSDestBearingRef = 0x17,
		GPSDestBearing = 0x18,
		GPSDestDistanceRef = 0x19,
		GPSDestDistance = 0x1A,
		GPSProcessingMethod = 0x1B,
		GPSAreaInformation = 0x1C,
		GPSDateStamp = 0x1D,
		GPSDifferential = 0x1E
	};
};

class MetadataExifIfd : public objList<MetadataExifIfdAttribute>
{
public:
	MetadataExifIfd(LPCSTR name, MetadataEndian *base) : _name(name), _base(base), _offset(0), _headerLen(0), _directoryLen(0), _dataLen(0), _totalLen(0) {}

	MetadataEndian *_base; // use _base->_basePtr to get the starting address of the TIFF header.
	astring _name; // name of an exif attribute tag; to be set by parse().
	int _offset; // an offset of the ifd header measured from the app1 tiff header.
	int _headerLen; // a 2-byte directory size specifier.
	int _directoryLen; // data fields represented by instances of MetadataExifIfdAttribute.
	int _dataLen; // size of a space that follows the directory fields and holds the values of attributes specified in the fields.
	int _totalLen; // = _headerLen+_directoryLen+_dataLen

	// use this 'at' instead of the [] operator of objList if the attribute's _coll may be obsolete. this method corrects it by redirecting the attribute to us.
	MetadataExifIfdAttribute *at(int pos)
	{
		MetadataExifIfdAttribute *p = (*this)[pos];
		// make sure _coll points to this instance.
		p->_coll = this;
		return p;
	}

	int parse(int dataOffset, int remainingLen)
	{
		__try
		{
			return _parse(dataOffset, remainingLen);
		}
		__except (_filterParserException(GetExceptionCode(), GetExceptionInformation()))
		{
			return -1;
		}
	}

	MetadataExifIfdAttribute *find(USHORT tagId) const
	{
		for (int i = 0; i < (int)size(); i++)
		{
			if ((*this)[i]->_ifd.Tag == tagId)
				return (*this)[i];
		}
		return NULL;
	}

	int _getDataUnitLength(EXIF_IFD_DATATYPE dtype) const
	{
		switch (dtype)
		{
		case IFD_BYTE:
		case IFD_ASCII:
		case IFD_CHAR:
		case IFD_UNDEFINED:
			return 1;
		case IFD_USHORT:
		case IFD_SHORT:
			return 2;
		case IFD_ULONG:
		case IFD_LONG:
		case IFD_FLOAT:
			return 4;
		case IFD_ULONGLONG:
		case IFD_LONGLONG:
		case IFD_DOUBLE:
			return 8;
		}
		return 0;
	}

protected:
	int _parse(int dataOffset, int remainingLen)
	{
		if (dataOffset < 0 || remainingLen <= 0)
			return 0;
		LPBYTE src = _base->_basePtr + dataOffset;
		int len, usedLen = sizeof(USHORT);
		USHORT i, tagCount;
		LPBYTE p = src + sizeof(USHORT);
		int endingOffset = 0, nextEnding;
		tagCount = _base->getUSHORT(src);
		for (i = 0; i < tagCount; i++)
		{
			MetadataExifIfdAttribute *next = new MetadataExifIfdAttribute((EXIF_IFD*)p, this);
			// note that len does not include the data bytes.
			len = next->parse();
			objList<MetadataExifIfdAttribute>::add(next);
			usedLen += len;
			p += len;
			nextEnding = next->_valueOffset + (int)next->_value.size();
			if (nextEnding > endingOffset)
				endingOffset = nextEnding;
		}
		_offset = dataOffset;
		_headerLen = sizeof(tagCount);
		_directoryLen = usedLen - _headerLen;
		if (endingOffset)
			_totalLen = endingOffset - dataOffset;
		else
			_totalLen = usedLen;
		_dataLen = _totalLen - _headerLen - _directoryLen;
		return usedLen; // return the directory length.
	}
	int _filterParserException(unsigned int code, struct _EXCEPTION_POINTERS *ep)
	{
		// code: EXCEPTION_ACCESS_VIOLATION
		char msg[128] = { 0 };
		sprintf_s(msg, sizeof(msg), "HEXDUMPTAB> Exception 0x%X at %p\n", code, (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionAddress : 0);
		OutputDebugStringA(msg);
		return EXCEPTION_EXECUTE_HANDLER;
	}
};

MetadataExifIfdAttribute::MetadataExifIfdAttribute(EXIF_IFD* header, MetadataExifIfd* coll) : _ifd(*header), _coll(coll), _offset(0), _headerLen(0), _valueOffset(0)
{
	_ifd.Tag = _coll->_base->getUSHORT(&_ifd.Tag);
	_ifd.Type = _coll->_base->getUSHORT(&_ifd.Type);
	_ifd.Count = _coll->_base->getULONG(&_ifd.Count);
	// defer endian translation for ValueOrOffset until parse().
	_offset = (int)((LPBYTE)header - _coll->_base->_basePtr);
}

MetadataExifIfdAttribute::~MetadataExifIfdAttribute()
{
}

int MetadataExifIfdAttribute::parse()
{
	LPBYTE p;
	int valLen = _ifd.Count*_coll->_getDataUnitLength((EXIF_IFD_DATATYPE)_ifd.Type);
	if (valLen <= 4)
	{
		p = (LPBYTE)&_ifd.ValueOrOffset;
		_value.add(p, valLen);
	}
	else
	{
		_ifd.ValueOrOffset = _coll->_base->getULONG(&_ifd.ValueOrOffset);
		p = _coll->_base->_basePtr + _ifd.ValueOrOffset;
		_value.add(p, valLen);
		// save the value offset. MetaJpegScan will be use it to create a region in gray that's associated with this ifd attribute entry.
		_valueOffset = (int)_ifd.ValueOrOffset;
	}
	_name.format("%s (%s, %d elements)", _IfdTagName(), _IfdDataType(), _ifd.Count);
	DBGPRINTF((L"MetadataExifIfdAttribute @ offset 0x%X: Tag=%d, DataType=%d, Count=%d\n", _offset, _ifd.Tag, _ifd.Type, _ifd.Count));
	DBGDUMP(("MetadataExifIfdAttribute.Value", _value.data(), (int)_value.size()));
	_headerLen = sizeof(EXIF_IFD);
	return _headerLen;
}

astring MetadataExifIfdAttribute::getText() const
{
	if (_ifd.Type == IFD_ASCII || _ifd.Type == IFD_CHAR)
		return astring((LPCSTR)_value.data(), (int)_value.size());
	if (_ifd.Type == IFD_BYTE || _ifd.Type == IFD_UNDEFINED)
	{
		astring2 t;
		if (_value.size() <= 2 * 16 + 8)
			t.hexdump(_value.data(), (UINT)_value.size(), ASTRHEXDUMP_FLAG_NO_OFFSET | ASTRHEXDUMP_FLAG_NO_ASCII | ASTRHEXDUMP_FLAG_NO_ENDING_LF);
		else
			t.hexdump(_value.data(), 2 * 16 + 8, ASTRHEXDUMP_FLAG_NO_OFFSET | ASTRHEXDUMP_FLAG_NO_ASCII | ASTRHEXDUMP_FLAG_NO_ENDING_LF | ASTRHEXDUMP_FLAG_ELLIPSIS);
		return t;
	}
	astring t;
	for (ULONG i = 0; i < _ifd.Count; i++)
	{
		if (t._length)
			t += ",";
		if (_ifd.Type == IFD_USHORT)
			t += astring2("%u", _coll->_base->getUSHORT(((USHORT*)_value.data())+i));
		else if (_ifd.Type == IFD_SHORT)
			t += astring2("%d", _coll->_base->getUSHORT(((SHORT*)_value.data()) + i));
		else if (_ifd.Type == IFD_ULONG)
			t += astring2("%lu", _coll->_base->getULONG(((ULONG*)_value.data())+i));
		else if (_ifd.Type == IFD_LONG)
			t += astring2("%ld", _coll->_base->getULONG(((LONG*)_value.data()) + i));
		else if (_ifd.Type == IFD_ULONGLONG)
		{
			// rational number: expressed as LowPart/HighPart
			ULARGE_INTEGER v;
			v.QuadPart = _coll->_base->getULONGLONG(((ULONGLONG*)_value.data()) + i);
			t += astring2("%u/%u", v.LowPart, v.HighPart);
		}
		else if (_ifd.Type == IFD_LONGLONG)
		{
			// srational (signed rational) number: LowPart/HighPart
			LARGE_INTEGER v;
			v.QuadPart = (LONGLONG)_coll->_base->getULONGLONG(((ULONGLONG*)_value.data()) + i);
			t += astring2("%d/%d", v.LowPart, v.HighPart);
		}
		else if (_ifd.Type == IFD_FLOAT)
			t += astring2("%g", _coll->_base->getFLOAT(((float*)_value.data()) + i));
		else if (_ifd.Type == IFD_DOUBLE)
			t += astring2("%g", _coll->_base->getDOUBLE(((double*)_value.data()) + i));
		else
		{
			t.format("(%d bytes of an unknown data type)", _value.size());
			break;
		}
	}
	return t;
}

//////////////////////////////////////////////////////////////

const BYTE JPEGSegMarkerStartCode = 0xFF;
const BYTE JPEGSegMarkerNot = 0; // this says the preceding 0xFF is not a marker but a part of the (compressed) image data stream
const BYTE JPEGSegMarkerTEM = 1;
const BYTE JPEGSegMarkerStartOfFrame0 = 0xC0; // baseline DCT
const BYTE JPEGSegMarkerStartOfFrame1 = 0xC1; // ditto
const BYTE JPEGSegMarkerStartOfFrame2 = 0xC2; // progressive DCT
const BYTE JPEGSegMarkerStartOfFrame3 = 0xC3;
const BYTE JPEGSegMarkerDefineHuffmanTable = 0xC4;
const BYTE JPEGSegMarkerStartOfFrame5 = 0xC5;
const BYTE JPEGSegMarkerStartOfFrame6 = 0xC6;
const BYTE JPEGSegMarkerStartOfFrame7 = 0xC7;
const BYTE JPEGSegMarkerStartOfFrame8 = 0xC8;
const BYTE JPEGSegMarkerStartOfFrame9 = 0xC9;
const BYTE JPEGSegMarkerStartOfFrame10 = 0xCA;
const BYTE JPEGSegMarkerStartOfFrame11 = 0xCB;
const BYTE JPEGSegMarkerDefineArithmeticTable = 0xCC;
const BYTE JPEGSegMarkerStartOfFrame13 = 0xCD;
const BYTE JPEGSegMarkerStartOfFrame14 = 0xCE;
const BYTE JPEGSegMarkerStartOfFrameLast = 0xCF;
const BYTE JPEGSegMarkerRestart0 = 0xD0;
const BYTE JPEGSegMarkerRestartLast = 0xD7;
const BYTE JPEGSegMarkerStartOfImage = 0xD8;
const BYTE JPEGSegMarkerEndOfImage = 0xD9;
const BYTE JPEGSegMarkerStartOfScan = 0xDA;
const BYTE JPEGSegMarkerDefineQuantizationTable = 0xDB;
const BYTE JPEGSegMarkerDefineRestartInterval = 0xDD;
const BYTE JPEGSegMarkerDHP = 0xDE;
const BYTE JPEGSegMarkerEXP = 0xDF;
const BYTE JPEGSegMarkerApp0 = 0xE0;
const BYTE JPEGSegMarkerApp1 = 0xE1;
const BYTE JPEGSegMarkerApp2 = 0xE2;
const BYTE JPEGSegMarkerApp3 = 0xE3;
const BYTE JPEGSegMarkerApp4 = 0xE4;
const BYTE JPEGSegMarkerApp5 = 0xE5;
const BYTE JPEGSegMarkerApp6 = 0xE6;
const BYTE JPEGSegMarkerApp7 = 0xE7;
const BYTE JPEGSegMarkerApp8 = 0xE8;
const BYTE JPEGSegMarkerApp9 = 0xE9;
const BYTE JPEGSegMarkerApp10 = 0xEA;
const BYTE JPEGSegMarkerApp11 = 0xEB;
const BYTE JPEGSegMarkerApp12 = 0xEC;
const BYTE JPEGSegMarkerApp13 = 0xED;
const BYTE JPEGSegMarkerApp14 = 0xEE;
const BYTE JPEGSegMarkerAppLast = 0xEF;
const BYTE JPEGSegMarkerComment = 0xFE;

class MetaJpegInfo
{
public:
	MetaJpegInfo(ROFileAccessor *rofa, FILEREADINFO2 *fi) : _rofa(rofa), _fi(fi), _bmv(NULL), _segCount(0), _imgdataRegion(0) {}
	~MetaJpegInfo() {}

#pragma pack(1)
	struct SEGMENTMARKER {
		BYTE SegMagic;
		BYTE SegType;
	};
	struct SEGMENTDATA {
		ULONG SegLength;
		SEGMENTMARKER SegMarker;
		USHORT DataLength;
		BYTE Data[1];
	};
	struct APP0_JFIF {
		BYTE Signature[5]; // 'JFIF'
		BYTE MajorVersion;
		BYTE MinorVersion;
		BYTE DensityUnit; // 0=no units; 1=pixels per inch; 2=pixels per cm
		USHORT DensityX, DensityY;
		BYTE ThumbnailX, ThumbnailY; // set to 0 if there is no thumbnail.
		/* thumbnail pixel data follows if ThumbnailX and ThumbnailY are non-zero.
		RGBTRIPLE ThumbnailImage[1];
		*/
	};
	struct APP0_JFXX {
		BYTE Signature[5]; // 'JFXX'
		BYTE ThumbnailFormat; // 10=JPEG; 11=8bpp with a palette; 13=24bpp RGB
		/*BYTE ThumbnailImage[1];*/
	};
	struct APP1_EXIF {
		BYTE Signature[4]; // 'Exif'
		BYTE Reserved[2];
		EXIF_TIFFHEADER TIFFHeader;
		EXIF_IFD IFD0;
		//EXIF_IFD IFDExif;
		//EXIF_IFD IFDGPS;
		//EXIF_IFD IFD1;
		//BYTE Thumbnail[1]; // in JPEG
	};
	/* Start Of Frame (Baseline DCT): Indicates that this is a baseline DCT-based JPEG, and specifies the width, height, number of components, and component subsampling
	*/
	struct SOF0 {
		BYTE DataPrecision; // i.e., bit depth; typically 8 bpp
		USHORT ImageHeight;
		USHORT ImageWidth;
		BYTE NumComponents; // 1=grayscale; 3=YCbCr or YIQ; 4=CMYK
		BYTE ComponentId; // 1=Y, 2=Cb, 3=Cr, 4=I, 5=Q
		BYTE SamplingFactor; // bit 0-3 vertical; 4-7 horizontal
		BYTE QuantizationTableNumber;
	};
	struct DRI {
		USHORT RestartInterval; // in units of MCU blocks. the first marker will be RST0, then RST1, etc., after RST7 repeating from RST0.
	};
	struct DQT {
		BYTE QTInfo; // bit 0..3 number of QT (0..3, otherwise error); bit 4..7 precision of QT, 0=8 bit, otherwise 16 bit
		BYTE Bytes[1]; // gives QT values, n=64*(precision+1)
	};
	struct DHT {
		BYTE HTInfo; // bit 0..3 number of HT (0..3, otherwise error); bit 4 type of HT (0=DC table, 1=AC table); bit 5..7 not used, must be 0.
		BYTE NumSymbols[16]; // number of symbols with codes of length 1..16, the sum(n) of these bytes is the total number of codes, which must be <= 256.
		BYTE Symbols[1]; // table containing the symbols in order of increasing code length (n=total number of codes).
	};
	struct SOS {
		BYTE NumComponents; // must be >=1 and <=4; otherwise error; usually 1 or 3
		USHORT Components[1]; // an array of NumComponents number of 2-byte component units; each consists of a 1-byte component id (1=Y, 2=Cb, 3=Cr, 4=I, 5=Q) and a 1-byte Huffman table to use: bit 0..3 AC table (0..3), bit 4..7 DC table (0..3).
		// the component array is followed by a 3-byte packet which should be ignored.
		// SOS is immediately followed by compressed image data (actual scans).
	};
#pragma pack()

	int _segCount;

	bool checkSig()
	{
		SEGMENTMARKER marker;
		if (!_rofa->fread(&marker, sizeof(SEGMENTMARKER), 1, _fi))
			return false;
		DBGDUMP(("JPEG.SOI", &marker, sizeof(marker)));
		if (marker.SegMagic == JPEGSegMarkerStartCode && marker.SegType == JPEGSegMarkerStartOfImage)
			return true;
		_fi->ErrorCode = ERROR_RANGE_NOT_FOUND;
		return false;
	}

	int collect()
	{
		BYTE sof = 0;
		AutoFree<SEGMENTDATA> sd;
		while ((sd = getNextSegment()))
		{
			if (JPEGSegMarkerStartOfFrame0 <= sd->SegMarker.SegType && sd->SegMarker.SegType <= JPEGSegMarkerStartOfFrameLast &&
				sd->SegMarker.SegType != JPEGSegMarkerDefineHuffmanTable &&
				sd->SegMarker.SegType != JPEGSegMarkerDefineArithmeticTable)
			{
				// SOF0 or SOF1 uses baseline DCT meaning there is just one image. SOS can jump straight to an EOI at the end of file.
				sof = sd->SegMarker.SegType;
			}
			else if (sd->SegMarker.SegType == JPEGSegMarkerStartOfScan)
			{
				// a compressed image data follows the SOS marker. skip it.
				// jump to end of file. then, search backward for an EOI.
				if (sof == JPEGSegMarkerStartOfFrame2)
				{
					// SOF2 is for a progressive DCT. there can be multiple images.
					SEGMENTMARKER marker = { JPEGSegMarkerStartCode,0 };
					_fi->Backward = false;
					while (_rofa->fsearch(_fi, (LPBYTE)&marker, 1))
					{
						// get the complete marker if it is a marker.
						if (!_rofa->fread(&marker, sizeof(SEGMENTMARKER), 1, _fi))
						{
							_fi->StartOffset.QuadPart++;
							continue;
						}

						if (marker.SegType == JPEGSegMarkerNot)
							continue;
						if (JPEGSegMarkerRestart0 <= marker.SegType && marker.SegType <= JPEGSegMarkerRestartLast)
							continue;
						// we have a new segment. we've passed the image data.
						// leave the loop. let getNextSegment tag the new segment.
						break;
					}
				}
				else
				{
					// SOF0 or SOF1 means there is just one image. jump to EOF and back up to EOI.
					_rofa->fseek(_fi, 0, ROFSEEK_ORIGIN_END);

					SEGMENTMARKER marker = { JPEGSegMarkerStartCode,JPEGSegMarkerEndOfImage };
					_fi->Backward = true;
					if (!_rofa->fsearch(_fi, (LPBYTE)&marker, sizeof(SEGMENTMARKER)))
						return -_segCount;
					_fi->Backward = false;
					// fall though to let getNextSegment tag and annotate the EOI.
				}
			}
			else if (sd->SegMarker.SegType == JPEGSegMarkerEndOfImage)
			{
				// EOI - we're done.
				if (_imgdataRegion)
					_annotateImage(_imgdataRegion);
				break;
			}
		}
		return _segCount;
	}

protected:
	BinhexMetaView *_bmv;
	ROFileAccessor *_rofa;
	FILEREADINFO2 *_fi;
	char _segMarker[8];
	int _imgdataRegion;

	SEGMENTDATA *getNextSegment()
	{
		SEGMENTDATA *sd = NULL;
		SEGMENTMARKER marker;
		if (!_rofa->fread(&marker, sizeof(SEGMENTMARKER), 1, _fi))
			return NULL;
		if (marker.SegMagic != JPEGSegMarkerStartCode)
		{
			_fi->ErrorCode = ERROR_RANGE_NOT_FOUND;
			return NULL;
		}

		LARGE_INTEGER markerOffset = _fi->PriorStart;
		ULONG markerLength = sizeof(SEGMENTMARKER);
		LARGE_INTEGER payloadOffset = { 0 };
		ULONG payloadLength = 0;

		if (_segCount == 0)
		{
			DBGDUMP(("JPEG.SOI", &marker, sizeof(marker)));
			if (marker.SegType != JPEGSegMarkerStartOfImage)
			{
				_fi->ErrorCode = ERROR_RANGE_NOT_FOUND;
				return NULL;
			}
		}

		if (marker.SegType == JPEGSegMarkerStartOfImage || marker.SegType == JPEGSegMarkerEndOfImage || (JPEGSegMarkerRestart0 <= marker.SegType && marker.SegType <= JPEGSegMarkerRestartLast))
		{
			// SOI, EOI, or RSTn - no payload
			sd = (SEGMENTDATA*)malloc(sizeof(SEGMENTDATA));
			ZeroMemory(sd, sizeof(SEGMENTDATA));
			sd->SegLength = sizeof(SEGMENTMARKER);
			sd->SegMarker = marker;
		}
		else
		{
			// variable-length payload: read the payload size.
			USHORT len;
			if (!_rofa->fread(&len, sizeof(USHORT), 1, _fi))
				return NULL;
			_reverseByteOrder(&len, sizeof(len));
			len -= sizeof(USHORT);
			USHORT seglen = sizeof(SEGMENTDATA) + len;
			sd = (SEGMENTDATA*)malloc(seglen);
			ZeroMemory(sd, seglen);
			sd->SegLength = seglen;
			sd->SegMarker = marker;
			sd->DataLength = len;
			if (!_rofa->fread(sd->Data, len, 1, _fi))
			{
				free(sd);
				return NULL;
			}
			markerLength += sizeof(USHORT);
			payloadOffset = _fi->PriorStart;
			payloadLength = _fi->PriorLength;
		}
		onScanSegment(_segCount, sd, markerOffset, markerLength, payloadOffset, payloadLength);
		_segCount++;
		return sd;
	}

	virtual void onScanSegment(int segCount, SEGMENTDATA *sd, LARGE_INTEGER markerOffset, ULONG markerLength, LARGE_INTEGER payloadOffset, ULONG payloadLength) {}

	LPCSTR _SegMarkerName(BYTE marker)
	{
		switch (marker)
		{
		case JPEGSegMarkerNot: // 0;
			return "(NULL)";
		case JPEGSegMarkerTEM: // 1;
			return "TEM";
		case JPEGSegMarkerDefineHuffmanTable: // 0xC4;
			return "DHT";
		case JPEGSegMarkerDefineArithmeticTable: // 0xCC;
			return "DAT";
		case JPEGSegMarkerStartOfFrame0: // 0xC0;
			return "SOF0 (Baseline DCT)";
		case JPEGSegMarkerStartOfFrame2:
			return "SOF2 (Progressive DCT)";
		case JPEGSegMarkerStartOfImage: // 0xD8;
			return "SOI";
		case JPEGSegMarkerEndOfImage: // 0xD9;
			return "EOI";
		case JPEGSegMarkerStartOfScan: // 0xDA;
			return "SOS";
		case JPEGSegMarkerDefineQuantizationTable: // 0xDB;
			return "DQT";
		case JPEGSegMarkerDefineRestartInterval: // 0xDD;
			return "DRI";
		case JPEGSegMarkerDHP: // 0xDE;
			return "DHP";
		case JPEGSegMarkerEXP: // 0xDF;
			return "EXP";
		case JPEGSegMarkerApp0: // 0xE0;
			return "APP0";
		case JPEGSegMarkerApp1: // 0xE1;
			return "APP1";
		case JPEGSegMarkerApp2: // 0xE2;
			return "APP2";
		case JPEGSegMarkerComment: // 0xFE;
			return "COM";
		}
		if (JPEGSegMarkerStartOfFrame0 <= marker && marker <= JPEGSegMarkerStartOfFrameLast)
		{
			sprintf(_segMarker, "SOF%d", marker - JPEGSegMarkerStartOfFrame0);
			return _segMarker;
		}
		if (JPEGSegMarkerApp0 <= marker && marker <= JPEGSegMarkerAppLast)
		{
			sprintf(_segMarker, "APP%d", marker - JPEGSegMarkerApp0);
			return _segMarker;
		}
		if (JPEGSegMarkerRestart0 <= marker && marker <= JPEGSegMarkerRestartLast)
		{
			sprintf(_segMarker, "RST%d", marker - JPEGSegMarkerRestart0);
			return _segMarker;
		}
		return "(UNKNOWN)";
	}

	LPCSTR _SegMarkerLongName(BYTE marker)
	{
		switch (marker)
		{
		case JPEGSegMarkerDefineHuffmanTable: // 0xC4;
			return "Define Huffman Table";
		case JPEGSegMarkerDefineArithmeticTable: // 0xCC;
			return "Define Arithmetic Table";
		case JPEGSegMarkerStartOfImage: // 0xD8;
			return "Start Of Image";
		case JPEGSegMarkerEndOfImage: // 0xD9;
			return "End Of Image";
		case JPEGSegMarkerStartOfScan: // 0xDA;
			return "Start Of Scan";
		case JPEGSegMarkerDefineQuantizationTable: // 0xDB;
			return "Define Quantization Table";
		case JPEGSegMarkerDefineRestartInterval: // 0xDD;
			return "Define Restart Interval";
		case JPEGSegMarkerDHP: // 0xDE;
			return "Define Hierarchical Progression";
		case JPEGSegMarkerEXP: // 0xDF;
			return "Expand Reference Components";
		case JPEGSegMarkerComment: // 0xFE;
			return "Comment";
		}
		if (JPEGSegMarkerStartOfFrame0 <= marker && marker <= JPEGSegMarkerStartOfFrameLast)
			return "Start Of Frame";
		if (JPEGSegMarkerApp0 <= marker && marker <= JPEGSegMarkerAppLast)
			return "Application Data";
		if (JPEGSegMarkerRestart0 <= marker && marker <= JPEGSegMarkerRestartLast)
			return "Restart";
		return "description not available";
	}

	LPCWSTR _DensityUnitString(BYTE densityUnit)
	{
		if (densityUnit == 1)
			return L"pix/in";
		if (densityUnit == 2)
			return L"pix/cm";
		return L"";
	}
	LPCWSTR _ThumbnailFormatString(BYTE thumbnailFormat)
	{
		if (thumbnailFormat == 10)
			return L"JPEG";
		if (thumbnailFormat == 11)
			return L"8-BPP w/Palette";
		if (thumbnailFormat == 13)
			return L"24-BPP RGB";
		return L"(UNKNOWN)";
	}

	bool _annotateImage(int regionId)
	{
		if (regionId == 0)
			return false;
		CRegionCollection &r = _bmv->_regions;
		AnnotationCollection &ac = _bmv->_annotations;
		int annotId = ac.queryByCtrlId(r[regionId - 1].AnnotCtrlId);
		if (annotId == -1)
			return false;
		HBITMAP hbmp;
		if (FAILED(_createImageBitmap(&hbmp)))
			return false;
		AnnotationOperation annot(ac[annotId], &ac);
		annot.attachThumbnail(hbmp);
		return true;
	}
	virtual HRESULT _createImageBitmap(HBITMAP *hbmpOut) { return E_NOTIMPL; }
};


class MetaJpegNestedScan : public MetaJpegInfo
{
public:
	MetaJpegNestedScan(BinhexMetaView *bmv, LPBYTE basePtr, ULONG baseOffset, int dataOffset, int dataLen, int parentRegion, bool jpegParent) : _romf(basePtr, baseOffset, dataOffset, dataLen), MetaJpegInfo(new ROFileAccessor(&_romf), &_romf._fi), _parentRegion(parentRegion), _scanOffset(0), _jpegParent(jpegParent)
	{
		_bmv = bmv;
	}
	~MetaJpegNestedScan()
	{
		delete _rofa;
		_rofa = NULL;
	}

protected:
	ROMemoryFile _romf;
	int _parentRegion;
	ULONG _scanOffset;
	bool _jpegParent;

	virtual void onScanSegment(int segCount, SEGMENTDATA *sd, LARGE_INTEGER markerOffset, ULONG markerLength, LARGE_INTEGER payloadOffset, ULONG payloadLength)
	{
		ustring smarker(CP_ACP, _SegMarkerName(sd->SegMarker.SegType));
		LARGE_INTEGER offset = { markerOffset.LowPart + _romf._baseOffset + _romf._dataOffset };
		if (_jpegParent)
			offset.QuadPart += +sizeof(EXIF_IFD);

		if (sd->SegMarker.SegType == JPEGSegMarkerEndOfImage && _scanOffset)
		{
			ULONG scanLen = offset.LowPart - _scanOffset;
			_imgdataRegion =
				_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST | BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND | BMVADDANNOT_FLAG_GRAYTEXT, _parentRegion), { _scanOffset }, scanLen, L"Thumbnail Image Data\n %d (0x%X) bytes", scanLen, scanLen);
		}

		_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND, _parentRegion), offset, markerLength, L"Thumbnail %s", (LPCWSTR)smarker);
		if (sd->DataLength > 0)
		{
			offset.LowPart += markerLength;
			_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND | BMVADDANNOT_FLAG_GRAYTEXT, _parentRegion), offset, sd->DataLength, L"Thumbnail %s Data\n %d (0x%X) bytes", (LPCWSTR)smarker, sd->DataLength, sd->DataLength);
			if (sd->SegMarker.SegType == JPEGSegMarkerStartOfScan)
				_scanOffset = offset.LowPart + sd->DataLength;
		}
	}

	virtual HRESULT _createImageBitmap(HBITMAP *hbmpOut)
	{
		ROMemoryFile mf(_romf._dataPtr, _romf._baseOffset + _romf._dataOffset, 0, _romf._dataLen);
		FILEREADINFO2 *fi = &mf._fi;
		HRESULT hr;
		LPSTREAM ps;
		hr = CreateStreamOnHGlobal(NULL, TRUE, &ps);
		if (hr == S_OK)
		{
			_rofa->fseek(fi, 0, ROFSEEK_ORIGIN_BEGIN);
			hr = _rofa->fcopyTo(ps, fi);
			if (hr == S_OK)
			{
				CodecJPEG img;
				hr = img.Load(ps);
				if (hr == S_OK)
				{
					HDC hdc0 = CreateDC(L"DISPLAY", NULL, NULL, NULL);
					HDC hdc = CreateCompatibleDC(hdc0);
					hr = img.CreateBitmap(hdc, hbmpOut); // this generates a top-down bitmap.
					if (hr == S_OK)
						hr = RefitBitmapToViewFrame(hdc, _bmv->_vi.FrameSize, true, hbmpOut);
					DeleteDC(hdc);
					DeleteDC(hdc0);
				}
			}
			ps->Release();
		}
		return hr;
	}
};

class MetadataExif : public MetadataEndian, public objList<MetadataExifIfd>
{
public:
	MetadataExif(BinhexMetaView *bmv, ULONG baseOffset) : _bmv(bmv), _baseOffset(baseOffset), _thumbnailOffset(0), _thumbnailLen(0) {}

	int parse(MetaJpegInfo::SEGMENTDATA *sd)
	{
		if (sd->DataLength < sizeof(MetaJpegInfo::APP1_EXIF))
			return -1;
		MetaJpegInfo::APP1_EXIF *exif = (MetaJpegInfo::APP1_EXIF*)sd->Data;
		if (0 != memcmp(exif->Signature, "Exif", 4))
			return -1;
		int dataLen = sd->DataLength - FIELD_OFFSET(MetaJpegInfo::APP1_EXIF, TIFFHeader);
		int res = parse((LPBYTE)&exif->TIFFHeader, dataLen);
		_headerLen = FIELD_OFFSET(MetaJpegInfo::APP1_EXIF, IFD0);
		_baseLen = FIELD_OFFSET(MetaJpegInfo::APP1_EXIF, TIFFHeader);
		return res;
	}

	int parse(LPBYTE metaData, LONG metaLength)
	{
		if (metaLength < sizeof(EXIF_TIFFHEADER) + sizeof(EXIF_IFD))
			return -1;

		EXIF_TIFFHEADER *hdr = (EXIF_TIFFHEADER*)metaData;
		if (hdr->ByteOrder != 0x4949 && hdr->ByteOrder != 0x4D4D)
			return -1; // invalid endian
		_big = hdr->ByteOrder == 0x4D4D;
		if (getUSHORT(&hdr->Signature) != 0x002A)
			return -1; // invalid tiff signature
		ULONG offset0 = getULONG(&hdr->IFDOffset);
		_basePtr = metaData;
		LPBYTE p0 = _basePtr + offset0;

		// first IFD (called IFD0)
		MetadataExifIfd *res0 = new MetadataExifIfd("0th IFD", this);
		int len = res0->parse(offset0, metaLength);
		if (len < 0)
		{
			delete res0;
			return -1;
		}
		objList<MetadataExifIfd>::add(res0);
		// the offset to another IFD (called IFD1) immediately follows IFD0. it contains a thumbnail image.
		ULONG offsetIfd1 = res0->_base->getULONG(p0 + len);
		// IFD0 includes an offset to a sub-IFD called 'Exif IFD'. it holds image configuration info.
		MetadataExifIfdAttribute *ifd = res0->find(MetadataExifIfdAttribute::OffsetToExifIFD);
		if (ifd)
		{
			ULONG offsetExifIfd = res0->_base->getULONG(ifd->_value.data());
			MetadataExifIfd *res2 = new MetadataExifIfd("Exif IFD", this);
			len = res2->parse(offsetExifIfd, metaLength - offsetExifIfd);
			if (len >= 0)
				objList<MetadataExifIfd>::add(res2);
			else
				delete res2;
		}
		// IFD0 may also include an offset to a GPS IFD.
		ifd = res0->find(MetadataExifIfdAttribute::OffsetToGpsIFD);
		if (ifd)
		{
			ULONG offsetGpsIfd = res0->_base->getULONG(ifd->_value.data());
			MetadataExifIfd *res3 = new MetadataExifIfd("GPS IFD", this);
			len = res3->parse(offsetGpsIfd, metaLength - offsetGpsIfd);
			if (len >= 0)
				objList<MetadataExifIfd>::add(res3);
			else
				delete res3;
		}
		if (offsetIfd1)
		{
			MetadataExifIfd *res1 = new MetadataExifIfd("Thumbnail IFD", this);
			len = res1->parse(offsetIfd1, metaLength - offsetIfd1);
			if (len >= 0)
			{
				objList<MetadataExifIfd>::add(res1);
				// the ifd is followed by thumbnail image data. it should be in jpeg.
				// if a thumbnail image is present in the exif, these are true.
				// - 'Collection' has the compression method which must be jpeg (6).
				// - 'JPEGInterchangeFormat' has an offset to the scan data of the thumbnail.
				// - 'JPEGInterchangeFormatLength' has the data length in bytes.
				ULONG toffset = 0;
				ULONG tlen = 0;
				ULONG compressionMethod = 0;
				for (int i = 0; i < res1->size(); i++)
				{
					MetadataExifIfdAttribute *iattrib = (*res1)[i];
					switch (iattrib->_ifd.Tag)
					{
					case MetadataExifIfdAttribute::Compression:
						compressionMethod = iattrib->_ifd.ValueOrOffset;
						break;
					case MetadataExifIfdAttribute::JPEGInterchangeFormat:
						toffset = iattrib->_ifd.ValueOrOffset;
						break;
					case MetadataExifIfdAttribute::JPEGInterchangeFormatLength:
						tlen = iattrib->_ifd.ValueOrOffset;
						break;
					}
				}
				if (compressionMethod == 6) // 6 is for jpeg.
				{
					_thumbnailOffset = toffset;
					_thumbnailLen = tlen;
				}
				else
					_thumbnailLen = 0;
			}
			else
				delete res1;
		}

		_headerLen = sizeof(EXIF_TIFFHEADER);
		_baseLen = 0;
		return (int)objList<MetadataExifIfd>::size();
	}

	void parseThumbnail(int parentRegion, bool jpegParent)
	{
		if (!_thumbnailLen)
			return;
		MetaJpegNestedScan jpeg(_bmv, _basePtr, _baseOffset, _thumbnailOffset, _thumbnailLen, parentRegion, jpegParent);
		jpeg.collect();
	}

protected:
	BinhexMetaView *_bmv;
	ULONG _baseOffset;
	int _thumbnailOffset;
	int _thumbnailLen;
};


class MetadataXMP : public MetadataEndian
{
public:
	MetadataXMP() {}

	astring _xmp;
	EzXmlElement _xel;

	bool parse(MetaJpegInfo::SEGMENTDATA *sd)
	{
		static char xmpUrl[] = "http://ns.adobe.com/xap/1.0/";
		if (sd->DataLength < sizeof(xmpUrl))
			return false;
		if (0 != memcmp(sd->Data, xmpUrl, sizeof(xmpUrl)))
			return false;
		_headerLen = sizeof(xmpUrl);
		return parse(sd->Data + _headerLen, sd->DataLength - _headerLen);
	}
	bool parse(LPBYTE metaData, LONG metaLength)
	{
		_baseLen = _headerLen;
		_basePtr = metaData;
		_xmp.assign((LPCSTR)_basePtr, metaLength);
		DBGPRINTF((L"XMP: %d bytes\n%S\n", _xmp._length, _xmp._buffer));

		EzXml xmp(_xmp._buffer, _xmp._length);
		if (FAILED(xmp.openTree("x:xmpmeta", &_xel)))
			return false;
		parseXmpDescription();
		return true;
	}
	
	HRESULT markXmpTags(LONGLONG baseOffset, int parentRegion, BinhexMetaView *bmv)
	{
		baseOffset += _baseLen;

		astring summary;
		int i,j;
		for (i = 0; i < _props.size(); i++)
		{
			astring s;
			RdfDescProp *prop = _props[i];
			s.format("%s: ", (LPCSTR)prop->Name);
			for (j = 0; j < prop->Values.size(); j++)
			{
				if (j>0)
					s.append(";" );
				astring *val = prop->Values[j];
				s.append(*val);
			}
			if (summary._length > 0)
				summary.append("\n");
			summary.append(s);
		}
		LARGE_INTEGER offset;
		offset.QuadPart = baseOffset;
		offset.LowPart += _xel._tag.OpenStart;
		int len = _xel._tag.OpenStop - _xel._tag.OpenStart;
		int rgn = bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND, parentRegion), offset, len, L"x:xmpmeta\n\n%S", (LPCSTR)summary);

		if (!_xel._children || _xel._children->size() == 0)
			return S_FALSE;

		ustring xpath(CP_UTF8, _xel._name._buffer, _xel._name._length);

		_baseOffset = baseOffset;
		_bmv = bmv;
		for (i = 0; i < (int)_xel._children->size(); i++)
		{
			_colorTag((*_xel._children)[i], (EzXmlElements*)_xel._children, xpath);
		}

		offset.QuadPart = baseOffset;
		offset.LowPart += _xel._tag.CloseStart;
		len = _xel._tag.Length + 3;
		_bmv->addTaggedRegion(MAKELONG(0, rgn), offset, len, NULL);

		return S_OK;
	}

protected:
	BinhexMetaView *_bmv;
	LONGLONG _baseOffset;
	// xel is a member of xgroup.
	void _colorTag(EzXmlElement *xel, EzXmlElements* xgroup, LPCWSTR xpath)
	{
		int len;
		USHORT pos1, pos2;
		LARGE_INTEGER offset;
		long offset0 = (long)(xgroup->_sourceData._buffer - _xmp._buffer);

		ustring xpath2(xpath);
		xpath2.append(L" \\ ");
		xpath2.appendCp(CP_UTF8, xel->_name._buffer, xel->_name._length);

		offset.QuadPart = _baseOffset;
		offset.LowPart += offset0;
		offset.LowPart += xel->_tag.OpenStart;
		len = xel->_tag.OpenStop - xel->_tag.OpenStart;
		pos1 = xel->_tag.OpenStart + 1;
		pos2 = xel->_tag.OpenStart + 1 + xel->_tag.Length;
		int rgn = _bmv->addTaggedRegion(BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND2, pos1, pos2, offset, len, xpath2);
		if (xel->_tag.CloseStart)
		{
			offset.QuadPart = _baseOffset;
			offset.LowPart += offset0;
			offset.LowPart += xel->_tag.CloseStart;
			len = xel->_tag.Length + 3; // 3 for the closing tag characters "<" and "/>"
			_bmv->addTaggedRegion(MAKELONG(0, rgn), offset, len, NULL);
		}

		EzXmlElements *xchildren = (EzXmlElements*)xel->_children;
		if (xchildren)
		{
			for (int i = 0; i < (int)xchildren->size(); i++)
			{
				_colorTag((*xchildren)[i], xchildren, xpath2);
			}
		}
	}

	struct RdfDescProp {
		astring Name;
		objList<astring> Values;
	};
	objList<RdfDescProp> _props;

	bool parseXmpDescription()
	{
		EzXml xmp(_xmp._buffer, _xmp._length);
		EzXmlElements elems;
		if (S_OK != xmp.collectElements("x:xmpmeta\\rdf:RDF\\rdf:Description", &elems))
			return false;
		// an rdf:Description contains 'Dublin Core' elements of image attributes like dc:title and dc:creator.
		for (int i = 0; i < (int)elems.size(); i++)
		{
			int n = elems[i]->_name.findChar(':');
			if (n == -1)
				continue;
			astring prefix(elems[i]->_name._buffer, n);
			if (0 != strcmp(prefix, "dc"))
				continue;
			astring xname(elems[i]->_name._buffer + elems[i]->_name._pos, elems[i]->_name._length - elems[i]->_name._pos);

			RdfDescProp *prop = new RdfDescProp;
			prop->Name.attach(xname);
			EzXmlElementsData xdata(elems[i]->_data);
			EzXmlElements elems2;
			xdata.collectElements(&elems2);
			for (int j = 0; j < (int)elems2.size(); j++)
			{
				// rdf:Seq, rdf:Alt
				EzXmlElementsData xdata2(elems2[j]->_data);
				EzXmlElements elems3;
				xdata2.collectElements(&elems3);
				for (int k = 0; k < (int)elems3.size(); k++)
				{
					// rdf:li
					astring *xvalue = new astring;
					elems3[k]->_data.clone(*xvalue);
					DBGPRINTF((L"MetadataXMP: %S: %S\n", (LPCSTR)prop->Name, (LPCSTR)*xvalue));
					prop->Values.add(xvalue);
				}
			}
			_props.add(prop);
		}
		return true;
	}
};

/* reference: https://www.color.org/specification/ICC1v43_2010-12.pdf
latest: https://www.color.org/specification/ICC.2-2019.pdf (see Table 12 - Profile header fields)
*/
#pragma pack(1)
struct ICCP_HEADER
{
	ULONG ProfileSize;
	BYTE PreferredCMMType[4];
	ULONG ProfileVersion;
	BYTE ProfileOrDeviceClass[4];
	BYTE ColorSpace[4];
	BYTE PCS[4];
	WORD CreationTime[6];
	BYTE ProfileFileSignature[4]; // 'acsp'
	BYTE PrimaryPlatformSignature[4];
	ULONG CMMOptionFlags;
	BYTE DeviceManufacturer[4];
	BYTE DeviceModel[4];
	ULONGLONG DeviceAttributes;
	ULONG RenderingIntent;
	ULONG PCSIlluminant[3]; // nCIEXYZ values
	BYTE ProfileCreatorSignature[4];
	ULONGLONG ProfileID[2];
	BYTE SpectralPCS[4];
	USHORT SpectralRange[3];
	USHORT BispectralRange[3];
	ULONG MCSSignature;
	BYTE ProfileOrDeviceSubclass[4];
	ULONG Reserved;
};

struct ICCP_HEADER_MAP
{
	LPCSTR FieldName;
	SHORT FieldOffset;
	SHORT DataType;
};

struct ICCPTagDataElement
{
	BYTE TagSignature[4];
	ULONG DataOffset;
	ULONG DataSize;
};

struct ICCPTagDataProp
{
	CHAR TagName[5];
	BYTE Flags;
	ULONG DataOffset, DataSize;
};

#pragma pack()


const SHORT ICCPDATATYPE_UNK = 0;
const SHORT ICCPDATATYPE_BYTE4 = 1; // 4 bytes
const SHORT ICCPDATATYPE_UINT32 = 2; // 4 bytes
const SHORT ICCPDATATYPE_UINT32_HEX = 3;
const SHORT ICCPDATATYPE_UINT64 = 4; // 8 bytes
const SHORT ICCPDATATYPE_UINT64_HEX = 5;
const SHORT ICCPDATATYPE_UINT64W = 6; // 16 bytes
const SHORT ICCPDATATYPE_UINT64W_HEX = 7;
const SHORT ICCPDATATYPE_DATETIME = 8; // 12 bytes
const SHORT ICCPDATATYPE_XYZ = 9; // 12 bytes
const SHORT ICCPDATATYPE_SPECTRALRANGE = 10; // 6 bytes


ICCP_HEADER_MAP ICCPHeaderFields[] =
{
	{"ProfileSize", FIELD_OFFSET(ICCP_HEADER, ProfileSize), ICCPDATATYPE_UINT32},
	{"PreferredCMMType", FIELD_OFFSET(ICCP_HEADER, PreferredCMMType), ICCPDATATYPE_BYTE4},
	{"ProfileVersion", FIELD_OFFSET(ICCP_HEADER, ProfileVersion), ICCPDATATYPE_UINT32_HEX},
	{"ProfileOrDeviceClass", FIELD_OFFSET(ICCP_HEADER, ProfileOrDeviceClass), ICCPDATATYPE_BYTE4},
	{"ColorSpace", FIELD_OFFSET(ICCP_HEADER, ColorSpace), ICCPDATATYPE_BYTE4},
	{"PCS", FIELD_OFFSET(ICCP_HEADER, PCS), ICCPDATATYPE_BYTE4},
	{"CreationTime", FIELD_OFFSET(ICCP_HEADER, CreationTime), ICCPDATATYPE_DATETIME},
	{"ProfileFileSignature", FIELD_OFFSET(ICCP_HEADER, ProfileFileSignature), ICCPDATATYPE_BYTE4},
	{"PrimaryPlatformSignature", FIELD_OFFSET(ICCP_HEADER, PrimaryPlatformSignature), ICCPDATATYPE_BYTE4},
	{"CMMOptionFlags", FIELD_OFFSET(ICCP_HEADER, CMMOptionFlags), ICCPDATATYPE_UINT32_HEX},
	{"DeviceManufacturer", FIELD_OFFSET(ICCP_HEADER, DeviceManufacturer), ICCPDATATYPE_BYTE4},
	{"DeviceModel", FIELD_OFFSET(ICCP_HEADER, DeviceModel), ICCPDATATYPE_BYTE4},
	{"DeviceAttributes", FIELD_OFFSET(ICCP_HEADER, DeviceAttributes), ICCPDATATYPE_UINT64_HEX},
	{"RenderingIntent", FIELD_OFFSET(ICCP_HEADER, RenderingIntent), ICCPDATATYPE_UINT32},
	{"PCSIlluminant", FIELD_OFFSET(ICCP_HEADER, PCSIlluminant), ICCPDATATYPE_XYZ},
	{"ProfileCreatorSignature", FIELD_OFFSET(ICCP_HEADER, ProfileCreatorSignature), ICCPDATATYPE_BYTE4},
	{"ProfileID", FIELD_OFFSET(ICCP_HEADER, ProfileID), ICCPDATATYPE_UINT64W_HEX},
	{"SpectralPCS", FIELD_OFFSET(ICCP_HEADER, SpectralPCS), ICCPDATATYPE_BYTE4},
	{"SpectralRange", FIELD_OFFSET(ICCP_HEADER, SpectralRange), ICCPDATATYPE_SPECTRALRANGE},
	{"BispectralRange", FIELD_OFFSET(ICCP_HEADER, BispectralRange), ICCPDATATYPE_SPECTRALRANGE},
	{"MCSSignature", FIELD_OFFSET(ICCP_HEADER, MCSSignature), ICCPDATATYPE_UINT32_HEX},
	{"ProfileOrDeviceSubclass", FIELD_OFFSET(ICCP_HEADER, ProfileOrDeviceSubclass), ICCPDATATYPE_BYTE4},
	{"Reserved", FIELD_OFFSET(ICCP_HEADER, Reserved), ICCPDATATYPE_UINT32},
};


class MetadataICCP : public MetadataEndian
{
public:
	MetadataICCP() :_profileNum(0), _profileTotal(0), _profileLen(0), _profileHeader(NULL) { _big = true; }

	BYTE _profileNum;
	BYTE _profileTotal;
	USHORT _profileLen;
	ICCP_HEADER *_profileHeader;

	bool parse(MetaJpegInfo::SEGMENTDATA *sd)
	{
		static char signature[] = "ICC_PROFILE";
		int headerLen = sizeof(signature) + 2;
		if (sd->DataLength < headerLen)
			return false;
		if (0 != memcmp(sd->Data, signature, sizeof(signature)))
			return false;
		_headerLen = headerLen;
		_profileNum = sd->Data[sizeof(signature)];
		_profileTotal = sd->Data[sizeof(signature) + 1];
		return parse(sd->Data + headerLen, sd->DataLength - headerLen);
	}
	bool parse(LPBYTE metaData, LONG metaLength)
	{
		_baseLen = _headerLen;
		_basePtr = metaData;
		_profileLen = LOWORD(metaLength);
		// parse the profile header first. all profile data is in big endian. translate to little endian.
		_profileHeader = fixHeaderByteOrder(metaData);
		if (_profileHeader->ProfileSize != _profileLen)
			return false; // the internal size differs from the actual. not a color profile after all?
		// find the number of metadata tags in the profile.
		LPBYTE p = metaData + sizeof(ICCP_HEADER);
		int nTags = (int)getULONG(p);
		p += sizeof(ULONG);
		
		/* tab table structure:

		Byte    Field     Content                 Encoded as…
		offset  length
				(bytes)
		------  -------   ---------------------   ----------
		0 to 3    4       Tag count (n)
		4 to 7    4       Tag signature
		8 to 11   4       Offset to beginning     uInt32Number
						  of tag data element
		12 to 15  4       Size of tag data        uInt32Number
						  element
		16 to    12(n-1)  Signature, offset and
		(12n+3)           size respectively of
						  subsequent n-1 tags
		Key
		n: Number of tags contained in the profile
		*/
		for (int i = 0; i < nTags; i++)
		{
			ICCPTagDataElement *src = (ICCPTagDataElement*)p;
			ICCPTagDataProp *dest = new ICCPTagDataProp;
			memcpy(dest->TagName, src->TagSignature, sizeof(src->TagSignature));
			dest->TagName[4] = 0;
			dest->DataOffset = getULONG(&src->DataOffset);
			dest->DataSize = getULONG(&src->DataSize);
			_tagProps.add(dest);
			p += 3 * sizeof(ULONG);
		}

		return true;
	}
	HRESULT markProfileFields(LONGLONG baseOffset, int parentRegion, BinhexMetaView *bmv)
	{
		baseOffset += _baseLen;

		astring props;
		int i;
		for (i = 0; i < (int)_headerProps.size(); i++)
		{
			props.append("\n");
			props.append(*_headerProps[i]);
		}

		LARGE_INTEGER offset;
		offset.QuadPart = baseOffset;

		int rgn = bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND, parentRegion), offset, sizeof(ICCP_HEADER),
			L"Profile header (%d bytes)\n%S", sizeof(ICCP_HEADER), (LPCSTR)props);

		offset.LowPart += sizeof(ICCP_HEADER);
		bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_HEADER | BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND, rgn), offset, sizeof(ULONG),
			L"TagCount: %d", _tagProps.size());

		for (i = 0; i < (int)_tagProps.size(); i++)
		{
			offset.QuadPart = baseOffset;
			offset.LowPart += sizeof(ICCP_HEADER) + sizeof(ULONG);
			offset.LowPart += i*3*sizeof(ULONG);
			rgn = bmv->addTaggedRegion(BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND2, offset, 3 * sizeof(ULONG), L"Tag no. %d: %S", i+1, _tagProps[i]->TagName);
			if (rgn)
			{
				offset.QuadPart = baseOffset;
				offset.LowPart += _tagProps[i]->DataOffset;
				bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND2, rgn), offset, _tagProps[i]->DataSize, L"Data of Tag %S (%d bytes)", _tagProps[i]->TagName, _tagProps[i]->DataSize);
			}
		}
		return S_OK;
	}

protected:
	objList<ICCPTagDataProp> _tagProps;
	objList<astring> _headerProps;
	ICCP_HEADER* fixHeaderByteOrder(LPBYTE hdr)
	{
		int i, j;
		for (i = 0; i < ARRAYSIZE(ICCPHeaderFields); i++)
		{
			astring *prop = new astring;
			LPBYTE p = hdr + ICCPHeaderFields[i].FieldOffset;
			if (ICCPHeaderFields[i].DataType == ICCPDATATYPE_UINT32 || ICCPHeaderFields[i].DataType == ICCPDATATYPE_UINT32_HEX)
			{
				ULONG t = getULONG(p);
				*(ULONG*)p = t;
				prop->format(ICCPHeaderFields[i].DataType == ICCPDATATYPE_UINT32_HEX? "%s: 0x%x" : "%s: %d", ICCPHeaderFields[i].FieldName, t);
			}
			else if (ICCPHeaderFields[i].DataType == ICCPDATATYPE_UINT64 || ICCPHeaderFields[i].DataType == ICCPDATATYPE_UINT64_HEX)
			{
				ULONGLONG t = getULONGLONG(p);
				*(ULONGLONG*)p = t;
				prop->format(ICCPHeaderFields[i].DataType == ICCPDATATYPE_UINT64_HEX? "%s: 0x%I64x" : "%s: %I64d", ICCPHeaderFields[i].FieldName, t);
			}
			else if (ICCPHeaderFields[i].DataType == ICCPDATATYPE_UINT64W || ICCPHeaderFields[i].DataType == ICCPDATATYPE_UINT64W_HEX)
			{
				// double UINT64
				ULONGLONG t[2];
				t[0] = getULONGLONG(p);
				*(ULONGLONG*)p = t[0];
				p += +sizeof(ULONGLONG);
				t[1] = getULONGLONG(p);
				*(ULONGLONG*)p = t[1];
				prop->format(ICCPHeaderFields[i].DataType == ICCPDATATYPE_UINT64W_HEX? "%s: 0x%I64x:%I64x" : "%s: %I64d:%I64d", ICCPHeaderFields[i].FieldName, t[0], t[1]);
			}
			else if (ICCPHeaderFields[i].DataType == ICCPDATATYPE_DATETIME)
			{
				/* see section 5.2 basic numeric types in https://www.color.org/icc32.pdf.

				dateTimeNumber: This dateTimeNumber is a 12 byte value representation of the time and date.The actual values are encoded as 16 bit unsigned integers.

				Byte Offset  Field name
				----------  ------------
				0-1	        number of the year (actual year, i.e. 1994)
				2-3         number of the month (1-12)
				4-5         number of the day of the month (1-31)
				6-7         number of hours (0-23)
				8-9         number of minutes (0-59)
				10-11       number of seconds (0-59)
				*/
				USHORT t[6];
				for (j = 0; j < ARRAYSIZE(t); j++)
				{
					t[j] = getUSHORT(p);
					*(USHORT*)p = t[j];
					p += sizeof(USHORT);
				}
				prop->format("%s: %d/%d/%d-%d:%02d:%02d", ICCPHeaderFields[i].FieldName, t[2], t[1], t[0], t[3], t[4], t[5]);
			}
			else if (ICCPHeaderFields[i].DataType == ICCPDATATYPE_XYZ)
			{
				/* XYZNumber: This type represents a set of three fixed signed 4 byte/32 bit quantities used to encode CIEXYZ tristimulus values where byte usage is assigned as follows:

				Byte Offset  Content  Encoding
				-----------  -------  --------
				0-3          CIE X    s15Fixed16Number
				4-7          CIE Y    s15Fixed16Number
				8-11         CIE Z    s15Fixed16Number

				s15Fixed16Number (s15.16): This type represents a fixed signed 4 byte/32 bit quantity which has 16 fractional bits. An example of this encoding is:

				-32768.0              :  80000000h
				0                     :  00000000h
				1.0                   :  00010000h
				32767 + (65535/65536) :  7FFFFFFFh
				*/
				ULONG t[3];
				for (j = 0; j < ARRAYSIZE(t); j++)
				{
					t[j] = getULONG(p);
					*(ULONG*)p = t[j];
					p += sizeof(ULONG);
				}
				prop->format("%s: 0x%x:%x:%x", ICCPHeaderFields[i].FieldName, t[0], t[1], t[2]);
			}
			else if (ICCPHeaderFields[i].DataType == ICCPDATATYPE_SPECTRALRANGE)
			{
				/* The spectralRange data type shall be used to specify spectral ranges. This data type shall be made up of two float16Number values and a uInt16Number value that define the starting wavelength, ending wavelength and total number of steps in the range. The encoding of a spectralRange data type is shown in Table 6.

				Byte        Field        Field contents       Encoded as…
				position    length
				            (bytes)
			    --------    -------      --------------       ------------
				0 to 1        2          Start wavelength     float16Number
				2 to 3        2          End wavelength       float16Number
				4 to 5        2          Steps in             uInt16Number
				                         wavelength range
				*/
				USHORT t[3];
				for (j = 0; j < ARRAYSIZE(t); j++)
				{
					t[j] = getUSHORT(p);
					*(USHORT*)p = t[j];
					p += sizeof(USHORT);
				}
				prop->format("%s: 0x%x:%x:%x", ICCPHeaderFields[i].FieldName, t[0], t[1], t[2]);
			}
			else //ICCPDATATYPE_BYTE4
			{
				if (p[0] != 0)
					prop->format("%s: %c%c%c%c", ICCPHeaderFields[i].FieldName, p[0], p[1], p[2], p[3]);
				else
					prop->format("%s: (na)", ICCPHeaderFields[i].FieldName);
			}
			_headerProps.add(prop);
		}

		return (ICCP_HEADER*)hdr;
	}
};

HRESULT ScanTag::parseExifMetaData(FILEREADINFO2 *fi, LONGLONG baseOffset, ULONG metaLength, int parentRegion)
{
	LARGE_INTEGER metaOffset;
	metaOffset.QuadPart = baseOffset;
	int res = fseek(fi, metaOffset.LowPart, ROFSEEK_ORIGIN_BEGIN);

	LPBYTE metadata = (LPBYTE)malloc(metaLength);
	if (!metadata)
		return E_OUTOFMEMORY;

	if (!fread(metadata, metaLength, 1, fi))
	{
		free(metadata);
		return E_FAIL;
	}

	HRESULT hr = S_OK;
	MetadataExif exif(_bmv, metaOffset.LowPart);
	if (exif.parse(metadata, metaLength) >= 0)
	{
		_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_HEADER, parentRegion), metaOffset, exif._headerLen, L"Exif Image Resource\n contains %d IFDs", exif.size());
		for (int i = 0; i < (int)exif.size(); i++)
		{
			ULONG offset0 = metaOffset.LowPart + exif._baseLen;

			MetadataExifIfd *res = exif[i];
			size_t n = res->size();
			if (n == 0)
				continue;

			_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND, parentRegion), { offset0 + res->_offset }, res->_headerLen, L"%d - %S\n %d datasets", i + 1, (LPCSTR)res->_name, res->size());

			MetadataExifIfdAttribute *dset;
			int j;
			for (j = 0; j < (int)n; j++)
			{
				dset = res->at(j);
				_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND, parentRegion), { offset0 + dset->_offset }, dset->_headerLen, L"%S\n%S", (LPCSTR)dset->_name, (LPCSTR)dset->getText());
			}
			for (j = 0; j < (int)n; j++)
			{
				dset = res->at(j);
				if (dset->_valueOffset)
				{
					_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND | BMVADDANNOT_FLAG_GRAYTEXT, parentRegion), { offset0 + dset->_valueOffset }, (ULONG)dset->_value.size(), L"Value of IFD attribute\n%S", (LPCSTR)dset->_name);
				}
			}
		}
		exif.parseThumbnail(parentRegion, false);
	}
	else
		hr = E_INVALIDARG;
	return hr;
}

HRESULT ScanTag::parseIccpMetaData(FILEREADINFO2 *fi, LONGLONG baseOffset, ULONG metaLength, int parentRegion)
{
	LARGE_INTEGER metaOffset;
	metaOffset.QuadPart = baseOffset;
	int res = fseek(fi, metaOffset.LowPart, ROFSEEK_ORIGIN_BEGIN);

	LPBYTE metadata = (LPBYTE)malloc(metaLength);
	if (!metadata)
		return E_OUTOFMEMORY;

	if (!fread(metadata, metaLength, 1, fi))
	{
		free(metadata);
		return E_FAIL;
	}

	HRESULT hr = S_OK;
	MetadataICCP iccp;
	if (iccp.parse(metadata, metaLength))
		hr = iccp.markProfileFields(baseOffset, parentRegion, _bmv);
	else
		hr = E_INVALIDARG;
	return hr;
}

/* parsing support for Extensible Metadata Platform (XMP)
*/
HRESULT ScanTag::parseXmpMetaData(FILEREADINFO2 *fi, LONGLONG baseOffset, ULONG metaLength, int parentRegion)
{
	LARGE_INTEGER metaOffset;
	metaOffset.QuadPart = baseOffset;
	int res = fseek(fi, metaOffset.LowPart, ROFSEEK_ORIGIN_BEGIN);

	LPBYTE metadata = (LPBYTE)malloc(metaLength);
	if (!metadata)
		return E_OUTOFMEMORY;

	if (!fread(metadata, metaLength, 1, fi))
	{
		free(metadata);
		return E_FAIL;
	}

	HRESULT hr;
	MetadataXMP xmp;
	if (xmp.parse(metadata, metaLength))
		hr = xmp.markXmpTags(baseOffset, parentRegion, _bmv);
	else
		hr = E_INVALIDARG;

	return hr;
}

HRESULT ScanTag::parsePhotoshopMetaData(FILEREADINFO2 *fi, LONGLONG baseOffset, ULONG metaLength, int parentRegion)
{
	LARGE_INTEGER metaOffset;
	metaOffset.QuadPart = baseOffset;
	int res = fseek(fi, metaOffset.LowPart, ROFSEEK_ORIGIN_BEGIN);

	LPBYTE metadata = (LPBYTE)malloc(metaLength);
	if (!metadata)
		return E_OUTOFMEMORY;

	if (!fread(metadata, metaLength, 1, fi))
	{
		free(metadata);
		return E_FAIL;
	}

	HRESULT hr;
	MetadataXMP xmp;
	if (xmp.parse(metadata, metaLength))
		hr = xmp.markXmpTags(baseOffset, parentRegion, _bmv);
	else
		hr = E_INVALIDARG;

	return hr;
}

/////////////////////////////////////////////////
struct PhotoshopImageResourceIdNameMap
{
	USHORT Id;
	LPCSTR Name;
	LPCSTR Comment;
};

// reference: https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
PhotoshopImageResourceIdNameMap PIRIdNameMaps[] =
{
{1000, "Image header", "2-byte channels, rows, columns, depth, and mode"},
{1001, "Print info record", "By Macintosh print manager"},
{1003, "Indexed color table", NULL},
{1005, "ResolutionInfo structure", NULL},
{1006, "Names of the alpha channels", "As a series of Pascal strings"},
{1007, "DisplayInfo structure", NULL},
{1008, "Caption", "As a Pascal string"},
{1009, "Border information", "Contains a fixed number (2 bytes real, \n2 bytes fraction) for the border width, \nand 2 bytes for border units (1=inches, \n2=cm, 3=points, 4=picas, 5=columns)"},
{1010, "Background color", NULL},
{1011, "Print flags", "A series of 1-byte boolean values: labels, \ncrop marks, color bars, registration marks, \nnegative, flip, interpolate, caption, print flags"},
{1012, "Grayscale and multichannel halftoning information", NULL},
{1013, "Color halftoning information", NULL},
{1014, "Duotone halftoning information", NULL},
{1015, "Grayscale and multichannel transfer function", NULL},
{1016, "Color transfer functions", NULL},
{1017, "Duotone transfer functions", NULL},
{1018, "Duotone image information", NULL},
{1019, "Dot range", "Two bytes for the effective black and \nwhite values for the dot range"},
{1021, "EPS options", NULL},
{1022, "Quick Mask information", "2 bytes containing Quick Mask channel ID; \n1 - byte boolean indicating whether the mask \nwas initially empty"},
{1024, "Layer state information", "2 bytes containing the index of target layer \n(0 = bottom layer)"},
{1025, "Working path"},
{1026, "Layers group information", "2 bytes per layer containing a group ID \nfor the dragging groups.Layers in a group have \nthe same group ID"},
{1028, "IPTC-NAA record", "Contains File Info"},
{1029, "Image mode for raw format files", NULL},
{1030, "JPEG quality", "Private"},
{1032, "Grid and guides information", NULL},
{1033, "Thumbnail resource", "For Photoshop 4.0 only"},
{1034, "Copyright flag", "Boolean indicating whether image is copyrighted. \nCan be set via Property suite or by user in File Info"},
{1035, "URL", "Handle of a text string with uniform resource \nlocator. Can be set via Property suite or by user in File Info"},
{1036, "Thumbnail resource", NULL},
{1037, "Global Angle", "4 bytes that contain an integer between \n0 and 359, which is the global lighting \nangle for effects layer. If not present, \nassumed to be 30."},
{1038, "Color samplers resource", NULL},
{1039, " ICC Profile", "The raw bytes of an ICC (International \nColor Consortium) format profile"},
{1040, "Watermark", "One byte"},
{1041, "ICC Untagged Profile", "1 byte that disables any assumed profile \nhandling when opening the file. \n1 = intentionally untagged"},
{1042, "Effects visible", "1-byte global flag to show / hide all \nthe effects layer. Only present when \nthey are hidden"},
{1043, "Spot Halftone", "4 bytes for version, 4 bytes for length, \nand the variable length data"},
{1044, "Document-specific IDs seed number", "4 bytes: Base value, starting at which layer \nIDs will be generated (or a greater value \nif existing IDs already exceed it). \nIts purpose is to avoid the case where we \nadd layers, flatten, save, open, and \nthen add more layers that end up with \nthe same IDs as the first set."},
{1045, "Unicode Alpha Names", "Unicode string (4 bytes length \nfollowed by string)"},
{1046, "Indexed Color Table Count", "2 bytes for the number of colors in \ntable that are actually defined"},
{1047, "Transparency Index", "2 bytes for the index of transparent \ncolor, if any"},
{1049, "Global Altitude", "4 byte entry for altitude"},
{1050, "Slices", NULL},
{1051, "Workflow URL", "Unicode string"},
{1052, "Jump To XPEP", "2 bytes major version, 2 bytes minor \nversion, 4 bytes count.Following is repeated for count: \n4 bytes block size, 4 bytes key, if \nkey='jtDd', then next is a Boolean \nfor the dirty flag; otherwise it's a \n4 byte entry for the mod date"},
{1053, "Alpha Identifiers", "4 bytes of length, followed by 4 bytes \neach for every alpha identifier"},
{1054, "URL List", "4 byte count of URLs, followed by 4 byte long, \n4 byte ID, and Unicode string for each count"},
{1057, " Version Info", "4 bytes version, 1 byte hasRealMergedData , \nUnicode string : writer name, Unicode string : \nreader name, 4 bytes file version"},
{1058, "EXIF data 1"},
{1059, "EXIF data 3"},
{1060, "XMP metadata", "File info as XML description"},
{1061, "Caption digest", "16 bytes: RSA Data Security, \nMD5 message - digest algorithm"},
{1062, "Print scale", "2 bytes style (0 = centered, \n1 = size to fit, 2 = user defined). \n4 bytes x location (floating point). \n4 bytes y location (floating point). 4 bytes scale (floating point)."},
{1064, "Pixel Aspect Ratio", "4 bytes (version = 1), 8 bytes double, \nx / y of a pixel"},
{1065, "Layer Comps", "4 bytes (descriptor version = 16), Descriptor"},
{1066, "Alternate Duotone Colors", "2 bytes (version = 1), 2 bytes count, \nfollowing is repeated for each count: [Color: \n2 bytes for space followed by 4 * 2 byte \ncolor component], following this is another \n2 byte count, usually 256, followed by Lab \ncolors one byte each for L, a, b. \nThis resource is not read or used by Photoshop."},
{1067, "Alternate Spot Colors", "2 bytes (version = 1), 2 bytes channel count, \nfollowing is repeated for each count: \n4 bytes channel ID, Color : 2 bytes for space followed \nby 4 * 2 byte color component. \nThis resource is not read or used by Photoshop"},
{1069, "Layer Selection ID(s)", "2 bytes count, following is repeated \nfor each count: 4 bytes layer ID"},
{1070, "HDR Toning information"},
{1072, "Layer Group(s) Enabled ID", "1 byte for each layer in the document, \nrepeated by length of the resource. \nNOTE: Layer groups have start and end markers"},
{1073, "Color samplers resource", "Also see ID 1038 for old format"},
{1074, "Measurement Scale", "4 bytes (descriptor version = 16), Descriptor"},
{1075, "Timeline Information", "4 bytes (descriptor version = 16), Descriptor"},
{1076, "Sheet Disclosure", "4 bytes(descriptor version = 16), Descriptor"},
{1077, "DisplayInfo", "Structure to support floating point clors"},
{1078, "Onion Skins", "4 bytes (descriptor version = 16), Descriptor"},
{2000, "Path Information", "(saved paths)"},
{2999, "Name of clipping path", NULL},
{4000, "Plug-In resource(s)", "Resources added by a plug-in"},
{7000, "Image Ready variables", "XML representation of variables definition"},
{7001, "Image Ready data sets"},
{8000, "Lightroom workflow", "If present the document is in the middle of a Lightroo"},
{10000, "Print flags information", "2 bytes version(= 1), \n1 byte center crop marks, \n1 byte(= 0), 4 bytes bleed \nwidth value, 2 bytes bleed width scale"}
};

class MetadataPhotoshopImageResourceDataset
{
public:
	LPBYTE _src;
	int _resId;
	int _offset;
	int _length;
	simpleList<BYTE> _dset;

	MetadataPhotoshopImageResourceDataset(USHORT resId, int offset) : _src(NULL), _resId(resId), _offset(offset), _length(0) {}

	virtual int parse(LPBYTE src, int remainingLen)
	{
		_length = remainingLen;
		_src = src;
		_dset.add(src, _length);
		return _length;
	}

	virtual astring name() const
	{
		int i = resourceName();
		if (i != -1)
		{
			if (PIRIdNameMaps[i].Comment)
			{
				astring2 n("RESOURCE ID %d (0x%02X)\n%s\n[%s]", _resId, _resId, PIRIdNameMaps[i].Name, PIRIdNameMaps[i].Comment);
				return n;
			}
			astring2 n("RESOURCE ID %d (0x%02X)\n%s", _resId, _resId, PIRIdNameMaps[i].Name);
			return n;
		}
		astring2 n("RESOURCE ID %d (0x%02X)", _resId, _resId);
		return n;
	}
	virtual astring value() const
	{
		astring2 v;
		if (_isTextData(_dset.data(), (int)_dset.size()))
			v.assign((LPCSTR)_dset.data(), (int)_dset.size());
		else if (_dset.size() <= 2 * 16 + 8)
			v.hexdump(_dset.data(), (int)_dset.size(), ASTRHEXDUMP_FLAG_NO_OFFSET | ASTRHEXDUMP_FLAG_NO_ASCII | ASTRHEXDUMP_FLAG_NO_ENDING_LF);
		else
			v.hexdump(_dset.data(), 2 * 16 + 8, ASTRHEXDUMP_FLAG_NO_OFFSET | ASTRHEXDUMP_FLAG_NO_ASCII | ASTRHEXDUMP_FLAG_NO_ENDING_LF | ASTRHEXDUMP_FLAG_ELLIPSIS);
		v.trim();
		return v;
	}

	int resourceName() const
	{
		for (int i = 0; i < ARRAYSIZE(PIRIdNameMaps); i++)
		{
			if (PIRIdNameMaps[i].Id == _resId)
				return i;
		}
		return -1;
	}

protected:
	bool _isTextData(LPBYTE src, int srcLen) const
	{
		for (int i = 0; i < srcLen; i++)
		{
			char c = src[i];
			if (c >= ' ')
				continue;
			if (c == '\n' || c == '\r' || c == '\t')
				continue;
			return false;
		}
		return true;
	}
};

#pragma pack(1)
struct IIMDataSetHeader
{
	BYTE TagMarker;
	BYTE RecordNum;
	BYTE DatasetNum;
	USHORT DataLength;
};

struct PASSTRING
{
	USHORT Length;
	char String[1];
};
#pragma pack()

class MetadataPhotoshopImageResource : public objList<MetadataPhotoshopImageResourceDataset>
{
public:
	MetadataPhotoshopImageResource(int offset) : _offset(offset), _headerLen(0) {}

#define PhotoshopImageResource_HeaderLength (4/*sizeof("8BIM")*/ + sizeof(USHORT) + FIELD_OFFSET(PASSTRING, String) + sizeof(ULONG))

	int _offset;
	int _headerLen;
	USHORT _resId;
	ULONG _resLen;
	objList<MetadataPhotoshopImageResourceDataset> _dsets;

	int parse(LPBYTE src, int remainingLe)
	{
		/* image resource block:
		4 bytes: signature '8BIM'
		2 bytes: resource ID (>= 1000)
		variable: name (a pascal string); or a null name made of 2 bytes of 0.
		4 bytes: size of resource data that follows.
		variable: the resource data.
		*/
		LPBYTE p = src + 4; // 4 for wcslen("8BIM")
		_resId = *(USHORT*)p;
		_reverseByteOrder(&_resId, sizeof(_resId));
		p += sizeof(USHORT);
		PASSTRING *ps = (PASSTRING*)p;
		p += FIELD_OFFSET(PASSTRING, String) + ps->Length;
		_resLen = *(ULONG*)p;
		_reverseByteOrder(&_resLen, sizeof(_resLen));
		p += sizeof(ULONG);
		_headerLen = PhotoshopImageResource_HeaderLength + ps->Length;
		int offset2 = _offset + (int)(p - src);
		switch (_resId)
		{
		case 0x0404: // 1028 - IPTC file information
			parseIPTCMetadata(p, _resLen, offset2);
			break;
		case 0x0425: // 1061 - Caption digest. 16 bytes: RSA Data Security, MD5 message-digest algorithm
			parseCaptionDigest(p, _resLen, offset2);
			break;
		default:
			parseDatasetDefault(p, _resLen, offset2);
			break;
		}
		return _resLen + PhotoshopImageResource_HeaderLength;
	}

protected:
	int parseDatasetDefault(LPBYTE srcData, int srcLen, int offset)
	{
		MetadataPhotoshopImageResourceDataset *dset = new MetadataPhotoshopImageResourceDataset(_resId, offset);
		int len = dset->parse(srcData, srcLen);
		_dsets.add(dset);
		return len;
	}
	int parseIPTCMetadata(LPBYTE srcData, int srcLen, int offset);
	int parseCaptionDigest(LPBYTE srcData, int srcLen, int offset);
};

class MetadataPhotoshopIIMDataSet : public IIMDataSetHeader, public MetadataPhotoshopImageResourceDataset
{
public:
	MetadataPhotoshopIIMDataSet(USHORT resId, int offset) : MetadataPhotoshopImageResourceDataset(resId, offset) {}

	virtual int parse(LPBYTE src, int remainingLen)
	{
		_src = src;
		IIMDataSetHeader *p = (IIMDataSetHeader *)src;
		TagMarker = p->TagMarker;
		RecordNum = p->RecordNum;
		DatasetNum = p->DatasetNum;
		DataLength = p->DataLength;
		_reverseByteOrder(&DataLength, sizeof(USHORT));
		int ret = remainingLen - (int)DataLength - sizeof(IIMDataSetHeader);
		if (ret >= 0)
			_dset.add(src + sizeof(IIMDataSetHeader), DataLength);
		_length = DataLength + sizeof(IIMDataSetHeader);
		return _length;
	}
	virtual astring name() const
	{
		astring2 n("IIM %02X", DatasetNum);
		return n;
	}
};

/* reference: https://iptc.org/std/IIM/4.2/specification/IIMV4.2.pdf
see Section 1.5 Tags.

dataset tag:
byte 1: tag marker
byte 2: record number
byte 3: dataset number
bytes 4-5: data field length in bytes
*/
int MetadataPhotoshopImageResource::parseIPTCMetadata(LPBYTE srcData, int srcLen, int offset)
{
	int c = 0, len;
	while (srcData && srcLen > 0)
	{
		MetadataPhotoshopIIMDataSet *dset = new MetadataPhotoshopIIMDataSet(_resId, offset);
		len = dset->parse(srcData, srcLen);
		_dsets.add(dset);
		srcLen -= len;
		srcData += len;
		offset += len;
		c++;
	}
	return c;
}

class MetadataPhotoshopCaptionDigest : public MetadataPhotoshopImageResourceDataset
{
public:
	simpleList<BYTE> _dset;

	MetadataPhotoshopCaptionDigest(USHORT resId, int offset) : MetadataPhotoshopImageResourceDataset(resId, offset) {}

	int parse(LPBYTE src, int remainingLen)
	{
		if (remainingLen < 16)
			return false;
		_length = 16;
		_src = src;
		_dset.add(src, 16);
		return _length;
	}
	virtual astring name() const
	{
		return "Caption Digest";
	}
	virtual astring value() const
	{
		astring2 v;
		v.hexdump(_dset.data(), (int)_dset.size(), ASTRHEXDUMP_FLAG_NO_OFFSET | ASTRHEXDUMP_FLAG_NO_ASCII | ASTRHEXDUMP_FLAG_NO_ENDING_LF);
		v.trim();
		return v;
	}
};

int MetadataPhotoshopImageResource::parseCaptionDigest(LPBYTE srcData, int srcLen, int offset)
{
	MetadataPhotoshopCaptionDigest *dset = new MetadataPhotoshopCaptionDigest(_resId, offset);
	dset->parse(srcData, srcLen);
	_dsets.add(dset);
	return 1;
}

class MetadataPhotoshop : public MetadataEndian, public objList<MetadataPhotoshopImageResource>
{
public:
	MetadataPhotoshop() : MetadataEndian(true), _signatureLen(0) {}

	int _signatureLen;

	int parse(MetaJpegInfo::SEGMENTDATA *sd)
	{
		if (sd->DataLength < 10)
			return -1;
		if (0 != memcmp(sd->Data, "Photoshop", 9))
			return -1;
		_signatureLen = (int)strlen((LPCSTR)sd->Data) + 1;
		return parse(sd->Data + _signatureLen, sd->DataLength - _signatureLen);
	}

	/* reference: https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
	*/
	int parse(LPBYTE metaData, LONG metaLength)
	{
		LPBYTE p0 = metaData;
		int remainingLen = metaLength;
		int offset = _signatureLen;
		int c = 0;
		int resLen;
		while (remainingLen > PhotoshopImageResource_HeaderLength && 0 == memcmp(p0, "8BIM", 4))
		{
			MetadataPhotoshopImageResource *res = new MetadataPhotoshopImageResource(offset);
			resLen = res->parse(p0, remainingLen);
			objList<MetadataPhotoshopImageResource>::add(res);
			p0 += resLen;
			// check a null-byte padding for an odd-numbered ending address.
			if ((resLen & 0x1) && *p0 == 0 && remainingLen > resLen)
			{
				// skip it. bump up the resource length by one.
				p0++;
				resLen++;
			}
			remainingLen -= resLen;
			offset += resLen;
			c++;
		}
		return c;
	}
};


class MetaJpegScan : public MetaJpegInfo
{
public:
	MetaJpegScan(ScanTag *sr, FILEREADINFO2 *fi) : MetaJpegInfo(sr, fi), _sr(sr), _scanOffset(0)
	{
		_bmv = sr->_bmv;
	}

protected:
	ScanTag *_sr;
	ULONG _scanOffset;

	virtual void onScanSegment(int segCount, SEGMENTDATA *sd, LARGE_INTEGER markerOffset, ULONG markerLength, LARGE_INTEGER payloadOffset, ULONG payloadLength)
	{
		int parentRegion;
		ustring smarker(CP_ACP, _SegMarkerName(sd->SegMarker.SegType));

		if (sd->SegMarker.SegType == JPEGSegMarkerEndOfImage && _scanOffset)
		{
			ULONG scanLen = markerOffset.LowPart - _scanOffset;
			_imgdataRegion = _sr->_bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST | BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND | BMVADDANNOT_FLAG_GRAYTEXT, { _scanOffset }, scanLen, L"Scanned Image Data\n %d (0x%X) bytes", scanLen, scanLen);
		}
		else if (sd->SegMarker.SegType == JPEGSegMarkerApp0)
		{
			if (sd->DataLength == sizeof(APP0_JFIF) && 0 == memcmp(sd->Data, "JFIF", 4))
			{
				APP0_JFIF *jfif = (APP0_JFIF*)sd->Data;
				_reverseByteOrder(&jfif->DensityX, sizeof(USHORT));
				_reverseByteOrder(&jfif->DensityY, sizeof(USHORT));
				ustring sthumbnail;
				if (jfif->ThumbnailX != 0 && jfif->ThumbnailY != 0)
					sthumbnail.format(L"%d x %d", jfif->ThumbnailX, jfif->ThumbnailY);
				else
					sthumbnail.assign(L"(NA)");
				_sr->_bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST, markerOffset, markerLength + sd->DataLength, L"%s - JFIF Header\n Version: %d.%d\n Density: %d x %d %s\n Thumbnail: %s", (LPCWSTR)smarker, jfif->MajorVersion, jfif->MinorVersion, jfif->DensityX, jfif->DensityY, _DensityUnitString(jfif->DensityUnit), (LPCWSTR)sthumbnail);
				return;
			}
			if (sd->DataLength == sizeof(APP0_JFXX) && 0 == memcmp(sd->Data, "JFXX", 4))
			{
				APP0_JFXX *jfxx = (APP0_JFXX*)sd->Data;
				_sr->_bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST, markerOffset, markerLength + sd->DataLength, L"%s - JFXX Header\n Thumbnail format: %s", (LPCWSTR)smarker, _ThumbnailFormatString(jfxx->ThumbnailFormat));
				return;
			}
		}
		else if (sd->SegMarker.SegType == JPEGSegMarkerApp1)
		{
			MetadataExif exif(_sr->_bmv, markerOffset.LowPart - sizeof(SEGMENTMARKER));
			if (exif.parse(sd) >= 0)
			{
				parentRegion = _sr->_bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST, markerOffset, markerLength + exif._headerLen, L"%s - Exif Image Resource\n contains %d IFDs", (LPCWSTR)smarker, exif.size());
				for (int i = 0; i < (int)exif.size(); i++)
				{
					ULONG offset0 = markerOffset.LowPart + markerLength + exif._baseLen;

					MetadataExifIfd *res = exif[i];
					size_t n = res->size();
					if (n == 0)
						continue;

					_sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND, parentRegion), { offset0 + res->_offset }, res->_headerLen, L"%d - %S\n %d datasets", i + 1, (LPCSTR)res->_name, res->size());

					MetadataExifIfdAttribute *dset;
					int j;
					for (j = 0; j < (int)n; j++)
					{
						dset = res->at(j);
						_sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND, parentRegion), { offset0 + dset->_offset }, dset->_headerLen, L"%S\n%S", (LPCSTR)dset->_name, (LPCSTR)dset->getText());
					}
					for (j = 0; j < (int)n; j++)
					{
						dset = res->at(j);
						if (dset->_valueOffset)
						{
							_sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND | BMVADDANNOT_FLAG_GRAYTEXT, parentRegion), { offset0 + dset->_valueOffset }, (ULONG)dset->_value.size(), L"Value of IFD attribute\n%S", (LPCSTR)dset->_name);
						}
					}
				}
				exif.parseThumbnail(parentRegion, true);
				return;
			}
			MetadataXMP xmp;
			if (xmp.parse(sd))
			{
				parentRegion = _sr->_bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST, markerOffset, markerLength + xmp._headerLen, L"%s - XMP Meta Data\n %d bytes", (LPCWSTR)smarker, xmp._xmp._length);

				xmp.markXmpTags(markerOffset.QuadPart + markerLength, parentRegion, _bmv);
				return;
			}
		}
		else if (sd->SegMarker.SegType == JPEGSegMarkerApp2)
		{
			MetadataICCP iccp;
			if (iccp.parse(sd))
			{
				parentRegion = _sr->_bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST, markerOffset, markerLength + iccp._headerLen, L"%s - ICC Profile %d/%d\n %d bytes", (LPCWSTR)smarker, iccp._profileNum, iccp._profileTotal, iccp._profileLen);

				iccp.markProfileFields(markerOffset.LowPart + markerLength, parentRegion, _sr->_bmv);
				return;
			}
		}
		else if (sd->SegMarker.SegType == JPEGSegMarkerApp13)
		{
			MetadataPhotoshop pshop;
			if (pshop.parse(sd) >= 0)
			{
				parentRegion = _sr->_bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST, markerOffset, markerLength + pshop._signatureLen, L"%s - Photoshop Image Resource\n contains %d sections", (LPCWSTR)smarker, pshop.size());
				for (int i = 0; i < (int)pshop.size(); i++)
				{
					MetadataPhotoshopImageResource *res = pshop[i];
					_sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_SEMIOPAQUEBACKGROUND, parentRegion), { markerOffset.LowPart + markerLength + res->_offset }, res->_headerLen, L"Resource Section %d\n ID %d\n %d bytes\n %d datasets", i + 1, res->_resId, res->_resLen, res->_dsets.size());

					for (int j = 0; j < (int)res->_dsets.size(); j++)
					{
						MetadataPhotoshopImageResourceDataset *dset = res->_dsets[j];
						_sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND, parentRegion), { markerOffset.LowPart + markerLength + dset->_offset }, dset->_length, L"%S\n%S", (LPCSTR)dset->name(), (LPCSTR)dset->value());
					}
				}
				return;
			}
		}

		parentRegion = _sr->_bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST, markerOffset, markerLength, L"JPEG Segment %s\n (%S)\n Tag 0x%X, Length %d bytes", (LPCWSTR)smarker, _SegMarkerLongName(sd->SegMarker.SegType), sd->SegMarker.SegType, sd->DataLength);

#ifdef SCANTAG_ADDS_LOGO
		if (sd->SegMarker.SegType == JPEGSegMarkerStartOfImage)
		{
			DUMPANNOTATIONINFO *ai = _bmv->openAnnotationOfRegion(parentRegion, true);
			if (ai)
			{
				HBITMAP hbmp = LoadBitmap(LibResourceHandle, MAKEINTRESOURCE(IDB_JPEG_LOGO));
				_bmv->_annotations.setPicture(ai, hbmp);
				DeleteObject(hbmp);
			}
		}
#endif//#ifdef SCANTAG_ADDS_LOGO
		if (sd->DataLength > 0)
		{
			LARGE_INTEGER offset = markerOffset;
			offset.LowPart += markerLength;
			_sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND | BMVADDANNOT_FLAG_GRAYTEXT, parentRegion), offset, sd->DataLength, L"%s Data\n %d (0x%X) bytes", (LPCWSTR)smarker, sd->DataLength, sd->DataLength);
			if (sd->SegMarker.SegType == JPEGSegMarkerStartOfScan)
				_scanOffset = markerOffset.LowPart + markerLength + sd->DataLength;
		}
	}

	virtual HRESULT _createImageBitmap(HBITMAP *hbmpOut)
	{
		HRESULT hr;
		LPSTREAM ps;
		hr = CreateStreamOnHGlobal(NULL, TRUE, &ps);
		if (hr == S_OK)
		{
			_rofa->fseek(_fi, 0, ROFSEEK_ORIGIN_BEGIN);
			hr = _rofa->fcopyTo(ps, _fi);
			if (hr == S_OK)
			{
				CodecJPEG img;
				hr = img.Load(ps);
				if (hr == S_OK)
				{
					HDC hdc0 = CreateDC(L"DISPLAY", NULL, NULL, NULL);
					HDC hdc = CreateCompatibleDC(hdc0);
					hr = img.CreateBitmap(hdc, hbmpOut); // this generates a top-down bitmap.
					if (hr == S_OK)
						hr = RefitBitmapToViewFrame(hdc, _bmv->_vi.FrameSize, true, hbmpOut);
					DeleteDC(hdc);
					DeleteDC(hdc0);
				}
			}
			ps->Release();
		}
		return hr;
	}
};

DWORD ScanTagJpg::runInternal(FILEREADINFO2 *fi)
{
	MetaJpegScan jpgscan(this, fi);
	jpgscan.collect();

	return fi->ErrorCode;
}


////////////////////////////////////////////////////////////////////

class MetaBmpIcoScan : public MetaBmpScan
{
public:
	MetaBmpIcoScan(ScanTag *sr, FILEREADINFO2 *fi, LPICONDIRENTRY dirent, int parentRegion) : MetaBmpScan(sr, fi, dirent->dwBytesInRes), _sr(sr), _fi(fi), _dirent(dirent), _baseOffset{ fi->DataOffset.LowPart + fi->DataUsed } {
		_parentRegion = parentRegion;
		_pixdataLabel = L"XOR Mask Image"; // override the base class label.
	}

	bool markANDMask()
	{
		LARGE_INTEGER offsetToAND = _getOffsetToPixelData();
		offsetToAND.QuadPart += _imgSize; // add the byte size of the XOR mask image.
		// a 1-bpp AND mask follows the XOR mask.
		USHORT cx = _dirent->bWidth ? MAKEWORD(_dirent->bWidth, 0) : 256;
		ULONG scanlineLen = SCANLINELENGTH_1BPP(cx);
		ULONG imgLen = scanlineLen * (_bmph.bV5Height- _bmph.bV5Width);

		if (ERROR_SUCCESS == _sr->fseek(_fi, (LONG)offsetToAND.LowPart, ROFSEEK_ORIGIN_BEGIN))
		{
			int regionId = _sr->_bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST | BMVADDANNOT_FLAG_TRANSPARENTBACKGROUND | BMVADDANNOT_FLAG_GRAYTEXT, _parentRegion), offsetToAND, imgLen, L"AND Mask Image\n Dimensions: %d x %d pixels\n Scan line: %d bytes\n Image size: %d bytes", _bmph.bV5Width, _getImageHeight(), scanlineLen, imgLen);

			LPBYTE imgData = (LPBYTE)malloc(imgLen);
			if (_sr->fread(imgData, imgLen, 1, _fi))
			{
#ifdef _DEBUG
				ustring fname = L"%tmp%\\" APP_WORK_FOLDER L"\\";
				ustring ftitle = _sr->_bmv->getFileTitle();
				ustring2 fsuffix(L"(AND_MASK_%dx%d)", cx, cx);
				fname += ftitle;
				fname += fsuffix;
				fname += L".bmp";
				_dumpANDMask(fname, imgData, imgLen, scanlineLen);
#endif//_DEBUG
				HBITMAP hbmp = _createBitmapFromANDMask(imgData, imgLen, scanlineLen);
				if (hbmp)
				{
					CRegionCollection &r = _sr->_bmv->_regions;
					AnnotationCollection &ac = _sr->_bmv->_annotations;
					int annotId = ac.queryByCtrlId(r[regionId - 1].AnnotCtrlId);
					if (annotId >= 0)
					{
						AnnotationOperation annot(ac[annotId], &ac);
						annot.attachThumbnail(hbmp);
					}
					else
						DeleteObject(hbmp);
				}
			}
			free(imgData);
		}
		return false;
	}

protected:
	ScanTag *_sr;
	FILEREADINFO2 *_fi;
	LPICONDIRENTRY _dirent;
	LARGE_INTEGER _baseOffset;

	virtual LARGE_INTEGER _getOffsetToPixelData()
	{
		DWORD paletteColors = _bmph.bV5ClrUsed;
		if (paletteColors == 0 && _bmph.bV5BitCount <= 8)
			paletteColors = 1 << _bmph.bV5BitCount;
		ULONG paletteSize = paletteColors*sizeof(RGBQUAD);
		return { _baseOffset.LowPart + _bmph.bV5Size + paletteSize };
	}
	virtual int _getImageHeight()
	{
		if (_bmph.bV5Height > _bmph.bV5Width)
			return (int)(_bmph.bV5Height-_bmph.bV5Width);
		return MetaBmpScan::_getImageHeight();
	}

	HBITMAP _createBitmapFromANDMask(LPBYTE imgData, ULONG imgLen, ULONG imgScanLen)
	{
		HDC hdc0 = CreateDC(L"DISPLAY", NULL, NULL, NULL);
		HDC hdc = CreateCompatibleDC(hdc0);
		LPBYTE pb = new BYTE[sizeof(BITMAPINFOHEADER) + 2 * sizeof(RGBQUAD)];
		ZeroMemory(pb, sizeof(BITMAPINFOHEADER) + 2 * sizeof(RGBQUAD));
		LPBITMAPINFO bmi = (LPBITMAPINFO)pb;
		COLORREF *rgb = (COLORREF*)(pb + sizeof(BITMAPINFOHEADER));

		bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi->bmiHeader.biPlanes = 1;
		bmi->bmiHeader.biBitCount = 1;
		bmi->bmiHeader.biWidth = _bmph.bV5Width;
		bmi->bmiHeader.biHeight = _getImageHeight();
		bmi->bmiHeader.biSizeImage = imgLen;
		bmi->bmiHeader.biClrUsed = 2;
		bmi->bmiHeader.biClrImportant = 2;
		bmi->bmiColors[0] = { 0, 0, 0 };
		bmi->bmiColors[1] = { 0xff, 0xff, 0xff };

		LPBYTE bits;
		HBITMAP hbmp = CreateDIBSection(hdc, bmi, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, 0);
		if (hbmp)
		{
			HBITMAP hbm0 = (HBITMAP)SelectObject(hdc, hbmp);
			CopyMemory(bits, imgData, imgLen);
			SetDIBits(hdc, hbmp, 0, bmi->bmiHeader.biHeight, bits, bmi, DIB_RGB_COLORS);
			SelectObject(hdc, hbm0);
		}
		delete[] pb;
		DeleteDC(hdc);
		DeleteDC(hdc0);
		return hbmp;
	}
	HRESULT _dumpANDMask(LPCWSTR filename, LPBYTE imgData, ULONG imgLen, ULONG imgScanLen)
	{
		ULONG scanLen = SCANLINELENGTH_32BPP(_bmph.bV5Width);
		BITMAPFILEHEADER bfh = { 0 };
		bfh.bfType = 0x4d42; // 'BM'
		bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
		bfh.bfSize = bfh.bfOffBits + scanLen *_getImageHeight();
		BITMAPINFOHEADER bih = { sizeof(BITMAPINFOHEADER) };
		bih.biPlanes = 1;
		bih.biBitCount = 32;
		bih.biWidth = _bmph.bV5Width;
		bih.biHeight = _getImageHeight();
		bih.biSizeImage = scanLen * bih.biHeight;

		HRESULT hr = S_OK;
		WCHAR buf[MAX_PATH];
		ExpandEnvironmentStrings(filename, buf, ARRAYSIZE(buf));
		HANDLE hf = CreateFile(buf, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
		if (hf == INVALID_HANDLE_VALUE)
			return HRESULT_FROM_WIN32(GetLastError());
		ULONG len;
		WriteFile(hf, &bfh, sizeof(bfh), &len, NULL);
		WriteFile(hf, &bih, sizeof(bih), &len, NULL);

		LPBYTE tAND = new BYTE[scanLen];
		int i0, x, y;
		int i, j, k, q, qmask;
		for (i = 0; i < bih.biHeight; i++)
		{
			/*// the rows of pixels are laid down bottom up. so, reverse the reading order.
			i0 = bih.biHeight - i - 1;
			*/
			i0 = i;
			// need a buffer to hold position-reversing bits and 
			// sum bits that fall on same screen column.
			ZeroMemory(tAND, scanLen);
			for (j = 0; j < bih.biWidth; j++)
			{
				k = j / 8; // which byte in the scan line?
				q = j % 8; // which bit in the byte?
				qmask = 1 << q; // form a mask to isolate the target bit.
				BYTE b = imgData[i0*imgScanLen+k]; // get the byte containing the bit.
				y = (b & qmask); // this is the AND bit.
				if (y)
				{
					// byte's highest order bit (8th) holds color of left-most pixel
					x = 8 * k + 7 - q;
					ASSERT(tAND[x]==0);
					tAND[x] = 1;
				}
			}
			for (j = 0; j < bih.biWidth; j++)
			{
				COLORREF rgbq = tAND[j] ? RGB(0, 0, 0) : RGB(0xff, 0xff, 0xff);
				len = 0;
				WriteFile(hf, &rgbq, sizeof(rgbq), &len, NULL);
			}
		}

		CloseHandle(hf);

		delete[] tAND;
		return hr;
	}
};


DWORD ScanTagIco::runInternal(FILEREADINFO2 *fi)
{
	LPICONDIR icondir = (LPICONDIR)freadp(FIELD_OFFSET(ICONDIR, idEntries), fi);
	if (icondir)
	{
		_bmv->addTaggedRegion(0, { 0 }, FIELD_OFFSET(ICONDIR, idEntries), L"Icon Directory\n Type: %d\n Entry count: %d", icondir->idType, icondir->idCount);
		if (icondir->idType == 1)
		{
			int dirCount = icondir->idCount;
			for (int i = 0; i < dirCount; i++)
			{
				ICONDIRENTRY dirent;
				if (!fread(&dirent, sizeof(ICONDIRENTRY), 1, fi))
					break;
				int parentRegion = _bmv->addTaggedRegion(BMVADDANNOT_FLAG_ADD_TO_SUMMARYLIST, fi->PriorStart, fi->PriorLength, L"Directory Entry %d\n Dimensions: %d x %d pixels\n Number of colors: %d\n Bit count: %d\n Size of image data: 0x%X\n Offset to data: 0x%X", i+1, dirent.bWidth? dirent.bWidth:256, dirent.bHeight? dirent.bHeight:256, dirent.bColorCount, dirent.wBitCount, dirent.dwBytesInRes, dirent.dwImageOffset);
				// save the current offset. we will come back to it after the image analysis.
				LARGE_INTEGER pos0 = fpos(fi);
				// jump to where the image data is.
				fseek(fi, (LONG)dirent.dwImageOffset, ROFSEEK_ORIGIN_BEGIN);
				// first, test for PNG.
				MetaPngNestedScan png(this, fi, dirent.dwBytesInRes, parentRegion);
				if (png.isPNG(true))
				{
					png.collect();
				}
				// otherwise, the image is a BMP.
				else
				{
					MetaBmpIcoScan ico(this, fi, &dirent, parentRegion);
					if (ico.collect())
					{
						ico.markANDMask();
					}
				}
				// go back to the directory table.
				if (ERROR_SUCCESS != fseek(fi, (LONG)pos0.LowPart, ROFSEEK_ORIGIN_BEGIN))
					break;
				// go read the next directory entry.
			}
			fi->ErrorCode = ERROR_SUCCESS;
		}
		else
			fi->ErrorCode = ERROR_INVALID_BLOCK;
	}
	else
		fi->ErrorCode = ERROR_INVALID_BLOCK;
	return fi->ErrorCode;
}


////////////////////////////////////////////////////////////////////

bool _textIsWide(const FILEREADINFO2 *fi)
{
	// look for wide space, cr, and lf.
	int spc = 0;
	int cr = 0;
	int lf = 0;
	int num = 0;
	int alpha = 0;
	WCHAR *p = (WCHAR*)fi->DataBuffer;
	int i, imax = fi->DataLength/sizeof(WCHAR);
	for (i = 0; i < imax; i++)
	{
		WCHAR wc = p[i];
		if (wc == 0x0020)
			spc++;
		else if (wc == 0x000d)
			cr++;
		else if (wc == 0x000a)
			lf++;
		else if (0x0030 <= wc && wc <= 0x0039)
			num++;
		else if ((0x0041 <= wc && wc <= 0x005a) || (0x0061 <= wc && wc <= 0x007a))
			alpha++;
	}
	if ((spc > 0 && lf > 0) && (num || alpha))
		return true;
	return false;
}

TEXTBYTEORDERMARK _find8bitTextEncoding(const FILEREADINFO2 *fi)
{
	LPBYTE p = fi->DataBuffer;
	int i, imax;
	for (i = 0; i < (int)fi->DataLength; i++)
	{
		if (p[i] & 0x80)
			break;
	}
	if (i == fi->DataLength)
		return TEXT_ASCII; // it's 7-bit ascii

	imax = fi->DataLength - 2;
	BYTE c1, c2, c3;
	for (i = 0; i < imax; i++)
	{
		c1 = p[i];
		if (!(c1 & 0x80))
			continue;
		if (0xc0 <= c1 && c1 <= 0xdf)
		{
			// 2-byte utf-8 sequence?
			c2 = p[++i];
			if (0x80 <= c2 && c2 <= 0xbf)
				continue; // the char is utf-8 in unicode range of 0x0080..0x07ff
		}
		else if (0xe0 <= c1 && c1 <= 0xef)
		{
			// 3-byte utf-8 sequence?
			c2 = p[++i];
			if (0x80 <= c2 && c2 <= 0xbf)
			{
				c3 = p[++i];
				if (0x80 <= c3 && c3 <= 0xbf)
					continue; // the char is utf-8 in unicode range of 0x0800..0xffff
			}
		}
		return TEXT_GENERIC; // 8-bit extended-ascii, i.e., latin-1
	}
	return TEXT_UTF8_NOBOM; // likely utf-8
}

DWORD ScanTagBOM::runInternal(FILEREADINFO2 *fi)
{
	fi->ErrorCode = ERROR_INVALID_BLOCK;
	LPBYTE src;
	int i;
	for (i = 0; i < _bomSize; i++)
	{
		src = freadp(_boms[i].DataLength, fi);
		if (!src)
		{
			_msg.format(ustring2(IDS_SM_BOM_START_FAIL), fi->ErrorCode, (LPCWSTR)ustring3(fi->ErrorCode));
			return fi->ErrorCode;
		}
		if (fi->ServedLength == _boms[i].DataLength)
		{
			if (0 == memcmp(src, _boms[i].Data, _boms[i].DataLength))
			{
				_bmv->_bom = _boms[i].BOM;
				_bmv->addTaggedRegion(0, { 0 }, _boms[i].DataLength, L"BOM: %S", _boms[i].Description);
				fi->ErrorCode = ERROR_SUCCESS;
				break;
			}
		}
		// rewind
		fi->RemainingLength += fi->ServedLength;
		fi->DataUsed -= fi->ServedLength;
	}
	if (fi->ErrorCode == ERROR_SUCCESS && _bmv->_bom == TEXT_UNKNOWN)
	{
		// if no bom is present, make a guess.
		if (_textIsWide(fi))
			_bmv->_bom = TEXT_UTF16_NOBOM; // likely unicode
		else
			_bmv->_bom = _find8bitTextEncoding(fi);
	}

	if (fi->ErrorCode == ERROR_SUCCESS)
		_msg.format(ustring2(IDS_SM_BOM_COMPLETED), _toTextEncodingName());
	else
		_msg.format(ustring2(IDS_SM_BOM_SCAN_FAIL), fi->ErrorCode, (LPCWSTR)ustring3(fi->ErrorCode));

	if (_wantLFs && fi->ErrorCode == ERROR_SUCCESS)
	{
		int count = 0;
		DWORD offset;
		int parentId = 0;
		if (_bmv->_bom <= TEXT_UTF8_NOBOM)
		{
			// 7- or 8-bit encoding, e.g., ascii and utf-8
			char marker = 0;
			while (fread(&marker, 1, 1, fi))
			{
				if (marker == '\r')
				{
					offset = fi->PriorStart.LowPart;
					if (fread(&marker, 1, 1, fi) && marker == '\n')
					{
						parentId = _bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_MASK_PARENTREGION, parentId), { offset }, 2, parentId? NULL : L"CR LF");
					}
					else
					{
						parentId = _bmv->addTaggedRegion((BMVADDANNOT_MASK_PARENTREGION, parentId), { offset }, 1, parentId ? NULL : L"CR");
					}
					count++;
				}
				else if (marker == '\n')
				{
					parentId = _bmv->addTaggedRegion((BMVADDANNOT_MASK_PARENTREGION, parentId), fi->PriorStart, 1, parentId ? NULL : L"LF");
					count++;
				}
			}
		}
		else // assuming 16-bit unicode
		{
			WCHAR marker = 0;
			while (fread(&marker, sizeof(WCHAR), 1, fi))
			{
				if (marker == '\r')
				{
					offset = fi->PriorStart.LowPart;
					if (fread(&marker, sizeof(WCHAR), 1, fi) && marker == '\n')
					{
						parentId = _bmv->addTaggedRegion(MAKELONG(BMVADDANNOT_MASK_PARENTREGION, parentId), { offset }, 2*sizeof(WCHAR), parentId ? NULL : L"CR LF (Unicode)");
					}
					else
					{
						parentId = _bmv->addTaggedRegion((BMVADDANNOT_MASK_PARENTREGION, parentId), { offset }, 1 * sizeof(WCHAR), parentId ? NULL : L"CR (Unicode)");
					}
					count++;
				}
				else if (marker == '\n')
				{
					parentId = _bmv->addTaggedRegion((BMVADDANNOT_MASK_PARENTREGION, parentId), fi->PriorStart, 1 * sizeof(WCHAR), parentId ? NULL : L"LF (Unicode)");
					count++;
				}
			}
		}

		if (count++)
		{
			ustring s;
			s.format(ustring2(IDS_SM_BOM_CRLF_DETECTED), count);
			_msg.append(s);
		}
	}

	return fi->ErrorCode;
}

