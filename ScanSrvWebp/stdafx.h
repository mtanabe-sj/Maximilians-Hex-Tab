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
// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <Shlwapi.h>
#include <olectl.h>


// reference additional headers your program requires here
// reference additional headers your program requires here
extern ULONG LibRefCount;
extern ULONG LibLockCount;

#define LIB_ADDREF InterlockedIncrement(&LibRefCount)
#define LIB_RELEASE InterlockedDecrement(&LibRefCount)
#define LIB_LOCK InterlockedIncrement(&LibLockCount)
#define LIB_UNLOCK InterlockedDecrement(&LibLockCount)

extern HMODULE LibInstanceHandle;
#define LibResourceHandle LibInstanceHandle
#define AppInstanceHandle LibInstanceHandle

#include "helper.h"
#include "RegistryHelper.h"
#include "IUnknownImpl.h"

