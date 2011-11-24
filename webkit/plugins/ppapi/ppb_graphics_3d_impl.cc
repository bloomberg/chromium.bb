// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_graphics_3d_impl.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::thunk::PPB_Graphics3D_API;

namespace webkit {
namespace ppapi {

namespace {
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

PP_Graphics3DTrustedState PPStateFromGPUState(
    const gpu::CommandBuffer::State& s) {
  PP_Graphics3DTrustedState state = {
      s.num_entries,
      s.get_offset,
      s.put_offset,
      s.token,
      static_cast<PPB_Graphics3DTrustedError>(s.error),
      s.generation
  };
  return state;
}
}  // namespace.

PPB_Graphics3D_Impl::PPB_Graphics3D_Impl(PP_Instance instance)
    : Resource(instance),
      bound_to_instance_(false),
      commit_pending_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Graphics3D_Impl::~PPB_Graphics3D_Impl() {
  DestroyGLES2Impl();
}

// static
PP_Resource PPB_Graphics3D_Impl::Create(PP_Instance instance,
                                        PP_Resource share_context,
                                        const int32_t* attrib_list) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d(
      new PPB_Graphics3D_Impl(instance));
  if (!graphics_3d->Init(share_context, attrib_list))
    return 0;
  return graphics_3d->GetReference();
}

PP_Resource PPB_Graphics3D_Impl::CreateRaw(PP_Instance instance,
                                           PP_Resource share_context,
                                           const int32_t* attrib_list) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d(
      new PPB_Graphics3D_Impl(instance));
  if (!graphics_3d->InitRaw(share_context, attrib_list))
    return 0;
  return graphics_3d->GetReference();
}

PPB_Graphics3D_API* PPB_Graphics3D_Impl::AsPPB_Graphics3D_API() {
  return this;
}

PP_Bool PPB_Graphics3D_Impl::InitCommandBuffer(int32_t size) {
  return PP_FromBool(GetCommandBuffer()->Initialize(size));
}

PP_Bool PPB_Graphics3D_Impl::GetRingBuffer(int* shm_handle,
                                           uint32_t* shm_size) {
  gpu::Buffer buffer = GetCommandBuffer()->GetRingBuffer();
  return ShmToHandle(buffer.shared_memory, buffer.size, shm_handle, shm_size);
}

PP_Graphics3DTrustedState PPB_Graphics3D_Impl::GetState() {
  return PPStateFromGPUState(GetCommandBuffer()->GetState());
}

int32_t PPB_Graphics3D_Impl::CreateTransferBuffer(uint32_t size) {
  return GetCommandBuffer()->CreateTransferBuffer(size, -1);
}

PP_Bool PPB_Graphics3D_Impl::DestroyTransferBuffer(int32_t id) {
  GetCommandBuffer()->DestroyTransferBuffer(id);
  return PP_TRUE;
}

PP_Bool PPB_Graphics3D_Impl::GetTransferBuffer(int32_t id,
                                               int* shm_handle,
                                               uint32_t* shm_size) {
  gpu::Buffer buffer = GetCommandBuffer()->GetTransferBuffer(id);
  return ShmToHandle(buffer.shared_memory, buffer.size, shm_handle, shm_size);
}

PP_Bool PPB_Graphics3D_Impl::Flush(int32_t put_offset) {
  GetCommandBuffer()->Flush(put_offset);
  return PP_TRUE;
}

PP_Graphics3DTrustedState PPB_Graphics3D_Impl::FlushSync(int32_t put_offset) {
  gpu::CommandBuffer::State state = GetCommandBuffer()->GetState();
  return PPStateFromGPUState(
      GetCommandBuffer()->FlushSync(put_offset, state.get_offset));
}

PP_Graphics3DTrustedState PPB_Graphics3D_Impl::FlushSyncFast(
    int32_t put_offset,
    int32_t last_known_get) {
  return PPStateFromGPUState(
      GetCommandBuffer()->FlushSync(put_offset, last_known_get));
}

bool PPB_Graphics3D_Impl::BindToInstance(bool bind) {
  bound_to_instance_ = bind;
  return true;
}

unsigned int PPB_Graphics3D_Impl::GetBackingTextureId() {
  return platform_context_->GetBackingTextureId();
}

void PPB_Graphics3D_Impl::ViewInitiatedPaint() {
}

void PPB_Graphics3D_Impl::ViewFlushedPaint() {
  commit_pending_ = false;

  if (HasPendingSwap())
    SwapBuffersACK(PP_OK);
}

gpu::CommandBuffer* PPB_Graphics3D_Impl::GetCommandBuffer() {
  return platform_context_->GetCommandBuffer();
}

int32 PPB_Graphics3D_Impl::DoSwapBuffers() {
  // We do not have a GLES2 implementation when using an OOP proxy.
  // The plugin-side proxy is responsible for adding the SwapBuffers command
  // to the command buffer in that case.
  if (gles2_impl())
    gles2_impl()->SwapBuffers();

  if (bound_to_instance_) {
    // If we are bound to the instance, we need to ask the compositor
    // to commit our backing texture so that the graphics appears on the page.
    // When the backing texture will be committed we get notified via
    // ViewFlushedPaint().
    //
    // Don't need to check for NULL from GetPluginInstance since when we're
    // bound, we know our instance is valid.
    ResourceHelper::GetPluginInstance(this)->CommitBackingTexture();
    commit_pending_ = true;
  } else {
    // Wait for the command to complete on the GPU to allow for throttling.
    platform_context_->Echo(base::Bind(&PPB_Graphics3D_Impl::OnSwapBuffers,
                                       weak_ptr_factory_.GetWeakPtr()));
  }


  return PP_OK_COMPLETIONPENDING;
}

bool PPB_Graphics3D_Impl::Init(PP_Resource share_context,
                               const int32_t* attrib_list) {
  if (!InitRaw(share_context, attrib_list))
    return false;

  gpu::CommandBuffer* command_buffer = GetCommandBuffer();
  if (!command_buffer->Initialize(kCommandBufferSize))
    return false;

  return CreateGLES2Impl(kCommandBufferSize, kTransferBufferSize);
}

bool PPB_Graphics3D_Impl::InitRaw(PP_Resource share_context,
                                  const int32_t* attrib_list) {
  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return false;

  // TODO(alokp): Support shared context.
  DCHECK_EQ(0, share_context);
  if (share_context != 0)
    return false;

  platform_context_.reset(plugin_instance->CreateContext3D());
  if (!platform_context_.get())
    return false;

  if (!platform_context_->Init(attrib_list))
    return false;

  platform_context_->SetContextLostCallback(
      base::Bind(&PPB_Graphics3D_Impl::OnContextLost,
                 weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void PPB_Graphics3D_Impl::OnSwapBuffers() {
  if (HasPendingSwap()) {
    // If we're off-screen, no need to trigger and wait for compositing.
    // Just send the swap-buffers ACK to the plugin immediately.
    commit_pending_ = false;
    SwapBuffersACK(PP_OK);
  }
}

void PPB_Graphics3D_Impl::OnContextLost() {
  // Don't need to check for NULL from GetPluginInstance since when we're
  // bound, we know our instance is valid.
  if (bound_to_instance_)
    ResourceHelper::GetPluginInstance(this)->BindGraphics(pp_instance(), 0);

  // Send context lost to plugin. This may have been caused by a PPAPI call, so
  // avoid re-entering.
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &PPB_Graphics3D_Impl::SendContextLost, weak_ptr_factory_.GetWeakPtr()));
}

void PPB_Graphics3D_Impl::SendContextLost() {
  // By the time we run this, the instance may have been deleted, or in the
  // process of being deleted. Even in the latter case, we don't want to send a
  // callback after DidDestroy.
  PluginInstance* instance = ResourceHelper::GetPluginInstance(this);
  if (!instance || !instance->container())
    return;

  const PPP_Graphics3D* ppp_graphics_3d =
      static_cast<const PPP_Graphics3D*>(
          instance->module()->GetPluginInterface(
              PPP_GRAPHICS_3D_INTERFACE));
  if (ppp_graphics_3d)
    ppp_graphics_3d->Graphics3DContextLost(pp_instance());
}

}  // namespace ppapi
}  // namespace webkit
