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
#include "hextabscan.h"


class ScanServerImpl :
	public IUnknownImpl<IHexTabScanServer, &IID_IHexTabScanServer>
{
public:
	ScanServerImpl();
	~ScanServerImpl();

	// IHexTabScanServer methods
	STDMETHOD(Init)(IHexTabScanSite *ScanSite, IHexTabScanData *SourceData);
	STDMETHOD(Term)();
	STDMETHOD(Scan)();
	STDMETHOD(GetErrorMessage) (BSTR *Message);

protected:
	AutoComRel<IHexTabScanSite> _site;
	AutoComRel<IHexTabScanData> _data;
	bstring _msg;

	typedef CHAR FOURCC[4];
	typedef BYTE INT24[3];

#pragma pack(1)
	struct CHUNK_RIFF
	{
		FOURCC ChunkId;
		DWORD ChunkSize;
		BYTE Payload[1];
	};
	struct CHUNK_HEADER
	{
		FOURCC ChunkId;
		DWORD ChunkSize;
		FOURCC Signature;
	};
	union VP8_FRAMETAG {
		INT24 b;
		struct _m {
			DWORD FrameType : 1;
			DWORD Version : 3;
			DWORD ShowFrame : 1;
			DWORD DataPartition1 : 19;
		} m;
	};
	struct PAYLOAD_VP8
	{
		//FOURCC ChunkId; // 'VP8 '
		//DWORD ChunkSize; // size in bytes of the lossy compressed data
		INT24 FrameTag; // cast it to VP8_FRAMETAG
		INT24 StartCode; // {0x9d,0x01,0x2a}
		WORD Width : 14; //2 bytes
		WORD HorzScale : 2;
		WORD Height; //2 bytes
		WORD VertScale : 2;
		//BYTE Data[1]; // VP8 data of ChunkSize bytes
	};
	struct PAYLOAD_VP8L
	{
		//FOURCC ChunkId; // 'VP8L'
		//DWORD ChunkSize; // size in bytes of the lossless compressed data
		DWORD Width : 14; // add 1 to get the actuall width
		DWORD Height : 14; // add 1 to get the actuall height
		DWORD AlphaIsUsed : 1;
		DWORD Version : 3;
		//BYTE Data[1]; // VP8L data of ChunkSize bytes
	};
	struct PAYLOAD_VP8X
	{
		//FOURCC ChunkId; // 'VP8X'
		//DWORD ChunkSize; // size in bytes of the extension data (=4+3+3=10 or greater)
		DWORD Rsv1 : 2;
		DWORD ICCP : 1;
		DWORD Alpha : 1;
		DWORD Exif : 1;
		DWORD XMP : 1;
		DWORD Animation : 1;
		DWORD Rsv2 : 1;
		DWORD Reserved : 24;
		INT24 Width; // add 1 to get the actuall width
		INT24 Height; // add 1 to get the actuall height
	};
	struct PAYLOAD_ANIM
	{
		//FOURCC ChunkId; // 'ANIM'
		DWORD ChunkSize; // size in bytes of the extension data (=4+2=6 or greater)
		DWORD BackgroundColor;
		WORD LoopCount;
	};
	struct PAYLOAD_ANMF
	{
		//FOURCC ChunkId; // 'ANMF'
		//DWORD ChunkSize; // size in bytes of the extension data (=5*3+2=17 or greater)
		INT24 FrameX;
		INT24 FrameY;
		INT24 FrameWdith;
		INT24 FrameHeight;
		INT24 FrameDuration;
		WORD Reserved : 6;
		WORD Blending : 1;
		WORD Disposal : 1;
		BYTE FrameData[1];
	};
	struct PAYLOAD_ALPHA
	{
		//FOURCC ChunkId; // 'ALPH'
		//DWORD ChunkSize; // size in bytes of the extension data (=4+2=6 or greater)
		BYTE Rsv : 1;
		BYTE Preprocessing : 1;
		BYTE FilteringMethod : 2;
		BYTE CompressionMethod : 2;
		//BYTE AlphaData[1]; // encoded alpha bitstream of ChunkSize - 1 bytes
	};
	struct PAYLOAD_ICCP
	{
		//FOURCC ChunkId; // 'ICCP'
		//DWORD ChunkSize; // size in bytes of the extension data (=4 or greater)
		DWORD ColorProfile;
	};
	struct CHUNK_METADATA
	{
		FOURCC ChunkId; // 'EXIF' or 'XMP '
		DWORD ChunkSize; // size in bytes of the extension data
		BYTE Metadata[1];
	};
#pragma pack()

	enum CHUNK_TYPE
	{
		UNK,
		WEBP,
		VP8,
		VP8L,
		VP8X,
		ANIM,
		ANMF,
		ALPH,
		ICCP,
		EXIF,
		XMP,
	};
	CHUNK_TYPE getChunkType(CHUNK_RIFF &chunk);
};

#define CHUNKSIZEOF(x) (FIELD_OFFSET(CHUNK_RIFF, Payload)+sizeof(x))
