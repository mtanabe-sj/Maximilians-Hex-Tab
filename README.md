# Maximilian's File Hex-Dump Tab for Windows Explorer

Maximilian's File Hex-Dump Tab is a Windows Explorer add-on written in C++. The Tab is much more than a simple dump utility. With it, not only can you quickly inspect the binary content of any file directly in Windows Explorer, you can also easily annotate, color and circle parts of the file data meaningful to you. For image files like JPEG, the Tab can scan the entire file, and automatically tag well-known data segments and structures. If you want, you can help us extend the Tab's scan and tag feature to other file types by writing a scan server. Refer to the demo project for how. The Tab is a full-featured Windows app. After the file is marked and commented, tell the Tab to send it to a printer for a paper printout, or save it as a bitmap image file on the disk. Or, copy and paste it to a report you are writing. The convenience features are only a click away.


## Features

Let's give you a functional overview first. Hopefull you develop a good idea on what the Tab can do. Later on, we will get to more technical stuff.

### Summary

* Hex view generator
o Small screen format
o Large screen format
o Keyword search
o ASCII to hex digits
* Meta objects
o Regions and annotations as tags
o Image annotation
o Large-text annotation
o Free-flowing shapes
  o Translation and rotation
  o Custom cursors
o Meta data persistence
* Tag scan
o built-in support
  o Thumbnail
  o Metadata parser: Exif, XMP, ICCP, Photoshop
  o BOM, Unicode, UTF-8
o External scan server
* Utility
o Page setup and print
o Save as bitmap image
o Copy to clipboard

### Windows Explorer Integration

The Tab is an add-on for Windows Explorer, which makes its hex-dump service easily accessible to users of Explorer. To see the Tab in action, open a folder window, select a file you want to examine. Open File Properties from the context menu. Hit the Hex Dump tab. Want to jump to hex digits you know? Use `Find Data` to search and be taken there. Want to know which pair of hex digits corresponds to an ASCII character in the right-hand column? Just click on the character. A red box is drawn to highlight the digit pair in the hex-dump area.

![alt Tab view - circle color and annotate](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/tab%20view%20-%20circle%2C%20color%20and%20annotate.png)

The Tab shows the file offset in the left-hand column, the hex digits of source bytes in the middle column, and the source bytes as ASCII characters in the right-hand column. If the file has text encoded in Unicode or UTF-8, run a scan. The Tab shows the text as Unicode characters in the right-hand column, instead.

Is the default 8-byte-per-row output format too small? Use menu command `Open in New Window` to open the file in a separate window with a larger display and show the hex output in a 16-byte-per-row format. The larger window comes with a tool bar of command buttons for your convenience, too.

![alt Tab view - wide display](https://github.com/mtanabe-sj/Maximilians-Hex-Dump-Tab/blob/main/ref/standalone%20tab.png)

### Colors, Tags and Shapes to Mark Data Blocks

Suppose you find interesting bytes in the hex output. You want to mark them, but how? Easy. Click on the starting byte, and drag to extend the selection of byte range. The bytes are automatically filled with color. Next, add a note of description. Need a picture with the note? No problem. Copy the source image onto the clipboard. Then, just paste it into the note. Want to circle the range? Or, underline it with a wavy line? With the Tab, you can do them, too. They are on the Shape menu. Is the circle (more like an ellipse) too small? Stretch it to cover all of the interesting bytes. If you need to, you can rotate the shape, too. Choose Apply or OK to keep the tags and shapes you added. Next time the file in opened for properties, they are automatically pulled up and shown.

Note that colored regions, notes, and shapes are referred to as `meta` objects in the Tab project.

### Scan for Tags

The Tab has built-in support for detecting documented segments and structures of image data and automatically placing tags with descriptive information.


The file types the Tab can scan are jpeg, jpg, png, bmp, gif, ico, and webp.

The Tab provides an API for extending scan to other file types. To enable scan for a file type you want, you can write your own scan server. Refer to demo project ScanSrvWebp for the [API details]().


### Print, Copy, or Save

To send the hex output to a printer, use `Print`. Choose a printer from the list. Set the range of pages. If the output is too crowded with tags making it difficult to distinguish one pair of hex digits from others, know you have an option of grouping the tags in a column. That may give you a clarity you need. Use the Fit option to squeeze the hex output to fit the paper.

Want to add the hex output to a document? Use `Save As Image`. That lets you copy it to the system clipboard. Paste it directly into your document. Or if you want it on a disk, you can save it as a PNG or BMP file, too.


### Standalone Viewer

Explorer's File Properties is compact in size. So, it can give the Tab only a limited real estate, and the view that can be generated is short and narrow. By default, the Tab outputs the hex digits in an 8-bytes-per-row format. It can output more bytes per row. But, you'd need to either use the scrollbar or choose a smaller font to see the digits you want. As an alternative, you can opt for the standalone viewer. That way, you don't have to live with the size constraint. Select `Open in New Window` from the context menu. The hex digits are now output in a 16-bytes-per-row format. If you need to see even more digits, you can choose the 32-bytes format. The standalone viewer sports a toolbar. Use it to quickly access main features of the Tab.

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

This is the main component project. It builds the Tab shell extension. The Tab consists of these components.

communication with host (explorer)
 propsheet extension
 custom host configuration
view generation
ui management and command invocation
 thread local storage
 keyboard accelerator
 keyword search
 print/page setup
 page save/copy
meta object management
 region
 annotation
 shape
 bom and unicode
 persistence
tag scan
 image support
 metadata parser
  exif, xmp, iccp, photoshop
 scan server api
  IHexDumpScanData
  IHexDumpScanSite
utility
 codec wrapper
 string manipulation
 space allocation
 list management
 logging


### HEXDUMPDLG Viewer Application

command line parser
window management
 propsheet hosting
 toolbar management
communication with the tab


### ScanSrvWebP Demo Scan Server

ScanServerImpl
 IHexDumpScanServer
 IHexDumpScanData
 IHexDumpScanSite


## Contributing

Bug reports and enhancement requests are welcome.



## License

Copyright (c) mtanabe, All rights reserved.

Maximilian's File Hex-Dump Tab is distributed under the terms of the MIT License.
The libheic and associated libraries are distributed under the terms of the GNU license.
