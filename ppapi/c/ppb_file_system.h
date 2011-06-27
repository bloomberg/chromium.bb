/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_FILE_SYSTEM_H_
#define PPAPI_C_PPB_FILE_SYSTEM_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"

struct PP_CompletionCallback;

#define PPB_FILESYSTEM_INTERFACE_0_6 "PPB_FileSystem;0.6"
#define PPB_FILESYSTEM_INTERFACE PPB_FILESYSTEM_INTERFACE_0_6

struct PPB_FileSystem {
  /** Creates a filesystem object of the given type. */
  PP_Resource (*Create)(PP_Instance instance, PP_FileSystemType type);

  /** Returns PP_TRUE if the given resource is a FileSystem. */
  PP_Bool (*IsFileSystem)(PP_Resource resource);

  /**
   * Opens the file system. A file system must be opened before running any
   * other operation on it.
   *
   * TODO(brettw) clarify whether this must have completed before a file can
   * be opened in it. Clarify what it means to be "completed."
   */
  int32_t (*Open)(PP_Resource file_system,
                  int64_t expected_size,
                  struct PP_CompletionCallback callback);

  /**
   * Returns the type of the given file system.
   *
   * Returns PP_FILESYSTEMTYPE_INVALID if the given resource is not a valid
   * filesystem. It is valid to call this function even before Open completes.
   */
  PP_FileSystemType (*GetType)(PP_Resource file_system);
};

#endif  /* PPAPI_C_PPB_FILE_SYSTEM_H_ */
