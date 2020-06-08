#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <d3d9.h>
#include "Include/d3dx9.h"
#include "detours.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "Lib/x86/d3dx9.lib")
#pragma comment(lib, "detours.lib")

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"

#include "helpers.h"

#define DEBUG true

#define INIT(a) static bool success = false; if (!success) { success = true; a;}

struct globals
{
    bool bIsMenuOpen;
    HWND hGame;
    HMODULE hmModule;
    WNDPROC wndproc_o;
}globals_t;

using fEndScene = HRESULT(WINAPI*)(LPDIRECT3DDEVICE9 device);
using fSetCursorPos = BOOL(WINAPI*)(int x, int y);

fEndScene pEndScene = NULL;
fSetCursorPos pSetCursorPos = NULL;

namespace menu
{
    void initImGui(HWND h, LPDIRECT3DDEVICE9 device)
    {
        ImGui::CreateContext();
        ImGui_ImplDX9_Init(device);
        ImGui_ImplWin32_Init(h);

        auto& io = ImGui::GetIO();
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 15.0f, NULL, io.Fonts->GetGlyphRangesCyrillic());

        ImGui::StyleColorsLight();
    }

    static bool a;
    static int b;
    static float c[4];

    void draw()
    {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();

        ImGui::NewFrame();

        ImGui::Begin("test", nullptr, ImVec2(300, 200), 1.0f);

        ImGui::Text("kek");
        ImGui::Checkbox("checkbox", &a);
        ImGui::SliderInt("slider int", &b, 0, 100);
        ImGui::ColorEdit4("colorpicker", c);
        ImGui::End();

        ImGui::Render();

        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }
}

HRESULT WINAPI endscene_hooked(LPDIRECT3DDEVICE9 device)
{
    INIT(menu::initImGui(globals_t.hGame, device))

    if (globals_t.bIsMenuOpen) {
        menu::draw();
        ImGui::GetIO().MouseDrawCursor = true;
    }

    return pEndScene(device);
}

LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

HRESULT WINAPI wndproc_hooked(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT)
    {
        globals_t.bIsMenuOpen = !globals_t.bIsMenuOpen;
        return TRUE;
    }

    if (globals_t.bIsMenuOpen)
    {
        ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam);
        return TRUE;
    }

    CallWindowProc(globals_t.wndproc_o, hwnd, uMsg, wParam, lParam);
}

BOOL WINAPI setcursorpos_hooked(int x, int y)
{
    if (globals_t.bIsMenuOpen)
        return TRUE;

    return pSetCursorPos(x, y);
}

void setup(void)
{
#if DEBUG
    helpers::console::attach("debug");
#endif // DEBUG

    globals_t.hGame = FindWindowA(NULL, "ImGui DirectX9 Example");//ImGui DirectX9 Example GTA: San Andreas GTA:SA:MP

    LPDIRECT3D9 pD3D;
    if ((pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
    {
        std::cout << "failed to create dx9 context\n";
        FreeLibraryAndExitThread(globals_t.hmModule, 1);
    }

    D3DPRESENT_PARAMETERS d3dpp;
    d3dpp.hDeviceWindow = globals_t.hGame;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.Windowed = FALSE;

    LPDIRECT3DDEVICE9 Device;
    if (FAILED(pD3D->CreateDevice(0, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &Device)))
    {      
        std::cout << "failed to create device #1\n";
        d3dpp.Windowed = !d3dpp.Windowed;
        if (FAILED(pD3D->CreateDevice(0, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &Device)))
        {
            std::cout << "failed to create device #2\n";
            pD3D->Release();
            FreeLibraryAndExitThread(globals_t.hmModule, 1);
        }
    }

    void** pVTable = *reinterpret_cast<void***>(Device);

    if (Device)
        Device->Release(), Device = nullptr;

    pEndScene = reinterpret_cast<fEndScene>(DetourFunction(reinterpret_cast<PBYTE>(pVTable[42]), reinterpret_cast<PBYTE>(endscene_hooked)));

    globals_t.wndproc_o = reinterpret_cast<WNDPROC>(SetWindowLong(globals_t.hGame, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(&wndproc_hooked)));

    pSetCursorPos = reinterpret_cast<fSetCursorPos>(DetourFunction(reinterpret_cast<PBYTE>(SetCursorPos), reinterpret_cast<PBYTE>(&setcursorpos_hooked)));

    Sleep(100);
    
    globals_t.bIsMenuOpen = true;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        globals_t.hmModule = hModule;
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)setup, 0, 0, 0);
        return TRUE;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
