// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_TESTS_INTEGRATION_TESTS_COMMON_H_
#define SANDBOX_TESTS_INTEGRATION_TESTS_COMMON_H_

#include <windows.h>

// Use the same header file for DLL and importers.
#ifdef _DLL_EXPORTING
#define DECLSPEC extern "C" __declspec(dllexport)
#else
#define DECLSPEC extern "C" __declspec(dllimport)
#endif

//------------------------------------------------------------------------------
// Tests
//------------------------------------------------------------------------------
const wchar_t* g_extension_point_test_mutex = L"ChromeExtensionTestMutex";

//------------------------------------------------------------------------------
// Hooking WinProc exe.
//------------------------------------------------------------------------------
const wchar_t* g_winproc_file = L"sbox_integration_test_win_proc.exe ";
const wchar_t* g_winproc_class_name = L"myWindowClass";
const wchar_t* g_winproc_window_name = L"ChromeMitigationTests";
const wchar_t* g_winproc_event = L"ChromeExtensionTestEvent";

//------------------------------------------------------------------------------
// Hooking dll.
//------------------------------------------------------------------------------
const wchar_t* g_hook_dll_file = L"sbox_integration_test_hook_dll.dll";
const wchar_t* g_hook_event = L"ChromeExtensionTestHookEvent";
const char* g_hook_handler_func = "HookProc";
const char* g_was_hook_called_func = "WasHookCalled";
const char* g_set_hook_func = "SetHook";

DECLSPEC LRESULT HookProc(int code, WPARAM wParam, LPARAM lParam);
DECLSPEC bool WasHookCalled();
DECLSPEC void SetHook(HHOOK hook_handle);

#endif  // SANDBOX_TESTS_INTEGRATION_TESTS_COMMON_H_
