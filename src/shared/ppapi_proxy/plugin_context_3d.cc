/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_context_3d.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "native_client/src/shared/ppapi_proxy/command_buffer_nacl.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_instance_data.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_core.h"
#include "native_client/src/shared/ppapi_proxy/plugin_surface_3d.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/third_party/ppapi/c/pp_errors.h"
#include "native_client/src/third_party/ppapi/c/pp_rect.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

const int32 kTransferBufferSize = 512 * 1024;

PP_Resource Create(PP_Instance instance,
                   PP_Config3D_Dev config,
                   PP_Resource share_context,
                   const int32_t* attrib_list) {
  DebugPrintf("PPB_Context3D::Create: instance=%"NACL_PRIu32"\n", instance);

  nacl_abi_size_t attrib_list_size = 0;
  PP_Resource resource;
  if (attrib_list) {
    attrib_list_size = 1;
    while (PP_GRAPHICS3DATTRIB_NONE != attrib_list[attrib_list_size - 1]) {
      attrib_list_size += 2;
    }
  }
  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Context3DTrusted_CreateRaw(
          GetMainSrpcChannel(),
          instance,
          config,
          share_context,
          attrib_list_size,
          const_cast<int32_t*>(attrib_list),
          &resource);

  if (retval == NACL_SRPC_RESULT_OK) {
    scoped_refptr<PluginContext3D> context_3d =
        PluginResource::AdoptAs<PluginContext3D>(resource);
    if (context_3d.get()) {
      context_3d->set_instance_id(instance);
      return resource;
    }
  }
  return kInvalidResourceId;
}

PP_Bool IsContext3D(PP_Resource resource) {
  DebugPrintf("PPB_Context3D::IsContext3D: resource=%"NACL_PRIu32"\n",
              resource);
  return PluginResource::GetAs<PluginContext3D>(resource).get()
      ? PP_TRUE : PP_FALSE;
}

int32_t GetAttrib(PP_Resource context,
                  int32_t attribute,
                  int32_t* value) {
  UNREFERENCED_PARAMETER(context);
  UNREFERENCED_PARAMETER(attribute);
  UNREFERENCED_PARAMETER(value);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_FAILED;
}

int32_t BindSurfaces(PP_Resource context_id,
                     PP_Resource draw_id,
                     PP_Resource read_id) {
  DebugPrintf("PPB_Context3D::BindSurfaces3D: context=%"NACL_PRIu32", "
              "draw surface=%"NACL_PRIu32"\n", context_id, draw_id);
  int32_t error;

  scoped_refptr<PluginContext3D> context =
      PluginResource::GetAs<PluginContext3D>(context_id).get();

  if (!context)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<PluginSurface3D> draw =
      PluginResource::GetAs<PluginSurface3D>(draw_id).get();

  if (!draw.get()
      || !PluginResource::GetAs<PluginSurface3D>(read_id).get())
    return PP_ERROR_BADRESOURCE;

  draw->set_bound_context(context_id);

  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Context3D_BindSurfaces(
          GetMainSrpcChannel(),
          context_id,
          draw_id,
          read_id,
          &error);
  if (retval == NACL_SRPC_RESULT_OK) {
    if (error == PP_OK) {
      PluginInstanceData* instance =
          PluginInstanceData::FromPP(context->instance_id());
      if (instance) {
        const PP_Size size = instance->position().size;
        context->impl()->ResizeCHROMIUM(size.width, size.height);
      }
    }
    return error;
  } else {
    return PP_ERROR_BADRESOURCE;
  }

}

int32_t GetBoundSurfaces(PP_Resource context,
                         PP_Resource* draw,
                         PP_Resource* read) {
  UNREFERENCED_PARAMETER(context);
  UNREFERENCED_PARAMETER(draw);
  UNREFERENCED_PARAMETER(read);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_FAILED;
}

}  // namespace

__thread PP_Resource PluginContext3D::cached_resource = 0;
__thread gpu::gles2::GLES2Implementation*
  PluginContext3D::cached_implementation = NULL;

PluginContext3D::PluginContext3D() : instance_id_(0) { }

PluginContext3D::~PluginContext3D() {
  // Invalidate the cache.
  cached_resource = 0;
  cached_implementation = NULL;
}

// static
gpu::gles2::GLES2Implementation* PluginContext3D::implFromResourceSlow(
    PP_Resource context) {
  // For performance reasons, we don't error-check the context, but crash on
  // NULL instead.
  gpu::gles2::GLES2Implementation* impl =
      PluginResource::GetAs<PluginContext3D>(context)->impl();
  cached_resource = context;
  cached_implementation = impl;
  return impl;
}


bool PluginContext3D::InitFromBrowserResource(PP_Resource res) {
  DebugPrintf("PluginContext3D::InitFromBrowserResource: "
              "resource=%"NACL_PRIu32"\n", res);

  // Create and initialize the objects required to issue GLES2 calls.
  command_buffer_.reset(new CommandBufferNacl(res, PluginCore::GetInterface()));
  command_buffer_->Initialize(kTransferBufferSize);
  gles2_helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer_.get()));
  gpu::Buffer buffer = command_buffer_->GetRingBuffer();
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
          false));
      return true;
    }
  }
  return false;
}

void PluginContext3D::SwapBuffers() {
  gles2_implementation_->SwapBuffers();
}

// static
const PPB_Context3D_Dev* PluginContext3D::GetInterface() {
  static const PPB_Context3D_Dev intf = {
    &Create,
    &IsContext3D,
    &GetAttrib,
    &BindSurfaces,
    &GetBoundSurfaces
  };
  return &intf;
}

}  // namespace ppapi_proxy

