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


//#define SHAPES_FADE_ON_GOING_OUT_OF_FOCUS
#define APP_USES_HOTKEYS
#define BINHEXVIEW_SAVES_PAINTED_PAGE

////////////////////////////////////////////////////////////////

#define TIMERID_DELAYTRANSPARENTANNOTATION 1
#define TIMERID_DELAYCONFIGSCROLLBARS 2
#define TIMERID_DELAYSHOWANNOT 3
#define TIMERID_DELAYPERSISTANNOT 4
#define TIMERMSEC_DELAYTRANSPARENTANNOTATION 250

#define READFILE_BUFSIZE 0x1000
#define DEFAULT_ANNOTATION_FONT_SIZE 10
#define DEFAULT_FONT_SIZE 8
#define DEFAULT_PRINTER_FONT_SIZE 10
#define DEFAULT_HEX_BYTES_PER_ROW 8
#define MAX_HEX_BYTES_PER_ROW 32
#define FRAMEMARGIN_CX 0
#define FRAMEMARGIN_CY 0
#define PAGEFRAMEMARGIN_CX 2
#define PAGEFRAMEMARGIN_CY 2

#define ANNOTATION_FONT_FACE L"Arial"
#define SEARCH_FONT_FACE L"Courier New"
#define DUMP_FONT_FACE L"Courier New"
#define OFFSET_COL_FORMAT "%08X  "
#define OFFSET_COL_LEN 8
#define LEFT_SPACING_COL_LEN 2
#define RIGHT_SPACING_COL_LEN 1
#define COL_LEN_PER_DUMP_BYTE 3
#define DIGITS_PER_DUMP_BYTE 2
#define DRAWTEXT_MARGIN_CX 2
#define DRAWTEXT_MARGIN_CY 1

#define FONT_PTS_PER_INCH 72

#define FINDCONFIGFLAG_ENCODING_MASK	0x00000003
#define FINDCONFIGFLAG_ENCODING_UTF8	0x00000000
#define FINDCONFIGFLAG_ENCODING_UTF16	0x00000001
#define FINDCONFIGFLAG_ENCODING_RAW		0x00000002
#define FINDCONFIGFLAG_DIR_MASK				0x00000100
#define FINDCONFIGFLAG_DIR_FORWARD		0x00000000
#define FINDCONFIGFLAG_DIR_BACKWARD		0x00000100
#define FINDCONFIGFLAG_MATCHFOUND			0x80000000

#define TEXT2HEXMAP_FLAG_CHAR_SELECTED	0x00000001
#define TEXT2HEXMAP_FLAG_ENCODING_MASK	0x00030000
#define TEXT2HEXMAP_FLAG_ENCODING_UTF8	0x00010000
#define TEXT2HEXMAP_FLAG_ENCODING_UTF16	0x00020000

#define MAX_DUMPREGIONCOLORS 16
#define MAX_DUMPGSHAPECOLORS 16

#define MAX_DUMPREGIONCOLORNAME 16
// longest color name in IDS_GSHAPE_COLORS.
#define MAX_DUMPGSHAPECOLORNAME 17
// black square as a bullet; it identifies which menu item maps to the color used by a selected tag or gshape.
#define WCHAR_COLORSAMPLE_BULLET 0x25a0

#define POSTMOUSEFLYOVER_RESTORE_MARGIN 8


enum TEXTBYTEORDERMARK {
	TEXT_UNKNOWN,
	TEXT_GENERIC, // 8-bit latin-1
	TEXT_ASCII, // 7-bit ascii
	TEXT_UTF7_A,
	TEXT_UTF7_B,
	TEXT_UTF7_C,
	TEXT_UTF7_D,
	TEXT_UTF7_E,
	TEXT_UTF8, // utf-8
	TEXT_UTF8_NOBOM, // likely utf-8 (no bom found)
	TEXT_UTF16, // unicode (little endian)
	TEXT_UTF16_NOBOM, // likely unicode (no bom found)
	TEXT_UTF16_BE,
	TEXT_UTF32,
	TEXT_UTF32_BE,
};
struct BYTEORDERMARK_DESC {
	UINT AssociatedResourceId;
	TEXTBYTEORDERMARK BOM;
	bool CanBeAdded;
	bool CanBeRemoved;
	USHORT DataLength;
	LPBYTE Data;
	LPCSTR Description;
};

const BYTE DUMPREGIONINFO_FLAG_READONLY = 0x01;
const BYTE DUMPREGIONINFO_FLAG_NESTED = 0x02;
const BYTE DUMPREGIONINFO_FLAG_SETLABEL = 0x04;
const BYTE DUMPREGIONINFO_FLAG_TAG = 0x08;

#pragma pack(1)
struct DUMPREGIONINFO {
	LARGE_INTEGER DataOffset;
	ULONG DataLength;
	COLORREF ForeColor;
	COLORREF BackColor;
	COLORREF BorderColor;
	bool NeedBorder;
	bool RenderAsHeader;
	BYTE RegionType; // 0 for a colored region; 1 for an underlined region; 2 for an overlined region
	BYTE Flags; // DUMPREGIONINFO_FLAG_*
	USHORT LabelStart, LabelStop;
	ULONG Reserved[3];
	char DataEnd;
	UINT AnnotCtrlId; // an associated annotation's CtrlId
};
struct DUMPANNOTATIONINFO {
	WCHAR Text[256];
	WCHAR TextEnd;
	BYTE Flags; // DUMPANNOTATION_FLAG_*
	USHORT TextLength;
	// an ID of a source file from which the thumbnail image is generated. see AnnotationOperation::attachThumbnail. renderAnnotation uses it to refer to the original source file to re-generate a thumbnail in high definition when rendering to a printer. ScanMarker subclasses assign a default ID of 0 to mean that the source file is the master file being hex-dumped. An image pasted in from the system clipboard is given a non-zero value. HexdumpPrintJob must prepare the file name array in its _highdefVP->ImageSources of VIEWINFO before calling repaint() to start printing.
	USHORT ImageSrcId;
	USHORT FontSize;
	POINT FrameOffset;
	SIZE FrameSize;
	LARGE_INTEGER DataOffset;
	COLORREF ForeColor;
	COLORREF BackColor;
	UINT CtrlId;
	HWND EditCtrl;
	UINT RegionId;
	UINT ThumbnailId; // an ID of a .tmp that GetTempFileName generates or an image ID 
	HBITMAP ThumbnailHandle;
};
struct DUMPSHAPEINFO {
	BYTE Shape; // DUMPSHAPEINFO_SHAPE_*
	BYTE Flags; // DUMPSHAPEINFO_FLAG_*
	USHORT StrokeThickness;
	POINT GeomOffset;
	SIZE GeomSize;
	POINT GeomPts[2];
	USHORT GeomRadius, GeomRadius2;
	SHORT GeomRotation, GeomOptions;
	POINT GeomMidPts[4];
	POINT GeomCornerPts[4];
	POINT GeomOuterPts[4];
	POINT GeomConnectorPts[2];
	LARGE_INTEGER DataOffset;
	COLORREF OutlineColor;
	COLORREF InteriorColor;
};

// icon directory structures
typedef struct
{
	BYTE        bWidth;          // Width, in pixels, of the image
	BYTE        bHeight;         // Height, in pixels, of the image
	BYTE        bColorCount;     // Number of colors in image (0 if >=8bpp)
	BYTE        bReserved;       // Reserved ( must be 0)
	WORD        wPlanes;         // Color Planes
	WORD        wBitCount;       // Bits per pixel
	DWORD       dwBytesInRes;    // sizeof the actual ICONIMAGE data
	DWORD       dwImageOffset;   // byte offset from top of file to the ICONIMAGE data
} ICONDIRENTRY, *LPICONDIRENTRY;

typedef struct
{
	WORD           idReserved;   // Reserved (must be 0)
	WORD           idType;       // Resource Type (1 for icons)
	WORD           idCount;      // How many images?
	ICONDIRENTRY   idEntries[1]; // An entry for each image (idCount of 'em)
} ICONDIR, *LPICONDIR;
#pragma pack()

// bit masks for DUMPANNOTATIONINFO.Flags.
const BYTE DUMPANNOTATION_FLAG_EDITING = 1;
const BYTE DUMPANNOTATION_FLAG_TEXTCHANGED = 2;
const BYTE DUMPANNOTATION_FLAG_EDITCTRLCLOSED = 4;
const BYTE DUMPANNOTATION_FLAG_UIDEFERRED = 8;
const BYTE DUMPANNOTATION_FLAG_SHOWCONNECTOR = 0x10;
const BYTE DUMPANNOTATION_FLAG_READONLY = 0x20;
const BYTE DUMPANNOTATION_FLAG_DONTDELETEIMAGE = 0x40;
const BYTE DUMPANNOTATION_FLAG_THUMBNAIL_SAVED = 0x80;

// shape types for DUMPSHAPEINFO.Shape
const BYTE DUMPSHAPEINFO_SHAPE_LINE = 1;
const BYTE DUMPSHAPEINFO_SHAPE_IMAGE = 2;
const BYTE DUMPSHAPEINFO_SHAPE_RECTANGLE = 3;
const BYTE DUMPSHAPEINFO_SHAPE_ELLIPSE = 4;

// bit masks for DUMPSHAPEINFO.Flags.
const BYTE DUMPSHAPEINFO_FLAG_CHANGING = 1;
const BYTE DUMPSHAPEINFO_FLAG_FILLED = 2;
const BYTE DUMPSHAPEINFO_FLAG_CONNECTOR = 4;

// bit masks for DUMPSHAPEINFO.GeomOptions
const BYTE DUMPSHAPEINFO_OPTION_ARROW = 1;
const BYTE DUMPSHAPEINFO_OPTION_ARROW2 = 2;
const BYTE DUMPSHAPEINFO_OPTION_WAVY = 3;

#define SCANLINELENGTH_1BPP(bmWidth) (4 * (((bmWidth + 7) / 8 + 3) / 4))
#define SCANLINELENGTH_2BPP(bmWidth) (4 * (((bmWidth + 3) / 4 + 3) / 4))
#define SCANLINELENGTH_4BPP(bmWidth) (4 * (((bmWidth + 1) / 2 + 3) / 4))
#define SCANLINELENGTH_8BPP(bmWidth) (4 * ((bmWidth + 3) / 4))
#define SCANLINELENGTH_16BPP(bmWidth) (4 * ((2*bmWidth + 3) / 4))
#define SCANLINELENGTH_24BPP(bmWidth) (4 * ((sizeof(RGBTRIPLE)*bmWidth + 3) / 4))
#define SCANLINELENGTH_32BPP(bmWidth) (sizeof(RGBQUAD)*bmWidth)

const double SHAPEGRIPPER_DIAGNAL = 5.0;
const int SHAPEGRIPPER_HALF = 3;
const int SHAPEGRIPPER_HALF1 = 4;
const int SHAPEGRIPPER_HALF2 = 5;
const int SHAPEGRIPPER_HALF3 = 8;

const int SHAPE_CONNECTOR_HALF_WIDTH = 6;


const COLORREF COLORREF_BALOON = RGB(255, 255, 240);
const COLORREF COLORREF_BALOON_FRAME = RGB(30, 144, 255);
const COLORREF COLORREF_BALOON_ARROW = RGB(0, 0, 128);
const COLORREF COLORREF_BLACK = 0;
const COLORREF COLORREF_WHITE = 0xFFFFFF;
const COLORREF COLORREF_DKGRAY = 0x777777;
const COLORREF COLORREF_GRAY = 0xA7A7A7;
const COLORREF COLORREF_LTGRAY = 0xCCCCCC;
const COLORREF COLORREF_LTLTGRAY = RGB(222, 222, 222);
const COLORREF COLORREF_LTBLUE = RGB(0, 0x80, 0xff);
const COLORREF COLORREF_BLUE = 0xFF0000;
const COLORREF COLORREF_GREEN = 0xFF00;
const COLORREF COLORREF_RED = 0xFF;
const COLORREF COLORREF_YELLOW = 0x00FFFF;
const COLORREF COLORREF_PURPLE = 0x9988CC;
const COLORREF COLORREF_VIOLET = 0xFF008F;
const COLORREF COLORREF_TURQUOISE = RGB(48, 213, 200);
const COLORREF COLORREF_LIME = 0x00FFFB;
const COLORREF COLORREF_PINK = RGB(254, 184, 198);
const COLORREF COLORREF_ORANGE = 0x0058FD;


// name of app's work folder created in %TMP%.
#define APP_WORK_FOLDER L"HexDumpTab"
// paths of troubleshooting image files; saved in the work folder
#define BMP_SAVE_PATH1 (L"%TMP%\\" APP_WORK_FOLDER L"\\_dbg_BinImageTab1.bmp")
#define BMP_SAVE_PATH2 (L"%TMP%\\" APP_WORK_FOLDER L"\\_dbg_BinImageTab2.bmp")
#define BMP_SAVE_PATH3  (L"%TMP%\\" APP_WORK_FOLDER L"\\_dbg_ScaledBitmap.bmp")
#define BMP_SAVE_PATH4  (L"%TMP%\\" APP_WORK_FOLDER L"\\_dbg_CroppedBitmap.bmp")
#define BMP_SAVE_PATH5  (L"%TMP%\\" APP_WORK_FOLDER L"\\_dbg_LoadImageBmp.bmp")

extern HRESULT SaveBmp(HBITMAP hbm, LPCWSTR filename, bool topdownScan = true, HWND hwnd=NULL);
extern HRESULT LoadBmp(HWND hwnd, LPCWSTR filename, HBITMAP *phbmp);
enum HDTIMAGETYPE {
	HDTIT_BMP,
	HDTIT_JPG,
	HDTIT_PNG,
	HDTIT_GIF
};
extern HRESULT LoadImageBmp(HWND hwnd, LPCWSTR filename, HDTIMAGETYPE imgType, SIZE *scaledSz, HANDLE hKill, HBITMAP *phbmp);
extern HDTIMAGETYPE GetImageTypeFromExtension(LPCWSTR imageFilename);
extern LPWSTR GetWorkFolder(LPWSTR buf, int bufSize, bool temp);
extern HRESULT EnsureWorkFolder(bool temp);
extern DWORD CleanDirectory(LPCWSTR dirPath, bool deleteDir);
extern DWORD MoveOrCopyFile(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, bool canKeepSrcIfCopied=true);
extern DWORD CopyFolderContent(LPCWSTR srcDir, LPCWSTR destDir);

extern BOOL FileExists(LPCWSTR pathname);
extern BOOL DirectoryExists(LPCWSTR pathname);

extern HRESULT SaveDataFile(LPCWSTR filename, const void *srcData, ULONG srcLen);
extern HRESULT LoadTextFile(LPCWSTR filename, ustring *outBuf);

extern HRESULT RefitBitmapToViewFrame(HDC hdc, SIZE viewframe, bool forceTopdown, HBITMAP *hbmpInOut);
extern HBITMAP FitBitmapToWindowFrame(HBITMAP hbmSrc, HWND hwnd);
// flags of _FitBitmapToWindowFrame
const ULONG FBTWF_FLAG_RELY_ON_FRAME_HEIGHT = 0x00000001;
const ULONG FBTWF_FLAG_RELY_ON_FRAME_WIDTH = 0x00000002;
const ULONG FBTWF_FLAG_IGNORE_SOURCE_ASPECT = 0x00000008;
const ULONG FBTWF_FLAG_KEEP_SIZE_IF_SMALLER = 0x00000010;
extern HBITMAP _FitBitmapToWindowFrame(HBITMAP hbmSrc, HWND hwnd, SIZE frameSize, HANDLE hKill=NULL, ULONG flags=0);
extern HBITMAP _CropBitmap(HWND hwnd, HBITMAP hbmSrc, RECT *destRect);
extern HRESULT GetSaveAsPath(HWND hwnd, LPWSTR outBuf, int bufSize);

enum GRIPID
{
	GRIPID_INVALID = -1,
	GRIPID_GRIP = 0,
	GRIPID_SIZENW,
	GRIPID_SIZESW,
	GRIPID_SIZESE,
	GRIPID_SIZENE,
	GRIPID_SIZENORTH,
	GRIPID_SIZESOUTH,
	GRIPID_SIZEWEST,
	GRIPID_SIZEEAST,
	GRIPID_CURVEDARROW,
	GRIPID_PINCH,
	MAX_SHAPE_GRIPS
};

const GRIPID START_LINE_SHAPE_GRIP = GRIPID_SIZEWEST;
const GRIPID START_ROTATION_SHAPE_GRIP = GRIPID_CURVEDARROW;

struct DUMPSHAPGRIPINFO
{
	GRIPID GripId;
	size_t ShapeIndex;
	bool Gripped;
	POINT GripPt;
	RECT ImageRect;
	HBITMAP TransitImage;
	RECT PreReplaceRect;
	HBITMAP PreReplaceImage;
};

struct DATARANGEINFO
{
	ULARGE_INTEGER FileSize;
	LARGE_INTEGER StartOffset;
	ULONG RangeBytes;
	ULONG RangeLines;
	bool ByLineCount;
	bool GroupAnnots;
};

struct FILEREADINFO {
	bool HasData;
	bool Backward;
	DWORD ErrorCode;
	ULONG DataUsed;
	ULONG DataLength;
	ULONG BufferSize;
	LPBYTE DataBuffer;
	LARGE_INTEGER DataOffset;
	ULARGE_INTEGER FileSize;
};

// a double buffer version of FILEREADINFO.
struct FILEREADINFO2 : public FILEREADINFO {
	// starting address of double buffer (2 x BufferSize). FILEREADINFO.DataBuffer is DoubleBuffer+BufferSize.
	LPBYTE DoubleBuffer;
	// starting address of a new data block just read
	LPBYTE DataStart;
	// 64-bit offset to beginning of new data block
	LARGE_INTEGER StartOffset;
	// number of bytes of new data block
	ULONG StartLength;
	// bytes being served; a combined value of StartLength and DataLength
	ULONG ServedLength;
	// remaining bytes; a combined value of StartLength and DataLength minus ServedLength
	ULONG RemainingLength;
	// fread() saves previous value of StartOffset in PriorStart.
	LARGE_INTEGER PriorStart;
	// fread() saves previous value of ServedLength in PriorLength.
	ULONG PriorLength;
};

struct PairIntInt
{
	int first;
	int second;
};

