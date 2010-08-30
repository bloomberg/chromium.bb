// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_graphics_3d.h"

#include "gpu/command_buffer/common/command_buffer.h"
#include "base/singleton.h"
#include "base/thread_local.h"
#include "third_party/ppapi/c/dev/ppb_graphics_3d_dev.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"

namespace pepper {

namespace {

struct CurrentContextTag {};
typedef Singleton<base::ThreadLocalPointer<Graphics3D>,
    DefaultSingletonTraits<base::ThreadLocalPointer<Graphics3D> >,
    CurrentContextTag> CurrentContextKey;

// Size of the transfer buffer.
enum { kTransferBufferSize = 512 * 1024 };

bool IsGraphics3D(PP_Resource resource) {
  return !!Resource::GetAs<Graphics3D>(resource);
}

bool GetConfigs(int32_t* configs, int32_t config_size, int32_t* num_config) {
  // TODO(neb): Implement me!
  return false;
}

bool ChooseConfig(const int32_t* attrib_list, int32_t* configs,
                  int32_t config_size, int32_t* num_config) {
  // TODO(neb): Implement me!
  return false;
}

bool GetConfigAttrib(int32_t config, int32_t attribute, int32_t* value) {
  // TODO(neb): Implement me!
  return false;
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

  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance) {
    return 0;
  }

  scoped_refptr<Graphics3D> context(new Graphics3D(instance->module()));
  if (!context->Init(instance_id, config, attrib_list)) {
    return 0;
  }

  return context->GetReference();
}

void* GetProcAddress(const char* name) {
  // TODO(neb): Implement me!
  return NULL;
}

bool MakeCurrent(PP_Resource graphics3d) {
  if (!graphics3d) {
    Graphics3D::ResetCurrent();
    return true;
  } else {
    scoped_refptr<Graphics3D> context(Resource::GetAs<Graphics3D>(graphics3d));
    return context.get() && context->MakeCurrent();
  }
}

PP_Resource GetCurrentContext() {
  Graphics3D* currentContext = Graphics3D::GetCurrent();
  return currentContext ? currentContext->GetReference() : 0;
}

bool SwapBuffers(PP_Resource graphics3d) {
  scoped_refptr<Graphics3D> context(Resource::GetAs<Graphics3D>(graphics3d));
  return context && context->SwapBuffers();
}

uint32_t GetError() {
  // TODO(neb): Figure out error checking.
  return PP_GRAPHICS_3D_ERROR_SUCCESS;
}

const PPB_Graphics3D_Dev ppb_graphics3d = {
  &IsGraphics3D,
  &GetConfigs,
  &ChooseConfig,
  &GetConfigAttrib,
  &QueryString,
  &CreateContext,
  &GetProcAddress,
  &MakeCurrent,
  &GetCurrentContext,
  &SwapBuffers,
  &GetError
};

}  // namespace

Graphics3D::Graphics3D(PluginModule* module)
    : Resource(module),
      command_buffer_(NULL),
      transfer_buffer_id_(0),
      method_factory3d_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

const PPB_Graphics3D_Dev* Graphics3D::GetInterface() {
  return &ppb_graphics3d;
}

Graphics3D* Graphics3D::GetCurrent() {
  return CurrentContextKey::get()->Get();
}

void Graphics3D::ResetCurrent() {
  CurrentContextKey::get()->Set(NULL);
}

Graphics3D::~Graphics3D() {
  Destroy();
}

bool Graphics3D::Init(PP_Instance instance_id, int32_t config,
                      const int32_t* attrib_list) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance) {
    return false;
  }

  // Create and initialize the objects required to issue GLES2 calls.
  platform_context_.reset(instance->delegate()->CreateContext3D());
  if (!platform_context_.get())
    return false;

  if (!platform_context_->Init(instance->position(),
                               instance->clip())) {
    platform_context_.reset();
    return false;
  }
  command_buffer_ = platform_context_->GetCommandBuffer();
  gles2_helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer_));
  gpu::Buffer buffer = command_buffer_->GetRingBuffer();
  if (gles2_helper_->Initialize(buffer.size)) {
    transfer_buffer_id_ =
        command_buffer_->CreateTransferBuffer(kTransferBufferSize);
    gpu::Buffer transfer_buffer =
        command_buffer_->GetTransferBuffer(transfer_buffer_id_);
    if (transfer_buffer.ptr) {
      gles2_implementation_.reset(new gpu::gles2::GLES2Implementation(
          gles2_helper_.get(),
          transfer_buffer.size,
          transfer_buffer.ptr,
          transfer_buffer_id_,
          false));
      platform_context_->SetNotifyRepaintTask(
          method_factory3d_.NewRunnableMethod(&Graphics3D::HandleRepaint,
                                              instance_id));
      return true;
    }
  }

  // Tear everything down if initialization failed.
  Destroy();
  return false;
}

bool Graphics3D::MakeCurrent() {
  if (!command_buffer_)
    return false;

  CurrentContextKey::get()->Set(this);

  // Don't request latest error status from service. Just use the locally
  // cached information from the last flush.
  // TODO(apatrick): I'm not sure if this should actually change the
  // current context if it fails. For now it gets changed even if it fails
  // becuase making GL calls with a NULL context crashes.
  // TODO(neb): Figure out error checking.
//  if (command_buffer_->GetCachedError() != gpu::error::kNoError)
//    return false;
  return true;
}

bool Graphics3D::SwapBuffers() {
  if (!command_buffer_)
    return false;

  // Don't request latest error status from service. Just use the locally cached
  // information from the last flush.
  // TODO(neb): Figure out error checking.
//  if (command_buffer_->GetCachedError() != gpu::error::kNoError)
//    return false;

  gles2_implementation_->SwapBuffers();
  return true;
}

void Graphics3D::Destroy() {
  if (GetCurrent() == this) {
    ResetCurrent();
  }

  method_factory3d_.RevokeAll();

  gles2_implementation_.reset();

  if (command_buffer_ && transfer_buffer_id_ != 0) {
    command_buffer_->DestroyTransferBuffer(transfer_buffer_id_);
    transfer_buffer_id_ = 0;
  }

  gles2_helper_.reset();

  // Platform context owns the command buffer.
  platform_context_.reset();
  command_buffer_ = NULL;
}

void Graphics3D::HandleRepaint(PP_Instance instance_id) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (instance) {
    instance->Graphics3DContextLost();
    if (platform_context_.get()) {
      platform_context_->SetNotifyRepaintTask(
          method_factory3d_.NewRunnableMethod(&Graphics3D::HandleRepaint,
                                              instance_id));
    }
  }
}

}  // namespace pepper

