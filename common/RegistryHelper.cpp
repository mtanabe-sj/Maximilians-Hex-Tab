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
#include "RegistryHelper.h"


LONG Registry_CreateComClsidKey(LPCTSTR pszClsid, LPCTSTR pszHandlerName)
{
	TCHAR regKey[256];
	TCHAR valBuf[256];
	LONG res;
	swprintf_s(regKey, ARRAYSIZE(regKey), L"CLSID\\%s", pszClsid);
	res = Registry_CreateNameValue(HKEY_CLASSES_ROOT, regKey, NULL, pszHandlerName);
	if (res == ERROR_SUCCESS)
	{
		swprintf_s(regKey, ARRAYSIZE(regKey), L"CLSID\\%s\\InProcServer32", pszClsid);
		GetModuleFileName(LibInstanceHandle, valBuf, ARRAYSIZE(valBuf));
		res = Registry_CreateNameValue(HKEY_CLASSES_ROOT, regKey, NULL, valBuf);
		if (res == ERROR_SUCCESS)
		{
			res = Registry_CreateNameValue(HKEY_CLASSES_ROOT, regKey, L"ThreadingModel", L"Apartment");
			if (res == ERROR_SUCCESS)
			{
				swprintf_s(regKey, ARRAYSIZE(regKey), L"CLSID\\%s\\DefaultIcon", pszClsid);
				wcscat_s(valBuf, ARRAYSIZE(valBuf), L",0");
				res = Registry_CreateNameValue(HKEY_CLASSES_ROOT, regKey, NULL, valBuf);
			}
		}
	}
	return res;
}

LONG Registry_CreateNameValue(HKEY hkeyRoot, LPCTSTR pszKey, LPCTSTR pszName, LPCTSTR pszValue)
{
	LONG res;
	HKEY hkey;
	DWORD dwDisposition;

	res = RegCreateKeyEx(hkeyRoot, pszKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkey, &dwDisposition);
	if (res == ERROR_SUCCESS)
	{
		res = RegSetValueEx(hkey, pszName, NULL, REG_SZ, (LPBYTE)pszValue, ((DWORD)wcslen(pszValue) + 1) * sizeof(TCHAR));
		RegCloseKey(hkey);
	}
	return res;
}

#ifndef REGHELP_EXCLUDES_DELETESUBKEY
LONG Registry_DeleteSubkey(HKEY hkeyRoot, LPCTSTR pszBaseKey, LPCTSTR pszSubKey)
{
	LONG res;
	HKEY hkey;

	res = RegOpenKeyEx(hkeyRoot, pszBaseKey, 0, KEY_ALL_ACCESS, &hkey);
	if (res == ERROR_SUCCESS)
	{
		res = SHDeleteKey(hkey, pszSubKey);
		RegCloseKey(hkey);
	}
	return res;
}
#endif//#ifndef REGHELP_EXCLUDES_DELETESUBKEY

LONG Registry_DeleteValue(HKEY hkeyRoot, LPCTSTR pszBaseKey, LPCTSTR pszValueName)
{
	LONG res;
	HKEY hkey;

	res = RegOpenKeyEx(hkeyRoot, pszBaseKey, 0, KEY_WRITE, &hkey);
	if (res == ERROR_SUCCESS)
	{
		res = RegDeleteValue(hkey, pszValueName);
		RegCloseKey(hkey);
	}
	return res;
}

LONG Registry_SetDwordValue(HKEY hkeyRoot, LPCTSTR pszKey, LPCTSTR pszName, DWORD dwValue)
{
	LONG res;
	HKEY hkey;
	DWORD dwDisposition;

	res = RegCreateKeyEx(hkeyRoot, pszKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkey, &dwDisposition);
	if (res == ERROR_SUCCESS)
	{
		res = RegSetValueEx(hkey, pszName, NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
		RegCloseKey(hkey);
	}
	return res;
}

LONG Registry_GetDwordValue(HKEY hkeyRoot, LPCTSTR pszKey, LPCTSTR pszName, DWORD* pdwValue)
{
	LONG res;
	HKEY hkey;

	res = RegOpenKeyEx(hkeyRoot, pszKey, 0, KEY_QUERY_VALUE, &hkey);
	if (res == ERROR_SUCCESS)
	{
		DWORD dwType = REG_DWORD;
		DWORD dwLength = sizeof(DWORD);
		res = RegQueryValueEx(hkey, pszName, NULL, &dwType, (LPBYTE)pdwValue, &dwLength);
		RegCloseKey(hkey);
	}
	return res;
}

LONG Registry_GetStringValue(HKEY hkeyRoot, LPCTSTR pszKey, LPCTSTR pszName, ustring &strValue)
{
	LONG res;
	HKEY hkey;

	res = RegOpenKeyEx(hkeyRoot, pszKey, 0, KEY_QUERY_VALUE, &hkey);
	if (res == ERROR_SUCCESS)
	{
		LPWSTR pszValue = strValue.alloc(MAX_PATH);
		DWORD dwType = REG_SZ;
		DWORD dwLength = MAX_PATH*sizeof(WCHAR);
		res = RegQueryValueEx(hkey, pszName, NULL, &dwType, (LPBYTE)pszValue, &dwLength);
		if (res == ERROR_SUCCESS)
			strValue._length = dwLength / sizeof(WCHAR);
		RegCloseKey(hkey);
	}
	return res;
}

LONG Registry_EnumPropEntries(HKEY hkeyRoot, LPCTSTR pszKey, simpleList<propNameValue> &outList, int maxNames/*=0x100*/)
{
	LONG res, status;
	HKEY hkey;

	res = RegOpenKeyEx(hkeyRoot, pszKey, 0, KEY_QUERY_VALUE, &hkey);
	if (res == ERROR_SUCCESS)
	{
		for (int i = 0; i < maxNames; i++)
		{
			propNameValue nv = { 0 };
			DWORD ccName = ARRAYSIZE(nv.Name);
			DWORD dwType = 0;
			DWORD cbValue = sizeof(nv.Value);
			status = RegEnumValue(hkey, i, nv.Name, &ccName, NULL, &dwType, (LPBYTE)&nv.Value, &cbValue);
			if (status != ERROR_SUCCESS) // E.g., status == ERROR_NO_MORE_ITEMS
				break;
			if (dwType != REG_DWORD)
				continue;
			outList.add(nv);
		}
		RegCloseKey(hkey);
	}
	return res;
}

LONG Registry_FindEntryByValue(HKEY hkeyRoot, LPCTSTR pszKey, LPCTSTR pszTestValue, LPTSTR pszEntryName, UINT ccMaxName, int cMaxNames)
{
	bool bGotoNext = *pszEntryName ? true : false;
	WCHAR szNextName[256] = { 0 };
	WCHAR szNextValue[256] = { 0 };
	BOOL bMatched = FALSE;
	HKEY hkey;
	LONG status = RegOpenKeyEx(hkeyRoot, pszKey, 0, KEY_QUERY_VALUE, &hkey);
	if (status == ERROR_SUCCESS) {
		for (int i = 0; i < cMaxNames; i++) {
			DWORD ccName = ARRAYSIZE(szNextName);
			DWORD dwType = 0;
			DWORD cbValue = sizeof(szNextValue);
			status = RegEnumValue(hkey, i, szNextName, &ccName, NULL, &dwType, (LPBYTE)szNextValue, &cbValue);
			if (status != ERROR_SUCCESS) // E.g., status == ERROR_NO_MORE_ITEMS
				break;
			if (dwType != REG_SZ)
				continue;
			if (bGotoNext)
			{
				if (0 == _wcsicmp(szNextName, pszEntryName))
					bGotoNext = false; // can start matching process at next entry onward
				continue;
			}
			if (_wcsicmp(szNextValue, pszTestValue) == 0)
			{
				wcscpy_s(pszEntryName, ccMaxName, szNextName);
				bMatched = TRUE;
				break;
			}
		}
		RegCloseKey(hkey);
	}
	if (!bMatched)
	{
		*pszEntryName = 0;
		if (status == ERROR_SUCCESS)
			status = ERROR_NO_MORE_ITEMS;
	}
	return status;

}
