// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PLUGIN_SWITCHES_H_
#define WEBKIT_GLUE_PLUGINS_PLUGIN_SWITCHES_H_

#include "build/build_config.h"

namespace switches {

extern const char kEnablePepperTesting[];

#if defined(OS_POSIX)
extern const char kDebugPluginLoading[];
#endif

}  // namespace switches

#endif  // WEBKIT_GLUE_PLUGINS_PLUGIN_SWITCHES_H_
