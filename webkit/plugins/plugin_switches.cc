// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/plugin_switches.h"

namespace switches {

// Enables the testing interface for PPAPI.
const char kEnablePepperTesting[] = "enable-pepper-testing";

// Dumps extra logging about plugin loading to the log file.
const char kDebugPluginLoading[] = "debug-plugin-loading";

// Disables NativeClient's access to Pepper3D.
const char kDisablePepper3dForUntrustedUse[] =
    "disable-pepper-3d-for-untrusted-use";

#if defined(OS_WIN)
// Used by the plugins_test when testing the older WMP plugin to force the new
// plugin to not get loaded.
extern const char kUseOldWMPPlugin[] = "use-old-wmp";
#endif

}  // namespace switches
