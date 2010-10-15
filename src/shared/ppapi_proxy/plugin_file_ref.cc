/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_file_ref.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"

namespace ppapi_proxy {

namespace {
PP_Resource CreatePersistentFileRef(PP_Instance instance,
                                    const char* path) {
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(path);
  return kInvalidResourceId;
}


PP_Resource CreateTemporaryFileRef(PP_Instance instance,
                                   const char* path) {
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(path);
  return kInvalidResourceId;
}

bool IsFileRef(PP_Resource resource) {
  UNREFERENCED_PARAMETER(resource);
  return false;
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
}  // namespace

const PPB_FileRef_Dev* PluginFileRef::GetInterface() {
  static const PPB_FileRef_Dev intf = {
    CreatePersistentFileRef,
    CreateTemporaryFileRef,
    IsFileRef,
    GetFileSystemType,
    GetName,
    GetPath,
    GetParent
  };
  return &intf;
}
}  // namespace ppapi_proxy
