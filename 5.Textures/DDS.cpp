#include "framework.h"

#include "DDS.h"

namespace
{

const UINT32 DDSSignature = 0x20534444;     ///< DDS file signature

#pragma pack(push)
#pragma pack(1)

/** Pixel format description structure */
struct PixelFormat
{
    UINT32 size;                ///< Structure size
    UINT32 flags;               ///< Format flags
    UINT32 fourCC;              ///< Four CC code
    UINT32 bitCount;            ///< Bit count
    UINT32 RMask, GMask, BMask, AMask;
};

/** DDS header */
struct DDSHeader
{
    UINT32 size;                ///< Header structure size
    UINT32 flags;               ///< DDS flags
    UINT32 height;              ///< Image height
    UINT32 width;               ///< Image width
    UINT32 pitchOrLinearSize;   ///< Pitch or linear size
    UINT32 depth;               ///< 3D texture depth
    UINT32 mipMapCount;         ///< Number of mip maps
    UINT32 reserved[11];
    PixelFormat pixelFormat;
    UINT32 caps, caps2, caps3, caps4;
    UINT32 reserved2;
};

/** DDS 10 header */
struct DDS10Header
{
    UINT32 dxgiFormat;
    UINT32 resourceDimension;
    UINT32 miscFlag;
    UINT32 arraySize;
    UINT32 miscFlags2;
};

#pragma pack(pop)

const UINT32 DDPF_FOURCC = 0x4;
const UINT32 DDPF_ALPHAPIXELS = 0x1;

const UINT32 DDSD_CAPS = 0x1;
const UINT32 DDSD_HEIGHT = 0x2;
const UINT32 DDSD_WIDTH = 0x4;
const UINT32 DDSD_PITCH = 0x8;
const UINT32 DDSD_PIXELFORMAT = 0x1000;
const UINT32 DDSD_MIPMAPCOUNT = 0x20000;
const UINT32 DDSD_LINEARSIZE = 0x80000;
const UINT32 DDSD_DEPTH = 0x800000;

bool HaveDXT10Header(const DDSHeader& header)
{
    if ((header.pixelFormat.flags & DDPF_FOURCC) != 0)
    {
        char fourCC[5] = { 0 };
        memcpy(fourCC, &header.pixelFormat.fourCC, 4);

        return strcmp(fourCC, "DX10") == 0;
    }

    return false;
}

bool ValidateFlags(const DDSHeader& header)
{
    return (header.flags & DDSD_CAPS) != 0
        && (header.flags & DDSD_HEIGHT) != 0
        && (header.flags & DDSD_WIDTH) != 0
        && (header.flags & DDSD_PIXELFORMAT) != 0;
}

DXGI_FORMAT GetTextureFormat(const DDSHeader& header)
{
    char fourCC[5] = { 0 };
    memcpy(fourCC, &header.pixelFormat.fourCC, 4);

    if (strcmp(fourCC, "DXT1") == 0)
    {
        return DXGI_FORMAT_BC1_UNORM;
    }
    if (strcmp(fourCC, "DXT1") == 0 && (header.pixelFormat.flags & DDPF_ALPHAPIXELS) != 0)
    {
        return DXGI_FORMAT_BC1_UNORM;
    }
    if (strcmp(fourCC, "DXT3") == 0)
    {
        return DXGI_FORMAT_BC2_UNORM;
    }
    if (strcmp(fourCC, "DXT5") == 0)
    {
        return DXGI_FORMAT_BC3_UNORM;
    }
    return DXGI_FORMAT_UNKNOWN;
}

}

bool LoadDDS(const std::wstring& filepath, TextureDesc& desc)
{
    FILE* pFile = nullptr;
    _wfopen_s(&pFile, filepath.c_str(), L"rb");
    if (pFile == nullptr)
    {
        return false;
    }

    // Try to read signature
    UINT32 signature = 0;
    if (fread(&signature, 1, sizeof(UINT32), pFile) != sizeof(UINT32)
        || signature != DDSSignature)
    {
        fclose(pFile);
        return false;
    }

    // Read DDS header
    DDSHeader header;
    memset(&header, 0, sizeof(DDSHeader));
    size_t readSize = fread(&header, 1, sizeof(header), pFile);
    if (readSize != sizeof(DDSHeader) || readSize != header.size)
    {
        fclose(pFile);
        return false;
    }

    // Check for DXT10 header presence
    DDS10Header header10;
    memset(&header10, 0, sizeof(DDS10Header));
    if (HaveDXT10Header(header))
    {
        readSize = fread(&header10, 1, sizeof(DDS10Header), pFile);
        if (readSize != sizeof(DDS10Header))
        {
            fclose(pFile);
            return false;
        }
    }

    // Validate header
    if (!ValidateFlags(header))
    {
        fclose(pFile);
        return false;
    }

    // Read pitch
    desc.pitch = (header.flags & DDSD_PITCH) != 0 ? (UINT32)header.pitchOrLinearSize : 0;

    // Read mipmap count
    desc.mipmapsCount = (header.flags & DDSD_MIPMAPCOUNT) != 0 ? (UINT32)header.mipMapCount : 1;

    // Read texture format
    desc.fmt = GetTextureFormat(header);
    if (desc.fmt == DXGI_FORMAT_UNKNOWN)
    {
        fclose(pFile);
        return false;
    }

    // Setup image size
    desc.width = header.width;
    desc.height = header.height;

    // Get data size
    UINT32 dataSize = (header.flags & DDSD_LINEARSIZE) != 0 ? (UINT32)header.pitchOrLinearSize : 0;
    if (dataSize == 0)
    {
        long long curPos = _ftelli64(pFile);
        fseek(pFile, 0, SEEK_END);
        dataSize = (UINT32)(_ftelli64(pFile) - curPos);
        fseek(pFile, (int)curPos, SEEK_SET);
    }
    else
    {
        UINT32 levelSize = dataSize / 4;
        // We have top level size - let's calculate the whole size
        for (UINT32 i = 1; i < header.mipMapCount; i++)
        {
            dataSize += levelSize;
            levelSize = std::max(16u, levelSize / 4);
        }
    }

    desc.pData = malloc(dataSize);
    readSize = fread(desc.pData, 1, dataSize, pFile);
    if (readSize != dataSize)
    {
        free(desc.pData);
        desc.pData = nullptr;
        fclose(pFile);
        return false;
    }

    fclose(pFile);

    return true;
}
