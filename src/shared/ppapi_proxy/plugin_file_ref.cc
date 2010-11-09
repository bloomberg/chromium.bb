/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_file_ref.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "ppapi/c/pp_completion_callback.h"

namespace ppapi_proxy {

namespace {
PP_Resource Create(PP_Resource file_system,
                   const char* path) {
  UNREFERENCED_PARAMETER(file_system);
  UNREFERENCED_PARAMETER(path);
  return kInvalidResourceId;
}

PP_Bool IsFileRef(PP_Resource resource) {
  UNREFERENCED_PARAMETER(resource);
  return PP_FALSE;
}

PP_FileSystemType_Dev GetFileSystemType(PP_Resource file_ref) {
  UNREFERENCED_PARAMETER(file_ref);
  return PP_FILESYSTEMTYPE_EXTERNAL;
}

PP_Var GetName(PP_Resource file_ref) {
  UNREFERENCED_PARAMETER(file_ref);
  return PP_MakeUndefined();
}

PP_Var GetPath(PP_Resource file_ref) {
  UNREFERENCED_PARAMETER(file_ref);
  return PP_MakeUndefined();
}

PP_Resource GetParent(PP_Resource file_ref) {
  UNREFERENCED_PARAMETER(file_ref);
  return kInvalidResourceId;
}

int32_t MakeDirectory(PP_Resource directory_ref,
                      PP_Bool make_ancestors,
                      struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(directory_ref);
  UNREFERENCED_PARAMETER(make_ancestors);
  UNREFERENCED_PARAMETER(callback);
  return 0;
}

int32_t Query(PP_Resource file_ref,
              struct PP_FileInfo_Dev* info,
              struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_ref);
  UNREFERENCED_PARAMETER(info);
  UNREFERENCED_PARAMETER(callback);
  return 0;
}

int32_t Touch(PP_Resource file_ref,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_ref);
  UNREFERENCED_PARAMETER(last_access_time);
  UNREFERENCED_PARAMETER(last_modified_time);
  UNREFERENCED_PARAMETER(callback);
  return 0;
}

int32_t Delete(PP_Resource file_ref,
               struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_ref);
  UNREFERENCED_PARAMETER(callback);
  return 0;
}

int32_t Rename(PP_Resource file_ref,
               PP_Resource new_file_ref,
               struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_ref);
  UNREFERENCED_PARAMETER(new_file_ref);
  UNREFERENCED_PARAMETER(callback);
  return 0;
}
}  // namespace

const PPB_FileRef_Dev* PluginFileRef::GetInterface() {
  static const PPB_FileRef_Dev intf = {
    Create,
    IsFileRef,
    GetFileSystemType,
    GetName,
    GetPath,
    GetParent,
    MakeDirectory,
    Query,
    Touch,
    Delete,
    Rename
  };
  return &intf;
}
}  // namespace ppapi_proxy
