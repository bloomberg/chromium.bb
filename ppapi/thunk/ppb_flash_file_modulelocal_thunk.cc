// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_file.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_flash_api.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

bool CreateThreadAdapterForInstance(PP_Instance instance) {
  EnterInstance enter(instance);
  if (enter.failed())
    return false;
  return enter.functions()->GetFlashAPI()->CreateThreadAdapterForInstance(
      instance);
}

void ClearThreadAdapterForInstance(PP_Instance instance) {
  EnterInstance enter(instance);
  if (enter.succeeded()) {
    return enter.functions()->GetFlashAPI()->ClearThreadAdapterForInstance(
        instance);
  }
}

int32_t OpenFile(PP_Instance instance,
                 const char* path,
                 int32_t mode,
                 PP_FileHandle* file) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_ERROR_BADARGUMENT;
  return enter.functions()->GetFlashAPI()->OpenFile(instance, path, mode, file);
}

int32_t RenameFile(PP_Instance instance,
                   const char* path_from,
                   const char* path_to) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_ERROR_BADARGUMENT;
  return enter.functions()->GetFlashAPI()->RenameFile(instance,
                                                      path_from, path_to);
}

int32_t DeleteFileOrDir(PP_Instance instance,
                        const char* path,
                        PP_Bool recursive) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_ERROR_BADARGUMENT;
  return enter.functions()->GetFlashAPI()->DeleteFileOrDir(instance, path,
                                                           recursive);
}

int32_t CreateDir(PP_Instance instance, const char* path) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_ERROR_BADARGUMENT;
  return enter.functions()->GetFlashAPI()->CreateDir(instance, path);
}

int32_t QueryFile(PP_Instance instance, const char* path, PP_FileInfo* info) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_ERROR_BADARGUMENT;
  return enter.functions()->GetFlashAPI()->QueryFile(instance, path, info);
}

int32_t GetDirContents(PP_Instance instance,
                       const char* path,
                       PP_DirContents_Dev** contents) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_ERROR_BADARGUMENT;
  return enter.functions()->GetFlashAPI()->GetDirContents(instance, path,
                                                          contents);
}

void FreeDirContents(PP_Instance instance,
                     PP_DirContents_Dev* contents) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->GetFlashAPI()->FreeDirContents(instance, contents);
}

int32_t CreateTemporaryFile(PP_Instance instance, PP_FileHandle* file) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_ERROR_BADARGUMENT;

  *file = PP_kInvalidFileHandle;
  return enter.functions()->GetFlashAPI()->CreateTemporaryFile(instance, file);
}

const PPB_Flash_File_ModuleLocal_3_0 g_ppb_flash_file_modulelocal_thunk_3_0 = {
  &CreateThreadAdapterForInstance,
  &ClearThreadAdapterForInstance,
  &OpenFile,
  &RenameFile,
  &DeleteFileOrDir,
  &CreateDir,
  &QueryFile,
  &GetDirContents,
  &FreeDirContents,
  &CreateTemporaryFile
};

}  // namespace

const PPB_Flash_File_ModuleLocal_3_0*
    GetPPB_Flash_File_ModuleLocal_3_0_Thunk() {
  return &g_ppb_flash_file_modulelocal_thunk_3_0;
}

}  // namespace thunk
}  // namespace ppapi
