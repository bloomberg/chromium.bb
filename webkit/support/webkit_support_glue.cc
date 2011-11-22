// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webkit_glue.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "webkit/glue/user_agent.h"

// Functions needed by webkit_glue.

namespace webkit_glue {

void AppendToLog(const char*, int, const char*) {
}

bool GetPluginFinderURL(std::string* plugin_finder_url) {
  return false;
}

#if defined(OS_WIN)
bool DownloadUrl(const std::string& url, HWND caller_window) {
  return false;
}
#endif

void EnableSpdy(bool enable) {
}

void UserMetricsRecordAction(const std::string& action) {
}

}  // namespace webkit_glue
