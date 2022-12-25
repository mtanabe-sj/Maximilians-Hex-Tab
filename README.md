# Maximilian's Hex Tab for Windows Explorer

Maximilian's Hex Tab is a Windows Explorer add-on written in C++. Tab is much more than a simple hex-dump utility. With it, not only can you quickly inspect the binary content of any file directly in Windows Explorer, you can also easily annotate, color and circle parts of the file data meaningful to you. For image files like JPEG, Tab can scan the entire file, and automatically tag well-known data segments and structures. If you want, you can help us extend Tab's scan and tag feature to other file types by writing a scan server. Refer to the demo project on how. Tab is a full-featured Windows app. After you are done marking places and annotating, tell Tab to send it to a printer for a printout, or save it as an image file on the disk. Or, tell Tab to copy it to the clipboard. You can then paste it to, e.g., a report you are writing. These convenience features are only a click away.

Use the [Setup program](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/installer/out/hextab_setup.exe) to install Tab.

## Features

Let's give you a functional overview first. Hopefully you will develop a good idea on what the Tab can do. Later, we will get to more interesting technical stuff.

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

Hex Tab is an add-on for Windows Explorer, which makes its hex-dump utility easily accessible to users of Explorer. To see Tab in action, open a folder window, select a file you want to examine. Open File Properties from the context menu. Hit the Hex Dump tab. Want to jump to data bytes you already know? Use `Find Data` to search and be taken there. Want to know which hex digit pair corresponds to an ASCII character in the right-hand text-dump column? Just click on the ASCII character. A red rectangle is drawn to highlight a digit pair in the hex-dump area that corresponds to the selected character.

![alt Tab view - circle color and annotate](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/ref/tab%20view%20-%20circle%2C%20color%20and%20annotate.png)

Tab display consists of three columnar areas, a left-hand column for showing the file offset, a middle column for showing hex digits of file data bytes, and a right-hand column showing the same data bytes as ASCII characters. If the file has text encoded in Unicode or UTF-8, run a scan. Tab displays the decoded Unicode characters in the right-hand column. Without a scan, the text would be shown as if it were in ASCII and end up looking garbled. Tha might not be meaningful.

Is the default 8-byte-per-row output format too small? Use menu command `Open in New Window`. The file is re-opened in a separate window with a larger display area capable of listing 16 bytes per dump line. The larger window comes with a tool bar of command buttons for quick access.

![alt Tab view - wide display](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/ref/standalone%20tab.png)

To search by keyword, use menu command `Find`. Type a keyword either as a binary sequence or character string (UTF-8 or Unicode).

![alt Find data](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/ref/find%20data.png)

### Colors, Tags and Shapes to Mark Data Blocks

Suppose you find interesting data bytes in the hex output, and want to highlight them. But how? Easy. Click on the starting byte, and drag to extend the selected byte range. The range is automatically given a color. Next, add a note to describe what the data is or what it means to you. Need to attach a picture to the note? No problem. Copy the picture onto the clipboard. Then, just go back to the note in Tab and hit the `Ctrl-V` or `Shift-Insert` keys. The picture should appear below the note. Want to circle the range? Or, underline it with a wavy line? With Tab, you can do them easily. Right click where you want the graphical shape to be drawn. Open the `Shape` menu and select a type. A shape should appear. You can use the mouse cursor to stretch it to a desired size or move it to a desired location. Is the circle (really an ellipse) too small? Stretch it to cover all of your interesting bytes. If you need to, you can rotate the shape, too. Move the cursor into the shape by a quarter length. The cursor turns into a rotation indicator. Hold down the mouse button and move the cursor up to rotate counterclockwise or down to rotate clockwise. Release the button to finalize the shape. Choose `Apply` or `OK` to keep the tags and shapes you have added. Next time the file is opened for properties, they are automatically pulled up and shown.

A keyboard person might prefer menu command `Add Tag` to the mouse method. You will be asked to supply a byte range and a comment.

![alt Add tag](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/ref/add%20tag.png)

Note that colored regions, attached notes, and drawn shapes are referred to as `meta` objects in Tab's programming.

### Scan for Tags

Tab has built-in support for detecting documented segments and structures of image data and automatically generating tags with descriptive information.

![alt Tab view after jpeg scan](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/ref/tab%20after%20jpeg%20scan.png)

The file types Tab can scan are __jpeg__, __jpg__, __png__, __bmp__, __gif__, __ico__, and __webp__.

Tab provides an API for extending the scan capability to other file types. To enable scan for a file type you want, you can write your own scan server. Refer to demo project [ScanSrvWebp](#ScanSrvWebP-Demo-Scan-Server) for the API details.

### Print, Copy, or Save

To send the hex output to a printer, use `Print`. Choose a printer from the list. Set the range of pages. If the output is overcrowded with tags making it difficult to see the data bytes, know that you have an option of grouping the tags in a column outside the data area. That may give you just a clarity you need. Also, you can use the Fit option to squeeze the hex output to fit the paper.

![alt Print page setup](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/ref/print%20page%20setup.png)

Want to insert hex output into a document you are writing? Use `Save As Image`. The menu command sends the hex dump to the system clipboard as an image object. You can paste it directly into your document. Or if you want it on a disk, you can save it as a PNG or BMP file, too.

![alt Save hexdump](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/ref/save%20hexdump.png)

### Standalone Viewer App

The window frame of Explorer's File Properties tends to be small. So, the Tab can only get a limited display area, and a view that it generates tends to be short and narrow. By default, the Tab outputs the hex digits in an 8-bytes-per-row format. Although it can be reconfigured to output more bytes per row, you'd need to either use the scrollbar or choose a smaller font to see all parts of the view. Tab gives you an alternative. You can run it as a  standalone viewer outside of Windows Explorer. Free from space constraint, the Tab runs in a much larger window frame, now able to output 16 or even 32 data bytes per row. To put it in wide-view mode, select `Open in New Window` from the context menu. Wide view defaults to a 16-bytes-per-row format. If you need to see even more bytes, choose the alternative 32-byte format. Aside from the larger display, the standalone viewer sports a toolbar. Use it to quickly access main features of the Tab.

![alt Tab after jpg scan](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/ref/standalone%20tab%20after%20jpg%20scan.png)

Actually, Tab can be launched from the Windows `Start` menu. Go to `Maximilian's Tools`. Select `Hex-Dump Viewer`. When prompted, choose a file you want to examine. The file's data is output in a wide format.

### Project Organization

Let's get technical. The project consists of four Visual Studio component projects plus a tool set for building an msi.

1) `hextab`, a Shell propertysheet extension,
2) `hexdlg`, a viewer app with a wide display,
3) `scansrvwebp`, a demo scan server for WebP images, and
4) `setup`, a self-extracting MSI installer.

Setup embeds both x64 and x86 msi installers. It picks and runs one appropriate for the system when it is launched.

The toolset for msi build consists of a template msi, a build staging batch process, and a script for generating database tables and packaging the tables and executable binaries in an msi. Refer to [build.md](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/installer/build.md) for info on how the project is built.

## Gettting Started

Prerequisites:

* Windows 7 or later.
* Visual Studio 2017 or newer. The Visual Studio solution and project files were generated using VS 2017. They can be imported by a newer version of Visual Studio.
* Windows SDK 10.0.17763. More recent SDKs should work, too, although no verification has been made.

The installer is available from [here]( ). Installation requires admin priviledges as it is a per-machine operation.

To build the installer, install [`Maximilian's Automation Utility`](https://github.com/mtanabe-sj/maximilians-automation-utility/tree/main/installer/out). The build process requires Utility's `MaxsUtilLib.VersionInfo` automation object. 

## Design and Implementation

### HEXTAB Shell Propertysheet Extention DLL

This is the main component project. It builds Tab as a Windows Shell extension. Tab consists of the components below with the C++ class or COM interface associations noted in parantheses.

* Communication with the host (_class PropsheetExtImpl_)
  * Propsheet extension (_interface IShellPropSheetExt_)
  * Custom host configuration (_interface IHexTabConfig_)
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
  * Scan server API (_class ScanTagExt, ScanDataImpl, interfaces IHexTabScanSite, IHexTabScanData_)
* Utility
  * Codec wrapper (_classes CodecImage, BitmapImage_)
  * String manipulation (_classes ustring, astring, bstring_)
  * List management (_classes simpleList, objList, strvalenum_)
  * Registry configuration (_functions Registry*_)
  * Logging (_EventLog_)

#### Communication with the host

Central to Tab is class _PropsheetExtImpl_. It creates and manages a custom Win32 property page that is bound to a system property sheet managed by Windows Shell. Thus, Tab appears as a tabbed page in a File Properties dialog run by Windows Explorer. The class implements _IShellPropSheetExt_ which enables communication with the host app (which is usually explorer.exe). _PropsheetExtImpl_ hosts a child window of _BinhexDumpWnd_ to manage view generation and interaction with an end user. _BinhexDumpWnd_ subclasses _BinhexMetaView_ by adding to it a capability for Win32 event handling and user interaction. _BinhexMetaView_ manages a layered meta view that renders meta objects on top of a hex view generated by base class _BinhexView_. A meta object is a colored region of data bytes with a text annotation called tag, or it is a free-flowing graphical shape a user can add to a part of the hex view. All meta objects added to a user file are serialized to arrays of fixed-length structures, and saved in a so-called meta file. A registry setting relates the meta file to the user file. Next time the user file is opened in File Properties, tha Tab locates the meta file through the registry, de-serializes the meta objects, and re-generates meta view. Class _persistMetafile_ is responsible for the serialization and de-serialization operations. The meta object structures are defined as follows:

```C++
struct DUMPREGIONINFO { LARGE_INTEGER DataOffset; ULONG DataLength; ...; UINT AnnotCtrlId; };
struct DUMPANNOTATIONINFO { WCHAR Text[256]; WCHAR TextEnd; BYTE Flags; ...; HBITMAP ThumbnailHandle; };
struct DUMPSHAPEINFO { BYTE Shape; BYTE Flags; USHORT StrokeThickness; ...; COLORREF InteriorColor; };
```

See _persistMetafile::save_ on how meta objects are serialized. This takes place when the `OK` or `Apply` button is selected. Loading or de-serialization of persisted meta objects happens automatically next time the Tab is opened for the same file. See the _persistMetafile::load_ method. A meta file that stores serialized meta objects is located in a subfolder of the per-user folder of _%LocalAppData%_. How does the load method know which meta file to read? The _save_ method generates a unique ID naming the meta file after it, and writes a named setting with the dumped file as the name and the meta file ID as the value in a per-user key of the system registry. The _load_ method makes a lookup for the dumped file in the registry and retrieves the meta file ID. Based on the file ID, it locates the meta file and de-serializes and loads the meta objects. The registry key that maintains the meta file mapping is _HKEY_CURRENT_USER\SOFTWARE\mtanabe\HexTab\MetaFiles_. Entries in the key look like this.

![alt Metafiles registry key](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/ref/metafiles%20registry%20key.png)

#### View Generation

During the startup, _BinhexDumpWnd::OnCreate_ initializes the view generator by creating fonts, obtaining the screen resolution, and text metrics, and preparing a dump buffer. _BinhexDumpWnd::OnPaint_ responds to system view refresh requests by calling the view generator's core method, _BinhexView::repaint_. _repaint_ through the _refreshInternal_ callbacks perform these display tasks in this order.
1) _BinhexMetaView_ reads source data.
2) _BinhexView_ outputs a hex dump and draws a row selector.
3) _BinhexMeataView_ paints the regions, annotations, and shapes.
4) _BinhexDumpView_ marks the search hit, if occuring in the current page.

Class _ROFile_ serves data bytes from the user file to the view generator. It wraps Win32 file management API. It uses structure _FILEREADINFO_ to buffer file data and track the current read offset. You can think of a pointer to a FILEREADINFO like a C file pointer of type FILE*. _BinhexDumpWnd_ subclasses _ROFile_ and uses the base class's methods to read the file.

#### UI Management

In a standard Win32 window application, defining and routing keyboard shortcut keys is straightforward. Add an accelerator table to the resource file (.rc) at design time, and call Win32 API function _TranslateAccelerator_ in a message pump at run time. Unfortunately, this simple model does not work for a shell extension like Tab. Tab does not own a message pump. Explorer does. Tab has no way to ask Explorer to adopt its keycode mappings and do keycode translation. To work around the shortcoming, Tab sets up a message hook and intercepts Win32 messages generated in Explorer's message loop. If the intercepted message is destined for Tab's main window (of _BinhexDumpWnd_), Tab calls _TranslateAccelerator_ generating a desired _WM_COMMAND_ message for itself. The accelerator loading and the message interception and translation are implemented in class _KeyboardAccel_. Another challenge Tab must deal with is this. Explorer is UI multi-threaded owning multiple message pumps. Multiple instances of Tab may coexist. So, to be able to bind to the right pump, Tab uses the Win32 thread local storage and saves the per-thread hook descriptors there. That way, each instance of Tab automatically hooks the right message pump, making sure that each shortcut key is directed to the right instance of Tab. The TLS management is implemented in class _TLSData_.  Tab maintains a singlton instance of _TLSData_ (refer to dllmain.cpp for the definition).

There are basically three sources of commands and messages that require Tab's attention. One is internal commands generated by context menus. Another is user-generated mouse events that reposition the cursor, scroll the page, or start and end drag and drop operations. There is a third kind. That's system-generated messages for routine windowing events like closing the app window. We won't go into the details for that since standard Win32 handling is all that's required.

_OnContextMenu_ of _BinhexDumpWnd_ prepares and runs a context menu on detecting a right-click. Depending on the type of an item clicked on, some menu commands are either grayed out or removed. But, all instances of the context menu derive from a single MENU resource definition (_IDR_VIEWPOPUP_). Here is an example menu that enables a color chooser submenu in response to a click on a colored region.

![alt Context menu](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/ref/context%20menu.png)

The menu commands and how they are processed are summarized below.

|Menu label|Command ID|Handler|Remarks|
|----------------------|----------------------|----------------------|---------------------------------------------------|
|Open in New Window|IDM_OPEN_NEW_WINDOW|OnOpenNewWindow|HEXDLG viewer app is run. Removed in HEXDLG.|
|Bytes per Line|IDM_BPL_(N)|OnSelectBPR(N)|IDM_BPL_8 and accompanying IDM_SAME_FONTSIZE are grayed in HEXDLG.|
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

Another important aspect of UI management is handling system events for drag and drop and page scroll actions. Tab handles these system messages.

|Message|Handler|Invoked Operations|
|---------------|-----------------|------------------------------------------------------------------|
|WM_LBUTTONDOWN|OnLButtonDown|1) Clear prior selection in ASCII column. 2) Start drag and drop on a shape object, or 3) Check annotation grab, or 4) Start a region drag to extend the selection.|
|WM_MOUSEMOVE|OnMouseMove|Button Down: 1) Continue shape drag, or 2) Continue annotation drag, or 3) Continue stretching the region.|
|||Button Up: 1) Grab a shape, or 2) Highlight shape at cursor, or 3) Highlight annotation at cursor.|
|WM_LBUTTONUP|OnLButtonUp|1) Stop ongoing drag operation and execute drop, or 2) Draw a box around hex number corresponding to ASCII column selection.|
|WM_VSCROLL|OnVScroll|The scroll page width is given by _vi.RowsPerPage_. Page-wise actions invoke startFadeoutDelay of AnnotationCollection to refresh with annotation fadeout.|
|WM_HSCROLL|OnHScroll|The scroll page width is given by _vi.ColsPerPage_. Horizontal scroll bar is visible only in 16 or 32 BPR. The offset column remains visible even when the hex and ASCII columns are shifted.|

Some of the commands metioned above engage in dialog interaction. They are _Keyword Search_, _Save as Image_, and _Print_.

_OnFindData_ for _Keyword Search_ uses dialog class `FindDlg`. Users can specify a keyword in one of the three encodings.

	* UTF-8 (FINDCONFIGFLAG_ENCODING_UTF8)
	* UTF-16 (FINDCONFIGFLAG_ENCODING_UTF16)
	* 2-digit hex (FINDCONFIGFLAG_ENCODING_RAW)

The dialog's _OnCommand_ calls _updateSearchConfig_ to save the keyword string in the selected encoding in the output structure of _FINDCONFIG_. _OnPaint_ draws the re-encoded keyword bytes as rows of 2-digit hex numbers as a visual feedback so that the user understands which binary byte value is being searched for. On return from the dialog, _searchAndShow_ calls _ROFile::search_ to search the file. If it finds a match in the current page, it simply refreshes the view. If the match is not in the current page, it lets _scrollToFileOffset_ scroll to the page and then resumes the search.

_OnSaveData_ for _Save as Image_ is another dialog-invoking command. It uses _class DataRangeDlg_ to interact with a user. The dialog class collects three settings, a destination medium, a data range, and a choice of grouping of annotations or not. The data range can be specified either in number of bytes or number of dump lines. To help the user see the effect of a change he makes before it is made permanent, the dialog uses _BinhexMetaPageView_, a subclass of _BinhexMetaView_, and renders a preview of what the changed hex dump would look like. The view generator subclass is capable of splitting the viewport into two sections, one section showing the usual hex numbers and ASCII text, and the other section showing a stack of annotations that belong to the data range in the current view. It uses the split viewport in response to selection of the _IDC_CHECK_SEGREGATE_ANNOTS_ option. When the split option is enabled, _BinhexMetaPageView_ generates a full-scale bitmap image first. Then, it reduces the image size to fit the preview rectangle. _DataRangeDlg_ keeps the full-scale bitmap in a member variable. _OnSaveData_ picks up the bitmap from the dialog. If the destination is a file, the method uses the bitmap helper class _BitmapImage_ to write the bitmap to a file in a user-supplied location. If the destination is the system clipboard, Win32 _SetClipboardData_ API is called with the bitmap.

Last but not least, there is the dialog-invoking command of _Print_. _OnPrintData_ responds to it. Tab maintains an instance of struct _HexdumpPageSetupParams_ throughout its life. The dialog class _HexdumpPrintDlg_ runs a page setup using setup parameters kept in the _HexdumpPageSetupParams_. On successful conclusion of the dialog, a print job is started by creating an instance of class _HexdumpPrintJob_. It's a worker running in a separate thread so as not to block the main UI thread. The worker renders the selected pages of hex dump in the device context of a selected printer. When it's done, the worker posts a notification _WM_COMMAND_ message (_IDM_PRINT_EVENT_NOTIF_ with task ID of TskFinishJob). _OnCommand_ of _BinhexDumpWnd_ responds to it by initiating a cleanup.

There are a few caveats you might want to be aware of when using the [PrintDlg API](https://docs.microsoft.com/en-us/windows/win32/api/commdlg/nc-commdlg-lpprinthookproc). There is the supposedly 'better' version called _PrintDlgEx_. With that API function, you are supposed to be able to take a smarter partial template approach rather than the approach imposed by _PrintDlg_. The latter forces you to import and edit the entire dialog template, a chore you won't miss. Unfortunately, I could not get _PrintDlgEx_ customization to work. If you know how, let me know.

To customize the PrintDlg UI, the _PRINTDLGORD_ dialog template was copied to and edited in res\lib.rc2. _PRINTDLGORD_ needs `dlgs.h` of the Windows SDK for control IDs. The most important aspect of the customization regarded the adding of a preview pane. It was neccessary to have a preview so Tab can give the user a feedback on a change he makes to the print range, margins, `Group Notes`, or `Fit to Page Width` before it's made permanent. Note that the preview pane (_IDC_STATIC_PAGE1_) is a static control with style _SS_OWNERDRAW_. _OnCommand_ responds to the control repaint event by invalidating the preview pane. _OnDrawItem_ responds to the refresh request by updating the preview output. The method uses class _BinhexMetaPageView_ to turn the starting or ending page into a bitmap image which is then scaled to fit between the margins.

_OnPrintData_ calls _HexdumpPrintDlg::startPrinting_ to print the selected pages. _startPrinting_ creates an instance of _HexdumpPrintJob_ passing a _PRINTDLG_ structure filled with a printer descriptor and page settings chosen in the setup dialog. _HexdumpPrintJob_ runs a worker on a new thread to process the print job. If the Cancel button is clicked, the host generates a _PSN_QUERYCANCEL_ invoking _propsheetQueryCancel_ of _BinhexDumpWnd_. On receiving a confirmation from the user, _BinhexDumpWnd_ aborts the print job by calling _HexdumpPrintJob::stop_. The _stop_ method raises the thread kill signal (_kill_). The print task loop of _HexdumpPrintJob::runWorker_ detects the raised _kill_ and terminates the worker. If the print job completes, _runWorker_ posts an IDM_PRINT_EVENT_NOTIF notification for status TskFinishJob to _BinhexDumpWnd_ which in turn calls _HexdumpPrintJob::stop_ to make sure the worker is terminated.

_HexdumpPrintJob_ takes advantage of device independence of GDI. Rendering to a printer in principle is same as displaying the page on a monitor. Both call method _BinhexView::repaint_. Unfortunately, when outputting to a printer, the method caused an issue, and so, an override had to be implemented. When a meta object was added, the object when drawn appeared displaced. For example, if a couple of hex numbers at the back of a dump row were selected as a colored region, the numbers drawn in color drifted away from the correct position. The displacement was slight but still significant. Why did the defect happen? Let's take a deeper look.

If you look at _drawHexDump_ and _finishDumpLine_ of _BinhexView_, you see that the data bytes are translated to hex numbers and ASCII characters which are then stored as a single string in a buffer. The whole string is then drawn in a single DrawText API call. That's how the base view of hex dump is rendered. This strategy is easy to implement and delivers a good performance. What about drawing of meta objects? Meta objects are collected into a view layer that lies on top of the hex dump view layer. It is handled slightly differently from how _dumpHexDump_ does it. _BinhexMetaView::repaintInternal_ which overrides the base method, handles the meta layer. The override invokes _drawRowBytes_ which still relies on DrawText for the actual paint operation. But, it passes a different rectangle to DrawText. The rectangle instructs the API where to start and end the regional drawing. Depending on the region, the rendition may not  start at origin. For our example case, it starts instead at the back of the data row. That's where the two hex numbers the region is associated with is. But then, doesn't the region's starting x-coordinate coincide with the x-coordinate of the leading number? Unfortunately, it does not in a printer context. Why? Well, it boils down to integer truncation. Here is the analysis.

Tab uses a 10-pt fixed-pitch Courier font to draw the hex numbers. In a 600-dpi printer context, that translates to an average character width of 83 dots according to font metrics data collected under a debugger. That means the true 10-pt width could be anywhere between 82.1 and 83.9 dots. The repaint method of _drawRowBytes_ bases the text rectangle calculation on the character width. If the true 10-pt character width was 83.9 pixels but was rounded down to 83 due to truncation in a floating point-to-integer conversion, that would allow an error factor of up to 1/83 = 1.2% to creep in. That may not be a lot. But, inaccuracy increases as our point of interest moves closer to the right edge of the view. In our example, the point of interest is at the back of the data row which is at 3984 pixels away from the left edge of the view. 83 dots per character times 3 characters (2 for the two hex digits of the dump byte plus 1 for a separation space character) per data byte times 16 data bytes per row yields 3984 dots per row. So, the colored region should start at 3486 dots away from the left edge. If the truncation error was indeed 1.2%, the region in a perfectly accurate context would start at 3486 * 1.012 = 3527 dot. Then, the displacement of the region expressed in units of character widths can be calculated as (3527-3486)/83 = 0.49. That's half the character width. 

As described above, the base repaint method draws the entire row of 16 hex numbers including the 2 trailling ones in a single call to _DrawText_. It renders the row starting at an x-coordinate origin of 0. In this case, the two trailling numbers are seem to be drawn starting at or close to the true point of 3527 dots from the origin. But, when the trailling numbers are marked as a colored region, they are displaced by an amount similar to what was predicted by the above analysis.

The abovementioned metrics problem is addressed this way.

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

Meta objects refer to the following constructs:
* data range definitions called colored regions,
* annotations attached to specific data bytes, and
* graphical shapes attached to specific data bytes.

They are logical objects created and managed by Tab. They are stored in a data file managed by _persistMetafile_. Tab uses the expression, `tag`, to refer to a colored region with an annotation.

Meta objects are managed by the three collection classes which all derive from _simpleList<T>_. _BinhexDumpWnd_, the main window of the Tab, owns the meta object collections, and uses _persistMetafile_ to persist them across processes that load the Tab.

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

In processing user-issued commands or responding to drag and drop actions, _BinhexDumpWnd_ operates on member objects of the meta object collections using operational methods of _CRegionOperation_, _AnnotationOperation_, or _GShapeOperation_ or their subclasses.


#### Tag Scan

For popular image types, Tab can scan the file data for documented data segments and structures and mark them in colors and annotate with descriptive text. A thumbnail image based on the source image data is generated and placed in an annotation attached to the image data section.

Tab natively supports images of `JPEG`, `PNG`, `GIF`, `BMP`, and `ICO`. It deploys classes _ScanTagJpg_, _ScanTagPng_, _ScanTagGif_, _ScanTagBmp_, or _ScanTagIco_, respectively, in response to the image file's extension name. See _BinhexDumpWnd::OnScanTagStart_. ScanTag and its subclasses use _MetadataExif_ and other parser classes to parse the well-known metadata of `Exif`, `XMP`, `ICC Profile`, and `Adobe Photoshop`. 

What about other image types? What about non-image files? To address such needs, the Tab provides a scan server API consisting of headers of COM interface deinitions and registration information. There is a demo server made available to give a quick jump start to those interested in expanding Tab's scan capacity. Refer to the demo project for the details.

Tab uses implements a scan server hosting site in class _ScanTagExt_. _OnScanTagStart_ calls the static _ScanTagExt::CanScanFile_ method to determine if there is an external scan server ready for the input file. If there is, the method runs an instance of _ScanTagExt_. _ScanTagExt::runInternal_ instantiates the server, and initializes the server passing an _IHexDumpScanSite_ object (actually, _this_ of the _ScanTagExt_ instance) and an _IHexDumpScanData_ object. Scan servers use methods on _IHexDumpScanSite_ to access host services and methods on _IHexDumpScanData_ to access data of the source file being scanned.


### HEXDLG Viewer Application
	
This component project builds a standalone propsheet viewer, an alternative to Explorer's _File Properties_. It features a wide hex-dump format and an easy-to-use command tool bar. The viewer may be started from the _Windows Start_ menu. Open the _Maximilian's Tools_ popup and select _Hex-Dump Viewer_.
	
The viewer consists of these components.

* Command line parser (function parseCommandline)
* Window management
	* Propsheet hosting (class PropsheetDlg)
	* Toolbar management (class ToolbarWindow)
* Communication with the tab (interfaces IShellPropSheetExt and IHexDumpConfig)

If you are interested in how Explorer loads a propsheet extension, check out _PropsheetDlg::createShellPropsheetExt_. The viewer app uses the customization interface _IHexTabConfig_ to configure its tool bar with private information from Tab.


### ScanSrvWebP Demo Scan Server

As explained in Section [Tag Scan](#Tag-Scan), Tab lets you add an external scan server. If you are interested in writing one, read the notes below, see how the demo does it first, and then, modify it to suite your needs.

The demo server supports `WebP`. WebP is well documented by Google who developed it in [this article](https://developers.google.com/speed/webp).

The basic formatting element WebP uses is the _chunk_ of _RIFF_. It is a generalized structure to package a piece of payload data, its size in bytes, and a textual code uniquely identifying the data. A file in WebP starts with a WebP file header of 12 bytes.

012345678901
RIFF****WEBP

where the **** 4-byte sequence is the file size in bytes. In its simplest form, WebP has two components, a WebP file header followed by a VP8 chunck for lossy compression or a VP8L chunk for lossless compression. Usually, however, WebP uses an extended format called VP8X. 

In terms of data structure, WebP is much like PNG. It's composed with well-defined data blocks, a.k.a., chunks. Parsing of WebP data is similer to parsing of PNG.
	
#### Server Registration

Since a scan server is a COM server, standard registration with COM is made in key HKCR\CLSID\{Server-CLSID}. The demo server has a CLSID of {6ccda86f-3c63-11ed-9d82-00155d938f70}.

![alt Scan server clsid registration](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/ref/scan%20server%20clsid%20registration.png)
	
To let Tab know the server's availability, the scan server makes these settings under HKLM.
ScanServers: .webp --> {Server-CLSID}
ExtGroups: .webp --> image

The above configuration settings instruct Tab to deploy the server if the file has an extension of .webp and find the COM server at the specified CLSID. When it runs a `File Open` dialog, the Tab adds the extension name (.webp) from the registry key to the file extensions of the _image_ group so that if the user restricts file listing to images, files of .webp are listed as well.

![alt Scan server registration](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/ref/scan%20server%20registration.png)

![alt Scan server ext-group registration](https://github.com/mtanabe-sj/Maximilians-Hex-Tab/blob/main/ref/scan%20server%20ext-group%20registration.png)
	
#### Interface Implementation

Tab defines three interfaces, one for a scan server, one for a scan site, and the other for a scan data. The scan server interface is implemented and exposed by a scan server. Tab which is a client of the scan server hosts the server and invokes the server's scan function through the interface.
	
* IHexTabScanServer
* IHexTabScanSite
* IHexTabScanData

Scan servers implement scan server interface _IHexTabScanServer_. Tab implements the other two. Scan servers use the data interface to read and search the file data. Servers can also use it to ask the Tab to parse metadata on its behalf.

A scan server may want to access services of the Tab as well as attributes of the dump file. It can do so through the scan site interface. Tab implements the site interface on its _ScanTagExt_ object. It passes a pointer to the interface to the server when it calls the server's _Init_ method to give the server a chance to initialize itself based on the file attributes available from the data and site interfaces the server receives.

```C++
DECLARE_INTERFACE_(IHexTabScanData, IUnknown)
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

The server uses methods _TagData_ and _Annotate_ of the site interface to mark a block of scanned data as a colored region with a description, and attach a descriptive note or bitmap image to a data byte, respectively.
	
```C++
DECLARE_INTERFACE_(IHexTabScanSite, IUnknown)
{
	...
	STDMETHOD(GetVersion)(THIS_ ULONG *SiteVersion) PURE;
	STDMETHOD(GetState) (THIS_ SCANSITE_STATE *SiteState) PURE;
	STDMETHOD(GetFileName)(THIS_ BSTR *FileName) PURE;
	STDMETHOD(TagData)(THIS_ USHORT Flags, LONGLONG TagOffset, LONG TagLength, BSTR TagNote, SHORT ParentIndex, SHORT *TagIndex) PURE;
	STDMETHOD(Annotate)(THIS_ LONG TagIndex, VARIANT *SourceData) PURE;
};
```

Tab calls _Init_ to initialize the scan server, _Scan_ to start a tag scan, and _Term_ to terminate the server. If it receives a fail code from the methods, the Tab calls _GetErrorMessage_ to retrieve a corresponding error message.
	
```C++
DECLARE_INTERFACE_(IHexTabScanServer, IUnknown)
{
	...
	STDMETHOD(Init) (THIS_ IHexTabScanSite *ScanSite, IHexTabScanData *SourceData) PURE;
	STDMETHOD(Term) (THIS) PURE;
	STDMETHOD(Scan) (THIS) PURE;
	STDMETHOD(GetErrorMessage) (THIS_ BSTR *Message) PURE;
};
```

Let's examine how the demo project implements a scan server. Tab communicates with a scan server through the latter's IHexDumpScanServer interface. The project defines class _ScanServerImpl_ to expose _IHexDumpScanServer_. Note that the Tab can release the server interface any time after it calls the server's _Term_ method.

```C++
class ScanServerImpl :
	public IUnknownImpl<IHexTabScanServer, &IID_IHexTabScanServer>
{
public:
	// IHexDumpScanServer methods
	STDMETHOD(Init)(IHexTabScanSite *ScanSite, IHexTabScanData *SourceData);
	STDMETHOD(Term)();
	STDMETHOD(Scan)();
	STDMETHOD(GetErrorMessage) (BSTR *Message);

protected:
	AutoComRel<IHexTabScanSite> _site;
	AutoComRel<IHexTabScanData> _data;
	bstring _msg;

	struct CHUNK_RIFF
	{
		FOURCC ChunkId;
		DWORD ChunkSize;
		BYTE Payload[1];
	};
	...
};
```

The class defines _AutoComRel_ smart pointers as member objects to maintain references on a scan site interface and scan data interface that are passed in when the Tab calls its _Init_ method. The interfaces are released when the _Term_ method is called. The class also defines a BSTR wrapper object _msg_ to hold an error phrase to be set by _Scan_ if the scan operation fails.

```C++
STDMETHODIMP ScanServerImpl::Init(IHexTabScanSite *ScanSite, IHexTabScanData *SourceData)
{
	_site = ScanSite;
	_data = SourceData;
	return S_OK;
}

STDMETHODIMP ScanServerImpl::Term()
{
	_site.release();
	_data.release();
	return S_OK;
}
```

Tab calls interface method _Scan_ to run a scan for tags. The method blocks. It does not return until the job complete. This is not a problem for Tab since the latter runs the scan operation in a worker thread. The demo shows how to use the data interface to seek and read source bytes from the input file and use the site interface to generate and attach tags to specific data locations. Below is a rendition of _Scan_ abridged for readability's sake. If _Scan_ returns a failure value, the Tab comes back to the interface and calls _GetErrorMessage_ for a descriptive error phrase so that it can alert the user.

```C++
STDMETHODIMP ScanServerImpl::Scan()
{
	CHUNK_HEADER hdr;
	ULONG len = sizeof(hdr);
	HRESULT hr = _data->Read(&hdr, &len);
	...
	LARGE_INTEGER offset = { 0 };
	short tagIndex;
	hr = _site->TagData(0, offset.QuadPart, len, bstring(L"WebP header"), 0, &tagIndex);

	offset.QuadPart += len;

	CHUNK_RIFF chunk;
	len = FIELD_OFFSET(CHUNK_RIFF, Payload);
	while (S_OK == (hr = _data->Read(&chunk, &len)))
	{
		hr = _site->TagData(0, offset.QuadPart, FIELD_OFFSET(CHUNK_RIFF, Payload), bstring(L"Next Chunk"), 0, &tagIndex);
		seekLen = (LONG)chunk.ChunkSize;

		if (seekLen)
			hr = _data->Seek(seekLen, SSEEK_ORIGIN_CUR);

		offset.QuadPart += FIELD_OFFSET(CHUNK_RIFF, Payload) + chunk.ChunkSize;
	}
	return hr;
}

STDMETHODIMP ScanServerImpl::GetErrorMessage(BSTR *Message)
{
	*Message = SysAllocString(_msg);
	return S_OK;
}
```

A scan server can use method _IHexTabScanSite::Annotate_ to annotate a data byte with descriptive text or a bitmap image. The demo uses the method to attach a WebP logo image to the _WebP Header_ structure.


## Contributing

Bug reports and enhancement requests are welcome.

	
## License

Copyright (c) Makoto Tanabe, All rights reserved.

Maximilian's Hex Tab is distributed under the terms of the MIT License.
The libheic and associated libraries are distributed under the terms of the GNU license.
