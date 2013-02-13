// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/test/test_registrar_constants.h"

namespace win8 {
namespace test {

// The AppUserModelId value to use during default browser registration.
const char kTestAppUserModelId[] = "appid";

// Allows the caller to override the visible name given to the exe being
// registered as the default browser.
const char kTestExeName[] = "exe-name";

// Specifies the path to the executable being registered as the default
// browser.
const char kTestExePath[] = "exe-path";

// The AppUserModelId value to use during default browser registration.
const char kTestProgId[] = "progid";

// Default values for the above flags.
const wchar_t kDefaultTestAppUserModelId[] = L"Chrome.test";
const wchar_t kDefaultTestExeName[] = L"Test Runner";
const wchar_t kDefaultTestExePath[] = L"chrome.exe";
const wchar_t kDefaultTestProgId[] = L"ChromeTestProgId";

}  // namespace test
}  // namespace win8
