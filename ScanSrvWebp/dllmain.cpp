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
// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <initguid.h>
#include "ScanServerImpl.h"


// propertysheet handler clsid
#define WSZ_CLSID_MyScanServer L"{6ccda86f-3c63-11ed-9d82-00155d938f70}"
DEFINE_GUID(CLSID_MyScanServer, 0x6ccda86f, 0x3c63, 0x11ed, 0x9d, 0x82, 0x00, 0x15, 0x5d, 0x93, 0x8f, 0x70);
#define WSZ_NAME_MyScanServer L"Maximilian's Webp Scan Server"


ULONG LibRefCount = 0;
ULONG LibLockCount = 0;
HMODULE LibInstanceHandle = NULL;


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		LibInstanceHandle = hModule;
		break;
	case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// Special entry points required for inproc servers

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
	*ppv = NULL;
	IClassFactory *pClassFactory;
	// propertysheet handlers
	if (IsEqualCLSID(rclsid, CLSID_MyScanServer))
		pClassFactory = new IClassFactoryNoAggrImpl<ScanServerImpl>;
	else
		return CLASS_E_CLASSNOTAVAILABLE;
	if (pClassFactory == NULL)
		return E_OUTOFMEMORY;
	HRESULT hr = pClassFactory->QueryInterface(riid, ppv);
	pClassFactory->Release();
	return hr;
}

STDAPI DllCanUnloadNow(void)
{
	return (LibRefCount == 0 && LibLockCount == 0) ? S_OK : S_FALSE;
}

DWORD DllGetVersion(void)
{
	return MAKELONG(1, 0);
}

STDAPI DllRegisterServer(void)
{
	long res;
	res = Registry_CreateComClsidKey(WSZ_CLSID_MyScanServer, WSZ_NAME_MyScanServer);
	if (res != ERROR_SUCCESS)
		return HRESULT_FROM_WIN32(res);
	res = Registry_CreateNameValue(HKEY_LOCAL_MACHINE, SCANSERVER_REGKEY, L".webp", WSZ_CLSID_MyScanServer);
	if (res == ERROR_SUCCESS)
		res = Registry_CreateNameValue(HKEY_LOCAL_MACHINE, SCANSERVER_EXTGROUPS_REGKEY, L".webp", L"image");
	return S_OK;
}

STDAPI DllUnregisterServer(void)
{
	Registry_DeleteValue(HKEY_LOCAL_MACHINE, SCANSERVER_EXTGROUPS_REGKEY, L".webp");
	Registry_DeleteValue(HKEY_LOCAL_MACHINE, SCANSERVER_REGKEY, L".webp");
	Registry_DeleteSubkey(HKEY_CLASSES_ROOT, L"CLSID", WSZ_CLSID_MyScanServer);
	return S_OK;
}

//////////////////////////////////////////////////////////////
