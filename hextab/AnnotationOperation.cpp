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
#include "AnnotationOperation.h"
#include "CRegionOperation.h"
#include "resource.h"
#include "libver.h"


/* continueDrag() can use the clipping. but for now, it's unnecessary.
#define ANNOTOP_CLIPS_REGION
*/

#define ANNOTATION_SUPPORTS_LARGETEXT

#define ANNOTATION_GRIP_WIDTH 4

// annotation fadeout control times in milliseconds
#define ANNOTATION_BEFORE_FADEOUT 4000
#define ANNOTATION_AFTER_FADEOUT 6000

// thumbnail image resolution in pixels for a bitmap picture inserted into an annotation.
#define DUMPANNOTTHUMBNAIL_WIDTH 200
#define DUMPANNOTTHUMBNAIL_HEIGHT 200

#define ANNOTATION_RESOURCE_THUMBNAIL L"thu"
#define ANNOTATION_RESOURCE_IMAGE L"ima"
#define ANNOTATION_RESOURCE_TEXT L"txt"


bool AnnotationOperation::initUI(LARGE_INTEGER contentPos, UINT id, bool startEditing)
{
	_ai.ForeColor = COLORREF_BLUE;
	_ai.DataOffset = contentPos;
	_ai.FrameOffset = { -8 * (int)_coll->_vc->_vi.AnnotationColWidth, (int)_coll->_vc->_vi.AnnotationRowHeight >> 1 };
	_ai.FrameSize = { 8 * (int)_coll->_vc->_vi.AnnotationColWidth + 4,(int)_coll->_vc->_vi.AnnotationRowHeight + 4 };
	_ai.CtrlId = IDC_ANNOTATION_EDITCTRL0 + id;
	if (!startEditing)
	{
		// the annotation is located outside the frame.
		_ai.Flags |= DUMPANNOTATION_FLAG_UIDEFERRED | DUMPANNOTATION_FLAG_READONLY;
		return true;
	}
	RECT aiRect;
	if (!annotationInFrame(aiRect))
		return false;
	_ai.Flags |= DUMPANNOTATION_FLAG_EDITING;
	_ai.EditCtrl = CreateWindowEx(0, L"EDIT", NULL,
		WS_BORDER | WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
		aiRect.left, aiRect.top,
		_ai.FrameSize.cx, _ai.FrameSize.cy,
		_coll->_vc->_hw,
		(HMENU)(UINT_PTR)_ai.CtrlId,
		LibInstanceHandle,
		NULL);
	SendMessage(_ai.EditCtrl, WM_SETFONT, (WPARAM)_coll->_vc->_hfontAnnotation, FALSE);
	return true;
}

bool AnnotationOperation::reinitUI(bool startEditing)
{
	if (!(_ai.Flags & DUMPANNOTATION_FLAG_UIDEFERRED))
		return false;
	_ai.Flags &= ~DUMPANNOTATION_FLAG_UIDEFERRED;
	if (_ai.Flags & DUMPANNOTATION_FLAG_READONLY)
		return true;
	DWORD wstyle = WS_BORDER | WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL;
	RECT aiRect = { 0 };
	if (!annotationInFrame(aiRect))
		wstyle &= ~WS_VISIBLE; // hide it since it's outside the view frame.
	else if (!startEditing)
		wstyle &= ~WS_VISIBLE;
	if (wstyle & WS_VISIBLE)
		_ai.Flags |= DUMPANNOTATION_FLAG_EDITING;
	/*	the tasks of creating the window and setting the text to it do not seem to mix. for some text, it works. for others, it does not. the api raises an access violation error. it's better to separate the two tasks.
	_ai.EditCtrl = CreateWindowEx(0, L"EDIT", getText(), wstyle,
		aiRect.left, aiRect.top,
		_ai.FrameSize.cx, _ai.FrameSize.cy,
		_coll->_vc->_hw,
		(HMENU)(UINT_PTR)_ai.CtrlId,
		LibInstanceHandle,
		NULL);
	if (!_ai.EditCtrl)
	{
		DWORD error = GetLastError();
		DBGPRINTF((L"AnnotationOperation::reinitUI - CreateWindowEx failed: error %d; CtrlId=%d, Text='%s'\n", error, _ai.CtrlId, getText()));
		_ai.EditCtrl = CreateWindowEx(0, L"EDIT", NULL, wstyle,
			aiRect.left, aiRect.top,
			_ai.FrameSize.cx, _ai.FrameSize.cy,
			_coll->_vc->_hw,
			(HMENU)(UINT_PTR)_ai.CtrlId,
			LibInstanceHandle,
			NULL);
		if (_ai.EditCtrl)
		{
			SetWindowText(_ai.EditCtrl, getText());
			return true;
		}
		return false;
	}
	*/
	_ai.EditCtrl = CreateWindowEx(0, L"EDIT", NULL, wstyle,
		aiRect.left, aiRect.top,
		_ai.FrameSize.cx, _ai.FrameSize.cy,
		_coll->_vc->_hw,
		(HMENU)(UINT_PTR)_ai.CtrlId,
		LibInstanceHandle,
		NULL);
	if (!_ai.EditCtrl)
	{
		DWORD error = GetLastError();
		DBGPRINTF((L"AnnotationOperation::reinitUI - CreateWindowEx failed: error %d; CtrlId=%d, Text='%s'\n", error, _ai.CtrlId, _ai.Text));
		return false;
	}
	SendMessage(_ai.EditCtrl, WM_SETFONT, (WPARAM)_coll->_vc->_hfontAnnotation, FALSE);
	SetWindowText(_ai.EditCtrl, getText());
	return true;
}

int _getTextCacheEntryId(LPCWSTR srcText)
{
	if (srcText[0] == '[' && srcText[1] == '@')
	{
		LPCWSTR p1 = srcText + 2;
		LPCWSTR p2 = wcschr(srcText, ']');
		if (p2)
		{
			ustring t(p1, (int)(p2 - p1));
			return _wtoi(t);
		}
	}
	return 0;
}

LPCWSTR AnnotationOperation::formatAsMenuItemLabel(ustring &outStr)
{
	LPWSTR p2 = wcschr(_ai.Text, '\n');
	if (p2)
	{
		outStr.clear();
		objList<ustring> *lines = ustring(_ai.Text).split('\n');
		for (int i = 0; i < lines->size(); i++)
		{
			outStr.append(*(*lines)[i]);
			if (outStr._length > 64)
			{
				outStr._buffer[63] = 0x2026; // ellipsis
				outStr._buffer[64] = 0;
				outStr._length = 64;
				break;
			}
			outStr.append(L" ");
		}
		delete lines;
	}
	else
		outStr.assign(_ai.Text);
	return outStr;
}

LPCWSTR AnnotationOperation::getText()
{
#ifdef ANNOTATION_SUPPORTS_LARGETEXT
	if (_ai.TextLength == ARRAYSIZE(_ai.Text) - 1)
	{
		int entryId = _getTextCacheEntryId(_ai.Text);
		AnnotationCollection::TextCacheEntry *cent = _coll->fromCache(entryId);
		if (cent)
			return cent->Data;
		ustring filename;
		queryWorkPath(true, ANNOTATION_RESOURCE_TEXT, entryId, filename);
		cent = new AnnotationCollection::TextCacheEntry;
		if (S_OK == LoadTextFile(filename, &cent->Data))
			return _coll->toCache(cent);
	}
#endif//#ifdef ANNOTATION_SUPPORTS_LARGETEXT
	return _ai.Text;
}

USHORT AnnotationOperation::getTextLength()
{
#ifdef ANNOTATION_SUPPORTS_LARGETEXT
	if (_ai.TextLength == ARRAYSIZE(_ai.Text) - 1)
		return LOWORD(wcslen(getText()));
#endif//#ifdef ANNOTATION_SUPPORTS_LARGETEXT
	return _ai.TextLength;
}

LPCWSTR AnnotationOperation::saveText(LPCWSTR srcText)
{
	if (!srcText)
		srcText = L"";
	_ai.TextLength = (USHORT)wcslen(srcText);
	if (_ai.TextLength >= ARRAYSIZE(_ai.Text))
	{
#ifdef ANNOTATION_SUPPORTS_LARGETEXT
		int entryId = _coll->queryTextSrcCount();
		entryId++;
		ustring rname = _coll->getRegistryKey();
		Registry_SetDwordValue(HKEY_CURRENT_USER, LIBREGKEY L"\\TextSource", rname, entryId);
		int len1 = _swprintf(_ai.Text, L"[@%lu]", entryId);
		int len2 = ARRAYSIZE(_ai.Text) - len1 - 1;
		lstrcpyn(_ai.Text + len1, srcText, len2);
		_ai.Text[ARRAYSIZE(_ai.Text) - 2] = 0x2026; // end it with a unicode ellipsis (...)
		_ai.TextLength = ARRAYSIZE(_ai.Text) - 1;
		ustring filename;
		queryWorkPath(true, ANNOTATION_RESOURCE_TEXT, entryId, filename);
		if (S_OK == SaveDataFile(filename, srcText, sizeof(WCHAR)*(ULONG)wcslen(srcText)))
		{
			ustring filename;
			queryWorkPath(true, ANNOTATION_RESOURCE_TEXT, entryId, filename);
			AnnotationCollection::TextCacheEntry *cent = new AnnotationCollection::TextCacheEntry;
			cent->EntryId = entryId;
			cent->Data.assign(srcText);
			return _coll->toCache(cent);
		}
		else
#endif//#ifdef ANNOTATION_SUPPORTS_LARGETEXT
		{
			CopyMemory(_ai.Text, srcText, sizeof(_ai.Text) - sizeof(WCHAR));
			if (_ai.TextLength > ARRAYSIZE(_ai.Text))
				_ai.Text[ARRAYSIZE(_ai.Text) - 2] = 0x2026; // end it with a unicode ellipsis (...)
			_ai.TextLength = ARRAYSIZE(_ai.Text) - 1;
		}
	}
	else
		wcscpy(_ai.Text, srcText);
	return _ai.Text;
}

LPCWSTR AnnotationOperation::syncText(LPCWSTR textToAdd/*=NULL*/)
{
	int tlen = GetWindowTextLength(_ai.EditCtrl);
	ustring buf;
	buf._length = GetWindowText(_ai.EditCtrl, buf.alloc(tlen), tlen+1);
	if (textToAdd)
		buf.append(textToAdd);
	return saveText(buf);
}

SIZE AnnotationOperation::_measureTextDimensions()
{
	SIZE sz = { 2, 1 };
	/* unfortunately, GetTextExtentPoint32 does not account for LFs (or CR LF pairs).
	HDC hdc = CreateDC(L"DISPLAY", NULL, NULL, NULL);
	if (!GetTextExtentPoint32(hdc, getText(), getTextLength(), &sz))
		sz = { 0 };
	DeleteDC(hdc);
	*/
	LPCWSTR t = getText();
	int tlen = getTextLength();
	int i, j=0;
	for (i = 0; i < tlen; i++)
	{
		if (t[i] == '\n')
		{
			sz.cy++;
			j = 0;
		}
		else if (++j > sz.cx)
			sz.cx = j;
	}
	return sz;
}

bool AnnotationOperation::setText(LPCWSTR srcText)
{
	saveText(srcText);
	SIZE tsz = _measureTextDimensions();
	SIZE fsz = { (tsz.cx + 4)*(int)_coll->_vc->_vi.AnnotationColWidth, tsz.cy*(int)_coll->_vc->_vi.AnnotationRowHeight + 2 };
	// this flag is raised when the call is from a worker thread. see BinhexMetaView::addTaggedRegion which makes such a call.
	if ((_ai.Flags & DUMPANNOTATION_FLAG_UIDEFERRED) || (_ai.Flags & DUMPANNOTATION_FLAG_READONLY))
	{
		_ai.Flags |= DUMPANNOTATION_FLAG_TEXTCHANGED;
		if (fsz.cx >= _ai.FrameSize.cx || fsz.cy >= _ai.FrameSize.cy)
			_ai.FrameSize = fsz;
		return true;
	}
	_ai.Flags &= ~DUMPANNOTATION_FLAG_EDITING;
	_ai.Flags |= DUMPANNOTATION_FLAG_TEXTCHANGED | DUMPANNOTATION_FLAG_EDITCTRLCLOSED;
	ShowWindow(_ai.EditCtrl, SW_HIDE);
	SetWindowText(_ai.EditCtrl, getText());
	if (fsz.cx >= _ai.FrameSize.cx || fsz.cy >= _ai.FrameSize.cy)
	{
		_ai.FrameSize = fsz;
		SetWindowPos(_ai.EditCtrl, NULL, 0, 0, _ai.FrameSize.cx, _ai.FrameSize.cy, SWP_NOZORDER | SWP_NOMOVE);
	}
	return true;
}

bool AnnotationOperation::updateUIText(LPCWSTR textToAdd)
{
	if (_ai.Flags & DUMPANNOTATION_FLAG_READONLY)
		return false;
	syncText(textToAdd);

	SIZE tsz = _measureTextDimensions();
	SIZE fsz = { (tsz.cx + 4)*(int)_coll->_vc->_vi.AnnotationColWidth, tsz.cy*(int)_coll->_vc->_vi.AnnotationRowHeight + 2 };
	_inflateSizeForImage(fsz);
	// is the text hitting the right edge of the edit pane?
	if (fsz.cx >= _ai.FrameSize.cx)
	{
		// give it extra typing space to the right of the text.
		fsz.cx += 4 * _coll->_vc->_vi.AnnotationColWidth;
		_ai.FrameSize = fsz;
		SetWindowPos(_ai.EditCtrl, NULL, 0, 0, _ai.FrameSize.cx, _ai.FrameSize.cy, SWP_NOZORDER | SWP_NOMOVE);
	}
	_ai.Flags |= DUMPANNOTATION_FLAG_TEXTCHANGED;
	return true;
}

bool AnnotationOperation::finalizeUIText(bool aborted)
{
	if (_ai.Flags & DUMPANNOTATION_FLAG_READONLY)
		return false;
	if (!(_ai.Flags & DUMPANNOTATION_FLAG_EDITING))
		return false;
	ASSERT(_ai.EditCtrl);
	if (!aborted)
	{
		syncText();
		if (_ai.Flags & DUMPANNOTATION_FLAG_TEXTCHANGED)
			resetDimensions();
	}
	DBGPRINTF((L"AnnotationOperation::finalizeUIText(Aborted=%d): Len=%d; '%s'\n", aborted, _ai.TextLength, _ai.Text));
	_ai.Flags &= ~DUMPANNOTATION_FLAG_EDITING;
	_ai.Flags |= DUMPANNOTATION_FLAG_TEXTCHANGED | DUMPANNOTATION_FLAG_EDITCTRLCLOSED;
	ShowWindow(_ai.EditCtrl, SW_HIDE);
	InvalidateRect(_coll->_vc->_hw, NULL, TRUE);
	return true;
}

void AnnotationOperation::resetDimensions(bool canSizeUI)
{
	SIZE tsz = _measureTextDimensions();
	SIZE fsz = { (tsz.cx + 4)*(int)_coll->_vc->_vi.AnnotationColWidth, tsz.cy*(int)_coll->_vc->_vi.AnnotationRowHeight + 2 };
	_inflateSizeForImage(fsz);
	if (canSizeUI && !_coll->_vc->_highdefVP && _ai.EditCtrl)
		SetWindowPos(_ai.EditCtrl, NULL, 0, 0, fsz.cx, fsz.cy, SWP_NOZORDER | SWP_NOMOVE);
	_ai.FrameSize = fsz;
}

bool AnnotationOperation::onSelect(POINT pt)
{
	DBGPRINTF((L"AnnotationCollection::onSelect(Flags=%X): Len=%d; '%s'\n", _ai.Flags, _ai.TextLength, _ai.Text));
	if (_ai.Flags & DUMPANNOTATION_FLAG_READONLY)
		return false;
	RECT annotRect;
	annotationInFrame(annotRect);
	// estimate caret position that corresponds to the input cursor point.
	POINT pos;
	pos.x = (pt.x - annotRect.left)/_coll->_vc->_vi.ColWidth;
	pos.y = (pt.y - annotRect.top) / _coll->_vc->_vi.RowHeight;
	if (pos.y == 0)
	{
		// move the caret. makes sure that the request is a valid position.
		if (pos.x < 0)
			pos.x = 0;
		else
		{
			SIZE sz = _measureTextDimensions();
			if (pos.x > sz.cx)
				pos.x = sz.cx;
			//TODO: what about the vertical displacement?
		}
		SendMessage(_ai.EditCtrl, EM_SETSEL, pos.x, pos.x);
	}
	// unhide the edit control and let the user make changes to the text.
	_ai.Flags |= DUMPANNOTATION_FLAG_EDITING;
	/*
	_ai.Flags &= ~DUMPANNOTATION_FLAG_EDITCTRLCLOSED;
	SetWindowPos(_ai.EditCtrl, NULL, annotRect.left, annotRect.top, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER);
	SetFocus(_ai.EditCtrl);
	*/
	_coll->_vc->invalidate();
	return true;
}

bool AnnotationOperation::reselect()
{
	DBGPRINTF((L"AnnotationOperation::reselect: Flags=%X\n", _ai.Flags));
	if (!(_ai.Flags & DUMPANNOTATION_FLAG_EDITING))
		return false;
	RECT annotRect;
	annotationInFrame(annotRect);
	_ai.Flags &= ~DUMPANNOTATION_FLAG_EDITCTRLCLOSED;
	SetWindowPos(_ai.EditCtrl, NULL, annotRect.left, annotRect.top, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER);
	SetFocus(_ai.EditCtrl);
	return true;
}

bool AnnotationOperation::containsPt(POINT pt, bool *onGripSpot)
{
	if (onGripSpot)
		*onGripSpot = false;
	RECT annotRect;
	if (!annotationInFrame(annotRect))
		return false;
	if (PtInRect(&annotRect, pt))
	{
		if (onGripSpot)
		{
			RECT innerRect = annotRect;
			InflateRect(&innerRect, -ANNOTATION_GRIP_WIDTH, -ANNOTATION_GRIP_WIDTH);
			if (!PtInRect(&innerRect, pt))
				*onGripSpot = true;
		}
		return true;
	}
	return false;
}

bool AnnotationOperation::annotationInFrame(RECT &outAnnotRect, POINT *outOriginPt)
{
	LARGE_INTEGER annotStart, annotEnd;
	LARGE_INTEGER dataEnd = { _coll->_vc->_fri.DataLength ,0 };

	annotStart.QuadPart = annotEnd.QuadPart = _ai.DataOffset.QuadPart - _coll->_vc->_fri.DataOffset.QuadPart;
	annotEnd.QuadPart += 2;

	LONG hitRow1, hitRow2;
	LONG hitCol1, hitCol2;

	if (annotStart.QuadPart < 0)
	{
		hitRow1 = (LONG)((annotStart.QuadPart - _coll->_vc->_vi.BytesPerRow - 1) / _coll->_vc->_vi.BytesPerRow);
		hitCol1 = (LONG)(_coll->_vc->_vi.BytesPerRow + (annotStart.QuadPart % _coll->_vc->_vi.BytesPerRow));
	}
	else
	{
		hitRow1 = (LONG)(annotStart.QuadPart / _coll->_vc->_vi.BytesPerRow);
		hitCol1 = (LONG)(annotStart.QuadPart % _coll->_vc->_vi.BytesPerRow);
	}
	hitRow2 = (LONG)(annotEnd.QuadPart / _coll->_vc->_vi.BytesPerRow);
	hitCol2 = (LONG)(annotEnd.QuadPart % _coll->_vc->_vi.BytesPerRow);

	POINT originPt;
	int originPt_x = _coll->_vc->_vi.ColWidth / 2 + 3*hitCol1 * _coll->_vc->_vi.ColWidth + DRAWTEXT_MARGIN_CX + _coll->_vc->_vi.OffsetColWidth;
	int originPt_y = _coll->_vc->_vi.RowHeight / 2 + hitRow1 * _coll->_vc->_vi.RowHeight + DRAWTEXT_MARGIN_CY;
	originPt.x = _coll->_vc->_getHexColCoord(hitCol1, 1, 2) + DRAWTEXT_MARGIN_CX;
	originPt.y = _coll->_vc->_vi.RowHeight / 2 + hitRow1 * _coll->_vc->_vi.RowHeight + DRAWTEXT_MARGIN_CY;

	originPt.x -= _coll->_vc->_vi.CurCol*_coll->_vc->_vi.ColWidth;
	if (originPt.x < (int)_coll->_vc->_vi.OffsetColWidth)
		originPt.x = _coll->_vc->_vi.OffsetColWidth;

	// convert _ai.FrameOffset to a logical offset by calling _getLogicalXCoord and _getLogicalYCoord. note, however, that _ai.FrameSize is already in logical units, requiring no conversion.
	RECT annotRect;
	annotRect.left = originPt.x + _coll->_vc->_getLogicalXCoord(_ai.FrameOffset.x);
	annotRect.top = originPt.y + _coll->_vc->_getLogicalYCoord(_ai.FrameOffset.y);
	annotRect.right = annotRect.left + _ai.FrameSize.cx;
	annotRect.bottom = annotRect.top + _ai.FrameSize.cy;

	int dx = annotRect.right - _coll->_vc->_vi.FrameSize.cx;
	if (dx > 0)
	{
		dx += 8;
		if (dx > annotRect.left)
			dx = annotRect.left;
		annotRect.right -= dx;
		annotRect.left -= dx;
	}

	DBGPRINTF_VERBOSE((L"annotationInFrame: rect={%d,%d,%d,%d}, pt={%d,%d}\n", annotRect.left, annotRect.top, annotRect.right, annotRect.bottom, originPt.x, originPt.y));

	outAnnotRect = annotRect;
	if (outOriginPt)
		*outOriginPt = originPt;

	RECT frameRect = { 0,0,_coll->_vc->_vi.FrameSize.cx,_coll->_vc->_vi.FrameSize.cy };
	return (PtInRect(&frameRect, *(LPPOINT)&annotRect.left) || PtInRect(&frameRect, *(LPPOINT)&annotRect.right)) ? true : false;
}

HRESULT AnnotationOperation::beforeRemove()
{
	if (_ai.Flags & DUMPANNOTATION_FLAG_READONLY)
		return E_ACCESSDENIED; // this one is readonly, and may not be removed.
	if (_ai.ThumbnailHandle)
	{
		if (!(_ai.Flags & DUMPANNOTATION_FLAG_DONTDELETEIMAGE))
			DeleteObject(_ai.ThumbnailHandle);
		_ai.ThumbnailHandle = NULL;
		_ai.Flags &= ~DUMPANNOTATION_FLAG_DONTDELETEIMAGE;
	}
	if (_ai.ImageSrcId == 0)
		return S_OK;
	// delete the pasted image from the work folder.
	ustring filename;
	if (queryWorkPath(true, ANNOTATION_RESOURCE_IMAGE, _ai.ImageSrcId, filename))
	{
		DeleteFile(filename);
	}
	return S_FALSE;
}

void AnnotationOperation::attachThumbnail(HBITMAP hbmp, USHORT imgSrcId)
{
	DBGPRINTF((L"AnnotationOperation::attachThumbnail: %p (SrcId=%d)\n", hbmp, imgSrcId));
	_ai.Flags &= ~DUMPANNOTATION_FLAG_THUMBNAIL_SAVED;
	if (_ai.ThumbnailHandle)
	{
		if (!(_ai.Flags & DUMPANNOTATION_FLAG_DONTDELETEIMAGE))
			DeleteObject(_ai.ThumbnailHandle);
		_ai.ThumbnailHandle = NULL;
		_ai.Flags &= ~DUMPANNOTATION_FLAG_DONTDELETEIMAGE;
	}
	resetDimensions(false);
	_ai.Flags &= ~DUMPANNOTATION_FLAG_THUMBNAIL_SAVED;
	_ai.ThumbnailHandle = hbmp;
	_ai.ImageSrcId = imgSrcId;
	_inflateSizeForImage(_ai.FrameSize);
}

ULONG AnnotationOperation::queryWorkPath(bool temp, LPCWSTR dataExt, USHORT srcId, ustring &outPath)
{
	ustring dir = _coll->getWorkDirectory(temp);
	WIN32_FIND_DATA wfd = { 0 };
	HANDLE hf = FindFirstFile(dir, &wfd);
	if (hf == INVALID_HANDLE_VALUE)
	{
		if (!CreateDirectory(dir, NULL))
		{
			DBGPRINTF((L"AnnotationOperation::queryWorkPath: CreateDirectory failed (%d)\n", GetLastError()));
			return 0;
		}
	}
	else
	{
		ASSERT(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		FindClose(hf);
	}
	if (srcId == 0)
	{
		for (srcId = 1; srcId < 0x100; srcId++)
		{
			outPath.format(L"%s\\ISRC%04X.%s", (LPCWSTR)dir, srcId, dataExt);
			if (!FileExists(outPath))
				break;
		}
	}
	else
		outPath.format(L"%s\\ISRC%04X.%s", (LPCWSTR)dir, srcId, dataExt);
	return MAKELONG(srcId,0);
}

HRESULT AnnotationOperation::loadThumbnail()
{
	_ai.ThumbnailHandle = NULL;
	if (!(_ai.Flags & DUMPANNOTATION_FLAG_THUMBNAIL_SAVED))
		return S_FALSE;
	ASSERT(_ai.ThumbnailId != 0);
	ustring dataFile;
	queryWorkPath(true, ANNOTATION_RESOURCE_THUMBNAIL, _ai.ThumbnailId, dataFile);
	return LoadBmp(NULL, dataFile, &_ai.ThumbnailHandle);
}

HBITMAP AnnotationOperation::saveThumbnail(bool clearHandle)
{
	if (!_ai.ThumbnailHandle || (_ai.Flags & DUMPANNOTATION_FLAG_THUMBNAIL_SAVED))
		return NULL;
	DBGPRINTF((L"AnnotationOperation::saveThumbnail: %p\n", _ai.ThumbnailHandle));
	ustring dataFile;
	ULONG fileId = queryWorkPath(true, ANNOTATION_RESOURCE_THUMBNAIL, _ai.ThumbnailId, dataFile);
	HBITMAP h = NULL;
	if (S_OK == SaveBmp(_ai.ThumbnailHandle, dataFile, true, _coll->_vc->_hw))
	{
		h = _ai.ThumbnailHandle;
		_ai.ThumbnailId = fileId;
		_ai.Flags |= DUMPANNOTATION_FLAG_THUMBNAIL_SAVED;
		if (clearHandle)
		{
			if (!(_ai.Flags & DUMPANNOTATION_FLAG_DONTDELETEIMAGE))
				DeleteObject(_ai.ThumbnailHandle);
			_ai.ThumbnailHandle = NULL;
			_ai.Flags &= ~DUMPANNOTATION_FLAG_DONTDELETEIMAGE;
		}
	}
	return h;
}

USHORT AnnotationOperation::saveSourceImage(HBITMAP hbmp, int srcId, int *srcCount)
{
	DBGPRINTF((L"AnnotationOperation::saveSourceImage: SrcId=%d, SrcCount=%d\n", srcId, *srcCount));
	_ai.ImageSrcId = srcId;
	ustring filename;
	ULONG imgId = queryWorkPath(true, ANNOTATION_RESOURCE_IMAGE, srcId, filename);
	if (imgId != 0)
	{
		ASSERT(imgId == (ULONG)srcId);
		if (S_OK == SaveBmp(hbmp, filename, true, _coll->_vc->_hw))
		{
			if (srcId > *srcCount)
			{
				ustring rname = _coll->getRegistryKey();
				Registry_SetDwordValue(HKEY_CURRENT_USER, LIBREGKEY L"\\ImageSource", rname, srcId);
				*srcCount = srcId;
			}
		}
		else
			imgId = 0;
	}
	return LOWORD(imgId);
}

HRESULT AnnotationOperation::persistResources()
{
	ustring srcName, destName;
	ULONG srcId, destId;
	if (_ai.ImageSrcId)
	{
		srcId = queryWorkPath(true, ANNOTATION_RESOURCE_IMAGE, _ai.ImageSrcId, srcName);
		destId = queryWorkPath(false, ANNOTATION_RESOURCE_IMAGE, _ai.ImageSrcId, destName);
		MoveOrCopyFile(srcName, destName);
	}
	int entryId = _getTextCacheEntryId(_ai.Text);
	if (entryId)
	{
		srcId = queryWorkPath(true, ANNOTATION_RESOURCE_TEXT, entryId, srcName);
		destId = queryWorkPath(false, ANNOTATION_RESOURCE_TEXT, entryId, destName);
		MoveOrCopyFile(srcName, destName);
	}
	if (_ai.ThumbnailId && (_ai.Flags & DUMPANNOTATION_FLAG_THUMBNAIL_SAVED))
	{
		srcId = queryWorkPath(true, ANNOTATION_RESOURCE_THUMBNAIL, _ai.ThumbnailId, srcName);
		destId = queryWorkPath(false, ANNOTATION_RESOURCE_THUMBNAIL, _ai.ThumbnailId, destName);
		MoveOrCopyFile(srcName, destName);
	}
	return S_OK;
}

// called when user cancels the propsheet.
void AnnotationOperation::cleanUp()
{
	if (_ai.ThumbnailId && (_ai.Flags & DUMPANNOTATION_FLAG_THUMBNAIL_SAVED))
	{
		ustring srcName;
		queryWorkPath(true, ANNOTATION_RESOURCE_THUMBNAIL, _ai.ThumbnailId, srcName);
		DeleteFile(srcName);
	}
}

void AnnotationOperation::_inflateSizeForImage(SIZE &fsz)
{
	if (!_ai.ThumbnailHandle)
		return;
	// will show the image in a bottom area of the rectagle. stretch the annotation's frame to fit the image.
	SIZE imgSz = _getImageSize();
	if (imgSz.cx > fsz.cx)
		fsz.cx = imgSz.cx + 2 * DRAWTEXT_MARGIN_CX;
	fsz.cy += imgSz.cy + 2 * DRAWTEXT_MARGIN_CY;
}

SIZE AnnotationOperation::_getImageSize()
{
	if (!_ai.ThumbnailHandle)
		return { 0,0 };
	BITMAP bm;
	GetObject(_ai.ThumbnailHandle, sizeof(BITMAP), &bm);
	SIZE exts;
	exts.cx = _coll->_vc->screenToLogical((int)bm.bmWidth);
	exts.cy = _coll->_vc->screenToLogical((int)bm.bmHeight);
	return exts;
}

void AnnotationOperation::drawAnnotationAt(HDC hdc, POINT ptOrig, POINT ptLT)
{
	RECT annotRect;
	annotRect.left = ptLT.x;
	annotRect.top = ptLT.y;
	annotRect.right = ptLT.x + _ai.FrameSize.cx;
	annotRect.bottom = ptLT.y + _ai.FrameSize.cy;
	DBGPRINTF_VERBOSE((L"drawAnnotationAt: Origin={%d,%d}; Rect={%d,%d,%d,%d}\n", ptOrig.x, ptOrig.y, annotRect.left, annotRect.top, annotRect.right, annotRect.bottom));

	renderAnnotation(hdc, annotRect, ptOrig, false);
	renderConnectorArrow(hdc, annotRect, ptOrig);
}

HRESULT AnnotationOperation::drawAnnotation(HDC hdc, bool reduceTransparency)
{
	if (_ai.Flags & DUMPANNOTATION_FLAG_EDITING)
		return S_FALSE; // not an error

	DBGPUTS_VERBOSE((L"drawAnnotation"));
	POINT originPt;
	RECT annotRect;
	if (!annotationInFrame(annotRect, &originPt))
		return S_FALSE; // not an error

	HRESULT hr;
	if (reduceTransparency)
	{
		if (_coll->_vc->_annotFadeoutOpacity==0)
			hr = renderAnnotation(hdc, annotRect, originPt);
		else
			hr = blendAnnotation(hdc, annotRect, originPt, LOBYTE(_coll->_vc->_annotFadeoutOpacity));
	}
	else if (_coll->_vc->_annotOpacity >= BHV_FULL_OPACITY)
		hr = renderAnnotation(hdc, annotRect, originPt);
	else
		hr = blendAnnotation(hdc, annotRect, originPt, LOBYTE(_coll->_vc->_annotOpacity));
	return hr;
}

RECT _originateAnnotationRect(RECT annotRect, POINT originPt)
{
	RECT rc = annotRect;
	if (originPt.x < rc.left)
		rc.left = originPt.x;
	else if (originPt.x > rc.right)
		rc.right = originPt.x;
	if (originPt.y < rc.top)
		rc.top = originPt.y;
	else if (originPt.y > rc.bottom)
		rc.bottom = originPt.y;
	return rc;
}

HRESULT AnnotationOperation::blendAnnotation(HDC hdc, RECT annotRect, POINT originPt, BYTE transparency)
{
	DBGPRINTF_VERBOSE((L"blendAnnotation(%d): {%d:%d}; %d\n", this->_ai.CtrlId, originPt.x, originPt.y, transparency));
	RECT rc = _originateAnnotationRect(annotRect, originPt);
	int cx = rc.right - rc.left;
	int cy = rc.bottom - rc.top;
	HDC hdc2 = CreateCompatibleDC(hdc);
	DWORD bmpSize = rc.right*rc.bottom * sizeof(COLORREF);
	BITMAPINFO bmi = { sizeof(BITMAPINFOHEADER), rc.right, rc.bottom, 1, 32, BI_RGB, bmpSize };
	UINT32* bits;
	HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, 0);
	HBITMAP hbmp0 = (HBITMAP)SelectObject(hdc2, hbmp);
	HBRUSH hbr = (HBRUSH)GetStockObject(WHITE_BRUSH);
	FillRect(hdc2, &rc, hbr);
	HRGN hrgn = NULL;
	HRESULT hr = renderAnnotation(hdc2, annotRect, originPt, (_ai.Flags & DUMPANNOTATION_FLAG_SHOWCONNECTOR) ? true : false, &hrgn);
	if (SUCCEEDED(hr))
	{
		OffsetRgn(hrgn, _coll->_vc->_vi.FrameMargin.x, _coll->_vc->_vi.FrameMargin.y);
		_coll->_vc->clipFrame(hdc, hrgn, RGN_AND);
		BLENDFUNCTION bf = { AC_SRC_OVER, 0, transparency, 0 };
		AlphaBlend(hdc, rc.left, rc.top, cx, cy, hdc2, rc.left, rc.top, cx, cy, bf);
		_coll->_vc->unclipFrame(hdc);
	}
	SelectObject(hdc2, hbmp0);
	DeleteObject(hbmp);
	DeleteDC(hdc2);
	if (hrgn)
		DeleteObject(hrgn);
	return hr;
}

bool AnnotationOperation::highlightAnnotation(RECT &hitRect)
{
	DBGPRINTF_VERBOSE((L"highlightAnnotation(%d): Flags=%X\n", _ai.CtrlId, _ai.Flags));
	if ((_ai.Flags & DUMPANNOTATION_FLAG_EDITING) && !(_ai.Flags & DUMPANNOTATION_FLAG_EDITCTRLCLOSED))
	{
		// text editing is ongoing.
		return false;
	}
	POINT originPt;
	RECT annotRect;
	if (!annotationInFrame(annotRect, &originPt))
		return false;
	RECT rc2 = _originateAnnotationRect(annotRect, originPt);
	if (!IsRectEmpty(&hitRect) && !EqualRect(&hitRect, &rc2))
	{
		InflateRect(&hitRect, POSTMOUSEFLYOVER_RESTORE_MARGIN, POSTMOUSEFLYOVER_RESTORE_MARGIN);
		InvalidateRect(_coll->_vc->_hw, &hitRect, TRUE);
	}
	hitRect = rc2;

	HDC hdc = _coll->_vc->setVC(true);
	renderAnnotation(hdc, annotRect, originPt);
	_coll->_vc->releaseVC();
	return true;
}

bool _isSideToOriginAngle45degOrLess(POINT pt, RECT rc)
{
	int cx, cy;
	int drop = (rc.bottom - rc.top) / 2; // half height
	if (pt.x < rc.left)
		cx = rc.left - pt.x;
	else if (rc.right < pt.x)
		cx = pt.x - rc.right;
	else
		return false;
	cy = (rc.top + drop) - pt.y;
	if (cy == 0)
		return true;
	else if (cy < 0) // pt is below rc?
		cy = -cy;
	if (cx >= cy)
		return true;
	return false;
}

void AnnotationOperation::_getBaloonLayout(RECT &annotRect, POINT &originPt, POINT pts[7], int inflate)
{
	ZeroMemory(pts, 7 * sizeof(POINT));
	POINT pt = originPt;
	RECT rc = annotRect;
	if (inflate)
	{
		InflateRect(&rc, inflate, inflate);
		pt.y += inflate;
	}

	int dx4 = (rc.right - rc.left) / 4; // quarter length of width
	int dx2 = (rc.right - rc.left) / 2; // half width
	int dy4 = (rc.bottom - rc.top) / 4; // quarter height
	int dy2 = (rc.bottom - rc.top) / 2; // half height
	// dx is the half width of the connector's base.
	int dx = dx4 / 2; // 8th width
	if (dx == 0)
		dx = _coll->_vc->screenToLogical(1); // minimum base width is 2.
	else
	{
		int dx3 = _coll->_vc->screenToLogical(3); // maximum base width is 6.
		if (dx > dx3)
			dx = dx3;
	}

	if (rc.right < pt.x && _isSideToOriginAngle45degOrLess(pt, rc))
	{
		// point of origin is to the right of the rectange.
		pts[0].x = rc.left;
		pts[0].y = rc.top;
		pts[1].x = rc.left;
		pts[1].y = rc.bottom;
		pts[2].x = rc.right;
		pts[2].y = rc.bottom;
		pts[3].x = rc.right;
		pts[3].y = rc.bottom - dy2;
		pts[4].x = pt.x;
		pts[4].y = pt.y;
		pts[5].x = rc.right;
		pts[5].y = rc.top + dy4;
		pts[6].x = rc.right;
		pts[6].y = rc.top;
	}
	else if (pt.x < rc.left && _isSideToOriginAngle45degOrLess(pt, rc))
	{
		// point of origin is to the left of the rectangle.
		pts[0].x = rc.left;
		pts[0].y = rc.top;
		pts[1].x = rc.left;
		pts[1].y = rc.top + dy4;
		pts[2].x = pt.x;
		pts[2].y = pt.y;
		pts[3].x = rc.left;
		pts[3].y = rc.bottom - dy2;
		pts[4].x = rc.left;
		pts[4].y = rc.bottom;
		pts[5].x = rc.right;
		pts[5].y = rc.bottom;
		pts[6].x = rc.right;
		pts[6].y = rc.top;
	}
	else if (rc.left + dx2 - dx4 / 2 < pt.x && pt.x < rc.left + dx2 + dx4 / 2)
	{
		// origin's x is aligned to exactly or near the middle of the rectangle's width. specifically, it's between -1/8 and +1/8 of the width measured from the middle point.
		if (pt.y > rc.top)
		{
			// point of origin is below the rectangle. a connector stretches downward from a middle section of the rectangle's bottom.
			pts[0].x = rc.left;
			pts[0].y = rc.top;
			pts[1].x = rc.left;
			pts[1].y = rc.bottom;
			pts[2].x = rc.left + dx2 + dx;
			pts[2].y = rc.bottom;
			pts[3].x = pt.x;
			pts[3].y = pt.y;
			pts[4].x = rc.right - dx2 - dx;
			pts[4].y = rc.bottom;
			pts[5].x = rc.right;
			pts[5].y = rc.bottom;
			pts[6].x = rc.right;
			pts[6].y = rc.top;
		}
		else
		{
			// point of origin is above the rectangle's top. a connector stretches upward from a middle point of the rectangle's top.
			pts[0].x = rc.left;
			pts[0].y = rc.top;
			pts[1].x = rc.left;
			pts[1].y = rc.bottom;
			pts[2].x = rc.right;
			pts[2].y = rc.bottom;
			pts[3].x = rc.right;
			pts[3].y = rc.top;
			pts[4].x = rc.right - dx2 + dx;
			pts[4].y = rc.top;
			pts[5].x = pt.x;
			pts[5].y = pt.y;
			pts[6].x = rc.left + dx2 - dx;
			pts[6].y = rc.top;
		}
	}
	else if (pt.x > rc.left + dx2)
	{
		// origin's x falls on the right half of the rectangle's width.
		if (pt.y > rc.top)
		{
			// point of origin is below the rectangle. a connector stretches downward from a middle section of the rectangle's bottom.
			pts[0].x = rc.left;
			pts[0].y = rc.top;
			pts[1].x = rc.left;
			pts[1].y = rc.bottom;
			pts[2].x = rc.left + dx2 + dx4 - dx;
			pts[2].y = rc.bottom;
			pts[3].x = pt.x;
			pts[3].y = pt.y;
			pts[4].x = rc.right - dx4 + dx;
			pts[4].y = rc.bottom;
			pts[5].x = rc.right;
			pts[5].y = rc.bottom;
			pts[6].x = rc.right;
			pts[6].y = rc.top;
		}
		else
		{
			// point of origin is above the rectangle's top. a connector stretches upward from a middle point of the rectangle's top.
			pts[0].x = rc.left;
			pts[0].y = rc.top;
			pts[1].x = rc.left;
			pts[1].y = rc.bottom;
			pts[2].x = rc.right;
			pts[2].y = rc.bottom;
			pts[3].x = rc.right;
			pts[3].y = rc.top;
			pts[4].x = rc.right - dx4 + dx;
			pts[4].y = rc.top;
			pts[5].x = pt.x;
			pts[5].y = pt.y;
			pts[6].x = rc.left + dx2 + dx4 - dx;
			pts[6].y = rc.top;
		}
	}
	else
	{
		// origin's x falls on the left half of the rectangle's width.
		if (pt.y > rc.top)
		{
			// point of origin is below the rectangle. a connector stretches downward from a middle section of the rectangle's bottom.
			pts[0].x = rc.left;
			pts[0].y = rc.top;
			pts[1].x = rc.left;
			pts[1].y = rc.bottom;
			pts[2].x = rc.left + dx4 - dx;
			pts[2].y = rc.bottom;
			pts[3].x = pt.x;
			pts[3].y = pt.y;
			pts[4].x = rc.right - dx2 - dx4 + dx;
			pts[4].y = rc.bottom;
			pts[5].x = rc.right;
			pts[5].y = rc.bottom;
			pts[6].x = rc.right;
			pts[6].y = rc.top;
		}
		else
		{
			// point of origin is above the rectangle's top. a connector stretches upward from a middle point of the rectangle's top.
			pts[0].x = rc.left;
			pts[0].y = rc.top;
			pts[1].x = rc.left;
			pts[1].y = rc.bottom;
			pts[2].x = rc.right;
			pts[2].y = rc.bottom;
			pts[3].x = rc.right;
			pts[3].y = rc.top;
			pts[4].x = rc.right - dx2 - dx4 + dx;
			pts[4].y = rc.top;
			pts[5].x = pt.x;
			pts[5].y = pt.y;
			pts[6].x = rc.left + dx4 - dx;
			pts[6].y = rc.top;
		}
	}
	DBGPRINTF_VERBOSE((L"_getBaloonLayout: 0=(%d:%d), 1=(%d:%d), 2=(%d:%d), 3=(%d:%d), 4=(%d:%d), 5=(%d:%d), 6=(%d:%d)\n", pts[0].x, pts[0].y, pts[1].x, pts[1].y, pts[2].x, pts[2].y, pts[3].x, pts[3].y, pts[4].x, pts[4].y, pts[5].x, pts[5].y, pts[6].x, pts[6].y));
}

HRGN AnnotationOperation::_createBaloonRgn(RECT annotRect, POINT originPt)
{
	POINT pts[7];
	_getBaloonLayout(annotRect, originPt, pts);
	return CreatePolygonRgn(pts, ARRAYSIZE(pts), ALTERNATE);
}

HRESULT AnnotationOperation::renderAnnotation(HDC hdc, RECT annotRect, POINT originPt, bool drawConnector, HRGN *hrgnOut)
{
	if (_coll->persistingAnnotation(_ai.CtrlId))
	{
		if (hrgnOut)
			*hrgnOut = NULL;
		return S_FALSE;
	}
	DBGPRINTF((L"renderAnnotation(%d): drawConnector=%X, originPt=(%d:%d), annotRect={%d,%d,%d,%d}\n", _ai.CtrlId, drawConnector, originPt.x, originPt.y, annotRect.left, annotRect.top, annotRect.right, annotRect.bottom));
	HRESULT hr = S_OK;
	bool drawGrippers = false;

	HRGN hrgn;
	if (drawConnector)
		hrgn = _createBaloonRgn(annotRect, originPt);
	else
		hrgn = CreateRectRgn(annotRect.left, annotRect.top, annotRect.right, annotRect.bottom);

	HBRUSH hbr1 = (HBRUSH)CreateSolidBrush(COLORREF_BALOON_FRAME);
	HBRUSH hbr2 = (HBRUSH)CreateSolidBrush(COLORREF_BALOON);
	HBRUSH hbr0 = (HBRUSH)SelectObject(hdc, hbr2);

	FillRgn(hdc, hrgn, hbr2);
	if (_ai.ThumbnailHandle)
	{
		SIZE imgSz = _getImageSize();
		HBITMAP hbmp = _ai.ThumbnailHandle;
		if (_coll->_vc->_highdefVP)
		{
			// if ImageSrcId is 0, that regards the master file. otherwise, it regards a pasted image. the image bitmap can be loaded from an ISRC*.bmp in the work folder. generate a pathname and add it to _highdefVP->ImageSources.
			if (_ai.ImageSrcId >= _coll->_vc->_highdefVP->ImageSources.size())
			{
				for (size_t i = _coll->_vc->_highdefVP->ImageSources.size(); i <= _ai.ImageSrcId; i++)
				{
					ustring *fname = new ustring;
					queryWorkPath(true, ANNOTATION_RESOURCE_IMAGE, LOWORD(i), *fname);
					_coll->_vc->_highdefVP->ImageSources.add(fname);
					DBGPRINTF((L"renderAnnotation: generated filename for ImgSrcId=%d\n", i));
				}
			}
			if (_ai.ImageSrcId < _coll->_vc->_highdefVP->ImageSources.size())
			{
				// load the source image from a file and scale it to fit the annotation frame.
				ustring *highdefImgName = _coll->_vc->_highdefVP->ImageSources[_ai.ImageSrcId];
				hr = LoadImageBmp(_coll->_vc->_hw, *highdefImgName, GetImageTypeFromExtension(*highdefImgName), &imgSz, _coll->_vc->_kill, &hbmp);
				if (FAILED(hr))
				{
					DBGPRINTF((L"renderAnnotation: LoadImageBmp failed with error 0x%X\n", hr));
					return hr;
				}
			}
			else
			{
				DBGPRINTF((L"renderAnnotation: ImageSrcId=%d is out of range of ImageSources \n", _ai.ImageSrcId));
			}
		}
		HDC hdc2 = CreateCompatibleDC(hdc);
		HBITMAP hbmp2 = (HBITMAP)SelectObject(hdc2, hbmp);
		DBGPRINTF_VERBOSE((L"BitBlt: hbmp=%p; %d:%d; %dx%d\n", hbmp2, annotRect.left + DRAWTEXT_MARGIN_CX, annotRect.bottom - imgSz.cy - DRAWTEXT_MARGIN_CY, imgSz.cx, imgSz.cy));
		if (!BitBlt(hdc, annotRect.left + DRAWTEXT_MARGIN_CX, annotRect.bottom - imgSz.cy - DRAWTEXT_MARGIN_CY, imgSz.cx, imgSz.cy, hdc2, 0, 0, SRCCOPY))
		{
			DWORD errorCode = ::GetLastError();
			DBGPRINTF((L"renderAnnotation: BitBlt failed; error %d; dest={%d, %d, %d, %d}, src={%d, %d}\n", errorCode, annotRect.left + DRAWTEXT_MARGIN_CX, annotRect.bottom - imgSz.cy - DRAWTEXT_MARGIN_CY, imgSz.cx, imgSz.cy, 0, 0));
			hr = E_FAIL;
		}
		SelectObject(hdc2, hbmp2);
		DeleteDC(hdc2);
		if (hbmp != _ai.ThumbnailHandle)
			DeleteObject(hbmp);
	}
	FrameRgn(hdc, hrgn, hbr1, _coll->_vc->_AnnotBorder, _coll->_vc->_AnnotBorder);

	int tlen = getTextLength();
	if (tlen)
	{
		HFONT hf0 = (HFONT)SelectObject(hdc, _coll->_vc->_hfontAnnotation);
		int mode0 = SetBkMode(hdc, TRANSPARENT);
		COLORREF cf0 = SetTextColor(hdc, _ai.ForeColor);
		RECT textRect = annotRect;
		InflateRect(&textRect, -2, -1);
		DrawText(hdc, getText(), tlen, &textRect, _ai.ThumbnailHandle? DT_WORD_ELLIPSIS : DT_WORD_ELLIPSIS| DT_CENTER);
		SetTextColor(hdc, cf0);
		SetBkMode(hdc, mode0);
		SelectObject(hdc, hf0);
	}

	if (drawGrippers)
	{
		RECT rc = { annotRect.left - 3,annotRect.top - 3,annotRect.left + 3,annotRect.top + 3 };
		FrameRect(hdc, &rc, hbr1);
		rc = { annotRect.left - 3,annotRect.bottom - 3,annotRect.left + 3,annotRect.bottom + 3 };
		FrameRect(hdc, &rc, hbr1);
		rc = { annotRect.right - 3,annotRect.bottom - 3,annotRect.right + 3,annotRect.bottom + 3 };
		FrameRect(hdc, &rc, hbr1);
		rc = { annotRect.right - 3,annotRect.top - 3,annotRect.right + 3,annotRect.top + 3 };
		FrameRect(hdc, &rc, hbr1);
	}

	SelectObject(hdc, hbr0);
	DeleteObject(hbr2);
	DeleteObject(hbr1);

	if (hrgnOut)
	{
		if (drawConnector)
		{
			*hrgnOut = hrgn;
			hrgn = NULL;
		}
		else
			*hrgnOut = _createBaloonRgn(annotRect, originPt);
	}
	if (hrgn)
		DeleteObject(hrgn);
	return hr;
}


void AnnotationOperation::renderConnectorArrow(HDC hdc, RECT annotRect, POINT originPt)
{
	HPEN hpen = CreatePen(PS_SOLID, 1, COLORREF_BALOON_ARROW);
	HPEN hpen0 = (HPEN)SelectObject(hdc, hpen);

	int drop = (annotRect.bottom - annotRect.top) / 2;
	
	POINT pt1, pt2;
	pt1 = originPt;
	pt2.x = annotRect.left;
	pt2.y = annotRect.top + drop;

	long theta;
	int dx = pt2.x - pt1.x;
	int dy = pt2.y - pt1.y;
	if (dx == 0)
		theta = 90;
	else if (dy == 0)
		theta = 0;
	else
		theta = lrint(180.0*atan((double)dy / (double)dx) / M_PI);

	long bladeLength = _coll->_vc->_vi.ColWidth;

	POINT arrowhead[3];
	arrowhead[0] = pt1;
	FLTRIG flt1(pt1, theta+15);
	arrowhead[1] = flt1.transformX(bladeLength);
	FLTRIG flt2(pt1, theta - 15);
	arrowhead[2] = flt2.transformX(bladeLength);

	MoveToEx(hdc, pt2.x, pt2.y, NULL);
	LineTo(hdc, pt1.x, pt1.y);

	HBRUSH hbr2 = CreateSolidBrush(COLORREF_BALOON_ARROW);
	HPEN hpen2 = CreatePen(PS_SOLID, 0, COLORREF_BALOON_ARROW);
	SelectObject(hdc, hbr2);
	SelectObject(hdc, hpen2);
	Polygon(hdc, arrowhead, 3);
	DeleteObject(hpen2);
	DeleteObject(hbr2);

	if (hpen0)
		SelectObject(hdc, hpen0);
	if (hpen)
		DeleteObject(hpen);
}

////////////////////////////////////////////////////
void AnnotationCollection::reinit(BinhexView *vc2, UINT annotFlags)
{
	_vc = vc2;

	if (annotFlags)
	{
		for (size_t i = 0; i < size(); i++)
		{
			AnnotationOperation ao(at(i), this);
			ao._ai.Flags |= annotFlags;
			ao.resetDimensions();
		}
	}
}

/* WARNING: this method is typically called from a scan worker thread. in that case a mutex puts the main thread in a wait state.
*/
DUMPANNOTATIONINFO *AnnotationCollection::addAnnotation(LARGE_INTEGER contentPos, bool startEditing)
{
	if (contentPos.QuadPart == -1LL)
		return NULL;
	DUMPANNOTATIONINFO ai = { 0 };
	AnnotationOperation ao(ai, this);
	if (!ao.initUI(contentPos, (UINT)size(), startEditing))
		return NULL;
	if (startEditing && ao._ai.EditCtrl)
	{
		SetWindowSubclass(ao._ai.EditCtrl, AnnotationSubclassProc, ao._ai.CtrlId, (DWORD_PTR)(LPVOID)this);
		SetFocus(ao._ai.EditCtrl);
	}
	size_t n = add(ai);
	return &at(n-1);
}

/* WARNING: this method is typically called from a scan worker thread. in that case a mutex puts the main thread in a wait state.
regionIndex - 1-based index to an item on list _regions.
*/
DUMPANNOTATIONINFO *AnnotationCollection::annotateRegion(class CRegionCollection *regions, int regionIndex, LPCWSTR annotText)
{
	ASSERT(0 < regionIndex && regionIndex <= (int)regions->size());
	DUMPREGIONINFO &ri = (*regions)[regionIndex - 1];
	DUMPANNOTATIONINFO *annot = addAnnotation(ri.DataOffset, false);
	if (!annot)
		return 0;
	AnnotationOperation(*annot, this).setText(annotText);
	annot->RegionId = regionIndex; // assign a one-based index of the region.
	ri.AnnotCtrlId = annot->CtrlId;
	DBGPRINTF_VERBOSE((L"annotateRegion: Annotaton.CtrlId=%d; '%s'\n", annot->CtrlId, annot->Text));
	return annot;
}

#define ANNOTATION_COLUMN_VERT_SPACING 4
#define ANNOTATION_COLUMN_HORZ_MARGIN 4

SIZE AnnotationCollection::getAnnotationsPane(class CRegionCollection *regions, simpleList<DUMPANNOTATIONINFO2> *outList)
{
	SIZE sz = { 0 };
	for (size_t i = 0; i < size(); i++)
	{
		DUMPANNOTATIONINFO &annot = at(i);
		if (annot.RegionId == 0)
		{
			// this annotation does not have a region association.
			if (_vc->_fri.DataOffset.QuadPart <= annot.DataOffset.QuadPart && annot.DataOffset.QuadPart < _vc->_fri.DataOffset.QuadPart + _vc->_fri.DataLength)
			{
				if (outList)
				{
					DUMPANNOTATIONINFO2 annot2;
					CopyMemory(&annot2, &annot, sizeof(DUMPANNOTATIONINFO));
					annot2.DataOffset = annot.DataOffset;
					outList->add(annot2);
				}
			}
			else
				continue;
		}
		else
		{
			DUMPREGIONINFO &ri = (*regions)[annot.RegionId - 1];
			if (_vc->_fri.DataOffset.QuadPart <= ri.DataOffset.QuadPart && ri.DataOffset.QuadPart < _vc->_fri.DataOffset.QuadPart + _vc->_fri.DataLength)
			{
				if (outList)
				{
					DUMPANNOTATIONINFO2 annot2;
					CopyMemory(&annot2, &annot, sizeof(DUMPANNOTATIONINFO));
					annot2.DataOffset = ri.DataOffset;
					outList->add(annot2);
				}
			}
			else
				continue;
		}
		sz.cy += annot.FrameSize.cy + ANNOTATION_COLUMN_VERT_SPACING;
		if (annot.FrameSize.cx > sz.cx)
			sz.cx = annot.FrameSize.cx;
	}
	sz.cx += 3 * ANNOTATION_COLUMN_HORZ_MARGIN;
	sz.cy += DRAWTEXT_MARGIN_CY;
	return sz;
}

void AnnotationCollection::redrawAll(HDC hdc, class CRegionCollection *regions)
{
	// draw only those annotations that belong to current view.
	simpleList<DUMPANNOTATIONINFO2> annots2;
	SIZE pane = getAnnotationsPane(regions, &annots2);

	// sort the collected annotations by the associated region's file offset.
	qsort(annots2.data(), annots2.size(), sizeof(DUMPANNOTATIONINFO2), DUMPANNOTATIONINFO2::compare);

	int row, col, row1, row2, col1, col2;
	POINT ptOrig, ptLT;
	int y0 = (_vc->_vi.FrameSize.cy - pane.cy) / 2;
	int y = y0;
	for (size_t i = 0; i < annots2.size(); i++)
	{
		DUMPANNOTATIONINFO2 &annot2 = annots2[i];
		AnnotationOperation ao(annot2, this);
		if (annot2.RegionId == 0)
		{
			// this annotation does not have a region association.
			row1 = (int)((annot2.DataOffset.QuadPart - _vc->_fri.DataOffset.QuadPart) / _vc->_vi.BytesPerRow);
			row2 = (int)((annot2.DataOffset.QuadPart + 1 - _vc->_fri.DataOffset.QuadPart) / _vc->_vi.BytesPerRow);

			col1 = (int)((annot2.DataOffset.QuadPart - _vc->_fri.DataOffset.QuadPart) % _vc->_vi.BytesPerRow);
			col2 = (int)((annot2.DataOffset.QuadPart + 1 - _vc->_fri.DataOffset.QuadPart) % _vc->_vi.BytesPerRow);
		}
		else
		{
			DUMPREGIONINFO &ri = (*regions)[annot2.RegionId - 1];

			row1 = (int)((ri.DataOffset.QuadPart - _vc->_fri.DataOffset.QuadPart) / _vc->_vi.BytesPerRow);
			row2 = (int)((ri.DataOffset.QuadPart + ri.DataLength - _vc->_fri.DataOffset.QuadPart) / _vc->_vi.BytesPerRow);

			col1 = (int)((ri.DataOffset.QuadPart - _vc->_fri.DataOffset.QuadPart) % _vc->_vi.BytesPerRow);
			col2 = (int)((ri.DataOffset.QuadPart + ri.DataLength - _vc->_fri.DataOffset.QuadPart) % _vc->_vi.BytesPerRow);
		}
		if (row1 < 0)
			row1 = 0;
		if (row2 >= (int)_vc->_vi.RowsPerPage)
			row2 = (int)_vc->_vi.RowsPerPage;
		row = row1 + (row2 - row1) / 2;
		if (row == row1)
		{
			if (row1 == row2)
				col = col2;
			else
				col = _vc->_vi.BytesPerRow;
		}
		else if (row == row2)
			col = col2;
		else
			col = _vc->_vi.BytesPerRow;

		ptOrig.y = row * _vc->_vi.RowHeight + _vc->_vi.RowHeight/2;
		/*
		ptOrig.x = _vc->_vi.OffsetColWidth + 3*col*_vc->_vi.ColWidth - _vc->_vi.ColWidth;
		*/
		ptOrig.x = _vc->_getHexColCoord(col, -1);

		ptLT.y = y;
		ptLT.x = _vc->_vi.FrameSize.cx + _vc->_vi.ColWidth;

		ao.drawAnnotationAt(hdc, ptOrig, ptLT);

		y += annot2.FrameSize.cy + ANNOTATION_COLUMN_VERT_SPACING;
	}
}

bool AnnotationCollection::editInProgress()
{
	for (size_t i = 0; i < size(); i++)
	{
		if (at(i).Flags & DUMPANNOTATION_FLAG_EDITING)
			return true;
	}
	return false;
}

void AnnotationCollection::redrawAll(HDC hdc)
{
	if (editInProgress())
		return;
	persistingAnnotation();
	if (_vc->_highdefVP)
		queryImageSrcCount();
	for (size_t i = size(); i > 0; i--)
	{
		AnnotationOperation ao(at(i-1), this);
		if (_persistCtrlId && _persistCtrlId != ao._ai.CtrlId)
			continue;
		if (ao._ai.Flags & DUMPANNOTATION_FLAG_UIDEFERRED)
		{
			if (ao.reinitUI() && ao._ai.EditCtrl)
				SetWindowSubclass(ao._ai.EditCtrl, AnnotationSubclassProc, ao._ai.CtrlId, (DWORD_PTR)(LPVOID)this);
		}
		if (FAILED(ao.drawAnnotation(hdc, _fadeoutDelayStart != 0)))
			return;
	}
}

bool AnnotationCollection::startFadeoutDelay()
{
	if (_n == 0)
		return false; // no annotations to process.
	_fadeoutDelayStart = ::GetTickCount();
	::SetTimer(_vc->_hw, TIMERID_DELAYTRANSPARENTANNOTATION, TIMERMSEC_DELAYTRANSPARENTANNOTATION, NULL);
	return true;
}

void AnnotationCollection::onFadeoutDelayTimer()
{
	if (persistingAnnotation())
	{
		::KillTimer(_vc->_hw, TIMERID_DELAYTRANSPARENTANNOTATION);
		_fadeoutDelayStart = 0;
		_vc->_annotFadeoutOpacity = 0;
		return;
	}
	LONG dt = (LONG)(::GetTickCount() - _fadeoutDelayStart);
	if (dt < ANNOTATION_BEFORE_FADEOUT)
	{
		_vc->_annotFadeoutOpacity = 0;
		return;
	}
	if (dt < ANNOTATION_AFTER_FADEOUT)
	{
		if (_vc->_annotFadeoutOpacity == 0)
			_vc->_annotFadeoutOpacity = BHV_FULL_OPACITY+1;
		_vc->_annotFadeoutOpacity -= (BHV_FULL_OPACITY+1 - BHV_NORMAL_OPACITY) / ((ANNOTATION_AFTER_FADEOUT - ANNOTATION_BEFORE_FADEOUT) / TIMERMSEC_DELAYTRANSPARENTANNOTATION);
	}
	else
	{
		::KillTimer(_vc->_hw, TIMERID_DELAYTRANSPARENTANNOTATION);
		_fadeoutDelayStart = 0;
		_vc->_annotFadeoutOpacity = 0;
	}
	// execute a repaint in delayed fashion. don't draw a borderline. a borderline has already been drawn since BinhexDumpWnd::Create specified WS_BORDER.
	_vc->repaint2(BINHEXVIEW_REPAINT_NO_FRAME);
}

int AnnotationCollection::queryTextSrcCount()
{
	if (_textSrcCount == 0)
	{
		ustring rname = getRegistryKey();
		Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY L"\\TextSource", rname, (DWORD*)&_textSrcCount);
	}
	return _textSrcCount;
}

int AnnotationCollection::queryImageSrcCount()
{
	if (_imageSrcCount == 0)
	{
		ustring rname = getRegistryKey();
		Registry_GetDwordValue(HKEY_CURRENT_USER, LIBREGKEY L"\\ImageSource", rname, (DWORD*)&_imageSrcCount);
	}
	return _imageSrcCount;
}

HRESULT AnnotationCollection::setPicture(DUMPANNOTATIONINFO *ai, IPicture *ipic)
{
	HBITMAP hbmp = NULL;
	HRESULT hr = ipic->get_Handle((OLE_HANDLE*)&hbmp);
	if (hr == S_OK)
		hr = setPicture(ai, hbmp);
	return hr;
}

HRESULT AnnotationCollection::setPicture(DUMPANNOTATIONINFO *ai, HBITMAP hbmp)
{
	queryImageSrcCount();
	bool replacing = ai->ThumbnailHandle ? true : false;
	USHORT srcId = replacing ? ai->ImageSrcId : LOWORD(_imageSrcCount + 1);
	AnnotationOperation ao(*ai, this);
	if (0 == ao.saveSourceImage(hbmp, srcId, &_imageSrcCount))
		return E_FAIL;
	ao.attachThumbnail(_FitBitmapToWindowFrame(hbmp, _vc->_hw, { DUMPANNOTTHUMBNAIL_WIDTH,DUMPANNOTTHUMBNAIL_HEIGHT }, NULL, FBTWF_FLAG_KEEP_SIZE_IF_SMALLER), srcId);
	ao.finalizeUIText();
	DBGPRINTF((L"setPicture: clearing recentCtrlId=%d\n", _recentCtrlId));
	_recentCtrlId = 0;
	/* the caller needs to do the IDM_SAVEMETA post itself.
	PostMessage(_vc->_hw, WM_COMMAND, IDM_SAVEMETA, MAKELPARAM(ai->CtrlId, 0));
	*/
	return S_OK;
}

/* https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclipboarddata

The clipboard controls the handle that the GetClipboardData function returns, not the application. The application should copy the data immediately. The application must not free the handle nor leave it locked. The application must not use the handle after the EmptyClipboard or CloseClipboard function is called, or after the SetClipboardData function is called with the same clipboard format.
*/
bool AnnotationCollection::insertFromClipboard(HWND hwnd)
{
	if (!IsClipboardFormatAvailable(CF_BITMAP))
		return false; // image not available
	// find an annotation window the input hwnd belongs to.
	for (size_t i = 0; i < size(); i++)
	{
		DUMPANNOTATIONINFO &ai = at(i);
		if (hwnd == ai.EditCtrl)
		{
			DBGPRINTF((L"insertFromClipboard: '%s' (%dB)\n", ai.Text, ai.TextLength));
			if (!OpenClipboard(_vc->_hw))
				return false;
			bool ret = false;
			if (IsClipboardFormatAvailable(CF_BITMAP))
			{
				queryImageSrcCount();
				bool replacing = ai.ThumbnailHandle ? true : false;
				USHORT srcId = replacing? ai.ImageSrcId : LOWORD(_imageSrcCount+1);
				AnnotationOperation ao(ai, this);
				HBITMAP hbmp = (HBITMAP)GetClipboardData(CF_BITMAP);
				if (0 != ao.saveSourceImage(hbmp, srcId, &_imageSrcCount))
				{
					ao.attachThumbnail(_FitBitmapToWindowFrame(hbmp, _vc->_hw, { DUMPANNOTTHUMBNAIL_WIDTH,DUMPANNOTTHUMBNAIL_HEIGHT }, NULL, FBTWF_FLAG_KEEP_SIZE_IF_SMALLER), srcId);
					ao.finalizeUIText();
					DBGPRINTF((L"insertFromClipboard: clearing recentCtrlId=%d\n", _recentCtrlId));
					_recentCtrlId = 0;
					PostMessage(_vc->_hw, WM_COMMAND, IDM_SAVEMETA, MAKELPARAM(i,0));
					ret = true;
				}
			}
			/* don't handle the text format. let the edit control respond.
			else if (IsClipboardFormatAvailable(CF_UNICODETEXT))
			{
				HGLOBAL hg = (HGLOBAL)GetClipboardData(CF_BITMAP);
				LPCWSTR src = (LPCWSTR)GlobalLock(hg);
				AnnotationOperation(ai, _vc).updateUIText(src);
				GlobalUnlock(hg);
			}
			*/
			CloseClipboard();
			return ret;
		}
	}
	return false;
}

void AnnotationCollection::abortAnnotationEdit()
{
	for (size_t i = 0; i < size(); i++)
	{
		AnnotationOperation(at(i), this).finalizeUIText(true);
	}
}

void AnnotationCollection::OnAnnotationEnChange(UINT ctrlId)
{
	for (size_t i = 0; i < size(); i++)
	{
		DUMPANNOTATIONINFO &ai = at(i);
		if (ai.CtrlId == ctrlId && ai.EditCtrl)
		{
#ifdef _DEBUG
			ustring prevtext(ai.Text, ai.TextLength);
			SIZE prevsize = ai.FrameSize;
#endif//_DEBUG
			AnnotationOperation(ai, this).updateUIText();
		}
	}
}

int AnnotationCollection::finalizeAnnotationText(HWND hwnd, bool aborted)
{
	if (!hwnd)
		return -1;
	int c = 0;
	for (size_t i = 0; i < size(); i++)
	{
		if (at(i).EditCtrl == hwnd)
		{
			AnnotationOperation(at(i), this).finalizeUIText(aborted);
			c++;
		}
	}
	return c;
}

bool AnnotationCollection::OnAnnotationKeyDown(HWND hwnd, UINT keycode)
{
	if (keycode == VK_RETURN || keycode == VK_ESCAPE)
	{
		if (finalizeAnnotationText(hwnd) > 0)
			return true;
	}
	else if (keycode == VK_INSERT)
	{
		if (GetKeyState(VK_SHIFT) < 0)
		{
			if (insertFromClipboard(hwnd))
				return true;
		}
	}
	return false;
}

void AnnotationCollection::OnAnnotationKillFocus(HWND hwndLosing, HWND hwndGaining)
{
	DBGPRINTF((L"OnAnnotationKillFocus: %p --> %p\n", hwndLosing, hwndGaining));
	finalizeAnnotationText(hwndLosing);
}

int AnnotationCollection::queryByCtrlId(UINT ctrlId)
{
	for (int i = 0; i < (int)size(); i++)
	{
		if (at(i).CtrlId == ctrlId)
		{
			return i;
		}
	}
	return -1;
}

void AnnotationCollection::OnFontChange()
{
	for (size_t i = 0; i < size(); i++)
	{
		AnnotationOperation(at(i), this).resetDimensions();
	}
}

void AnnotationCollection::beforeLoad(bool temp, int size)
{
	if (temp)
		return;
	// prepare the destination folder at %TMP%\HexDumpTab\res<MetafileId>.
	ustring tmpDir = getWorkDirectory(true);
	// first clean it. get rid of temp files left over from prior session.
	if (ERROR_PATH_NOT_FOUND == CleanDirectory(tmpDir, false))
		CreateDirectory(tmpDir, NULL);

	ustring workDir = getWorkDirectory(false);
	CopyFolderContent(workDir, tmpDir);

	//_vc->lockObjAccess();
}

void AnnotationCollection::load(DUMPANNOTATIONINFO &ai)
{
	if (ai.ThumbnailId)
		AnnotationOperation(ai, this).loadThumbnail();
	ai.EditCtrl = NULL;
	ai.Flags &= DUMPANNOTATION_FLAG_THUMBNAIL_SAVED;
	ai.Flags |= DUMPANNOTATION_FLAG_UIDEFERRED;
	add(ai);
}

void AnnotationCollection::afterLoad(HRESULT loadResult)
{
	//_vc->unlockObjAccess();
}

void AnnotationCollection::beforeSave(simpleList<HBITMAP> &list, bool appTerminating)
{
	_vc->lockObjAccess();
	for (size_t i = 0; i < size(); i++)
	{
		list.add(AnnotationOperation(at(i), this).saveThumbnail(appTerminating));
	}
}

void AnnotationCollection::afterSave(simpleList<HBITMAP> &list, bool temp)
{
	for (size_t i = 0; i < size(); i++)
	{
		at(i).ThumbnailHandle = list[i];
		if (!temp)
			AnnotationOperation(at(i), this).persistResources();
	}
	_vc->unlockObjAccess();
}

// cleanup on cancelation by user.
void AnnotationCollection::cleanUp()
{
	for (size_t i = 0; i < size(); i++)
	{
		AnnotationOperation(at(i), this).cleanUp();
	}
	ustring resDir = getWorkDirectory(true);
	CleanDirectory(resDir, true);
}

int AnnotationCollection::querySize(bool limitToIndies)
{
	int n = 0;
	if (limitToIndies)
	{
		// collect only those independent of region association
		for (size_t i = 0; i < size(); i++)
		{
			if (at(i).RegionId == 0)
				n++;
		}
	}
	else
	{
		// count only those dependent on a region.
		for (size_t i = 0; i < size(); i++)
		{
			if (at(i).RegionId != 0)
				n++;
		}
	}
	return n;
}

int AnnotationCollection::queryNextItem(LARGE_INTEGER contentPos, ULARGE_INTEGER fileSize, bool goBack, bool limitToIndies)
{
	DUMPANNOTATIONINFO *item;
	LARGE_INTEGER offset0 = contentPos;
	if (0 <= _itemVisited && _itemVisited < (int)size())
	{
		item = &at(_itemVisited);
		offset0 = item->DataOffset;
	}
	LONGLONG delta0 = fileSize.QuadPart, delta;
	int i0 = -1;
	int i = 0;
	for (; i < (int)size(); i++)
	{
		if (i == _itemVisited)
			continue;
		item = &at(i);
		if (item->DataOffset.QuadPart == offset0.QuadPart)
			continue;
		if (goBack)
			delta = offset0.QuadPart - item->DataOffset.QuadPart;
		else
			delta = item->DataOffset.QuadPart - offset0.QuadPart;
		if (delta < 0)
			delta += fileSize.QuadPart;
		if (delta < delta0)
		{
			delta0 = delta;
			i0 = i;
		}
	}
	return i0;
}

DUMPANNOTATIONINFO *AnnotationCollection::gotoItem(int itemIndex)
{
	_itemVisited = itemIndex;
	if (itemIndex != -1)
	{
		return &at(itemIndex);
	}
	return NULL;
}

int AnnotationCollection::queryAnnotationAt(POINT pt, bool screenCoords, bool excludeIfReadonly)
{
	if (screenCoords)
		ScreenToClient(_vc->_hw, &pt);

	_annotationHere = -1;
	_dnd.Gripping = _dnd.Gripped = false;
	for (int i = 0; i < (int)size(); i++)
	{
		DUMPANNOTATIONINFO &ai = at(i);
		if (excludeIfReadonly)
		{
			if (ai.Flags & DUMPANNOTATION_FLAG_READONLY)
				continue;
		}
		bool gripping;
		if (AnnotationOperation(ai, this).containsPt(pt, &gripping))
		{
			_annotationHere = i;
			if (gripping && (ai.Flags & DUMPANNOTATION_FLAG_EDITING))
				gripping = false;
			_dnd.Gripping = gripping;
			DBGPRINTF_VERBOSE((L"queryAnnotationAt: index=%d, gripping=%d\n", i, _dnd.Gripping));
			break;
		}
	}
	return _annotationHere;
}

HCURSOR AnnotationCollection::queryCursor()
{
	if (_annotationHere != -1)
	{
		// show a Move cursor for grabbing the edge.
		if (_dnd.Gripping)
			return LoadCursor(NULL, IDC_SIZEALL);
		// show an I beam for the text editable area, if it's not readonly.
		if (!(at(_annotationHere).Flags & DUMPANNOTATION_FLAG_READONLY))
			return LoadCursor(NULL, IDC_IBEAM);
	}
	if (_recentCtrlId)
	{
		for (int i = 0; i < (int)size(); i++)
		{
			AnnotationOperation ao(at(i), this);
			if (ao._ai.CtrlId == _recentCtrlId)
			{
				if (!(ao._ai.Flags & DUMPANNOTATION_FLAG_EDITING))
				{
					POINT pt;
					if (GetCursorPos(&pt))
					{
						::ScreenToClient(_vc->_hw, &pt);
						DBGPRINTF((L"queryCursor: recentCtrlId=%d; pos=(%d:%d)\n", _recentCtrlId, pt.x, pt.y));
						bool grip;
						if (ao.containsPt(pt, &grip))
						{
							DBGPRINTF((L"queryCursor: containsPt.grip=%d\n", grip));
							if (grip)
								return LoadCursor(NULL, IDC_SIZEALL);
						}
					}
				}
				return NULL;
			}
		}
	}
	return NULL;
}

bool AnnotationCollection::highlightAnnotationByCtrlId(UINT ctrlId)
{
	if (!ctrlId)
	{
		if (_persistCtrlId)
			ctrlId = _persistCtrlId;
		else
			return false;
	}
	int index = queryByCtrlId(ctrlId);
	if (index == -1)
		return false;
	if (persistingAnnotation(ctrlId))
		return false;
	AnnotationOperation ao(at(index), this);
	if (ao.highlightAnnotation(_hitRect))
	{
		_recentCtrlId = ao._ai.CtrlId; // save the just repainted annotation's id to stop screen flickers.
		DBGPRINTF((L"highlightAnnotationByCtrlId: recentCtrlId=%d; _persistCtrlId=%d\n", _recentCtrlId, _persistCtrlId));
		return true;
	}
	return false;
}

bool AnnotationCollection::persistingAnnotation(int ctrlId)
{
	if (_persistStop)
	{
		time_t t2;
		time(&t2);
		int dt = (int)(_persistStop - t2);
		DBGPRINTF((L"persistingAnnotation(CtrlId=%d): persistCtrlId=%d, secondsLeft=%d\n", ctrlId, _persistCtrlId, dt));
		int pcId = _persistCtrlId;
		if (dt > 0)
		{
			if (ctrlId && pcId == ctrlId)
				return false;
			return true;
		}
		stopPersistentAnnotation();
	}
	return false;
}

void AnnotationCollection::startPersistentAnnotation(UINT ctrlId, int persistSeconds)
{
	DBGPRINTF((L"startPersistentAnnotation(CtrlId=%d)\n", ctrlId));
	time(&_persistStop);
	_persistStop += persistSeconds;
	_persistCtrlId = ctrlId;
}

void AnnotationCollection::stopPersistentAnnotation()
{
	DBGPRINTF((L"stopPersistentAnnotation(CtrlId=%d)\n", _persistCtrlId));
	_persistCtrlId = 0;
	_persistStop = 0;
}

bool AnnotationCollection::highlightAnnotation(POINT pt, bool screenCoords)
{
	int objId = queryAnnotationAt(pt, screenCoords);
	if (objId < 0)
		return false;
	if (persistingAnnotation(objId))
	{
		if(_persistCtrlId == 0)
			return false;
		objId = _persistCtrlId - IDC_ANNOTATION_EDITCTRL0;
	}
	// if the annotation with an image attachment has recently been repainted, do not bother repainting it again. doing so can cause the screen to flicker.
	AnnotationOperation ao(at(objId), this);
	if (ao._ai.CtrlId == _recentCtrlId && ao._ai.ThumbnailHandle)
	{
		DBGPRINTF((L"highlightAnnotation: recentCtrlId=%d (ThumbnailHandle=%p)\n", _recentCtrlId, ao._ai.ThumbnailHandle));
		return true;
	}
	bool ret = ao.highlightAnnotation(_hitRect);
	if (ret)
	{
		_recentCtrlId = ao._ai.CtrlId; // save the just repainted annotation's id to stop screen flickers.
		DBGPRINTF((L"highlightAnnotation: recentCtrlId=%d\n", _recentCtrlId));
	}
	return ret;
}

bool AnnotationCollection::clearHighlight()
{
	if (IsRectEmpty(&_hitRect))
		return false;
	if (_recentCtrlId != -1 && _persistCtrlId == _recentCtrlId)
		return false;
	InflateRect(&_hitRect, POSTMOUSEFLYOVER_RESTORE_MARGIN, POSTMOUSEFLYOVER_RESTORE_MARGIN);
	_vc->invalidate(&_hitRect);
	SetRectEmpty(&_hitRect);
	DBGPRINTF((L"clearHighlight: recentCtrlId=%d\n", _recentCtrlId));
	_recentCtrlId = 0;
	return true;
}

void AnnotationCollection::clearDragdropInfo()
{
	if (_dnd.PreReplaceImage)
		DeleteObject(_dnd.PreReplaceImage);
	if (_dnd.TransitImage)
		DeleteObject(_dnd.TransitImage);
	ZeroMemory(&_dnd, sizeof(_dnd));
}

void AnnotationCollection::clearItem(DUMPANNOTATIONINFO *pitem)
{
	if (!pitem)
		return;
	if (pitem->Flags & DUMPANNOTATION_FLAG_DONTDELETEIMAGE)
		return;
	HBITMAP h = (HBITMAP)InterlockedExchangePointer((LPVOID*)&pitem->ThumbnailHandle, NULL);
	if (h)
	{
		DBGPRINTF((L"AnnotationCollection::clearItem: ThumbnailHandle=%X\n", h));
		DeleteObject(h);
	}
}

bool AnnotationCollection::selectAt(POINT pt)
{
	ASSERT(_dnd.TransitImage == NULL);
	_annotationHere = queryAnnotationAt(pt);
	if (_annotationHere < 0)
		return false;
	if (_dnd.Gripping)
		return startDrag(pt);
	// stop an ongoing fade-out operation. if left on, the fadeout would hide the annotation.
	::KillTimer(_vc->_hw, TIMERID_DELAYTRANSPARENTANNOTATION);
	// put the current annotation in edit mode and move the edit caret to pt.
	return AnnotationOperation(at(_annotationHere), this).onSelect(pt);
}

bool AnnotationCollection::reselectHere()
{
	if (_annotationHere < 0)
		return false;
	AnnotationOperation(at(_annotationHere), this).reselect();
	return false;
}

bool AnnotationCollection::startDrag(POINT pt)
{
	DBGPRINTF((L"AnnotationCollection::startDrag(%d)\n", _annotationHere));
	_dnd.TransitImage = AnnotationOperation(at(_annotationHere), this).renderToBitmap(&_dnd.ImageRect);
	_dnd.PreReplaceRect = _dnd.ImageRect;
	_dnd.Gripping = false;
	_dnd.Gripped = true;
	_dnd.StartPt = pt;
	_dnd.StartFrameOffset = at(_annotationHere).FrameOffset;
	return true;
}

HBITMAP AnnotationOperation::renderToBitmap(RECT *outRect)
{
	POINT originPt;
	RECT annotRect;
	if (!annotationInFrame(annotRect, &originPt))
		return NULL;
	// snap the annotation in a bitmap. continueDrag will use it to indicate to user where the annotation has been dragged to.
	HDC hdc = _coll->_vc->setVC();
	HDC hdc1 = CreateCompatibleDC(hdc);
	HDC hdc2 = CreateCompatibleDC(hdc);
	HBITMAP hbmp1 = CreateCompatibleBitmap(hdc, _coll->_vc->_vi.FrameSize.cx, _coll->_vc->_vi.FrameSize.cy);
	HBITMAP hbmp01 = (HBITMAP)SelectObject(hdc1, hbmp1);
	RECT frameRect = { 0,0,_coll->_vc->_vi.FrameSize.cx, _coll->_vc->_vi.FrameSize.cy };
	FillRect(hdc1, &frameRect, GetStockBrush(WHITE_BRUSH));
	renderAnnotation(hdc1, annotRect, originPt, false);
	SIZE imageSize = { annotRect.right - annotRect.left, annotRect.bottom - annotRect.top };
	HBITMAP hbmp2 = CreateCompatibleBitmap(hdc, imageSize.cx, imageSize.cy);
	HBITMAP hbmp02 = (HBITMAP)SelectObject(hdc2, hbmp2);
	BitBlt(hdc2, 0, 0, imageSize.cx, imageSize.cy, hdc1, annotRect.left, annotRect.top, SRCCOPY);
	SelectObject(hdc1, hbmp01);
	SelectObject(hdc2, hbmp02);
	DeleteDC(hdc1);
	DeleteDC(hdc2);
	_coll->_vc->releaseVC();
	DeleteObject(hbmp1);
	if (outRect)
		*outRect = annotRect;
	return hbmp2;
}

bool AnnotationCollection::continueDrag(POINT newPt)
{
	if (!_dnd.Gripped)
		return false;

	POINT movement = { newPt.x - _dnd.StartPt.x,newPt.y - _dnd.StartPt.y };
	DBGPRINTF((L"AnnotationCollection::continueDrag(%d)\n", _annotationHere));

	HDC hdc = _vc->setVC();
	HDC hdc1;
	HBITMAP hbmp01;

	if (_dnd.PreReplaceImage)
	{
		hdc1 = CreateCompatibleDC(hdc);
		hbmp01 = (HBITMAP)SelectObject(hdc1, _dnd.PreReplaceImage);
		BitBlt(hdc, _dnd.PreReplaceRect.left, _dnd.PreReplaceRect.top, _dnd.PreReplaceRect.right - _dnd.PreReplaceRect.left, _dnd.PreReplaceRect.bottom - _dnd.PreReplaceRect.top, hdc1, 0, 0, SRCCOPY);
		SelectObject(hdc1, hbmp01);
		DeleteDC(hdc1);
		DeleteObject(_dnd.PreReplaceImage);
	}
	RECT rc2 = _dnd.ImageRect;
	OffsetRect(&rc2, movement.x, movement.y);
	int cx = rc2.right - rc2.left;
	int cy = rc2.bottom - rc2.top;

	hdc1 = CreateCompatibleDC(hdc);
	BITMAPINFO bmi = { sizeof(BITMAPINFOHEADER), cx, cy, 1, 32, BI_RGB, cx*cy * sizeof(COLORREF) };
	UINT32* bits;
	HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, 0);
	hbmp01 = (HBITMAP)SelectObject(hdc1, hbmp);
	BitBlt(hdc1, 0, 0, cx, cy, hdc, rc2.left, rc2.top, SRCCOPY);
	/*#ifdef _DEBUG
		BitBlt(hdc, 0, 0, cx, cy, hdc1, 0, 0, SRCCOPY);
	#endif*/
	SelectObject(hdc1, hbmp01);
	DeleteDC(hdc1);
	_dnd.PreReplaceImage = hbmp;
	_dnd.PreReplaceRect = rc2;

	HDC hdc2 = CreateCompatibleDC(hdc);
	HBITMAP hbmp02 = (HBITMAP)SelectObject(hdc2, _dnd.TransitImage);
	BLENDFUNCTION bf = { AC_SRC_OVER, 0, BHV_CONTINUEDRAG_ANNOT_OPACITY, 0 };
	AlphaBlend(hdc, rc2.left, rc2.top, cx, cy, hdc2, 0, 0, cx, cy, bf);
#ifdef ANNOTOP_CLIPS_REGION
	SelectClipRgn(hdc, _vc->_hrgnClip);
#endif//#ifdef ANNOTOP_CLIPS_REGION
	SelectObject(hdc2, hbmp02);
	DeleteDC(hdc2);
	_vc->releaseVC();
	return true;
}

bool AnnotationCollection::stopDrag(POINT newPt)
{
	if (!_dnd.Gripped)
		return false;
	DBGPRINTF((L"AnnotationCollection::stopDrag(%d)\n", _annotationHere));
	DUMPANNOTATIONINFO &ai = at(_annotationHere);
	POINT movement = { newPt.x - _dnd.StartPt.x,newPt.y - _dnd.StartPt.y };
	ai.FrameOffset.x += movement.x;
	ai.FrameOffset.y += movement.y;
	_vc->invalidate();
	clearDragdropInfo();
	return true;
}

bool AnnotationCollection::removeHere()
{
	if (_annotationHere < 0)
		return false;
	HRESULT hr = AnnotationOperation(at(_annotationHere), this).beforeRemove();
	if (FAILED(hr))
		return true; // true for having processed the request; but the object is readonly, and has not been removed.
	if (_imageSrcCount == 0)
		queryImageSrcCount();
	if (_imageSrcCount > 0 && hr == S_FALSE)
	{
		ustring rname = getRegistryKey();
		if (--_imageSrcCount == 0)
		{
			// delete the image count from the registry.
			Registry_DeleteValue(HKEY_CURRENT_USER, LIBREGKEY L"\\ImageSource", rname);
			ustring dir = getWorkDirectory(true);
			RemoveDirectory(dir);
		}
		else
		{
			// update the item count in the registry.
			Registry_SetDwordValue(HKEY_CURRENT_USER, LIBREGKEY L"\\ImageSource", rname, _imageSrcCount);
		}
		PostMessage(_vc->_hw, WM_COMMAND, IDM_SAVEMETA, MAKELPARAM(_annotationHere,1));
	}
	if (remove(_annotationHere) < 0)
		return false;
	DBGPRINTF((L"AnnotationCollection::removeHere(%d)\n", _annotationHere));
	_annotationHere = -1;
	_vc->invalidate();
	return true;
}

ustring AnnotationCollection::getRegistryKey()
{
	ustring2 kname(L"file%08X", _vc->_MetafileId);
	return kname;
}

ustring AnnotationCollection::getWorkDirectory(bool temp, bool addSlash, LPCWSTR dataType /*= L"res"*/)
{
	WCHAR buf[MAX_PATH];
	GetWorkFolder(buf, ARRAYSIZE(buf), temp);
	LPWSTR p2 = buf + wcslen(buf);
	wsprintf(p2, L"%s%04d", dataType, _vc->_MetafileId);
	if (addSlash)
		wcscat_s(buf, ARRAYSIZE(buf), L"\\");
	ustring path(buf);
	return path;
}

LPCWSTR AnnotationCollection::toCache(TextCacheEntry *newEntry)
{
	_tcache.add(newEntry);
	return newEntry->Data;
}

AnnotationCollection::TextCacheEntry *AnnotationCollection::fromCache(int entryId)
{
	for (int i = 0; i < (int)_tcache.size(); i++)
	{
		TextCacheEntry *entry = _tcache[i];
		if (entry->EntryId == entryId)
			return entry;
	}
	return NULL;
}

