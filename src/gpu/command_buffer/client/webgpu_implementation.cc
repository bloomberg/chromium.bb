// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/webgpu_implementation.h"

#include <vector>

#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"

#define GPU_CLIENT_SINGLE_THREAD_CHECK()

namespace gpu {
namespace webgpu {

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_implementation_impl_autogen.h"

WebGPUImplementation::WebGPUImplementation(
    WebGPUCmdHelper* helper,
    TransferBufferInterface* transfer_buffer,
    GpuControl* gpu_control)
    : ImplementationBase(helper, transfer_buffer, gpu_control),
      helper_(helper) {}

WebGPUImplementation::~WebGPUImplementation() {}

gpu::ContextResult WebGPUImplementation::Initialize(
    const SharedMemoryLimits& limits) {
  TRACE_EVENT0("gpu", "WebGPUImplementation::Initialize");
  return ImplementationBase::Initialize(limits);
}

// ContextSupport implementation.
void WebGPUImplementation::SetAggressivelyFreeResources(
    bool aggressively_free_resources) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::Swap(uint32_t flags,
                                SwapCompletedCallback complete_callback,
                                PresentationCallback presentation_callback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::SwapWithBounds(
    const std::vector<gfx::Rect>& rects,
    uint32_t flags,
    SwapCompletedCallback swap_completed,
    PresentationCallback presentation_callback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::PartialSwapBuffers(
    const gfx::Rect& sub_buffer,
    uint32_t flags,
    SwapCompletedCallback swap_completed,
    PresentationCallback presentation_callback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::CommitOverlayPlanes(
    uint32_t flags,
    SwapCompletedCallback swap_completed,
    PresentationCallback presentation_callback) {
  NOTREACHED();
}
void WebGPUImplementation::ScheduleOverlayPlane(
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    unsigned overlay_texture_id,
    const gfx::Rect& display_bounds,
    const gfx::RectF& uv_rect,
    bool enable_blend,
    unsigned gpu_fence_id) {
  NOTREACHED();
}
uint64_t WebGPUImplementation::ShareGroupTracingGUID() const {
  NOTIMPLEMENTED();
  return 0;
}
void WebGPUImplementation::SetErrorMessageCallback(
    base::RepeatingCallback<void(const char*, int32_t)> callback) {
  NOTIMPLEMENTED();
}
bool WebGPUImplementation::ThreadSafeShallowLockDiscardableTexture(
    uint32_t texture_id) {
  NOTREACHED();
  return false;
}
void WebGPUImplementation::CompleteLockDiscardableTexureOnContextThread(
    uint32_t texture_id) {
  NOTREACHED();
}
bool WebGPUImplementation::ThreadsafeDiscardableTextureIsDeletedForTracing(
    uint32_t texture_id) {
  NOTREACHED();
  return false;
}
void* WebGPUImplementation::MapTransferCacheEntry(uint32_t serialized_size) {
  NOTREACHED();
  return nullptr;
}
void WebGPUImplementation::UnmapAndCreateTransferCacheEntry(uint32_t type,
                                                            uint32_t id) {
  NOTREACHED();
}
bool WebGPUImplementation::ThreadsafeLockTransferCacheEntry(uint32_t type,
                                                            uint32_t id) {
  NOTREACHED();
  return false;
}
void WebGPUImplementation::UnlockTransferCacheEntries(
    const std::vector<std::pair<uint32_t, uint32_t>>& entries) {
  NOTREACHED();
}
void WebGPUImplementation::DeleteTransferCacheEntry(uint32_t type,
                                                    uint32_t id) {
  NOTREACHED();
}
unsigned int WebGPUImplementation::GetTransferBufferFreeSize() const {
  NOTREACHED();
  return 0;
}

// ImplementationBase implementation.
void WebGPUImplementation::IssueShallowFlush() {
  NOTIMPLEMENTED();
}

// GpuControlClient implementation.
void WebGPUImplementation::OnGpuControlLostContext() {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlLostContextMaybeReentrant() {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlErrorMessage(const char* message,
                                                    int32_t id) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlSwapBuffersCompleted(
    const SwapBuffersCompleteParams& params) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnSwapBufferPresented(
    uint64_t swap_id,
    const gfx::PresentationFeedback& feedback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlReturnData(
    base::span<const uint8_t> data) {
  // TODO: Handle return commands
  NOTIMPLEMENTED();
}

void WebGPUImplementation::Dummy() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] wgDummy()");
  helper_->Dummy();
  helper_->Flush();
}

}  // namespace webgpu
}  // namespace gpu
