// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_file_ref_private.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_file_ref_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Resource file_system, const char* path) {
  EnterFunctionGivenResource<ResourceCreationAPI> enter(file_system, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateFileRef(file_system, path);
}

PP_Bool IsFileRef(PP_Resource resource) {
  EnterResource<PPB_FileRef_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_FileSystemType GetFileSystemType(PP_Resource file_ref) {
  EnterResource<PPB_FileRef_API> enter(file_ref, true);
  if (enter.failed())
    return PP_FILESYSTEMTYPE_INVALID;
  return enter.object()->GetFileSystemType();
}

PP_Var GetName(PP_Resource file_ref) {
  EnterResource<PPB_FileRef_API> enter(file_ref, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetName();
}

PP_Var GetPath(PP_Resource file_ref) {
  EnterResource<PPB_FileRef_API> enter(file_ref, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetPath();
}

PP_Resource GetParent(PP_Resource file_ref) {
  EnterResource<PPB_FileRef_API> enter(file_ref, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->GetParent();
}

int32_t MakeDirectory(PP_Resource directory_ref,
                      PP_Bool make_ancestors,
                      PP_CompletionCallback callback) {
  EnterResource<PPB_FileRef_API> enter(directory_ref, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->MakeDirectory(make_ancestors, callback);
  return MayForceCallback(callback, result);
}

int32_t Touch(PP_Resource file_ref,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              PP_CompletionCallback callback) {
  EnterResource<PPB_FileRef_API> enter(file_ref, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Touch(last_access_time, last_modified_time,
                                         callback);
  return MayForceCallback(callback, result);
}

int32_t Delete(PP_Resource file_ref,
               PP_CompletionCallback callback) {
  EnterResource<PPB_FileRef_API> enter(file_ref, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Delete(callback);
  return MayForceCallback(callback, result);
}

int32_t Rename(PP_Resource file_ref,
               PP_Resource new_file_ref,
               PP_CompletionCallback callback) {
  EnterResource<PPB_FileRef_API> enter(file_ref, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Rename(new_file_ref, callback);
  return MayForceCallback(callback, result);
}

PP_Var GetAbsolutePath(PP_Resource file_ref) {
  EnterResource<PPB_FileRef_API> enter(file_ref, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetAbsolutePath();
}

const PPB_FileRef g_ppb_file_ref_thunk = {
  &Create,
  &IsFileRef,
  &GetFileSystemType,
  &GetName,
  &GetPath,
  &GetParent,
  &MakeDirectory,
  &Touch,
  &Delete,
  &Rename
};

const PPB_FileRefPrivate g_ppb_file_ref_private_thunk = {
  &GetAbsolutePath
};

}  // namespace

const PPB_FileRef* GetPPB_FileRef_Thunk() {
  return &g_ppb_file_ref_thunk;
}

const PPB_FileRefPrivate* GetPPB_FileRefPrivate_Thunk() {
  return &g_ppb_file_ref_private_thunk;
}

}  // namespace thunk
}  // namespace ppapi
