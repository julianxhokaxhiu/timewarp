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
#include <winternl.h>
#include <cstdint>
#include <fstream>
#include <string>
#include <toml++/toml.h>
#include <mutex>

#include <detours/detours.h>

// MinHook headers are not bundled; this file expects a local MinHook integration.
// For this workspace we provide a tiny IAT hook implementation instead.

namespace timewarp
{

struct Config
{
    bool enabled = false;
    // Custom time as ISO 8601 string (e.g. "2024-01-10T12:34:56")
    std::string custom_time;
    bool has_custom_time = false;
    // Custom time expressed as FILETIME (UTC)
    FILETIME custom_time_filetime = {0, 0};
    // Real time (FILETIME) captured when the config is first loaded
    FILETIME real_time_filetime_at_start = {0, 0};
    Config() : enabled(false), custom_time(), has_custom_time(false) {}
};

static Config g_cfg;
static std::once_flag g_cfgOnce;
static HMODULE g_selfModule = nullptr;

using PFN_GetSystemTimeAsFileTime = VOID(WINAPI*)(LPFILETIME);
using PFN_GetSystemTimePreciseAsFileTime = VOID(WINAPI*)(LPFILETIME);
using PFN_GetSystemTime = VOID(WINAPI*)(LPSYSTEMTIME);
using PFN_GetLocalTime = VOID(WINAPI*)(LPSYSTEMTIME);

static PFN_GetSystemTimeAsFileTime g_origGetSystemTimeAsFileTime = nullptr;
static PFN_GetSystemTimePreciseAsFileTime g_origGetSystemTimePreciseAsFileTime = nullptr;
static PFN_GetSystemTime g_origGetSystemTime = nullptr;
static PFN_GetLocalTime g_origGetLocalTime = nullptr;

static void GetRealSystemTimeAsFileTime(FILETIME* ft)
{
    if (g_origGetSystemTimeAsFileTime)
        g_origGetSystemTimeAsFileTime(ft);
    else
        GetSystemTimeAsFileTime(ft);
}

static std::wstring GetModuleDir()
{
    wchar_t path[MAX_PATH] = {};
    if (!g_selfModule)
        g_selfModule = GetModuleHandleW(L"dxgi.dll");
    GetModuleFileNameW(g_selfModule ? g_selfModule : GetModuleHandleW(nullptr), path, MAX_PATH);
    std::wstring p = path;
    auto pos = p.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"" : p.substr(0, pos);
}

static void LoadConfig()
{
    std::call_once(g_cfgOnce, [] {
        g_cfg = {};
        auto dir = GetModuleDir();
        std::wstring wpath = dir + L"\\timewarp.toml";

        try {
            // Convert wstring to UTF-8 for toml++
            int len = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string path_utf8(len - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, &path_utf8[0], len, nullptr, nullptr);
            auto tbl = toml::parse_file(path_utf8);

            if (auto enabled = tbl["enabled"]) {
                g_cfg.enabled = enabled.value_or(false);
            }
            if (auto ct = tbl["custom_start_time"]) {
                g_cfg.custom_time = ct.value_or("");
                // Parse ISO 8601: YYYY-MM-DDTHH:MM:SS
                const std::string& s = g_cfg.custom_time;
                if (s.size() >= 19 && s[4] == '-' && s[7] == '-' && (s[10] == 'T' || s[10] == 't')) {
                    SYSTEMTIME st = {};
                    try {
                        st.wYear   = static_cast<WORD>(std::stoi(s.substr(0, 4)));
                        st.wMonth  = static_cast<WORD>(std::stoi(s.substr(5, 2)));
                        st.wDay    = static_cast<WORD>(std::stoi(s.substr(8, 2)));
                        st.wHour   = static_cast<WORD>(std::stoi(s.substr(11, 2)));
                        st.wMinute = static_cast<WORD>(std::stoi(s.substr(14, 2)));
                        st.wSecond = static_cast<WORD>(std::stoi(s.substr(17, 2)));
                        // Store custom_time as FILETIME + store real FILETIME at start
                        if (SystemTimeToFileTime(&st, &g_cfg.custom_time_filetime)) {
                            GetRealSystemTimeAsFileTime(&g_cfg.real_time_filetime_at_start);
                            g_cfg.has_custom_time = true;
                        } else {
                            g_cfg.has_custom_time = false;
                        }
                    } catch (...) {
                        g_cfg.has_custom_time = false;
                    }
                }
            }
        } catch (const std::exception&) {
            // Ignore parse errors, leave defaults
        }
    });
}

static void ApplyWarpToFileTime(FILETIME* ft)
{
    if (!g_cfg.enabled)
        return;

    ULARGE_INTEGER ui;
    ui.LowPart = ft->dwLowDateTime;
    ui.HighPart = ft->dwHighDateTime;

    if (g_cfg.has_custom_time)
    {
        ULARGE_INTEGER now, start, custom;
        now.QuadPart = ui.QuadPart;
        start.LowPart = g_cfg.real_time_filetime_at_start.dwLowDateTime;
        start.HighPart = g_cfg.real_time_filetime_at_start.dwHighDateTime;
        custom.LowPart = g_cfg.custom_time_filetime.dwLowDateTime;
        custom.HighPart = g_cfg.custom_time_filetime.dwHighDateTime;

        ULONGLONG elapsed = 0;
        if (now.QuadPart >= start.QuadPart)
            elapsed = now.QuadPart - start.QuadPart;

        ui.QuadPart = custom.QuadPart + elapsed;
    }

    ft->dwLowDateTime = ui.LowPart;
    ft->dwHighDateTime = ui.HighPart;
}

static VOID WINAPI Hook_GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
{
    g_origGetSystemTimeAsFileTime(lpSystemTimeAsFileTime);
    ApplyWarpToFileTime(lpSystemTimeAsFileTime);
}

static VOID WINAPI Hook_GetSystemTimePreciseAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
{
    g_origGetSystemTimePreciseAsFileTime(lpSystemTimeAsFileTime);
    ApplyWarpToFileTime(lpSystemTimeAsFileTime);
}


static VOID WINAPI Hook_GetSystemTime(LPSYSTEMTIME lpSystemTime)
{
    FILETIME ft;
    g_origGetSystemTimeAsFileTime(&ft);
    ApplyWarpToFileTime(&ft);
    FileTimeToSystemTime(&ft, lpSystemTime);
}


static VOID WINAPI Hook_GetLocalTime(LPSYSTEMTIME lpSystemTime)
{
    FILETIME ft;
    g_origGetSystemTimeAsFileTime(&ft);
    ApplyWarpToFileTime(&ft);
    SYSTEMTIME stUtc;
    FileTimeToSystemTime(&ft, &stUtc);
    SystemTimeToTzSpecificLocalTime(nullptr, &stUtc, lpSystemTime);
}

static void InstallHooks()
{
    g_origGetSystemTimeAsFileTime = &GetSystemTimeAsFileTime;
    g_origGetSystemTime = &GetSystemTime;
    g_origGetLocalTime = &GetLocalTime;

    // This API may not exist on older Windows; resolve dynamically.
    g_origGetSystemTimePreciseAsFileTime = reinterpret_cast<PFN_GetSystemTimePreciseAsFileTime>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetSystemTimePreciseAsFileTime"));
    if (!g_origGetSystemTimePreciseAsFileTime)
    {
        auto kb = GetModuleHandleW(L"kernelbase.dll");
        if (kb)
            g_origGetSystemTimePreciseAsFileTime = reinterpret_cast<PFN_GetSystemTimePreciseAsFileTime>(
                GetProcAddress(kb, "GetSystemTimePreciseAsFileTime"));
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<PVOID*>(&g_origGetSystemTimeAsFileTime), Hook_GetSystemTimeAsFileTime);
    DetourAttach(reinterpret_cast<PVOID*>(&g_origGetSystemTime), Hook_GetSystemTime);
    DetourAttach(reinterpret_cast<PVOID*>(&g_origGetLocalTime), Hook_GetLocalTime);
    if (g_origGetSystemTimePreciseAsFileTime)
        DetourAttach(reinterpret_cast<PVOID*>(&g_origGetSystemTimePreciseAsFileTime), Hook_GetSystemTimePreciseAsFileTime);
    DetourTransactionCommit();
}

} // namespace timewarp

extern "C" void faketime_initialize(HMODULE self)
{
    timewarp::g_selfModule = self;
    timewarp::LoadConfig();
    timewarp::InstallHooks();
}
