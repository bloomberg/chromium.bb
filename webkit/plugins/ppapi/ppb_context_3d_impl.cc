// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_context_3d_impl.h"

#include "base/logging.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_surface_3d_impl.h"

namespace webkit {
namespace ppapi {

namespace {

// Size of the transfer buffer.
const int32 kCommandBufferSize = 1024 * 1024;
const int32 kTransferBufferSize = 1024 * 1024;

PP_Resource Create(PP_Instance instance_id,
                   PP_Config3D_Dev config,
                   PP_Resource share_context,
                   const int32_t* attrib_list) {
  // TODO(alokp): Support shared context.
  DCHECK_EQ(0, share_context);
  if (share_context != 0)
    return 0;

  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<PPB_Context3D_Impl> context(
      new PPB_Context3D_Impl(instance));
  if (!context->Init(config, share_context, attrib_list))
    return 0;

  return context->GetReference();
}

PP_Bool IsContext3D(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_Context3D_Impl>(resource));
}

int32_t GetAttrib(PP_Resource context,
                  int32_t attribute,
                  int32_t* value) {
  // TODO(alokp): Implement me.
  return 0;
}

int32_t BindSurfaces(PP_Resource context_id,
                     PP_Resource draw,
                     PP_Resource read) {
  scoped_refptr<PPB_Context3D_Impl> context(
      Resource::GetAs<PPB_Context3D_Impl>(context_id));
  if (!context.get())
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<PPB_Surface3D_Impl> draw_surface(
      Resource::GetAs<PPB_Surface3D_Impl>(draw));
  if (!draw_surface.get())
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<PPB_Surface3D_Impl> read_surface(
      Resource::GetAs<PPB_Surface3D_Impl>(read));
  if (!read_surface.get())
    return PP_ERROR_BADRESOURCE;

  return context->BindSurfaces(draw_surface.get(), read_surface.get());
}

int32_t GetBoundSurfaces(PP_Resource context,
                         PP_Resource* draw,
                         PP_Resource* read) {
  // TODO(alokp): Implement me.
  return 0;
}

const PPB_Context3D_Dev ppb_context3d = {
  &Create,
  &IsContext3D,
  &GetAttrib,
  &BindSurfaces,
  &GetBoundSurfaces,
};

}  // namespace

PPB_Context3D_Impl::PPB_Context3D_Impl(PluginInstance* instance)
    : Resource(instance),
      instance_(instance),
      transfer_buffer_id_(0),
      draw_surface_(NULL),
      read_surface_(NULL) {
}

PPB_Context3D_Impl::~PPB_Context3D_Impl() {
  Destroy();
}

const PPB_Context3D_Dev* PPB_Context3D_Impl::GetInterface() {
  return &ppb_context3d;
}

PPB_Context3D_Impl* PPB_Context3D_Impl::AsPPB_Context3D_Impl() {
  return this;
}

bool PPB_Context3D_Impl::Init(PP_Config3D_Dev config,
                              PP_Resource share_context,
                              const int32_t* attrib_list) {
  // Create and initialize the objects required to issue GLES2 calls.
  platform_context_.reset(instance()->CreateContext3D());
  if (!platform_context_.get()) {
    Destroy();
    return false;
  }
  if (!platform_context_->Init()) {
    Destroy();
    return false;
  }

  gpu::CommandBuffer* command_buffer = platform_context_->GetCommandBuffer();
  DCHECK(command_buffer);

  if (!command_buffer->Initialize(kCommandBufferSize)) {
    Destroy();
    return false;
  }

  // Create the GLES2 helper, which writes the command buffer protocol.
  helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer));
  if (!helper_->Initialize(kCommandBufferSize)) {
    Destroy();
    return false;
  }

  // Create a transfer buffer used to copy resources between the renderer
  // process and the GPU process.
  transfer_buffer_id_ =
      command_buffer->CreateTransferBuffer(kTransferBufferSize);
  if (transfer_buffer_id_ < 0) {
    Destroy();
    return false;
  }

  // Map the buffer into the renderer process's address space.
  gpu::Buffer transfer_buffer =
      command_buffer->GetTransferBuffer(transfer_buffer_id_);
  if (!transfer_buffer.ptr) {
    Destroy();
    return false;
  }

  // Create the object exposing the OpenGL API.
  gles2_impl_.reset(new gpu::gles2::GLES2Implementation(
      helper_.get(),
      transfer_buffer.size,
      transfer_buffer.ptr,
      transfer_buffer_id_,
      false));

  return true;
}

int32_t PPB_Context3D_Impl::BindSurfaces(PPB_Surface3D_Impl* draw,
                                         PPB_Surface3D_Impl* read) {
  // TODO(alokp): Support separate draw-read surfaces.
  DCHECK_EQ(draw, read);
  if (draw != read)
    return PP_GRAPHICS3DERROR_BAD_MATCH;

  if (draw == draw_surface_)
    return PP_OK;

  if (draw && draw->context())
    return PP_GRAPHICS3DERROR_BAD_ACCESS;

  if (draw_surface_)
    draw_surface_->BindToContext(NULL);
  if (draw && !draw->BindToContext(this))
    return PP_ERROR_NOMEMORY;

  draw_surface_ = draw;
  read_surface_ = read;
  return PP_OK;
}

void PPB_Context3D_Impl::Destroy() {
  if (draw_surface_)
    draw_surface_->BindToContext(NULL);

  gles2_impl_.reset();

  if (platform_context_.get() && transfer_buffer_id_ != 0) {
    gpu::CommandBuffer* command_buffer = platform_context_->GetCommandBuffer();
    command_buffer->DestroyTransferBuffer(transfer_buffer_id_);
    transfer_buffer_id_ = 0;
  }

  helper_.reset();
  platform_context_.reset();
}

}  // namespace ppapi
}  // namespace webkit

