// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_FILE_REF_DETAILED_INFO_H
#define PPAPI_SHARED_IMPL_FILE_REF_DETAILED_INFO_H

#include <string>

#include "base/files/file_path.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_resource.h"

namespace ppapi {

// This structure is only used for exchanging information about FileRefs from
// the browser to the renderer.
struct FileRefDetailedInfo {
  // This struct doesn't hold a ref on this resource.
  PP_Resource resource;
  PP_FileSystemType file_system_type;
  std::string file_system_url_spec;
  base::FilePath external_path;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_FILE_REF_DETAILED_INFO_H
