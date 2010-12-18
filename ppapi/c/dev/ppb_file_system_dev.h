// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPB_FILE_SYSTEM_DEV_H_
#define PPAPI_C_DEV_PPB_FILE_SYSTEM_DEV_H_

#include "ppapi/c/dev/pp_file_info_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"

struct PP_CompletionCallback;

#define PPB_FILESYSTEM_DEV_INTERFACE "PPB_FileSystem(Dev);0.2"

struct PPB_FileSystem_Dev {
  // Creates a weak pointer to the filesystem of the given type.
  PP_Resource (*Create)(PP_Instance instance, PP_FileSystemType_Dev type);

  // Opens the file system. A file system must be opened before running any
  // other operation on it.
  int32_t (*Open)(PP_Resource file_system,
                  int64_t expected_size,
                  struct PP_CompletionCallback callback);
};

#endif  // PPAPI_C_DEV_PPB_FILE_SYSTEM_DEV_H_
