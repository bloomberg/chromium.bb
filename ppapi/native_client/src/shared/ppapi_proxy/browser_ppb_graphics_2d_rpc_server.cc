// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Graphics2D functions.

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPBGraphics2DInterface;

namespace {

// Two functions below use a NULL PP_Rect pointer to indicate that the
// entire image should be updated.
const struct PP_Rect* kEntireImage = NULL;

}  // namespace

void PpbGraphics2DRpcServer::PPB_Graphics2D_Create(NaClSrpcRpc* rpc,
                                                   NaClSrpcClosure* done,
                                                   PP_Instance instance,
                                                   nacl_abi_size_t size_bytes,
                                                   char* size,
                                                   int32_t is_always_opaque,
                                                   PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *resource = ppapi_proxy::kInvalidResourceId;
  if (size_bytes != sizeof(struct PP_Size)) {
    return;
  }
  *resource = PPBGraphics2DInterface()->Create(
      instance,
      const_cast<const struct PP_Size*>(
          reinterpret_cast<struct PP_Size*>(size)),
      (is_always_opaque ? PP_TRUE : PP_FALSE));
  DebugPrintf("PPB_Graphics2D::Create: resource=%"NACL_PRIu32"\n", *resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics2DRpcServer::PPB_Graphics2D_IsGraphics2D(NaClSrpcRpc* rpc,
                                                         NaClSrpcClosure* done,
                                                         PP_Resource resource,
                                                         int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *success = 0;
  PP_Bool pp_success = PPBGraphics2DInterface()->IsGraphics2D(resource);
  *success = (pp_success == PP_TRUE);
  DebugPrintf("PPB_Graphics2D::IsGraphics2D: pp_success=%d\n", pp_success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics2DRpcServer::PPB_Graphics2D_Describe(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource graphics_2d,
    nacl_abi_size_t* size_bytes,
    char* size,
    int32_t* is_always_opaque,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *success = 0;
  if (*size_bytes != sizeof(struct PP_Size)) {
    return;
  }
  PP_Bool is_opaque;
  PP_Bool pp_success = PPBGraphics2DInterface()->Describe(
      graphics_2d, reinterpret_cast<struct PP_Size*>(size), &is_opaque);
  *is_always_opaque = (is_opaque == PP_TRUE);
  *success = (pp_success == PP_TRUE);
  DebugPrintf("PPB_Graphics2D::Describe: pp_success=%d\n", pp_success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics2DRpcServer::PPB_Graphics2D_PaintImageData(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource graphics_2d,
    PP_Resource image,
    nacl_abi_size_t top_left_bytes,
    char* top_left,
    nacl_abi_size_t src_rect_bytes,
    char* src_rect) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (top_left_bytes != sizeof(struct PP_Point)) {  // NULL top_left is invalid.
    return;
  }
  const struct PP_Rect* rect = kEntireImage;
  if (src_rect_bytes == sizeof(struct PP_Rect)) {
    rect = const_cast<const struct PP_Rect*>(
        reinterpret_cast<struct PP_Rect*>(src_rect));
  } else if (src_rect_bytes != 0) {
    return;
  }
  PPBGraphics2DInterface()->PaintImageData(
      graphics_2d,
      image,
      const_cast<const struct PP_Point*>(
          reinterpret_cast<struct PP_Point*>(top_left)),
      rect);
  DebugPrintf("PPB_Graphics2D::PaintImageData\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics2DRpcServer::PPB_Graphics2D_Scroll(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource graphics_2d,
    nacl_abi_size_t clip_rect_bytes,
    char* clip_rect,
    nacl_abi_size_t amount_bytes,
    char* amount) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (amount_bytes != sizeof(struct PP_Point)) {  // NULL amount is invalid.
    return;
  }
  const struct PP_Rect* rect = kEntireImage;
  if (clip_rect_bytes == sizeof(struct PP_Rect)) {
    rect = const_cast<const struct PP_Rect*>(
        reinterpret_cast<struct PP_Rect*>(clip_rect));
  } else if (clip_rect_bytes != 0) {
    return;
  }
  PPBGraphics2DInterface()->Scroll(
      graphics_2d,
      rect,
      const_cast<const struct PP_Point*>(
          reinterpret_cast<struct PP_Point*>(amount)));
  DebugPrintf("PPB_Graphics2D::Scroll\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics2DRpcServer::PPB_Graphics2D_ReplaceContents(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource graphics_2d,
    PP_Resource image) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PPBGraphics2DInterface()->ReplaceContents(graphics_2d, image);
  DebugPrintf("PPB_Graphics2D::ReplaceContents\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics2DRpcServer::PPB_Graphics2D_Flush(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource graphics_2d,
    int32_t callback_id,
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback =
      ppapi_proxy::MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (remote_callback.func == NULL)
    return;  // Treat this as a generic SRPC error.

  *pp_error = PPBGraphics2DInterface()->Flush(graphics_2d, remote_callback);
  if (*pp_error != PP_OK_COMPLETIONPENDING)  // Async error.
    ppapi_proxy::DeleteRemoteCallbackInfo(remote_callback);
  DebugPrintf("PPB_Graphics2D::Flush: pp_error=%"NACL_PRId32"\n", *pp_error);
  rpc->result = NACL_SRPC_RESULT_OK;
}
