#pragma once

#include "framework.h"

#include <d3d11.h>

struct Model
{
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
        bool animated = false;

        Split(ID3D11Buffer* pVertexBuffer, ID3D11Buffer* pIndexBuffer, ID3D11InputLayout* pInputLayout, UINT indexCount, DXGI_FORMAT indexFormat, UINT vertexStride, const std::vector<std::pair<ID3D11Texture2D*, ID3D11ShaderResourceView*>>& textures, bool textured, bool animated) noexcept
            : pVertexBuffer(pVertexBuffer)
            , pIndexBuffer(pIndexBuffer)
            , pInputLayout(pInputLayout)
            , indexCount(indexCount)
            , indexFormat(indexFormat)
            , vertexStride(vertexStride)
            , textures(textures)
            , textured(textured)
            , animated(animated)
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

    struct Mesh
    {
        ID3D11Buffer* pModelCB = nullptr;
        std::vector<Split> splits;

        Mesh() noexcept : pModelCB(nullptr)
        {}

        Mesh(Mesh&& rhs) noexcept
        {
            std::swap(splits, rhs.splits);
            std::swap(pModelCB, rhs.pModelCB);
        }

        ~Mesh()
        {
            SAFE_RELEASE(pModelCB);
        }
    };

    struct Node
    {
        bool useMatrix;
        union
        {
            DirectX::XMMATRIX matrix;
            struct {
                DirectX::XMVECTOR rotation;
                DirectX::XMVECTOR translation;
                DirectX::XMVECTOR scale;
            } transform;
        };

        Node(const DirectX::XMMATRIX& matrix = DirectX::XMMatrixIdentity())
            : useMatrix(true)
            , matrix(matrix)
        {}

        Node(const DirectX::XMVECTOR& rotation, const DirectX::XMVECTOR& translation, const DirectX::XMVECTOR& scale)
            : useMatrix(false)
        {
            transform.rotation = rotation;
            transform.translation = translation;
            transform.scale = scale;
        }

        std::vector<int> children;
    };

    struct AnimationSampler
    {
        // Assumed it is always linear interpolation here
        std::vector<float> timeKeys;                // Time key values
        std::vector<DirectX::XMVECTOR> keys;        // Key values
    };

    struct AnimationChannel
    {
        enum Type
        {
            Rotation,
            Translation,
            Scale
        };

        int animSamplerIdx;
        int nodeIdx;
        Type type;
    };

    struct Animation
    {
        float maxAnimationTime = 0.0f;
        float minAnimationTime = std::numeric_limits<float>::max();
        std::vector<AnimationSampler> animationSamplers;
        std::vector<AnimationChannel> animationChannels;
    };

    std::vector<Mesh> meshes;

    Model() noexcept
        : skinned(false)
        , animationTime(0.0)
        , currentAnim(0)
        , animStart(true)
        , curAnimName("")
        , rootNodeIdx(0)
        , pCB(nullptr)
    {}

    ~Model()
    {
        SAFE_RELEASE(pCB);
    }

    void ApplyAnimation();
    void UpdateMatrices(ID3D11DeviceContext* pDeviceContext);
    void UpdateNodeMatrices(int nodeIdx, const DirectX::XMMATRIX& parent);

    bool skinned;
    std::map<std::string, Animation> animations;
    double animationTime;
    int currentAnim;
    bool animStart; // Set this to not to move to the next frame, when updating frame for the first time in animation
    std::string curAnimName;
    int rootNodeIdx;
    ID3D11Buffer* pCB;

    std::vector<Node> nodes;

    std::vector<DirectX::XMMATRIX> nodeMatrices;
    std::vector<DirectX::XMMATRIX> nodeNormalMatrices;

    std::vector<int> joints;
    std::vector<DirectX::XMMATRIX> invBindMatrices;
};

struct LoadParams
{
    ID3DBlob* pPlainVertexShaderCode = nullptr;
    ID3DBlob* pTexturedVertexShaderCode = nullptr;
    ID3DBlob* pAnimatedTexturedVertexShaderCode = nullptr;
};

bool LoadModel(ID3D11Device* pDevice, const LoadParams& loadParams, const std::string& filename, Model& model);
