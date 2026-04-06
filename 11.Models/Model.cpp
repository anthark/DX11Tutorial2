#include "Model.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

namespace
{

struct MeshCB
{
    DirectX::XMMATRIX trans;
    DirectX::XMMATRIX transNormal;
};

struct ModelCB
{
    static const int MaxNodes = 200;

    DirectX::XMMATRIX nodeTransforms[MaxNodes];
    DirectX::XMMATRIX nodeNormalTransforms[MaxNodes];
    int jointIndices[MaxNodes];
};

struct Vertex
{
    float pos[3];
    float norm[3];
};

struct TexturedVertex : Vertex
{
    float uv[2];
};

struct AnimatedTexturedVertex : TexturedVertex
{
    unsigned short joints[4];
    float weights[4];
};

bool ScanNode(ID3D11Device* pDevice, const LoadParams& loadParams, const tinygltf::Model& model, int nodeIdx, const DirectX::XMMATRIX& parent, std::map<int, std::pair<ID3D11Texture2D*, ID3D11ShaderResourceView*>>& allTextures, std::vector<Model::Mesh>& models, std::vector<Model::Node>& nodes)
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
    Model::Node newNode(m);
#pragma endregion transform

    m = DirectX::XMMatrixMultiply(m, parent);

    // Load node's geometry
#pragma region geometry
    if (node.mesh != -1)
    {
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];

        MeshCB cb;
        cb.trans = m;
        cb.transNormal = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, m));

        models.resize(models.size() + 1);
        Model::Mesh& newModel = models.back();

        D3D11_BUFFER_DESC modelBufferDesc = {};
        modelBufferDesc.ByteWidth = (UINT)sizeof(MeshCB);
        modelBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        modelBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        modelBufferDesc.CPUAccessFlags = 0;
        modelBufferDesc.MiscFlags = 0;
        modelBufferDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &cb;
        data.SysMemPitch = (UINT)sizeof(MeshCB);
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
            int jointsIdx = -1;
            if (prim.attributes.find("JOINTS_0") != prim.attributes.end())
            {
                jointsIdx = prim.attributes.find("JOINTS_0")->second;
            }
            int weightIdx = -1;
            if (prim.attributes.find("WEIGHTS_0") != prim.attributes.end())
            {
                weightIdx = prim.attributes.find("WEIGHTS_0")->second;
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
            const unsigned short* pJoints = nullptr;
            const float* pWeights = nullptr;
            if (uvIdx != -1)
            {
                const tinygltf::Accessor& uv = model.accessors[uvIdx];
                const tinygltf::BufferView& uvView = model.bufferViews[uv.bufferView];
                pUV = reinterpret_cast<const float*>(model.buffers[uvView.buffer].data.data() + uvView.byteOffset + uv.byteOffset);
            }
            if (jointsIdx != -1)
            {
                const tinygltf::Accessor& joints = model.accessors[jointsIdx];
                const tinygltf::BufferView& jointsView = model.bufferViews[joints.bufferView];
                pJoints = reinterpret_cast<const unsigned short*>(model.buffers[jointsView.buffer].data.data() + jointsView.byteOffset + joints.byteOffset);
            }
            if (weightIdx != -1)
            {
                const tinygltf::Accessor& weights = model.accessors[weightIdx];
                const tinygltf::BufferView& weightsView = model.bufferViews[weights.bufferView];
                pWeights = reinterpret_cast<const float*>(model.buffers[weightsView.buffer].data.data() + weightsView.byteOffset + weights.byteOffset);
            }

            const D3D11_INPUT_ELEMENT_DESC input[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
            };

            const D3D11_INPUT_ELEMENT_DESC texturedInput[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
            };

            const D3D11_INPUT_ELEMENT_DESC animatedTexturedInput[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"JOINTS", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0}
            };

            // Collect all values to interlaced data
            UINT vertexStride = 0;
            std::vector<byte> vertexData;
            const D3D11_INPUT_ELEMENT_DESC* pInput = nullptr;
            UINT inputSize = 0;
            if (uvIdx == -1)
            {
                vertexData.resize(pos.count * sizeof(Vertex));
                Vertex* pVerts = reinterpret_cast<Vertex*>(vertexData.data());
                for (size_t i = 0; i < pos.count; i++)
                {
                    for (int j = 0; j < 3; j++)
                    {
                        pVerts[i].pos[j] = pPos[i * 3 + j];
                        pVerts[i].norm[j] = pNorm[i * 3 + j];
                    }
                }
                vertexStride = sizeof(Vertex);
                pInput = input;
                inputSize = (UINT)sizeof(input) / sizeof(D3D11_INPUT_ELEMENT_DESC);
            }
            else if (jointsIdx == -1)
            {
                vertexData.resize(pos.count * sizeof(TexturedVertex));
                TexturedVertex* pVerts = reinterpret_cast<TexturedVertex*>(vertexData.data());
                for (size_t i = 0; i < pos.count; i++)
                {
                    for (int j = 0; j < 3; j++)
                    {
                        pVerts[i].pos[j] = pPos[i * 3 + j];
                        pVerts[i].norm[j] = pNorm[i * 3 + j];
                    }
                    for (int j = 0; j < 2; j++)
                    {
                        pVerts[i].uv[j] = pUV[i * 2 + j];
                    }
                }
                vertexStride = sizeof(TexturedVertex);
                pInput = texturedInput;
                inputSize = (UINT)sizeof(texturedInput) / sizeof(D3D11_INPUT_ELEMENT_DESC);
            }
            else
            {
                vertexData.resize(pos.count * sizeof(AnimatedTexturedVertex));
                AnimatedTexturedVertex* pVerts = reinterpret_cast<AnimatedTexturedVertex*>(vertexData.data());
                for (size_t i = 0; i < pos.count; i++)
                {
                    for (int j = 0; j < 3; j++)
                    {
                        pVerts[i].pos[j] = pPos[i * 3 + j];
                        pVerts[i].norm[j] = pNorm[i * 3 + j];
                    }
                    for (int j = 0; j < 2; j++)
                    {
                        pVerts[i].uv[j] = pUV[i * 2 + j];
                    }
                    for (int j = 0; j < 4; j++)
                    {
                        pVerts[i].joints[j] = pJoints[i * 4 + j];
                        pVerts[i].weights[j] = pWeights[i * 4 + j];
                    }
                }
                vertexStride = sizeof(AnimatedTexturedVertex);
                pInput = animatedTexturedInput;
                inputSize = (UINT)sizeof(animatedTexturedInput) / sizeof(D3D11_INPUT_ELEMENT_DESC);
            }

            D3D11_BUFFER_DESC vertexBufferDesc = {};
            vertexBufferDesc.ByteWidth = (UINT)vertexData.size();
            vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
            vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vertexBufferDesc.CPUAccessFlags = 0;
            vertexBufferDesc.MiscFlags = 0;
            vertexBufferDesc.StructureByteStride = 0;

            D3D11_SUBRESOURCE_DATA data;
            data.pSysMem = vertexData.data();
            data.SysMemPitch = (UINT)vertexData.size();
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

            ID3DBlob* pVertexShaderCode = nullptr;
            if (uvIdx == -1)
            {
                pVertexShaderCode = loadParams.pPlainVertexShaderCode;
            }
            else if (jointsIdx == -1)
            {
                pVertexShaderCode = loadParams.pTexturedVertexShaderCode;
            }
            else
            {
                pVertexShaderCode = loadParams.pAnimatedTexturedVertexShaderCode;
            }
            hr = pDevice->CreateInputLayout(pInput, inputSize, pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &pInputLayout);
            if (FAILED(hr))
            {
                return false;
            }

            Model::Split split{ pVertexBuffer, pIndexBuffer, pInputLayout, (UINT)indices.count,
                indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT,
                vertexStride, textures, uvIdx != -1, jointsIdx != 1
            };
            newModel.splits.emplace_back(std::move(split));
        }
    }
#pragma endregion geometry

    if (nodeIdx >= nodes.size())
    {
        nodes.resize(nodeIdx + 1);
        nodes[nodeIdx] = std::move(newNode);
    }
    for (int i = 0; i < node.children.size(); i++)
    {
        if (!ScanNode(pDevice, loadParams, model, node.children[i], m, allTextures, models, nodes))
        {
            return false;
        }
    }
    nodes[nodeIdx].children = node.children;

    return true;
}

}

void Model::ApplyAnimation()
{
    const auto& anim = animations[curAnimName];

    if (anim.maxAnimationTime == 0.0f) // No animation
    {
        return;
    }

    double localTime = animationTime;
    while (localTime > anim.maxAnimationTime)
    {
        localTime -= int(localTime / anim.maxAnimationTime) * anim.maxAnimationTime;
    }

    for (int i = 0; i < anim.animationChannels.size(); i++)
    {
        const auto& sampler = anim.animationSamplers[anim.animationChannels[i].animSamplerIdx];

        DirectX::XMVECTOR value;
        if (sampler.timeKeys.size() == 1)
        {
            value = sampler.keys[0];
        }
        else
        {
            int idx0, idx1;
            float ratio;

            if (localTime < sampler.timeKeys.front())
            {
                idx0 = (int)sampler.keys.size() - 1;
                idx1 = 0;

                ratio = (float)localTime / sampler.timeKeys.front();
            }
            else
            {
                for (int j = 1; j < sampler.timeKeys.size(); j++)
                {
                    if (localTime < sampler.timeKeys[j])
                    {
                        idx0 = j - 1;
                        idx1 = j;

                        ratio = ((float)localTime - sampler.timeKeys[j - 1]) / (sampler.timeKeys[j] - sampler.timeKeys[j - 1]);

                        break;
                    }
                }
            }

            if (anim.animationChannels[i].type == Model::AnimationChannel::Rotation)
            {
                value = DirectX::XMQuaternionSlerp(sampler.keys[idx0], sampler.keys[idx1], ratio);
            }
            else
            {
                value = DirectX::XMVectorLerp(sampler.keys[idx0], sampler.keys[idx1], ratio);
            }
        }

        if (nodes[anim.animationChannels[i].nodeIdx].useMatrix == true)
        {
            nodes[anim.animationChannels[i].nodeIdx].transform.rotation = DirectX::XMVectorSet(0, 0, 0, 1);
            nodes[anim.animationChannels[i].nodeIdx].transform.translation = DirectX::XMVectorSet(0, 0, 0, 0);
            nodes[anim.animationChannels[i].nodeIdx].transform.scale = DirectX::XMVectorSet(1, 1, 1, 1);

            nodes[anim.animationChannels[i].nodeIdx].useMatrix = false;
        }
        switch (anim.animationChannels[i].type)
        {
            case Model::AnimationChannel::Rotation:
                nodes[anim.animationChannels[i].nodeIdx].transform.rotation = value;
                break;

            case Model::AnimationChannel::Translation:
                nodes[anim.animationChannels[i].nodeIdx].transform.translation = value;
                break;

            case Model::AnimationChannel::Scale:
                nodes[anim.animationChannels[i].nodeIdx].transform.scale = value;
                break;
        }
    }
}

void Model::UpdateMatrices(ID3D11DeviceContext* pDeviceContext)
{
    DirectX::XMMATRIX m = DirectX::XMMatrixIdentity();
    //m.r->m128_f32[0] = -m.r->m128_f32[0];

    UpdateNodeMatrices(rootNodeIdx, m);

    static ModelCB cb;
    memcpy(cb.jointIndices, joints.data(), joints.size() * sizeof(int));
    memcpy(cb.nodeTransforms, nodeMatrices.data(), nodeMatrices.size() * sizeof(DirectX::XMMATRIX));
    memcpy(cb.nodeNormalTransforms, nodeNormalMatrices.data(), nodeNormalMatrices.size() * sizeof(DirectX::XMMATRIX));

    pDeviceContext->UpdateSubresource(pCB, 0, nullptr, &cb, sizeof(cb), 0);
}

void Model::UpdateNodeMatrices(int nodeIdx, const DirectX::XMMATRIX& parent)
{
    const Node& node = nodes[nodeIdx];

    DirectX::XMMATRIX m;

    if (node.useMatrix)
    {
        m = node.matrix;
    }
    else
    {
        DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationQuaternion(node.transform.rotation);
        DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(node.transform.translation.m128_f32[0], node.transform.translation.m128_f32[1], node.transform.translation.m128_f32[2]);
        DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScaling(node.transform.scale.m128_f32[0], node.transform.scale.m128_f32[1], node.transform.scale.m128_f32[2]);
        m = DirectX::XMMatrixMultiply(
            scaleMatrix, DirectX::XMMatrixMultiply(rotationMatrix, translationMatrix)
        );
    }

    m = DirectX::XMMatrixMultiply(m, parent);

    if (nodeIdx >= nodeMatrices.size())
    {
        nodeMatrices.resize(nodeIdx + 1);
        nodeNormalMatrices.resize(nodeIdx + 1);
    }

    DirectX::XMMATRIX fullMatrix = DirectX::XMMatrixMultiply(invBindMatrices[nodeIdx], m);

    nodeMatrices[nodeIdx] = fullMatrix;
    nodeNormalMatrices[nodeIdx] = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, fullMatrix));

    for (auto idx : node.children)
    {
        UpdateNodeMatrices(idx, m);
    }
}

bool LoadModel(ID3D11Device* pDevice, const LoadParams& loadParams, const std::string& filename, Model& model)
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
        ret = ScanNode(pDevice, loadParams, *pModel, 0, m, allTextures, model.meshes, model.nodes);
    }

    model.rootNodeIdx = pModel->scenes[0].nodes[0];
    model.invBindMatrices.resize(pModel->nodes.size(), DirectX::XMMatrixIdentity());

    // Load GLTF model skinning
    if (!pModel->skins.empty())
    {
        model.skinned = true;

        assert(pModel->skins.size() == 1);

        tinygltf::Accessor invBindMatricesAccessor = pModel->accessors[pModel->skins[0].inverseBindMatrices];

        assert(invBindMatricesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
        assert(invBindMatricesAccessor.type == TINYGLTF_TYPE_MAT4);

        tinygltf::BufferView invBindMatricesView = pModel->bufferViews[invBindMatricesAccessor.bufferView];
        tinygltf::Buffer invBindMatricesBuffer = pModel->buffers[invBindMatricesView.buffer];

        const DirectX::XMMATRIX* pInvBindMatrices = reinterpret_cast<const DirectX::XMMATRIX*>(invBindMatricesBuffer.data.data() + invBindMatricesView.byteOffset + invBindMatricesAccessor.byteOffset);

        assert(pModel->skins[0].joints.size() <= ModelCB::MaxNodes);

        for (int i = 0; i < pModel->skins[0].joints.size(); i++)
        {
            int nodeIdx = pModel->skins[0].joints[i];
            if (nodeIdx >= model.joints.size())
            {
                model.joints.resize(nodeIdx + 1);
            }
            model.joints[i] = nodeIdx;
            model.invBindMatrices[nodeIdx] = pInvBindMatrices[i];
        }
    }

    // Load GLTF model animations
    if (!pModel->animations.empty())
    {
        for (int i = 0; i < pModel->animations.size(); i++)
        {
            const auto& gltfAnim = pModel->animations[i];

            std::string name = gltfAnim.name;

            Model::Animation anim;

            for (int i = 0; i < gltfAnim.samplers.size(); i++)
            {
                Model::AnimationSampler animSampler;

                tinygltf::Accessor timeKeysAccessor = pModel->accessors[gltfAnim.samplers[i].input];

                assert(timeKeysAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                assert(timeKeysAccessor.type == TINYGLTF_TYPE_SCALAR);

                tinygltf::BufferView timeKeysView = pModel->bufferViews[timeKeysAccessor.bufferView];
                tinygltf::Buffer timeKeysBuffer = pModel->buffers[timeKeysView.buffer];

                const float* timeKeys = reinterpret_cast<const float*>(timeKeysBuffer.data.data() + timeKeysView.byteOffset + timeKeysAccessor.byteOffset);

                animSampler.timeKeys.resize(timeKeysAccessor.count);
                for (int j = 0; j < timeKeysAccessor.count; j++)
                {
                    animSampler.timeKeys[j] = timeKeys[j];
                }

                anim.maxAnimationTime = std::max(anim.maxAnimationTime, animSampler.timeKeys.back());
                anim.minAnimationTime = std::min(anim.minAnimationTime, animSampler.timeKeys.front());

                tinygltf::Accessor keysAccessor = pModel->accessors[gltfAnim.samplers[i].output];

                assert(keysAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                assert(keysAccessor.type == TINYGLTF_TYPE_VEC3 || keysAccessor.type == TINYGLTF_TYPE_VEC4);

                tinygltf::BufferView keysView = pModel->bufferViews[keysAccessor.bufferView];
                tinygltf::Buffer keysBuffer = pModel->buffers[keysView.buffer];

                const float* keys3f = nullptr;
                const float* keys4f = nullptr;

                if (keysAccessor.type == TINYGLTF_TYPE_VEC3)
                {
                    keys3f = reinterpret_cast<const float*>(keysBuffer.data.data() + keysView.byteOffset + keysAccessor.byteOffset);
                }
                else if (keysAccessor.type == TINYGLTF_TYPE_VEC4)
                {
                    keys4f = reinterpret_cast<const float*>(keysBuffer.data.data() + keysView.byteOffset + keysAccessor.byteOffset);
                }

                assert(timeKeysAccessor.count == keysAccessor.count);
                animSampler.keys.resize(keysAccessor.count);
                for (int j = 0; j < timeKeysAccessor.count; j++)
                {
                    if (keys3f)
                    {
                        animSampler.keys[j] = DirectX::XMVectorSet(keys3f[j * 3 + 0], keys3f[j * 3 + 1], keys3f[j * 3 + 2], 0.0f);
                    }
                    else if (keys4f)
                    {
                        animSampler.keys[j] = DirectX::XMVectorSet(keys4f[j * 3 + 0], keys4f[j * 3 + 1], keys4f[j * 3 + 2], keys4f[j * 3 + 3]);
                    }
                }
                anim.animationSamplers.emplace_back(std::move(animSampler));
            }

            for (auto& animSampler : anim.animationSamplers)
            {
                for (auto& timeKey : animSampler.timeKeys)
                {
                    timeKey -= anim.minAnimationTime;
                }
            }
            anim.maxAnimationTime -= anim.minAnimationTime;
            anim.minAnimationTime = 0.0f;

            for (int i = 0; i < gltfAnim.channels.size(); i++)
            {
                Model::AnimationChannel animChannel;

                if (gltfAnim.channels[i].target_path == "rotation")
                {
                    animChannel.type = Model::AnimationChannel::Rotation;
                }
                else if (gltfAnim.channels[i].target_path == "translation")
                {
                    animChannel.type = Model::AnimationChannel::Translation;
                }
                else if (gltfAnim.channels[i].target_path == "scale")
                {
                    animChannel.type = Model::AnimationChannel::Scale;
                }
                animChannel.animSamplerIdx = gltfAnim.channels[i].sampler;
                animChannel.nodeIdx = gltfAnim.channels[i].target_node;

                anim.animationChannels.emplace_back(std::move(animChannel));
            }

            model.animations[name] = std::move(anim);
        }
    }

    if (ret)
    {
        D3D11_BUFFER_DESC modelBufferDesc = {};
        modelBufferDesc.ByteWidth = (UINT)sizeof(ModelCB);
        modelBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        modelBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        modelBufferDesc.CPUAccessFlags = 0;
        modelBufferDesc.MiscFlags = 0;
        modelBufferDesc.StructureByteStride = 0;

        ModelCB cb;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &cb;
        data.SysMemPitch = sizeof(cb);
        data.SysMemSlicePitch = 0;

        ID3D11Buffer* pModelCB = nullptr;
        HRESULT hr = pDevice->CreateBuffer(&modelBufferDesc, &data, &pModelCB);
        if (FAILED(hr))
        {
            return false;
        }
        model.pCB = pModelCB;
    }

    delete pModel;
    for (auto& it : allTextures)
    {
        it.second.first->Release();
        it.second.second->Release();
    }

    return ret;
}