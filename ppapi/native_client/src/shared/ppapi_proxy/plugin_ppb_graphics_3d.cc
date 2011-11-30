/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_graphics_3d.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "native_client/src/shared/ppapi_proxy/command_buffer_nacl.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_instance_data.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_core.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_var.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

const int32 kRingBufferSize = 4096 * 1024;
const int32 kTransferBufferSize = 4096 * 1024;

int32_t GetNumAttribs(const int32_t* attrib_list) {
  int32_t num = 0;
  if (attrib_list) {
    // skip over attrib pairs
    while (attrib_list[num] != PP_GRAPHICS3DATTRIB_NONE)
      num += 2;
    // Add one more for PP_GRAPHICS3DATTRIB_NONE.
    num += 1;
  }
  return num;
}

int32_t GetAttribMaxValue(PP_Instance instance,
                          int32_t attrib,
                          int32_t* attrib_value) {
  DebugPrintf("PPB_Graphics3D::GetAttribMaxValue: instance=%"NACL_PRIu32"\n",
              instance);
  int32_t pp_error;
  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Graphics3D_GetAttribMaxValue(
          GetMainSrpcChannel(),
          instance,
          attrib,
          attrib_value,
          &pp_error);
  if (retval != NACL_SRPC_RESULT_OK) {
    return PP_ERROR_BADARGUMENT;
  }
  return pp_error;
}

PP_Resource Create(PP_Instance instance,
                   PP_Resource share_context,
                   const int32_t* attrib_list) {
  DebugPrintf("PPB_Graphics3D::Create: instance=%"NACL_PRIu32"\n", instance);
  PP_Resource graphics3d_id = kInvalidResourceId;
  nacl_abi_size_t num_attribs = GetNumAttribs(attrib_list);
  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_CreateRaw(
          GetMainSrpcChannel(),
          instance,
          share_context,
          num_attribs, const_cast<int32_t *>(attrib_list),
          &graphics3d_id);
  if (retval == NACL_SRPC_RESULT_OK) {
    scoped_refptr<PluginGraphics3D> graphics_3d =
        PluginResource::AdoptAs<PluginGraphics3D>(graphics3d_id);
    if (graphics_3d.get()) {
      graphics_3d->set_instance_id(instance);
      return graphics3d_id;
    }
  }
  return kInvalidResourceId;
}

PP_Bool IsGraphics3D(PP_Resource resource) {
  DebugPrintf("PPB_Graphics3D::IsGraphics3D: resource=%"NACL_PRIu32"\n",
              resource);
  return PluginResource::GetAs<PluginGraphics3D>(resource).get()
      ? PP_TRUE : PP_FALSE;
}

int32_t GetAttribs(PP_Resource graphics3d_id,
                   int32_t* attrib_list) {
  int32_t pp_error;
  nacl_abi_size_t num_attribs = GetNumAttribs(attrib_list);
  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Graphics3D_GetAttribs(
          GetMainSrpcChannel(),
          graphics3d_id,
          num_attribs, attrib_list,
          &num_attribs, attrib_list,
          &pp_error);
  if (retval != NACL_SRPC_RESULT_OK) {
    return PP_ERROR_BADARGUMENT;
  }
  return pp_error;
}

int32_t SetAttribs(PP_Resource graphics3d_id,
                   int32_t* attrib_list) {
  int32_t pp_error;
  nacl_abi_size_t num_attribs = GetNumAttribs(attrib_list);
  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Graphics3D_SetAttribs(
          GetMainSrpcChannel(),
          graphics3d_id,
          num_attribs, attrib_list,
          &pp_error);
  if (retval != NACL_SRPC_RESULT_OK) {
    return PP_ERROR_BADARGUMENT;
  }
  return pp_error;
}

int32_t GetError(PP_Resource graphics3d_id) {
  DebugPrintf("PPB_Graphics3D::GetError: graphics3d_id=%"NACL_PRIu32"\n",
              graphics3d_id);
  int32_t pp_error;
  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Graphics3D_GetError(
          GetMainSrpcChannel(),
          graphics3d_id,
          &pp_error);
  if (retval != NACL_SRPC_RESULT_OK) {
    DebugPrintf("PPB_Graphics3D::GetError: retval != NACL_SRPC_RESULT_OK\n");
    return PP_ERROR_BADARGUMENT;
  }
  DebugPrintf("PPB_Graphics3D::GetError: pp_error=%"NACL_PRIu32"\n", pp_error);
  return pp_error;
}

int32_t ResizeBuffers(PP_Resource graphics3d_id,
                      int32_t width,
                      int32_t height) {
  if ((width < 0) || (height < 0))
    return PP_ERROR_BADARGUMENT;
  gpu::gles2::GLES2Implementation* impl =
      PluginGraphics3D::implFromResource(graphics3d_id);
  if (impl == NULL)
    return PP_ERROR_BADRESOURCE;
  impl->ResizeCHROMIUM(width, height);
  return PP_OK;
}

int32_t SwapBuffs(PP_Resource graphics3d_id,
                  struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_Graphics3D::SwapBuffers: graphics3d_id=%"NACL_PRIu32"\n",
              graphics3d_id);

  scoped_refptr<PluginGraphics3D> graphics3d =
      PluginResource::GetAs<PluginGraphics3D>(graphics3d_id).get();
  if (!graphics3d.get())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);

  return MayForceCallback(callback,
      graphics3d->SwapBuffers(graphics3d_id, callback));
}

}  // namespace

  // TODO(nfullagar): make cached_* variables TLS once 64bit NaCl TLS is faster,
  // and the proxy has support for being called off the main thread.
  // see: http://code.google.com/p/chromium/issues/detail?id=99217
  PP_Resource PluginGraphics3D::cached_graphics3d_id = 0;
  gpu::gles2::GLES2Implementation*
  PluginGraphics3D::cached_implementation = NULL;

PluginGraphics3D::PluginGraphics3D() : instance_id_(0) { }

PluginGraphics3D::~PluginGraphics3D() {
  DebugPrintf("PluginGraphics3D::~PluginGraphics3D()\n");
  // Explicitly tear down scopted pointers; ordering below matters.
  gles2_implementation_.reset();
  gles2_helper_.reset();
  command_buffer_.reset();
  // Invalidate the cache.
  cached_graphics3d_id = 0;
  cached_implementation = NULL;
}

// static
gpu::gles2::GLES2Implementation* PluginGraphics3D::implFromResourceSlow(
    PP_Resource graphics3d_id) {
  DebugPrintf("PluginGraphics3D::implFromResourceSlow: "
              "resource=%"NACL_PRIu32"\n", graphics3d_id);

  // For performance reasons, we don't error-check the context, but crash on
  // NULL instead.
  gpu::gles2::GLES2Implementation* impl =
      PluginResource::GetAs<PluginGraphics3D>(graphics3d_id)->impl();
  cached_graphics3d_id = graphics3d_id;
  cached_implementation = impl;
  return impl;
}


bool PluginGraphics3D::InitFromBrowserResource(PP_Resource res) {
  DebugPrintf("PluginGraphics3D::InitFromBrowserResource: "
              "resource=%"NACL_PRIu32"\n", res);

  // Create and initialize the objects required to issue GLES2 calls.
  command_buffer_.reset(new CommandBufferNacl(res, PluginCore::GetInterface()));
  if (command_buffer_->Initialize(kRingBufferSize)) {
    gles2_helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer_.get()));
    gpu::Buffer buffer = command_buffer_->GetRingBuffer();
    DebugPrintf("PluginGraphics3D::InitFromBrowserResource: buffer size: %d\n",
        buffer.size);
    if (gles2_helper_->Initialize(buffer.size)) {
      // Request id -1 to signify 'don't care'
      int32 transfer_buffer_id =
          command_buffer_->CreateTransferBuffer(kTransferBufferSize, -1);
      gpu::Buffer transfer_buffer =
          command_buffer_->GetTransferBuffer(transfer_buffer_id);
      if (transfer_buffer.ptr) {
        gles2_implementation_.reset(new gpu::gles2::GLES2Implementation(
            gles2_helper_.get(),
            transfer_buffer.size,
            transfer_buffer.ptr,
            transfer_buffer_id,
            false,
            true));
        return true;
      }
    }
  }
  return false;
}

int32_t PluginGraphics3D::SwapBuffers(PP_Resource graphics3d_id,
                                      struct PP_CompletionCallback callback) {
  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  impl()->SwapBuffers();
  int32_t pp_error;
  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Graphics3D_SwapBuffers(
          GetMainSrpcChannel(),
          graphics3d_id,
          callback_id,
          &pp_error);
  if (retval != NACL_SRPC_RESULT_OK)
    return PP_ERROR_FAILED;

  if ((PP_OK_COMPLETIONPENDING != pp_error) && (PP_OK != pp_error))
    return pp_error;

  return PP_OK_COMPLETIONPENDING;
}


// static
const PPB_Graphics3D* PluginGraphics3D::GetInterface() {
  static const PPB_Graphics3D intf = {
    &GetAttribMaxValue,
    &Create,
    &IsGraphics3D,
    &GetAttribs,
    &SetAttribs,
    &GetError,
    &ResizeBuffers,
    &SwapBuffs,
  };
  return &intf;
}

}  // namespace ppapi_proxy

