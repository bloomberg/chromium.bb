/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the implementation of the command buffer Renderer.

#include "core/cross/precompile.h"

#include "gpu/command_buffer/client/o3d_cmd_helper.h"
#include "gpu/command_buffer/client/fenced_allocator.h"
#include "gpu/command_buffer/common/gapi_interface.h"
#include "core/cross/command_buffer/buffer_cb.h"
#include "core/cross/command_buffer/effect_cb.h"
#include "core/cross/command_buffer/param_cache_cb.h"
#include "core/cross/command_buffer/primitive_cb.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "core/cross/command_buffer/render_surface_cb.h"
#include "core/cross/command_buffer/sampler_cb.h"
#include "core/cross/command_buffer/states_cb.h"
#include "core/cross/command_buffer/stream_bank_cb.h"
#include "core/cross/command_buffer/texture_cb.h"
#include "core/cross/command_buffer/display_window_cb.h"
#include "core/cross/renderer_platform.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gpu_processor.h"
#include "gpu/np_utils/np_browser.h"
#include "gpu/np_utils/np_utils.h"

namespace o3d {
using ::base::SharedMemory;
using ::command_buffer::CommandBufferService;
using command_buffer::o3d::GAPIInterface;
using command_buffer::O3DCmdHelper;
using np_utils::NPBrowser;
using np_utils::NPCreateObject;
using np_utils::NPGetProperty;
using np_utils::NPInvoke;
using np_utils::NPInvokeVoid;
using np_utils::NPObjectPointer;

#if !defined(CB_SERVICE_REMOTE)
using command_buffer::CommandBuffer;
using command_buffer::GPUProcessor;
#endif

RendererCB::RendererCB(ServiceLocator* service_locator,
                       int32 transfer_memory_size)
    : Renderer(service_locator),
      transfer_memory_size_(transfer_memory_size),
      transfer_shm_id_(command_buffer::kInvalidSharedMemoryId),
      transfer_shm_address_(NULL),
      npp_(NULL),
      helper_(NULL),
      allocator_(NULL),
      frame_token_(0),
      state_manager_(new StateManager) {
  DCHECK_GT(transfer_memory_size, 0);
  state_manager_->AddStateHandlers(this);
}

RendererCB::~RendererCB() {
  Destroy();
}

void RendererCB::Destroy() {
  if (command_buffer_.get()) {
    command_buffer_->DestroyTransferBuffer(transfer_shm_id_);
  }

  transfer_shm_id_ = command_buffer::kInvalidSharedMemoryId;
  transfer_shm_ = NULL;
  transfer_shm_address_ = NULL;

  if (allocator_) {
    delete allocator_;
    allocator_ = NULL;
  }

  if (helper_) {
    helper_->Finish();
    delete helper_;
    helper_ = NULL;
  }

  npp_ = NULL;
}

void RendererCB::ApplyDirtyStates() {
  state_manager_->ValidateStates(helper_);
}

void RendererCB::Resize(int width, int height) {
  // TODO: the Resize event won't be coming to the client, but
  // potentially to the server, so that function doesn't directly make sense
  // in the command buffer implementation.
  SetClientSize(width, height);
}

bool RendererCB::PlatformSpecificBeginDraw() {
  return true;
}

// Adds the CLEAR command to the command buffer.
void RendererCB::PlatformSpecificClear(const Float4 &color,
                                       bool color_flag,
                                       float depth,
                                       bool depth_flag,
                                       int stencil,
                                       bool stencil_flag) {
  uint32 buffers = (color_flag ? command_buffer::o3d::kColor : 0) |
                   (depth_flag ? command_buffer::o3d::kDepth : 0) |
                   (stencil_flag ? command_buffer::o3d::kStencil : 0);
  helper_->Clear(buffers, color[0], color[1], color[2], color[3],
                 depth, stencil);
}

void RendererCB::PlatformSpecificEndDraw() {
}

// Adds the BeginFrame command to the command buffer.
bool RendererCB::PlatformSpecificStartRendering() {
  // Any device issues are handled in the command buffer backend
  DCHECK(helper_);
  helper_->BeginFrame();
  return true;
}

// Adds the EndFrame command to the command buffer, and flushes the commands.
void RendererCB::PlatformSpecificFinishRendering() {
  // Any device issues are handled in the command buffer backend
  helper_->EndFrame();
  helper_->WaitForToken(frame_token_);
  frame_token_ = helper_->InsertToken();
}

void RendererCB::PlatformSpecificPresent() {
  // TODO(gman): The EndFrame command needs to be split into EndFrame
  //     and PRESENT.
}

// Assign the surface arguments to the renderer, and update the stack
// of pushed surfaces.
void RendererCB::SetRenderSurfacesPlatformSpecific(
    const RenderSurface* surface,
    const RenderDepthStencilSurface* surface_depth) {
  const RenderSurfaceCB* surface_cb =
      down_cast<const RenderSurfaceCB*>(surface);
  const RenderDepthStencilSurfaceCB* surface_depth_cb =
      down_cast<const RenderDepthStencilSurfaceCB*>(surface_depth);
  helper_->SetRenderSurface(
      surface_cb->resource_id(),
      surface_depth_cb->resource_id());
}

void RendererCB::SetBackBufferPlatformSpecific() {
  helper_->SetBackSurfaces();
}

// Creates a StreamBank, returning a platform specific implementation class.
StreamBank::Ref RendererCB::CreateStreamBank() {
  return StreamBank::Ref(new StreamBankCB(service_locator(), this));
}

// Creates a Primitive, returning a platform specific implementation class.
Primitive::Ref RendererCB::CreatePrimitive() {
  return Primitive::Ref(new PrimitiveCB(service_locator(), this));
}

// Creates a DrawElement, returning a platform specific implementation
// class.
DrawElement::Ref RendererCB::CreateDrawElement() {
  return DrawElement::Ref(new DrawElement(service_locator()));
}

Sampler::Ref RendererCB::CreateSampler() {
  return SamplerCB::Ref(new SamplerCB(service_locator(), this));
}

// Creates and returns a platform-specific float buffer
VertexBuffer::Ref RendererCB::CreateVertexBuffer() {
  return VertexBuffer::Ref(new VertexBufferCB(service_locator(), this));
}

// Creates and returns a platform-specific integer buffer
IndexBuffer::Ref RendererCB::CreateIndexBuffer() {
  return IndexBuffer::Ref(new IndexBufferCB(service_locator(), this));
}

// Creates and returns a platform-specific effect object
Effect::Ref RendererCB::CreateEffect() {
  return Effect::Ref(new EffectCB(service_locator(), this));
}

// Creates and returns a platform-specific Texture2D object.  It allocates
// the necessary resources to store texture data for the given image size
// and format.
Texture2D::Ref RendererCB::CreatePlatformSpecificTexture2D(
    int width,
    int height,
    Texture::Format format,
    int levels,
    bool enable_render_surfaces) {
  return Texture2D::Ref(Texture2DCB::Create(service_locator(), format, levels,
                                            width, height,
                                            enable_render_surfaces));
}

// Creates and returns a platform-specific Texture2D object.  It allocates
// the necessary resources to store texture data for the given image size
// and format.
TextureCUBE::Ref RendererCB::CreatePlatformSpecificTextureCUBE(
    int edge,
    Texture::Format format,
    int levels,
    bool enable_render_surfaces) {
  return TextureCUBE::Ref(TextureCUBECB::Create(service_locator(), format,
                                                levels, edge,
                                                enable_render_surfaces));
}

// Creates a platform specific ParamCache.
ParamCache* RendererCB::CreatePlatformSpecificParamCache() {
  return new ParamCacheCB();
}

void RendererCB::SetViewportInPixels(int left,
                                     int top,
                                     int width,
                                     int height,
                                     float min_z,
                                     float max_z) {
  helper_->SetViewport(left, top, width, height, min_z, max_z);
}

const int* RendererCB::GetRGBAUByteNSwizzleTable() {
  static int swizzle_table[] = { 0, 1, 2, 3, };
  return swizzle_table;
}

command_buffer::parse_error::ParseError RendererCB::GetParseError() {
  return helper_->GetParseError();
}

// Creates and returns a platform specific RenderDepthStencilSurface object.
RenderDepthStencilSurface::Ref RendererCB::CreateDepthStencilSurface(
    int width,
    int height) {
  return RenderDepthStencilSurface::Ref(
      new RenderDepthStencilSurfaceCB(service_locator(),
                                      width,
                                      height,
                                      this));
}

Renderer::InitStatus RendererCB::InitPlatformSpecific(
    const DisplayWindow& display_window,
    bool off_screen) {
  if (off_screen) {
    // TODO: Off-screen support ?
    return UNINITIALIZED;  // equivalent to 0/false
  }

  const DisplayWindowCB& display_platform =
      static_cast<const DisplayWindowCB&>(display_window);

  command_buffer_.reset(display_platform.command_buffer());
  DCHECK(command_buffer_.get());

  // Create and initialize a O3DCmdHelper.
  helper_ = new O3DCmdHelper(command_buffer_.get());
  if (!helper_->Initialize()) {
    Destroy();
    return INITIALIZATION_ERROR;
  }

  transfer_shm_id_ = command_buffer_->CreateTransferBuffer(
      transfer_memory_size_);
  if (transfer_shm_id_ < 0) {
    Destroy();
    return INITIALIZATION_ERROR;
  }

  transfer_shm_ = command_buffer_->GetTransferBuffer(transfer_shm_id_);
  if (!transfer_shm_) {
    Destroy();
    return INITIALIZATION_ERROR;
  }

  if (!transfer_shm_->Map(transfer_memory_size_)) {
    Destroy();
    return INITIALIZATION_ERROR;
  }

  transfer_shm_address_ = transfer_shm_->memory();

  // Insert a token.
  frame_token_ = helper_->InsertToken();
  if (frame_token_ < 0) {
    Destroy();
    return INITIALIZATION_ERROR;
  }

  // Create a fenced allocator.
  allocator_ = new FencedAllocatorWrapper(transfer_memory_size_,
                                          helper_,
                                          transfer_shm_address_);

  SetClientSize(display_platform.width(), display_platform.height());

  return SUCCESS;
}

// This should be enough to hold the biggest possible buffer
// (2048x2048xABGR16F texture = 32MB)
static const int32 kDefaultTransferMemorySize = 32 << 20;

#if defined(CB_SERVICE_REMOTE)

RendererCBRemote *RendererCBRemote::CreateDefault(
    ServiceLocator* service_locator) {
  return new RendererCBRemote(service_locator,
                              kDefaultTransferMemorySize);
}

RendererCBRemote::RendererCBRemote(ServiceLocator* service_locator,
                                   int32 transfer_memory_size)
    : RendererCB(service_locator, transfer_memory_size) {
}

RendererCBRemote::~RendererCBRemote() {
}

Renderer* Renderer::CreateDefaultRenderer(ServiceLocator* service_locator) {
  return RendererCBRemote::CreateDefault(service_locator);
}

#else  // CB_SERVICE_REMOTE

static const unsigned int kDefaultCommandBufferSize = 256 << 10;

RendererCBLocal *RendererCBLocal::CreateDefault(
    ServiceLocator* service_locator) {
  return new RendererCBLocal(service_locator,
                             kDefaultTransferMemorySize);
}

RendererCBLocal::RendererCBLocal(ServiceLocator* service_locator,
                                 int32 transfer_memory_size)
    : RendererCB(service_locator, transfer_memory_size) {
}

RendererCBLocal::~RendererCBLocal() {
}

CommandBuffer* RendererCBLocal::CreateCommandBuffer(NPP npp,
                                                    void* hwnd,
                                                    int32 size) {
#if defined(OS_WIN)
  scoped_ptr<SharedMemory> ring_buffer(new SharedMemory);
  if (!ring_buffer->Create(std::wstring(), false, false, size))
    return NULL;

  if (!ring_buffer->Map(size))
    return NULL;

  scoped_ptr<CommandBufferService> command_buffer(new CommandBufferService);
  if (!command_buffer->Initialize(ring_buffer.release()))
    return NULL;

  scoped_refptr<GPUProcessor> gpu_processor(
      new GPUProcessor(npp, command_buffer.get()));
  if (!gpu_processor->Initialize(reinterpret_cast<HWND>(hwnd)))
    return NULL;

  command_buffer->SetPutOffsetChangeCallback(
      NewCallback(gpu_processor.get(), &GPUProcessor::ProcessCommands));

  return command_buffer.release();

#else
  return NULL;
#endif
}

Renderer* Renderer::CreateDefaultRenderer(ServiceLocator* service_locator) {
  return RendererCBLocal::CreateDefault(service_locator);
}

#endif  // CB_SERVICE_REMOTE

}  // namespace o3d
