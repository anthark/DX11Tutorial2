// 1. Window.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "7.Lighting.h"
#include "Renderer.h"

#include <windowsx.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

UINT                WindowWidth = 1280;
UINT                WindowHeight = 720;

Renderer* pRenderer = nullptr;

bool PressedKeys[0xff] = {};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MY7LIGHTING, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Fix working folder
    std::wstring dir;
    dir.resize(MAX_PATH + 1);
    GetCurrentDirectory(MAX_PATH + 1, &dir[0]);
    size_t configPos = dir.find(L"x64");
    if (configPos != std::wstring::npos)
    {
        dir.resize(configPos);
        dir += szTitle;
        SetCurrentDirectory(dir.c_str());
    }

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MY7LIGHTING));

    MSG msg;

    bool exit = false;
    while (!exit)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (msg.message == WM_QUIT)
            {
                exit = true;
            }
        }
        //OutputDebugString(_T("Render\n"));
        if (pRenderer->Update())
        {
            pRenderer->Render();
        }
    }

    pRenderer->Term();
    delete pRenderer;

    return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MY1WINDOW));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    //wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.hbrBackground = nullptr;
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_MY7LIGHTING);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    pRenderer = new Renderer();
    if (!pRenderer->Init(hWnd))
    {
        delete pRenderer;
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Adjust window size
    {
        RECT rc;
        rc.left = 0;
        rc.right = WindowWidth;
        rc.top = 0;
        rc.bottom = WindowHeight;

        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, TRUE);

        MoveWindow(hWnd, 100, 100, rc.right - rc.left, rc.bottom - rc.top, TRUE);
    }

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    switch (message)
    {
    case WM_SIZE:
        if (pRenderer != nullptr)
        {
            RECT rc;
            GetClientRect(hWnd, &rc);
            pRenderer->Resize(rc.right - rc.left, rc.bottom - rc.top);
        }
        break;
    case WM_RBUTTONDOWN:
        if (pRenderer != nullptr)
        {
            pRenderer->MouseRBPressed(true, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        break;

    case WM_RBUTTONUP:
        if (pRenderer != nullptr)
        {
            pRenderer->MouseRBPressed(false, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        break;

    case WM_MOUSEMOVE:
        if (pRenderer != nullptr)
        {
            pRenderer->MouseMoved(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        break;

    case WM_MOUSEWHEEL:
        if (pRenderer != nullptr)
        {
            pRenderer->MouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        }
        break;

    case WM_KEYDOWN:
        if (!PressedKeys[wParam])
        {
            if (pRenderer != nullptr)
            {
                pRenderer->KeyPressed((int)wParam);
            }
            PressedKeys[wParam] = true;
        }
        break;

    case WM_KEYUP:
        if (PressedKeys[wParam])
        {
            if (pRenderer != nullptr)
            {
                pRenderer->KeyReleased((int)wParam);
            }
            PressedKeys[wParam] = false;
        }
        break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
