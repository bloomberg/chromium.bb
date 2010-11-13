// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gen/native_client/src/shared/ppapi_proxy/ppb_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"

//
// The following methods are the SRPC dispatchers for ppapi/c/ppb_graphics_2d.h.
//

namespace {

const PPB_Graphics2D* GetInterface() {
  static const PPB_Graphics2D* graphics2d_interface = NULL;

  if (graphics2d_interface == NULL) {
    graphics2d_interface = reinterpret_cast<const PPB_Graphics2D*>(
        ppapi_proxy::GetBrowserInterface(PPB_GRAPHICS_2D_INTERFACE));
  }
  if (graphics2d_interface == NULL) {
    ppapi_proxy::DebugPrintf(
        "ERROR: attempt to get PPB_Graphics2D interface failed.\n");
  }
  CHECK(graphics2d_interface != NULL);
  return graphics2d_interface;
}

}  // namespace

void PpbGraphics2DRpcServer::PPB_Graphics2D_Create(NaClSrpcRpc* rpc,
                                                   NaClSrpcClosure* done,
                                                   int64_t module,
                                                   nacl_abi_size_t size_bytes,
                                                   int32_t* size,
                                                   int32_t is_always_opaque,
                                                   int64_t* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *resource = ppapi_proxy::kInvalidResourceId;
  if (size_bytes != sizeof(struct PP_Size)) {
    return;
  }
  PP_Resource pp_resource = GetInterface()->Create(
      static_cast<PP_Module>(module),
      const_cast<const struct PP_Size*>(
          reinterpret_cast<struct PP_Size*>(size)),
      (is_always_opaque ? PP_TRUE : PP_FALSE));
  *resource = static_cast<int64_t>(pp_resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics2DRpcServer::PPB_Graphics2D_IsGraphics2D(NaClSrpcRpc* rpc,
                                                         NaClSrpcClosure* done,
                                                         int64_t resource,
                                                         int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *success = 0;
  PP_Bool pp_success =
      GetInterface()->IsGraphics2D(static_cast<PP_Resource>(resource));
  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics2DRpcServer::PPB_Graphics2D_Describe(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int64_t graphics_2d,
    nacl_abi_size_t* size_bytes,
    int32_t* size,
    int32_t* is_always_opaque,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *success = 0;
  if (*size_bytes != sizeof(struct PP_Size)) {
    return;
  }
  PP_Bool is_opaque;
  PP_Bool pp_success = GetInterface()->Describe(
      static_cast<PP_Resource>(graphics_2d),
      reinterpret_cast<struct PP_Size*>(size),
      &is_opaque);
  *is_always_opaque = (is_opaque == PP_TRUE);
  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics2DRpcServer::PPB_Graphics2D_PaintImageData(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int64_t graphics_2d,
    int64_t image,
    nacl_abi_size_t top_left_bytes,
    int32_t* top_left,
    nacl_abi_size_t src_rect_bytes,
    int32_t* src_rect) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (top_left_bytes != sizeof(struct PP_Point) ||
      src_rect_bytes != sizeof(struct PP_Rect)) {
    return;
  }
  GetInterface()->PaintImageData(
      static_cast<PP_Resource>(graphics_2d),
      static_cast<PP_Resource>(image),
      const_cast<const struct PP_Point*>(
          reinterpret_cast<struct PP_Point*>(top_left)),
      const_cast<const struct PP_Rect*>(
          reinterpret_cast<struct PP_Rect*>(src_rect)));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics2DRpcServer::PPB_Graphics2D_Scroll(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int64_t graphics_2d,
    nacl_abi_size_t clip_rect_bytes,
    int32_t* clip_rect,
    nacl_abi_size_t amount_bytes,
    int32_t* amount) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (clip_rect_bytes != sizeof(struct PP_Rect) ||
      amount_bytes != sizeof(struct PP_Point)) {
    return;
  }
  GetInterface()->Scroll(
      static_cast<PP_Resource>(graphics_2d),
      const_cast<const struct PP_Rect*>(
          reinterpret_cast<struct PP_Rect*>(clip_rect)),
      const_cast<const struct PP_Point*>(
          reinterpret_cast<struct PP_Point*>(amount)));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbGraphics2DRpcServer::PPB_Graphics2D_ReplaceContents(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int64_t graphics_2d,
    int64_t image) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  GetInterface()->ReplaceContents(
      static_cast<PP_Resource>(graphics_2d),
      static_cast<PP_Resource>(image));
  rpc->result = NACL_SRPC_RESULT_OK;
}

// Flush is handled on the upcall thread, where another RPC service
// (PppUpcallRpcServer) is exported.
