// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_flash_file_impl.h"

#include <string.h>

#include <string>

#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "ppapi/c/dev/pp_file_info_dev.h"
#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/c/private/ppb_flash_file.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/error_util.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

namespace webkit {
namespace ppapi {

namespace {

FilePath GetFilePathFromUTF8(const char* path) {
#if defined(OS_WIN)
  return FilePath(UTF8ToUTF16(path));
#else
  return FilePath(path);
#endif
}

int32_t OpenModuleLocalFile(PP_Instance pp_instance,
                            const char* path,
                            int32_t mode,
                            PP_FileHandle* file) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_ERROR_FAILED;

  int flags = 0;
  if (mode & PP_FILEOPENFLAG_READ)
    flags |= base::PLATFORM_FILE_READ;
  if (mode & PP_FILEOPENFLAG_WRITE) {
    flags |= base::PLATFORM_FILE_WRITE;
    flags |= base::PLATFORM_FILE_WRITE_ATTRIBUTES;
  }
  if (mode & PP_FILEOPENFLAG_TRUNCATE) {
    DCHECK(mode & PP_FILEOPENFLAG_WRITE);
    flags |= base::PLATFORM_FILE_TRUNCATE;
  }

  if (mode & PP_FILEOPENFLAG_CREATE) {
    if (mode & PP_FILEOPENFLAG_EXCLUSIVE)
      flags |= base::PLATFORM_FILE_CREATE;
    else
      flags |= base::PLATFORM_FILE_OPEN_ALWAYS;
  } else {
    flags |= base::PLATFORM_FILE_OPEN;
  }

  base::PlatformFile base_file;
  base::PlatformFileError result = instance->delegate()->OpenModuleLocalFile(
      instance->module()->name(),
      GetFilePathFromUTF8(path),
      flags,
      &base_file);
  *file = base_file;
  return PlatformFileErrorToPepperError(result);
}


int32_t RenameModuleLocalFile(PP_Instance pp_instance,
                              const char* path_from,
                              const char* path_to) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_ERROR_FAILED;

  base::PlatformFileError result = instance->delegate()->RenameModuleLocalFile(
      instance->module()->name(),
      GetFilePathFromUTF8(path_from),
      GetFilePathFromUTF8(path_to));
  return PlatformFileErrorToPepperError(result);
}

int32_t DeleteModuleLocalFileOrDir(PP_Instance pp_instance,
                                   const char* path,
                                   PP_Bool recursive) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_ERROR_FAILED;

  base::PlatformFileError result =
      instance->delegate()->DeleteModuleLocalFileOrDir(
          instance->module()->name(), GetFilePathFromUTF8(path),
          PPBoolToBool(recursive));
  return PlatformFileErrorToPepperError(result);
}

int32_t CreateModuleLocalDir(PP_Instance pp_instance, const char* path) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_ERROR_FAILED;

  base::PlatformFileError result = instance->delegate()->CreateModuleLocalDir(
      instance->module()->name(), GetFilePathFromUTF8(path));
  return PlatformFileErrorToPepperError(result);
}

int32_t QueryModuleLocalFile(PP_Instance pp_instance,
                             const char* path,
                             PP_FileInfo_Dev* info) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_ERROR_FAILED;

  base::PlatformFileInfo file_info;
  base::PlatformFileError result = instance->delegate()->QueryModuleLocalFile(
      instance->module()->name(), GetFilePathFromUTF8(path), &file_info);
  if (result == base::PLATFORM_FILE_OK) {
    info->size = file_info.size;
    info->creation_time = file_info.creation_time.ToDoubleT();
    info->last_access_time = file_info.last_accessed.ToDoubleT();
    info->last_modified_time = file_info.last_modified.ToDoubleT();
    info->system_type = PP_FILESYSTEMTYPE_EXTERNAL;
    if (file_info.is_directory)
      info->type = PP_FILETYPE_DIRECTORY;
    else
      info->type = PP_FILETYPE_REGULAR;
  }
  return PlatformFileErrorToPepperError(result);
}

int32_t GetModuleLocalDirContents(PP_Instance pp_instance,
                                  const char* path,
                                  PP_DirContents_Dev** contents) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_ERROR_FAILED;

  *contents = NULL;
  DirContents pepper_contents;
  base::PlatformFileError result =
      instance->delegate()->GetModuleLocalDirContents(
          instance->module()->name(),
          GetFilePathFromUTF8(path),
          &pepper_contents);

  if (result != base::PLATFORM_FILE_OK)
    return PlatformFileErrorToPepperError(result);

  *contents = new PP_DirContents_Dev;
  size_t count = pepper_contents.size();
  (*contents)->count = count;
  (*contents)->entries = new PP_DirEntry_Dev[count];
  for (size_t i = 0; i < count; ++i) {
    PP_DirEntry_Dev& entry = (*contents)->entries[i];
#if defined(OS_WIN)
    const std::string& name = UTF16ToUTF8(pepper_contents[i].name.value());
#else
    const std::string& name = pepper_contents[i].name.value();
#endif
    size_t size = name.size() + 1;
    char* name_copy = new char[size];
    memcpy(name_copy, name.c_str(), size);
    entry.name = name_copy;
    entry.is_dir = BoolToPPBool(pepper_contents[i].is_dir);
  }
  return PP_OK;
}

void FreeModuleLocalDirContents(PP_Instance instance,
                                PP_DirContents_Dev* contents) {
  DCHECK(contents);
  for (int32_t i = 0; i < contents->count; ++i) {
    delete [] contents->entries[i].name;
  }
  delete [] contents->entries;
  delete contents;
}

const PPB_Flash_File_ModuleLocal ppb_flash_file_module_local = {
  &OpenModuleLocalFile,
  &RenameModuleLocalFile,
  &DeleteModuleLocalFileOrDir,
  &CreateModuleLocalDir,
  &QueryModuleLocalFile,
  &GetModuleLocalDirContents,
  &FreeModuleLocalDirContents,
};

}  // namespace

// static
const PPB_Flash_File_ModuleLocal*
    PPB_Flash_File_ModuleLocal_Impl::GetInterface() {
  return &ppb_flash_file_module_local;
}

}  // namespace ppapi
}  // namespace webkit
