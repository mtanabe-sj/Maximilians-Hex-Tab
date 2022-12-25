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


class TLSData
{
public:
	~TLSData() { term(); }

	static TLSData *instance()
	{
		if (!TLSD)
			TLSD = new TLSData;
		return TLSD;
	}

	struct CONTROLPARAMS
	{
		ULONG RefCount;
		ULONG Flags;
	};

	CONTROLPARAMS *allocData(UINT dataLen)
	{
		CONTROLPARAMS *p = (CONTROLPARAMS *)malloc(dataLen);
		if (p)
			memset(p, 0, dataLen);
		return p;
	}
	void freeData(CONTROLPARAMS **data)
	{
		CONTROLPARAMS *p = *data;
		free(p);
		*data = NULL;
	}

	CONTROLPARAMS *get(bool addref=false)
	{
		EnterCriticalSection(&_cs);
		CONTROLPARAMS *p = (CONTROLPARAMS *)TlsGetValue(_tls);
		if (addref)
			p->RefCount++;
		LeaveCriticalSection(&_cs);
		return p;
	}
	/* caller must use allocData to allocate newData. after the call, newData belongs to TLSData. caller must not free it.
	*/
	bool set(CONTROLPARAMS *newData, bool addref=false)
	{
		EnterCriticalSection(&_cs);
		CONTROLPARAMS *cp = (CONTROLPARAMS *)TlsGetValue(_tls);
		if (cp)
		{
			if (newData != cp)
			{
				if (addref)
					cp->RefCount++;
				newData->RefCount = cp->RefCount;
				freeData(&cp);
			}
			else
			{
				if (addref)
					newData->RefCount++;
			}
		}
		else
		{
			newData->RefCount = addref? 1 : 0;
		}
		if (newData != cp)
			TlsSetValue(_tls, newData);
		LeaveCriticalSection(&_cs);
		return true;
	}
	ULONG addref()
	{
		EnterCriticalSection(&_cs);
		CONTROLPARAMS *cp = (CONTROLPARAMS *)TlsGetValue(_tls);
		ULONG ref = 0;
		if (cp)
		{
			ref = ++cp->RefCount;
			TlsSetValue(_tls, cp);
		}
		LeaveCriticalSection(&_cs);
		return ref;
	}
	ULONG release()
	{
		EnterCriticalSection(&_cs);
		CONTROLPARAMS *cp = (CONTROLPARAMS *)TlsGetValue(_tls);
		ULONG ref = 0;
		if (cp)
		{
			ref = --cp->RefCount;
			if (ref == 0)
			{
				TlsSetValue(_tls, NULL);
				freeData(&cp);
				_tls = 0;
			}
		}
		LeaveCriticalSection(&_cs);
		return ref;
	}

private:
	// make sure the definition is made in a .cpp module.
	static TLSData *TLSD;
	CRITICAL_SECTION _cs;
	DWORD _tls;

	TLSData() : _cs{ 0 }, _tls(TLS_OUT_OF_INDEXES) { init(); }

	// disallow cloning
	TLSData(const TLSData&) = delete;
	TLSData & operator=(const TLSData&) = delete;

	bool init()
	{
		InitializeCriticalSection(&_cs);
		_tls = TlsAlloc();
		if (_tls == TLS_OUT_OF_INDEXES)
			return false;
		return true;
	}
	void term()
	{
		EnterCriticalSection(&_cs);
		TlsFree(_tls);
		_tls = TLS_OUT_OF_INDEXES;
		LeaveCriticalSection(&_cs);
		DeleteCriticalSection(&_cs);
	}
};

