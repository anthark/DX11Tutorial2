#include "framework.h"

#include "Renderer.h"
#include "DDS.h"

#include <d3dcompiler.h>

#include <chrono>

#define _USE_MATH_DEFINES
#include <math.h>

#include "../Math/Matrix.h"

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

struct TextureTangentVertex
{
    Point3f pos;
    Point3f tangent;
    Point3f norm;
    Point2f uv;
};

struct ColorVertex
{
    float x, y, z;
    COLORREF color;
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

const Point3f Renderer::Rect0Pos = Point3f{ 1.0f, 0, 0 };
const Point3f Renderer::Rect1Pos = Point3f{ 1.2f, 0, 0 };

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

    if (SUCCEEDED(result))
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight();

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(hWnd);
        ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);

        m_sceneBuffer.lightCount.x = 1;
        m_sceneBuffer.lights[0].pos = Point4f{0, 1.05f, 0, 1};
        m_sceneBuffer.lights[0].color = Point4f{1,1,0};
        m_sceneBuffer.ambientColor = Point4f(0,0,0.2f,0);
    }

    if (FAILED(result))
    {
        Term();
    }

    return SUCCEEDED(result);
}

void Renderer::Term()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

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

    UpdateCubes(deltaSec);

    // Move light bulb spheres
    {
        for (int i = 0; i < m_sceneBuffer.lightCount.x; i++)
        {
            RectGeomBuffer geomBuffer;
            geomBuffer.m = DirectX::XMMatrixTranslation(m_sceneBuffer.lights[i].pos.x , m_sceneBuffer.lights[i].pos.y, m_sceneBuffer.lights[i].pos.z);
            geomBuffer.color = m_sceneBuffer.lights[i].color;

            m_pDeviceContext->UpdateSubresource(m_pSmallSphereGeomBuffers[i], 0, nullptr, &geomBuffer, 0, 0);
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
        m_sceneBuffer.vp = DirectX::XMMatrixMultiply(v, p);
        m_sceneBuffer.cameraPos = cameraPos;

        memcpy(subresource.pData, &m_sceneBuffer, sizeof(SceneBuffer));

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

    ID3D11ShaderResourceView* resources[] = {m_pTextureView, m_pTextureViewNM};
    m_pDeviceContext->PSSetShaderResources(0, 2, resources);

    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = {m_pVertexBuffer};
    UINT strides[] = {44};
    UINT offsets[] = {0};
    ID3D11Buffer* cbuffers[] = {m_pSceneBuffer, m_pGeomBufferInst};
    m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    m_pDeviceContext->IASetInputLayout(m_pInputLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 2, cbuffers);
    m_pDeviceContext->PSSetConstantBuffers(0, 2, cbuffers);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);
    m_pDeviceContext->DrawIndexedInstanced(36, m_instCount, 0, 0, 0);

    if (m_showLightBulbs)
    {
        RenderSmallSpheres();
    }

    RenderSphere();

    RenderRects();

    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::Begin("Lights");

        ImGui::Checkbox("Show bulbs", &m_showLightBulbs);
        ImGui::Checkbox("Use normal maps", &m_useNormalMaps);
        ImGui::Checkbox("Show normals", &m_showNormals);

        m_sceneBuffer.lightCount.y = m_useNormalMaps ? 1 : 0;
        m_sceneBuffer.lightCount.z = m_showNormals ? 1 : 0;

        bool add = ImGui::Button("+");
        ImGui::SameLine();
        bool remove = ImGui::Button("-");

        if (add && m_sceneBuffer.lightCount.x < 10)
        {
            ++m_sceneBuffer.lightCount.x;
            m_sceneBuffer.lights[m_sceneBuffer.lightCount.x - 1] = Light();
        }
        if (remove && m_sceneBuffer.lightCount.x > 0)
        {
            --m_sceneBuffer.lightCount.x;
        }

        char buffer[1024];
        for (int i = 0; i < m_sceneBuffer.lightCount.x; i++)
        {
            ImGui::Text("Light %d", i);
            sprintf_s(buffer, "Pos %d", i);
            ImGui::DragFloat3(buffer, (float*)&m_sceneBuffer.lights[i].pos, 0.1f, -10.0f, 10.0f);
            sprintf_s(buffer, "Color %d", i);
            ImGui::ColorEdit3(buffer, (float*)&m_sceneBuffer.lights[i].color);
        }

        ImGui::End();

        ImGui::Begin("Instances");
        add = ImGui::Button("+");
        ImGui::SameLine();
        remove = ImGui::Button("-");
        ImGui::Text("Count %d", m_instCount);
        ImGui::End();
        if (add && m_instCount < MaxInst)
        {
            Point4f pos = m_geomBuffers[m_instCount].posAngle;
            if (pos.x == 0 && pos.y == 0 && pos.z == 0)
            {
                InitGeom(m_geomBuffers[m_instCount]);
            }
            ++m_instCount;
        }
        if (remove && m_instCount > 0)
        {
            --m_instCount;
        }
    }

    // Rendering
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

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
    static const TextureTangentVertex Vertices[24] = {
        // Bottom face
        {Point3f{-0.5, -0.5,  0.5}, Point3f{1, 0, 0}, Point3f{0, -1, 0}, Point2f{0, 1}},
        {Point3f{ 0.5, -0.5,  0.5}, Point3f{1, 0, 0}, Point3f{0, -1, 0}, Point2f{1, 1}},
        {Point3f{ 0.5, -0.5, -0.5}, Point3f{1, 0, 0}, Point3f{0, -1, 0}, Point2f{1, 0}},
        {Point3f{-0.5, -0.5, -0.5}, Point3f{1, 0, 0}, Point3f{0, -1, 0}, Point2f{0, 0}},
        // Top face
        {Point3f{-0.5,  0.5, -0.5}, Point3f{1, 0, 0}, Point3f{0, 1, 0}, Point2f{0, 1}},
        {Point3f{ 0.5,  0.5, -0.5}, Point3f{1, 0, 0}, Point3f{0, 1, 0}, Point2f{1, 1}},
        {Point3f{ 0.5,  0.5,  0.5}, Point3f{1, 0, 0}, Point3f{0, 1, 0}, Point2f{1, 0}},
        {Point3f{-0.5,  0.5,  0.5}, Point3f{1, 0, 0}, Point3f{0, 1, 0}, Point2f{0, 0}},
        // Front face
        {Point3f{ 0.5, -0.5, -0.5}, Point3f{0, 0, 1}, Point3f{1, 0, 0}, Point2f{0, 1}},
        {Point3f{ 0.5, -0.5,  0.5}, Point3f{0, 0, 1}, Point3f{1, 0, 0}, Point2f{1, 1}},
        {Point3f{ 0.5,  0.5,  0.5}, Point3f{0, 0, 1}, Point3f{1, 0, 0}, Point2f{1, 0}},
        {Point3f{ 0.5,  0.5, -0.5}, Point3f{0, 0, 1}, Point3f{1, 0, 0}, Point2f{0, 0}},
        // Back face
        {Point3f{-0.5, -0.5,  0.5}, Point3f{0, 0, -1}, Point3f{-1, 0, 0}, Point2f{0, 1}},
        {Point3f{-0.5, -0.5, -0.5}, Point3f{0, 0, -1}, Point3f{-1, 0, 0}, Point2f{1, 1}},
        {Point3f{-0.5,  0.5, -0.5}, Point3f{0, 0, -1}, Point3f{-1, 0, 0}, Point2f{1, 0}},
        {Point3f{-0.5,  0.5,  0.5}, Point3f{0, 0, -1}, Point3f{-1, 0, 0}, Point2f{0, 0}},
        // Left face
        {Point3f{ 0.5, -0.5,  0.5}, Point3f{-1, 0, 0}, Point3f{0, 0, 1}, Point2f{0, 1}},
        {Point3f{-0.5, -0.5,  0.5}, Point3f{-1, 0, 0}, Point3f{0, 0, 1}, Point2f{1, 1}},
        {Point3f{-0.5,  0.5,  0.5}, Point3f{-1, 0, 0}, Point3f{0, 0, 1}, Point2f{1, 0}},
        {Point3f{ 0.5,  0.5,  0.5}, Point3f{-1, 0, 0}, Point3f{0, 0, 1}, Point2f{0, 0}},
        // Right face
        {Point3f{-0.5, -0.5, -0.5}, Point3f{1, 0, 0}, Point3f{0, 0, -1}, Point2f{0, 1}},
        {Point3f{ 0.5, -0.5, -0.5}, Point3f{1, 0, 0}, Point3f{0, 0, -1}, Point2f{1, 1}},
        {Point3f{ 0.5,  0.5, -0.5}, Point3f{1, 0, 0}, Point3f{0, 0, -1}, Point2f{1, 0}},
        {Point3f{-0.5,  0.5, -0.5}, Point3f{1, 0, 0}, Point3f{0, 0, -1}, Point2f{0, 0}}
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
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0}
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
        result = CompileAndCreateShader(L"SimpleTexture.vs", (ID3D11DeviceChild**)&m_pVertexShader, {}, &pVertexShaderCode);
    }
    if (SUCCEEDED(result))
    {
        result = CompileAndCreateShader(L"SimpleTexture.ps", (ID3D11DeviceChild**)&m_pPixelShader);
    }

    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateInputLayout(InputDesc, 4, pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &m_pInputLayout);
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
        desc.ByteWidth = sizeof(GeomBuffer) * MaxInst;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        result = m_pDevice->CreateBuffer(&desc, nullptr, &m_pGeomBufferInst);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pGeomBufferInst, "GeomBufferInst");
        }
        if (SUCCEEDED(result))
        {
            m_geomBuffers[0].shineSpeedTexIdNM.x = 0.0f;
            m_geomBuffers[0].shineSpeedTexIdNM.y = ModelRotationSpeed;
            m_geomBuffers[0].shineSpeedTexIdNM.z = 0.0f;
            int useNM = 1;
            m_geomBuffers[0].shineSpeedTexIdNM.w = *reinterpret_cast<float*>(&useNM);
            m_geomBuffers[0].posAngle = Point4f{ 0.00001f, 0, 0, 0 };

            m_geomBuffers[1].shineSpeedTexIdNM.x = 64.0f;
            m_geomBuffers[1].shineSpeedTexIdNM.y = 0.0f;
            m_geomBuffers[1].shineSpeedTexIdNM.z = 0.0f;
            m_geomBuffers[1].shineSpeedTexIdNM.w = *reinterpret_cast<float*>(&useNM);
            m_geomBuffers[1].posAngle = Point4f{ 2.0f, 0, 0, 0 };
            DirectX::XMMATRIX m = DirectX::XMMatrixMultiply(
                DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f), -(float)m_geomBuffers[1].posAngle.w),
                DirectX::XMMatrixTranslation(m_geomBuffers[1].posAngle.x, m_geomBuffers[1].posAngle.y, m_geomBuffers[1].posAngle.z)
            );
            m_geomBuffers[1].m = m;
            m = DirectX::XMMatrixInverse(nullptr, m);
            m = DirectX::XMMatrixTranspose(m);
            m_geomBuffers[1].normalM = m;

            for (int i = 2; i < 10; i++)
            {
                InitGeom(m_geomBuffers[i]);
            }
            m_instCount = 10;
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
        const std::wstring TextureName = L"../Common/Brick.dds";

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
        const std::wstring TextureName = L"../Common/BrickNM.dds";

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
        result = m_pDevice->CreateTexture2D(&desc, data.data(), &m_pTextureNM);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pTextureNM, WCSToMBS(TextureName));
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

        result = m_pDevice->CreateShaderResourceView(m_pTextureNM, &desc, &m_pTextureViewNM);
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
    if (SUCCEEDED(result))
    {
        result = InitSmallSphere();
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
        result = CompileAndCreateShader(L"SphereTexture.vs", (ID3D11DeviceChild**)&m_pSphereVertexShader, {}, &pSphereVertexShaderCode);
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

HRESULT Renderer::InitSmallSphere()
{
    static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    HRESULT result = S_OK;

    static const size_t SphereSteps = 8;

    std::vector<Point3f> sphereVertices;
    std::vector<UINT16> indices;

    size_t indexCount;
    size_t vertexCount;

    GetSphereDataSize(SphereSteps, SphereSteps, indexCount, vertexCount);

    sphereVertices.resize(vertexCount);
    indices.resize(indexCount);

    m_smallSphereIndexCount = (UINT)indexCount;

    CreateSphere(SphereSteps, SphereSteps, indices.data(), sphereVertices.data());

    for (auto& v : sphereVertices)
    {
        v = v * 0.125f;
    }

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

        result = m_pDevice->CreateBuffer(&desc, &data, &m_pSmallSphereVertexBuffer);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pSmallSphereVertexBuffer, "SmallSphereVertexBuffer");
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

        result = m_pDevice->CreateBuffer(&desc, &data, &m_pSmallSphereIndexBuffer);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pSmallSphereIndexBuffer, "SmallSphereIndexBuffer");
        }
    }

    ID3DBlob* pSmallSphereVertexShaderCode = nullptr;
    if (SUCCEEDED(result))
    {
        result = CompileAndCreateShader(L"TransColor.vs", (ID3D11DeviceChild**)&m_pSmallSphereVertexShader, {}, &pSmallSphereVertexShaderCode);
    }
    if (SUCCEEDED(result))
    {
        result = CompileAndCreateShader(L"TransColor.ps", (ID3D11DeviceChild**)&m_pSmallSpherePixelShader);
    }

    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateInputLayout(InputDesc, 1, pSmallSphereVertexShaderCode->GetBufferPointer(), pSmallSphereVertexShaderCode->GetBufferSize(), &m_pSmallSphereInputLayout);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pSmallSphereInputLayout, "SmallSphereInputLayout");
        }
    }

    SAFE_RELEASE(pSmallSphereVertexShaderCode);

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
        geomBuffer.color = Point4f{1,1,1,1};

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = &geomBuffer;
        data.SysMemPitch = sizeof(geomBuffer);
        data.SysMemSlicePitch = 0;

        for (int i = 0; i < 10 && SUCCEEDED(result); i++)
        {
            result = m_pDevice->CreateBuffer(&desc, &data, &m_pSmallSphereGeomBuffers[i]);
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pSmallSphereGeomBuffers[i], "SmallSphereGeomBuffer");
            }
        }
    }

    assert(SUCCEEDED(result));

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

    for (int i = 0; i < 4; i++)
    {
        m_boundingRects[0].v[i] = Point3f{ Vertices[i].x, Vertices[i].y, Vertices[i].z } + Rect0Pos;
        m_boundingRects[1].v[i] = Point3f{ Vertices[i].x, Vertices[i].y, Vertices[i].z } + Rect1Pos;
    }

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
        result = CompileAndCreateShader(L"TransColor.vs", (ID3D11DeviceChild**)&m_pRectVertexShader, {}, &pRectVertexShaderCode);
    }
    if (SUCCEEDED(result))
    {
        result = CompileAndCreateShader(L"TransColor.ps", (ID3D11DeviceChild**)&m_pRectPixelShader, { "USE_LIGHTS" });
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
        geomBuffer.m = DirectX::XMMatrixTranslation(Rect0Pos.x, Rect0Pos.y, Rect0Pos.z);
        geomBuffer.color = Point4f{ 0.5f, 0, 0.5f, 0.5f };

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
            geomBuffer.m = DirectX::XMMatrixTranslation(Rect1Pos.x, Rect1Pos.y, Rect1Pos.z);
            geomBuffer.color = Point4f{ 0.5f, 0.5f, 0, 0.5f };

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

void Renderer::UpdateCubes(double deltaSec)
{
    if (m_rotateModel)
    {
        for (UINT i = 0; i < m_instCount; i++)
        {
            if (fabs(m_geomBuffers[i].shineSpeedTexIdNM.y) > 0.0001)
            {
                m_geomBuffers[i].posAngle.w = m_geomBuffers[i].posAngle.w + (float)deltaSec * m_geomBuffers[i].shineSpeedTexIdNM.y;

                // Model matrix
                // Angle is reversed, as DirectXMath calculates it as clockwise
                DirectX::XMMATRIX m =DirectX::XMMatrixMultiply(
                    DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f), -(float)m_geomBuffers[i].posAngle.w),
                    DirectX::XMMatrixTranslation(m_geomBuffers[i].posAngle.x, m_geomBuffers[i].posAngle.y, m_geomBuffers[i].posAngle.z)
                );

                m_geomBuffers[i].m = m;
                m = DirectX::XMMatrixInverse(nullptr, m);
                m = DirectX::XMMatrixTranspose(m);
                m_geomBuffers[i].normalM = m;
            }
        }

        m_pDeviceContext->UpdateSubresource(m_pGeomBufferInst, 0, nullptr, m_geomBuffers.data(), 0, 0);
    }
}

void Renderer::InitGeom(GeomBuffer& geomBuffer)
{
    Point3f offset = Point3f{ randNormf(), randNormf(), randNormf() } *7.0f - Point3f{ 3.5f, 3.5f, 3.5f };

    geomBuffer.shineSpeedTexIdNM.x = randNormf() > 0.5f ? 64.0f : 0.0f;
    geomBuffer.shineSpeedTexIdNM.y = randNormf() * 2 * (float)M_PI;
    geomBuffer.shineSpeedTexIdNM.z = 0;
    int useNM = 1;
    geomBuffer.shineSpeedTexIdNM.w = *reinterpret_cast<float*>(&useNM);

    geomBuffer.posAngle = Point4f{ offset.x, offset.y, offset.z, 0};
}

void Renderer::TermScene()
{
    SAFE_RELEASE(m_pSampler);

    SAFE_RELEASE(m_pTextureView);
    SAFE_RELEASE(m_pTexture);
    SAFE_RELEASE(m_pTextureViewNM);
    SAFE_RELEASE(m_pTextureNM);

    SAFE_RELEASE(m_pRasterizerState);
    SAFE_RELEASE(m_pDepthState);
    SAFE_RELEASE(m_pTransDepthState);

    SAFE_RELEASE(m_pInputLayout);
    SAFE_RELEASE(m_pPixelShader);
    SAFE_RELEASE(m_pVertexShader);

    SAFE_RELEASE(m_pIndexBuffer);
    SAFE_RELEASE(m_pVertexBuffer);

    SAFE_RELEASE(m_pSceneBuffer);
    SAFE_RELEASE(m_pGeomBufferInst);

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

    // Term small sphere
    SAFE_RELEASE(m_pSmallSphereIndexBuffer);
    SAFE_RELEASE(m_pSmallSphereVertexBuffer);
    SAFE_RELEASE(m_pSmallSphereInputLayout);
    SAFE_RELEASE(m_pSmallSphereVertexShader);
    SAFE_RELEASE(m_pSmallSpherePixelShader);
    for (int i = 0; i < 10; i++)
    {
        SAFE_RELEASE(m_pSmallSphereGeomBuffers[i]);
    }
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

void Renderer::RenderSmallSpheres()
{
    m_pDeviceContext->OMSetBlendState(m_pOpaqueBlendState, nullptr, 0xffffffff);
    m_pDeviceContext->OMSetDepthStencilState(m_pDepthState, 0);

    m_pDeviceContext->IASetIndexBuffer(m_pSmallSphereIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { m_pSmallSphereVertexBuffer };
    UINT strides[] = { 12 };
    UINT offsets[] = { 0 };
    m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    m_pDeviceContext->IASetInputLayout(m_pSmallSphereInputLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->VSSetShader(m_pSmallSphereVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pSmallSpherePixelShader, nullptr, 0);

    for (int i = 0; i < m_sceneBuffer.lightCount.x; i++)
    {
        ID3D11Buffer* cbuffers[] = { m_pSceneBuffer, m_pSmallSphereGeomBuffers[i] };
        m_pDeviceContext->VSSetConstantBuffers(0, 2, cbuffers);
        m_pDeviceContext->PSSetConstantBuffers(0, 2, cbuffers);
        m_pDeviceContext->DrawIndexed(m_smallSphereIndexCount, 0, 0);
    }
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

    float d0 = 0.0f, d1 = 0.0f;
    Point3f cameraPos = m_camera.poi + Point3f{ cosf(m_camera.theta) * cosf(m_camera.phi), sinf(m_camera.theta), cosf(m_camera.theta) * sinf(m_camera.phi) } *m_camera.r;
    for (int i = 0; i < 4; i++)
    {
        d0 = std::max(d0, (cameraPos - m_boundingRects[0].v[i]).lengthSqr());
        d1 = std::max(d1, (cameraPos - m_boundingRects[1].v[i]).lengthSqr());
    }

    if (d0 > d1)
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

class D3DInclude : public ID3DInclude
{
    STDMETHOD(Open)(THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes)
    {
        FILE* pFile = nullptr;
        fopen_s(&pFile, pFileName, "rb");
        assert(pFile != nullptr);
        if (pFile == nullptr)
        {
            return E_FAIL;
        }

        fseek(pFile, 0, SEEK_END);
        long long size = _ftelli64(pFile);
        fseek(pFile, 0, SEEK_SET);

        VOID* pData = malloc(size);
        if (pData == nullptr)
        {
            fclose(pFile);
            return E_FAIL;
        }

        size_t rd = fread(pData, 1, size, pFile);
        assert(rd == (size_t)size);

        if (rd != (size_t)size)
        {
            fclose(pFile);
            free(pData);
            return E_FAIL;
        }

        *ppData = pData;
        *pBytes = (UINT)size;

        return S_OK;
    }
    STDMETHOD(Close)(THIS_ LPCVOID pData)
    {
        free(const_cast<void*>(pData));
        return S_OK;
    }
};

HRESULT Renderer::CompileAndCreateShader(const std::wstring& path, ID3D11DeviceChild** ppShader, const std::vector<std::string>& defines, ID3DBlob** ppCode)
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

    D3DInclude includeHandler;

    std::vector<D3D_SHADER_MACRO> shaderDefines;
    shaderDefines.resize(defines.size() + 1);
    for (int i = 0; i < defines.size(); i++)
    {
        shaderDefines[i].Name = defines[i].c_str();
        shaderDefines[i].Definition = "";
    }
    shaderDefines.back().Name = nullptr;
    shaderDefines.back().Definition = nullptr;

    // Try to compile
    ID3DBlob* pCode = nullptr;
    ID3DBlob* pErrMsg = nullptr;
    HRESULT result = D3DCompile(data.data(), data.size(), WCSToMBS(path).c_str(), shaderDefines.data(), &includeHandler, entryPoint.c_str(), platform.c_str(), flags1, 0, &pCode, &pErrMsg);
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
