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
#include <initguid.h>
#include "ScanTagExt.h"
#include "RegistryHelper.h"
#include "libver.h"


// color tables defined in CRegionOperation.cpp
extern COLORREF s_regionInteriorColor[MAX_DUMPREGIONCOLORS];
extern COLORREF s_regionBorderColor[MAX_DUMPREGIONCOLORS];


ScanTagExt::ScanTagExt(BinhexMetaView *bmv) : ScanTag(bmv)
{
}

ScanTagExt::~ScanTagExt()
{
}

bool ScanTagExt::CanScanFile(ROFile* rof)
{
	LPCWSTR fext = rof->getFileExtension();
	ustring srvclsid;
	LONG res = Registry_GetStringValue(HKEY_LOCAL_MACHINE, SCANSERVER_REGKEY, fext, srvclsid);
	if (res != ERROR_SUCCESS)
		return false;
	return true;
}

HRESULT ScanTagExt::createScanServer(IHexTabScanServer **server)
{
	LPCWSTR fext = _bmv->getFileExtension();
	ustring srvclsid;
	LONG res = Registry_GetStringValue(HKEY_LOCAL_MACHINE, SCANSERVER_REGKEY, fext, srvclsid);
	if (res != ERROR_SUCCESS)
		return HRESULT_FROM_WIN32(res);
	CLSID clsid;
	HRESULT hr = CLSIDFromString(srvclsid, &clsid);
	if (hr == S_OK)
		hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, IID_IHexTabScanServer, (LPVOID*)server);
	return hr;
}

DWORD ScanTagExt::runInternal(FILEREADINFO2 *fi)
{
	CoInitialize(NULL);
	IHexTabScanServer *srv;
	HRESULT hr = createScanServer(&srv);
	if (hr == S_OK)
	{
		ScanDataImpl *sdata = new ScanDataImpl(fi, this);
		hr = srv->Init(this, sdata);
		if (hr == S_OK)
		{
			hr = srv->Scan();
			if (FAILED(hr))
			{
				BSTR msg = NULL;
				if (S_OK == srv->GetErrorMessage(&msg))
				{
					_msg = msg;
					SysFreeString(msg);
				}
				if (fi->ErrorCode != ERROR_SUCCESS)
					hr = HRESULT_FROM_WIN32(fi->ErrorCode);
				else
					fi->ErrorCode = hr;
			}
			else if (hr == S_FALSE)
				hr = S_OK; // because the caller (ScanTag) would mistake S_FALSE (1) as a fatal error.
			srv->Term();
		}
		srv->Release();
		sdata->Release();
		ASSERT(_ref == 1);
	}
	CoUninitialize();
	return hr;
}

STDMETHODIMP ScanTagExt::GetVersion(ULONG *SiteVersion)
{
	*SiteVersion = MAKELONG(LIB_VERMINOR, LIB_VERMAJOR);
	return S_OK;
}

STDMETHODIMP ScanTagExt::GetState(SCANSITE_STATE *SiteState)
{
	if (isKilled())
		*SiteState = SSTATE_KILLED;
	else
		*SiteState = SSTATE_RUNNING;
	return S_OK;
}

STDMETHODIMP ScanTagExt::GetFileName(BSTR *FileName)
{
	bstring s(_bmv->getFilename());
	*FileName = s.detach();
	return S_OK;
}

STDMETHODIMP ScanTagExt::TagData(USHORT Flags, LONGLONG TagOffset, LONG TagLength, BSTR TagNote, SHORT ParentIndex, SHORT *TagIndex)
{
	// make a range check.
	if (TagOffset < 0 || (TagOffset + TagLength) > (LONGLONG)_bmv->_fri.FileSize.QuadPart)
		return E_INVALIDARG;
	// ParentIndex is a 1-based index to an item in the _regions list. or, it is 0 if a parent annotation is not required.
	if (ParentIndex < 0 || ParentIndex > _bmv->_regions.size())
		return E_INVALIDARG;
	int iColor = _bmv->_regions.size() % MAX_DUMPREGIONCOLORS;
	COLORREF clr1 = s_regionBorderColor[iColor];
	COLORREF clr2 = s_regionInteriorColor[iColor];
	LARGE_INTEGER dataOffset;
	dataOffset.QuadPart = TagOffset;
	ULONG flags2 = MAKELONG(Flags, ParentIndex);
	if (ParentIndex != 0)
		flags2 |= BMVADDANNOT_MASK_PARENTREGION;
	int indx = _bmv->addTaggedRegion(flags2, dataOffset, TagLength, clr1, clr2, TagNote);
	if (TagIndex)
		*TagIndex = indx;
	return S_OK;
}

STDMETHODIMP ScanTagExt::Annotate(LONG TagIndex, VARIANT *SourceData)
{
	HRESULT hr = E_INVALIDARG;
	_bmv->lockObjAccess();
	// TagIndex is a 1-based index to an item on the _regions list. 
	if (0 < TagIndex && TagIndex <= (int)_bmv->_regions.size())
	{
		DUMPANNOTATIONINFO *ai;
		if (SourceData->vt == VT_BSTR)
		{
			ai = _bmv->_annotations.annotateRegion(&_bmv->_regions, TagIndex, SourceData->bstrVal);
		}
		else if (SourceData->vt == VT_UNKNOWN)
		{
			ai = _bmv->openAnnotationOfRegion(TagIndex, true);
			if (ai)
			{
				IPicture *ipic;
				hr = SourceData->punkVal->QueryInterface(IID_IPicture, (LPVOID*)&ipic);
				if (hr == S_OK)
				{
					hr = _bmv->_annotations.setPicture(ai, ipic);
					ipic->Release();
				}
			}
		}
	}
	_bmv->unlockObjAccess();
	return hr;
}

//////////////////////////////////////////////////////////

ScanDataImpl::ScanDataImpl(FILEREADINFO2 *fi, ScanTag *sm) : _fi(fi), _sm(sm)
{
}

STDMETHODIMP ScanDataImpl::GetFileSize(LONGLONG *SizeInBytes)
{
	if (!_fi)
		return E_UNEXPECTED;
	*SizeInBytes = (LONGLONG)_fi->FileSize.QuadPart;
	return S_OK;
}

STDMETHODIMP ScanDataImpl::GetFilePointer(LONGLONG *Offset)
{
	if (!_fi)
		return E_UNEXPECTED;
	*Offset = _fi->StartOffset.QuadPart;
	return S_OK;
}

STDMETHODIMP ScanDataImpl::Read(LPVOID DataBuffer, ULONG *DataSizeInBytes)
{
	if (!_fi || !_sm)
		return E_UNEXPECTED;
	ULONG len1 = *DataSizeInBytes;
	ULONG len2 = _sm->fread(DataBuffer, 1, len1, _fi);
	if (len2 == 0 && _fi->ErrorCode)
		return _sm->isKilled()? E_ABORT : E_FAIL;
	*DataSizeInBytes = len2;
	return len1==len2? S_OK : S_FALSE;
}

STDMETHODIMP ScanDataImpl::Seek(LONG MoveDistance, SCANSEEK_ORIGIN SeekOrigin)
{
	if (!_fi || !_sm)
		return E_UNEXPECTED;
	int res = _sm->fseek(_fi, MoveDistance, (ROFSEEK_ORIGIN)SeekOrigin);
	return HRESULT_FROM_WIN32(res);
}

STDMETHODIMP ScanDataImpl::Search(LPBYTE BytesToSearch, ULONG ByteCount)
{
	if (!_fi || !_sm)
		return E_UNEXPECTED;
	HRESULT hr = _sm->fsearch(_fi, BytesToSearch, ByteCount);
	return hr;
}

STDMETHODIMP ScanDataImpl::ParseMetaData(SCAN_METADATA_TYPE MetaType, LONGLONG MetaOffset, LONG MetaLength, SHORT ParentIndex, SHORT *TagIndex)
{
	HRESULT hr = E_INVALIDARG;

	if (MetaType == SMDTYPE_EXIF)
		hr = _sm->parseExifMetaData(_fi, MetaOffset, MetaLength, ParentIndex);
	else if (MetaType == SMDTYPE_XMP)
		hr = _sm->parseXmpMetaData(_fi, MetaOffset, MetaLength, ParentIndex);
	else if (MetaType == SMDTYPE_ICCP)
		hr = _sm->parseIccpMetaData(_fi, MetaOffset, MetaLength, ParentIndex);
	else if (MetaType == SMDTYPE_PHOTOSHOP)
		hr = _sm->parsePhotoshopMetaData(_fi, MetaOffset, MetaLength, ParentIndex);

	if (hr == S_OK && TagIndex)
		*TagIndex = LOWORD(_sm->_bmv->_regions.size());

	return hr;
}

