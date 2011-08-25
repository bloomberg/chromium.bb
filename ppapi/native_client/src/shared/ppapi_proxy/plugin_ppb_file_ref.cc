// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_file_ref.h"

#include <string.h>
#include <cmath>
#include <string>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {
bool IsDirectoryTraversal(const std::string& path) {
  bool is_directory_traversal = true;
  // TODO(sanga): Do we need to guard against percent encoded strings as well?
  if (path.find("../") == std::string::npos)
    is_directory_traversal = false;
  return is_directory_traversal;
}

PP_Resource Create(PP_Resource file_system,
                   const char* path) {
  DebugPrintf("PPB_FileRef::Create: file_system=%"NACL_PRIu32", path=%s\n",
              file_system, path);

  PP_Resource resource = kInvalidResourceId;

  if (IsDirectoryTraversal(path))
    return kInvalidResourceId;

  NaClSrpcError srpc_result = PpbFileRefRpcClient::PPB_FileRef_Create(
      GetMainSrpcChannel(),
      file_system,
      strlen(path) + 1,  // Add 1 to preserve terminating '\0'
      const_cast<char*>(path),
      &resource);
  DebugPrintf("PPB_FileRef::Create: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return resource;
  return kInvalidResourceId;
}

PP_Bool IsFileRef(PP_Resource file_ref) {
  DebugPrintf("PPB_FileRef::IsFileRef: file_ref=%"NACL_PRIu32"\n", file_ref);

  int32_t is_file_ref = 0;
  NaClSrpcError srpc_result = PpbFileRefRpcClient::PPB_FileRef_IsFileRef(
      GetMainSrpcChannel(),
      file_ref,
      &is_file_ref);
  DebugPrintf("PPB_FileRef::IsFileRef: %s\n", NaClSrpcErrorString(srpc_result));

  if ((srpc_result == NACL_SRPC_RESULT_OK) && is_file_ref)
    return PP_TRUE;
  return PP_FALSE;
}

PP_FileSystemType GetFileSystemType(PP_Resource file_ref) {
  DebugPrintf("PPB_FileRef::GetFileSystemType: file_ref=%"NACL_PRIu32"\n",
              file_ref);

  int32_t file_system_type = PP_FILESYSTEMTYPE_INVALID;
  NaClSrpcError srpc_result =
      PpbFileRefRpcClient::PPB_FileRef_GetFileSystemType(
          GetMainSrpcChannel(),
          file_ref,
          &file_system_type);
  DebugPrintf("PPB_FileRef::GetFileSystemType: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return static_cast<PP_FileSystemType>(file_system_type);
  return PP_FILESYSTEMTYPE_INVALID;
}

PP_Var GetName(PP_Resource file_ref) {
  DebugPrintf("PPB_FileRef::GetName: file_ref=%"NACL_PRIu32"\n", file_ref);

  PP_Var name = PP_MakeUndefined();
  nacl_abi_size_t length = kMaxVarSize;
  nacl::scoped_array<char> name_bytes(new char[length]);
  NaClSrpcError srpc_result = PpbFileRefRpcClient::PPB_FileRef_GetName(
      GetMainSrpcChannel(),
      file_ref,
      &length,
      name_bytes.get());
  DebugPrintf("PPB_FileRef::GetName: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK) {
    if (DeserializeTo(GetMainSrpcChannel(), name_bytes.get(), length,
                      1,  // argc
                      &name))
      return name;
  }
  return PP_MakeUndefined();
}

PP_Var GetPath(PP_Resource file_ref) {
  DebugPrintf("PPB_FileRef::GetPath: file_ref=%"NACL_PRIu32"\n", file_ref);

  PP_Var path = PP_MakeUndefined();
  nacl_abi_size_t length = kMaxVarSize;
  nacl::scoped_array<char> path_bytes(new char[length]);
  NaClSrpcError srpc_result = PpbFileRefRpcClient::PPB_FileRef_GetPath(
      GetMainSrpcChannel(),
      file_ref,
      &length,
      path_bytes.get());
  DebugPrintf("PPB_FileRef::GetPath: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK &&
    DeserializeTo(GetMainSrpcChannel(), path_bytes.get(), length,
                  1,  // argc
                  &path)) {
      return path;
  }
  return PP_MakeUndefined();
}

PP_Resource GetParent(PP_Resource file_ref) {
  DebugPrintf("PPB_FileRef::GetParent: file_ref=%"NACL_PRIu32"\n", file_ref);

  PP_Resource parent = kInvalidResourceId;
  NaClSrpcError srpc_result = PpbFileRefRpcClient::PPB_FileRef_GetParent(
      GetMainSrpcChannel(),
      file_ref,
      &parent);
  DebugPrintf("PPB_FileRef::GetParent: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return parent;
  return kInvalidResourceId;
}

int32_t MakeDirectory(PP_Resource directory_ref,
                      PP_Bool make_ancestors,
                      struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_FileRef::MakeDirectory: directory_ref=%"NACL_PRIu32", "
              "make_ancestors=%s\n", directory_ref,
              (make_ancestors ? "true" : "false"));

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result = PpbFileRefRpcClient::PPB_FileRef_MakeDirectory(
      GetMainSrpcChannel(),
      directory_ref,
      make_ancestors,
      callback_id,
      &pp_error);
  DebugPrintf("PPB_FileRef::MakeDirectory: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return MayForceCallback(callback, pp_error);
  return PP_ERROR_FAILED;
}

int32_t Touch(PP_Resource file_ref,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_FileRef::Touch: file_ref=%"NACL_PRIu32", "
              "last_access_time=%lf, last_modified_time=%lf\n",
              file_ref, last_access_time, last_modified_time);

  if (std::isnan(last_access_time) ||
      std::isnan(last_modified_time)) {
    return PP_ERROR_FAILED;
  }

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result = PpbFileRefRpcClient::PPB_FileRef_Touch(
      GetMainSrpcChannel(),
      file_ref,
      last_access_time,
      last_modified_time,
      callback_id,
      &pp_error);
  DebugPrintf("PPB_FileRef::Touch: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return MayForceCallback(callback, pp_error);
  return PP_ERROR_FAILED;
}

int32_t Delete(PP_Resource file_ref,
               struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_FileRef::Delete: file_ref=%"NACL_PRIu32"\n", file_ref);

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result = PpbFileRefRpcClient::PPB_FileRef_Delete(
      GetMainSrpcChannel(),
      file_ref,
      callback_id,
      &pp_error);
  DebugPrintf("PPB_FileRef::Delete: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return MayForceCallback(callback, pp_error);
  return PP_ERROR_FAILED;
}

int32_t Rename(PP_Resource file_ref,
               PP_Resource new_file_ref,
               struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_FileRef::Rename: file_ref=%"NACL_PRIu32", "
              "new_file_ref=%"NACL_PRIu32"\n", file_ref, new_file_ref);

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result = PpbFileRefRpcClient::PPB_FileRef_Rename(
      GetMainSrpcChannel(),
      file_ref,
      new_file_ref,
      callback_id,
      &pp_error);
  DebugPrintf("PPB_FileRef::Rename: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return MayForceCallback(callback, pp_error);
  return PP_ERROR_FAILED;
}
}  // namespace

const PPB_FileRef* PluginFileRef::GetInterface() {
  static const PPB_FileRef file_ref_interface = {
    Create,
    IsFileRef,
    GetFileSystemType,
    GetName,
    GetPath,
    GetParent,
    MakeDirectory,
    Touch,
    Delete,
    Rename
  };
  return &file_ref_interface;
}
}  // namespace ppapi_proxy
