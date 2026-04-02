#include "Model.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

namespace
{

struct ModelCB
{
    DirectX::XMMATRIX trans;
    DirectX::XMMATRIX transNormal;
};

bool ScanNode(ID3D11Device* pDevice, const LoadParams& loadParams, const tinygltf::Model& model, int nodeIdx, const DirectX::XMMATRIX& parent, std::map<int, std::pair<ID3D11Texture2D*, ID3D11ShaderResourceView*>>& allTextures, std::vector<Model>& models)
{
    const tinygltf::Node& node = model.nodes[nodeIdx];

    // Load nodes matrix/transform
#pragma region transform
    DirectX::XMMATRIX m;
    if (!node.matrix.empty())
    {
        m = DirectX::XMMatrixSet(
            (float)node.matrix[0],  (float)node.matrix[1],  (float)node.matrix[2],  (float)node.matrix[3],
            (float)node.matrix[4],  (float)node.matrix[5],  (float)node.matrix[6],  (float)node.matrix[7],
            (float)node.matrix[8],  (float)node.matrix[9],  (float)node.matrix[10], (float)node.matrix[11],
            (float)node.matrix[12], (float)node.matrix[13], (float)node.matrix[14], (float)node.matrix[15]
        );
    }
    else
    {
        m = DirectX::XMMatrixIdentity();
        if (!node.scale.empty())
        {
            m = DirectX::XMMatrixMultiply(m,
                DirectX::XMMatrixScaling((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]));
        }
        if (!node.rotation.empty())
        {
            DirectX::FXMVECTOR q = DirectX::XMVectorSet((float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2], (float)node.rotation[3]);
            m = DirectX::XMMatrixMultiply(m,
                DirectX::XMMatrixRotationQuaternion(q));
        }
        if (!node.translation.empty())
        {
            m = DirectX::XMMatrixMultiply(m,
                DirectX::XMMatrixTranslation((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]));
        }
    }
#pragma endregion transform

    m = DirectX::XMMatrixMultiply(m, parent);

    // Load node's geometry
#pragma region geometry
    if (node.mesh != -1)
    {
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];

        ModelCB cb;
        cb.trans = m;
        cb.transNormal = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, m));

        models.resize(models.size() + 1);
        Model& newModel = models.back();

        D3D11_BUFFER_DESC modelBufferDesc = {};
        modelBufferDesc.ByteWidth = (UINT)sizeof(ModelCB);
        modelBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        modelBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        modelBufferDesc.CPUAccessFlags = 0;
        modelBufferDesc.MiscFlags = 0;
        modelBufferDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &cb;
        data.SysMemPitch = (UINT)sizeof(ModelCB);
        data.SysMemSlicePitch = 0;

        ID3D11Buffer* pModelCB = nullptr;
        HRESULT hr = pDevice->CreateBuffer(&modelBufferDesc, &data, &pModelCB);
        if (FAILED(hr))
        {
            return false;
        }

        newModel.pModelCB = pModelCB;

        // Splits
        for (int i = 0; i < mesh.primitives.size(); i++)
        {
            const tinygltf::Primitive& prim = mesh.primitives[i];
            assert(prim.mode == TINYGLTF_MODE_TRIANGLES);

            std::vector<std::pair<ID3D11Texture2D*, ID3D11ShaderResourceView*>> textures = {};
            // Parse material
            // -->
            const tinygltf::Material& material = model.materials[prim.material];
            int diffuseIndex = material.pbrMetallicRoughness.baseColorTexture.index;
            if (diffuseIndex == -1)
            {
                auto it = material.extensions.find("KHR_materials_pbrSpecularGlossiness");
                if (it != material.extensions.end() && it->second.Type() == tinygltf::OBJECT_TYPE)
                {
                    if (it->second.Has("diffuseTexture"))
                    {
                        auto value = it->second.Get("diffuseTexture").Get("index");

                        diffuseIndex = value.GetNumberAsInt();
                    }
                }
            }
            if (diffuseIndex != -1)
            {
                auto it = allTextures.find(diffuseIndex);
                if (it == allTextures.end())
                {
                    const tinygltf::Image& image = model.images[model.textures[diffuseIndex].source];

                    assert(image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);
                    assert(image.component == 4);
                    assert(image.bits == 8);

                    D3D11_TEXTURE2D_DESC desc = {};
                    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                    desc.ArraySize = 1;
                    desc.MipLevels = 1;
                    desc.Usage = D3D11_USAGE_IMMUTABLE;
                    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                    desc.CPUAccessFlags = 0;
                    desc.MiscFlags = 0;
                    desc.SampleDesc.Count = 1;
                    desc.SampleDesc.Quality = 0;
                    desc.Height = image.height;
                    desc.Width = image.width;

                    const tinygltf::BufferView& imageView = model.bufferViews[image.bufferView];

                    D3D11_SUBRESOURCE_DATA texData = {};
                    texData.pSysMem = image.image.data();
                    texData.SysMemPitch = image.width * 4;

                    ID3D11Texture2D* pTexture = nullptr;
                    hr = pDevice->CreateTexture2D(&desc, &texData, &pTexture);
                    if (FAILED(hr))
                    {
                        return false;
                    }
                    ID3D11ShaderResourceView* pTextureView = nullptr;
                    hr = pDevice->CreateShaderResourceView(pTexture, nullptr, &pTextureView);
                    if (FAILED(hr))
                    {
                        return false;
                    }
                    allTextures[diffuseIndex] = std::make_pair(pTexture, pTextureView);
                    it = allTextures.find(diffuseIndex);
                }
                if (it != allTextures.end())
                {
                    textures.push_back(it->second);
                    it->second.first->AddRef();
                    it->second.second->AddRef();
                }
                else
                {
                    textures.push_back({ nullptr,nullptr });
                }
            }
            // <--

            int posIdx = prim.attributes.find("POSITION")->second;
            int normIdx = prim.attributes.find("NORMAL")->second;
            int uvIdx = -1;
            if (prim.attributes.find("TEXCOORD_0") != prim.attributes.end())
            {
                uvIdx = prim.attributes.find("TEXCOORD_0")->second;
            }

            int idxIdx = prim.indices;

            const tinygltf::Accessor& pos = model.accessors[posIdx];
            const tinygltf::Accessor& norm = model.accessors[normIdx];
            const tinygltf::Accessor& indices = model.accessors[idxIdx];

            assert(pos.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
            assert(pos.type == TINYGLTF_TYPE_VEC3);
            assert(norm.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
            assert(norm.type == TINYGLTF_TYPE_VEC3);
            assert(indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT || indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
            assert(indices.type == TINYGLTF_TYPE_SCALAR);

            const tinygltf::BufferView& posView = model.bufferViews[pos.bufferView];
            const tinygltf::BufferView& normView = model.bufferViews[norm.bufferView];
            const tinygltf::BufferView& indicesView = model.bufferViews[indices.bufferView];

            const float* pPos = reinterpret_cast<const float*>(model.buffers[posView.buffer].data.data() + posView.byteOffset + pos.byteOffset);
            const float* pNorm = reinterpret_cast<const float*>(model.buffers[normView.buffer].data.data() + normView.byteOffset + norm.byteOffset);
            const float* pUV = nullptr;
            if (uvIdx != -1)
            {
                const tinygltf::Accessor& uv = model.accessors[uvIdx];
                const tinygltf::BufferView& uvView = model.bufferViews[uv.bufferView];
                pUV = reinterpret_cast<const float*>(model.buffers[uvView.buffer].data.data() + uvView.byteOffset + uv.byteOffset);
            }

            // Collect all values to interlaced data
            UINT vertexStride = uvIdx == -1 ? 6 : 8;

            std::vector<float> vertexData;
            vertexData.resize(pos.count * vertexStride);
            for (size_t i = 0; i < pos.count; i++)
            {
                // Position
                for (int j = 0; j < 3; j++)
                {
                    vertexData[i * vertexStride + j] = pPos[i * 3 + j];
                }
                // Normal
                for (int j = 0; j < 3; j++)
                {
                    vertexData[i * vertexStride + 3 + j] = pNorm[i * 3 + j];
                }
                // Texcoord
                if (pUV != nullptr)
                {
                    for (int j = 0; j < 2; j++)
                    {
                        vertexData[i * vertexStride + 6 + j] = pUV[i * 2 + j];
                    }
                }
            }

            D3D11_BUFFER_DESC vertexBufferDesc = {};
            vertexBufferDesc.ByteWidth = (UINT)(vertexData.size() * sizeof(float));
            vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
            vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vertexBufferDesc.CPUAccessFlags = 0;
            vertexBufferDesc.MiscFlags = 0;
            vertexBufferDesc.StructureByteStride = 0;

            D3D11_SUBRESOURCE_DATA data;
            data.pSysMem = vertexData.data();
            data.SysMemPitch = (UINT)(vertexData.size() * sizeof(float));
            data.SysMemSlicePitch = 0;

            ID3D11Buffer* pVertexBuffer = nullptr;
            hr = pDevice->CreateBuffer(&vertexBufferDesc, &data, &pVertexBuffer);
            if (FAILED(hr))
            {
                return false;
            }

            const void* pIndices = reinterpret_cast<const void*>(model.buffers[indicesView.buffer].data.data() + indicesView.byteOffset + indices.byteOffset);

            UINT indexDataSize = 0;
            if (indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            {
                indexDataSize = (UINT)(indices.count * sizeof(UINT32));
            }
            else
            {
                indexDataSize = (UINT)(indices.count * sizeof(UINT16));
            }

            D3D11_BUFFER_DESC indexBufferDesc = {};
            indexBufferDesc.ByteWidth = (UINT)indexDataSize;
            indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
            indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            indexBufferDesc.CPUAccessFlags = 0;
            indexBufferDesc.MiscFlags = 0;
            indexBufferDesc.StructureByteStride = 0;
            data.pSysMem = pIndices;
            data.SysMemPitch = (UINT)indexDataSize;

            ID3D11Buffer* pIndexBuffer = nullptr;
            hr = pDevice->CreateBuffer(&indexBufferDesc, &data, &pIndexBuffer);
            if (FAILED(hr))
            {
                return false;
            }

            ID3D11InputLayout* pInputLayout = nullptr;

            D3D11_INPUT_ELEMENT_DESC input[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
            };

            D3D11_INPUT_ELEMENT_DESC texturedInput[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
            };

            ID3DBlob* pVertexShaderCode = nullptr;
            if (uvIdx == -1)
            {
                pVertexShaderCode = loadParams.pPlainVertexShaderCode;
            }
            else
            {
                pVertexShaderCode = loadParams.pTexturedVertexShaderCode;
            }
            if (uvIdx == -1)
            {
                hr = pDevice->CreateInputLayout(input, 2, pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &pInputLayout);
            }
            else
            {
                hr = pDevice->CreateInputLayout(texturedInput, 3, pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &pInputLayout);
            }
            if (FAILED(hr))
            {
                return false;
            }

            Split split{ pVertexBuffer, pIndexBuffer, pInputLayout, (UINT)indices.count,
                indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT,
                vertexStride * sizeof(float), textures, uvIdx != -1
            };
            newModel.splits.emplace_back(std::move(split));
        }
    }
#pragma endregion geometry

    for (int i = 0; i < node.children.size(); i++)
    {
        if (!ScanNode(pDevice, loadParams, model, node.children[i], m, allTextures, models))
        {
            return false;
        }
    }
    return true;
}

}

bool LoadModel(ID3D11Device* pDevice, const LoadParams& loadParams, const std::string& filename, std::vector<Model>& models)
{
    tinygltf::Model* pModel = new tinygltf::Model();
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    DirectX::XMMATRIX m = DirectX::XMMatrixIdentity();

    std::map<int, std::pair<ID3D11Texture2D*, ID3D11ShaderResourceView*>> allTextures = {};

    bool ret = loader.LoadBinaryFromFile(pModel, &err, &warn, filename);
    if (ret)
    {
        ret = ScanNode(pDevice, loadParams, *pModel, 0, m, allTextures, models);
    }
    delete pModel;
    for (auto& it : allTextures)
    {
        it.second.first->Release();
        it.second.second->Release();
    }
    return ret;
}