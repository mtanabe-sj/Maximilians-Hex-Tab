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
#include "BinhexMetaView.h"


class ScanTag : public ROFileAccessor
{
public:
	ScanTag(BinhexMetaView *bmv);
	virtual ~ScanTag();

	BinhexMetaView *_bmv;
	ustring _msg;

	// worker start and stop methods
	bool start();
	void stop();

	virtual LPBYTE freadp(ULONG dataLen, FILEREADINFO2 *fi)
	{
		if (isKilled(&fi->ErrorCode))
			return NULL;
		return ROFileAccessor::freadp(dataLen, fi);
	}
	virtual int fseek(FILEREADINFO2 *fi, LONG moveLen, ROFSEEK_ORIGIN origin)
	{
		if (isKilled(&fi->ErrorCode))
			return ERROR_OPERATION_ABORTED;
		return ROFileAccessor::fseek(fi, moveLen, origin);
	}
	virtual bool fsearch(FILEREADINFO2 *fi, LPBYTE searchWord, ULONG searchLen)
	{
		if (isKilled(&fi->ErrorCode))
			return false;
		return ROFileAccessor::fsearch(fi, searchWord, searchLen);
	}

	bool isKilled(DWORD *errorOut=NULL)
	{
		if (_kill && WAIT_TIMEOUT != WaitForSingleObject(_kill, 0))
		{
			if (errorOut)
				*errorOut = ERROR_OPERATION_ABORTED;
			return true;
		}
		return false;
	}
	HRESULT parseExifMetaData(FILEREADINFO2 *fi, LONGLONG baseOffset, ULONG metaLength, int parentRegion);
	HRESULT parseXmpMetaData(FILEREADINFO2 *fi, LONGLONG baseOffset, ULONG metaLength, int parentRegion);
	HRESULT parseIccpMetaData(FILEREADINFO2 *fi, LONGLONG baseOffset, ULONG metaLength, int parentRegion);
	HRESULT parsePhotoshopMetaData(FILEREADINFO2 *fi, LONGLONG baseOffset, ULONG metaLength, int parentRegion);

protected:
	bool _terminated;
	HANDLE _thread;
	HANDLE _kill;

	static DWORD WINAPI _runWorker(LPVOID pThis)
	{
		return ((ScanTag*)pThis)->runWorker();
	}

	DWORD runWorker();
	virtual DWORD runInternal(FILEREADINFO2 *fi) { return ERROR_NOT_SUPPORTED; }

	LPCSTR _toTextEncodingName();
};


class ScanTagPng : public ScanTag
{
public:
	ScanTagPng(BinhexMetaView *bmv) : ScanTag(bmv) {}

protected:
	virtual DWORD runInternal(FILEREADINFO2 *fi);
};

class ScanTagBmp : public ScanTag
{
public:
	ScanTagBmp(BinhexMetaView *bmv) : ScanTag(bmv) {}

protected:
	virtual DWORD runInternal(FILEREADINFO2 *fi);
};

class ScanTagGif : public ScanTag
{
public:
	ScanTagGif(BinhexMetaView *bmv) : ScanTag(bmv) {}

protected:
	virtual DWORD runInternal(FILEREADINFO2 *fi);
};

class ScanTagJpg : public ScanTag
{
public:
	ScanTagJpg(BinhexMetaView *bmv) : ScanTag(bmv) {}

protected:
	virtual DWORD runInternal(FILEREADINFO2 *fi);
};

class ScanTagIco : public ScanTag
{
public:
	ScanTagIco(BinhexMetaView *bmv) : ScanTag(bmv) {}

protected:
	virtual DWORD runInternal(FILEREADINFO2 *fi);
};

class ScanTagBOM : public ScanTag
{
public:
	ScanTagBOM(BinhexMetaView *bmv, BYTEORDERMARK_DESC *boms, int bomSize, bool wantLFs) : ScanTag(bmv), _boms(boms), _bomSize(bomSize), _wantLFs(wantLFs) {}

protected:
	BYTEORDERMARK_DESC *_boms;
	int _bomSize;
	bool _wantLFs;

	virtual DWORD runInternal(FILEREADINFO2 *fi);
};