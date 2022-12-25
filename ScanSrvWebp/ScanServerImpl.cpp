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
#include "resource.h"
#include "ScanServerImpl.h"

#define SCANLOOP_ADDS_LOGO


ScanServerImpl::ScanServerImpl() : _site(NULL), _data(NULL)
{
}

ScanServerImpl::~ScanServerImpl()
{
}

STDMETHODIMP ScanServerImpl::Init(IHexTabScanSite *ScanSite, IHexTabScanData *SourceData)
{
	_site = ScanSite;
	_data = SourceData;
	return S_OK;
}

STDMETHODIMP ScanServerImpl::Term()
{
	_site.release();
	_data.release();
	return S_OK;
}

STDMETHODIMP ScanServerImpl::Scan()
{
	CHUNK_HEADER hdr;
	ULONG len = sizeof(hdr);
	HRESULT hr = _data->Read(&hdr, &len);
	if (hr != S_OK ||
		0 != memcmp(hdr.ChunkId, "RIFF", sizeof(hdr.ChunkId)) ||
		0 != memcmp(hdr.Signature, "WEBP", sizeof(hdr.Signature)))
	{
		_msg = L"Not WebP";
		return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
	}
	LARGE_INTEGER offset = { 0 };
	short tagIndex;
	hr = _site->TagData(SCANSITETAGDATA_FLAG_ADD_TO_SUMMARYLIST, offset.QuadPart, len, bstring(L"WebP header"), 0, &tagIndex);
	if (hr != S_OK)
		return hr;
#ifdef SCANLOOP_ADDS_LOGO
	PICTDESC desc = { sizeof(PICTDESC), PICTYPE_BITMAP };
	desc.bmp.hbitmap = LoadBitmap(LibResourceHandle, MAKEINTRESOURCE(IDB_WEBPLOGO));
	IPicture *ipic;
	hr = OleCreatePictureIndirect(&desc, IID_IPicture, TRUE, (LPVOID*)&ipic);
	if (hr != S_OK)
	{
		_msg = L"Logo image could not be opened";
		return hr;
	}
	VARIANT var;
	var.vt = VT_UNKNOWN;
	var.punkVal = ipic;
	hr = _site->Annotate(tagIndex, &var);
	ipic->Release();
	if (hr != S_OK)
		return hr;
#endif//#ifdef SCANLOOP_ADDS_LOGO

	offset.QuadPart += len;

	CHUNK_RIFF chunk;
	len = FIELD_OFFSET(CHUNK_RIFF, Payload);
	while (S_OK == (hr = _data->Read(&chunk, &len)))
	{
		char chunkId[6] = { 0 };
		memcpy(chunkId, chunk.ChunkId, sizeof(chunk.ChunkId));
#ifdef SCANLOOP_READS_ALL_OF_PAYLOAD
		// read the payload
		LPBYTE p = (LPBYTE)malloc(chunk.ChunkSize);
		if (!p)
		{
			_msg = L"Out of memory";
			hr = E_OUTOFMEMORY;
			break;
		}
		len = chunk.ChunkSize;
		hr = _data->Read(p, &len);
		if (hr != S_OK)
		{
			free(p);
			_msg = L"Read fault";
			hr = HRESULT_FROM_WIN32(ERROR_READ_FAULT);
			break;
		}
		if (memcmp(chunk.ChunkId, "VP8 ", 4) == 0)
		{
			PAYLOAD_VP8 *vp8 = (PAYLOAD_VP8 *)p;
			VP8_FRAMETAG *ftag = (VP8_FRAMETAG*)&vp8->FrameTag;
			ustring2 note(L"VP8 ImageSize: %d x %d\r\nFrameType: %d\r\nVersion: %d\r\nShowFrame: %d\r\n1stDataPartition: %d", vp8->Width, vp8->Height, ftag->m.FrameType, ftag->m.Version, ftag->m.ShowFrame, ftag->m.DataPartition1);
			int chunkLen = CHUNKSIZEOF(PAYLOAD_VP8);
			hr = _site->TagData(0, offset.QuadPart, chunkLen, bstring(note), 0, &tagIndex);
			if (hr == S_OK)
				hr = _site->TagData(SCANSITETAGDATA_FLAG_TRANSPARENTBACKGROUND | SCANSITETAGDATA_FLAG_GRAYTEXT, offset.QuadPart + chunkLen, len-sizeof(PAYLOAD_VP8), bstring(L"VP8 image data"), tagIndex, NULL);
		}
		free(p);
#else//#ifdef SCANLOOP_READS_ALL_OF_PAYLOAD
		LPBYTE p = NULL;
		ULONG readLen = 0;
		CHUNK_TYPE ctype = getChunkType(chunk);
		if (ctype == VP8)
			readLen = sizeof(PAYLOAD_VP8);
		else if (ctype == VP8L)
			readLen = sizeof(PAYLOAD_VP8L);
		else if (ctype == VP8X)
			readLen = sizeof(PAYLOAD_VP8X);

		if (readLen)
		{
			p = (LPBYTE)malloc(readLen);
			if (!p)
			{
				_msg = L"Out of memory";
				hr = E_OUTOFMEMORY;
				break;
			}
			hr = _data->Read(p, &readLen);
			if (hr != S_OK)
			{
				free(p);
				_msg = L"Data read fault";
				hr = HRESULT_FROM_WIN32(ERROR_READ_FAULT);
				break;
			}
		}

		LONG seekLen = 0;
		if (ctype == VP8)
		{
			PAYLOAD_VP8 *vp8 = (PAYLOAD_VP8 *)p;
			VP8_FRAMETAG *ftag = (VP8_FRAMETAG*)&vp8->FrameTag;
			ustring2 note(L"Chunk VP8\n ImageSize: %d x %d\n FrameType: %d\n Version: %d\n ShowFrame: %d\n 1stDataPartition: %d", vp8->Width, vp8->Height, ftag->m.FrameType, ftag->m.Version, ftag->m.ShowFrame, ftag->m.DataPartition1);
			hr = _site->TagData(SCANSITETAGDATA_FLAG_ADD_TO_SUMMARYLIST, offset.QuadPart, CHUNKSIZEOF(PAYLOAD_VP8), bstring(note), 0, &tagIndex);
			if (hr == S_OK)
				hr = _site->TagData(SCANSITETAGDATA_FLAG_TRANSPARENTBACKGROUND | SCANSITETAGDATA_FLAG_GRAYTEXT, offset.QuadPart + CHUNKSIZEOF(PAYLOAD_VP8), chunk.ChunkSize - sizeof(PAYLOAD_VP8), bstring(L"VP8 lossy compressed image data"), tagIndex, NULL);
			seekLen = chunk.ChunkSize - sizeof(PAYLOAD_VP8);
		}
		else if (ctype == VP8L)
		{
			PAYLOAD_VP8L *vp8l = (PAYLOAD_VP8L *)p;
			ustring2 note(L"Chunk VP8L\n ImageSize: %d x %d\n AlphaIsUsed: %d\n Version: %d", vp8l->Width, vp8l->Height, vp8l->AlphaIsUsed, vp8l->Version);
			hr = _site->TagData(SCANSITETAGDATA_FLAG_ADD_TO_SUMMARYLIST, offset.QuadPart, CHUNKSIZEOF(PAYLOAD_VP8L), bstring(note), 0, &tagIndex);
			if (hr == S_OK)
				hr = _site->TagData(SCANSITETAGDATA_FLAG_TRANSPARENTBACKGROUND | SCANSITETAGDATA_FLAG_GRAYTEXT, offset.QuadPart + CHUNKSIZEOF(PAYLOAD_VP8L), chunk.ChunkSize - sizeof(PAYLOAD_VP8L), bstring(L"VP8L lossless compressed image data"), tagIndex, NULL);
			seekLen = chunk.ChunkSize - sizeof(PAYLOAD_VP8L);
		}
		else if (ctype == VP8X)
		{
			PAYLOAD_VP8X *vp8x = (PAYLOAD_VP8X *)p;
			int cx=0, cy=0;
			memcpy(&cx, vp8x->Width, sizeof(INT24));
			memcpy(&cy, vp8x->Height, sizeof(INT24));
			cx++;
			cy++;
			ustring2 note(L"Chunk VP8X\n ImageSize: %d x %d\n Alpha: %d\n Animation: %d\n ICCP: %d\n Exif: %d\n XMP: %d", cx, cy, vp8x->Alpha, vp8x->Animation, vp8x->ICCP, vp8x->Exif, vp8x->XMP);
			hr = _site->TagData(SCANSITETAGDATA_FLAG_ADD_TO_SUMMARYLIST, offset.QuadPart, CHUNKSIZEOF(PAYLOAD_VP8X), bstring(note), 0, &tagIndex);
			seekLen = chunk.ChunkSize - sizeof(PAYLOAD_VP8X);
		}
		else if (ctype == EXIF || ctype == XMP || ctype == ICCP)
		{
			ustring2 note(L"Chunk %S (%d bytes)", chunkId, chunk.ChunkSize);
			hr = _site->TagData(SCANSITETAGDATA_FLAG_ADD_TO_SUMMARYLIST, offset.QuadPart, FIELD_OFFSET(CHUNK_RIFF, Payload), bstring(note), 0, &tagIndex);
			if (hr == S_OK)
			{
				SCAN_METADATA_TYPE mtype;
				if (ctype == XMP)
					mtype = SMDTYPE_XMP;
				else if (ctype == ICCP)
					mtype = SMDTYPE_ICCP;
				else
					mtype = SMDTYPE_EXIF;
				hr = _data->ParseMetaData(mtype, offset.QuadPart + FIELD_OFFSET(CHUNK_RIFF, Payload), chunk.ChunkSize, tagIndex, NULL);
			}
			//seekLen = (LONG)chunk.ChunkSize;
		}
		else
		{
			ustring2 note(L"Chunk %S", chunkId);
			hr = _site->TagData(SCANSITETAGDATA_FLAG_ADD_TO_SUMMARYLIST, offset.QuadPart, FIELD_OFFSET(CHUNK_RIFF, Payload), bstring(note), 0, &tagIndex);
			if (hr == S_OK)
			{
				note.format(L"%S payload data (%d bytes)", chunkId, chunk.ChunkSize);
				hr = _site->TagData(SCANSITETAGDATA_FLAG_TRANSPARENTBACKGROUND | SCANSITETAGDATA_FLAG_GRAYTEXT, offset.QuadPart + FIELD_OFFSET(CHUNK_RIFF, Payload), chunk.ChunkSize, bstring(note), tagIndex, NULL);
			}
			seekLen = (LONG)chunk.ChunkSize;
		}

		if (p)
			free(p);

		if (seekLen)
		{
			hr = _data->Seek(seekLen, SSEEK_ORIGIN_CUR);
			if (FAILED(hr))
			{
				_msg = L"Data seek fault";
				break;
			}
		}
#endif//#ifdef SCANLOOP_READS_ALL_OF_PAYLOAD
		offset.QuadPart += FIELD_OFFSET(CHUNK_RIFF, Payload) + chunk.ChunkSize;
	}
	return hr;
}

ScanServerImpl::CHUNK_TYPE ScanServerImpl::getChunkType(CHUNK_RIFF &chunk)
{
	if (memcmp(chunk.ChunkId, "WEBP", 4) == 0)
		return WEBP;
	if (memcmp(chunk.ChunkId, "VP8 ", 4) == 0)
		return VP8;
	if (memcmp(chunk.ChunkId, "VP8L", 4) == 0)
		return VP8L;
	if (memcmp(chunk.ChunkId, "VP8X", 4) == 0)
		return VP8X;
	if (memcmp(chunk.ChunkId, "ANIM", 4) == 0)
		return ANIM;
	if (memcmp(chunk.ChunkId, "ANMF", 4) == 0)
		return ANMF;
	if (memcmp(chunk.ChunkId, "ALPH", 4) == 0)
		return ALPH;
	if (memcmp(chunk.ChunkId, "ICCP", 4) == 0)
		return ICCP;
	if (memcmp(chunk.ChunkId, "EXIF", 4) == 0)
		return EXIF;
	if (memcmp(chunk.ChunkId, "XMP ", 4) == 0)
		return XMP;
	return UNK;
}

STDMETHODIMP ScanServerImpl::GetErrorMessage(BSTR *Message)
{
	if (!_msg._b)
		return E_NOTIMPL;
	*Message = SysAllocString(_msg);
	return S_OK;
}
