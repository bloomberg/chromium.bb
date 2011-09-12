// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_file_system.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Resource Create(PP_Instance instance, PP_FileSystemType type) {
  DebugPrintf("PPB_FileSystem::Create: instance=%"NACL_PRIu32" "
              "type=%"NACL_PRIu32"\n",
              instance, type);
  PP_Resource pp_resource = kInvalidResourceId;
  NaClSrpcError srpc_result =
      PpbFileSystemRpcClient::PPB_FileSystem_Create(
          GetMainSrpcChannel(),
          instance,
          type,
          &pp_resource);
  DebugPrintf("PPB_FileSystem::Create: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK)
    return pp_resource;
  return kInvalidResourceId;
}

PP_Bool IsFileSystem(PP_Resource resource) {
  DebugPrintf("PPB_FileSystem::IsFileSystem: resource=%"NACL_PRIu32"\n",
              resource);
  int32_t is_file_system = 0;
  NaClSrpcError srpc_result =
      PpbFileSystemRpcClient::PPB_FileSystem_IsFileSystem(
          GetMainSrpcChannel(),
          resource,
          &is_file_system);
  DebugPrintf("PPB_FileSystem::IsFileSystem: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (NACL_SRPC_RESULT_OK != srpc_result) {
    return PP_FALSE;
  }
  return PP_FromBool(is_file_system);
}

int32_t Open(PP_Resource file_system,
             int64_t expected_size,
             struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_FileSystem::Open: file_system=%"NACL_PRIu32"\n",
              file_system);
  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)
    return PP_ERROR_BADARGUMENT;

  int32_t pp_error;
  NaClSrpcError srpc_result =
      PpbFileSystemRpcClient::PPB_FileSystem_Open(
          GetMainSrpcChannel(),
          file_system,
          expected_size,
          callback_id,
          &pp_error);
  DebugPrintf("PPB_FileSystem::Open: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

PP_FileSystemType GetType(PP_Resource file_system) {
  DebugPrintf("PPB_FileSystem::GetType: file_system=%"NACL_PRIu32"\n",
              file_system);
  int32_t type = PP_FILESYSTEMTYPE_INVALID;
  NaClSrpcError srpc_result =
      PpbFileSystemRpcClient::PPB_FileSystem_GetType(
          GetMainSrpcChannel(),
          file_system,
          &type);
  DebugPrintf("PPB_FileSystem::GetType: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK)
    return static_cast<PP_FileSystemType>(type);
  return PP_FILESYSTEMTYPE_INVALID;
}

}  // namespace

const PPB_FileSystem* PluginFileSystem::GetInterface() {
  static const PPB_FileSystem file_system_interface = {
    Create,
    IsFileSystem,
    Open,
    GetType
  };
  return &file_system_interface;
}
}  // namespace ppapi_proxy
