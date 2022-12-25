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
#include "ROFile.h"


////////////////////////////////////////////////////////////
// ROFileBase methods

HRESULT ROFileBase::read(FILEREADINFO *fri) const
{
	ULONG readSize = fri->DataLength;
	return readBlock(fri, readSize);
}

HRESULT ROFileBase::search(FILEREADINFO *fi, LPBYTE searchWord, ULONG searchLen, LARGE_INTEGER *foundOffset) const
{
	HRESULT hr = S_FALSE;
	BYTE nextByte;
	ULONG j;
	LARGE_INTEGER processedBytes = fi->DataOffset;
	HANDLE hf;
	bool matchFound = false;

	if (fi->DataUsed)
		processedBytes.QuadPart += fi->DataUsed;

	ASSERT(fi->DataBuffer);

	hf = openFile(fi);
	if (hf == INVALID_HANDLE_VALUE)
		return E_ACCESSDENIED;

	if (fi->Backward)
	{
		j = 0;
		while (S_OK == (hr = readByte(hf, fi, &processedBytes, &nextByte)))
		{
			if (searchWord[searchLen - j - 1] == nextByte)
			{
				if (++j == searchLen)
				{
					foundOffset->QuadPart = processedBytes.QuadPart;
					matchFound = true;
					break;
				}
			}
			else if (j != 0)
			{
				processedBytes.QuadPart += j;
				j = 0;
			}
		}
	}
	else
	{
		j = 0;
		while (S_OK == (hr = readByte(hf, fi, &processedBytes, &nextByte)))
		{
			if (searchWord[j] == nextByte)
			{
				if (++j == searchLen)
				{
					foundOffset->QuadPart = processedBytes.QuadPart - j;
					matchFound = true;
					break;
				}
			}
			else if (j != 0)
			{
				processedBytes.QuadPart -= j;
				j = 0;
			}
		}
	}

	if (FAILED(hr))
		fi->ErrorCode = GetLastError();

	if (hf)
		CloseHandle(hf);

	if (FAILED(hr))
		return hr;
	return matchFound ? S_OK : S_FALSE;
}

HRESULT ROFileBase::readBlock(FILEREADINFO *fi, ULONG blockLen) const
{
	if (!fi->DataBuffer)
	{
		fi->ErrorCode = ERROR_NO_DATA;
		return E_POINTER;
	}

	ZeroMemory(fi->DataBuffer, fi->BufferSize);
	fi->DataUsed = 0;
	fi->DataLength = 0;
	fi->HasData = false;

	HANDLE hf = openFile(fi);
	if (hf == INVALID_HANDLE_VALUE)
		return E_ACCESSDENIED;

	HRESULT hr = readFile(hf, fi, &fi->DataOffset, blockLen);

	if (hf)
		CloseHandle(hf);
	return hr;
}

/* readByte - sequentially reads a byte from a file at a specified offset. the method maintains
  a block of data of 0x1000 bytes in cache for performance.

hf - handle to a file from which content data is read.
FI - a data cache decriptor. it locates the cached data within the content stream of the
  file for synchronization purposes.
  FI->Backward is set to true if reading a byte right behind the byte read previously.
  or, it's set to false if reading forward, or reading a byte ahead of the previous byte.
ProcessedBytes - a number of bytes so far read sequentially from the file.
  in effect, it's a content pointer pointing at a new byte to read from the file in forward mode.
  it's a content pointer for an old byte previously read in backward mode. in this mode, the byte to be read this time around is one byte behind the old byte.
OutputByte - address of a buffer to receive the data byte read by the method.
*/
HRESULT ROFileBase::readByte(HANDLE hf, FILEREADINFO* fi, LARGE_INTEGER* processedBytes, LPBYTE outputByte) const
{
	BOOL res = FALSE;
	LONG j;

	if (fi->Backward)
	{
		// going in reverse
		// check cached data first. if our byte is not there, we will go to the file.
		if (fi->HasData)
		{
			// calculate the index to the next byte in the cached data block.
			j = (LONG)(processedBytes->QuadPart - fi->DataOffset.QuadPart);
			j--;
			// our byte is in cache if the index is in range.
			if (0 <= j && j < (LONG)fi->DataLength)
			{
				// the byte is in cache. advance the content pointer for next read.
				processedBytes->QuadPart--;
				// pick up the byte.
				*outputByte = fi->DataBuffer[j];
				return S_OK;
			}
		}

		// processedBytes is a content pointer and locates a byte we have read in a prior read operation. pb will point to a part of the file that starts a block we will read next. so, in backward mode, processedBytes points to the end of the next block.
		LARGE_INTEGER pb = *processedBytes;
		pb.QuadPart -= fi->BufferSize;
		LARGE_INTEGER offset = { fi->BufferSize, 0 };
		// check overshooting the beginning of the file
		if (pb.QuadPart < 0LL)
		{
			// move it back to current content pointer
			offset.QuadPart = processedBytes->QuadPart;
			// a block of content we want to read starts at top of file.
			pb.QuadPart = 0LL;
		}
		// the byte we want is at current offset minus 1.
		offset.QuadPart--;
		// move the file pointer to target content and read the block of content data.
		if (S_OK == readFile(hf, fi, &pb, fi->BufferSize))
		{
			// update the content data offset.
			fi->DataOffset.QuadPart = pb.QuadPart;
			// do we have the block we've asked for?
			if (fi->DataLength < offset.LowPart)
				return S_FALSE; // no, reached beginning of file
			// yes, get the byte from the end of the read block.
			*outputByte = fi->DataBuffer[offset.LowPart];
			// advance the content pointer.
			processedBytes->QuadPart--;
			// indicate we have data in cache.
			fi->HasData = true;
			return S_OK;
		}
		// else the read operation failed.
	}
	else
	{
		// moving forward
		if (fi->HasData)
		{
			// DataOffset maps a place in the file content to the cached data in DataBuffer. processedBytes points to the next byte we want to read. so, if our byte is in cache, the condition processedBytes < DataOffset is true. calcuate the offset to the next byte within the cached data block.
			j = (LONG)(processedBytes->QuadPart - fi->DataOffset.QuadPart);
			// is it in the correct range?
			if (0 <= j && j < (LONG)fi->DataLength)
			{
				// yes, the byte is in cache. advance the content pointer for the next read.
				processedBytes->QuadPart++;
				// pick up the byte from cache.
				*outputByte = fi->DataBuffer[j];
				return S_OK;
			}
		}

		// move to the beginning of the next block in the file.
		// read the block from the file. it's 0x1000 bytes long.
		if (S_OK == readFile(hf, fi, processedBytes, fi->BufferSize))
		{
			// save the content pointer. that's where the just read data block starts in the file.
			fi->DataOffset.QuadPart = processedBytes->QuadPart;
			// did we read anything at all?
			if (fi->DataLength == 0)
				return S_FALSE; // no, we've reached te end of file.
			// our byte sits at beginning of data block in cache.
			*outputByte = fi->DataBuffer[0];
			// advance the content pointer for next read.
			processedBytes->QuadPart++;
			// we have data bytes.
			fi->HasData = true;
			return S_OK;
		}
	}
	// the read operation failed.
	fi->HasData = false;
	return HRESULT_FROM_WIN32(fi->ErrorCode);
}


////////////////////////////////////////////////////////////
// ROFile methods

ULONG ROFile::getFileId() const
{
	ustring fname(_filename);
	_wcsupr(fname._buffer);
	ULONG csum = 0;
	for (ULONG i = 0; i < fname._length; i++)
	{
		csum += _filename[i] + i+1;
	}
	return csum;
}

LPCWSTR ROFile::getFileExtension() const
{
	LPCWSTR sep = wcsrchr(_filename, '\\');
	LPCWSTR ext = wcsrchr(_filename, '.');
	if (ext && ext > sep)
		return ext;
	return NULL;
}

DWORD ROFile::queryFileSize(ULARGE_INTEGER *fileSize) const
{
	HANDLE hf = CreateFile(_filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hf == INVALID_HANDLE_VALUE)
	{
		return GetLastError();
	}
	fileSize->LowPart = GetFileSize(hf, &fileSize->HighPart);
	CloseHandle(hf);
	return ERROR_SUCCESS;
}

HANDLE ROFile::openFile(FILEREADINFO *fi) const
{
	HANDLE hf = CreateFile(_filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hf == INVALID_HANDLE_VALUE)
		fi->ErrorCode = GetLastError();
	else if (fi->FileSize.QuadPart == 0)
		fi->FileSize.LowPart = GetFileSize(hf, &fi->FileSize.HighPart);
	return hf;
}

HRESULT ROFile::readFile(HANDLE hf, FILEREADINFO *fi, LARGE_INTEGER *seekOffset, ULONG readSize) const
{
	// move the file pointer to target content.
	if (INVALID_SET_FILE_POINTER ==
		SetFilePointer(hf, seekOffset->LowPart, &seekOffset->HighPart, FILE_BEGIN))
	{
		// failed. pick up the error code.
		fi->ErrorCode = GetLastError();
		return E_UNEXPECTED;
	}
	// let's read the block of content data.
	if (readSize == 0 ||
		ReadFile(hf, fi->DataBuffer, readSize, &fi->DataLength, NULL))
	{
		fi->ErrorCode = 0;
		fi->HasData = true;
		return S_OK;
	}
	fi->ErrorCode = GetLastError();
	return E_FAIL;
}

HRESULT ROFile::readByteAt(LONGLONG offset, LPBYTE outBuf)
{
	BYTE buf[2] = { 0 };
	FILEREADINFO fi = { 0 };
	fi.DataBuffer = buf;
	fi.BufferSize = sizeof(buf);
	fi.DataOffset.QuadPart = offset;
	HRESULT hr = readBlock(&fi, 1);
	if (hr == S_OK)
		*outBuf = buf[0];
	return hr;
}

////////////////////////////////////////////////////////////
// ROFileAccessor methods

LPBYTE ROFileAccessor::freadp(ULONG dataLen, FILEREADINFO2 *fi)
{
	LPBYTE p;
	ULONG dataLeft = fi->RemainingLength;
	if (dataLeft >= dataLen)
	{
		p = fi->DataBuffer + fi->DataUsed;
		fi->DataStart = p;
		fi->StartLength = 0;
		fi->StartOffset.QuadPart = fi->DataOffset.QuadPart + fi->DataUsed;
		fi->DataUsed += dataLen;
		fi->ServedLength = dataLen;
		fi->RemainingLength = fi->DataLength - fi->DataUsed;
		return p;
	}
	// is the request too large to fit in our buffer?
	if (dataLen - dataLeft > fi->BufferSize)
	{
		fi->ErrorCode = ERROR_INSUFFICIENT_BUFFER;
		return NULL;
	}
	ZeroMemory(fi->DoubleBuffer, fi->BufferSize);
	p = fi->DoubleBuffer + fi->BufferSize - dataLeft;
	CopyMemory(p, fi->DataBuffer + fi->DataUsed, dataLeft);
	fi->DataStart = p;
	fi->StartLength = dataLeft;
	fi->StartOffset.QuadPart = fi->DataOffset.QuadPart + fi->DataUsed;

	fi->DataOffset.QuadPart += fi->DataLength;
	fi->DataLength = fi->BufferSize;
	if (FAILED(_rof->read(fi)))
		return NULL;
	if (fi->DataLength == 0)
		return NULL; // at EOF. note _rof->read() succeeds but the buffer is empty.
	if (dataLen <= fi->DataLength + dataLeft)
	{
		fi->DataUsed = dataLen - dataLeft;
		fi->ServedLength = dataLen;
	}
	else
	{
		fi->DataUsed = fi->DataLength - dataLeft;
		fi->ServedLength = fi->DataLength;
	}
	fi->RemainingLength = fi->DataLength - fi->DataUsed;
	return p;
}

ULONG ROFileAccessor::fread(LPVOID dataPtr, ULONG blockLen, ULONG blockCount, FILEREADINFO2 *fi)
{
	LPBYTE p;
	ULONG dataLen = blockLen * blockCount;
	if (dataLen <= fi->BufferSize)
	{
		p = freadp(dataLen, fi);
		if (!p)
			return 0;
		fi->PriorStart = fi->StartOffset;
		fi->PriorLength = fi->ServedLength;
		if (fi->ServedLength == 0)
			return 0;
		CopyMemory(dataPtr, p, fi->ServedLength);
		fi->StartOffset.QuadPart += fi->ServedLength;
		blockCount = fi->ServedLength / blockLen;
		fi->ServedLength = 0;
		return blockCount;
	}

	ULONG remainingLen = dataLen;
	ULONG servedLen = 0;
	LARGE_INTEGER startOffset = fi->StartOffset;

	while (remainingLen)
	{
		if (dataLen > fi->BufferSize)
			dataLen = fi->BufferSize;
		if (dataLen > remainingLen)
			dataLen = remainingLen;
		p = freadp(dataLen, fi);
		if (!p)
			return 0;
		remainingLen -= dataLen;

		fi->PriorStart = fi->StartOffset;
		fi->PriorLength = fi->ServedLength;
		servedLen += fi->ServedLength;
		if (fi->ServedLength == 0)
			return 0;
		CopyMemory(dataPtr, p, fi->ServedLength);
		dataPtr = (LPBYTE)dataPtr + fi->ServedLength;
		fi->StartOffset.QuadPart += fi->ServedLength;
		fi->ServedLength = 0;
	}

	blockCount = servedLen / blockLen;
	return blockCount;
}

int ROFileAccessor::fseek(FILEREADINFO2 *fi, LONG moveLen, ROFSEEK_ORIGIN origin)
{
	if (fi->FileSize.QuadPart == 0)
		_rof->queryFileSize(&fi->FileSize);

	LONGLONG src, dest;
	LONGLONG deltaMax, delta = moveLen;

	if (origin == ROFSEEK_ORIGIN_BEGIN)
	{
		if (delta < 0)
		{
			fi->ErrorCode = ERROR_RANGE_NOT_FOUND;
			return ERROR_RANGE_NOT_FOUND;
		}
		src = 0;
		deltaMax = fi->FileSize.QuadPart;
	}
	else if (origin == ROFSEEK_ORIGIN_END)
	{
		if (delta > 0)
		{
			fi->ErrorCode = ERROR_RANGE_NOT_FOUND;
			return ERROR_RANGE_NOT_FOUND;
		}
		src = fi->FileSize.QuadPart;
		deltaMax = -src;
	}
	else if (origin == ROFSEEK_ORIGIN_CUR)
	{
		src = fi->StartOffset.QuadPart + fi->ServedLength;
		if (delta > 0)
			deltaMax = fi->FileSize.QuadPart - src;
		else if (delta < 0)
			deltaMax = -src;
	}
	else
	{
		fi->ErrorCode = ERROR_INVALID_PARAMETER;
		return ERROR_INVALID_PARAMETER;
	}

	if (delta > 0)
	{
		if (delta < deltaMax)
			dest = src + delta;
		else
			dest = src + deltaMax;
	}
	else if (delta < 0)
	{
		if (delta > deltaMax)
			dest = src + delta;
		else
			dest = src + deltaMax;
	}
	else // delta==0
		dest = src;

	ULONG n;
	LPBYTE p1, p2;
	LONGLONG r1, r2;
	r1 = fi->DataOffset.QuadPart;
	if (fi->StartLength)
	{
		r1 = fi->StartOffset.QuadPart;
		r2 = r1 + fi->StartLength;
	}
	else
		r2 = r1 + fi->DataLength;
	if (r1 <= dest && dest <= r2)
	{
		// destination offset belongs to current data range.
		delta = dest - fi->DataOffset.QuadPart;
		p1 = fi->DoubleBuffer + fi->BufferSize;
		p2 = p1 + delta;
		if (delta < 0)
		{
			// destination offset is in first-half buffer. move the data forward.
			n = fi->BufferSize;
			memmove(p2, p1, n);
		}
		else if (delta > 0)
		{
			// destination offset is in second-half buffer. move the data backward.
			n = fi->BufferSize - (ULONG)(LONG)delta;
			memmove(p1, p2, n);
		}
		fi->StartOffset.QuadPart = fi->DataOffset.QuadPart = dest;
		fi->DataLength = (ULONG)(r2 - dest);
	}
	else
	{
		// destination offset is outside current data range. clear the double buffer.
		ZeroMemory(fi->DoubleBuffer, 2 * fi->BufferSize);
		fi->StartOffset.QuadPart = fi->DataOffset.QuadPart = dest;
		fi->DataLength = 0;
	}
	fi->PriorStart = fi->StartOffset;
	fi->PriorLength = fi->ServedLength;
	fi->DataStart = fi->DataBuffer;
	fi->RemainingLength = fi->DataLength;
	fi->StartLength = 0;
	fi->DataUsed = 0;
	fi->ServedLength = 0;

	return ERROR_SUCCESS;
}

bool ROFileAccessor::fsearch(FILEREADINFO2 *fi, LPBYTE searchWord, ULONG searchLen)
{
	FILEREADINFO fi2;
	CopyMemory(&fi2, fi, sizeof(FILEREADINFO));
	fi2.HasData = false;
	// search starts at an offset given by fi->DataOffset+fi->DataUsed.
	HRESULT hr = _rof->search(&fi2, searchWord, searchLen, &fi->StartOffset);
	if (hr == S_OK)
	{
		fi->DataOffset = fi2.DataOffset;
		fi->DataUsed = (ULONG)(LONG)(fi->StartOffset.QuadPart - fi2.DataOffset.QuadPart);
		fi->DataLength = fi2.DataLength;
		fi->ServedLength = 0; // perhaps tempted to set it to searchLen, but no, since we are not serving the found data piece.
		fi->RemainingLength = fi2.DataLength - fi->DataUsed;
		return true;
	}
	// no match was found, or an error occurred. invalidate any data in the buffer. it's likely an ending part of the file data which may not correspond to the pre-search offset.
	fi->DataLength = fi->DataUsed = fi->ServedLength = fi->RemainingLength = 0;
	return false;
}

LARGE_INTEGER ROFileAccessor::fpos(FILEREADINFO2 *fi)
{
	LARGE_INTEGER pos = fi->DataOffset;
	pos.QuadPart += fi->DataUsed;
	return pos;
}

HRESULT ROFileAccessor::fcopyTo(LPSTREAM ps, FILEREADINFO2 *fi)
{
	HRESULT hr = S_OK;
	LPBYTE p;
	ULONG len = 0;
	bool commit = false;
	while ((p = freadp(fi->BufferSize, fi)))
	{
		ULONG cbWritten;
		hr = ps->Write(p, fi->ServedLength, &cbWritten);
		if (hr != S_OK)
			break;
		len += cbWritten;
		if (fi->ServedLength < fi->BufferSize)
		{
			commit = true;
			break;
		}
	}
	if (hr == S_OK && !commit)
	{
		if (len == fi->FileSize.LowPart)
			commit = true;
	}
	if (commit)
	{
		hr = ps->Commit(0);
		if (hr == S_OK)
			hr = ps->Seek({ 0 }, STREAM_SEEK_SET, NULL);
	}
	return hr;
}
