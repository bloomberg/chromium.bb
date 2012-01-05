// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_file_system_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance, PP_FileSystemType type) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateFileSystem(instance, type);
}

PP_Bool IsFileSystem(PP_Resource resource) {
  EnterResource<PPB_FileSystem_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Open(PP_Resource file_system,
             int64 expected_size,
             PP_CompletionCallback callback) {
  EnterResource<PPB_FileSystem_API> enter(file_system, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Open(expected_size, callback);
  return MayForceCallback(callback, result);
}

PP_FileSystemType GetType(PP_Resource file_system) {
  EnterResource<PPB_FileSystem_API> enter(file_system, true);
  if (enter.failed())
    return PP_FILESYSTEMTYPE_INVALID;
  return enter.object()->GetType();
}

const PPB_FileSystem g_ppb_file_system_thunk = {
  &Create,
  &IsFileSystem,
  &Open,
  &GetType
};

}  // namespace

const PPB_FileSystem_1_0* GetPPB_FileSystem_1_0_Thunk() {
  return &g_ppb_file_system_thunk;
}

}  // namespace thunk
}  // namespace ppapi
