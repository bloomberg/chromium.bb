// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/test_app/constants.h"

namespace updater {

const char kInstallUpdaterSwitch[] = "install-updater";
const char kRegisterToUpdaterSwitch[] = "ipc-register";
const char kForegroundUpdateSwitch[] = "ipc-update";

#if defined(OS_WIN)
const base::char16 kAppId[] = L"6f0f9a34-a0ab-4a75-a0eb-6eab78d0dc4b";
#endif  // defined(OS_WIN)

}  // namespace updater
