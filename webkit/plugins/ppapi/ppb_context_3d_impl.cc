// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_context_3d_impl.h"

#include "base/logging.h"
#include "base/shared_memory.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ppapi/c/dev/ppb_context_3d_trusted_dev.h"
#include "ppapi/thunk/enter.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_surface_3d_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Context3D_API;
using ppapi::thunk::PPB_Surface3D_API;

namespace webkit {
namespace ppapi {

namespace {

// Size of the transfer buffer.
const int32 kCommandBufferSize = 1024 * 1024;
const int32 kTransferBufferSize = 1024 * 1024;

PP_Bool ShmToHandle(base::SharedMemory* shm,
                    size_t size,
                    int* shm_handle,
                    uint32_t* shm_size) {
  if (!shm || !shm_handle || !shm_size)
    return PP_FALSE;
#if defined(OS_POSIX)
  *shm_handle = shm->handle().fd;
#elif defined(OS_WIN)
  *shm_handle = reinterpret_cast<int>(shm->handle());
#else
  #error "Platform not supported."
#endif
  *shm_size = size;
  return PP_TRUE;
}

PP_Context3DTrustedState GetErrorState() {
  PP_Context3DTrustedState error_state = { 0 };
  error_state.error = kGenericError;
  return error_state;
}

PP_Context3DTrustedState PPStateFromGPUState(
    const gpu::CommandBuffer::State& s) {
  PP_Context3DTrustedState state = {
      s.num_entries,
      s.get_offset,
      s.put_offset,
      s.token,
      static_cast<PPB_Context3DTrustedError>(s.error),
      s.generation
  };
  return state;
}

}  // namespace

PPB_Context3D_Impl::PPB_Context3D_Impl(PP_Instance instance)
    : Resource(instance),
      transfer_buffer_id_(0),
      draw_surface_(NULL),
      read_surface_(NULL),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Context3D_Impl::~PPB_Context3D_Impl() {
  Destroy();
}

// static
PP_Resource PPB_Context3D_Impl::Create(PP_Instance instance,
                                       PP_Config3D_Dev config,
                                       PP_Resource share_context,
                                       const int32_t* attrib_list) {
  // TODO(alokp): Support shared context.
  DCHECK_EQ(0, share_context);
  if (share_context != 0)
    return 0;

  scoped_refptr<PPB_Context3D_Impl> context(new PPB_Context3D_Impl(instance));
  if (!context->Init(config, share_context, attrib_list))
    return 0;

  return context->GetReference();
}

// static
PP_Resource PPB_Context3D_Impl::CreateRaw(PP_Instance instance,
                                          PP_Config3D_Dev config,
                                          PP_Resource share_context,
                                          const int32_t* attrib_list) {
  // TODO(alokp): Support shared context.
  DCHECK_EQ(0, share_context);
  if (share_context != 0)
    return 0;

  scoped_refptr<PPB_Context3D_Impl> context(new PPB_Context3D_Impl(instance));
  if (!context->InitRaw(config, share_context, attrib_list))
    return 0;

  return context->GetReference();
}

PPB_Context3D_API* PPB_Context3D_Impl::AsPPB_Context3D_API() {
  return this;
}

int32_t PPB_Context3D_Impl::GetAttrib(int32_t attribute, int32_t* value) {
  // TODO(alokp): Implement me.
  return 0;
}

int32_t PPB_Context3D_Impl::BindSurfaces(PP_Resource draw, PP_Resource read) {
  EnterResourceNoLock<PPB_Surface3D_API> enter_draw(draw, true);
  if (enter_draw.failed())
    return PP_ERROR_BADRESOURCE;
  PPB_Surface3D_Impl* new_draw =
      static_cast<PPB_Surface3D_Impl*>(enter_draw.object());

  EnterResourceNoLock<PPB_Surface3D_API> enter_read(read, true);
  if (enter_read.failed())
    return PP_ERROR_BADRESOURCE;
  PPB_Surface3D_Impl* new_read =
      static_cast<PPB_Surface3D_Impl*>(enter_read.object());
  return BindSurfacesImpl(new_draw, new_read);
}

int32_t PPB_Context3D_Impl::BindSurfacesImpl(PPB_Surface3D_Impl* new_draw,
                                             PPB_Surface3D_Impl* new_read) {
  // TODO(alokp): Support separate draw-read surfaces.
  DCHECK_EQ(new_draw, new_read);
  if (new_draw != new_read)
    return PP_ERROR_NOTSUPPORTED;

  if (new_draw == draw_surface_)
    return PP_OK;

  if (new_draw && new_draw->context())
    return PP_ERROR_BADARGUMENT;  // Already bound.

  if (draw_surface_)
    draw_surface_->BindToContext(NULL);
  if (new_draw && !new_draw->BindToContext(this))
    return PP_ERROR_NOMEMORY;

  draw_surface_ = new_draw;
  read_surface_ = new_read;
  return PP_OK;
}

int32_t PPB_Context3D_Impl::GetBoundSurfaces(PP_Resource* draw,
                                             PP_Resource* read) {
  // TODO(alokp): Implement me.
  return 0;
}

PP_Bool PPB_Context3D_Impl::InitializeTrusted(int32_t size) {
  if (!platform_context_.get())
    return PP_FALSE;
  return PP_FromBool(platform_context_->GetCommandBuffer()->Initialize(size));
}

PP_Bool PPB_Context3D_Impl::GetRingBuffer(int* shm_handle,
                                          uint32_t* shm_size) {
  if (!platform_context_.get())
    return PP_FALSE;
  gpu::Buffer buffer = platform_context_->GetCommandBuffer()->GetRingBuffer();
  return ShmToHandle(buffer.shared_memory, buffer.size, shm_handle, shm_size);
}

PP_Context3DTrustedState PPB_Context3D_Impl::GetState() {
  if (!platform_context_.get())
    return GetErrorState();
  return PPStateFromGPUState(platform_context_->GetCommandBuffer()->GetState());
}

PP_Bool PPB_Context3D_Impl::Flush(int32_t put_offset) {
  if (!platform_context_.get())
    return PP_FALSE;
  platform_context_->GetCommandBuffer()->Flush(put_offset);
  return PP_TRUE;
}

PP_Context3DTrustedState PPB_Context3D_Impl::FlushSync(int32_t put_offset) {
  if (!platform_context_.get())
    return GetErrorState();
  gpu::CommandBuffer::State state =
      platform_context_->GetCommandBuffer()->GetState();
  return PPStateFromGPUState(
      platform_context_->GetCommandBuffer()->FlushSync(put_offset,
                                                       state.get_offset));
}

int32_t PPB_Context3D_Impl::CreateTransferBuffer(uint32_t size) {
  if (!platform_context_.get())
    return 0;
  return platform_context_->GetCommandBuffer()->CreateTransferBuffer(size, -1);
}

PP_Bool PPB_Context3D_Impl::DestroyTransferBuffer(int32_t id) {
  if (!platform_context_.get())
    return PP_FALSE;
  platform_context_->GetCommandBuffer()->DestroyTransferBuffer(id);
  return PP_TRUE;
}

PP_Bool PPB_Context3D_Impl::GetTransferBuffer(int32_t id,
                                              int* shm_handle,
                                              uint32_t* shm_size) {
  if (!platform_context_.get())
    return PP_FALSE;
  gpu::Buffer buffer =
      platform_context_->GetCommandBuffer()->GetTransferBuffer(id);
  return ShmToHandle(buffer.shared_memory, buffer.size, shm_handle, shm_size);
}

PP_Context3DTrustedState PPB_Context3D_Impl::FlushSyncFast(
    int32_t put_offset,
    int32_t last_known_get) {
  if (!platform_context_.get())
    return GetErrorState();
  return PPStateFromGPUState(
      platform_context_->GetCommandBuffer()->FlushSync(put_offset,
                                                       last_known_get));
}

void* PPB_Context3D_Impl::MapTexSubImage2DCHROMIUM(GLenum target,
                                                   GLint level,
                                                   GLint xoffset,
                                                   GLint yoffset,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLenum format,
                                                   GLenum type,
                                                   GLenum access) {
  if (!gles2_impl_.get())
    return NULL;
  return gles2_impl_->MapTexSubImage2DCHROMIUM(
      target, level, xoffset, yoffset, width, height, format, type, access);
}

void PPB_Context3D_Impl::UnmapTexSubImage2DCHROMIUM(const void* mem) {
  if (gles2_impl_.get())
    gles2_impl_->UnmapTexSubImage2DCHROMIUM(mem);
}

gpu::gles2::GLES2Implementation* PPB_Context3D_Impl::GetGLES2Impl() {
  return gles2_impl();
}

bool PPB_Context3D_Impl::Init(PP_Config3D_Dev config,
                              PP_Resource share_context,
                              const int32_t* attrib_list) {
  if (!InitRaw(config, share_context, attrib_list))
    return false;

  if (!CreateImplementation()) {
    Destroy();
    return false;
  }

  return true;
}

bool PPB_Context3D_Impl::InitRaw(PP_Config3D_Dev config,
                                 PP_Resource share_context,
                                 const int32_t* attrib_list) {
  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return false;

  // Create and initialize the objects required to issue GLES2 calls.
  platform_context_.reset(plugin_instance->CreateContext3D());
  if (!platform_context_.get()) {
    Destroy();
    return false;
  }

  static const int32 kAttribs[] = {
    PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 8,
    PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
    PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 8,
    PP_GRAPHICS3DATTRIB_SAMPLES, 0,
    PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
    PP_GRAPHICS3DATTRIB_HEIGHT, 1,
    PP_GRAPHICS3DATTRIB_WIDTH, 1,
    PP_GRAPHICS3DATTRIB_NONE,
  };
  if (!platform_context_->Init(kAttribs)) {
    Destroy();
    return false;
  }

  platform_context_->SetContextLostCallback(
      callback_factory_.NewCallback(&PPB_Context3D_Impl::OnContextLost));
  return true;
}

bool PPB_Context3D_Impl::CreateImplementation() {
  gpu::CommandBuffer* command_buffer = platform_context_->GetCommandBuffer();
  DCHECK(command_buffer);

  if (!command_buffer->Initialize(kCommandBufferSize))
    return false;

  // Create the GLES2 helper, which writes the command buffer protocol.
  helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer));
  if (!helper_->Initialize(kCommandBufferSize))
    return false;

  // Create a transfer buffer used to copy resources between the renderer
  // process and the GPU process.
  transfer_buffer_id_ =
      command_buffer->CreateTransferBuffer(kTransferBufferSize, -1);
  if (transfer_buffer_id_ < 0)
    return false;

  // Map the buffer into the renderer process's address space.
  gpu::Buffer transfer_buffer =
      command_buffer->GetTransferBuffer(transfer_buffer_id_);
  if (!transfer_buffer.ptr)
    return false;

  // Create the object exposing the OpenGL API.
  gles2_impl_.reset(new gpu::gles2::GLES2Implementation(
      helper_.get(),
      transfer_buffer.size,
      transfer_buffer.ptr,
      transfer_buffer_id_,
      false,
      true));

  return true;
}

void PPB_Context3D_Impl::Destroy() {
  if (draw_surface_)
    draw_surface_->BindToContext(NULL);

  gles2_impl_.reset();

  if (platform_context_.get() && transfer_buffer_id_ != 0) {
    platform_context_->GetCommandBuffer()->DestroyTransferBuffer(
        transfer_buffer_id_);
    transfer_buffer_id_ = 0;
  }

  helper_.reset();
  platform_context_.reset();
}

void PPB_Context3D_Impl::OnContextLost() {
  if (draw_surface_)
    draw_surface_->OnContextLost();
  if (read_surface_)
    read_surface_->OnContextLost();
}

}  // namespace ppapi
}  // namespace webkit
