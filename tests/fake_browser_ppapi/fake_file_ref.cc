/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/tests/fake_browser_ppapi/fake_file_ref.h"

#include <stdio.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"

#include "native_client/tests/fake_browser_ppapi/fake_resource.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"

using ppapi_proxy::DebugPrintf;

namespace fake_browser_ppapi {

namespace {

PP_Resource Create(PP_Resource file_system_id,
                   const char* path) {
  DebugPrintf("FileRef::Create: file_system_id=%"NACL_PRId64" path=%s\n",
              file_system_id, path);
  NACL_UNIMPLEMENTED();
  return Resource::Invalid()->resource_id();
}

PP_Bool IsFileRef(PP_Resource resource_id) {
  DebugPrintf("FileRef::IsFileRef: resource_id=%"NACL_PRId64"\n", resource_id);
  NACL_UNIMPLEMENTED();
  return PP_FALSE;
}

PP_FileSystemType_Dev GetFileSystemType(PP_Resource file_ref_id) {
  DebugPrintf("FileRef::GetFileSystemType: file_ref_id=%"NACL_PRId64"\n",
              file_ref_id);
  NACL_UNIMPLEMENTED();
  return PP_FILESYSTEMTYPE_EXTERNAL;
}

PP_Var GetName(PP_Resource file_ref_id) {
  DebugPrintf("FileRef::GetName: file_ref_id=%"NACL_PRId64"\n", file_ref_id);
  NACL_UNIMPLEMENTED();
  return PP_MakeUndefined();
}

PP_Var GetPath(PP_Resource file_ref_id) {
  DebugPrintf("FileRef::GetPath: file_ref_id=%"NACL_PRId64"\n", file_ref_id);
  NACL_UNIMPLEMENTED();
  return PP_MakeUndefined();
}

PP_Resource GetParent(PP_Resource file_ref_id) {
  DebugPrintf("FileRef::GetParent: file_ref_id=%"NACL_PRId64"\n", file_ref_id);
  NACL_UNIMPLEMENTED();
  return Resource::Invalid()->resource_id();
}

int32_t MakeDirectory(PP_Resource directory_ref_id,
                      PP_Bool make_ancestors,
                      struct PP_CompletionCallback callback) {
  DebugPrintf("FileRef::MakDirectory: directory_ref_id=%"NACL_PRId64"\n",
              directory_ref_id);
  UNREFERENCED_PARAMETER(make_ancestors);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_BADRESOURCE;
}

int32_t Query(PP_Resource file_ref_id,
              struct PP_FileInfo_Dev* info,
              struct PP_CompletionCallback callback) {
  DebugPrintf("FileRef::Query: file_ref_id=%"NACL_PRId64"\n", file_ref_id);
  UNREFERENCED_PARAMETER(info);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return 0;
}

int32_t Touch(PP_Resource file_ref_id,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              struct PP_CompletionCallback callback) {
  DebugPrintf("FileRef::Touch: file_ref_id=%"NACL_PRId64"\n", file_ref_id);
  UNREFERENCED_PARAMETER(last_access_time);
  UNREFERENCED_PARAMETER(last_modified_time);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_BADRESOURCE;
}

int32_t Delete(PP_Resource file_ref_id,
               struct PP_CompletionCallback callback) {
  DebugPrintf("FileRef::Delete: file_ref_id=%"NACL_PRId64"\n", file_ref_id);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_BADRESOURCE;
}

int32_t Rename(PP_Resource file_ref_id,
               PP_Resource new_file_ref,
               struct PP_CompletionCallback callback) {
  DebugPrintf("FileRef::Rename: file_ref_id=%"NACL_PRId64"\n", file_ref_id);
  UNREFERENCED_PARAMETER(new_file_ref);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_BADRESOURCE;
}

}  // namespace


const PPB_FileRef_Dev* FileRef::GetInterface() {
  static const PPB_FileRef_Dev file_ref_interface = {
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
  return &file_ref_interface;
}

}  // namespace fake_browser_ppapi
