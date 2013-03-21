// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PLUGIN_SWITCHES_H_
#define WEBKIT_PLUGINS_PLUGIN_SWITCHES_H_

#include "build/build_config.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace switches {

WEBKIT_PLUGINS_EXPORT extern const char kDebugPluginLoading[];
WEBKIT_PLUGINS_EXPORT extern const char kDisablePepper3d[];
WEBKIT_PLUGINS_EXPORT extern const char kPpapiFlashArgs[];
WEBKIT_PLUGINS_EXPORT extern const char kDisablePepperThreading[];

#if defined(OS_WIN)
WEBKIT_PLUGINS_EXPORT extern const char kUseOldWMPPlugin[];
#endif

}  // namespace switches

#endif  // WEBKIT_PLUGINS_PLUGIN_SWITCHES_H_
