# Maximilian's File Hex-Dump Tab for Windows Explorer

Maximilian's File Hex-Dump Tab is a Windows Explorer add-on written in C++. The Tab is much more than a simple dump utility. With it, not only can you quickly inspect the binary content of any file directly in Windows Explorer, you can also easily annotate, color and circle parts of the file data meaningful to you. For image files like JPEG, the Tab can scan the entire file, and automatically tag well-known data segments and structures. If you want, you can help us extend the Tab's scan and tag feature to other file types by writing a scan server. Refer to the demo project for how. The Tab is a full-featured Windows app. After the file is marked and commented, tell the Tab to send it to a printer for a paper printout, or save it as a bitmap image file on the disk. Or, copy and paste it to a report you are writing. The convenience features are only a click away.

## Features

Let's give you a functional overview first. Hopefull you develop a good idea on what the Tab can do. Later on, we will get to more technical stuff.

### At a Glance

* Hex view generator
  * Dump formats (8, 16, and 32 bytes per row)
  * Keyword search
  * ASCII column to hex column association
* Meta objects
  * Regions and annotations as tags
  * Image annotation
  * Large-text annotation
  * Free-flowing shapes
    * Translation and rotation
    * Custom cursors
  * Meta object persistence
  * Meta object navigation
* Tag scan
  * built-in support
    * Thumbnail
    * Metadata parser: Exif, XMP, ICCP, Photoshop
    * BOM, Unicode, UTF-8
  * External scan server
* Utility
  * Page setup and print
  * Save as bitmap image
  * Copy to clipboard

### Windows Explorer Integration

The Tab is an add-on for Windows Explorer, which makes its hex-dump service easily accessible to users of Explorer. To see the Tab in action, open a folder window, select a file you want to examine. Open File Properties from the context menu. Hit the Hex Dump tab. Want to jump to hex digits you know? Use `Find Data` to search and be taken there. Want to know which pair of hex digits corresponds to an ASCII character in the right-hand column? Just click on the character. A red box is drawn to highlight the digit pair in the hex-dump area.

![alt Tab view - circle color and annotate](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/tab%20view%20-%20circle%2C%20color%20and%20annotate.png)

The Tab shows the file offset in the left-hand column, the hex digits of source bytes in the middle column, and the source bytes as ASCII characters in the right-hand column. If the file has text encoded in Unicode or UTF-8, run a scan. The Tab shows the text as Unicode characters in the right-hand column, instead.

Is the default 8-byte-per-row output format too small? Use menu command `Open in New Window` to open the file in a separate window with a larger display and show the hex output in a 16-byte-per-row format. The larger window comes with a tool bar of command buttons for your convenience, too.

![alt Tab view - wide display](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/standalone%20tab.png)

To search by keyword, use menu command `Find`. Type a keyword as a binary sequence or character string (UTF-8 or Unicode).

![alt Find data](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/find%20data.png)

### Colors, Tags and Shapes to Mark Data Blocks

Suppose you find interesting bytes in the hex output, and want to highlight. But how? Easy. Click on the starting byte, and drag to extend the selection of byte range. The bytes are automatically filled with color. Next, add a note of description. Need a picture with the note? No problem. Copy the source image onto the clipboard. Then, just paste it into the note. Want to circle the range? Or, underline it with a wavy line? With the Tab, you can do them, too. They are on the Shape menu. Is the circle (more like an ellipse) too small? Stretch it to cover all of the interesting bytes. If you need to, you can rotate the shape, too. Choose Apply or OK to keep the tags and shapes you added. Next time the file in opened for properties, they are automatically pulled up and shown.

A keyboard person might prefer menu command `Add Tag` to the mouse method, and supply a byte range and a comment.

![alt Add tag](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/add%20tag.png)

Note that colored regions, notes, and shapes are referred to as `meta` objects in the Tab project.

### Scan for Tags

The Tab has built-in support for detecting documented segments and structures of image data and automatically placing tags with descriptive information.

![alt Tab view after jpeg scan](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/tab%20after%20jpeg%20scan.png)

The file types the Tab can scan are __jpeg__, __jpg__, __png__, __bmp__, __gif__, __ico__, and __webp__.

The Tab provides an API for extending scan to other file types. To enable scan for a file type you want, you can write your own scan server. Refer to demo project [ScanSrvWebp](#ScanSrvWebP-Demo-Scan-Server) for the API details.

### Print, Copy, or Save

To send the hex output to a printer, use `Print`. Choose a printer from the list. Set the range of pages. If the output is too crowded with tags making it difficult to distinguish one pair of hex digits from others, know you have an option of grouping the tags in a column. That may give you a clarity you need. Use the Fit option to squeeze the hex output to fit the paper.

![alt Print page setup](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/print%20page%20setup.png)

Want to add the hex output to a document? Use `Save As Image`. The menu command lets you copy it to the system clipboard. Paste it directly into your document. Or if you want it on a disk, you can save it as a PNG or BMP file, too.

![alt Save hexdump](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/save%20hexdump.png)

### Standalone Viewer App

Explorer's File Properties is compact in size. So, it can give the Tab only a limited real estate, and the view that can be generated is short and narrow. By default, the Tab outputs the hex digits in an 8-bytes-per-row format. It can output more bytes per row. But, you'd need to either use the scrollbar or choose a smaller font to see the digits you want. As an alternative, you can opt for the standalone viewer. You don't have to live with the space constraint. Select `Open in New Window` from the context menu. The hex digits are now output in a 16-bytes-per-row format. If you need to see even more digits, you can choose the 32-bytes format. The standalone viewer sports a toolbar. Use it to quickly access main features of the Tab.

![alt Tab after jpg scan](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/standalone%20tab%20after%20jpg%20scan.png)

The wide-format viewer is actually always available as a standalone app. Hit Windows Start. Go to `Maximilian's Tools`. Select View File Hex-Dump. Choose a file you want to examine. The hex digits are output in the wide format.

### Project Organization

Let's get technical. The project consists of four Visual Studio component projects plus a tool set for building the msi.

1) `hexdumptab`, a Shell propertysheet extension,
2) `hexdumpdlg`, a viewer app for larger dump formats,
3) `scansrvwebp`, a demo scan server for WebP images, and
4) `setup`, a self-extracting MSI-based installer.

Setup embeds both a x64 and x86 msi installers. It starts an msi appropriate for the system when it is run.

The msi tool set consists of a template msi, a build staging batch process, and a script for generating msi tables and packaging the project files in the msi. Refer to [build.md]( ) for info on how to build the project.

## Gettting Started

Prerequisites:

* Windows 7 or later.
* Visual Studio 2017 or later. The VS solution and project files were generated using VS 2017. They can be imported by a later version of Visual Studio.
* Windows SDK 10.0.17763.0. More recent SDKs should work, too, although no verification has been made.

The installer is available from [here]( ). Installation requires admin priviledges as it performs a per-machine install.

To build the installer, install [`Maximilian's Automation Utility`](https://github.com/mtanabe-sj/maximilians-automation-utility/tree/main/installer/out). The build process requires Utility's `MaxsUtilLib.VersionInfo` automation object. 

## Design and Implementation

### HEXDUMPTAB Shell Propertysheet Extention DLL

This is the main component project. It builds the Tab shell extension. The Tab consists of the components below with the C++ class or COM interface associations noted in parantheses.

* Communication with the host (_class PropsheetExtImpl_)
  * Propsheet extension (_interface IShellPropSheetExt_)
  * Custom host configuration (_interface IHexDumpConfig_)
* View generation (_class BinhexView_)
* UI management and command invocation (_class BinhexDumpWnd_)
  * Thread local storage (_class TLSData_)
  * Keyboard accelerator (_class KeyboardAccel_)
  * Context menu (_method BinhexDumpWnd::OnContextMenu_)
  * Command processing (_method BinhexDumpWnd::OnCommand_)
  * Drag and drop processing (_methods OnLButtonDown, OnMouseMove, OnLButtonUp_)
  * Keyword search (_class FindDlg_)
  * Print/page setup (_class HexdumpPrintJob, class HexdumpPrintDlg_)
  * Page save/copy (_class DataRangeDlg_)
* Meta object management (_class BinhexMetaView_)
  * Tag (_classes CRegionCollection, AnnotationCollection, AddTagDlg_)
  * Shape (_classes GShapeCollection, GripCursor_)
  * BOM and Unicode (_class BOMFile_)
  * Serialization (_class persistMetafile_)
* Tag scan (_class ScanTag_)
  * Image support (_classes ScanTagJpg, ScanTagPng, ScanTagGif, ScanTagBmp, ScanTagIco_)
  * Text support (_class ScanTagBOM_)
  * Metadata parser (_classes MetadataExif, MetadataXMP, MetadataICCP, MetadataPhotoshop_)
  * Scan server API (_class ScanTagExt, ScanDataImpl, interfaces IHexDumpScanSite, IHexDumpScanData_)
* Utility
  * Codec wrapper (_classes CodecImage, BitmapImage_)
  * String manipulation (_classes ustring, astring, bstring_)
  * List management (_classes simpleList, objList, strvalenum_)
  * Registry configuration (_functions Registry*_)
  * Logging (_EventLog_)

#### Communication with the host

Central to the Tab as an Explorer add-on is class PropsheetExtImpl. It creates and manages a Win32 property page bound to a property sheet managed by Explorer. The class implements IShellPropSheetExt which enables communication with the host app. PropsheetExtImpl hosts a child window of BinhexDumpWnd to manage view generation and interactions with the user. BinhexDumpWnd subclasses BinhexMetaView which manages the hex view and meta objects. Meta objects are tags (colored regions and annotations) and free-flowing graphical shapes that users can attach to parts of the hex view. Meta objects are serialized to and de-serialized from arrays of fixed-length structures forming a so-called meta file. Class persistMetafile is responsible for the operations. These are the meta object structures.

```C++
struct DUMPREGIONINFO { LARGE_INTEGER DataOffset; ULONG DataLength; ...; UINT AnnotCtrlId; };
struct DUMPANNOTATIONINFO { WCHAR Text[256]; WCHAR TextEnd; BYTE Flags; ...; HBITMAP ThumbnailHandle; };
struct DUMPSHAPEINFO { BYTE Shape; BYTE Flags; USHORT StrokeThickness; ...; COLORREF InteriorColor; };
```

See _persistMetafile::save_ on how meta objects are serialized. This takes place when the OK or Apply button is selected. Loading or de-serialization of persisted meta objects happens automatically next time the Tab is opened for the same file. See the _persistMetafile::load_ method. A meta file that stores serialized meta objects is located in a subfolder of the per-user folder of _%LocalAppData%_. How does the load method know which meta file to read? The _save_ method generates a unique ID naming the meta file after it, and writes a named setting with the dumped file as the name and the meta file ID as the value in a per-user key of the system registry. The _load_ method makes a lookup for the dumped file in the registry and retrieves the meta file ID. Based on the file ID, it locates the meta file and de-serializes and loads the meta objects. The registry key that maintains the meta file mapping is HKEY_CURRENT_USER\SOFTWARE\mtanabe\HexDumpTab\MetaFiles. Entries in the key look like this.

![alt Metafiles registry key](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/metafiles%20registry%20key.png)

#### View Generation

During the startup, _BinhexDumpWnd::OnCreate_ initializes the view generator by creating fonts, obtaining the screen resolution, and text metrics, and preparing a dump buffer. _BinhexDumpWnd::OnPaint_ responds to system view refresh requests by calling the view generator's core method, _BinhexView::repaint_. _repaint_ through the _refreshInternal_ callbacks perform these display tasks in this order.
1) _BinhexMetaView_ reads source data.
2) _BinhexView_ outputs a hex dump and draws a row selector.
3) _BinhexMeataView_ paints the regions, annotations, and shapes.
4) _BinhexDumpView_ marks the search hit, if occuring in the current page.

Class _ROFile_ serves bytes from the file being examined to the view generator. It wraps Win32 file management API. It uses structure _FILEREADINFO_ to buffer file data and track the current read offset. You can think of a pointer to FILEREADINFO like the traditional C file pointer. _BinhexDumpWnd_ subclasses _ROFile_ and uses its methods to read the file.

#### UI Management

In a standard Win32 window application, defining and routing keyboard shortcuts means adding an accelerator table to the resource file (rc) and calling TranslateAccelerator in the message pump. Unfortunately, this simple model does not work for the propsheet extension. The Tab does not own the message pump. To work around the shortcoming, the Tab sets up a message hook and intercepts Win32 messages generated in Explorer's message loop. If the intercepted message is intended for the Tab's main window (of BinhexDumpWnd), TranslateAccelerator is called generating a desired WM_COMMAND message. The accelerator loading and message interception and translation are implemented in class _KeyboardAccel_. Explorer is UI multi-threaded owning multiple message pumps. Multiple instances of the Tab may coexist. So to be able to bind to multiple pumps, the Tab uses WIn32 thread local storage and saves the per-thread hook descriptors. That way, multiple message hooks can be maintained. The TLS management is implemented in class _TLSData_. The Tab maintains a singlton instance of TLSData (the definition in dllmain.cpp).

There are basically three sources of commands and messages that require Tab's attention. One is internal commands generated by context menus. Another is user-generated mouse events that reposition the cursor, scroll the page, or start and end drag and drop operations. There is the third kind. That's system-generated messages for routine windowing events like closing the app window. We won't go into them since they require standard handling and no special documentation is necessary.

OnContextMenu of BinhexDumpWnd prepares and runs a context menu on detecting a right-click. Depending on the type of an item clicked, some menu commands are either grayed out or removed. But, all instances of context menu derive from a single MENU resource definition (_IDR_VIEWPOPUP_). Here is an example menu enabling a color chooser submenu in response to a click on a colored region.

![alt Context menu](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/context%20menu.png)

The menu commands and how they are processed are summarized below.

|Menu label|Command ID|Handler|Remarks|
|----------------------|----------------------|----------------------|---------------------------------------------------|
|Open in New Window|IDM_OPEN_NEW_WINDOW|OnOpenNewWindow|HEXDUMPDLG viewer app is run. Removed in HEXDUMPDLG.|
|Bytes per Line|IDM_BPL_(N)|OnSelectBPR(N)|IDM_BPL_8 and accompanying IDM_SAME_FONTSIZE are grayed in HEXDUMPDLG.|
|Add Tag|IDM_ADD_TAG|OnAddTag|To be followed by parameter fill in dialog of AddTagDlg.|
|Add Note|IDM_ANNOTATE|OnAnnotate|addAnnotation of AnnotationCollection shows an empty note for user to fill.|
|Remove|IDM_REMOVE|OnRemoveMetadata|grayed out if no meta object is selected; method removeHere of CRegionCollection, AnnotationCollection, or GShapeCollection is called for actual removal.|
|Color Tag|IDM_COLOR_TAG0+N|changeColorHere of class CRegionCollection|N indexes static color palettes, s_regionInteriorColor and s_regionBorderColor.|
|Remove All Tags|IDM_REMOVE_ALL|OnRemoveAllTags|clear of class CRegionCollection deletes all regions and their associated annotations.|
|Shape, Line/Rectangle/Circle|IDM_DRAW_(shape)|OnDrawShape(shape)|a DUMPSHAPEINFO is initialized for selected shape type and added to GShapeCollection.|
|Shape, Properties, Line, Thickness|IDM_LINE_THICK(N)|OnLineThickness(N)|DUMPSHAPEINFO.StrokeThickness is set to selected thickness (1, 2, or 4).|
|Shape, Properties, Line, Type|IDM_LINE_(T)|OnLineType(T)|DUMPSHAPEINFO.GeomOptions is set to e.g., DUMPSHAPEINFO_OPTION_WAVY for wavy line. WAVY, ARROW, or ARROW2 are available. ARROW means an arrow head on one end. ARROW2 arrow head on both ends.|
|Shape, Properties, Line, Interior Color|IDM_COLOR_IN0+N|OnShapeInteriorColor(N)|DUMPSHAPEINFO.InteriorColor is set to index of selected color sample.|
|Shape, Properties, Interior Color|IDM_COLOR_IN0+N, IDM_COLOR_IN_TRANSPARENT|OnShapeInteriorColor(N), OnShapeInteriorTransparent|DUMPSHAPEINFO.InteriorColor is set to index of selected color, and DUMPSHAPEINFO.Flags raises DUMPSHAPEINFO_FLAG_FILLED. OnShapeInteriorTransparent clears DUMPSHAPEINFO_FLAG_FILLED.|
|Shape, Properties, Outline Color|IDM_COLOR_OUT0+N|OnShapeOutlineColor(N)|DUMPSHAPEINFO.OutlineColor is set to index of selected color.|
|Save as Image|IDM_SAVEDATA|OnSaveData|DataRangeDlg is run with initial offset set to current page offset. On return, generated bitmpa of the page is saved in a file or copied to the clipboard.|
|Print|IDM_PRINTDATA|OnPrintData|HexdumpPrintDlg is run with a HexdumpPageSetupParams configuration initially set to default values. On successful page setup, HexdumpPrintJob is started on worker thread. Successful job completion posts IDM_PRINT_EVENT_NOTIF with task status TskFinishJob. OnCommand stops the worker thread.|
|Find|IDM_FINDDATA|OnFindData|FindDlg is run, followed by navigation with searchAndShow to found data.|
|Scan|IDM_SCANTAG_START|OnScanTagStart|A ScanTag subclass runs on worker thread. Successful completion posts IDM_SCANTAG_FINISH with status 0.  OnScanTagFinish responds by persisting scanned tags in meta file, and trigger a view refresh with fade-out.|

Another important aspect of UI management is handling system events for drag and drop and page scroll actions. The Tab handles these system messages.

|Message|Handler|Invoked Operations|
|---------------|-----------------|------------------------------------------------------------------|
|WM_LBUTTONDOWN|OnLButtonDown|1) Clear prior selection in ASCII column. 2) Start drag and drop on a shape object, or 3) Check annotation grab, or 4) Start a region drag to extend the selection.|
|WM_MOUSEMOVE|OnMouseMove|Button Down: 1) Continue shape drag, or 2) Continue annotation drag, or 3) Continue stretching the region.|
|||Button Up: 1) Grab a shape, or 2) Highlight shape at cursor, or 3) Highlight annotation at cursor.|
|WM_LBUTTONUP|OnLButtonUp|1) Stop ongoing drag operation and execute drop, or 2) Draw a box around hex number corresponding to ASCII column selection.|
|WM_VSCROLL|OnVScroll|The scroll page width is given by _vi.RowsPerPage_. Page-wise actions invoke startFadeoutDelay of AnnotationCollection to refresh with annotation fadeout.|
|WM_HSCROLL|OnHScroll|The scroll page width is given by _vi.ColsPerPage_. Horizontal scroll bar is visible only in 16 or 32 BPR. The offset column remains visible even when the hex and ASCII columns are shifted.|

Some of the commands metioned above engage dialog interaction. They are _Keyword Search_, _Save as Image_, and _Print_.

_OnFindData_ for _Keyword Search_ uses dialog class FindDlg. Users can specify a keyword in one of the three encodings.

	* UTF-8 (FINDCONFIGFLAG_ENCODING_UTF8)
	* UTF-16 (FINDCONFIGFLAG_ENCODING_UTF16)
	* 2-digit hex (FINDCONFIGFLAG_ENCODING_RAW)

The dialog's OnCommand calls _updateSearchConfig_ to save the keyword string in the selected encoding in the output structure of _FINDCONFIG_. OnPaint draws the re-encoded keyword bytes as rows of 2-digit hex numbers as a visual feedback so that the user knows what binary byte value is being searched for. On return from the dialog, _searchAndShow_ calls _ROFile::search_ to search the file. If it finds a match in the current page, it simply refreshes the view. If the match is not in the current page, it lets _scrollToFileOffset_ to scroll to the page.

_OnSaveData_ for _Save as Image_ is another dialog-invoking command. It uses _class DataRangeDlg_ to interact with the user. The dialog class collects three settings, a destination medium, a data range, grouping of annotations or not. The data range can be specified either in number of bytes or number of dump lines. To help the user see the change, _BinhexMetaPageView_, a subclass of _BinhexMetaView_, is used to give a preview of what the hex dump as an image would look like. The view generator subclass is capable of splitting the viewport into two, one showing the hex digits and ASCII column, and the other showing the annotations that belong to the data range. It uses the split viewport in response to selection of the _IDC_CHECK_SEGREGATE_ANNOTS_ option. Actually, _BinhexMetaPageView_ generates a full-scale bitmap image first. Then, it scales it to fit the preview rectangle. _DataRangeDlg_ keeps the full-scale bitmap in a member variable. _OnSaveData_ picks up the bitmap from the dialog. If the destination is file, the method uses the bitmap helper class _BitmapImage_ to a file of a user-supplied name. If the destination is clipboard, Win32 SetClipboardData API is called on the bitmap.

The last but not least dialog-invoking command is _Print_. _OnPrintData_ responds to it. It maintains an instance of struct _HexdumpPageSetupParams_ throughout the life of the Tab instance. The dialog class _HexdumpPrintDlg_ runs a page setup holding setup parameters in the _HexdumpPageSetupParams_. On successful conclusion of the dialog, a print job is started by creating an instance of class _HexdumpPrintJob_. It's a worker running in a separate thread so as not to block the main UI thread. The worker renders the selected pages of hex dump in the device context of a selected printer. When it's done, the worker posts a notification command message (IDM_PRINT_EVENT_NOTIF with task ID of TskFinishJob). OnCommand of _BinhexDumpWnd_ responds to it by doing a cleanup.

There are a few caveats you need to be aware of when using the [PrintDlg API](https://docs.microsoft.com/en-us/windows/win32/api/commdlg/nc-commdlg-lpprinthookproc). There is a supposedly 'better' version called PrintDlgEx. With the latter API, you could take the smarter partial template approach rather than import and edit the entire dialog template which I had to do for PrintDlg. Unfortunately, I could not get PrintDlgEx customization to work. If you know how, let me know.

To customize the PrintDlg UI, edit the PRINTDLGORD dialog template in res\hexdumptab.rc2. PRINTDLGORD needs dlgs.h of the Windows SDK for control IDs. The customization includes a preview pane. A change made to the print range, margins, Group Notes, or Fit to Page Width is immediately reflected in the preview display. Note that the preview pane (IDC_STATIC_PAGE1) is a static control with style SS_OWNERDRAW. OnCommand responds to control events by invalidating the preview pane. OnDrawItem responds to the refresh request by updating the preview output. The method uses class _BinhexMetaPageView_ to turn the starting or ending page into a bitmap image which is then scaled to fit into the margins.

_OnPrintData_ calls _HexdumpPrintDlg::startPrinting_ to print the selected pages. _startPrinting_ creates an instance of _HexdumpPrintJob_ passing the PRINTDLG structure filled with a printer descriptor and page settings chosen in the setup dialog. _HexdumpPrintJob_ runs a worker on a new thread to process the print job. If the Cancel button is clicked, the host generates a PSN_QUERYCANCEL invoking _propsheetQueryCancel_ of _BinhexDumpWnd_. On receiving a confirmation from the user, _BinhexDumpWnd_ aborts the print job by calling _HexdumpPrintJob::stop_. The _stop_ method raises the thread kill signal (_kill_). The print task loop of _HexdumpPrintJob::runWorker_ detects the raised _kill_ and terminates the worker. If the print job completes, _runWorker_ posts an IDM_PRINT_EVENT_NOTIF notification for TskFinishJob to _BinhexDumpWnd_ which calls _HexdumpPrintJob::stop_ to make sure the worker is terminated.

_HexdumpPrintJob_ takes advantage of device independence of GDI. Rendering to a printer is basically the same as displaying the page on a monitor. Both call _BinhexView::repaint_. The difference is in configuration. The text metrics differs because the device contexts of the two media use different resolutions. The printer uses a high definition of 600 dpi while the display monitor uses a low 96 dpi. Only the metrics is different. But, the code is the same. So, I thought. Actually, the code had to be modified for the printer DC. If you look at _drawHexDump_ and _finishDumpLine_ of _BinhexView_, you see that the hex numbers and ASCII characters are buffered together and rendered using DrawText API per row. This strategy is easy to implement. But it has a drawback. The further away from the start of the row a character is positioned, the less accurate the character position becomes. When previewed on screen, the rows are a snuggle fit inside the margins (because the Fit to Width option was used). But, when printed, the rows are too short leaving a gap between the ASCII column and the right margin or too long sticking past the right margin.

Why is that? It boils down to rounding error. A 10-pt fixed-pitch Courier font in a 96-dpi screen context translates to 8 pixels in character width. The same 10-pt Courier in a 600-dpi printer context is 83 dots wide. The low-def screen context has a significant figure of one, while the high-def printer context has two. If the true 10-pt character has a screen width of 8.9 pixels but was rounded off to 8 due to float-to-integer truncation, that's an error of 0.9/8 = 11%. For the printer context, the rounding error can be as large as 0.9/83 = 1%. What matters to us is that the screen metrics tends to be more inaccurate, and that the inaccuracy is many times over for the character at the end of the row. The printer metrics is less so. This is why a previewed page may not actually fit inside the margins when printed. 

Compounding the problem is this. The Tab lets users tag pieces of data. The selected range is colored and outlined. To achieve the effect, the Tab locates the front of the data range in device coordinates and redraws the hex numbers and ASCII text over the range. The starting location is calculated by multiplying the character width by the column position of the range front. The rounding error in the character width becomes progressively large for a character toward the end of the row, and the character and its bounding outline appear off, displaced from the original position. Even if the error is 1%, when multiplied by the number of character spaces, it becomes significant and makes the displacement noticeable.

The abovementioned metrics problems are addressed this way.

* Call GDI _GetTextExtentExPoint_ on a string of as many 'X' characters as necessary to fill a hex-dump row. That gives us an accurate logical paper width. When printing, go an extra step in _setupViewport_ after the logical-to-device coordinate mapping is calculated. Keep the full extents (width and height) of the 'X' row in _VIEWINFO.FullRowExts_. It will be used to compensate for the rounding error. 

* Override _BinhexView_ when it calculates the x-coordinate of the position of a hex number or ASCII dump character. _BinhexView_ gets the position by multiplying the character width by the column position. _HexdumpPrintJob_ uses two overrides, one for hex number, and the other for ASCII character. The two do this. Compute the x-coordinate by dividing _FullRowExts.cx_ by the column position.

```C++
void HexdumpPrintJob::setupViewport()
{
	// map a logical inch (1000 TOI units) to printer (device) dpi (e.g., 600 dpi).
	SetMapMode(_hdc, MM_ISOTROPIC);
	setViewMapping({ 0,0 }, { 0,0 });

	// find the full extents of the dump rectangle.
	ASSERT(_setup->requiredCols == (OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + RIGHT_SPACING_COL_LEN + (COL_LEN_PER_DUMP_BYTE + 1) * _vi.BytesPerRow));
	// generate a text string that spans the full dump width. since it's a fixed pitch font, any character will do. use 'X'.
	astring src;
	src.fill(_setup->requiredCols, 'X');
	// select the font we use in hex dump to the printer device context.
	HFONT hf0 = (HFONT)SelectObject(_hdc, _hfontDump);
	// ask gdi to measure the full width.
	GetTextExtentExPointA(_hdc, src, src._length, 0, NULL, NULL, &_vi.FullRowExts);
	SelectObject(_hdc, hf0);
}

int HexdumpPrintJob::_getAscColCoord(int colPos)
{
	if ((_setup->flags & HDPSP_FLAG_FITTOWIDTH) && _vi.FullRowExts.cx != 0)
	{
		int p2 = OFFSET_COL_LEN + LEFT_SPACING_COL_LEN + RIGHT_SPACING_COL_LEN + COL_LEN_PER_DUMP_BYTE * _vi.BytesPerRow + colPos;
		int x = MulDiv(_vi.FullRowExts.cx, p2, _setup->requiredCols);
		return x;
	}
	return BinhexMetaView::_getAscColCoord(colPos);
}
```

#### Meta Object Management

Meta objects regard data range definitions called colored regions, annotations attached to specific data bytes, and graphical shapes attached to specific data bytes. These objects are created and managed by the Tab. They are stored in a data file managed by _persistMetafile_. The Tab uses the term Tag to refer to a colored region with an annotation.

The meta objects are managed by the three collection classes which all derive from simpleList<T>. BinhexDumpWnd, the main window of the Tab, owns the collections of meta objects, and uses _persistMetafile_ to persist them across processes of the Tab.

```C++
class CRegionCollection : public simpleList<DUMPREGIONINFO> {...};
class AnnotationCollection : public simpleList<DUMPANNOTATIONINFO> {...};
class GShapeCollection : public simpleList<DUMPSHAPEINFO> {...};

class BinhexMetaView : public BinhexView, public ROFile
{
public:
	...
	CRegionCollection _regions;
	AnnotationCollection _annotations;
	GShapeCollection _shapes;
	...
};

class BinhexDumpWnd : public _ChildWindow, public BinhexMetaView { ... };
```

In processing user-issued commands or responding to drag and drop actions, BinhexDumpWnd operate on member objects of the meta object collections using operational methods of CRegionOperation, AnnotationOperation, or GShapeOperation or their subclasses.


#### Tag Scan

For popular image types, the Tab can scan the file data for documented data segments and structures and mark them in colors and annotate with descriptive text. A thumbnail image based on the source image data is generated and placed in an annotation attached to the image data section.

The Tab natively supports images of JPEG, PNG, GIF, BMP, and ICO. It deploys classes ScanTagJpg, ScanTagPng, ScanTagGif, ScanTagBmp, or ScanTagIco_, respectively, in response to the image file's extension name. See _BinhexDumpWnd::OnScanTagStart_. ScanTag and its subclasses use MetadataExif and other parser classes to parse the well-known metadata of Exif, XMP, ICC Profile, and Adobe Photoshop. 

What about other image types? What about non-image files? To address such needs, the Tab provides a scan server API consisting of headers of COM interface deinitions and registration information. There is a demo server made available to give a quick jump start to those interested in expanding the Tab's scan capacity. Refer to the demo project for the details.

The Tab uses implements a scan server hosting site in class ScanTagExt. _OnScanTagStart_ calls the static _ScanTagExt::CanScanFile_ method to determine if there is an external scan server ready for the input file. If there is, the method runs an instance of _ScanTagExt_. _ScanTagExt::runInternal_ instantiates the server, and initializes the server passing an _IHexDumpScanSite_ object (actually, _this_ of the _ScanTagExt_ instance) and an _IHexDumpScanData_ object. Scan servers use methods on _IHexDumpScanSite_ to access host services and methods on _IHexDumpScanData_ to access data of the source file being scanned.


### HEXDUMPDLG Viewer Application
	
This component project builds a standalone propsheet viewer, an alternative to _Explorer's File Properties_. It features a wide hex-dump format and an easy-to-use command tool bar. The viewer may be started from the _Windows Start_ menu. Open the _Maximilian's Tools_ popup and select _Hex-Dump Viewer_.
	
The viewer consists of these components.

* Command line parser (function parseCommandline)
* Window management
	* Propsheet hosting (class PropsheetDlg)
	* Toolbar management (class ToolbarWindow)
* Communication with the tab (interfaces IShellPropSheetExt and IHexDumpConfig)

If you are interested in how Explorer loads a propsheet extension, check out _PropsheetDlg::createShellPropsheetExt_. The viewer app uses the customization interface IHexDumpConfig to configure its tool bar with private information from the Tab.


### ScanSrvWebP Demo Scan Server

As explained in Section [Tag Scan](#Tag-Scan), the Tab lets you add an external scan server. If you are interested in writing one, read the notes below, see how the demo does it first, and then, rewrite it to suite your needs.

The demo server supports WebP. WebP is well documented by Google who developed it in [this article](https://developers.google.com/speed/webp).

The basic formatting element WebP uses is the _chunk_ of _RIFF_. It is a generalized structure to package a piece of payload data, its size in bytes, and a textual code uniquely identifying the data. A file in WebP starts with a WebP file header of 12 bytes.

012345678901
RIFF****WEBP

where the **** 4-byte sequence is the file size in bytes. In its simplest form, WebP has two components, a WebP file header followed by a VP8 chunck for lossy compression or a VP8L chunk for lossless compression. Usually, however, WebP uses an extended format called VP8X. 

In terms of data structure, WebP is much like PNG. It's composed with well-defined data blocks, a.k.a., chunks. Parsing of WebP data is similer to parsing of PNG.
	
#### Server Registration

Since a scan server is a COM server, standard registration with COM is made in key HKCR\CLSID\{Server-CLSID}. The demo server has a CLSID of {6ccda86f-3c63-11ed-9d82-00155d938f70}.

![alt Scan server clsid registration](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/scan%20server%20clsid%20registration.png)
	
To let the Tab know the server's availability, the scan server makes these settings under HKLM.
ScanServers: .webp --> {Server-CLSID}
ExtGroups: .webp --> image

The above configuration settings instruct the Tab to deploy the server if the file has an extension of .webp and find the COM server at the specified CLSID. When it runs a File Open dialog, the Tab adds the extension name (.webp) from the registry key to the file extensions of the _image_ group so that if the user restricts file listing to images, files of .webp are listed as well.

![alt Scan server registration](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/scan%20server%20registration.png)

![alt Scan server ext-group registration](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/scan%20server%20ext-group%20registration.png)
	
#### Interface Implementation

The Tab defines three interfaces, one for a scan server, one for a scan site, and the other for a scan data. The scan server interface is implemented and exposed by a scan server. The Tab which is a client of the scan server hosts the server and invokes the server's scan function through the interface.
	
* IHexDumpScanServer
* IHexDumpScanSite
* IHexDumpScanData

Scan servers implement the scan server interface IHexDumpScanServer. The Tab implements the other two. Scan servers use the data interface to read and search the file data. Servers can also use it to ask the Tab to parse metadata on its behalf.

A scan server may want to access services of the Tab as well as attributes of the dump file. It can do so through the scan site interface. The Tab implements the site interface on its _ScanTagExt_ object. It passes a pointer to the interface to the server when it calls the server's Init method to give the server a chance to initialize itself based on the file attributes available from the data and site interfaces the server receives.

```C++
DECLARE_INTERFACE_(IHexDumpScanData, IUnknown)
{
	...
	STDMETHOD(GetFileSize) (THIS_ LONGLONG *SizeInBytes) PURE;
	STDMETHOD(GetFilePointer) (THIS_ LONGLONG *Offset) PURE;
	STDMETHOD(Read) (THIS_ LPVOID DataBuffer, ULONG *DataSizeInBytes) PURE;
	STDMETHOD(Seek) (THIS_ LONG MoveDistance, SCANSEEK_ORIGIN SeekOrigin) PURE;
	STDMETHOD(Search) (THIS_ LPBYTE BytesToSearch, ULONG ByteCount) PURE;
	STDMETHOD(ParseMetaData)(THIS_ SCAN_METADATA_TYPE MetaType, LONGLONG MetaOffset, LONG MetaLength, SHORT ParentIndex, SHORT *TagIndex) PURE;
};
```

The server uses methods TagData and Annotate of the site interface to mark a block of scanned data as a colored region with a description, and attach a descriptive note or bitmap image to a data byte, respectively.
	
```C++
DECLARE_INTERFACE_(IHexDumpScanSite, IUnknown)
{
	...
	STDMETHOD(GetVersion)(THIS_ ULONG *SiteVersion) PURE;
	STDMETHOD(GetState) (THIS_ SCANSITE_STATE *SiteState) PURE;
	STDMETHOD(GetFileName)(THIS_ BSTR *FileName) PURE;
	STDMETHOD(TagData)(THIS_ USHORT Flags, LONGLONG TagOffset, LONG TagLength, BSTR TagNote, SHORT ParentIndex, SHORT *TagIndex) PURE;
	STDMETHOD(Annotate)(THIS_ LONG TagIndex, VARIANT *SourceData) PURE;
};
```

The Tab calls Init to initialize the scan server, Scan to start a tag scan, and Term to terminate the server. If it receives a fail code from the methods, the Tab calls GetErrorMessage to retrieve a corresponding error message.
	
```C++
DECLARE_INTERFACE_(IHexDumpScanServer, IUnknown)
{
	...
	STDMETHOD(Init) (THIS_ IHexDumpScanSite *ScanSite, IHexDumpScanData *SourceData) PURE;
	STDMETHOD(Term) (THIS) PURE;
	STDMETHOD(Scan) (THIS) PURE;
	STDMETHOD(GetErrorMessage) (THIS_ BSTR *Message) PURE;
};
```


## Contributing

Bug reports and enhancement requests are welcome.

	
## License

Copyright (c) mtanabe, All rights reserved.

Maximilian's File Hex-Dump Tab is distributed under the terms of the MIT License.
The libheic and associated libraries are distributed under the terms of the GNU license.
