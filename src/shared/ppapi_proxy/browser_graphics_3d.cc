// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/third_party/ppapi/c/dev/ppb_context_3d_dev.h"
#include "native_client/src/third_party/ppapi/c/dev/ppb_context_3d_trusted_dev.h"
#include "native_client/src/third_party/ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;

namespace {

/// Check that the attribute list is well formed.
bool VerifyAttribList(nacl_abi_size_t attrib_list_count, int32_t* attrib_list) {
  // Attrib list must either be NULL, or must have odd size and the last item
  // must be the terminator.
  DebugPrintf("VerifyAttribList: count = %d, ptr_null = %d\n",
              (int) attrib_list_count, attrib_list == NULL ? 1 : 0);

  return  (!attrib_list_count /* && !attrib_list*/) ||
      ((attrib_list_count & 1)
      && (attrib_list[attrib_list_count - 1] == PP_GRAPHICS3DATTRIB_NONE));
}

}  // namespace

//@{
/// The following methods are the SRPC dispatchers for ppapi/c/ppb_context_3d.h.
void PpbGraphics3DRpcServer::PPB_Context3D_BindSurfaces(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource context,
    PP_Resource draw_surface,
    PP_Resource read_surface,
    int32_t* error_code) {
  DebugPrintf("PPB_Context3D_BindSurfaces, context: %"NACL_PRIu32", "
              "draw: %"NACL_PRIu32"\n", context, draw_surface);
  NaClSrpcClosureRunner runner(done);
  *error_code = ppapi_proxy::PPBContext3DInterface()->BindSurfaces(
      context, draw_surface, read_surface);
  rpc->result = NACL_SRPC_RESULT_OK;
}

//@}

//@{
/// The following methods are the SRPC dispatchers for ppapi/c/ppb_surface_3d.h.
void PpbGraphics3DRpcServer::PPB_Surface3D_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t config,
    nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
    PP_Resource* resource_id) {
  DebugPrintf("PPB_Surface3D_Create, instance: %"NACL_PRIu32"\n", instance);
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (!VerifyAttribList(attrib_list_bytes, attrib_list))
    return;
  *resource_id = ppapi_proxy::PPBSurface3DInterface()->Create(instance,
                                                              config,
                                                              attrib_list);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Surface3D_SwapBuffers(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource surface,
    int32_t callback_id,
    int32_t* pp_error) {
  DebugPrintf("PPB_Surface3D_SwapBuffers, surface: %"NACL_PRIu32"\n", surface);
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback =
      ppapi_proxy::MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (remote_callback.func == NULL)
    return;  // Treat this as a generic SRPC error.

  *pp_error = ppapi_proxy::PPBSurface3DInterface()->SwapBuffers(
      surface, remote_callback);
  if (*pp_error != PP_OK_COMPLETIONPENDING)  // Async error.
    ppapi_proxy::DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
  DebugPrintf("PPB_Surface3D_SwapBuffers: pp_error=%"NACL_PRId32"\n",
              *pp_error);
}

//@}

//@{
/// The following methods are the SRPC dispatchers for
/// ppapi/c/ppb_context_3d_trusted.h.
void PpbGraphics3DRpcServer::PPB_Context3DTrusted_CreateRaw(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t config,
    PP_Resource share_context,
    nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
    PP_Resource* resource_id) {
  DebugPrintf("PPB_Context3DTrusted_CreateRaw: instance: %"NACL_PRIu32"\n",
              instance);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (!VerifyAttribList(attrib_list_bytes, attrib_list))
    return;

  *resource_id = ppapi_proxy::PPBContext3DTrustedInterface()->CreateRaw(
      instance, config, share_context, attrib_list);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Context3DTrusted_Initialize(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    int32_t size,
    int32_t* success) {
  DebugPrintf("PPB_Context3DTrusted_Initialize\n");
  NaClSrpcClosureRunner runner(done);
  *success = ppapi_proxy::PPBContext3DTrustedInterface()->Initialize(
      resource_id, size);
  rpc->result = NACL_SRPC_RESULT_OK;
}


void PpbGraphics3DRpcServer::PPB_Context3DTrusted_GetRingBuffer(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    NaClSrpcImcDescType* shm_desc,
    int32_t* shm_size) {
  DebugPrintf("PPB_Context3DTrusted_GetRingBuffer\n");
  nacl::DescWrapperFactory factory;
  nacl::scoped_ptr<nacl::DescWrapper> desc_wrapper;
  NaClSrpcClosureRunner runner(done);

  int native_handle = 0;
  uint32_t native_size = 0;
  ppapi_proxy::PPBContext3DTrustedInterface()->GetRingBuffer(
      resource_id, &native_handle, &native_size);
  desc_wrapper.reset(factory.ImportShmHandle((NaClHandle)native_handle,
                                             native_size));
  *shm_desc = desc_wrapper->desc();
  *shm_size = native_size;
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Context3DTrusted_GetState(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    nacl_abi_size_t* state_bytes, char* state) {
  DebugPrintf("PPB_Context3DTrusted_GetState\n");
  NaClSrpcClosureRunner runner(done);
  PP_Context3DTrustedState trusted_state =
      ppapi_proxy::PPBContext3DTrustedInterface()->GetState(resource_id);
  *reinterpret_cast<PP_Context3DTrustedState*>(state) = trusted_state;
  *state_bytes = sizeof(trusted_state);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Context3DTrusted_Flush(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    int32_t put_offset) {
  DebugPrintf("PPB_Context3DTrusted_Flush\n");
  NaClSrpcClosureRunner runner(done);
  ppapi_proxy::PPBContext3DTrustedInterface()->Flush(resource_id, put_offset);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Context3DTrusted_FlushSync(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    int32_t put_offset,
    nacl_abi_size_t* state_bytes, char* state) {
  DebugPrintf("PPB_Context3DTrusted_FlushSync\n");
  NaClSrpcClosureRunner runner(done);
  PP_Context3DTrustedState trusted_state =
      ppapi_proxy::PPBContext3DTrustedInterface()->FlushSync(resource_id,
                                                             put_offset);
  *reinterpret_cast<PP_Context3DTrustedState*>(state) = trusted_state;
  *state_bytes = sizeof(trusted_state);
  rpc->result = NACL_SRPC_RESULT_OK;

}

void PpbGraphics3DRpcServer::PPB_Context3DTrusted_CreateTransferBuffer(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    int32_t size,
    int32_t id_request,
    int32_t* id) {
  UNREFERENCED_PARAMETER(id_request);

  DebugPrintf("PPB_Context3DTrusted_CreateTransferBuffer\n");
  NaClSrpcClosureRunner runner(done);
  *id = ppapi_proxy::PPBContext3DTrustedInterface()->CreateTransferBuffer(
      resource_id, size);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Context3DTrusted_DestroyTransferBuffer(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    int32_t id) {
  DebugPrintf("PPB_Context3DTrusted_DestroyTransferBuffer\n");
  NaClSrpcClosureRunner runner(done);
  ppapi_proxy::PPBContext3DTrustedInterface()->DestroyTransferBuffer(
      resource_id, id);
  rpc->result = NACL_SRPC_RESULT_OK;

}

void PpbGraphics3DRpcServer::PPB_Context3DTrusted_GetTransferBuffer(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    int32_t id,
    NaClSrpcImcDescType* shm_desc,
    int32_t* shm_size) {
  DebugPrintf("PPB_Context3DTrusted_GetTransferBuffer\n");
  nacl::DescWrapperFactory factory;
  nacl::scoped_ptr<nacl::DescWrapper> desc_wrapper;
  NaClSrpcClosureRunner runner(done);

  int native_handle = 0;
  uint32_t native_size = 0;
  ppapi_proxy::PPBContext3DTrustedInterface()->GetTransferBuffer(resource_id,
                                                                 id,
                                                                 &native_handle,
                                                                 &native_size);
  desc_wrapper.reset(factory.ImportShmHandle((NaClHandle)native_handle,
                                             native_size));
  *shm_desc = desc_wrapper->desc();
  *shm_size = native_size;
  rpc->result = NACL_SRPC_RESULT_OK;

}

//@}
