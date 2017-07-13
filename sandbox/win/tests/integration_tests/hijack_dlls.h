// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_TESTS_INTEGRATION_TESTS_HIJACK_DLLS_H_
#define SANDBOX_TESTS_INTEGRATION_TESTS_HIJACK_DLLS_H_

#include <windows.h>

namespace hijack_dlls {

constexpr wchar_t g_hijack_dll_file[] = L"sbox_integration_test_hijack_dll.dll";
constexpr wchar_t g_hijack_shim_dll_file[] =
    L"sbox_integration_test_hijack_shim_dll.dll";

// System mutex to prevent conflicting tests from running at the same time.
// This particular mutex is related to the use of the hijack dlls.
constexpr wchar_t g_hijack_dlls_mutex[] = L"ChromeTestHijackDllsMutex";

// Function definition for hijack shim dll.
int CheckHijackResult(bool expect_system);

}  // namespace hijack_dlls

#endif  // SANDBOX_TESTS_INTEGRATION_TESTS_HIJACK_DLLS_H_
