// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Thes file contains stuff that should be shared among projects that do some
// special handling with default plugin

#ifndef WEBKIT_GLUE_PLUGINS_DEFAULT_PLUGIN_SHARED_H
#define WEBKIT_GLUE_PLUGINS_DEFAULT_PLUGIN_SHARED_H

namespace default_plugin {

// We use the NPNGetValue host function to send notification message to host.
// This corresponds to NPNVariable defined in npapi.h, and should be chosen so
// as to not overlap values if NPAPI is updated.

const int kMissingPluginStatusStart = 5000;

enum MissingPluginStatus {
  MISSING_PLUGIN_AVAILABLE,
  MISSING_PLUGIN_USER_STARTED_DOWNLOAD
};

#if defined(OS_WIN)
#include <windows.h>
const int kInstallMissingPluginMessage = WM_APP + 117;
#endif

}  // namespace default_plugin

#endif // WEBKIT_GLUE_PLUGINS_DEFAULT_PLUGIN_SHARED_H
