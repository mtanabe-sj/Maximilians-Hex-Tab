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


// read-only file access

class ROFileBase
{
public:
	ROFileBase() {}

	HRESULT read(FILEREADINFO *fi) const;
	HRESULT search(FILEREADINFO *fi, LPBYTE searchWord, ULONG searchLen, LARGE_INTEGER *foundOffset) const;

	virtual DWORD queryFileSize(ULARGE_INTEGER *fileSize) const { return 0; }

protected:
	virtual HANDLE openFile(FILEREADINFO *fi) const { return NULL; }
	virtual HRESULT readFile(HANDLE hf, FILEREADINFO *fi, LARGE_INTEGER* seekOffset, ULONG readSize) const { return E_NOTIMPL; }

	virtual HRESULT readByte(HANDLE hf, FILEREADINFO* fi, LARGE_INTEGER* seekOffset, LPBYTE outputByte) const;
	virtual HRESULT readBlock(FILEREADINFO *fi, ULONG blockLen) const;
};

class ROFile : public ROFileBase
{
public:
	ROFile() : _filename{ 0 } {}

	ustring getFileTitle() const
	{
		ustring ftitle;
		LPCWSTR p = wcsrchr(_filename, '\\');
		if (p)
			ftitle = p + 1;
		else
			ftitle = _filename;
		LPWSTR fext = wcsrchr(ftitle._buffer, '.');
		if (fext)
		{
			*fext = 0;
			ftitle._length = (ULONG)(fext - ftitle._buffer);
		}
		return ftitle;
	}
	LPCWSTR getFilename() const { return _filename; }
	LPCWSTR getFileExtension() const;
	ULONG getFileId() const;
	virtual DWORD queryFileSize(ULARGE_INTEGER *fileSize) const;

protected:
	WCHAR _filename[_MAX_PATH];

	virtual HANDLE openFile(FILEREADINFO *fi) const;
	virtual HRESULT readFile(HANDLE hf, FILEREADINFO *fi, LARGE_INTEGER* seekOffset, ULONG readSize) const;
	HRESULT readByteAt(LONGLONG offset, LPBYTE outBuf);
};


enum ROFSEEK_ORIGIN { ROFSEEK_ORIGIN_BEGIN, ROFSEEK_ORIGIN_CUR, ROFSEEK_ORIGIN_END };

// advanced read-only file access
class ROFileAccessor
{
public:
	ROFileAccessor(ROFileBase *rof) : _rof(rof) {}

	virtual LPBYTE freadp(ULONG dataLen, FILEREADINFO2 *fi);
	virtual ULONG fread(LPVOID dataPtr, ULONG blockLen, ULONG blockCount, FILEREADINFO2 *fi);
	virtual int fseek(FILEREADINFO2 *fi, LONG moveLen, ROFSEEK_ORIGIN origin);
	virtual bool fsearch(FILEREADINFO2 *fi, LPBYTE searchWord, ULONG searchLen);
	LARGE_INTEGER fpos(FILEREADINFO2 *fi);
	HRESULT fcopyTo(LPSTREAM ps, FILEREADINFO2 *fi);

protected:
	ROFileBase *_rof;
};

