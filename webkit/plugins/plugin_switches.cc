// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/plugin_switches.h"

namespace switches {

// Dumps extra logging about plugin loading to the log file.
const char kDebugPluginLoading[] = "debug-plugin-loading";

// Disable Pepper3D.
const char kDisablePepper3d[] = "disable-pepper-3d";

// "Command-line" arguments for the PPAPI Flash; used for debugging options.
const char kPpapiFlashArgs[] = "ppapi-flash-args";

// Set to true to not allow threadsafety support in native Pepper plugins.
const char kDisablePepperThreading[] = "disable-pepper-threading";

#if defined(OS_WIN)
// Used by the plugins_test when testing the older WMP plugin to force the new
// plugin to not get loaded.
extern const char kUseOldWMPPlugin[] = "use-old-wmp";
#endif

}  // namespace switches
