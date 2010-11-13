/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_graphics_2d.h"

#include <stdio.h>
#include <string.h>
#include "gen/native_client/src/shared/ppapi_proxy/ppb_rpc.h"
#include "gen/native_client/src/shared/ppapi_proxy/upcall.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"

namespace ppapi_proxy {

namespace {

const nacl_abi_size_t kPpSizeBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_Size));
const nacl_abi_size_t kPpPointBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_Point));
const nacl_abi_size_t kPpRectBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_Rect));

PP_Resource Create(PP_Module module,
                   const struct PP_Size* size,
                   PP_Bool is_always_opaque) {
  int32_t* size_as_int_ptr =
      reinterpret_cast<int32_t*>(const_cast<struct PP_Size*>(size));
  int32_t is_always_opaque_as_int = static_cast<int32_t>(is_always_opaque);
  int64_t resource;
  NaClSrpcError retval =
      PpbGraphics2DRpcClient::PPB_Graphics2D_Create(
          GetMainSrpcChannel(),
          static_cast<int64_t>(module),
          kPpSizeBytes,
          size_as_int_ptr,
          is_always_opaque_as_int,
          &resource);
  if (retval == NACL_SRPC_RESULT_OK) {
    return static_cast<PP_Resource>(resource);
  } else {
    return kInvalidResourceId;
  }
}

PP_Bool IsGraphics2D(PP_Resource resource) {
  int32_t result;
  NaClSrpcError retval =
      PpbGraphics2DRpcClient::PPB_Graphics2D_IsGraphics2D(
          GetMainSrpcChannel(),
          static_cast<int64_t>(resource),
          &result);
  if (retval == NACL_SRPC_RESULT_OK) {
    return result ? PP_TRUE : PP_FALSE;
  } else {
    return PP_FALSE;
  }
}

PP_Bool Describe(PP_Resource graphics_2d,
                 struct PP_Size* size,
                 PP_Bool* is_always_opaque) {
  int32_t is_always_opaque_as_int;
  nacl_abi_size_t size_ret = kPpSizeBytes;
  int32_t result;
  NaClSrpcError retval =
      PpbGraphics2DRpcClient::PPB_Graphics2D_Describe(
          GetMainSrpcChannel(),
          static_cast<int64_t>(graphics_2d),
          &size_ret,
          reinterpret_cast<int32_t*>(size),
          &is_always_opaque_as_int,
          &result);
  if (retval == NACL_SRPC_RESULT_OK || size_ret != kPpSizeBytes) {
    *is_always_opaque = is_always_opaque_as_int ? PP_TRUE : PP_FALSE;
    return result ? PP_TRUE : PP_FALSE;
  } else {
    return PP_FALSE;
  }
}

void PaintImageData(PP_Resource graphics_2d,
                    PP_Resource image,
                    const struct PP_Point* top_left,
                    const struct PP_Rect* src_rect) {
  // TODO(sehr,polina): there is no way to report a failure through this
  // interface design other than crash.  Let's find one.
  (void) PpbGraphics2DRpcClient::PPB_Graphics2D_PaintImageData(
      GetMainSrpcChannel(),
      static_cast<int64_t>(graphics_2d),
      static_cast<int64_t>(image),
      kPpPointBytes,
      reinterpret_cast<int32_t*>(const_cast<struct PP_Point*>(top_left)),
      kPpRectBytes,
      reinterpret_cast<int32_t*>(const_cast<struct PP_Rect*>(src_rect)));
}

void Scroll(PP_Resource graphics_2d,
            const struct PP_Rect* clip_rect,
            const struct PP_Point* amount) {
  // TODO(sehr,polina): there is no way to report a failure through this
  // interface design other than crash.  Let's find one.
  (void) PpbGraphics2DRpcClient::PPB_Graphics2D_Scroll(
      GetMainSrpcChannel(),
      static_cast<int64_t>(graphics_2d),
      kPpRectBytes,
      reinterpret_cast<int32_t*>(const_cast<struct PP_Rect*>(clip_rect)),
      kPpPointBytes,
      reinterpret_cast<int32_t*>(const_cast<struct PP_Point*>(amount)));
}

void ReplaceContents(PP_Resource graphics_2d, PP_Resource image) {
  (void) PpbGraphics2DRpcClient::PPB_Graphics2D_ReplaceContents(
      GetMainSrpcChannel(),
      static_cast<int64_t>(graphics_2d),
      static_cast<int64_t>(image));
}

int32_t Flush(PP_Resource graphics_2d,
              struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(graphics_2d);
  UNREFERENCED_PARAMETER(callback);
  // TODO(sehr): implement the call on the upcall channel once the completion
  // callback implementation is in place.
  NACL_UNIMPLEMENTED();
  return PP_ERROR_BADRESOURCE;
}

}  // namespace

const PPB_Graphics2D* PluginGraphics2D::GetInterface() {
  static const PPB_Graphics2D intf = {
    Create,
    IsGraphics2D,
    Describe,
    PaintImageData,
    Scroll,
    ReplaceContents,
    Flush,
  };
  return &intf;
}

}  // namespace ppapi_proxy
