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
// setup.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "setup.h"
#include "CommandArgs.h"


HINSTANCE _hInstance;


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	int res;
	
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(nCmdShow);

	_hInstance = hInstance;

	CommandArgs cargs;
	res = cargs.parse(lpCmdLine);
	if (res == ERROR_SUCCESS)
	{
		AppSetup setup;
		res = setup.init(&cargs);
		if (res == ERROR_SUCCESS)
		{
			res = setup.exportMSI();
			if (res == ERROR_SUCCESS)
				res = setup.runMSI();
		}
	}
	return res;
}


////////////////////////////////////////////////////////////////

int AppSetup::init(CommandArgs *cargs)
{
	CommandArg *opt;
	WCHAR buf[MAX_PATH];

#ifdef _DEBUG
	// give you a chance to hook to the runtime instance for debugging purposes.
	MessageBox(NULL, L"debug init", L"HexDumpTab Setup", MB_OK | MB_SYSTEMMODAL);
#endif//_DEBUG

	// build a temp path for the log file.
	GetTempPath(ARRAYSIZE(buf), buf);
	Pathname_AppendFileTitle(buf, SETUP_SETUP_LOG);
	_log._path.assign(buf);
	// delete an old log.
	DeleteFile(_log._path);
	// is the logging option enabled? _logLevel will be 0 if logging is not wanted, or 1 if it's desired. greater level numbers are possible. but levels higher than 1 are not currently used.
	opt = cargs->getOption('l');
	if (opt)
	{
		// find the logging level.
		if (opt->_setting)
			_log._logLevel = opt->_setting->_buffer[0] - '0';
		else
			_log._logLevel = 1;
	}
	else // no explicit flag. let's use the standard logging.
		_log._logLevel = 1;
	// determine if we are running on a 64-bit machine.
	_wow64 = IsWow64();
	_log.writef(L"AppSetup::init: WOW64=%d", _wow64);

	// generate a path to the system's MSI installer.
	GetSystemDirectory(buf, ARRAYSIZE(buf));
	Pathname_AppendFileTitle(buf, L"msiexec.exe");
	_installerPath.assign(buf);
	_log.writef(L"_installerPath: '%s'", buf);

	// need a temp location we will copy our MSI image to. msiexec will run our MSI from there.
	GetTempPath(ARRAYSIZE(buf), buf);
	Pathname_AppendFileTitle(buf, _wow64 ? SETUP_MSI64_NAME : SETUP_MSI86_NAME);
	_msiPath.assign(buf);
	_log.writef(L"_msiPath: '%s'", buf);

	// finally, make up a log file in the user's %TMP%.
	GetTempPath(ARRAYSIZE(buf), buf);
	Pathname_AppendFileTitle(buf, SETUP_MSI_LOG);
	_msiLogPath.assign(buf);
	_log.writef(L"_msiLogPath: '%s'", buf);
	// delete an older log file.
	DeleteFile(_msiLogPath);

	// does the user want to pass property assignments to the MSI?
	opt = cargs->getOption('p');
	if (opt && opt->_setting)
		_userProps.assign(*opt->_setting);
	_log.writef(L"_userProps: '%s'", (LPCWSTR)_userProps);

	// check the quiet level
	opt = cargs->getOption('q');
	if (opt)
	{
		// possible setting: n=No UI, f=Full UI, b=basic UI, r=reduced UI
		if (opt->_setting)
			_quietLevel.format(L"/q%s", *opt->_setting);
		else
			_quietLevel = L"/passive"; // most quiet except for progress display
	}
	_log.writef(L"_quietLevel: '%s'", (LPCWSTR)_quietLevel);

	// we're ready to go.
	return ERROR_SUCCESS;
}

// copy an appropriate MSI image as a resource to a temp file.
int AppSetup::exportMSI()
{
	return exportResoure(L"MSIDB", _wow64 ? IDR_MSI64 : IDR_MSI86, _msiPath);
}

// extract a file image from a resource. save it as a file at path.
int AppSetup::exportResoure(LPCWSTR resourceType, UINT datafileId, LPCWSTR path)
{
	DWORD ret;
	HRSRC hRes;
	HGLOBAL hMem;
	DWORD cb, cbWritten;
	LPBYTE pb;
	HANDLE hf;

	_log.writef(L"AppSetup::exportResoure started: %s; %d; '%s'", resourceType, datafileId, path);
	hRes = FindResourceEx(_hInstance, resourceType, MAKEINTRESOURCE(datafileId), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
	if (hRes)
	{
		cb = SizeofResource(_hInstance, hRes);
		hMem = LoadResource(_hInstance, hRes);
		pb = (LPBYTE)LockResource(hMem);
		hf = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hf)
		{
			if (WriteFile(hf, pb, cb, &cbWritten, NULL))
				ret = ERROR_SUCCESS;
			else
				ret = GetLastError();
			CloseHandle(hf);
		}
		else
			ret = GetLastError();
	}
	else
		ret = GetLastError();
	_log.writef(L"AppSetup::exportResoure stopped: RES=%d; FileSize=%d", ret, cb);
	return ret;
}

// execute an MSI installation using ShellExecute. wait until the process completes. pick up and return the exit code from the process.
int AppSetup::runMSI()
{
	DWORD nRes;
	SHELLEXECUTEINFO sei = { 0 };
	int i;
	ustring logOption;
	ustring cmdln;

	_log.write(L"AppSetup::runMSI started");
	if (_log._logLevel)
		logOption.format(L" /lv \"%s\"", (LPCWSTR)_msiLogPath);
	cmdln.format(L"/i \"%s\" %s %s%s", (LPCWSTR)_msiPath, (LPCWSTR)_quietLevel, (LPCWSTR)_userProps, (LPCWSTR)logOption);

	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_WAITFORINPUTIDLE;
	sei.nShow = SW_SHOWNORMAL;
	sei.lpFile = _installerPath;
	sei.lpParameters = cmdln;
	_log.writef(L"File: '%s'", sei.lpFile, false);
	_log.writef(L"Parameters: '%s'", sei.lpParameters, false);
	if (ShellExecuteEx(&sei))
	{
		nRes = ERROR_GEN_FAILURE;
		i = 0;
		if (sei.hProcess)
		{
			while (WAIT_TIMEOUT == WaitForSingleObject(sei.hProcess, 1000))
			{
				i++;
			}
			/* get the exit code from the msi process.
			it's usually one of these two success codes.
			- ERROR_SUCCESS_REBOOT_INITIATED
			- ERROR_SUCCESS_REBOOT_REQUIRED
			*/
			if (!GetExitCodeProcess(sei.hProcess, &nRes))
				nRes = ERROR_READ_FAULT;
			CloseHandle(sei.hProcess);
		}
		/*// thumheic's com registration has already sent out an association change notification. so, this is redundant and not necessary.
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
		*/
	}
	else
		nRes = GetLastError();
	_log.writef(L"AppSetup::runMSI stopped: RES=%d", nRes);
	return nRes;
}

void AppSetup::cleanup()
{
	// delete the MSI installation database file if an installation was run.
	if (!_msiPath.empty())
	{
		DeleteFile(_msiPath);
		_msiPath.clear();
	}
}


