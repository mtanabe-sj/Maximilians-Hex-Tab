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
#ifndef __HEXTABCONFIG_H_
#define __HEXTABCONFIG_H_

DEFINE_GUID(IID_IHexTabConfig, 0x5153840A, 0xA25D, 0x11D1, 0xBD, 0xB2, 0x0, 0x0, 0x86, 0x13, 0x9E, 0x6F );


// configuration parameters available to GetParam and SetParam.
enum HDCPID
{
	HDCPID_NONE,
	HDCPID_CONTROLLER,
	HDCPID_BPR,
	HDCPID_METAFILEID,
	HDCPID_HOST_HWND,
	HDCPID_HOST_NOTIF_COMMAND_ID,
};

// available types of results that can be queried by QueryCommand.
enum HDCQCTYPE
{
	HDCQCTYPE_STATE,
};

// possible result values returned by QueryCommand.
enum HDCQCRESULT
{
	HDCQCRESULT_UNAVAIL,
	HDCQCRESULT_AVAIL,
	HDCQCRESULT_BUSY,
};

// print tasks.
enum HDCPrintTask
{
	TskStartJob,
	TskStartDoc,
	TskStartPage,
	TskPrintPage,
	TskFinishPage,
	TskFinishDoc,
	TskFinishJob,
	TskAbortJob
};


#undef INTERFACE
#define INTERFACE IHexTabConfig

DECLARE_INTERFACE_(IHexTabConfig, IUnknown)
{
	// *** IUnknown methods ***
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR *ppvObj)  PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	// *** BHConfig methods ***
	// read the value of a specified configuration parameter from the hexdump server.
	STDMETHOD(GetParam) (THIS_ HDCPID paramId, VARIANT *varValue) PURE;
	// set a configuration parameter to a specified value on the hexdump server.
	STDMETHOD(SetParam) (THIS_ HDCPID paramId, VARIANT *varValue) PURE;
	// send a command to the hexdump server.
	STDMETHOD(SendCommand) (THIS_ VARIANT *varCommand, VARIANT *varResult) PURE;
	// ask the hexdump server about availability of a command.
	STDMETHOD(QueryCommand) (THIS_ VARIANT *varCommand, HDCQCTYPE queryType, HDCQCRESULT *queryResult) PURE;
};

#endif //__HEXTABCONFIG_H_
