// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_TEST_METRO_REGISTRATION_HELPER_H_
#define WIN8_TEST_METRO_REGISTRATION_HELPER_H_

#include "base/strings/string16.h"

namespace win8 {

// Synchronously makes chrome.exe THE default Win8 browser after which it is
// activatable into a Metro viewer process using
// win8::test::kDefaultTestAppUserModelId. Intended to be used by a test binary
// in the output directory and assumes the presence of test_registrar.exe,
// chrome.exe (the viewer process), and all needed DLLs in the same directory
// as the calling module.
// NOTE: COM must be initialized prior to calling this method.
bool MakeTestDefaultBrowserSynchronously();

}  // namespace win8

#endif  // WIN8_TEST_METRO_REGISTRATION_HELPER_H_
