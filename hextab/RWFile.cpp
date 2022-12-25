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
#include "RWFile.h"


HRESULT RWFile::insertContent(LPVOID insertData, ULONG insertLen, LARGE_INTEGER offset)
{
	HRESULT hr;
	ULARGE_INTEGER oldSize, newSize;
	LARGE_INTEGER offset1, offset2;
	DWORD errorCode;
	LARGE_INTEGER dataLength, readLength;
	ULONG bytesWritten;
	LPBYTE dataBuffer = NULL;
	HANDLE hf;

	hf = CreateFile(_rof->getFilename(), GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hf == INVALID_HANDLE_VALUE)
	{
		errorCode = GetLastError();
		return HRESULT_FROM_WIN32(errorCode);
	}
	oldSize.LowPart = GetFileSize(hf, &oldSize.HighPart);

	offset1.QuadPart = (oldSize.QuadPart / READFILE_BUFSIZE)*READFILE_BUFSIZE;
	offset2.QuadPart = offset1.QuadPart + insertLen;
	readLength.QuadPart = oldSize.QuadPart - offset1.QuadPart;
	ASSERT(readLength.HighPart == 0);

	// stretch the file by the amount of the input data being inserted.
	if (INVALID_SET_FILE_POINTER ==
		SetFilePointer(hf, insertLen, NULL, FILE_END))
	{
		errorCode = GetLastError();
		hr = E_UNEXPECTED;
	}
	// finalize the new file size.
	else if (!SetEndOfFile(hf))
	{
		errorCode = GetLastError();
		hr = E_UNEXPECTED;
	}
	else
	{
		// move the existing content forward, i.e., toward the EOF.

		// the block closest to the old EOF is moved first, the block prior to the last is moved next, and so on, until all old content is moved forward.
		
		// offset1 is the old location of the ending block. offset2 is the new forwarded location (equal to the EOF) the block is moved to.
		offset1.QuadPart = (oldSize.QuadPart / READFILE_BUFSIZE)*READFILE_BUFSIZE;
		offset2.QuadPart = offset1.QuadPart + insertLen;

		dataBuffer = (LPBYTE)malloc(READFILE_BUFSIZE);
		readLength.QuadPart = oldSize.QuadPart - offset1.QuadPart;
		ASSERT(readLength.HighPart == 0);

		// move the file pointer to the beginning of the next block of the file data. this is the old location.
		hr = E_FAIL;
		while (INVALID_SET_FILE_POINTER !=
			SetFilePointer(hf, offset1.LowPart, &offset1.HighPart, FILE_BEGIN))
		{
			// read the next block from the file.
			dataLength.HighPart = 0;
			if (!ReadFile(hf, dataBuffer, readLength.LowPart, &dataLength.LowPart, NULL))
				break;
			// move the file pointer to the new block location.
			if (INVALID_SET_FILE_POINTER ==
				SetFilePointer(hf, offset2.LowPart, &offset2.HighPart, FILE_BEGIN))
				break;
			// write out the data block at the new location.
			if (!WriteFile(hf, dataBuffer, dataLength.LowPart, &bytesWritten, NULL))
				break;
			if (dataLength.LowPart != bytesWritten)
			{
				hr = E_UNEXPECTED;
				break;
			}
			// are we done?
			if (offset1.QuadPart == 0)
			{
				hr = S_OK;
				break;
			}
			// move to the prior block.
			offset1.QuadPart -= (LONGLONG)READFILE_BUFSIZE;
			offset2.QuadPart -= (LONGLONG)READFILE_BUFSIZE;
			readLength.LowPart = READFILE_BUFSIZE;
		}
		if (hr != S_OK)
		{
			// something went wrong.
			errorCode = GetLastError();
		}
		else
		{
			// the existing content has been successfully moved leaving enough room at the file's beginning. insert the new piece of content.
			SetFilePointer(hf, 0, NULL, FILE_BEGIN);
			if (WriteFile(hf, insertData, insertLen, &bytesWritten, NULL))
			{
				newSize.LowPart = GetFileSize(hf, &newSize.HighPart);
				hr = S_OK;
			}
			else
			{
				errorCode = GetLastError();
				hr = E_FAIL;
			}
		}
	}
	CloseHandle(hf);

	if (dataBuffer)
		free(dataBuffer);

	return hr;
}

HRESULT RWFile::removeContent(ULONG removeLen, LARGE_INTEGER offset)
{
	HRESULT hr;
	ULARGE_INTEGER oldSize, newSize;
	LARGE_INTEGER offset1, offset2;
	DWORD errorCode;
	LARGE_INTEGER dataLength;
	ULONG bytesWritten;
	LPBYTE dataBuffer;
	HANDLE hf;

	hf = CreateFile(_rof->getFilename(), GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hf == INVALID_HANDLE_VALUE)
	{
		errorCode = GetLastError();
		return HRESULT_FROM_WIN32(errorCode);
	}
	oldSize.LowPart = GetFileSize(hf, &oldSize.HighPart);
	offset1.QuadPart = offset.QuadPart+removeLen;
	offset2.QuadPart = offset.QuadPart;

	dataBuffer = (LPBYTE)malloc(READFILE_BUFSIZE);

	// move the file content backward by the amount equal to the number of bytes of the piece of data being removed.
	// the block starting at the removal location is moved first, the block that follows it is moved next, and so on until all content is moved backward, toward the beginning of the file.
	hr = E_FAIL;
	while (INVALID_SET_FILE_POINTER !=
		SetFilePointer(hf, offset1.LowPart, &offset1.HighPart, FILE_BEGIN))
	{
		// read the next block.
		dataLength.HighPart = 0;
		if (!ReadFile(hf, dataBuffer, READFILE_BUFSIZE, &dataLength.LowPart, NULL))
			break;
		// move the file pointer to the new location. the just read block is written there.
		if (INVALID_SET_FILE_POINTER ==
			SetFilePointer(hf, offset2.LowPart, &offset2.HighPart, FILE_BEGIN))
			break;
		// move the block to the new location.
		if (!WriteFile(hf, dataBuffer, dataLength.LowPart, &bytesWritten, NULL))
			break;
		if (dataLength.LowPart != bytesWritten)
		{
			hr = E_UNEXPECTED;
			break;
		}
		// point to the beginning of the next block.
		offset1.QuadPart += dataLength.QuadPart;
		offset2.QuadPart += dataLength.QuadPart;
		if (offset1.QuadPart == oldSize.QuadPart)
		{
			hr = S_OK;
			break;
		}
	}
	if (hr != S_OK)
	{
		errorCode = GetLastError();
	}
	else
	{
		// all content data has been successfully moved. finalize the new file size.
		SetFilePointer(hf, offset2.LowPart, &offset2.HighPart, FILE_BEGIN);
		if (!SetEndOfFile(hf))
		{
			errorCode = GetLastError();
			hr = E_UNEXPECTED;
		}
		else
		{
			newSize.LowPart = GetFileSize(hf, &newSize.HighPart);
			hr = S_OK;
		}
	}
	CloseHandle(hf);

	free(dataBuffer);

	return hr;
}

///////////////////////////////////////////////////////////
// BOMFile


UINT BOMFile::AnalyzeFileType(BYTEORDERMARK_DESC BomDesc[], int BomCount)
{
	int i;
	UINT resourceId = 0;
	FILEREADINFO fri = { 0 };
	LPCWSTR extensionName = _rof->getFileExtension();

	_textByteOrderMark = TEXT_UNKNOWN;
	fri.DataBuffer = (LPBYTE)malloc(READFILE_BUFSIZE);
	fri.BufferSize = READFILE_BUFSIZE;
	fri.DataLength = 16;

	if (S_OK == _rof->read(&fri))
	{
		_textByteOrderMark = TEXT_GENERIC;
		for (i = 0; i < BomCount; i++)
		{
			if (fri.DataLength >= BomDesc[i].DataLength)
			{
				if (0 == memcmp(fri.DataBuffer, BomDesc[i].Data, BomDesc[i].DataLength))
				{
					_textByteOrderMark = BomDesc[i].BOM;
					_textBomCanBeAdded = BomDesc[i].CanBeAdded;
					_textBomCanBeRemoved = BomDesc[i].CanBeRemoved;
					resourceId = BomDesc[i].AssociatedResourceId;
					break;
				}
			}
		}
	}

	free(fri.DataBuffer);
	return resourceId;
}

HRESULT BOMFile::ClearBOM(BYTEORDERMARK_DESC *BomDesc)
{
	if (!_textBomCanBeRemoved)
		return E_ACCESSDENIED;

	return removeContent(MAKELONG(BomDesc->DataLength,0), { 0 });
}

HRESULT BOMFile::SetBOM(BYTEORDERMARK_DESC *BomDesc)
{
	if (!_textBomCanBeAdded)
		return E_ACCESSDENIED;

	return insertContent(BomDesc->Data, MAKELONG(BomDesc->DataLength,0), { 0 });
}

