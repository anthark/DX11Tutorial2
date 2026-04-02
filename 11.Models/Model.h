#pragma once

#include <string>
#include <d3d11.h>

#include "framework.h"

struct Split
{
    ID3D11Buffer* pVertexBuffer = nullptr;
    ID3D11Buffer* pIndexBuffer = nullptr;
    ID3D11InputLayout* pInputLayout = nullptr;
    UINT indexCount = 0;
    DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
    UINT vertexStride = 0;
    std::vector<std::pair<ID3D11Texture2D*, ID3D11ShaderResourceView*>> textures = {};
    bool textured = false;

    Split(ID3D11Buffer* pVertexBuffer, ID3D11Buffer* pIndexBuffer, ID3D11InputLayout* pInputLayout, UINT indexCount, DXGI_FORMAT indexFormat, UINT vertexStride, const std::vector<std::pair<ID3D11Texture2D*, ID3D11ShaderResourceView*>>& textures, bool textured) noexcept
        : pVertexBuffer(pVertexBuffer)
        , pIndexBuffer(pIndexBuffer)
        , pInputLayout(pInputLayout)
        , indexCount(indexCount)
        , indexFormat(indexFormat)
        , vertexStride(vertexStride)
        , textures(textures)
        , textured(textured)
    {}

    Split(Split&& rhs) noexcept
        : pVertexBuffer(rhs.pVertexBuffer)
        , pIndexBuffer(rhs.pIndexBuffer)
        , pInputLayout(rhs.pInputLayout)
        , indexCount(rhs.indexCount)
        , indexFormat(rhs.indexFormat)
        , vertexStride(rhs.vertexStride)
    {
        rhs.pVertexBuffer = rhs.pIndexBuffer = nullptr;
        rhs.pInputLayout = nullptr;
        rhs.indexCount = 0;
        rhs.indexFormat = DXGI_FORMAT_UNKNOWN;
        rhs.vertexStride = 0;
        textures = std::move(rhs.textures);
        textured = std::move(rhs.textured);
    }

    ~Split()
    {
        SAFE_RELEASE(pVertexBuffer);
        SAFE_RELEASE(pIndexBuffer);
        SAFE_RELEASE(pInputLayout);
        for (int i = 0; i < textures.size(); i++)
        {
            SAFE_RELEASE(textures[i].first);
            SAFE_RELEASE(textures[i].second);
        }
    }
};

struct Model
{
    ID3D11Buffer* pModelCB = nullptr;
    std::vector<Split> splits;

    Model() noexcept : pModelCB(nullptr)
    {}

    Model(Model&& rhs) noexcept
    {
        std::swap(splits, rhs.splits);
        std::swap(pModelCB, rhs.pModelCB);
    }

    ~Model()
    {
        SAFE_RELEASE(pModelCB);
    }
};

struct LoadParams
{
    ID3DBlob* pPlainVertexShaderCode = nullptr;
    ID3DBlob* pTexturedVertexShaderCode = nullptr;
};

bool LoadModel(ID3D11Device* pDevice, const LoadParams& loadParams, const std::string& filename, std::vector<Model>& models);
