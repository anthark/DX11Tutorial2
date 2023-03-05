#include "framework.h"

#include "Renderer.h"
#include "DDS.h"

#include <d3dcompiler.h>

#include <chrono>

#define _USE_MATH_DEFINES
#include <math.h>

#include "../Math/Matrix.h"

struct TextureVertex
{
    float x, y, z;
    float u, v;
};

struct ColorVertex
{
    float x, y, z;
    COLORREF color;
};

struct GeomBuffer
{
    DirectX::XMMATRIX m;
};

struct SphereGeomBuffer
{
    DirectX::XMMATRIX m;
    Point4f size;
};

struct RectGeomBuffer
{
    DirectX::XMMATRIX m;
    Point4f color;
};


struct SceneBuffer
{
    DirectX::XMMATRIX vp;
    Point4f cameraPos;
};

static const float CameraRotationSpeed = (float)M_PI * 2.0f;
static const float ModelRotationSpeed = (float)M_PI / 2.0f;

static const float Eps = 0.00001f;

namespace
{

void GetSphereDataSize(size_t latCells, size_t lonCells, size_t& indexCount, size_t& vertexCount)
{
    vertexCount = (latCells + 1) * (lonCells + 1);
    indexCount = latCells * lonCells * 6;
}

void CreateSphere(size_t latCells, size_t lonCells, UINT16* pIndices, Point3f* pPos)
{
    for (size_t lat = 0; lat < latCells + 1; lat++)
    {
        for (size_t lon = 0; lon < lonCells + 1; lon++)
        {
            int index = (int)(lat * (lonCells + 1) + lon);
            float lonAngle = 2.0f * (float)M_PI * lon / lonCells + (float)M_PI;
            float latAngle = -(float)M_PI / 2 + (float)M_PI * lat / latCells;

            Point3f r = Point3f{
                sinf(lonAngle) * cosf(latAngle),
                sinf(latAngle),
                cosf(lonAngle) * cosf(latAngle)
            };

            pPos[index] = r * 0.5f;
        }
    }

    for (size_t lat = 0; lat < latCells; lat++)
    {
        for (size_t lon = 0; lon < lonCells; lon++)
        {
            size_t index = lat * lonCells * 6 + lon * 6;
            pIndices[index + 0] = (UINT16)(lat * (latCells + 1) + lon + 0);
            pIndices[index + 2] = (UINT16)(lat * (latCells + 1) + lon + 1);
            pIndices[index + 1] = (UINT16)(lat * (latCells + 1) + latCells + 1 + lon);
            pIndices[index + 3] = (UINT16)(lat * (latCells + 1) + lon + 1);
            pIndices[index + 5] = (UINT16)(lat * (latCells + 1) + latCells + 1 + lon + 1);
            pIndices[index + 4] = (UINT16)(lat * (latCells + 1) + latCells + 1 + lon);
        }
    }
}

}

void Renderer::Camera::GetDirections(Point3f& forward, Point3f& right)
{
    Point3f dir = -Point3f{ cosf(theta) * cosf(phi), sinf(theta), cosf(theta) * sinf(phi) };
    float upTheta = theta + (float)M_PI / 2;
    Point3f up = Point3f{ cosf(upTheta) * cosf(phi), sinf(upTheta), cosf(upTheta) * sinf(phi) };
    right = up.cross(dir);
    right.y = 0.0f;
    right.normalize();

    if (fabs(dir.x) > Eps || fabs(dir.z) > Eps)
    {
        forward = Point3f{ dir.x, 0.0f, dir.z };
    }
    else
    {
        forward = Point3f{ up.x, 0.0f, up.z };
    }
    forward.normalize();
}

const double Renderer::PanSpeed = 2.0;

bool Renderer::Init(HWND hWnd)
{
    HRESULT result;

    // Create a DirectX graphics interface factory.
    IDXGIFactory* pFactory = nullptr;
    result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);

    // Select hardware adapter
    IDXGIAdapter* pSelectedAdapter = NULL;
    if (SUCCEEDED(result))
    {
        IDXGIAdapter* pAdapter = NULL;
        UINT adapterIdx = 0;
        while (SUCCEEDED(pFactory->EnumAdapters(adapterIdx, &pAdapter)))
        {
            DXGI_ADAPTER_DESC desc;
            pAdapter->GetDesc(&desc);

            if (wcscmp(desc.Description, L"Microsoft Basic Render Driver") != 0)
            {
                pSelectedAdapter = pAdapter;
                break;
            }

            pAdapter->Release();

            adapterIdx++;
        }
    }
    assert(pSelectedAdapter != NULL);

    // Create DirectX 11 device
    D3D_FEATURE_LEVEL level;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    if (SUCCEEDED(result))
    {
        UINT flags = 0;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // _DEBUG
        result = D3D11CreateDevice(pSelectedAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL,
            flags, levels, 1, D3D11_SDK_VERSION, &m_pDevice, &level, &m_pDeviceContext);
        assert(level == D3D_FEATURE_LEVEL_11_0);
        assert(SUCCEEDED(result));
    }

    // Create swapchain
    if (SUCCEEDED(result))
    {
        DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Width = m_width;
        swapChainDesc.BufferDesc.Height = m_height;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = hWnd;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Windowed = true;
        swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapChainDesc.Flags = 0;

        result = pFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
        assert(SUCCEEDED(result));
    }

    if (SUCCEEDED(result))
    {
        result = SetupBackBuffer();
    }

    if (SUCCEEDED(result))
    {
        result = InitScene();
    }

    // Initial camera setup
    if (SUCCEEDED(result))
    {
        m_camera.poi = Point3f{ 0,0,0 };
        m_camera.r = 5.0f;
        m_camera.phi = -(float)M_PI/4;
        m_camera.theta = (float)M_PI/4;
    }

    SAFE_RELEASE(pSelectedAdapter);
    SAFE_RELEASE(pFactory);

    if (FAILED(result))
    {
        Term();
    }

    return SUCCEEDED(result);
}

void Renderer::Term()
{
    TermScene();

    SAFE_RELEASE(m_pBackBufferRTV);
    SAFE_RELEASE(m_pSwapChain);
    SAFE_RELEASE(m_pDeviceContext);

#ifdef _DEBUG
    if (m_pDevice != nullptr)
    {
        ID3D11Debug* pDebug = nullptr;
        HRESULT result = m_pDevice->QueryInterface(__uuidof(ID3D11Debug), (void**)&pDebug);
        assert(SUCCEEDED(result));
        if (pDebug != nullptr)
        {
            if (pDebug->AddRef() != 3) // ID3D11Device && ID3D11Debug && after AddRef()
            {
                pDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
            }
            pDebug->Release();

            SAFE_RELEASE(pDebug);
        }
    }
#endif // _DEBUG

    SAFE_RELEASE(m_pDevice);
}

bool Renderer::Update()
{
    size_t usec = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (m_prevUSec == 0)
    {
        m_prevUSec = usec; // Initial update
    }

    double deltaSec = (usec - m_prevUSec) / 1000000.0;

    // Move camera
    {
        Point3f cf, cr;
        m_camera.GetDirections(cf, cr);
        Point3f d = (cf * (float)m_forwardDelta + cr * (float)m_rightDelta) * (float)deltaSec;
        m_camera.poi = m_camera.poi + d;
    }

    if (m_rotateModel)
    {
        m_angle = m_angle + deltaSec * ModelRotationSpeed;

        GeomBuffer geomBuffer;

        // Model matrix
        // Angle is reversed, as DirectXMath calculates it as clockwise
        DirectX::XMMATRIX m = DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f), -(float)m_angle);

        geomBuffer.m = m;

        m_pDeviceContext->UpdateSubresource(m_pGeomBuffer, 0, nullptr, &geomBuffer, 0, 0);

        // Model matrix for second cube
        m = DirectX::XMMatrixTranslation(2.0f, 0.0f, 0.0f);
        geomBuffer.m = m;
        m_pDeviceContext->UpdateSubresource(m_pGeomBuffer2, 0, nullptr, &geomBuffer, 0, 0);

        // Model matrix for rect
        {
            RectGeomBuffer rectGeomBuffer;

            m = DirectX::XMMatrixTranslation(1.0f, 0.0f, 0.0f);
            rectGeomBuffer.m = m;
            rectGeomBuffer.color = Point4f{0.5f, 0, 0.5f, 1.0f};
            m_pDeviceContext->UpdateSubresource(m_pRectGeomBuffer, 0, nullptr, &rectGeomBuffer, 0, 0);

            // Model matrix for second rect
            m = DirectX::XMMatrixTranslation(1.2f, 0.0f, 0.0f);
            rectGeomBuffer.m = m;
            rectGeomBuffer.color = Point4f{ 0.5f, 0.5f, 0, 1.0f };
            m_pDeviceContext->UpdateSubresource(m_pRectGeomBuffer2, 0, nullptr, &rectGeomBuffer, 0, 0);
        }
    }

    m_prevUSec = usec;

    // Setup camera
    DirectX::XMMATRIX v;
    Point4f cameraPos;
    {
        Point3f pos = m_camera.poi + Point3f{ cosf(m_camera.theta) * cosf(m_camera.phi), sinf(m_camera.theta), cosf(m_camera.theta) * sinf(m_camera.phi) } * m_camera.r;
        float upTheta = m_camera.theta + (float)M_PI / 2;
        Point3f up = Point3f{ cosf(upTheta) * cosf(m_camera.phi), sinf(upTheta), cosf(upTheta) * sinf(m_camera.phi) };

        v = DirectX::XMMatrixLookAtLH(
            DirectX::XMVectorSet(pos.x, pos.y, pos.z, 0.0f),
            DirectX::XMVectorSet(m_camera.poi.x, m_camera.poi.y, m_camera.poi.z, 0.0f),
            DirectX::XMVectorSet(up.x, up.y, up.z, 0.0f)
        );

        cameraPos = pos;
    }

    float f = 100.0f;
    float n = 0.1f;
    float fov = (float)M_PI / 3;
    float c = 1.0f / tanf(fov / 2);
    float aspectRatio = (float)m_height / m_width;
    DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveLH(tanf(fov / 2) * 2 * f, tanf(fov / 2) * 2 * f * aspectRatio, f, n);

    D3D11_MAPPED_SUBRESOURCE subresource;
    HRESULT result = m_pDeviceContext->Map(m_pSceneBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource);
    assert(SUCCEEDED(result));
    if (SUCCEEDED(result))
    {
        SceneBuffer& sceneBuffer = *reinterpret_cast<SceneBuffer*>(subresource.pData);

        sceneBuffer.vp = DirectX::XMMatrixMultiply(v, p);
        sceneBuffer.cameraPos = cameraPos;

        m_pDeviceContext->Unmap(m_pSceneBuffer, 0);
    }

    return SUCCEEDED(result);
}

bool Renderer::Render()
{
    m_pDeviceContext->ClearState();

    ID3D11RenderTargetView* views[] = { m_pBackBufferRTV };
    m_pDeviceContext->OMSetRenderTargets(1, views, m_pDepthBufferDSV);

    static const FLOAT BackColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
    m_pDeviceContext->ClearRenderTargetView(m_pBackBufferRTV, BackColor);
    m_pDeviceContext->ClearDepthStencilView(m_pDepthBufferDSV, D3D11_CLEAR_DEPTH, 0.0f, 0);

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (FLOAT)m_width;
    viewport.Height = (FLOAT)m_height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_pDeviceContext->RSSetViewports(1, &viewport);

    D3D11_RECT rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = m_width;
    rect.bottom = m_height;
    m_pDeviceContext->RSSetScissorRects(1, &rect);

    m_pDeviceContext->OMSetDepthStencilState(m_pDepthState, 0);

    m_pDeviceContext->RSSetState(m_pRasterizerState);

    m_pDeviceContext->OMSetBlendState(m_pOpaqueBlendState, nullptr, 0xFFFFFFFF);

    ID3D11SamplerState* samplers[] = {m_pSampler};
    m_pDeviceContext->PSSetSamplers(0, 1, samplers);

    ID3D11ShaderResourceView* resources[] = {m_pTextureView};
    m_pDeviceContext->PSSetShaderResources(0, 1, resources);

    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = {m_pVertexBuffer};
    UINT strides[] = {20};
    UINT offsets[] = {0};
    ID3D11Buffer* cbuffers[] = {m_pSceneBuffer, m_pGeomBuffer};
    m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    m_pDeviceContext->IASetInputLayout(m_pInputLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 2, cbuffers);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);
    m_pDeviceContext->DrawIndexed(36, 0, 0);

    ID3D11Buffer* cbuffers2[] = { m_pGeomBuffer2 };
    m_pDeviceContext->VSSetConstantBuffers(1, 1, cbuffers2);
    m_pDeviceContext->DrawIndexed(36, 0, 0);

    RenderSphere();

    RenderRects();

    HRESULT result = m_pSwapChain->Present(0, 0);
    assert(SUCCEEDED(result));

    return SUCCEEDED(result);
}

bool Renderer::Resize(UINT width, UINT height)
{
    if (width != m_width || height != m_height)
    {
        SAFE_RELEASE(m_pBackBufferRTV);
        SAFE_RELEASE(m_pDepthBuffer);
        SAFE_RELEASE(m_pDepthBufferDSV);

        HRESULT result = m_pSwapChain->ResizeBuffers(2, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            m_width = width;
            m_height = height;

            result = SetupBackBuffer();

            // Setup skybox sphere
            float n = 0.1f;
            float fov = (float)M_PI / 3;
            float halfW = tanf(fov / 2) * n;
            float halfH = (float)m_height / m_width * halfW;

            float r = sqrtf(n*n + halfH*halfH + halfW*halfW) * 1.1f * 2.0f;

            SphereGeomBuffer geomBuffer;
            geomBuffer.m = DirectX::XMMatrixIdentity();
            geomBuffer.size = r;
            m_pDeviceContext->UpdateSubresource(m_pSphereGeomBuffer, 0, nullptr, &geomBuffer, 0, 0);
        }

        return SUCCEEDED(result);
    }

    return true;
}

void Renderer::MouseRBPressed(bool pressed, int x, int y)
{
    m_rbPressed = pressed;
    if (m_rbPressed)
    {
        m_prevMouseX = x;
        m_prevMouseY = y;
    }
}

void Renderer::MouseMoved(int x, int y)
{
    if (m_rbPressed)
    {
        float dx = -(float)(x - m_prevMouseX) / m_width * CameraRotationSpeed;
        float dy = (float)(y - m_prevMouseY) / m_width * CameraRotationSpeed;

        m_camera.phi += dx;
        m_camera.theta += dy;
        m_camera.theta = std::min(std::max(m_camera.theta, -(float)M_PI / 2), (float)M_PI / 2);

        m_prevMouseX = x;
        m_prevMouseY = y;
    }
}

void Renderer::MouseWheel(int delta)
{
    m_camera.r -= delta / 100.0f;
    if (m_camera.r < 1.0f)
    {
        m_camera.r = 1.0f;
    }
}

void Renderer::KeyPressed(int keyCode)
{
    switch (keyCode)
    {
        case ' ':
            m_rotateModel = !m_rotateModel;
            break;

        case 'W':
        case 'w':
            m_forwardDelta += PanSpeed;
            break;

        case 'S':
        case 's':
            m_forwardDelta -= PanSpeed;
            break;

        case 'D':
        case 'd':
            m_rightDelta += PanSpeed;
            break;

        case 'A':
        case 'a':
            m_rightDelta -= PanSpeed;
            break;
    }
}

void Renderer::KeyReleased(int keyCode)
{
    switch (keyCode)
    {
        case 'W':
        case 'w':
            m_forwardDelta -= PanSpeed;
            break;

        case 'S':
        case 's':
            m_forwardDelta += PanSpeed;
            break;

        case 'D':
        case 'd':
            m_rightDelta -= PanSpeed;
            break;

        case 'A':
        case 'a':
            m_rightDelta += PanSpeed;
            break;
    }
}

HRESULT Renderer::SetupBackBuffer()
{
    ID3D11Texture2D* pBackBuffer = NULL;
    HRESULT result = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &m_pBackBufferRTV);

        SAFE_RELEASE(pBackBuffer);
    }
    if (SUCCEEDED(result))
    {
        D3D11_TEXTURE2D_DESC desc;
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.ArraySize = 1;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.Height = m_height;
        desc.Width = m_width;
        desc.MipLevels = 1;

        result = m_pDevice->CreateTexture2D(&desc, nullptr, &m_pDepthBuffer);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pDepthBuffer, "DepthBuffer");
        }
    }
    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateDepthStencilView(m_pDepthBuffer, nullptr, &m_pDepthBufferDSV);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pDepthBuffer, "DepthBufferView");
        }
    }

    assert(SUCCEEDED(result));

    return result;
}

HRESULT Renderer::InitScene()
{
    // Textured cube
    static const TextureVertex Vertices[24] = {
        // Bottom face
        {-0.5, -0.5,  0.5, 0, 1},
        { 0.5, -0.5,  0.5, 1, 1},
        { 0.5, -0.5, -0.5, 1, 0},
        {-0.5, -0.5, -0.5, 0, 0},
        // Top face
        {-0.5,  0.5, -0.5, 0, 1},
        { 0.5,  0.5, -0.5, 1, 1},
        { 0.5,  0.5,  0.5, 1, 0},
        {-0.5,  0.5,  0.5, 0, 0},
        // Front face
        { 0.5, -0.5, -0.5, 0, 1},
        { 0.5, -0.5,  0.5, 1, 1},
        { 0.5,  0.5,  0.5, 1, 0},
        { 0.5,  0.5, -0.5, 0, 0},
        // Back face
        {-0.5, -0.5,  0.5, 0, 1},
        {-0.5, -0.5, -0.5, 1, 1},
        {-0.5,  0.5, -0.5, 1, 0},
        {-0.5,  0.5,  0.5, 0, 0},
        // Left face
        { 0.5, -0.5,  0.5, 0, 1},
        {-0.5, -0.5,  0.5, 1, 1},
        {-0.5,  0.5,  0.5, 1, 0},
        { 0.5,  0.5,  0.5, 0, 0},
        // Right face
        {-0.5, -0.5, -0.5, 0, 1},
        { 0.5, -0.5, -0.5, 1, 1},
        { 0.5,  0.5, -0.5, 1, 0},
        {-0.5,  0.5, -0.5, 0, 0}
    };
    static const UINT16 Indices[36] = {
        0, 2, 1, 0, 3, 2,
        4, 6, 5, 4, 7, 6,
        8, 10, 9, 8, 11, 10,
        12, 14, 13, 12, 15, 14,
        16, 18, 17, 16, 19, 18,
        20, 22, 21, 20, 23, 22
    };
    static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    HRESULT result = S_OK;

    // Create vertex buffer
    if (SUCCEEDED(result))
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(Vertices);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &Vertices;
        data.SysMemPitch = sizeof(Vertices);
        data.SysMemSlicePitch = 0;

        result = m_pDevice->CreateBuffer(&desc, &data, &m_pVertexBuffer);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pVertexBuffer, "VertexBuffer");
        }
    }

    // Create index buffer
    if (SUCCEEDED(result))
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(Indices);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &Indices;
        data.SysMemPitch = sizeof(Indices);
        data.SysMemSlicePitch = 0;

        result = m_pDevice->CreateBuffer(&desc, &data, &m_pIndexBuffer);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pIndexBuffer, "IndexBuffer");
        }
    }

    ID3DBlob* pVertexShaderCode = nullptr;
    if (SUCCEEDED(result))
    {
        result = CompileAndCreateShader(L"SimpleTexture.vs", (ID3D11DeviceChild**)&m_pVertexShader, &pVertexShaderCode);
    }
    if (SUCCEEDED(result))
    {
        result = CompileAndCreateShader(L"SimpleTexture.ps", (ID3D11DeviceChild**)&m_pPixelShader);
    }

    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateInputLayout(InputDesc, 2, pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &m_pInputLayout);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pInputLayout, "InputLayout");
        }
    }

    SAFE_RELEASE(pVertexShaderCode);

    // Create geometry buffer
    if (SUCCEEDED(result))
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(GeomBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        GeomBuffer geomBuffer;
        geomBuffer.m = DirectX::XMMatrixIdentity();

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &geomBuffer;
        data.SysMemPitch = sizeof(geomBuffer);
        data.SysMemSlicePitch = 0;

        result = m_pDevice->CreateBuffer(&desc, &data, &m_pGeomBuffer);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pGeomBuffer, "GeomBuffer");
        }
        if (SUCCEEDED(result))
        {
            result = m_pDevice->CreateBuffer(&desc, &data, &m_pGeomBuffer2);
            assert(SUCCEEDED(result));
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pGeomBuffer2, "GeomBuffer2");
            }
        }
    }
    // Create scene buffer
    if (SUCCEEDED(result))
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(SceneBuffer);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        result = m_pDevice->CreateBuffer(&desc, nullptr, &m_pSceneBuffer);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pSceneBuffer, "SceneBuffer");
        }
    }

    // CCW culling rasterizer state
    if (SUCCEEDED(result))
    {
        D3D11_RASTERIZER_DESC desc = {};
        desc.AntialiasedLineEnable = FALSE;
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.FrontCounterClockwise = FALSE;
        desc.DepthBias = 0;
        desc.SlopeScaledDepthBias = 0.0f;
        desc.DepthBiasClamp = 0.0f;
        desc.DepthClipEnable = TRUE;
        desc.ScissorEnable = FALSE;
        desc.MultisampleEnable = FALSE;

        result = m_pDevice->CreateRasterizerState(&desc, &m_pRasterizerState);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pRasterizerState, "RasterizerState");
        }
    }

    // Create blend states
    if (SUCCEEDED(result))
    {
        D3D11_BLEND_DESC desc = {};
        desc.AlphaToCoverageEnable = FALSE;
        desc.IndependentBlendEnable = FALSE;
        desc.RenderTarget[0].BlendEnable = TRUE;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        result = m_pDevice->CreateBlendState(&desc, &m_pTransBlendState);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pTransBlendState, "TransBlendState");
        }
        if (SUCCEEDED(result))
        {
            desc.RenderTarget[0].BlendEnable = FALSE;
            result = m_pDevice->CreateBlendState(&desc, &m_pOpaqueBlendState);
        }
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pOpaqueBlendState, "OpaqueBlendState");
        }
    }

    // Create reverse depth state
    if (SUCCEEDED(result))
    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable = TRUE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
        desc.StencilEnable = FALSE;

        result = m_pDevice->CreateDepthStencilState(&desc, &m_pDepthState);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pDepthState, "DepthState");
        }
    }

    // Create reverse transparent depth state
    if (SUCCEEDED(result))
    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable = TRUE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_GREATER;
        desc.StencilEnable = FALSE;

        result = m_pDevice->CreateDepthStencilState(&desc, &m_pTransDepthState);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pTransDepthState, "TransDepthState");
        }
    }

    // Load texture
    DXGI_FORMAT textureFmt;
    if (SUCCEEDED(result))
    {
        const std::wstring TextureName = L"../Common/Kitty.dds";

        TextureDesc textureDesc;
        bool ddsRes = LoadDDS(TextureName.c_str(), textureDesc);

        textureFmt = textureDesc.fmt;

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Format = textureDesc.fmt;
        desc.ArraySize = 1;
        desc.MipLevels = textureDesc.mipmapsCount;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Height = textureDesc.height;
        desc.Width = textureDesc.width;

        UINT32 blockWidth = DivUp(desc.Width, 4u);
        UINT32 blockHeight = DivUp(desc.Height, 4u);
        UINT32 pitch = blockWidth * GetBytesPerBlock(desc.Format);
        const char* pSrcData = reinterpret_cast<const char*>(textureDesc.pData);

        std::vector<D3D11_SUBRESOURCE_DATA> data;
        data.resize(desc.MipLevels);
        for (UINT32 i = 0; i < desc.MipLevels; i++)
        {
            data[i].pSysMem = pSrcData;
            data[i].SysMemPitch = pitch;
            data[i].SysMemSlicePitch = 0;

            pSrcData += pitch * blockHeight;
            blockHeight = std::max(1u, blockHeight / 2);
            blockWidth = std::max(1u, blockWidth / 2);
            pitch = blockWidth * GetBytesPerBlock(desc.Format);
        }
        result = m_pDevice->CreateTexture2D(&desc, data.data(), &m_pTexture);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pTexture, WCSToMBS(TextureName));
        }

        free(textureDesc.pData);
    }
    if (SUCCEEDED(result))
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = textureFmt;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 11;
        desc.Texture2D.MostDetailedMip = 0;

        result = m_pDevice->CreateShaderResourceView(m_pTexture, &desc, &m_pTextureView);
    }
    if (SUCCEEDED(result))
    {
        D3D11_SAMPLER_DESC desc = {};

        //desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        //desc.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        desc.Filter = D3D11_FILTER_ANISOTROPIC;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.MinLOD = -FLT_MAX;
        desc.MaxLOD = FLT_MAX;
        desc.MipLODBias = 0.0f;
        desc.MaxAnisotropy = 16;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;

        result = m_pDevice->CreateSamplerState(&desc, &m_pSampler);
    }

    if (SUCCEEDED(result))
    {
        result = InitSphere();
    }
    if (SUCCEEDED(result))
    {
        result = InitCubemap();
    }

    if (SUCCEEDED(result))
    {
        result = InitRect();
    }

    assert(SUCCEEDED(result));

    return result;
}

HRESULT Renderer::InitSphere()
{
    static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    HRESULT result = S_OK;

    static const size_t SphereSteps = 32;

    std::vector<Point3f> sphereVertices;
    std::vector<UINT16> indices;

    size_t indexCount;
    size_t vertexCount;

    GetSphereDataSize(SphereSteps, SphereSteps, indexCount, vertexCount);

    sphereVertices.resize(vertexCount);
    indices.resize(indexCount);

    m_sphereIndexCount = (UINT)indexCount;

    CreateSphere(SphereSteps, SphereSteps, indices.data(), sphereVertices.data());

    // Create vertex buffer
    if (SUCCEEDED(result))
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = (UINT)(sphereVertices.size() * sizeof(Point3f));
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = sphereVertices.data();
        data.SysMemPitch = (UINT)(sphereVertices.size() * sizeof(Point3f));
        data.SysMemSlicePitch = 0;

        result = m_pDevice->CreateBuffer(&desc, &data, &m_pSphereVertexBuffer);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pSphereVertexBuffer, "SphereVertexBuffer");
        }
    }

    // Create index buffer
    if (SUCCEEDED(result))
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = (UINT)(indices.size() * sizeof(UINT16));
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = indices.data();
        data.SysMemPitch = (UINT)(indices.size() * sizeof(UINT16));
        data.SysMemSlicePitch = 0;

        result = m_pDevice->CreateBuffer(&desc, &data, &m_pSphereIndexBuffer);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pSphereIndexBuffer, "SphereIndexBuffer");
        }
    }

    ID3DBlob* pSphereVertexShaderCode = nullptr;
    if (SUCCEEDED(result))
    {
        result = CompileAndCreateShader(L"SphereTexture.vs", (ID3D11DeviceChild**)&m_pSphereVertexShader, &pSphereVertexShaderCode);
    }
    if (SUCCEEDED(result))
    {
        result = CompileAndCreateShader(L"SphereTexture.ps", (ID3D11DeviceChild**)&m_pSpherePixelShader);
    }

    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateInputLayout(InputDesc, 1, pSphereVertexShaderCode->GetBufferPointer(), pSphereVertexShaderCode->GetBufferSize(), &m_pSphereInputLayout);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pSphereInputLayout, "SphereInputLayout");
        }
    }

    SAFE_RELEASE(pSphereVertexShaderCode);

    // Create geometry buffer
    if (SUCCEEDED(result))
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(SphereGeomBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        SphereGeomBuffer geomBuffer;
        geomBuffer.m = DirectX::XMMatrixIdentity();
        geomBuffer.size.x = 2.0f;
        //geomBuffer.m = DirectX::XMMatrixTranslation(2.0f, 0.0f, 0.0f);

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &geomBuffer;
        data.SysMemPitch = sizeof(geomBuffer);
        data.SysMemSlicePitch = 0;

        result = m_pDevice->CreateBuffer(&desc, &data, &m_pSphereGeomBuffer);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pSphereGeomBuffer, "SphereGeomBuffer");
        }
    }

    return result;
}

HRESULT Renderer::InitRect()
{
    static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    static const ColorVertex Vertices[] =
    {
        {0.0, -0.75, -0.75, RGB(128,0,128)},
        {0.0,  0.75, -0.75, RGB(128,0,128)},
        {0.0,  0.75,  0.75, RGB(128,0,128)},
        {0.0, -0.75,  0.75, RGB(128,0,128)}
    };
    static const UINT16 Indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    HRESULT result = S_OK;

    // Create vertex buffer
    if (SUCCEEDED(result))
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = (UINT)sizeof(Vertices);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = Vertices;
        data.SysMemPitch = (UINT)sizeof(Vertices);
        data.SysMemSlicePitch = 0;

        result = m_pDevice->CreateBuffer(&desc, &data, &m_pRectVertexBuffer);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pRectVertexBuffer, "RectVertexBuffer");
        }
    }

    // Create index buffer
    if (SUCCEEDED(result))
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = (UINT)sizeof(Indices);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = Indices;
        data.SysMemPitch = (UINT)sizeof(Indices);
        data.SysMemSlicePitch = 0;

        result = m_pDevice->CreateBuffer(&desc, &data, &m_pRectIndexBuffer);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pRectIndexBuffer, "RectIndexBuffer");
        }
    }

    ID3DBlob* pRectVertexShaderCode = nullptr;
    if (SUCCEEDED(result))
    {
        result = CompileAndCreateShader(L"TransColor.vs", (ID3D11DeviceChild**)&m_pRectVertexShader, &pRectVertexShaderCode);
    }
    if (SUCCEEDED(result))
    {
        result = CompileAndCreateShader(L"TransColor.ps", (ID3D11DeviceChild**)&m_pRectPixelShader);
    }

    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateInputLayout(InputDesc, 2, pRectVertexShaderCode->GetBufferPointer(), pRectVertexShaderCode->GetBufferSize(), &m_pRectInputLayout);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pRectInputLayout, "RectInputLayout");
        }
    }

    SAFE_RELEASE(pRectVertexShaderCode);

    // Create geometry buffer
    if (SUCCEEDED(result))
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(RectGeomBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        RectGeomBuffer geomBuffer;
        geomBuffer.m = DirectX::XMMatrixIdentity();
        geomBuffer.color = Point4f{ 1,1,1,1 };

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &geomBuffer;
        data.SysMemPitch = sizeof(geomBuffer);
        data.SysMemSlicePitch = 0;

        result = m_pDevice->CreateBuffer(&desc, &data, &m_pRectGeomBuffer);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pRectGeomBuffer, "RectGeomBuffer");
        }
        if (SUCCEEDED(result))
        {
            result = m_pDevice->CreateBuffer(&desc, &data, &m_pRectGeomBuffer2);
        }
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pRectGeomBuffer2, "RectGeomBuffer2");
        }
    }

    return result;
}

HRESULT Renderer::InitCubemap()
{
    HRESULT result = S_OK;

    DXGI_FORMAT textureFmt;
    if (SUCCEEDED(result))
    {
        const std::wstring TextureNames[6] =
        {
            L"../Common/posx.dds", L"../Common/negx.dds",
            L"../Common/posy.dds", L"../Common/negy.dds",
            L"../Common/posz.dds", L"../Common/negz.dds"
        };
        TextureDesc texDescs[6];
        bool ddsRes = true;
        for (int i = 0; i < 6 && ddsRes; i++)
        {
            ddsRes = LoadDDS(TextureNames[i].c_str(), texDescs[i], true);
        }

        textureFmt = texDescs[0].fmt; // Assume all are the same

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Format = textureFmt;
        desc.ArraySize = 6;
        desc.MipLevels = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Height = texDescs[0].height;
        desc.Width = texDescs[0].width;

        UINT32 blockWidth = DivUp(desc.Width, 4u);
        UINT32 blockHeight = DivUp(desc.Height, 4u);
        UINT32 pitch = blockWidth * GetBytesPerBlock(desc.Format);

        D3D11_SUBRESOURCE_DATA data[6];
        for (int i = 0; i < 6; i++)
        {
            data[i].pSysMem = texDescs[i].pData;
            data[i].SysMemPitch = pitch;
            data[i].SysMemSlicePitch = 0;
        }
        result = m_pDevice->CreateTexture2D(&desc, data, &m_pCubemapTexture);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pCubemapTexture, "CubemapTexture");
        }
        for (int i = 0; i < 6; i++)
        {
            free(texDescs[i].pData);
        }
    }
    if (SUCCEEDED(result))
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc;
        desc.Format = textureFmt;
        desc.ViewDimension = D3D_SRV_DIMENSION_TEXTURECUBE;
        desc.TextureCube.MipLevels = 1;
        desc.TextureCube.MostDetailedMip = 0;

        result = m_pDevice->CreateShaderResourceView(m_pCubemapTexture, &desc, &m_pCubemapView);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pCubemapView, "CubemapView");
        }
    }

    return result;
}

void Renderer::TermScene()
{
    SAFE_RELEASE(m_pSampler);

    SAFE_RELEASE(m_pTextureView);
    SAFE_RELEASE(m_pTexture);

    SAFE_RELEASE(m_pRasterizerState);
    SAFE_RELEASE(m_pDepthState);
    SAFE_RELEASE(m_pTransDepthState);

    SAFE_RELEASE(m_pInputLayout);
    SAFE_RELEASE(m_pPixelShader);
    SAFE_RELEASE(m_pVertexShader);

    SAFE_RELEASE(m_pIndexBuffer);
    SAFE_RELEASE(m_pVertexBuffer);

    SAFE_RELEASE(m_pSceneBuffer);
    SAFE_RELEASE(m_pGeomBuffer);
    SAFE_RELEASE(m_pGeomBuffer2);

    SAFE_RELEASE(m_pTransBlendState);
    SAFE_RELEASE(m_pOpaqueBlendState);

    // Term sphere
    SAFE_RELEASE(m_pSphereInputLayout);
    SAFE_RELEASE(m_pSpherePixelShader);
    SAFE_RELEASE(m_pSphereVertexShader);

    SAFE_RELEASE(m_pSphereIndexBuffer);
    SAFE_RELEASE(m_pSphereVertexBuffer);

    SAFE_RELEASE(m_pSphereGeomBuffer);

    SAFE_RELEASE(m_pCubemapTexture);
    SAFE_RELEASE(m_pCubemapView);

    // Term rect
    SAFE_RELEASE(m_pRectInputLayout);
    SAFE_RELEASE(m_pRectPixelShader);
    SAFE_RELEASE(m_pRectVertexShader);

    SAFE_RELEASE(m_pRectIndexBuffer);
    SAFE_RELEASE(m_pRectVertexBuffer);

    SAFE_RELEASE(m_pRectGeomBuffer);
    SAFE_RELEASE(m_pRectGeomBuffer2);

    // Term depth buffer
    SAFE_RELEASE(m_pDepthBuffer);
    SAFE_RELEASE(m_pDepthBufferDSV);
}

void Renderer::RenderSphere()
{
    ID3D11SamplerState* samplers[] = { m_pSampler };
    m_pDeviceContext->PSSetSamplers(0, 1, samplers);

    ID3D11ShaderResourceView* resources[] = { m_pCubemapView };
    m_pDeviceContext->PSSetShaderResources(0, 1, resources);

    m_pDeviceContext->IASetIndexBuffer(m_pSphereIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { m_pSphereVertexBuffer };
    UINT strides[] = { 12 };
    UINT offsets[] = { 0 };
    ID3D11Buffer* cbuffers[] = { m_pSceneBuffer, m_pSphereGeomBuffer };
    m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    m_pDeviceContext->IASetInputLayout(m_pSphereInputLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->VSSetShader(m_pSphereVertexShader, nullptr, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 2, cbuffers);
    m_pDeviceContext->PSSetShader(m_pSpherePixelShader, nullptr, 0);
    m_pDeviceContext->DrawIndexed(m_sphereIndexCount, 0, 0);
}

void Renderer::RenderRects()
{
    m_pDeviceContext->OMSetDepthStencilState(m_pTransDepthState, 0);

    m_pDeviceContext->OMSetBlendState(m_pTransBlendState, nullptr, 0xFFFFFFFF);

    m_pDeviceContext->IASetIndexBuffer(m_pRectIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { m_pRectVertexBuffer };
    UINT strides[] = { 16 };
    UINT offsets[] = { 0 };
    ID3D11Buffer* cbuffers[] = { m_pSceneBuffer, nullptr };
    m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    m_pDeviceContext->IASetInputLayout(m_pRectInputLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->VSSetShader(m_pRectVertexShader, nullptr, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 2, cbuffers);
    m_pDeviceContext->PSSetConstantBuffers(0, 2, cbuffers);
    m_pDeviceContext->PSSetShader(m_pRectPixelShader, nullptr, 0);

    Point3f dir, right;
    m_camera.GetDirections(dir, right);

    if (dir.x < 0.0)
    {
        cbuffers[1] = m_pRectGeomBuffer;
        m_pDeviceContext->VSSetConstantBuffers(0, 2, cbuffers);
        m_pDeviceContext->PSSetConstantBuffers(0, 2, cbuffers);
        m_pDeviceContext->DrawIndexed(6, 0, 0);

        cbuffers[1] = m_pRectGeomBuffer2;
        m_pDeviceContext->VSSetConstantBuffers(0, 2, cbuffers);
        m_pDeviceContext->PSSetConstantBuffers(0, 2, cbuffers);
        m_pDeviceContext->DrawIndexed(6, 0, 0);
    }
    else
    {
        cbuffers[1] = m_pRectGeomBuffer2;
        m_pDeviceContext->VSSetConstantBuffers(0, 2, cbuffers);
        m_pDeviceContext->PSSetConstantBuffers(0, 2, cbuffers);
        m_pDeviceContext->DrawIndexed(6, 0, 0);

        cbuffers[1] = m_pRectGeomBuffer;
        m_pDeviceContext->VSSetConstantBuffers(0, 2, cbuffers);
        m_pDeviceContext->PSSetConstantBuffers(0, 2, cbuffers);
        m_pDeviceContext->DrawIndexed(6, 0, 0);
    }
}

HRESULT Renderer::CompileAndCreateShader(const std::wstring& path, ID3D11DeviceChild** ppShader, ID3DBlob** ppCode)
{
    // Try to load shader's source code first
    FILE* pFile = nullptr;
    _wfopen_s(&pFile, path.c_str(), L"rb");
    assert(pFile != nullptr);
    if (pFile == nullptr)
    {
        return E_FAIL;
    }

    fseek(pFile, 0, SEEK_END);
    long long size = _ftelli64(pFile);
    fseek(pFile, 0, SEEK_SET);

    std::vector<char> data;
    data.resize(size + 1);

    size_t rd = fread(data.data(), 1, size, pFile);
    assert(rd == (size_t)size);

    fclose(pFile);

    // Determine shader's type
    std::wstring ext = Extension(path);

    std::string entryPoint = "";
    std::string platform = "";

    if (ext == L"vs")
    {
        entryPoint = "vs";
        platform = "vs_5_0";
    }
    else if (ext == L"ps")
    {
        entryPoint = "ps";
        platform = "ps_5_0";
    }

    // Setup flags
    UINT flags1 = 0;
#ifdef _DEBUG
    flags1 |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // _DEBUG

    // Try to compile
    ID3DBlob* pCode = nullptr;
    ID3DBlob* pErrMsg = nullptr;
    HRESULT result = D3DCompile(data.data(), data.size(), WCSToMBS(path).c_str(), nullptr, nullptr, entryPoint.c_str(), platform.c_str(), flags1, 0, &pCode, &pErrMsg);
    if (!SUCCEEDED(result) && pErrMsg != nullptr)
    {
        OutputDebugStringA((const char*)pErrMsg->GetBufferPointer());
    }
    assert(SUCCEEDED(result));
    SAFE_RELEASE(pErrMsg);

    // Create shader itself if anything else is OK
    if (SUCCEEDED(result))
    {
        if (ext == L"vs")
        {
            ID3D11VertexShader* pVertexShader = nullptr;
            result = m_pDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &pVertexShader);
            if (SUCCEEDED(result))
            {
                *ppShader = pVertexShader;
            }
        }
        else if (ext == L"ps")
        {
            ID3D11PixelShader* pPixelShader = nullptr;
            result = m_pDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &pPixelShader);
            if (SUCCEEDED(result))
            {
                *ppShader = pPixelShader;
            }
        }
    }
    if (SUCCEEDED(result))
    {
        result = SetResourceName(*ppShader, WCSToMBS(path).c_str());
    }

    if (ppCode)
    {
        *ppCode = pCode;
    }
    else
    {
        SAFE_RELEASE(pCode);
    }

    return result;
}
