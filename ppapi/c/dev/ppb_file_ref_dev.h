/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPB_FILE_REF_DEV_H_
#define PPAPI_C_DEV_PPB_FILE_REF_DEV_H_

#include "ppapi/c/dev/pp_file_info_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

struct PP_CompletionCallback;

#define PPB_FILEREF_DEV_INTERFACE "PPB_FileRef(Dev);0.7"

// A FileRef is a "weak pointer" to a file in a file system.  It contains a
// PP_FileSystemType identifier and a file path string.
struct PPB_FileRef_Dev {
  // Creates a weak pointer to a file in the given filesystem. File paths are
  // POSIX style.  Returns 0 if the path is malformed.
  PP_Resource (*Create)(PP_Resource file_system, const char* path);

  // Returns PP_TRUE if the given resource is a FileRef. Returns PP_FALSE if the
  // resource is invalid or some type other than a FileRef.
  PP_Bool (*IsFileRef)(PP_Resource resource);

  // Returns the file system identifier of this file, or
  // PP_FILESYSTEMTYPE_INVALID if the file ref is invalid.
  PP_FileSystemType_Dev (*GetFileSystemType)(PP_Resource file_ref);

  // Returns the name of the file. The value returned by this function does not
  // include any path component (such as the name of the parent directory, for
  // example). It is just the name of the file. To get the full file path, use
  // the GetPath() function.
  struct PP_Var (*GetName)(PP_Resource file_ref);

  // Returns the absolute path of the file.  This method fails if the file
  // system type is PP_FileSystemType_External.
  struct PP_Var (*GetPath)(PP_Resource file_ref);

  // Returns the parent directory of this file.  If file_ref points to the root
  // of the filesystem, then the root is returned.  This method fails if the
  // file system type is PP_FileSystemType_External.
  PP_Resource (*GetParent)(PP_Resource file_ref);

  // Makes a new directory in the filesystem as well as any parent directories
  // if the make_ancestors parameter is PP_TRUE.  It is not valid to make a
  // directory in the external filesystem.  Fails if the directory already
  // exists or if ancestor directories do not exist and make_ancestors was not
  // passed as PP_TRUE.
  int32_t (*MakeDirectory)(PP_Resource directory_ref,
                           PP_Bool make_ancestors,
                           struct PP_CompletionCallback callback);

  // Updates timestamps for a file.  You must have write access to the file if
  // it exists in the external filesystem.
  int32_t (*Touch)(PP_Resource file_ref,
                   PP_Time last_access_time,
                   PP_Time last_modified_time,
                   struct PP_CompletionCallback callback);

  // Delete a file or directory.  If file_ref refers to a directory, then the
  // directory must be empty.  It is an error to delete a file or directory
  // that is in use.  It is not valid to delete a file in the external
  // filesystem.
  int32_t (*Delete)(PP_Resource file_ref,
                    struct PP_CompletionCallback callback);

  // Rename a file or directory.  file_ref and new_file_ref must both refer to
  // files in the same filesystem.  It is an error to rename a file or
  // directory that is in use.  It is not valid to rename a file in the
  // external filesystem.
  int32_t (*Rename)(PP_Resource file_ref,
                    PP_Resource new_file_ref,
                    struct PP_CompletionCallback callback);
};

#endif  /* PPAPI_C_DEV_PPB_FILE_REF_DEV_H_ */

