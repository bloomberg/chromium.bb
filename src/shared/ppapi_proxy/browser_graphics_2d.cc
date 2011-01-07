// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "srpcgen/ppb_rpc.h"

//
// The following methods are the SRPC dispatchers for ppapi/c/ppb_graphics_2d.h.
//

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
  *resource = ppapi_proxy::PPBGraphics2DInterface()->Create(
      instance,
      const_cast<const struct PP_Size*>(
          reinterpret_cast<struct PP_Size*>(size)),
      (is_always_opaque ? PP_TRUE : PP_FALSE));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics2DRpcServer::PPB_Graphics2D_IsGraphics2D(NaClSrpcRpc* rpc,
                                                         NaClSrpcClosure* done,
                                                         PP_Resource resource,
                                                         int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *success = 0;
  PP_Bool pp_success =
      ppapi_proxy::PPBGraphics2DInterface()->IsGraphics2D(resource);
  *success = (pp_success == PP_TRUE);
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
  PP_Bool pp_success = ppapi_proxy::PPBGraphics2DInterface()->Describe(
      graphics_2d, reinterpret_cast<struct PP_Size*>(size), &is_opaque);
  *is_always_opaque = (is_opaque == PP_TRUE);
  *success = (pp_success == PP_TRUE);
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
  if (top_left_bytes != sizeof(struct PP_Point) ||
      src_rect_bytes != sizeof(struct PP_Rect)) {
    return;
  }
  ppapi_proxy::PPBGraphics2DInterface()->PaintImageData(
      graphics_2d,
      image,
      const_cast<const struct PP_Point*>(
          reinterpret_cast<struct PP_Point*>(top_left)),
      const_cast<const struct PP_Rect*>(
          reinterpret_cast<struct PP_Rect*>(src_rect)));
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
  if (clip_rect_bytes != sizeof(struct PP_Rect) ||
      amount_bytes != sizeof(struct PP_Point)) {
    return;
  }
  ppapi_proxy::PPBGraphics2DInterface()->Scroll(
      graphics_2d,
      const_cast<const struct PP_Rect*>(
          reinterpret_cast<struct PP_Rect*>(clip_rect)),
      const_cast<const struct PP_Point*>(
          reinterpret_cast<struct PP_Point*>(amount)));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics2DRpcServer::PPB_Graphics2D_ReplaceContents(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource graphics_2d,
    PP_Resource image) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  ppapi_proxy::PPBGraphics2DInterface()->ReplaceContents(graphics_2d, image);
  rpc->result = NACL_SRPC_RESULT_OK;
}

// Flush is handled on the upcall thread, where another RPC service
// (PppUpcallRpcServer) is exported.
