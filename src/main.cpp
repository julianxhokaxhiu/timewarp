/****************************************************************************/
//    Copyright (C) 2026 Julian Xhokaxhiu                                   //
//                                                                          //
//    This file is part of timewarp                                         //
//                                                                          //
//    timewarp is free software: you can redistribute it and/or modify      //
//    it under the terms of the GNU General Public License as published by  //
//    the Free Software Foundation, either version 3 of the License         //
//                                                                          //
//    timewarp is distributed in the hope that it will be useful,           //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of        //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
//    GNU General Public License for more details.                          //
/****************************************************************************/

#include <windows.h>
#include <string>
#include <mutex>

extern "C" void faketime_initialize(HMODULE self);

static HMODULE g_realDxgi = nullptr;
static std::once_flag g_loadOnce;

static bool LoadRealDxgi()
{
    std::call_once(g_loadOnce, [] {
        wchar_t sysDir[MAX_PATH] = {};
        if (!GetSystemDirectoryW(sysDir, MAX_PATH))
            return;

        std::wstring path = sysDir;
        if (!path.empty() && path.back() != L'\\')
            path += L"\\";
        path += L"dxgi.dll";

        g_realDxgi = LoadLibraryW(path.c_str());
    });

    return g_realDxgi != nullptr;
}

static FARPROC GetRealProc(const char* name)
{
    if (!LoadRealDxgi())
        return nullptr;
    return GetProcAddress(g_realDxgi, name);
}

using PFN_CreateDXGIFactory = HRESULT(WINAPI*)(REFIID, void**);
using PFN_CreateDXGIFactory1 = HRESULT(WINAPI*)(REFIID, void**);
using PFN_CreateDXGIFactory2 = HRESULT(WINAPI*)(UINT, REFIID, void**);
using PFN_DXGIDeclareAdapterRemovalSupport = HRESULT(WINAPI*)();

extern "C" __declspec(dllexport) HRESULT WINAPI CreateDXGIFactory(REFIID riid, void** ppFactory)
{
    auto p = reinterpret_cast<PFN_CreateDXGIFactory>(GetRealProc("CreateDXGIFactory"));
    return p ? p(riid, ppFactory) : E_FAIL;
}

extern "C" __declspec(dllexport) HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void** ppFactory)
{
    auto p = reinterpret_cast<PFN_CreateDXGIFactory1>(GetRealProc("CreateDXGIFactory1"));
    return p ? p(riid, ppFactory) : E_FAIL;
}

extern "C" __declspec(dllexport) HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory)
{
    auto p = reinterpret_cast<PFN_CreateDXGIFactory2>(GetRealProc("CreateDXGIFactory2"));
    return p ? p(Flags, riid, ppFactory) : E_FAIL;
}

extern "C" __declspec(dllexport) HRESULT WINAPI DXGIDeclareAdapterRemovalSupport()
{
    auto p = reinterpret_cast<PFN_DXGIDeclareAdapterRemovalSupport>(GetRealProc("DXGIDeclareAdapterRemovalSupport"));
    return p ? p() : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinst);
        faketime_initialize(hinst);
    }
    return TRUE;
}
