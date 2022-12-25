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
#include "ScanTag.h"
#include "IUnknownImpl.h"
#include "hextabscan.h"


class ScanDataImpl :
	public IUnknownImpl<IHexTabScanData, &IID_IHexTabScanData>
{
public:
	ScanDataImpl(FILEREADINFO2 *fi, ScanTag *sm);
	~ScanDataImpl() {}

	STDMETHOD(GetFileSize) (LONGLONG *SizeInBytes);
	STDMETHOD(GetFilePointer) (LONGLONG *Offset);
	STDMETHOD(Read) (LPVOID DataBuffer, ULONG *DataSizeInBytes);
	STDMETHOD(Seek) (LONG MoveDistance, SCANSEEK_ORIGIN SeekOrigin);
	STDMETHOD(Search) (LPBYTE BytesToSearch, ULONG ByteCount);

	STDMETHOD(ParseMetaData)(SCAN_METADATA_TYPE MetaType, LONGLONG MetaOffset, LONG MetaLength, SHORT ParentIndex, SHORT *TagIndex);

protected:
	FILEREADINFO2 *_fi;
	ScanTag *_sm;
};


class ScanTagExt :
	public ScanTag,
	public IUnknownImpl<IHexTabScanSite, &IID_IHexTabScanSite>
{
public:
	ScanTagExt(BinhexMetaView *bmv);
	~ScanTagExt();

	// IHexTabScanSite methods
	STDMETHOD(GetVersion)(ULONG *SiteVersion);
	STDMETHOD(GetState)(SCANSITE_STATE *SiteState);
	STDMETHOD(GetFileName)(BSTR *FileName);
	STDMETHOD(TagData)(USHORT Flags, LONGLONG TagOffset, LONG TagLength, BSTR TagNote, SHORT ParentIndex, SHORT *TagIndex);
	STDMETHOD(Annotate)(LONG TagIndex, VARIANT *SourceData);

	static bool CanScanFile(ROFile* rof);

protected:
	virtual DWORD runInternal(FILEREADINFO2 *fi);

	HRESULT createScanServer(IHexTabScanServer **server);
};

