// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_context_3d_impl.h"

#include "gpu/command_buffer/common/command_buffer.h"
#include "ppapi/c/dev/ppb_graphics_3d_dev.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit {
namespace ppapi {

namespace {

// Size of the transfer buffer.
enum { kTransferBufferSize = 512 * 1024 };

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
      new PPB_Context3D_Impl(instance->module()));
  if (!context->Init(instance, config, share_context, attrib_list))
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

int32_t BindSurfaces(PP_Resource context,
                     PP_Resource draw,
                     PP_Resource read) {
  // TODO(alokp): Implement me.
  return 0;
}

int32_t GetBoundSurfaces(PP_Resource context,
                         PP_Resource* draw,
                         PP_Resource* read) {
  // TODO(alokp): Implement me.
  return 0;
}

int32_t SwapBuffers(PP_Resource context_id,
                    PP_CompletionCallback callback) {
  scoped_refptr<PPB_Context3D_Impl> context(
      Resource::GetAs<PPB_Context3D_Impl>(context_id));
  return context->SwapBuffers();
}

const PPB_Context3D_Dev ppb_context3d = {
  &Create,
  &IsContext3D,
  &GetAttrib,
  &BindSurfaces,
  &GetBoundSurfaces,
  &SwapBuffers
};

}  // namespace

PPB_Context3D_Impl::PPB_Context3D_Impl(PluginModule* module)
    : Resource(module),
      bound_instance_(NULL),
      gles2_impl_(NULL) {
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

bool PPB_Context3D_Impl::Init(PluginInstance* instance,
                              PP_Config3D_Dev config,
                              PP_Resource share_context,
                              const int32_t* attrib_list) {
  DCHECK(instance);
  // Create and initialize the objects required to issue GLES2 calls.
  platform_context_.reset(instance->delegate()->CreateContext3D());
  if (!platform_context_.get()) {
    Destroy();
    return false;
  }
  if (!platform_context_->Init()) {
    Destroy();
    return false;
  }

  gles2_impl_ = platform_context_->GetGLES2Implementation();
  DCHECK(gles2_impl_);

  return true;
}

bool PPB_Context3D_Impl::BindToInstance(PluginInstance* new_instance) {
  if (bound_instance_ == new_instance)
    return true;  // Rebinding the same device, nothing to do.
  if (bound_instance_ && new_instance)
    return false;  // Can't change a bound device.

  if (new_instance) {
    // Resize the backing texture to the size of the instance when it is bound.
    platform_context_->ResizeBackingTexture(new_instance->position().size());

    // This is a temporary hack. The SwapBuffers is issued to force the resize
    // to take place before any subsequent rendering. This might lead to a
    // partially rendered frame being displayed. It is also not thread safe
    // since the SwapBuffers is written to the command buffer and that command
    // buffer might be written to by another thread.
    // TODO(apatrick): Figure out the semantics of binding and resizing.
    platform_context_->SwapBuffers();
  }

  bound_instance_ = new_instance;
  return true;
}

bool PPB_Context3D_Impl::SwapBuffers() {
  if (!platform_context_.get())
    return false;

  return platform_context_->SwapBuffers();
}

void PPB_Context3D_Impl::SetSwapBuffersCallback(Callback0::Type* callback) {
  if (!platform_context_.get())
    return;

  platform_context_->SetSwapBuffersCallback(callback);
}

unsigned int PPB_Context3D_Impl::GetBackingTextureId() {
  if (!platform_context_.get())
    return 0;

  return platform_context_->GetBackingTextureId();
}

void PPB_Context3D_Impl::ResizeBackingTexture(const gfx::Size& size) {
  if (!platform_context_.get())
    return;

  platform_context_->ResizeBackingTexture(size);
}

void PPB_Context3D_Impl::Destroy() {
  gles2_impl_ = NULL;
  platform_context_.reset();
}

}  // namespace ppapi
}  // namespace webkit

