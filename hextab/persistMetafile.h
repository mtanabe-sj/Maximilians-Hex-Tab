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
#include "PropsheetExtImpl.h"
#include "AnnotationOperation.h"


const ULONG PMFLAG_APP_SCANNED = 0x00000001;
const ULONG PMFLAG_APP_TERMINATING = 0x80000000;

class persistMetafile
{
public:
	persistMetafile(bool temp, LPCWSTR userFile, ULONG metafileId=0, LPCWSTR dataFile=NULL);
	~persistMetafile();

	HRESULT load(simpleList<DUMPREGIONINFO> &outRegions, AnnotationCollection &outAnnotations, simpleList<DUMPSHAPEINFO> &outDrawings, WIN32_FILE_ATTRIBUTE_DATA *fad, ULONG *flags);
	HRESULT save(simpleList<DUMPREGIONINFO> &inRegions, AnnotationCollection &inAnnotations, simpleList<DUMPSHAPEINFO> &inDrawings, WIN32_FILE_ATTRIBUTE_DATA *fad, ULONG flags);
	static ustring queryMetafile(ULONG fileId, bool temp);
	ULONG getFileId() { return _tempId; }

protected:
#pragma pack(1)
	struct PMFHeader
	{
		BYTE sig[2]; // 'pm'
		USHORT version;
		ULONG flags;
		USHORT regionCount, regionLength;
		USHORT annotCount, annotLength;
		USHORT gshapeCount, gshapeLength;
		ULONG reserved;
	};
#pragma pack()

	PMFHeader _header;
	ustring _userFile;
	ustring _dataFile;
	HANDLE _hf; // file handle of _dataFile.
	bool _temp;
	ULONG _tempId; // temp file number returned by GetTempFileName API function; 0 if _dataFile has a valid name.

	HRESULT readUserFileAttributes(WIN32_FILE_ATTRIBUTE_DATA *fad);
	HRESULT readDataBlockSize(ULONG &blockSize);
	HRESULT readSubdataBlock(LPVOID outBuffer, ULONG bufferSize);
	HRESULT readDataBlock(LPVOID outBuffer, ULONG bufferSize);
	HRESULT writeDataBlock(LPVOID srcData, ULONG srcLength);
	void setDataFile();
	//bool equalFad(WIN32_FILE_ATTRIBUTE_DATA *inFad);
};

