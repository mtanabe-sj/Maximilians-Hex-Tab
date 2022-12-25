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
#include "persistMetafile.h"
#include "libver.h"


//#define PMF_USES_TEMP_FOLDER

persistMetafile::persistMetafile(bool temp, LPCWSTR userFile, ULONG metafileId, LPCWSTR dataFile) : _hf(NULL), _temp(temp), _tempId(metafileId), _header{ 0 }
{
	_userFile = userFile;
	if (dataFile)
		_dataFile = dataFile;
}

persistMetafile::~persistMetafile()
{
	if (_hf)
		CloseHandle(_hf);
}

HRESULT persistMetafile::load(simpleList<DUMPREGIONINFO> &outRegions, AnnotationCollection &outAnnotations, simpleList<DUMPSHAPEINFO> &outDrawings, WIN32_FILE_ATTRIBUTE_DATA *fad, ULONG *flags)
{
	DBGPUTS((L"persistMetafile::load"));
	HRESULT hr;

	if (0 != Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY L"\\MetaFiles", _userFile, &_tempId))
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

	if (_dataFile._length == 0)
		setDataFile();

	_hf = CreateFile(_dataFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, _tempId ? FILE_ATTRIBUTE_TEMPORARY : FILE_ATTRIBUTE_NORMAL, NULL);
	if (_hf == INVALID_HANDLE_VALUE)
	{
		hr = HRESULT_FROM_WIN32(::GetLastError()); // no such file. perhaps, temp dir has been cleaned.
		Registry_DeleteValue(HKEY_CURRENT_USER, LIBREGKEY L"\\MetaFiles", _userFile);
		return hr;
	}

#ifdef _DEBUG
	USHORT ver;
	hr = readDataBlock(&ver, sizeof(ver));
	if (hr==S_OK && ver == PMF_VERSION_NUM)
	{
		_header.version = ver;
		_header.sig[0] = 'p';
		_header.sig[1] = 'm';

		WIN32_FILE_ATTRIBUTE_DATA inFad;
		hr = readDataBlock(&inFad, sizeof(inFad));
	}
	else
	{
		SetFilePointer(_hf, 0, NULL, FILE_BEGIN);
		hr = readDataBlock(&_header, sizeof(_header));
		if (hr == S_OK && (_header.sig[0] != 'p' || _header.sig[1] != 'm'))
			hr = HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
	}
#else//#ifdef _DEBUG
	hr = readDataBlock(&_header, sizeof(_header));
	if (hr == S_OK && (_header.sig[0] != 'p' || _header.sig[1] != 'm'))
		hr = HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
#endif//#ifdef _DEBUG
	if (hr == S_OK)
	{
		if (_header.version == PMF_VERSION_NUM)
		{
			ULONG blockSize;
			hr = readDataBlockSize(blockSize);
			if (hr == S_OK)
			{
				DUMPREGIONINFO region;
				size_t i, n = blockSize / sizeof(DUMPREGIONINFO);
				for (i = 0; i < n; i++)
				{
					hr = readSubdataBlock(&region, sizeof(DUMPREGIONINFO));
					if (hr != S_OK)
						break;
					outRegions.add(region);
				}
				if (hr == S_OK)
				{
					hr = readDataBlockSize(blockSize);
					if (hr == S_OK)
					{
						DUMPANNOTATIONINFO annotation;
						int i, n = blockSize / sizeof(DUMPANNOTATIONINFO);
						outAnnotations.beforeLoad(_temp, n);
						for (i = 0; i < n; i++)
						{
							hr = readSubdataBlock(&annotation, sizeof(DUMPANNOTATIONINFO));
							if (hr != S_OK)
								break;
							outAnnotations.load(annotation);
						}
						outAnnotations.afterLoad(hr);
						if (hr == S_OK)
						{
							hr = readDataBlockSize(blockSize);
							if (hr == S_OK)
							{
								DUMPSHAPEINFO drawing;
								n = blockSize / sizeof(DUMPSHAPEINFO);
								for (i = 0; i < n; i++)
								{
									hr = readSubdataBlock(&drawing, sizeof(DUMPSHAPEINFO));
									if (hr != S_OK)
										break;
									outDrawings.add(drawing);
								}
							}
						}
					}
				}
			}
			if (hr == S_OK)
			{
				readUserFileAttributes(fad);
				*flags = _header.flags;
#ifdef _DEBUG
				if (_header.regionCount == 0)
				{
					_header.regionCount = LOWORD(outRegions.size());
					_header.regionLength = LOWORD(outRegions.dataLength());
					_header.annotCount = LOWORD(outAnnotations.size());
					_header.annotLength = LOWORD(outAnnotations.dataLength());
					_header.gshapeCount = LOWORD(outDrawings.size());
					_header.gshapeLength = LOWORD(outDrawings.dataLength());
					_header.reserved = 0;
				}
#endif//#ifdef _DEBUG
				if (outRegions.size() != _header.regionCount ||
					outAnnotations.size() != _header.annotCount ||
					outDrawings.size() != _header.gshapeCount)
				{
					hr = E_BOUNDS;
				}
			}
		}
		else
			hr = E_UNEXPECTED;
	}

	// note that _hf will be closed by the destructor.
	return hr;
}

HRESULT persistMetafile::save(simpleList<DUMPREGIONINFO> &inRegions, AnnotationCollection &inAnnotations, simpleList<DUMPSHAPEINFO> &inDrawings, WIN32_FILE_ATTRIBUTE_DATA *fad, ULONG flags)
{
	DBGPUTS((L"persistMetafile::save"));
	if (_dataFile._length == 0)
		setDataFile();

	HRESULT hr;
	_hf = CreateFile(_dataFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, _tempId ? FILE_ATTRIBUTE_TEMPORARY : FILE_ATTRIBUTE_NORMAL, NULL);
	if (_hf != INVALID_HANDLE_VALUE)
	{
		// write the header
		_header.sig[0] = 'p';
		_header.sig[1] = 'm';
		_header.version = PMF_VERSION_NUM;
		_header.regionCount = LOWORD(inRegions.size());
		_header.regionLength = LOWORD(inRegions.dataLength());
		_header.annotCount = LOWORD(inAnnotations.size());
		_header.annotLength = LOWORD(inAnnotations.dataLength());
		_header.gshapeCount = LOWORD(inDrawings.size());
		_header.gshapeLength = LOWORD(inDrawings.dataLength());
		_header.reserved = 0;
		_header.flags = flags;
		hr = writeDataBlock(&_header, sizeof(_header));
		if (hr == S_OK)
		{
			// write our metadata for the user file
			// 1) regions
			hr = writeDataBlock(inRegions.data(), (ULONG)inRegions.dataLength());
			// 2) annotations
			if (hr == S_OK)
			{
				simpleList<HBITMAP> hbmps;
				inAnnotations.beforeSave(hbmps, flags & PMFLAG_APP_TERMINATING);
				hr = writeDataBlock(inAnnotations.data(), (ULONG)inAnnotations.dataLength());
				inAnnotations.afterSave(hbmps, _temp);
			}
			// 3) drawings
			if (hr == S_OK)
				hr = writeDataBlock(inDrawings.data(), (ULONG)inDrawings.dataLength());
			if (hr == S_OK)
			{
				// write a termination block (a zero DWORD).
				hr = writeDataBlock(NULL, 0);

				Registry_SetDwordValue(HKEY_CURRENT_USER, LIBREGKEY L"\\MetaFiles", _userFile, _tempId);

				readUserFileAttributes(fad);
			}
		}
	}
	else
		hr = HRESULT_FROM_WIN32(::GetLastError());

	return hr;
}

/*
bool persistMetafile::equalFad(WIN32_FILE_ATTRIBUTE_DATA *inFad)
{
	if (_fad.nFileSizeHigh != inFad->nFileSizeHigh)
		return false;
	if (_fad.nFileSizeLow != inFad->nFileSizeLow)
		return false;
	if (_fad.ftLastWriteTime.dwHighDateTime != inFad->ftLastWriteTime.dwHighDateTime)
		return false;
	if (_fad.ftLastWriteTime.dwLowDateTime != inFad->ftLastWriteTime.dwLowDateTime)
		return false;
	return true;
}
*/

HRESULT persistMetafile::readUserFileAttributes(WIN32_FILE_ATTRIBUTE_DATA *fad)
{
	if (GetFileAttributesEx(_userFile, GetFileExInfoStandard, fad))
		return S_OK;
	return HRESULT_FROM_WIN32(::GetLastError());
}

HRESULT persistMetafile::readDataBlockSize(ULONG &blockSize)
{
	ULONG readSize;
	if (!ReadFile(_hf, &blockSize, sizeof(blockSize), &readSize, NULL))
		return HRESULT_FROM_WIN32(::GetLastError());
	return S_OK;
}

HRESULT persistMetafile::readDataBlock(LPVOID outBuffer, ULONG bufferSize)
{
	ULONG blockSize;
	HRESULT hr = readDataBlockSize(blockSize);
	if (hr != S_OK)
		return hr;
	if (blockSize != bufferSize)
		return E_UNEXPECTED;
	return readSubdataBlock(outBuffer, bufferSize);
}

HRESULT persistMetafile::readSubdataBlock(LPVOID outBuffer, ULONG bufferSize)
{
	ULONG readSize;
	if (!ReadFile(_hf, outBuffer, bufferSize, &readSize, NULL))
		return HRESULT_FROM_WIN32(::GetLastError());
	return S_OK;
}

HRESULT persistMetafile::writeDataBlock(LPVOID srcData, ULONG srcLength)
{
	ULONG writtenSize;
	ULONG blockSize = srcLength;
	if (!WriteFile(_hf, &blockSize, sizeof(blockSize), &writtenSize, NULL))
		return HRESULT_FROM_WIN32(::GetLastError());
	if (!WriteFile(_hf, srcData, srcLength, &writtenSize, NULL))
		return HRESULT_FROM_WIN32(::GetLastError());
	return S_OK;
}

void persistMetafile::setDataFile()
{
	WCHAR buf[MAX_PATH] = { 0 };
	GetWorkFolder(buf, ARRAYSIZE(buf), _temp);
	_dataFile.alloc(MAX_PATH);
	_tempId = GetTempFileName(buf, L"BIT", _tempId, _dataFile._buffer);
	_dataFile._length = (ULONG)wcslen(_dataFile._buffer);
}

ustring persistMetafile::queryMetafile(ULONG fileId, bool temp)
{
	ustring dataFile;
	WCHAR buf[MAX_PATH] = { 0 };
	GetWorkFolder(buf, ARRAYSIZE(buf), temp);
	dataFile.alloc(MAX_PATH);
	ULONG fileId2 = GetTempFileName(buf, L"BIT", fileId, dataFile._buffer);
	ASSERT(fileId2 == fileId);
	dataFile._length = (ULONG)wcslen(dataFile._buffer);
	return dataFile;
}
