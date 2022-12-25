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
#include <Windows.h>
#include "lib.h"
#include "BinhexView.h"


class AnnotationCollection;

/* AnnotationOperation - performs display and text edit operations related to a single annotation.
*/
class AnnotationOperation
{
public:
	AnnotationOperation(DUMPANNOTATIONINFO &ai, AnnotationCollection *coll) : _ai(ai), _coll(coll) {}
	virtual ~AnnotationOperation() {}

	DUMPANNOTATIONINFO &_ai; // reference to the annotation descriptor managed by AnnotationCollection.

	bool initUI(LARGE_INTEGER contentPos, UINT id, bool startEditing);
	bool reinitUI(bool startEditing = false);
	void drawAnnotationAt(HDC hdc, POINT ptOrig, POINT ptLT);
	HRESULT drawAnnotation(HDC hdc, bool reduceTransparency);
	bool highlightAnnotation(RECT &hitRect);
	HRESULT renderAnnotation(HDC hdc, RECT annotRect, POINT originPt, bool drawConnector = true, HRGN *hrgnOut=NULL);
	HRESULT blendAnnotation(HDC hdc, RECT annotRect, POINT originPt, BYTE transparency);
	void renderConnectorArrow(HDC hdc, RECT annotRect, POINT originPt);
	HBITMAP renderToBitmap(RECT *outRect);
	void attachThumbnail(HBITMAP hbmp, USHORT imgSrcId=0); // set imgSrcId to 0 to refer to the dumped file itself; file name of source image is obtained from _vc->_highdefVP->ImageSources[imgSrcId]
	USHORT saveSourceImage(HBITMAP hbmp, int srcId, int *srcCount);
	HBITMAP saveThumbnail(bool clearHandle);
	HRESULT persistResources();
	ULONG queryWorkPath(bool temp, LPCWSTR dataExt, USHORT srcId, ustring &outPath);
	HRESULT loadThumbnail();

	bool onSelect(POINT pt);
	bool reselect();
	bool containsPt(POINT pt, bool *onGripSpot);
	bool setText(LPCWSTR srcText);
	bool finalizeUIText(bool aborted=false);
	bool updateUIText(LPCWSTR textToAdd=NULL);
	void resetDimensions(bool canSizeUI=true);
	HRESULT beforeRemove();
	void cleanUp();

	LPCWSTR saveText(LPCWSTR srcText);
	LPCWSTR syncText(LPCWSTR textToAdd = NULL);
	LPCWSTR getText();
	USHORT getTextLength();

	LPCWSTR formatAsMenuItemLabel(ustring &outStr);

protected:
	AnnotationCollection *_coll; // back pointer to the collection that the annotation belongs to.

	bool annotationInFrame(RECT &outAnnotRect, POINT *outOriginPt = NULL);
	void _getBaloonLayout(RECT &annotRect, POINT &originPt, POINT pts[7], int inflate = 0);
	HRGN _createBaloonRgn(RECT annotRect, POINT originPt);
	SIZE _measureTextDimensions();
	void _inflateSizeForImage(SIZE &fsz);
	SIZE _getImageSize();
};

/* DUMPANNOTATIONINFO2 extends the base descriptor. it supports  see AnnotationCollection::getAnnotationsPane for an application.
*/
struct DUMPANNOTATIONINFO2 : public DUMPANNOTATIONINFO
{
	LARGE_INTEGER DataOffset; // duplicate of DUMPANNOTATIONINFO.DataOffset.
	// this comparison method is for qsort support. AnnotationCollection::redrawAll uses it to sort the annotations by data offset.
	static int compare(const void *a, const void *b)
	{
		DUMPANNOTATIONINFO2 *a2 = (DUMPANNOTATIONINFO2*)a;
		DUMPANNOTATIONINFO2 *b2 = (DUMPANNOTATIONINFO2*)b;
		LONGLONG diff = a2->DataOffset.QuadPart - b2->DataOffset.QuadPart;
		return (int)diff;
	}
};

/* AnnotationCollection - creates and manages meta annotations.
*/
class AnnotationCollection : public simpleList<DUMPANNOTATIONINFO>
{
public:
	AnnotationCollection(BinhexView *vc) : _vc(vc), _fadeoutDelayStart(0), _annotationHere(-1), _dnd{0}, _hitRect{0}, _recentCtrlId(0), _itemVisited(-1), _imageSrcCount(0), _textSrcCount(0), _persistStop(0), _persistCtrlId(0) {}

	// system tick starting a delayed display fadeout.
	DWORD _fadeoutDelayStart;
	// index of an annotation at the cursor.
	int _annotationHere;
	// count image sources and large-text sources.
	int _imageSrcCount, _textSrcCount;
	// drag and drop parameters.
	struct DRAGDROPINFO
	{
		bool Gripping, Gripped;
		POINT StartPt;
		POINT StartFrameOffset;
		RECT ImageRect;
		RECT PreReplaceRect;
		HBITMAP TransitImage;
		HBITMAP PreReplaceImage;
	} _dnd;

	void reinit(BinhexView *vc2, UINT annotFlags);

	DUMPANNOTATIONINFO *addAnnotation(LARGE_INTEGER contentPos, bool startEditing=true);
	DUMPANNOTATIONINFO *annotateRegion(class CRegionCollection *regions, int regionIndex, LPCWSTR annotText);
	HRESULT setPicture(DUMPANNOTATIONINFO *ai, IPicture *ipic);
	HRESULT setPicture(DUMPANNOTATIONINFO *ai, HBITMAP hbmp);

	bool selectAt(POINT pt);
	bool reselectHere();
	bool startDrag(POINT pt);
	bool continueDrag(POINT pt);
	bool stopDrag(POINT pt);

	bool removeHere();
	void redrawAll(HDC hdc);

	void redrawAll(HDC hdc, class CRegionCollection *regions);
	SIZE getAnnotationsPane(class CRegionCollection *regions, simpleList<DUMPANNOTATIONINFO2> *outList=NULL);

	bool startFadeoutDelay();
	void onFadeoutDelayTimer();
	void startPersistentAnnotation(UINT ctrlId, int persistSeconds);
	void stopPersistentAnnotation();

	void abortAnnotationEdit();
	HCURSOR queryCursor();
	int queryByCtrlId(UINT ctrlId);
	int queryAnnotationAt(POINT pt, bool screenCoords = false, bool excludeIfReadonly=false);
	int querySize(bool limitToIndies);
	int queryNextItem(LARGE_INTEGER contentPos, ULARGE_INTEGER fileSize, bool goBack, bool limitToIndies=true);
	DUMPANNOTATIONINFO *gotoItem(int itemIndex);
	bool highlightAnnotation(POINT pt, bool screenCoords = false);
	bool highlightAnnotationByCtrlId(UINT ctrlId);
	bool clearHighlight();

	void OnFontChange();
	void OnAnnotationEnChange(UINT ctrlId);
	void OnAnnotationKillFocus(HWND hwndLosing, HWND hwndGaining);
	bool OnAnnotationKeyDown(HWND hwnd, UINT keycode);

	void beforeLoad(bool temp, int size);
	void load(DUMPANNOTATIONINFO &ai);
	void afterLoad(HRESULT loadResult);
	void beforeSave(simpleList<HBITMAP> &list, bool appTerminating);
	void afterSave(simpleList<HBITMAP> &list, bool temp);
	void cleanUp();

	bool editInProgress();

protected:
	BinhexView *_vc; // pointer to the view generator.
	RECT _hitRect; // rectangle of an annotation recently highlighted.
	UINT _recentCtrlId; // most recently repainted annotation id. save it to prevent display flickers.
	int _itemVisited;
	// large-text cache entry
	struct TextCacheEntry {
		int EntryId; // index of an AnnotationObject.
		ustring Data; // keeps the large text.
		// standard constructor.
		TextCacheEntry() : EntryId(0) {}
		// constructor for cloning src.
		TextCacheEntry(const TextCacheEntry *src) : EntryId(src->EntryId)
		{
			Data = src->Data;
		}
	};
	objList<TextCacheEntry> _tcache; // large-text cache.
	// in persistent display, a single annotation is kept visible (fully opaque) for at least 5 seconds (see BinhexDumpWnd::OnGotoRegion) while other annotations surrounding it are hidden. the point is to put one annotation in the spot light.
	int _persistCtrlId; // index of an annotation subject to the persistent display
	time_t _persistStop; // when the persistent display should stop.

	bool persistingAnnotation(int ctrlId=0);

	bool insertFromClipboard(HWND hwnd);
	int finalizeAnnotationText(HWND hwnd, bool aborted=false);
	void clearDragdropInfo();
	int queryTextSrcCount();
	int queryImageSrcCount();
	ustring getRegistryKey();
	ustring getWorkDirectory(bool temp, bool addSlash=false, LPCWSTR dataType=L"res");
	LPCWSTR toCache(TextCacheEntry *newEntry);
	TextCacheEntry *fromCache(int entryId);

	// simpleList override
	virtual void clearItem(DUMPANNOTATIONINFO *pitem);

private:
	/* window subclass callback for the EDIT control AnnotationOperation hosts. the purpose is to detect key presses and input focus losses. see AnnotationCollection::addAnnotation.
	*/
	static LRESULT CALLBACK AnnotationSubclassProc(HWND hwnd, UINT msg_, WPARAM wp_, LPARAM lp_, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
	{
		AnnotationCollection* pThis = (AnnotationCollection*)dwRefData;
		switch (msg_)
		{
		case WM_KEYDOWN:
			if (wp_ == VK_RETURN || wp_ == VK_ESCAPE || wp_ == VK_INSERT)
			{
				if (pThis->OnAnnotationKeyDown(hwnd, (UINT)wp_))
					return 0;
				return DefSubclassProc(hwnd, msg_, wp_, lp_) | DLGC_WANTMESSAGE;
			}
			break;
		case WM_KILLFOCUS:
			pThis->OnAnnotationKillFocus(hwnd, (HWND)wp_);
			break;
		case WM_NCDESTROY:
			RemoveWindowSubclass(hwnd, &AnnotationSubclassProc, uIdSubclass);
			break;
		}
		return DefSubclassProc(hwnd, msg_, wp_, lp_);
	}

	friend AnnotationOperation;
};
