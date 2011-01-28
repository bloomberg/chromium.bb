// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_surface_3d_impl.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ppapi/c/dev/ppb_graphics_3d_dev.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_context_3d_impl.h"

namespace webkit {
namespace ppapi {

namespace {

PP_Resource Create(PP_Instance instance_id,
                   PP_Config3D_Dev config,
                   const int32_t* attrib_list) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<PPB_Surface3D_Impl> surface(
      new PPB_Surface3D_Impl(instance));
  if (!surface->Init(config, attrib_list))
    return 0;

  return surface->GetReference();
}

PP_Bool IsSurface3D(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_Surface3D_Impl>(resource));
}

int32_t SetAttrib(PP_Resource surface_id,
                  int32_t attribute,
                  int32_t value) {
  // TODO(alokp): Implement me.
  return 0;
}

int32_t GetAttrib(PP_Resource surface_id,
                  int32_t attribute,
                  int32_t* value) {
  // TODO(alokp): Implement me.
  return 0;
}

int32_t SwapBuffers(PP_Resource surface_id,
                    PP_CompletionCallback callback) {
  scoped_refptr<PPB_Surface3D_Impl> surface(
      Resource::GetAs<PPB_Surface3D_Impl>(surface_id));
  return surface->SwapBuffers(callback);
}

const PPB_Surface3D_Dev ppb_surface3d = {
  &Create,
  &IsSurface3D,
  &SetAttrib,
  &GetAttrib,
  &SwapBuffers
};

}  // namespace

PPB_Surface3D_Impl::PPB_Surface3D_Impl(PluginInstance* instance)
    : Resource(instance),
      bound_to_instance_(false),
      swap_initiated_(false),
      swap_callback_(PP_BlockUntilComplete()),
      context_(NULL) {
}

PPB_Surface3D_Impl::~PPB_Surface3D_Impl() {
}

const PPB_Surface3D_Dev* PPB_Surface3D_Impl::GetInterface() {
  return &ppb_surface3d;
}

PPB_Surface3D_Impl* PPB_Surface3D_Impl::AsPPB_Surface3D_Impl() {
  return this;
}

bool PPB_Surface3D_Impl::Init(PP_Config3D_Dev config,
                              const int32_t* attrib_list) {
  return true;
}

bool PPB_Surface3D_Impl::BindToInstance(bool bind) {
  bound_to_instance_ = bind;
  return true;
}

bool PPB_Surface3D_Impl::BindToContext(
    PPB_Context3D_Impl* context) {
  if (context == context_)
    return true;

  // Unbind from the current context.
  if (context_) {
    context_->platform_context()->SetSwapBuffersCallback(NULL);
  }
  if (context) {
    // Resize the backing texture to the size of the instance when it is bound.
    // TODO(alokp): This should be the responsibility of plugins.
    gpu::gles2::GLES2Implementation* impl = context->gles2_impl();
    if (impl) {
      const gfx::Size& size = instance()->position().size();
      impl->ResizeCHROMIUM(size.width(), size.height());
    }

    context->platform_context()->SetSwapBuffersCallback(
        NewCallback(this, &PPB_Surface3D_Impl::OnSwapBuffers));
  }
  context_ = context;
  return true;
}

bool PPB_Surface3D_Impl::SwapBuffers(PP_CompletionCallback callback) {
  if (!context_)
    return false;

  if (swap_callback_.func) {
    // Already a pending SwapBuffers that hasn't returned yet.
    return false;
  }

  swap_callback_ = callback;
  gpu::gles2::GLES2Implementation* impl = context_->gles2_impl();
  if (impl) {
    context_->gles2_impl()->SwapBuffers();
  }
  return true;
}

void PPB_Surface3D_Impl::ViewInitiatedPaint() {
  if (swap_callback_.func) {
    swap_initiated_ = true;
  }
}

void PPB_Surface3D_Impl::ViewFlushedPaint() {
  if (swap_initiated_ && swap_callback_.func) {
    // We must clear swap_callback_ before issuing the callback. It will be
    // common for the plugin to issue another SwapBuffers in response to the
    // callback, and we don't want to think that a callback is already pending.
    PP_CompletionCallback callback = PP_BlockUntilComplete();
    std::swap(callback, swap_callback_);
    swap_initiated_ = false;
    PP_RunCompletionCallback(&callback, PP_OK);
  }
}

unsigned int PPB_Surface3D_Impl::GetBackingTextureId() {
  return context_ ? context_->platform_context()->GetBackingTextureId() : 0;
}

void PPB_Surface3D_Impl::OnSwapBuffers() {
  if (bound_to_instance_)
    instance()->CommitBackingTexture();
}

}  // namespace ppapi
}  // namespace webkit

