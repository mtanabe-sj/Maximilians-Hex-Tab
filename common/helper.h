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
#include <stdlib.h>
#include <strsafe.h>
#include <time.h>


#ifdef _DEBUG
#define ASSERT(good) if (!(good)) DebugBreak();
#else
#define ASSERT(good)
#endif

#define stringify(s) #s
#define xstringify(s) stringify(s)

#define POWEROF2(p) (1 << (p))

#define _CompareRectsByWidth(rc1, rc2) ((rc1.right - rc1.left)-(rc2.right - rc2.left))
#define _CompareRectsByHeight(rc1, rc2) ((rc1.bottom - rc1.top)-(rc2.bottom - rc2.top))

template <class T>
void _swap(T t1, T t2)
{
	T t = t1;
	t1 = t2;
	t2 = t;
}


template <class T>
class AutoFree
{
public:
	AutoFree(T *src = NULL) : _p(src) {}
	~AutoFree() { Free(); }
	T *Alloc(size_t n, bool clear = true)
	{
		_p = (T*)malloc(n);
		if (_p && clear)
			memset(_p, 0, n);
	}
	void Free()
	{
		LPVOID p = InterlockedExchangePointer((LPVOID*)&_p, NULL);
		if (p)
			free(p);
	}
	operator T*() { return _p; }
	T** operator &() { return &_p; }
	T* operator ->() const { return _p; }
	T* operator =(T* src)
	{
		LPVOID p = InterlockedExchangePointer((LPVOID*)&_p, src);
		if (p)
			free(p);
		return _p;
	}
protected:
	T *_p;
};


class strvalenum
{
public:
	strvalenum(wchar_t sep, LPCWSTR src) : _sep(sep), _p0(_wcsdup(src)) { _decompose(); }
	~strvalenum() { if (_p0) free(_p0); }

	LPCWSTR getAt(LPVOID pos)
	{
		INT_PTR i2 = ((INT_PTR)pos) - 1;
		if (i2 < 0 || i2 >= _count)
			return NULL;
		LPTSTR p = _p0;
		for (INT_PTR i = 0; i < i2; i++)
		{
			p += wcslen(p) + 1;
			while (*p == ' ') { p++; };
		}
		return p;
	}
	LPVOID getHeadPosition()
	{
		if (_count)
			return (LPVOID)1;
		return (LPVOID)0;
	}
	LPCWSTR getNext(LPVOID& pos)
	{
		LPCWSTR p = getAt(pos);
		INT_PTR c = (INT_PTR)pos;
		if (c >= _count)
			c = 0;
		else
			c++;
		pos = (LPVOID)c;
		return p;
	}
	LPVOID getTailPosition()
	{
		if (_count)
			return (LPVOID)(ULONG_PTR)_count;
		return (LPVOID)0;
	}
	LPCWSTR getPrior(LPVOID& pos)
	{
		LPCWSTR p = getAt(pos);
		INT_PTR c = (INT_PTR)pos;
		if (c > 0)
			c--;
		else
			c = 0;
		pos = (LPVOID)c;
		return p;
	}
	int getCount() { return _count; }

protected:
	LPWSTR _p0;
	wchar_t _sep;
	int _count;

	void _decompose()
	{
		int cc = (int)wcslen(_p0);
		_count = 0;
		if (*_p0)
			_count++;
		LPWSTR p = _p0;
		for (int i = 0; i < cc; i++) {
			if (_p0[i] == '"') {
				for (int j = i + 1; j < cc; j++) {
					if (_p0[j] == '"') {
						*p = 0; // in case this is the last entry
						i = j;
						break;
					}
					*p++ = _p0[j];
				}
			}
			else if (_p0[i] == _sep) {
				*p++ = 0;
				_count++;
			}
			else {
				*p++ = _p0[i];
			}
		}
		if (p < _p0 + cc)
			*p = 0; // terminate the last item.
	}
};

//////////////////////////////////////////////////////

#define OBJLIST_GROW_SIZE 8

template <class T>
class objList
{
public:
	objList() : _p(NULL), _n(0), _max(0) {}
	objList(const objList<T> &src) : _p(NULL), _n(0), _max(0) { clone(src); }
	virtual ~objList() { clear(); }

	T* operator[](const int index) const
	{
		ASSERT(0 <= index && index < _n);
		return _p[index];
	}
	size_t size() const
	{
		return _n;
	}

	void attach(objList<T> *src)
	{
		clear();
		_p = src->_p;
		_n = src->_n;
		_max = src->_max;
		src->_p = NULL;
		src->_n = src->_max = 0;
	}
	bool clone(const objList<T> &src)
	{
		for (int i = 0; i < src._n; i++)
		{
			T* obj = new T(src[i]);
			if (add(obj) == -1)
				return false;
		}
		return true;
	}

	void invalidate()
	{
		_p = NULL;
		_n = 0;
		_max = 0;
	}
	void clear(bool deleteObjs = true)
	{
		if (!_p)
			return;
		if (deleteObjs)
		{
			for (int i = 0; i < _max; i++)
			{
				T *pi = (T*)InterlockedExchangePointer((LPVOID*)(_p + i), NULL);
				if (pi)
				{
					delete pi;
					_n--;
				}
			}
		}
		ASSERT(_n == 0);
		T **p = (T**)InterlockedExchangePointer((LPVOID*)&_p, NULL);
		free(p);
		_max = 0;
	}

	int add(T *obj)
	{
		int i;
		for (i = 0; i < _max; i++)
		{
			T *pi = (T*)InterlockedCompareExchangePointer((LPVOID*)(_p + i), obj, NULL);
			if (!pi)
			{
				_n++;
				return i;
			}
		}
		T **p2 = (T**)realloc(_p, (_max + OBJLIST_GROW_SIZE) * sizeof(T*));
		if (!p2)
			return -1;
		T **p3 = p2 + _max;
		ZeroMemory(p3, OBJLIST_GROW_SIZE * sizeof(T*));
		*p3 = obj;
		_max += OBJLIST_GROW_SIZE;
		_p = p2;
		i = _n++;
		return i;
	}
	bool remove(T *obj)
	{
		for (int i = 0; i < _max; i++)
		{
			T *pi = InterlockedCompareExchangePointer((LPVOID*)(_p + i), NULL, obj);
			if (pi)
			{
				delete pi;
				_n--;
				ASSERT(_n >= 0);
				_repack(i);
				return true;
			}
		}
		return false;
	}
	bool removeAt(int index)
	{
		if (index < 0 || index >= _max)
			return false;
		T *pi = InterlockedExchangePointer((LPVOID*)(_p + index), NULL);
		if (!pi)
			return false;
		delete pi;
		_n--;
		ASSERT(_n >= 0);
		_repack(index);
		return true;
	}

public:
	T **_p;
protected:
	int _max, _n;

	void _repack(int i0)
	{
		int j = i0;
		for (int i = i0; i < _max; i++)
		{
			if (_p[i])
			{
				if (j < i)
					_p[j] = InterlockedExchangePointer((LPVOID*)(_p + i), NULL);
				j++;
			}
		}
	}
};

//////////////////////////////////////////////////////

#define SIMPLELIST_GROW_SIZE 8

template<class T>
class simpleList
{
public:
	// make an empty list
	simpleList() : _p(NULL), _n(0), _max(0) {}
	// pre-allocate n elements
	simpleList(size_t n) : _p((T*)calloc(n, sizeof(T))), _n(n), _max(SIMPLELIST_GROW_SIZE*((n + SIMPLELIST_GROW_SIZE - 1) / SIMPLELIST_GROW_SIZE)) { ZeroMemory(_p, dataLength()); }
	// clone src
	simpleList(const simpleList &src) : _p((T*)calloc(src._n, sizeof(T))), _n(src._n), _max(src._max) { CopyMemory(_p, src._p, dataLength()); }

	~simpleList() { clear(); }

	const simpleList<T>& operator=(const simpleList &src)
	{
		clear();
		_p = (T*)calloc(src._n, sizeof(T));
		_n = src._n;
		_max = src._max;
		CopyMemory(_p, src._p, dataLength());
		return *this;
	}

	T& operator[](const size_t index) const
	{
		ASSERT(0 <= index && index < _n);
		return _p[index];
	}
	T& at(const size_t index) const
	{
		ASSERT(0 <= index && index < _n);
		return _p[index];
	}
	size_t size() const
	{
		return _n;
	}
	size_t dataLength() const
	{
		return _n * sizeof(T);
	}
	T *data() const
	{
		return _p;
	}
	bool resize(size_t n)
	{
		if (n == _n)
			return true;
		if (n > _n)
		{
			for (size_t i = _n; i < n; i++)
				if (!alloc())
					return false;
		}
		else
		{
			for (size_t i = _n; i >= n; i--)
				if (-1 == remove(i - 1))
					return false;
		}
		return true;
	}
	T *alloc(size_t index = -1)
	{
		if (index == -1)
			index = _n;
		if (index > _n)
			return NULL;
		if (_n + 1 > _max)
		{
			T* p2 = (T*)realloc(_p, (_max + SIMPLELIST_GROW_SIZE) * sizeof(T));
			T* p3 = p2 + _max;
			memset(p3, 0, SIMPLELIST_GROW_SIZE * sizeof(T));
			_max += SIMPLELIST_GROW_SIZE;
			_p = p2;
		}
		memmove(_p + index + 1, _p + index, sizeof(T)*(_n - index));
		_n++;
		return _p + index;
	}
	size_t add(const T *srcElements, size_t numElements)
	{
		for (size_t i = 0; i < numElements; i++)
			add(srcElements[i]);
		return _n;
	}
	size_t add(const T &src)
	{
		if (_n < _max)
		{
			_p[_n] = src;
			return ++_n;
		}
		T* p2 = (T*)realloc(_p, (_max + SIMPLELIST_GROW_SIZE) * sizeof(T));
		T* p3 = p2 + _max;
		memset(p3, 0, SIMPLELIST_GROW_SIZE * sizeof(T));
		*p3 = src;
		_max += SIMPLELIST_GROW_SIZE;
		_p = p2;
		return ++_n;
	}
	size_t remove(size_t index)
	{
		if (!_p || index >= _max || index < 0)
			return (size_t)-1;
		clearItem(_p + index);
		size_t moveLen = _max - (index + 1);
		if (moveLen)
			memmove(_p + index, _p + index + 1, moveLen * sizeof(T));
		memset(_p + _max - 1, 0, sizeof(T));
		_n--;
		return _n;
	}
	virtual void clear()
	{
		LPVOID p = InterlockedExchangePointer((LPVOID*)&_p, NULL);
		if (p)
		{
			for (size_t i = 0; i < _n; i++)
				clearItem((T*)p + i);
			free(p);
		}
		_n = _max = 0;
	}
	// subclasses: override this method to do custom cleanup per item.
	virtual void clearItem(T* pitem)
	{
	}

protected:
	T *_p;
	size_t _n, _max;
};


// use this class if you need last-in, first-out (LIFO) buffering support
template<class T>
class simpleStack : public simpleList<T>
{
public:
	simpleStack() {}

	T &top()
	{
		ASSERT(simpleList<T>::_n > 0);
		return simpleList<T>::at(simpleList<T>::_n - 1);
	}

	size_t push(T &src)
	{
		return simpleList<T>::add(src);
	}
	size_t pop()
	{
		if (simpleList<T>::_n > 0)
			return simpleList<T>::remove(simpleList<T>::_n - 1);
		ASSERT(FALSE);
		return -1;
	}
};

//////////////////////////////////////////////////////

class astring
{
public:
	astring() { init(); }
	astring(const astring& asText) { init(); assign(asText); }
	astring(LPCSTR pszText, int ccText = -1) { init(); assign(pszText, ccText); }
	~astring() { clear(); }

	LPSTR _buffer; // string buffer
	ULONG _maxLength; // total length of buffer including trailing null char
	ULONG _length;

	operator LPCSTR() { return _buffer ? _buffer : ""; }
	const astring& operator=(const astring& src)
	{
		assign(src);
		return *this;
	}
	const astring& operator+=(LPCSTR src)
	{
		append(src);
		return *this;
	}

	void clear()
	{
		if (_buffer)
			free(_buffer);
		init();
	}
	void attach(astring& src)
	{
		clear();
		ASSERT(src._length <= src._maxLength);
		_buffer = src._buffer;
		_maxLength = src._maxLength;
		_length = src._length;
		src.init();
	}
	LPSTR alloc(ULONG cc)
	{
		if (cc == -1) // -1 is considered invalid. _vscprintf returns -1 if it fails.
			return NULL;
		cc++; // add a byte for a termination null char
		if (!(_buffer = (LPSTR)malloc(cc)))
			return NULL;
		ZeroMemory(_buffer, cc);
		_length = 0;
		_maxLength = cc;
		return _buffer;
	}
	bool assign(const astring& src)
	{
		clear();
		if (!src._buffer)
			return true;
		if (!alloc(src._length))
			return false;
		CopyMemory(_buffer, src._buffer, src._length);
		_length = src._length;
		return true;
	}
	bool assign(LPCSTR pszText, int ccText = -1)
	{
		clear();
		if (!pszText)
			return true; // Null input string is legal.
		if (ccText == -1)
			ccText = (int)strlen(pszText);
		if (!alloc(ccText))
			return false;
		CopyMemory(_buffer, pszText, ccText);
		_length = ccText;
		return true;
	}
	bool assignCp(UINT nCodepage, LPCWSTR pwszText, int ccText = -1)
	{
		clear();
		if (!pwszText)
			return true;
		if (ccText == -1)
			ccText = (int)wcslen(pwszText);
		int cc2 = WideCharToMultiByte(nCodepage, 0, pwszText, ccText, NULL, 0, NULL, NULL);
		if (!cc2)
			return true;
		if (!alloc(cc2))
			return false;
		_length = WideCharToMultiByte(nCodepage, 0, pwszText, ccText, _buffer, cc2, NULL, NULL);
		ASSERT(_length == cc2);
		return true;
	}
	bool append(LPCSTR pszText, int ccText = -1)
	{
		if (!pszText)
			return true;
		if (ccText == -1)
			ccText = (int)strlen(pszText);
		ULONG cc1 = _length;
		ULONG cc2 = cc1 + ccText;
		LPSTR p1 = (LPSTR)InterlockedExchangePointer((LPVOID*)&_buffer, NULL);
		if (!alloc(cc2))
			return FALSE;
		if (p1)
		{
			CopyMemory(_buffer, p1, cc1);
			free(p1);
		}
		CopyMemory(_buffer + cc1, pszText, ccText);
		_length = cc2;
		return true;
	}
	bool empty() { return (_buffer && *_buffer) ? false : true; }
	bool vformat(LPCSTR fmt, va_list& args)
	{
		if (!alloc(_vscprintf(fmt, args)))
			return false;
		_length = vsprintf_s(_buffer, _maxLength, fmt, args);
		return true;
	}
	bool format(LPCSTR fmt, ...)
	{
		va_list	args;
		va_start(args, fmt);
		bool success = vformat(fmt, args);
		va_end(args);
		return success;
	}
	int contains(LPCSTR token)
	{
		if (!_buffer || _length < strlen(token))
			return -1;
		LPCSTR p = strstr(_buffer, token);
		if (!p)
			return -1;
		return (int)(p - _buffer);
	}
	void trimLeft(char ch = ' ')
	{
		if (!_buffer) return;
		LPSTR p = _buffer;
		while (*p == ch) { p++; };
		if (p > _buffer) {
			astring trimmed(p);
			attach(trimmed);
		}
	}
	void trimRight(char ch = ' ') {
		if (!_buffer) return;
		LPSTR p = _buffer + _length;
		INT c = 0;
		while (p > _buffer && *(--p) == ch) { *p = 0; c++; };
		if (c) {
			astring trimmed(_buffer);
			attach(trimmed);
		}
	}
	void trim(char ch = ' ') {
		trimLeft(ch);
		trimRight(ch);
	}
	LPCSTR fill(ULONG len, char ch = 0)
	{
		if (_maxLength < len)
		{
			if (!alloc(len))
				return NULL;
		}
		memset(_buffer, ch, len);
		_buffer[len] = 0;
		_length = len;
		return _buffer;
	}

protected:
	void init()
	{
		_buffer = NULL; _maxLength = _length = 0;
	}
};


// possible flags of astring2::hexdump
#define ASTRHEXDUMP_FLAG_NO_OFFSET 1
#define ASTRHEXDUMP_FLAG_NO_ASCII 2
#define ASTRHEXDUMP_FLAG_NO_ENDING_LF 4
#define ASTRHEXDUMP_FLAG_ELLIPSIS 8

class astring2 : public astring
{
public:
	astring2() {}
	astring2(LPCSTR fmt, ...) {
		va_list	args;
		va_start(args, fmt);
		vformat(fmt, args);
		va_end(args);
	}

	LPCSTR hexdump(LPVOID dataSrc, int dataLen, UINT flags = 0)
	{
		LPBYTE src = (LPBYTE)dataSrc;
		char buf[80] = { 0 };
		for (int i = 0; i < (dataLen + 15) / 16; i++)
		{
			memset(buf, ' ', sizeof(buf) - 1);
			LPSTR dest = buf;
			if (!(flags & ASTRHEXDUMP_FLAG_NO_OFFSET))
				dest += sprintf(buf, "%04X: ", i);
			size_t j, j2 = min(16, dataLen - i * 16);
			for (j = 0; j < j2; j++)
			{
				sprintf(dest, "%02X ", src[i * 16 + j]);
				dest += 3;
			}
			if (!(flags & ASTRHEXDUMP_FLAG_NO_ASCII))
			{
				dest[0] = ' ';
				for (; j < 16; j++)
					dest += 3;
				for (j = 0; j < j2; j++)
				{
					char c = src[i * 16 + j];
					if (c < ' ' || c > 0x7f)
						c = '.';
					*dest++ = c;
				}
			}
			*dest++ = '\n';
			*dest = '\0';
			append(buf);
		}
		if (flags & ASTRHEXDUMP_FLAG_NO_ENDING_LF)
			trimRight('\n');
		if (flags & ASTRHEXDUMP_FLAG_ELLIPSIS)
			append("...");
		return _buffer;
	}
};

class bstring
{
public:
	bstring(const bstring&src) : _b(NULL) { assignW(src); }
	bstring(LPCWSTR s = NULL, int slen = -1) :_b(NULL) { assignW(s, slen); }
	~bstring() { free(); }
	BSTR _b;

	BSTR detach()
	{
		return (BSTR)InterlockedExchangePointer((LPVOID*)&_b, NULL);
	}
	void attach(bstring &src)
	{
		free();
		_b = src.detach();
	}
	BSTR alloc(int slen)
	{
		free();
		_b = SysAllocStringLen(NULL, slen);
		return _b;
	}

	operator BSTR() const { return _b; }
	operator LPCWSTR() const { return _b ? _b : L""; }
	BSTR* operator&() { return &_b; }
	int length() const { return _b? (int)wcslen(_b) : 0; }
	const bstring& operator=(LPCWSTR s)
	{
		assignW(s);
		return *this;
	}
	const bstring& operator+=(LPCWSTR s)
	{
		appendW(s);
		return *this;
	}
	objList<bstring> *split(WCHAR sep) const
	{
		objList<bstring> *v = new objList<bstring>;
		int i, i0, cc, len=SysStringLen(_b);
		for (i = 0, i0 = 0; i < len; i++)
		{
			if (_b[i] != sep)
				continue;
			cc = i - i0;
			v->add(new bstring(_b+i0, cc));
			i0 = i+1;
		}
		if ((cc = i - i0) > 0)
			v->add(new bstring(_b + i0, cc));
		return v;
	}
	bool assignW(LPCWSTR s, int slen = -1)
	{
		free();
		if (!s)
			return true;
		if (slen != -1)
		{
			_b = SysAllocStringLen(NULL, slen);
			CopyMemory(_b, s, slen * sizeof(WCHAR));
		}
		else
			_b = SysAllocString(s);
		if (!_b)
			return false;
		return true;
	}
	bool appendW(LPCWSTR s, int slen = -1)
	{
		if (!s)
			return true;
		size_t n1 = 0;
		size_t n2 = slen == -1 ? wcslen(s) : slen;
		if (_b)
			n1 = SysStringLen(_b);
		size_t n = n1 + n2;
		BSTR b2 = SysAllocStringLen(NULL, (UINT)n);
		if (!b2)
			return false;
		CopyMemory(b2, _b, n1 * sizeof(WCHAR));
		CopyMemory(b2 + n1, s, n2 * sizeof(WCHAR));
		free();
		_b = b2;
		return true;
	}
	bool assignA(LPCSTR s, int slen = -1, int cp = CP_UTF8)
	{
		free();
		if (!s)
			return true;
		size_t n = MultiByteToWideChar(cp, 0, s, slen, NULL, 0);
		_b = SysAllocStringLen(NULL, (UINT)n); // this actually allocates n+1 wchar space.
		if (!_b)
			return false;
		MultiByteToWideChar(cp, 0, s, slen, _b, (int)n + 1);
		return true;
	}
	bool appendA(LPCSTR s, int slen = -1, int cp = CP_UTF8)
	{
		if (!s)
			return true;
		size_t n2 = MultiByteToWideChar(cp, 0, s, slen, NULL, 0);
		size_t n1 = 0;
		if (_b)
			n1 = SysStringLen(_b);
		size_t n = n1 + n2;
		BSTR b2 = SysAllocStringLen(NULL, (UINT)n);
		if (!b2)
			return false;
		wcscpy(b2, _b);
		MultiByteToWideChar(cp, 0, s, slen, b2 + n1, (int)n2 + 1);
		free();
		_b = b2;
		return true;
	}
	void free()
	{
		if (_b)
		{
			SysFreeString(_b);
			_b = NULL;
		}
	}
};

class ustring
{
public:
	ustring() { init(); }
	ustring(ustring &src) { init(); assign(src); }
	ustring(UINT codepage, LPCSTR pszText, int ccText = -1) { init(); assignCp(codepage, pszText, ccText); }
	ustring(LPCWSTR pszText, int ccText = -1) { init(); assign(pszText, ccText); }
	virtual ~ustring() { clear(); }

	operator LPCWSTR() { return _buffer ? _buffer : L""; }
	const ustring& operator+=(LPCWSTR src)
	{
		append(src);
		return *this;
	}
	const ustring& operator=(const ustring& src)
	{
		assign(src);
		return *this;
	}

	virtual void clear()
	{
		LPVOID p = InterlockedExchangePointer((LPVOID*)&_buffer, NULL);
		if (p)
		{
			free(p);
			_maxLength = _length = 0;
		}
	}
	bool assign(const ustring &src)
	{
		return assign(src._buffer, src._length);
	}
	bool assign(LPCWSTR pszText, int ccText = -1)
	{
		clear();
		if (!pszText)
			return true; // Null input string is legal.
		if (ccText == -1)
			ccText = (int)wcslen(pszText);
		if (ccText < 0) return false;
		if (!alloc(ccText))
			return false;
		wcsncpy(_buffer, pszText, ccText);
		_length = ccText;
		return true;
	}
	bool assignCp(UINT codepage, LPCSTR pszText, int ccText = -1) {
		clear();
		if (!pszText)
			return true; // Null input string is legal.
		if (ccText == -1)
			ccText = (int)strlen(pszText);
		if (ccText < 0) return false;
		// Get the char count of the Ansi string.
		int ccUnicode = MultiByteToWideChar(codepage, 0, pszText, ccText, NULL, 0);
		// Allocate the Unicode buffer.
		if (!alloc(ccUnicode))
			return false;
		// Convert the Ansi string to Unicode.
		_length = MultiByteToWideChar(codepage, 0, pszText, ccText, _buffer, _maxLength / sizeof(WCHAR));
		ASSERT(_length == ccUnicode);
		return true;
	}
	bool append(const ustring &src)
	{
		return append(src._buffer, src._length);
	}
	bool append(LPCWSTR pszText, int ccText = -1)
	{
		if (!pszText)
			return true; // Null input string is legal.
		ULONG ccExtra = 0;
		if (ccText == -1) {
			ccText = (int)wcslen(pszText);
			ccText++;
		}
		else
			ccExtra = 1;
		ULONG cc1 = _length;
		ULONG cc2 = cc1 + ccText + ccExtra;
		LPWSTR p1 = (LPWSTR)InterlockedExchangePointer((LPVOID*)&_buffer, NULL);
		if (!alloc(cc2))
			return FALSE;
		if (p1) {
			CopyMemory(_buffer, p1, cc1 * sizeof(WCHAR));
			free(p1);
		}
		CopyMemory(_buffer + cc1, pszText, ccText * sizeof(WCHAR));
		_length = cc2 - 1;
		return true;
	}
	bool appendCp(UINT codepage, const astring &src)
	{
		return appendCp(codepage, src._buffer, src._length);
	}
	bool appendCp(UINT codepage, LPCSTR pszText, int ccText = -1)
	{
		ustring str2(codepage, pszText, ccText);
		return append(str2);
	}
	void attach(ustring& src)
	{
		clear();
		ASSERT(src._length <= src._maxLength);
		_buffer = src._buffer;
		_maxLength = src._maxLength;
		_length = src._length;
		src.init();
	}
	LPWSTR alloc(int cc)
	{
		clear();
		if (cc >= 0)
		{
			_buffer = (LPWSTR)calloc(++cc, sizeof(WCHAR));
			if (_buffer)
			{
				ZeroMemory(_buffer, cc * sizeof(WCHAR));
				_length = 0;
				_maxLength = cc * sizeof(WCHAR);
			}
		}
		return _buffer;
	}
	bool empty() { return (_buffer && *_buffer) ? false : true; }
#define USTR_VFORMAT_GROWTH_SIZE 0x10
	bool vformat2(LPCWSTR fmt, va_list& args)
	{
		HRESULT hr;
		int retry = 0;
		int len = lstrlen(fmt) + USTR_VFORMAT_GROWTH_SIZE;
		while (alloc(len))
		{
			size_t remainingLen = 0;
			hr = StringCbVPrintfExW(_buffer, _maxLength, NULL, &remainingLen, STRSAFE_FILL_ON_FAILURE, fmt, args);
			if (SUCCEEDED(hr))
				return true;
			if (hr != STRSAFE_E_INSUFFICIENT_BUFFER)
				break;
			len += USTR_VFORMAT_GROWTH_SIZE;
		}
		clear();
		return false;
	}
	bool vformat(LPCWSTR fmt, va_list& args)
	{
		if (!alloc(_vscwprintf(fmt, args)))
			return false;
		_length = vswprintf_s(_buffer, _maxLength / sizeof(WCHAR), fmt, args);
		return true;
	}
	bool format(LPCWSTR fmt, ...)
	{
		va_list	args;
		va_start(args, fmt);
		bool success = vformat(fmt, args);
		va_end(args);
		return success;
	}
	int contains(LPCWSTR token)
	{
		if (!_buffer || _length < wcslen(token))
			return -1;
		LPCWSTR p = wcsstr(_buffer, token);
		if (!p)
			return -1;
		return (int)(p - _buffer);
	}
	objList<ustring> *split(WCHAR sep) const
	{
		objList<ustring> *v = new objList<ustring>;
		int i, i0, cc, len = _length;
		for (i = 0, i0 = 0; i < len; i++)
		{
			if (_buffer[i] != sep)
				continue;
			cc = i - i0;
			v->add(new ustring(_buffer + i0, cc));
			i0 = i + 1;
		}
		if ((cc = i - i0) > 0)
			v->add(new ustring(_buffer + i0, cc));
		return v;
	}

	LPWSTR _buffer;
	ULONG _maxLength; // number of allocated bytes in the buffer
	ULONG _length; // number of wide characters used in the buffer (_length*sizeof(WCHAR) <= _maxLength)

protected:
	void init()
	{
		_buffer = NULL; _maxLength = _length = 0;
	}
};


class ustring2 : public ustring
{
public:
	ustring2(UINT stringResourceId = 0) {
		if (stringResourceId)
			loadString(stringResourceId);
	}
	ustring2(LPCWSTR fmt, ...) {
		va_list	args;
		va_start(args, fmt);
		vformat(fmt, args);
		va_end(args);
	}
	bool loadString(ULONG ids) {
		int i;
		int cc1 = 32;
		for (i = 0; i < 8; i++) {
			if (!alloc(cc1))
				return false;
			_length = ::LoadString(LibResourceHandle, ids, _buffer, cc1);
			if ((int)_length < cc1 - 1) {
				if (_length == 0)
					swprintf_s(_buffer, cc1, L"(STRRES_%d)", ids);
				break;
			}
			clear();
			cc1 += 32;
		}
		ASSERT(i < 32);
		return true;
	}
	bool formatOSMessage(DWORD nErrorCode) {
		LPVOID lpMsgBuf;
		if (FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			nErrorCode,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			(LPWSTR)&lpMsgBuf,
			0,
			NULL))
		{
			assign((LPCWSTR)lpMsgBuf);
			LocalFree(lpMsgBuf);
			return true;
		}
		return false;
	}
};

class ustring3 : public ustring2
{
public:
	ustring3(DWORD errorCode)
	{
		formatOSMessage(errorCode);
	}
};

class ustring4 : public ustring2
{
public:
	ustring4(LPCWSTR dirPath, LPCWSTR filename=NULL) : ustring2(dirPath)
	{
		if (filename)
			appendFilename(filename);
	}
	WCHAR front()
	{
		if (_length)
			return _buffer[0];
		return 0;
	}
	WCHAR back()
	{
		if (_length)
			return _buffer[_length-1];
		return 0;
	}
	bool appendFilename(LPCWSTR filename)
	{
		if (_length && back() != '\\')
			append(L"\\");
		return append(filename);
	}
};


////////////////////////////////////////////
// read-only string support

typedef struct _RSTRING {
	LONG _length;
	LPCSTR _buffer;
	LONG _maxLength;
} RSTRING, *PRSTRING;

class rstring : public RSTRING {
public:
	rstring(LPCSTR sourceText = NULL, long sourceLength = 0) : _pos(0) {
		_buffer = sourceText;
		_length = _maxLength = sourceLength;
	}
	rstring(rstring& source) : _pos(0) {
		_buffer = source._buffer;
		_length = source._length;
		_maxLength = source._maxLength;
	}
	~rstring() {
	}

	LONG _pos;

	void clear() {
		_buffer = NULL;
		_length = _maxLength = _pos = 0;
	}
	LPCSTR clone(astring &dest) {
		dest.assign(_buffer, _length);
		return dest;
	}
	void assign(rstring &sourceString) {
		_buffer = sourceString._buffer;
		_length = sourceString._length;
		_maxLength = sourceString._maxLength;
	}
	void assign(LPCSTR sourceText, long sourceLength) {
		_buffer = sourceText;
		_length = sourceLength;
		if (_length > _maxLength)
			_maxLength = _length;
	}
	BOOL isEqualTo(rstring &testString) {
		long i;
		if (_length != testString._length)
			return FALSE;
		for (i = 0; i < _length; i++) {
			if (_buffer[i] != testString._buffer[i])
				return FALSE;
		}
		return TRUE;
	}
	BOOL isEqualTo(LPCSTR testText) {
		long i;
		for (i = 0; i < _length; i++) {
			if (testText[i] == 0)
				return FALSE;
			if (_buffer[i] != testText[i])
				return FALSE;
		}
		return TRUE;
	}
	long findChar(char charToFind) {
		long i;
		for (i = _pos; i < _length; i++) {
			if (_buffer[i] == charToFind) {
				_pos = i + 1;
				return i;
			}
		}
		return -1;
	}
	long findCharN(char charToFind, long maxLength) {
		long i;
		if (maxLength == -1 || maxLength > _length)
			maxLength = _length;
		for (i = _pos; i < maxLength; i++) {
			if (_buffer[i] == charToFind) {
				_pos = i + 1;
				return i;
			}
		}
		return -1;
	}
	long findString(LPCSTR substringToFind, long substringLength) {
		long i, searchLength;
		searchLength = _length - substringLength + 1;
		if (searchLength > 0) {
			for (i = _pos; i < searchLength; i++) {
				if (_buffer[i] == substringToFind[0]) {
					if (0 == memcmp(_buffer + i, substringToFind, substringLength)) {
						_pos = i + substringLength;
						return i;
					}
				}
			}
		}
		return -1;
	}
	// scan forward for a substring
	BOOL scan(LPCSTR substringToFind, rstring *leadingSubstring, rstring *traillingSubstring) {
		long i, j;
		long ccToken = (long)strlen(substringToFind);
		long ccScan = this->_length - ccToken - 1;
		if (ccScan <= 0 || ccToken == 0)
			return FALSE;
		for (i = 0; i < ccScan; i++)
		{
			for (j = 0; j < ccToken; j++) {
				if (this->_buffer[i + j] != substringToFind[j])
					break;
			}
			if (j == ccToken)
			{
				if (leadingSubstring)
				{
					leadingSubstring->_length = leadingSubstring->_maxLength = i;
					leadingSubstring->_buffer = this->_buffer;
				}
				if (traillingSubstring)
				{
					traillingSubstring->_length = this->_length - i - ccToken;
					traillingSubstring->_buffer = this->_buffer + i + ccToken;
				}
				return TRUE;
			}
		}
		return FALSE;
	}
	BOOL split/*Forward*/(char splittingChar, rstring *leadingSubstring, rstring *traillingSubstring) {
		long i;
		// scan backward
		for (i = 0; i < _length; i++)
		{
			if (_buffer[i] == splittingChar)
			{
				if (leadingSubstring)
				{
					// leading part can be 'this'
					leadingSubstring->_length = i;
					if (this != leadingSubstring)
					{
						leadingSubstring->_buffer = _buffer;
						leadingSubstring->_maxLength = leadingSubstring->_length;
					}
				}
				i++;
				if (traillingSubstring)
				{
					traillingSubstring->_buffer = _buffer + i;
					traillingSubstring->_length = _length - i;
					traillingSubstring->_maxLength = traillingSubstring->_length;
				}
				return TRUE;
			}
		}
		return FALSE;
	}
	BOOL splitReverse(char splittingChar, rstring *leadingSubstring, rstring *traillingSubstring) {
		long i;
		// scan backward
		for (i = _length; i > 0; i--)
		{
			if (_buffer[i - 1] == splittingChar)
			{
				if (traillingSubstring)
				{
					traillingSubstring->_buffer = _buffer + i;
					traillingSubstring->_length = _length - i;
					traillingSubstring->_maxLength = traillingSubstring->_length;
				}
				i--;
				if (leadingSubstring)
				{
					// leading part can be 'this'
					leadingSubstring->_length = i;
					if (this != leadingSubstring)
					{
						leadingSubstring->_buffer = _buffer;
						leadingSubstring->_maxLength = leadingSubstring->_length;
					}
				}
				return TRUE;
			}
		}
		return FALSE;
	}
	void trimLeft(char charToRemove = ' ') {
		while (_length > 0 && _buffer[0] == charToRemove) {
			_buffer++;
			_length--;
			_maxLength--;
		}
	}
	void trimRight(char charToRemove = ' ') {
		while (_length > 0 && _buffer[_length - 1] == charToRemove) {
			_length--;
			_maxLength--;
		}
	}
	void trim(char charToRemove = ' ') {
		trimLeft(charToRemove);
		trimRight(charToRemove);
	}
};

//////////////////////////////////////////////////////

class EventLog
{
public:
	EventLog()
	{
	}
	~EventLog()
	{
	}

	void write(LPCWSTR msg, bool timestamp = true)
	{
		if (timestamp)
		{
			time_t now;
			time(&now);
			struct tm t = *localtime(&now);
			ustring2 msg2(L"[%d.%d][%02d:%02d:%02d] %s\n", GetCurrentProcessId(), GetCurrentThreadId(), t.tm_hour, t.tm_min, t.tm_sec, msg);
			OutputDebugStringW(msg2);
#ifdef EVENTLOG_FILE
			_fwriteW(msg2, msg2._length);
#endif//#ifdef EVENTLOG_FILE
		}
		else
		{
			OutputDebugStringW(msg);
#ifdef EVENTLOG_FILE
			_fwriteW(msg, (ULONG)wcslen(msg));
#endif//#ifdef EVENTLOG_FILE
		}
	}
	void writef(LPCWSTR fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		ustring msg;
		msg.vformat(fmt, args);
		write(msg, false);
		va_end(args);
	}

	void dumpData(LPCSTR dataLabel, LPVOID dataSrc, int dataLen)
	{
		OutputDebugStringA(astring2("%s (%d bytes)\n", dataLabel, dataLen));
		OutputDebugStringA(astring2().hexdump(dataSrc, dataLen));
	}

protected:
#ifdef EVENTLOG_FILE
	bool _fwriteW(LPCWSTR src, ULONG len)
	{
		CHAR path[MAX_PATH];
		ExpandEnvironmentStringsA(EVENTLOG_FILE, path, ARRAYSIZE(path));
		FILE *fp = fopen(path, "ab+");
		fwrite(src, 2, len, fp);
		fclose(fp);
		return true;
	}
#endif//#ifdef EVENTLOG_FILE
};


class CRC32
{
public:
	CRC32() : _val(0xFFFFFFFF) {}

	//borrowed from https://stackoverflow.com/questions/26049150/calculate-a-32-bit-_val-lookup-table-in-c-c/26051190
	void feedData(BYTE *src, UINT len)
	{
		while (src && len--)
		{
			UINT tmp = (_val^*src++) & 0xFF;
			for (int i = 0; i < 8; i++)
			{
				tmp = tmp & 1 ? (tmp >> 1) ^ 0xEDB88320 : tmp >> 1;
			}
			_val = tmp ^ _val >> 8;
		}
	}
	UINT value()
	{
		return _val ^ 0xFFFFFFFF;
	}

protected:
	UINT _val;
};

class UIModalState
{
public:
	UIModalState() {}
	virtual void Enter() PURE;
	virtual void Leave() PURE;
};


