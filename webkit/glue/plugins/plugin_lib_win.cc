// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/plugin_lib.h"

#include "base/file_version_info.h"
#include "base/file_version_info_win.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_list.h"

namespace NPAPI {

bool PluginLib::ReadWebPluginInfo(const FilePath &filename,
                                  WebPluginInfo* info) {
  // On windows, the way we get the mime types for the library is
  // to check the version information in the DLL itself.  This
  // will be a string of the format:  <type1>|<type2>|<type3>|...
  // For example:
  //     video/quicktime|audio/aiff|image/jpeg
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(filename.value()));
  if (!version_info.get()) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Could not get version info for plugin "
        << filename.value();
    return false;
  }

  FileVersionInfoWin* version_info_win =
      static_cast<FileVersionInfoWin*>(version_info.get());
  PluginVersionInfo pvi;
  pvi.mime_types = version_info_win->GetStringValue(L"MIMEType");
  pvi.file_extensions = version_info_win->GetStringValue(L"FileExtents");
  pvi.type_descriptions = version_info_win->GetStringValue(L"FileOpenName");
  pvi.product_name = version_info->product_name();
  pvi.file_description = version_info->file_description();
  pvi.file_version = version_info->file_version();
  pvi.path = filename;

  return PluginList::CreateWebPluginInfo(pvi, info);
}

}  // namespace NPAPI
