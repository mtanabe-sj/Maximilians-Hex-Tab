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
// hexdumpdlg.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "hexdlg.h"
#include "helper.h"
#include "PropsheetDlg.h"


// this embeds a manifest for using v6 common controls of win32.
// it is needed for a SysLink control used in the about box.
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// global variables
HMODULE AppInstanceHandle;
ULONG LibRefCount;
#ifdef _DEBUG
EventLog evlog;
#endif//#ifdef _DEBUG

bool parseCommandline(LPCWSTR cmdline, objList<ustring> &paths, ULONG &outflags);

//////////////////////////////////////////////////////////////////////////

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	AppInstanceHandle = hInstance;
	LibRefCount = 0;

	ULONG flags = 0;
	objList<ustring> args;
	if (!parseCommandline(lpCmdLine, args, flags))
		return ERROR_INVALID_PARAMETER;

	PropsheetDlg dlg;
	if (!dlg.Open(args))
	{
		return ERROR_FILE_NOT_FOUND;
	}
	dlg.Run();
	dlg.Close();
	return ERROR_SUCCESS;
}

bool parseCommandline(LPCWSTR cmdline, objList<ustring> &args, ULONG &outflags)
{
	strvalenum senum(' ', cmdline);
	LPVOID pos = senum.getHeadPosition();
	while (pos)
	{
		LPCWSTR p = senum.getNext(pos);
		if (*p == '-')
		{
			//TODO: match p to option switches
			return false;
		}
		else
			args.add(new ustring(p));
	}
	return true;
}

