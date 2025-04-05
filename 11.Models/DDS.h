#pragma once

#include <string>

struct TextureDesc
{
    UINT32 pitch = 0;
    UINT32 mipmapsCount = 0;

    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;

    UINT32 width = 0;
    UINT32 height = 0;

    void* pData = nullptr;
};

bool LoadDDS(const std::wstring& filepath, TextureDesc& desc, bool singleMip = false);
