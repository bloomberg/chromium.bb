// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Graphics3D functions.

#include <limits>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/dev/pp_graphics_3d_dev.h"
#include "ppapi/c/dev/ppb_graphics_3d_dev.h"
#include "ppapi/c/dev/ppb_graphics_3d_trusted_dev.h"
#include "ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::SerializeTo;

namespace {

const int32_t kMaxAllowedBufferSize = 16777216;

/// Check that the attribute list is well formed.
bool ValidateAttribList(nacl_abi_size_t attrib_list_count,
                        int32_t* attrib_list) {
  DebugPrintf("ValidateAttribList: count = %d, ptr_null = %d\n",
      static_cast<int>(attrib_list_count), (attrib_list == NULL) ? 1 : 0);
  // Zero count lists are ok.
  if (attrib_list_count == 0)
    return true;
  // NULL lists w/ a non-zero count are not allowed.
  if (attrib_list == NULL)
    return false;
  // Must be an odd count, and the last item must be the terminator.
  return (attrib_list_count & 1) &&
         (attrib_list[attrib_list_count - 1] == PP_GRAPHICS3DATTRIB_NONE);
}

/// Check that the attribute list is well formed, then copy to output.
bool ValidateAndCopyAttribList(nacl_abi_size_t in_attrib_list_count,
                               int32_t* in_attrib_list,
                               nacl_abi_size_t* out_attrib_list_count,
                               int32_t* out_attrib_list) {

  DebugPrintf("ValidateAndCopyAttribList: in_count = %d, in_ptr_null = %d\n",
      static_cast<int>(in_attrib_list_count),
      (in_attrib_list == NULL) ? 1 : 0);
  DebugPrintf("                           out_count = %d, out_ptr_null = %d\n",
      static_cast<int>(*out_attrib_list_count),
      (out_attrib_list == NULL) ? 1 : 0);

  // Attrib lists can both be NULL w/ 0 count.
  if ((in_attrib_list == NULL) && (out_attrib_list == NULL))
    return (in_attrib_list_count == 0) && (*out_attrib_list_count == 0);
  // Don't allow only one list to be NULL.
  if ((in_attrib_list == NULL) || (out_attrib_list == NULL))
    return false;
  // Input and output lists must be the same size.
  if (in_attrib_list_count != *out_attrib_list_count)
    return false;
  // Make sure input list is well formed.
  if (!ValidateAttribList(in_attrib_list_count, in_attrib_list))
    return false;
  // Copy input list to output list.
  // Note: attrib lists can be zero sized.
  for (nacl_abi_size_t i = 0; i < in_attrib_list_count; ++i)
    out_attrib_list[i] = in_attrib_list[i];
  return true;
}

}  // namespace

//@{
/// The following methods are SRPC dispatchers for ppapi/c/ppb_graphics_3d.h.

void PpbGraphics3DRpcServer::PPB_Graphics3D_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    PP_Resource share_context,
    nacl_abi_size_t num_attrib_list, int32_t* attrib_list,
    PP_Resource* graphics3d_id) {
  DebugPrintf("PpbGraphics3DRpcServer::PPB_Graphics3D_Create(...)\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (num_attrib_list == 0)
    attrib_list = NULL;
  if (!ValidateAttribList(num_attrib_list, attrib_list))
    return;
  *graphics3d_id = ppapi_proxy::PPBGraphics3DInterface()->Create(
      instance, share_context, attrib_list);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Graphics3D_GetAttribs(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource graphics3d_id,
    nacl_abi_size_t in_attrib_list_count, int32_t* in_attrib_list,
    nacl_abi_size_t* out_attrib_list_count, int32_t* out_attrib_list,
    int32_t* pp_error) {
  DebugPrintf("PpbGraphics3DRpcServer::PPB_Graphics3D_GetAttrib(...)\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (in_attrib_list_count == 0)
    in_attrib_list = NULL;
  if (*out_attrib_list_count == 0)
    out_attrib_list = NULL;
  if (!ValidateAndCopyAttribList(in_attrib_list_count, in_attrib_list,
                                 out_attrib_list_count, out_attrib_list))
    return;
  *pp_error = ppapi_proxy::PPBGraphics3DInterface()->GetAttribs(
      graphics3d_id, out_attrib_list);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Graphics3D_SetAttribs(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource graphics3d_id,
    nacl_abi_size_t attrib_list_count, int32_t* attrib_list,
    int32_t* pp_error) {
  DebugPrintf("PpbGraphics3DRpcServer::PPB_Graphics3D_SetAttrib(...)\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (attrib_list_count == 0)
    attrib_list = NULL;
  if (!ValidateAttribList(attrib_list_count, attrib_list))
    return;
  *pp_error = ppapi_proxy::PPBGraphics3DInterface()->SetAttribs(
      graphics3d_id, attrib_list);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Graphics3D_ResizeBuffers(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource graphics3d_id,
    int32_t width,
    int32_t height,
    int32_t* pp_error) {
  DebugPrintf("PpbGraphics3DRpcServer::PPB_Graphics3D_ResizeBuffers(...)\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *pp_error = ppapi_proxy::PPBGraphics3DInterface()->ResizeBuffers(
      graphics3d_id, width, height);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Graphics3D_SwapBuffers(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource graphics3d_id,
    int32_t callback_id,
    int32_t* pp_error) {
  DebugPrintf("PpbGraphics3DRpcServer::PPB_Graphics3D_SwapBuffers(...)\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PP_CompletionCallback remote_callback =
      ppapi_proxy::MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (remote_callback.func == NULL) {
    DebugPrintf("    PPB_Graphics3D_SwapBuffers() FAILED!\n");
    return;  // Treat this as a generic SRPC error.
  }
  *pp_error = ppapi_proxy::PPBGraphics3DInterface()->SwapBuffers(
      graphics3d_id, remote_callback);
  if (*pp_error != PP_OK_COMPLETIONPENDING)
    ppapi_proxy::DeleteRemoteCallbackInfo(remote_callback);
  DebugPrintf("    PPB_Graphics3D_SwapBuffers: pp_error=%"NACL_PRId32"\n",
              *pp_error);
  rpc->result = NACL_SRPC_RESULT_OK;
}

//@}

//@{
/// The following methods are the SRPC dispatchers for
/// ppapi/c/ppb_graphics_3d_trusted.h.
void PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_CreateRaw(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    PP_Resource share_context,
    nacl_abi_size_t attrib_list_size, int32_t* attrib_list,
    PP_Resource* resource_id) {
  DebugPrintf("PPB_Graphics3DTrusted_CreateRaw: instance: %"NACL_PRIu32"\n",
              instance);
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (attrib_list_size == 0)
    attrib_list = NULL;
  if (!ValidateAttribList(attrib_list_size, attrib_list))
    return;
  *resource_id = ppapi_proxy::PPBGraphics3DTrustedInterface()->CreateRaw(
      instance, share_context, attrib_list);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_InitCommandBuffer(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    int32_t size,
    int32_t* success) {
  DebugPrintf("PPB_Graphics3DTrusted_InitCommandBuffer(...) resource_id: %d\n",
      resource_id);
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if ((size > kMaxAllowedBufferSize) || (size < 0))
    return;
  *success = ppapi_proxy::PPBGraphics3DTrustedInterface()->InitCommandBuffer(
      resource_id, size);
  rpc->result = NACL_SRPC_RESULT_OK;
}


void PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_GetRingBuffer(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    NaClSrpcImcDescType* shm_desc,
    int32_t* shm_size) {
  DebugPrintf("PPB_Graphics3DTrusted_GetRingBuffer\n");
  nacl::DescWrapperFactory factory;
  nacl::scoped_ptr<nacl::DescWrapper> desc_wrapper;
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  int native_handle = 0;
  uint32_t native_size = 0;
  ppapi_proxy::PPBGraphics3DTrustedInterface()->GetRingBuffer(
      resource_id, &native_handle, &native_size);
  desc_wrapper.reset(factory.ImportShmHandle(
      (NaClHandle)native_handle, native_size));
  *shm_desc = desc_wrapper->desc();
  *shm_size = native_size;
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_GetState(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    nacl_abi_size_t* state_size, char* state) {
  DebugPrintf("PPB_Graphics3DTrusted_GetState\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (*state_size != sizeof(PP_Graphics3DTrustedState))
    return;
  PP_Graphics3DTrustedState trusted_state =
      ppapi_proxy::PPBGraphics3DTrustedInterface()->GetState(resource_id);
  *reinterpret_cast<PP_Graphics3DTrustedState*>(state) = trusted_state;
  *state_size = sizeof(trusted_state);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_Flush(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    int32_t put_offset) {
  DebugPrintf("PPB_Graphics3DTrusted_Flush(id: %d, put_offset: %d\n",
      resource_id, put_offset);
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  ppapi_proxy::PPBGraphics3DTrustedInterface()->Flush(resource_id, put_offset);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_FlushSync(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    int32_t put_offset,
    nacl_abi_size_t* state_size, char* state) {
  DebugPrintf("PPB_Graphics3DTrusted_FlushSync\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (*state_size != sizeof(PP_Graphics3DTrustedState))
    return;
  PP_Graphics3DTrustedState trusted_state =
      ppapi_proxy::PPBGraphics3DTrustedInterface()->FlushSync(resource_id,
                                                              put_offset);
  *reinterpret_cast<PP_Graphics3DTrustedState*>(state) = trusted_state;
  *state_size = sizeof(trusted_state);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_CreateTransferBuffer(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    int32_t size,
    int32_t id_request,
    int32_t* id) {
  UNREFERENCED_PARAMETER(id_request);
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  DebugPrintf("PPB_Graphics3DTrusted_CreateTransferBuffer\n");
  if ((size > kMaxAllowedBufferSize) || (size < 0))
    return;
  *id = ppapi_proxy::PPBGraphics3DTrustedInterface()->CreateTransferBuffer(
      resource_id, size);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_DestroyTransferBuffer(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    int32_t id) {
  DebugPrintf("PPB_Graphics3DTrusted_DestroyTransferBuffer\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  ppapi_proxy::PPBGraphics3DTrustedInterface()->DestroyTransferBuffer(
      resource_id, id);
  rpc->result = NACL_SRPC_RESULT_OK;

}

void PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_GetTransferBuffer(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource_id,
    int32_t id,
    NaClSrpcImcDescType* shm_desc,
    int32_t* shm_size) {
  DebugPrintf("PPB_Graphics3DTrusted_GetTransferBuffer\n");
  nacl::DescWrapperFactory factory;
  nacl::scoped_ptr<nacl::DescWrapper> desc_wrapper;
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  int native_handle = 0;
  uint32_t native_size = 0;
  ppapi_proxy::PPBGraphics3DTrustedInterface()->
      GetTransferBuffer(resource_id, id, &native_handle, &native_size);
  desc_wrapper.reset(factory.ImportShmHandle(
      (NaClHandle)native_handle, native_size));
  *shm_desc = desc_wrapper->desc();
  *shm_size = native_size;
  rpc->result = NACL_SRPC_RESULT_OK;

}

//@}
