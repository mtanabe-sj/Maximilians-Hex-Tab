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

#define PRODUCTLANGUAGE  0x409
#define PRODUCTCODEPAGE  1200
#define STRPRODUCTLANGCP "040904b0"

#define LIB_VERMAJOR 1
#define LIB_VERMINOR 0
#define LIB_REVISION 0
#define LIB_BUILD 1

#define FILEVER			1,0,0,1
#define PRODUCTVER		1,0,0,1
#define STRFILEVER		"1,0,0,1\0"
#define STRPRODUCTVER	"1, 0, 0, 1\0"

#define STRPRODUCTNAME	"Maximilian's Hex Tab\0"
#define STRPRODUCTDESC	"A hex-dump utility as a Windows Shell property page\0"
#define STRCOMPANYNAME			"mtanabe\0"
#define STRLEGALCOPYRIGHT		"Copyright © 2023\0"
#define STRLEGALTRADEMARK		"HexTab\0"
#define STRCOMMENTS         "\0"
#define STRPRIVATEBUILD		"\0"
#define STRSPECIALBUILD		"\0"

///////////////////////////////////////////////////////////////
// registry key of the product
#define LIBREGKEY L"Software\\mtanabe\\HexTab"
#define LIBREGKEY2 L"Software\\mtanabe\\HexTab\\dlg"

// propertysheet handler clsid
#define WSZ_CLSID_MyShellPropsheetExt L"{33f95b01-9721-11d3-BA5D-00104BC64064}"
DEFINE_GUID(CLSID_MyShellPropsheetExt, 0x33f95b01, 0x9721, 0x11d3, 0xba, 0x5d, 0x0, 0x10, 0x4b, 0xc6, 0x40, 0x64);
#define WSZ_NAME_MyShellPropsheetExt L"Maximilian's Hex Tab"

// version number assigned to metadata files
const USHORT PMF_VERSION_NUM = 0x0100;
