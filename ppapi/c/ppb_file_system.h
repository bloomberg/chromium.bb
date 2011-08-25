/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_file_system.idl modified Wed Aug 24 20:52:19 2011. */

#ifndef PPAPI_C_PPB_FILE_SYSTEM_H_
#define PPAPI_C_PPB_FILE_SYSTEM_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_FILESYSTEM_INTERFACE_1_0 "PPB_FileSystem;1.0"
#define PPB_FILESYSTEM_INTERFACE PPB_FILESYSTEM_INTERFACE_1_0

/**
 * @file
 * This file defines the API to create a file system associated with a file.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_FileSystem</code> struct identifies the file system type
 * associated with a file.
 */
struct PPB_FileSystem {
  /** Create() creates a file system object of the given type.
   *
   * @param[in] instance A <code>PP_Instance</code> indentifying the instance
   * with the file.
   * @param[in] type A file system type as defined by
   * <code>PP_FileSystemType</code> enum.
   *
   * @return A <code>PP_Resource</code> corresponding to a file system if
   * successful.
   */
  PP_Resource (*Create)(PP_Instance instance, PP_FileSystemType type);
  /**
   * IsFileSystem() determines if the provided resource is a file system.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a file
   * system.
   *
   * @return <code>PP_TRUE</code> if the resource is a
   * <code>PPB_FileSystem</code>, <code>PP_FALSE</code> if the resource is
   * invalid or some type other than <code>PPB_FileSystem</code>.
   */
  PP_Bool (*IsFileSystem)(PP_Resource resource);
  /**
   * Open() opens the file system. A file system must be opened before running
   * any other operation on it.
   *
   * @param[in] file_system A <code>PP_Resource</code> corresponding to a file
   * system.
   * @param[in] expected_size The expected size of the file system.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of Open().
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   *
   * TODO(brettw) clarify whether this must have completed before a file can
   * be opened in it. Clarify what it means to be "completed."
   */
  int32_t (*Open)(PP_Resource file_system,
                  int64_t expected_size,
                  struct PP_CompletionCallback callback);
  /**
   * GetType() returns the type of the provided file system.
   *
   * @param[in] file_system A <code>PP_Resource</code> corresponding to a file
   * system.
   *
   * @return A <code>PP_FileSystemType</code> with the file system type if
   * valid or <code>PP_FILESYSTEMTYPE_INVALID</code> if the provided resource
   * is not a valid file system. It is valid to call this function even before
   * Open() completes.
   */
  PP_FileSystemType (*GetType)(PP_Resource file_system);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_FILE_SYSTEM_H_ */

