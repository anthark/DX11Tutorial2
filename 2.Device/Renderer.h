#pragma once

#include <dxgi.h>
#include <d3d11.h>

class Renderer
{
public:
    Renderer()
        : m_pDevice(nullptr)
        , m_pDeviceContext(nullptr)
        , m_pSwapChain(nullptr)
        , m_pBackBufferRTV(nullptr)
        , m_width(16)
        , m_height(16)
    {}

    bool Init(HWND hWnd);
    void Term();

    bool Render();
    bool Resize(UINT width, UINT height);

private:
    HRESULT SetupBackBuffer();

private:
    ID3D11Device* m_pDevice;
    ID3D11DeviceContext* m_pDeviceContext;

    IDXGISwapChain* m_pSwapChain;
    ID3D11RenderTargetView* m_pBackBufferRTV;

    UINT m_width;
    UINT m_height;
};
