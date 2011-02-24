// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_FLASH_FILE_H_
#define PPAPI_C_PRIVATE_PPB_FLASH_FILE_H_

#ifdef _WIN32
#include <windows.h>
#endif

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"

#ifdef _WIN32
typedef HANDLE PP_FileHandle;
static const PP_FileHandle PP_kInvalidFileHandle = NULL;
#else
typedef int PP_FileHandle;
static const PP_FileHandle PP_kInvalidFileHandle = -1;
#endif

struct PP_CompletionCallback;
struct PP_FontDescription_Dev;
struct PP_FileInfo_Dev;

struct PP_DirEntry_Dev {
  const char* name;
  PP_Bool is_dir;
};

struct PP_DirContents_Dev {
  int32_t count;
  struct PP_DirEntry_Dev* entries;
};

// PPB_Flash_File_ModuleLocal --------------------------------------------------

#define PPB_FLASH_FILE_MODULELOCAL_INTERFACE "PPB_Flash_File_ModuleLocal;1"

struct PPB_Flash_File_ModuleLocal {
  // Opens a module-local file, returning a file descriptor (posix) or a HANDLE
  // (win32) into file. Module-local file paths (here and below) are
  // '/'-separated UTF-8 strings, relative to a module-specific root. The return
  // value is the ppapi error, PP_OK if success, one of the PP_ERROR_* in case
  // of failure.
  int32_t (*OpenFile)(PP_Instance instance,
                      const char* path,
                      int32_t mode,
                      PP_FileHandle* file);

  // Renames a module-local file. The return value is the ppapi error, PP_OK if
  // success, one of the PP_ERROR_* in case of failure.
  int32_t (*RenameFile)(PP_Instance instance,
                        const char* path_from,
                        const char* path_to);

  // Deletes a module-local file or directory. If recursive is set and the path
  // points to a directory, deletes all the contents of the directory. The
  // return value is the ppapi error, PP_OK if success, one of the PP_ERROR_* in
  // case of failure.
  int32_t (*DeleteFileOrDir)(PP_Instance instance,
                             const char* path,
                             PP_Bool recursive);

  // Creates a module-local directory. The return value is the ppapi error,
  // PP_OK if success, one of the PP_ERROR_* in case of failure.
  int32_t (*CreateDir)(PP_Instance instance, const char* path);

  // Queries information about a module-local file. The return value is the
  // ppapi error, PP_OK if success, one of the PP_ERROR_* in case of failure.
  int32_t (*QueryFile)(PP_Instance instance,
                       const char* path,
                       struct PP_FileInfo_Dev* info);

  // Gets the list of files contained in a module-local directory. The return
  // value is the ppapi error, PP_OK if success, one of the PP_ERROR_* in case
  // of failure. If non-NULL, the returned contents should be freed with
  // FreeDirContents.
  int32_t (*GetDirContents)(PP_Instance instance,
                            const char* path,
                            struct PP_DirContents_Dev** contents);

  // Frees the data allocated by GetDirContents.
  void (*FreeDirContents)(PP_Instance instance,
                          struct PP_DirContents_Dev* contents);
};

#endif  // PPAPI_C_PRIVATE_PPB_FLASH_FILE_H_
