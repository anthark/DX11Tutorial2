#pragma once

#include <dxgi.h>
#include <d3d11.h>

#include "../Math/Point.h"

class Renderer
{
    static const double PanSpeed;

    static const Point3f Rect0Pos;
    static const Point3f Rect1Pos;

public:
    static const int MaxInst = 100;

public:
    Renderer()
        : m_pDevice(nullptr)
        , m_pDeviceContext(nullptr)
        , m_pSwapChain(nullptr)
        , m_pBackBufferRTV(nullptr)
        , m_pDepthBuffer(nullptr)
        , m_pDepthBufferDSV(nullptr)
        , m_pDepthState(nullptr)
        , m_pTransDepthState(nullptr)
        , m_width(16)
        , m_height(16)
        , m_pGeomBufferInst(nullptr)
        , m_pGeomBufferInstVis(nullptr)
        , m_pSceneBuffer(nullptr)
        , m_pVertexBuffer(nullptr)
        , m_pIndexBuffer(nullptr)
        , m_pPixelShader(nullptr)
        , m_pVertexShader(nullptr)
        , m_pInputLayout(nullptr)
        , m_pRectGeomBuffer(nullptr)
        , m_pRectGeomBuffer2(nullptr)
        , m_pRectVertexBuffer(nullptr)
        , m_pRectIndexBuffer(nullptr)
        , m_pRectPixelShader(nullptr)
        , m_pRectVertexShader(nullptr)
        , m_pRectInputLayout(nullptr)
        , m_pSphereGeomBuffer(nullptr)
        , m_pSphereVertexBuffer(nullptr)
        , m_pSphereIndexBuffer(nullptr)
        , m_pSpherePixelShader(nullptr)
        , m_pSphereVertexShader(nullptr)
        , m_pSphereInputLayout(nullptr)
        , m_sphereIndexCount(0)
        , m_pSmallSphereVertexBuffer(nullptr)
        , m_pSmallSphereIndexBuffer(nullptr)
        , m_pSmallSpherePixelShader(nullptr)
        , m_pSmallSphereVertexShader(nullptr)
        , m_pSmallSphereInputLayout(nullptr)
        , m_smallSphereIndexCount(0)
        , m_pCubemapTexture(nullptr)
        , m_pCubemapView(nullptr)
        , m_pRasterizerState(nullptr)
        , m_pTransBlendState(nullptr)
        , m_pOpaqueBlendState(nullptr)
        , m_pColorBuffer(nullptr)
        , m_pColorBufferRTV(nullptr)
        , m_pColorBufferSRV(nullptr)
        , m_pSepiaPixelShader(nullptr)
        , m_pSepiaVertexShader(nullptr)
        , m_prevUSec(0)
        , m_rbPressed(false)
        , m_prevMouseX(0)
        , m_prevMouseY(0)
        , m_rotateModel(true)
        , m_angle(0.0)
        , m_pTexture(nullptr)
        , m_pTextureView(nullptr)
        , m_pTextureNM(nullptr)
        , m_pTextureViewNM(nullptr)
        , m_pSampler(nullptr)
        , m_forwardDelta(0.0)
        , m_rightDelta(0.0)
        , m_showLightBulbs(true)
        , m_useNormalMaps(true)
        , m_showNormals(false)
        , m_doCull(true)
        , m_useSepia(false)
        , m_geomBuffers(MaxInst)
        , m_geomBBs(MaxInst)
        , m_instCount(2)
        , m_visibleInstances(0)
        , m_computeCull(false)
        , m_pCullShader(nullptr)
        , m_pIndirectArgsSrc(nullptr)
        , m_pIndirectArgs(nullptr)
        , m_pCullParams(nullptr)
        , m_pGeomBufferInstVisGPU(nullptr)
        , m_pGeomBufferInstVisGPU_UAV(nullptr)
        , m_pIndirectArgsUAV(nullptr)
        , m_updateCullParams(false)
        , m_curFrame(0)
        , m_lastCompletedFrame(0)
        , m_gpuVisibleInstances(0)
    {
        for (int i = 0; i < 10; i++)
        {
            m_pSmallSphereGeomBuffers[i] = nullptr;
        }
        for (int i = 0; i < 10; i++)
        {
            m_queries[i] = nullptr;
        }
    }

    bool Init(HWND hWnd);
    void Term();

    bool Update();
    bool Render();
    bool Resize(UINT width, UINT height);

    void MouseRBPressed(bool pressed, int x, int y);
    void MouseMoved(int x, int y);
    void MouseWheel(int delta);
    void KeyPressed(int keyCode);
    void KeyReleased(int keyCode);

private:
    struct Camera
    {
        Point3f poi;    // Point of interest
        float r;        // Distance to POI
        float phi;      // Angle in plane x0z
        float theta;    // Angle from plane x0z

        void GetDirections(Point3f& forward, Point3f& right);
    };

    struct Light
    {
        Point4f pos = Point4f{ 0,0,0,0 };
        Point4f color = Point4f{ 1,1,1,0 };
    };

    struct SceneBuffer
    {
        DirectX::XMMATRIX vp;
        Point4f cameraPos;
        Point4i lightCount; // x - light count (max 10), y - use normal maps, z - show normals, w - do culling
        Point4i postProcess; // x - use sepia
        Light lights[10];
        Point4f ambientColor;
        Point4f frustum[6];
    };

    struct AABB
    {
        Point3f vmin = Point3f{
            std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()
        };
        Point3f vmax = Point3f{
            std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()
        };

        inline Point3f GetVert(int idx) const
        {
            return Point3f
            {
                (idx & 1) == 0 ? vmin.x : vmax.x,
                (idx & 2) == 0 ? vmin.y : vmax.y,
                (idx & 4) == 0 ? vmin.z : vmax.z
            };
        }
    };

    struct GeomBuffer
    {
        DirectX::XMMATRIX m;
        DirectX::XMMATRIX normalM;
        Point4f shineSpeedTexIdNM; // x - shininess, y - rotation speed, z - texture id, w - normal map presence
        Point4f posAngle; // xyz - position, w - current angle
    };

private:
    HRESULT SetupBackBuffer();
    HRESULT InitScene();
    HRESULT InitSphere();
    HRESULT InitSmallSphere();
    HRESULT InitRect();
    HRESULT InitCubemap();
    HRESULT InitPostProcess();
    HRESULT InitCull();

    void UpdateCubes(double deltaSec);

    void InitGeom(GeomBuffer& geomBuffer, AABB& bb);

    void TermScene();

    void RenderSphere();
    void RenderSmallSpheres();
    void RenderRects();
    void RenderPostProcess();
    void ReadQueries();

    void CalcFrustum(Point4f frutsum[6]);
    void CullBoxes();

    HRESULT CompileAndCreateShader(const std::wstring& path, ID3D11DeviceChild** ppShader, const std::vector<std::string>& defines = {}, ID3DBlob** ppCode = nullptr);

private:
    ID3D11Device* m_pDevice;
    ID3D11DeviceContext* m_pDeviceContext;

    IDXGISwapChain* m_pSwapChain;
    ID3D11RenderTargetView* m_pBackBufferRTV;

    ID3D11Texture2D* m_pDepthBuffer;
    ID3D11DepthStencilView* m_pDepthBufferDSV;

    ID3D11DepthStencilState* m_pDepthState;
    ID3D11DepthStencilState* m_pTransDepthState;

    ID3D11Buffer* m_pSceneBuffer;

    // For cubes
    ID3D11Buffer* m_pGeomBufferInst;
    ID3D11Buffer* m_pGeomBufferInstVis;
    ID3D11Buffer* m_pVertexBuffer;
    ID3D11Buffer* m_pIndexBuffer;
    ID3D11PixelShader* m_pPixelShader;
    ID3D11VertexShader* m_pVertexShader;
    ID3D11InputLayout* m_pInputLayout;
    std::vector<GeomBuffer> m_geomBuffers;
    std::vector<AABB> m_geomBBs;
    UINT m_instCount;
    UINT m_visibleInstances;

    // For sphere
    ID3D11Buffer* m_pSphereGeomBuffer;
    ID3D11Buffer* m_pSphereVertexBuffer;
    ID3D11Buffer* m_pSphereIndexBuffer;
    ID3D11PixelShader* m_pSpherePixelShader;
    ID3D11VertexShader* m_pSphereVertexShader;
    ID3D11InputLayout* m_pSphereInputLayout;
    UINT m_sphereIndexCount;

    // For small sphere
    ID3D11Buffer* m_pSmallSphereGeomBuffers[10];
    ID3D11Buffer* m_pSmallSphereVertexBuffer;
    ID3D11Buffer* m_pSmallSphereIndexBuffer;
    ID3D11PixelShader* m_pSmallSpherePixelShader;
    ID3D11VertexShader* m_pSmallSphereVertexShader;
    ID3D11InputLayout* m_pSmallSphereInputLayout;
    UINT m_smallSphereIndexCount;

    // For rect
    ID3D11Buffer* m_pRectGeomBuffer;
    ID3D11Buffer* m_pRectGeomBuffer2;
    ID3D11Buffer* m_pRectVertexBuffer;
    ID3D11Buffer* m_pRectIndexBuffer;
    ID3D11PixelShader* m_pRectPixelShader;
    ID3D11VertexShader* m_pRectVertexShader;
    ID3D11InputLayout* m_pRectInputLayout;

    ID3D11Texture2D* m_pCubemapTexture;
    ID3D11ShaderResourceView* m_pCubemapView;

    ID3D11RasterizerState* m_pRasterizerState;

    ID3D11BlendState* m_pTransBlendState;
    ID3D11BlendState* m_pOpaqueBlendState;

    ID3D11Texture2D* m_pTexture;
    ID3D11ShaderResourceView* m_pTextureView;
    ID3D11Texture2D* m_pTextureNM;
    ID3D11ShaderResourceView* m_pTextureViewNM;
    ID3D11SamplerState* m_pSampler;

    ID3D11Texture2D* m_pColorBuffer;
    ID3D11RenderTargetView* m_pColorBufferRTV;
    ID3D11ShaderResourceView* m_pColorBufferSRV;
    ID3D11PixelShader* m_pSepiaPixelShader;
    ID3D11VertexShader* m_pSepiaVertexShader;

    ID3D11ComputeShader* m_pCullShader;
    ID3D11Buffer* m_pIndirectArgsSrc;
    ID3D11Buffer* m_pIndirectArgs;
    ID3D11Buffer* m_pCullParams;
    ID3D11Buffer* m_pGeomBufferInstVisGPU;
    ID3D11UnorderedAccessView* m_pGeomBufferInstVisGPU_UAV;
    ID3D11UnorderedAccessView* m_pIndirectArgsUAV;
    ID3D11Query* m_queries[10];
    UINT64 m_curFrame;
    UINT64 m_lastCompletedFrame;

    AABB m_boundingRects[2];

    UINT m_width;
    UINT m_height;

    Camera m_camera;
    bool m_rbPressed;
    int m_prevMouseX;
    int m_prevMouseY;
    bool m_rotateModel;
    double m_angle;
    double m_forwardDelta;
    double m_rightDelta;

    bool m_showLightBulbs;
    bool m_useNormalMaps;
    bool m_showNormals;
    bool m_doCull;
    bool m_useSepia;
    bool m_computeCull;
    bool m_updateCullParams;

    int m_gpuVisibleInstances;

    size_t m_prevUSec;

    SceneBuffer m_sceneBuffer;
};
