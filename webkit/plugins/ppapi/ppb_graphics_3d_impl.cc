// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_graphics_3d_impl.h"

#include "gpu/command_buffer/common/command_buffer.h"
#include "ppapi/c/dev/ppb_graphics_3d_dev.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit {
namespace ppapi {

namespace {

// Size of the transfer buffer.
enum { kTransferBufferSize = 512 * 1024 };

PP_Bool IsGraphics3D(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_Graphics3D_Impl>(resource));
}

PP_Bool GetConfigs(int32_t* configs, int32_t config_size, int32_t* num_config) {
  // TODO(neb): Implement me!
  return PP_FALSE;
}

PP_Bool ChooseConfig(const int32_t* attrib_list, int32_t* configs,
                     int32_t config_size, int32_t* num_config) {
  // TODO(neb): Implement me!
  return PP_FALSE;
}

PP_Bool GetConfigAttrib(int32_t config, int32_t attribute, int32_t* value) {
  // TODO(neb): Implement me!
  return PP_FALSE;
}

const char* QueryString(int32_t name) {
  switch (name) {
    case EGL_CLIENT_APIS:
      return "OpenGL_ES";
    case EGL_EXTENSIONS:
      return "";
    case EGL_VENDOR:
      return "Google";
    case EGL_VERSION:
      return "1.0 Google";
    default:
      return NULL;
  }
}

PP_Resource CreateContext(PP_Instance instance_id, int32_t config,
                          int32_t share_context,
                          const int32_t* attrib_list) {
  DCHECK_EQ(0, share_context);

  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance) {
    return 0;
  }

  scoped_refptr<PPB_Graphics3D_Impl> context(
      new PPB_Graphics3D_Impl(instance->module()));
  if (!context->Init(instance_id, config, attrib_list)) {
    return 0;
  }

  return context->GetReference();
}

void* GetProcAddress(const char* name) {
  // TODO(neb): Implement me!
  return NULL;
}

PP_Bool SwapBuffers(PP_Resource graphics3d) {
  scoped_refptr<PPB_Graphics3D_Impl> context(
      Resource::GetAs<PPB_Graphics3D_Impl>(graphics3d));
  return BoolToPPBool(context && context->SwapBuffers());
}

uint32_t GetError() {
  // TODO(alokp): Fix this.
  return 0;
}

const PPB_Graphics3D_Dev ppb_graphics3d = {
  &IsGraphics3D,
  &GetConfigs,
  &ChooseConfig,
  &GetConfigAttrib,
  &QueryString,
  &CreateContext,
  &GetProcAddress,
  &SwapBuffers,
  &GetError
};

}  // namespace

PPB_Graphics3D_Impl::PPB_Graphics3D_Impl(PluginModule* module)
  : Resource(module),
    bound_instance_(NULL) {
}

const PPB_Graphics3D_Dev* PPB_Graphics3D_Impl::GetInterface() {
  return &ppb_graphics3d;
}

PPB_Graphics3D_Impl::~PPB_Graphics3D_Impl() {
  Destroy();
}

PPB_Graphics3D_Impl* PPB_Graphics3D_Impl::AsPPB_Graphics3D_Impl() {
  return this;
}

bool PPB_Graphics3D_Impl::Init(PP_Instance instance_id, int32_t config,
                               const int32_t* attrib_list) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance) {
    return false;
  }

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

  gles2_implementation_ = platform_context_->GetGLES2Implementation();
  DCHECK(gles2_implementation_);

  return true;
}

bool PPB_Graphics3D_Impl::BindToInstance(PluginInstance* new_instance) {
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

bool PPB_Graphics3D_Impl::SwapBuffers() {
  if (!platform_context_.get())
    return false;

  return platform_context_->SwapBuffers();
}

unsigned PPB_Graphics3D_Impl::GetError() {
  if (!platform_context_.get())
    return 0;

  return platform_context_->GetError();
}

void PPB_Graphics3D_Impl::ResizeBackingTexture(const gfx::Size& size) {
  if (!platform_context_.get())
    return;

  platform_context_->ResizeBackingTexture(size);
}

void PPB_Graphics3D_Impl::SetSwapBuffersCallback(Callback0::Type* callback) {
  if (!platform_context_.get())
    return;

  platform_context_->SetSwapBuffersCallback(callback);
}

unsigned PPB_Graphics3D_Impl::GetBackingTextureId() {
  if (!platform_context_.get())
    return 0;

  return platform_context_->GetBackingTextureId();
}

void PPB_Graphics3D_Impl::Destroy() {
  gles2_implementation_ = NULL;
  platform_context_.reset();
}

}  // namespace ppapi
}  // namespace webkit

