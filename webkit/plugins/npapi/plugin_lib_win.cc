// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_lib.h"

#include "base/file_version_info.h"
#include "base/file_version_info_win.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "webkit/plugins/npapi/plugin_constants_win.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace webkit {
namespace npapi {

bool PluginLib::ReadWebPluginInfo(const FilePath &filename,
                                  WebPluginInfo* info) {
  // On windows, the way we get the mime types for the library is
  // to check the version information in the DLL itself.  This
  // will be a string of the format:  <type1>|<type2>|<type3>|...
  // For example:
  //     video/quicktime|audio/aiff|image/jpeg
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(filename));
  if (!version_info.get()) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Could not get version info for plugin "
        << filename.value();
    return false;
  }

  FileVersionInfoWin* version_info_win =
      static_cast<FileVersionInfoWin*>(version_info.get());

  info->name = version_info->product_name();
  info->desc = version_info->file_description();
  info->version = version_info->file_version();
  info->path = filename;
  info->enabled = WebPluginInfo::USER_ENABLED_POLICY_UNMANAGED;

  // TODO(evan): Move the ParseMimeTypes code inline once Pepper is updated.
  if (!PluginList::ParseMimeTypes(
          UTF16ToASCII(version_info_win->GetStringValue(L"MIMEType")),
          UTF16ToASCII(version_info_win->GetStringValue(L"FileExtents")),
          version_info_win->GetStringValue(L"FileOpenName"),
          &info->mime_types)) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Plugin " << info->name << " has bad MIME types, skipping";
    return false;
  }

  return true;
}

}  // namespace npapi
}  // namespace webkit
