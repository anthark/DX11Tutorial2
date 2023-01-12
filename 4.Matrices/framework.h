// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <assert.h>

#include <string>
#include <vector>

#include <dxgi.h>
#include <d3d11.h>

#include <DirectXMath.h>

#define ASSERT_RETURN(expr, returnValue) \
{\
   bool value = (expr);\
   assert(value);\
   if (!value)\
   {\
      return returnValue;\
   }\
}

#define SAFE_RELEASE(p)\
{\
    if (p != nullptr)\
    {\
        p->Release();\
        p = nullptr;\
    }\
}

inline HRESULT SetResourceName(ID3D11DeviceChild* pResource, const std::string& name)
{
    return pResource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)name.length(), name.c_str());
}

inline std::wstring Extension(const std::wstring& filename)
{
    size_t dotPos = filename.rfind(L'.');
    if (dotPos != std::wstring::npos)
    {
        return filename.substr(dotPos + 1);
    }
    return L"";
}

inline std::string WCSToMBS(const std::wstring& wstr)
{
    size_t len = wstr.length();

    // We suppose that on Windows platform wchar_t is 2 byte length
    std::vector<char> res;
    res.resize(len + 1);

    size_t resLen = 0;
    wcstombs_s(&resLen, res.data(), res.size(), wstr.c_str(), len);

    return res.data();
}
