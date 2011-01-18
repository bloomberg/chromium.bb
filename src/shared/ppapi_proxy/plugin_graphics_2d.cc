/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_graphics_2d.h"

#include <stdio.h>
#include <string.h>
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_upcall.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "srpcgen/ppb_rpc.h"
#include "srpcgen/upcall.h"

namespace ppapi_proxy {

namespace {

const nacl_abi_size_t kPpSizeBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_Size));
const nacl_abi_size_t kPpPointBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_Point));
const nacl_abi_size_t kPpRectBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_Rect));

PP_Resource Create(PP_Instance instance,
                   const struct PP_Size* size,
                   PP_Bool is_always_opaque) {
  DebugPrintf("PPB_Graphics2D::Create: instance=%"NACL_PRIx32"\n", instance);
  char* size_as_char_ptr =
      reinterpret_cast<char*>(const_cast<struct PP_Size*>(size));
  int32_t is_always_opaque_as_int = static_cast<int32_t>(is_always_opaque);
  PP_Resource resource;
  NaClSrpcError retval =
      PpbGraphics2DRpcClient::PPB_Graphics2D_Create(
          GetMainSrpcChannel(),
          instance,
          kPpSizeBytes,
          size_as_char_ptr,
          is_always_opaque_as_int,
          &resource);
  if (retval == NACL_SRPC_RESULT_OK) {
    return resource;
  } else {
    return kInvalidResourceId;
  }
}

PP_Bool IsGraphics2D(PP_Resource resource) {
  DebugPrintf("PPB_Graphics2D::IsGraphics2D: resource=%"NACL_PRIx32"\n",
              resource);
  return PluginResource::GetAs<PluginGraphics2D>(resource).get()
      ? PP_TRUE : PP_FALSE;
}

PP_Bool Describe(PP_Resource graphics_2d,
                 struct PP_Size* size,
                 PP_Bool* is_always_opaque) {
  DebugPrintf("PPB_Graphics2D::Describe: graphics_2d=%"NACL_PRIx32"\n",
              graphics_2d);
  int32_t is_always_opaque_as_int;
  nacl_abi_size_t size_ret = kPpSizeBytes;
  int32_t result;
  NaClSrpcError retval =
      PpbGraphics2DRpcClient::PPB_Graphics2D_Describe(
          GetMainSrpcChannel(),
          graphics_2d,
          &size_ret,
          reinterpret_cast<char*>(size),
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
    DebugPrintf("PPB_Graphics2D::PaintImageData: graphics_2d=%"NACL_PRIx32"\n",
                graphics_2d);
  // TODO(sehr,polina): there is no way to report a failure through this
  // interface design other than crash.  Let's find one.
  (void) PpbGraphics2DRpcClient::PPB_Graphics2D_PaintImageData(
      GetMainSrpcChannel(),
      graphics_2d,
      image,
      kPpPointBytes,
      reinterpret_cast<char*>(const_cast<struct PP_Point*>(top_left)),
      kPpRectBytes,
      reinterpret_cast<char*>(const_cast<struct PP_Rect*>(src_rect)));
}

void Scroll(PP_Resource graphics_2d,
            const struct PP_Rect* clip_rect,
            const struct PP_Point* amount) {
  DebugPrintf("PPB_Graphics2D::Scroll: graphics_2d=%"NACL_PRIx32"\n",
              graphics_2d);
  // TODO(sehr,polina): there is no way to report a failure through this
  // interface design other than crash.  Let's find one.
  (void) PpbGraphics2DRpcClient::PPB_Graphics2D_Scroll(
      GetMainSrpcChannel(),
      graphics_2d,
      kPpRectBytes,
      reinterpret_cast<char*>(const_cast<struct PP_Rect*>(clip_rect)),
      kPpPointBytes,
      reinterpret_cast<char*>(const_cast<struct PP_Point*>(amount)));
}

void ReplaceContents(PP_Resource graphics_2d, PP_Resource image) {
  DebugPrintf("PPB_Graphics2D::ReplaceContents: graphics_2d=%"NACL_PRIx32"\n",
              graphics_2d);
  (void) PpbGraphics2DRpcClient::PPB_Graphics2D_ReplaceContents(
      GetMainSrpcChannel(), graphics_2d, image);
}

int32_t Flush(PP_Resource graphics_2d,
              struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_Graphics2D::Flush: graphics_2d=%"NACL_PRIx32"\n",
              graphics_2d);
  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BADARGUMENT;

  int32_t pp_error;
  NaClSrpcError retval =
      PpbGraphics2DRpcClient::PPB_Graphics2D_Flush(
          GetMainSrpcChannel(), graphics_2d, callback_id, &pp_error);
  if (retval != NACL_SRPC_RESULT_OK) {
    return PP_ERROR_FAILED;
  }
  return pp_error;
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
