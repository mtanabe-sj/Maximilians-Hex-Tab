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
#ifndef __EZXML_H_
#define __EZXML_H_
#include "helper.h"


///////////////////////////////////////////
class EzXmlTagString {
public:
	EzXmlTagString( LPCSTR tagPath ) {
		_next = strchr( tagPath, '\\');
		if (_next) {
			_tagLength = (int)(_next-tagPath);
			_next++;
		} else {
			_tagLength = (int)strlen( tagPath);
		}
		_first = (LPSTR) LocalAlloc( LPTR, _tagLength+1);
		memcpy( _first, tagPath, _tagLength);
	}
	~EzXmlTagString() {
		LocalFree( (HLOCAL)_first);
	}
	int _tagLength;
	LPSTR _first;
	LPCSTR _next;
};

////////////////////////////////////////////
class EzXmlAttribute {
public:
	EzXmlAttribute() {
	}
	~EzXmlAttribute() {
	}
	rstring _name, _value;
};

class EzXmlAttributes : public objList<EzXmlAttribute> {
public:
	EzXmlAttributes() {
	}
	RSTRING _sourceData;
};

class EzXmlAttributesData : public rstring {
public:
	EzXmlAttributesData( LPCSTR dataStart, ULONG dataLength) : rstring( dataStart, dataLength) {}
	long collectElements( EzXmlAttributes *outputCollection) {
		EzXmlAttribute *attribute;
		long p1, p2, p3, pos1, pos2;
		int i;
		trim();
		outputCollection->_sourceData._buffer = _buffer;
		outputCollection->_sourceData._length = _length;
		outputCollection->_sourceData._maxLength = _maxLength;
		p1 = 0;
		for (i = 0; i < 8; i++) {
			pos1 = _pos;
			p3 = findChar( ' ');
			pos2 = _pos;
			_pos = pos1;
			p2 = findCharN( '=', p3);
			if (p2 > p1) {
				// attribute with a value
				attribute = new EzXmlAttribute;
				attribute->_name.assign( _buffer+p1, p2-p1);
				p2++; // pass the '=' char.
				if (p3 == -1)
					attribute->_value.assign( _buffer+p2, _length-p2);
				else
					attribute->_value.assign( _buffer+p2, p3-p2);
				attribute->_name.trim();
				attribute->_value.trim();
				attribute->_value.trim('"');
				outputCollection->add( attribute);
			} else if (p2 == -1) {
				if (p3 == -1)
					p2 = _length;
				else
					p2 = p3;
				if (p2 > p1) {
					// an attribute without a value
					attribute = new EzXmlAttribute;
					attribute->_name.assign( _buffer+p1, p2-p1);
				}
			}
			if (p3 == -1)
				break;
			p1 = p3+1;
			_pos = pos2;
		}
		return (long)outputCollection->size();
	}
};

//////////////////////////////////////////////
struct EzXmlTagPos
{
	int OpenStart, OpenStop;
	int CloseStart;
	int Length;
};

class EzXmlElement;

class EzXmlElement {
public:
	EzXmlElement() : _children(NULL), _tag{ 0 } {}
	~EzXmlElement() {
		clear();
	}

	rstring _name, _attributes, _data;
	objList<EzXmlElement> *_children;
	EzXmlTagPos _tag;

	void clear() {
		objList<EzXmlElement> *t = (objList<EzXmlElement> *)InterlockedExchangePointer((LPVOID*)&_children, NULL);
		if (t)
			delete t;
	}
	void attach(EzXmlElement *src) {
		clear();
		_name.assign(src->_name);
		_attributes.assign(src->_attributes);
		_data.assign(src->_data);
		_tag = src->_tag;
		if (src->_children)
			_children = (objList<EzXmlElement> *)InterlockedExchangePointer((LPVOID*)&src->_children, NULL);
	}

	void setValue( LPCSTR sourceData, long dataLength) {
		// check for a CDATA section, i.g., <![CDATA[ ... ]]>
		if (dataLength > 9+3) {
			if (0 == memcmp( sourceData, "<![CDATA[", 9)) {
				sourceData += 9;
				dataLength -= 9;
				if (0 == memcmp( sourceData+dataLength-3, "]]>", 3))
					dataLength -= 3;
			}
		}
		_data.assign( sourceData, dataLength);
	}
	bool getAttribute( LPCSTR inputName, rstring& outputValue) {
		long i;
		EzXmlAttributes attributes;
		EzXmlAttributesData attributesData( this->_attributes._buffer, this->_attributes._length);
		if (attributesData.collectElements( &attributes))
		{
			for (i = 0; i < (long)attributes.size(); i++)
			{
				EzXmlAttribute *attribute = attributes[i];
				if (attribute->_name.isEqualTo( inputName))
				{
					outputValue.assign( attribute->_value);
					return true;
				}
			}
		}
		return false;
	}
};

class EzXmlElements : public objList<EzXmlElement> {
public:
	EzXmlElements() : _status(0) {}

	int _status;
	RSTRING _sourceData;
};

class EzXmlElementsData : public rstring {
public:
	EzXmlElementsData(LPCSTR dataStart, ULONG dataLength) : rstring(dataStart, dataLength), _tagName{ 0 } {}
	EzXmlElementsData(EzXmlElement* sourceElem) : rstring(sourceElem->_data), _tagName { 0 } {}
	EzXmlElementsData(rstring &ro) : rstring(ro), _tagName{ 0 } {}

	long collectElements( EzXmlElements *outputCollection) {
		EzXmlElement *nextElement;
		outputCollection->_sourceData._buffer = _buffer;
		outputCollection->_sourceData._length = _length;
		outputCollection->_sourceData._maxLength = _maxLength;
		while (findNextElement( &nextElement)) {
			outputCollection->add( nextElement);
		}
		return (long)outputCollection->size();
	}
	bool parseElement(EzXmlElement *elem)
	{
		EzXmlElement *t;
		if (!findNextElement(&t))
			return false;
		elem->attach(t);
		delete t;
		return true;
	}

protected:
	char _tagName[256];
	bool findNextElement( EzXmlElement **outputElement) {
		long tagLength, openTagsClosingBracketLength;
		long openStart, openStop, attributesStart, attributesStop, dataStart, dataStop;
		long p1, p2, p3;
		p1 = findChar( '<');
		if (p1 != -1) {
			p2 = findChar( '>');
			if (p2) {
				openTagsClosingBracketLength = 1;
				if (_buffer[p2-1] == '/')
					openTagsClosingBracketLength++;
				openStart = p1;
				openStop = p2+1;
				_pos = p1;
				p3 = findChar( ' ');
				if (p3 != -1 && p3 < p2)
					tagLength = p3-p1-1;
				else
					tagLength = p2-p1-openTagsClosingBracketLength;
				attributesStart = openStart +tagLength +1;
				attributesStop = openStop -openTagsClosingBracketLength;
				_pos = openStop;
				EzXmlElement* elem = new EzXmlElement;
				elem->_tag.Length = tagLength;
				elem->_tag.OpenStart = openStart;
				elem->_tag.OpenStop = openStop;
				elem->_name.assign( _buffer+openStart+1, tagLength);
				elem->_attributes.assign( _buffer+attributesStart, attributesStop-attributesStart);
				if (openTagsClosingBracketLength > 1) {
					// no closing tag follows. we're done.
					*outputElement = elem;
					return true;
				}
				// a closing tag exists. find it.
				CopyMemory( _tagName, "</", 2);
				CopyMemory( _tagName+2, elem->_name._buffer, elem->_name._length);
				CopyMemory( _tagName+2+elem->_name._length, ">", 2);
				dataStop = findString( _tagName, elem->_name._length+3);
				if (dataStop) {
					dataStart = openStop;
					elem->setValue( _buffer+dataStart, dataStop-dataStart);
					elem->_tag.CloseStart = dataStop;
					*outputElement = elem;
					return true;
				}
				delete elem;
			}
		}
		return false;
	}
};

class EzXml {
public:
	// InputXmlData - [in, writable] XML text data terminated by a null char
	EzXml( LPSTR InputXmlData, long InputLength=0) {
		_dataBuffer = InputXmlData;
		_dataLength = InputLength;
		if (_dataLength) {
			_trailChar = _dataBuffer[_dataLength];
			_dataBuffer[_dataLength] = 0;
		} else
			_trailChar = 0;
		ZeroMemory( _tagName, sizeof(_tagName));
	}
	~EzXml() {
		if (_dataLength)
			_dataBuffer[_dataLength] = _trailChar;
	}
	LPSTR _dataBuffer;
	long _dataLength;
	char _trailChar;

	HRESULT enumElements( LPCSTR basePath, EzXmlElements *baseElems) {
		HRESULT hr = collectElements(basePath, baseElems);
		if (hr == S_OK)
		{
			for (long i = 0; i < (long)baseElems->size(); i++)
			{
				if (FAILED(hr = parseInner((*baseElems)[i])))
					break;
			}
		}
		return hr;
	}

	HRESULT openTree(LPCSTR basePath, EzXmlElement *baseElem) {
		HRESULT hr = openElement(basePath, baseElem);
		if (hr == S_OK && baseElem->_children)
		{
			for (long i = 0; i < (long)baseElem->_children->size(); i++)
			{
				if (FAILED(hr = parseInner((*baseElem->_children)[i])))
					break;
			}
		}
		return hr;
	}

	HRESULT openElement(LPCSTR searchPath, EzXmlElement *baseElem) {
		long openOut1, openOut2, openIn1, openIn2;
		long closeOut1, closeOut2;
		bool closeTagExists;
		EzXmlTagString tag(searchPath);
		_iSearch = 0;

		HRESULT hr = S_FALSE;
		if (!searchOpenTag(tag._first, openOut1, openOut2, openIn1, openIn2, closeTagExists))
			return E_FAIL;
		if (closeTagExists && !searchCloseTag(tag._first, closeOut1, closeOut2))
			return E_FAIL;

		EzXmlElements* outList = new EzXmlElements;

		EzXmlElementsData edata(_dataBuffer + openOut2, closeOut1 - openOut2);
		if (tag._next) {
			EzXmlElements elems;
			if (edata.collectElements(&elems))
			{
				EzXmlTagString nextTag(tag._next);
				for (long i = 0; i < (long)elems.size(); i++)
				{
					EzXmlElement* elem = elems[i];
					if (elem->_name.isEqualTo(nextTag._first))
					{
						if (nextTag._next)
						{
							delete outList;
							EzXml parser2((LPSTR)elem->_data._buffer, elem->_data._length);
							hr = parser2.openElement(nextTag._next, baseElem);
							return hr;
						}
						EzXmlElementsData edata2((LPSTR)elem->_data._buffer, elem->_data._length);
						edata2.collectElements(outList);
						baseElem->_children = outList;
						baseElem->attach(elem);
						return S_OK;
					}
				}
			}
			delete outList;
			return E_FAIL;
		}

		baseElem->_tag.OpenStart = openOut1;
		baseElem->_tag.OpenStop = openOut2;
		baseElem->_tag.CloseStart = closeOut1;
		baseElem->_tag.Length = (int)strlen(tag._first);
		baseElem->_name.assign(_dataBuffer + openOut1 + 1, baseElem->_tag.Length);
		baseElem->_attributes.assign(_dataBuffer + openIn1, openIn2 - openIn1);

		edata.collectElements(outList);
		baseElem->_children = outList;
		return S_OK;
	}

	HRESULT collectElements( LPCSTR searchPath, EzXmlElements* outList) {
		long openOut1, openOut2, openIn1, openIn2;
		long closeOut1, closeOut2;
		bool closeTagExists;
		EzXmlTagString tag(searchPath);
		_iSearch = 0;

		HRESULT hr = S_FALSE;
		while (true) {
			if (!searchOpenTag( tag._first, openOut1, openOut2, openIn1, openIn2, closeTagExists))
				return outList->size()? S_OK:E_FAIL;
			if (closeTagExists) {
				if (!searchCloseTag( tag._first, closeOut1, closeOut2))
					return E_FAIL;
			}
			EzXmlElementsData edata( _dataBuffer+openOut2, closeOut1-openOut2);
			if (tag._next) {
				EzXmlElements elems;
				if (edata.collectElements( &elems))
				{
					EzXmlTagString nextTag(tag._next);
					for (long i = 0; i < (long)elems.size(); i++)
					{
						EzXmlElement* elem = elems[i];
						if (elem->_name.isEqualTo(nextTag._first))
						{
							if (nextTag._next)
							{
								EzXml parser2((LPSTR)elem->_data._buffer, elem->_data._length);
								hr = parser2.collectElements(nextTag._next, outList);
								if (hr != S_OK)
									return hr;
							}
							else
							{
								EzXmlElementsData edata2((LPSTR)elem->_data._buffer, elem->_data._length);
								edata2.collectElements(outList);
								return S_OK;
							}
						}
					}
				}
			}
			else
			{
				edata.collectElements( outList);
				hr = S_OK;
			}
			_iSearch = openOut2;
		}
		return hr;
	}

protected:
	long _iSearch;
	char _tagName[80];

	bool searchOpenTag( LPCSTR targetTag, long &outerStart, long &outerStop, long &innerStart, long &innerStop, bool &closeTagExists) {
		long dataLength;
		LPCSTR p0, p1, p2;
		*_tagName = '<';
		strcpy_s( _tagName+1, sizeof(_tagName)-2, targetTag);
		dataLength = (long) strlen( _tagName);
		p0 = _dataBuffer+_iSearch;
		p1 = strstr( p0, _tagName);
		if (p1 && isalpha(p1[dataLength])) {
			p1 = strstr( p1+dataLength, _tagName);
		}
		if (p1) {
			p2 = strchr( p1, '>');
			if (p2) {
				outerStart = (long)(p1 -_dataBuffer);
				outerStop = (long)(p2+1 -_dataBuffer);
				innerStart = outerStart +dataLength;
				innerStop = outerStop -1;
				if (*(p2-1) == '/') {
					innerStop--;
					closeTagExists = false;
				} else {
					closeTagExists = true;
				}
				_iSearch = outerStop;
				return true;
			}
		}
		return false;
	}
	bool searchCloseTag( LPCSTR targetTag, long &outerStart, long &outerStop) {
		long dataLength;
		LPCSTR p0, p1, p2;
		strcpy_s( _tagName, sizeof(_tagName)-1, "</");
		strcat_s( _tagName, sizeof(_tagName)-1, targetTag);
		strcat_s( _tagName, sizeof(_tagName)-1, ">");
		p0 = _dataBuffer+_iSearch;
		p1 = strstr( p0, _tagName);
		if (p1) {
			p2 = strchr( p1, '>');
			if (p2) {
				dataLength = (long) strlen( _tagName);
				outerStart = (long)(p1 -_dataBuffer);
				outerStop = (long)(p2+1 -_dataBuffer);
				_iSearch = outerStop;
				return true;
			}
		}
		return false;
	}

	HRESULT parseInner(EzXmlElement *elem)
	{
		if (!elem->_data._length)
			return S_FALSE;
		HRESULT hr = S_FALSE;
		EzXmlElementsData edata(elem->_data);
		EzXmlElements *children = new EzXmlElements;
		if (edata.collectElements(children))
		{
			for (long j = 0; j < (long)children->size(); j++)
			{
				if (FAILED(hr = parseInner((*children)[j])))
					break;
			}
			elem->_children = children;
		}
		else
			delete children;
		return hr;
	}
};

#endif//__EZXML_H_
