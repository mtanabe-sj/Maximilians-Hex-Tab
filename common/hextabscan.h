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
#ifndef __HEXTABSCAN_H_
#define __HEXTABSCAN_H_


#define HEXDUMPTAB_REGKEY L"Software\\mtanabe\\HexTab"
#define SCANSERVER_REGKEY HEXDUMPTAB_REGKEY L"\\ScanServers"
#define SCANSERVER_EXTGROUPS_REGKEY SCANSERVER_REGKEY L"\\ExtGroups"


/* b278fa47-3c63-11ed-9d82-00155d938f70 */
DEFINE_GUID(IID_IHexTabScanServer, 0xb278fa47, 0x3c63, 0x11ed, 0x9d, 0x82, 0x00, 0x15, 0x5d, 0x93, 0x8f, 0x70);
/* 0e5b3a3d-3c64-11ed-9d82-00155d938f70 */
DEFINE_GUID(IID_IHexTabScanSite, 0x0e5b3a3d, 0x3c64, 0x11ed, 0x9d, 0x82, 0x00, 0x15, 0x5d, 0x93, 0x8f, 0x70);
/* 24a55d3f-3c8d-11ed-9d82-00155d938f70 */
DEFINE_GUID(IID_IHexTabScanData, 0x24a55d3f, 0x3c8d, 0x11ed, 0x9d, 0x82, 0x00, 0x15, 0x5d, 0x93, 0x8f, 0x70);


enum SCANSEEK_ORIGIN { SSEEK_ORIGIN_BEGIN, SSEEK_ORIGIN_CUR, SSEEK_ORIGIN_END };
enum SCANSITE_STATE { SSTATE_RUNNING, SSTATE_KILLED };
enum SCAN_METADATA_TYPE { SMDTYPE_UNK, SMDTYPE_EXIF, SMDTYPE_XMP, SMDTYPE_ICCP, SMDTYPE_PHOTOSHOP };


// bit flags of method IHexTabScanSite::TagData.
#define SCANSITETAGDATA_FLAG_TRANSPARENTBACKGROUND 0x00000001
#define SCANSITETAGDATA_FLAG_SEMIOPAQUEBACKGROUND 0x00000002
#define SCANSITETAGDATA_FLAG_HEADER 0x00000004
#define SCANSITETAGDATA_FLAG_GRAYTEXT 0x00000008
#define SCANSITETAGDATA_FLAG_MODIFIABLE 0x00000010
#define SCANSITETAGDATA_FLAG_SEMIOPAQUEBACKGROUND2 0x00000020
#define SCANSITETAGDATA_FLAG_ADD_TO_SUMMARYLIST 0x00000080

#undef INTERFACE
#define INTERFACE IHexTabScanData

DECLARE_INTERFACE_(IHexTabScanData, IUnknown)
{
	// *** IUnknown methods ***
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR *ppvObj)  PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	// *** IHexTabScanData methods ***
	STDMETHOD(GetFileSize) (THIS_ LONGLONG *SizeInBytes) PURE;
	STDMETHOD(GetFilePointer) (THIS_ LONGLONG *Offset) PURE;
	STDMETHOD(Read) (THIS_ LPVOID DataBuffer, ULONG *DataSizeInBytes) PURE;
	STDMETHOD(Seek) (THIS_ LONG MoveDistance, SCANSEEK_ORIGIN SeekOrigin) PURE;
	STDMETHOD(Search) (THIS_ LPBYTE BytesToSearch, ULONG ByteCount) PURE;
	STDMETHOD(ParseMetaData)(THIS_ SCAN_METADATA_TYPE MetaType, LONGLONG MetaOffset, LONG MetaLength, SHORT ParentIndex, SHORT *TagIndex) PURE;
};


// values that parameter DataType of Annotate can be set to
#define IHexDumpScanSite_Annotate_DataType_Text 0
#define IHexDumpScanSite_Annotate_DataType_Bitmap 1

#undef INTERFACE
#define INTERFACE IHexTabScanSite

DECLARE_INTERFACE_(IHexTabScanSite, IUnknown)
{
	// *** IUnknown methods ***
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR *ppvObj)  PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	// *** IHexTabScanSite methods ***
	STDMETHOD(GetVersion)(THIS_ ULONG *SiteVersion) PURE;
	STDMETHOD(GetState) (THIS_ SCANSITE_STATE *SiteState) PURE;
	STDMETHOD(GetFileName)(THIS_ BSTR *FileName) PURE;
	STDMETHOD(TagData)(THIS_ USHORT Flags, LONGLONG TagOffset, LONG TagLength, BSTR TagNote, SHORT ParentIndex, SHORT *TagIndex) PURE;
	STDMETHOD(Annotate)(THIS_ LONG TagIndex, VARIANT *SourceData) PURE;
};


#undef INTERFACE
#define INTERFACE IHexTabScanServer

DECLARE_INTERFACE_(IHexTabScanServer, IUnknown)
{
	// *** IUnknown methods ***
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR *ppvObj)  PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	// *** IHexTabScanServer methods ***
	STDMETHOD(Init) (THIS_ IHexTabScanSite *ScanSite, IHexTabScanData *SourceData) PURE;
	STDMETHOD(Term) (THIS) PURE;
	STDMETHOD(Scan) (THIS) PURE;
	STDMETHOD(GetErrorMessage) (THIS_ BSTR *Message) PURE;
};


#endif //__HEXTABSCAN_H_
