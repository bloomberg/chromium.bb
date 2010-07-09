// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webkit_glue.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/plugins/plugin_list.h"

// Functions needed by webkit_glue.

namespace webkit_glue {

void GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins) {
  NPAPI::PluginList::Singleton()->GetPlugins(refresh, plugins);
}

bool IsDefaultPluginEnabled() {
  return false;
}

bool IsPluginRunningInRendererProcess() {
  return true;
}

void AppendToLog(const char*, int, const char*) {
}

bool GetApplicationDirectory(FilePath* path) {
  return PathService::Get(base::DIR_EXE, path);
}

bool GetExeDirectory(FilePath* path) {
  return GetApplicationDirectory(path);
}

bool IsProtocolSupportedForMedia(const GURL& url) {
  if (url.SchemeIsFile() ||
      url.SchemeIs("http") ||
      url.SchemeIs("https") ||
      url.SchemeIs("data"))
    return true;
  return false;
}

void CloseCurrentConnections() {
}

void SetCacheMode(bool enabled) {
}

void ClearCache() {
}

std::string GetProductVersion() {
  return std::string("DumpRenderTree/0.0.0.0");
}

bool GetPluginFinderURL(std::string* plugin_finder_url) {
  return false;
}

#if defined(OS_WIN)
bool DownloadUrl(const std::string& url, HWND caller_window) {
  return false;
}
#endif

bool IsSingleProcess() {
  return true;
}

#if defined(OS_LINUX)
int MatchFontWithFallback(const std::string& face, bool bold,
                          bool italic, int charset) {
  return -1;
}

bool GetFontTable(int fd, uint32_t table, uint8_t* output,
                  size_t* output_length) {
  return false;
}
#endif

}  // namespace webkit_glue
