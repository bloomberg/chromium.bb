// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_graphics_3d_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/thunk/enter.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "webkit/plugins/plugin_switches.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics3D_API;
using WebKit::WebConsoleMessage;
using WebKit::WebFrame;
using WebKit::WebPluginContainer;
using WebKit::WebString;

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
    : PPB_Graphics3D_Shared(instance),
      bound_to_instance_(false),
      commit_pending_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Graphics3D_Impl::~PPB_Graphics3D_Impl() {
  DestroyGLES2Impl();
}

// static
PP_Bool PPB_Graphics3D_Impl::IsGpuBlacklisted() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line)
    return PP_FromBool(
        command_line->HasSwitch(switches::kDisablePepper3d));
  return PP_TRUE;
}

// static
PP_Resource PPB_Graphics3D_Impl::Create(PP_Instance instance,
                                        PP_Resource share_context,
                                        const int32_t* attrib_list) {
  PPB_Graphics3D_API* share_api = NULL;
  if (IsGpuBlacklisted())
    return 0;
  if (share_context) {
    EnterResourceNoLock<PPB_Graphics3D_API> enter(share_context, true);
    if (enter.failed())
      return 0;
    share_api = enter.object();
  }
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d(
      new PPB_Graphics3D_Impl(instance));
  if (!graphics_3d->Init(share_api, attrib_list))
    return 0;
  return graphics_3d->GetReference();
}

// static
PP_Resource PPB_Graphics3D_Impl::CreateRaw(PP_Instance instance,
                                           PP_Resource share_context,
                                           const int32_t* attrib_list) {
  PPB_Graphics3D_API* share_api = NULL;
  if (IsGpuBlacklisted())
    return 0;
  if (share_context) {
    EnterResourceNoLock<PPB_Graphics3D_API> enter(share_context, true);
    if (enter.failed())
      return 0;
    share_api = enter.object();
  }
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d(
      new PPB_Graphics3D_Impl(instance));
  if (!graphics_3d->InitRaw(share_api, attrib_list))
    return 0;
  return graphics_3d->GetReference();
}

PP_Bool PPB_Graphics3D_Impl::InitCommandBuffer() {
  return PP_FromBool(GetCommandBuffer()->Initialize());
}

PP_Bool PPB_Graphics3D_Impl::SetGetBuffer(int32_t transfer_buffer_id) {
  GetCommandBuffer()->SetGetBuffer(transfer_buffer_id);
  return PP_TRUE;
}

PP_Graphics3DTrustedState PPB_Graphics3D_Impl::GetState() {
  return PPStateFromGPUState(GetCommandBuffer()->GetState());
}

int32_t PPB_Graphics3D_Impl::CreateTransferBuffer(uint32_t size) {
  int32_t id = -1;
  GetCommandBuffer()->CreateTransferBuffer(size, &id);
  return id;
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

bool PPB_Graphics3D_Impl::IsOpaque() {
  return platform_context_->IsOpaque();
}

void PPB_Graphics3D_Impl::ViewWillInitiatePaint() {
}

void PPB_Graphics3D_Impl::ViewInitiatedPaint() {
  commit_pending_ = false;

  if (HasPendingSwap())
    SwapBuffersACK(PP_OK);
}

void PPB_Graphics3D_Impl::ViewFlushedPaint() {
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

bool PPB_Graphics3D_Impl::Init(PPB_Graphics3D_API* share_context,
                               const int32_t* attrib_list) {
  if (!InitRaw(share_context, attrib_list))
    return false;

  gpu::CommandBuffer* command_buffer = GetCommandBuffer();
  if (!command_buffer->Initialize())
    return false;

  gpu::gles2::GLES2Implementation* share_gles2 = NULL;
  if (share_context) {
    share_gles2 =
        static_cast<PPB_Graphics3D_Shared*>(share_context)->gles2_impl();
  }

  return CreateGLES2Impl(kCommandBufferSize, kTransferBufferSize,
                         share_gles2);
}

bool PPB_Graphics3D_Impl::InitRaw(PPB_Graphics3D_API* share_context,
                                  const int32_t* attrib_list) {
  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return false;

  PluginDelegate::PlatformContext3D* share_platform_context = NULL;
  if (share_context) {
    PPB_Graphics3D_Impl* share_graphics =
        static_cast<PPB_Graphics3D_Impl*>(share_context);
    share_platform_context = share_graphics->platform_context();
  }

  platform_context_.reset(plugin_instance->CreateContext3D());
  if (!platform_context_.get())
    return false;

  if (!platform_context_->Init(attrib_list, share_platform_context))
    return false;

  platform_context_->SetContextLostCallback(
      base::Bind(&PPB_Graphics3D_Impl::OnContextLost,
                 weak_ptr_factory_.GetWeakPtr()));

  platform_context_->SetOnConsoleMessageCallback(
      base::Bind(&PPB_Graphics3D_Impl::OnConsoleMessage,
                 weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void PPB_Graphics3D_Impl::OnConsoleMessage(const std::string& message,
                                           int id) {
  if (!bound_to_instance_)
    return;
  WebPluginContainer* container =
      ResourceHelper::GetPluginInstance(this)->container();
  if (!container)
    return;
  WebFrame* frame = container->element().document().frame();
  if (!frame)
    return;
  WebConsoleMessage console_message = WebConsoleMessage(
      WebConsoleMessage::LevelError, WebString(UTF8ToUTF16(message)));
  frame->addMessageToConsole(console_message);
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
