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
#include "ROFile.h"


/* RWFile - provides read/write file access
*/
class RWFile
{
public:
	RWFile(const ROFile *rof) : _rof(rof) {}
	~RWFile() {}

	HRESULT insertContent(LPVOID insertData, ULONG insertLen, LARGE_INTEGER offset);
	HRESULT removeContent(ULONG removeLen, LARGE_INTEGER offset);

protected:
	const ROFile *_rof;
};


/* BOMFile - detects and manages Byte-Order-Mark
*/
class BOMFile : public RWFile
{
public:
	BOMFile(const ROFile *rof) : RWFile(rof), _textByteOrderMark(TEXT_UNKNOWN), _textBomCanBeAdded(false), _textBomCanBeRemoved(false) {}

	TEXTBYTEORDERMARK _textByteOrderMark;
	bool _textBomCanBeAdded, _textBomCanBeRemoved;

	UINT AnalyzeFileType(BYTEORDERMARK_DESC BomDesc[], int BomCount);
	HRESULT ClearBOM(BYTEORDERMARK_DESC *BomDesc);
	HRESULT SetBOM(BYTEORDERMARK_DESC *BomDesc);
};
