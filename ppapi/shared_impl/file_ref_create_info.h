// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_FILE_REF_CREATE_INFO_H
#define PPAPI_SHARED_IMPL_FILE_REF_CREATE_INFO_H

#include <string>

#include "base/files/file_path.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_resource.h"

namespace ppapi {

// FileRefs are created in a number of places and they include a number of
// return values. This struct encapsulates everything in one place.
struct FileRef_CreateInfo {
  PP_FileSystemType file_system_type;
  std::string internal_path;
  std::string display_name;

  // Used when a FileRef is created in the Renderer.
  int pending_host_resource_id;

  // Since FileRef needs to hold a FileSystem reference, we need to pass the
  // resource in this CreateInfo.
  PP_Resource file_system_plugin_resource;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_FILE_REF_CREATE_INFO_H
