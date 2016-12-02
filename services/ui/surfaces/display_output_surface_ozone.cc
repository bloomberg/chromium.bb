// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/display_output_surface_ozone.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/output_surface_frame.h"
#include "cc/scheduler/begin_frame_source.h"
#include "components/display_compositor/buffer_queue.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/display/types/display_snapshot.h"

using display_compositor::BufferQueue;

namespace ui {

DisplayOutputSurfaceOzone::DisplayOutputSurfaceOzone(
    scoped_refptr<cc::InProcessContextProvider> context_provider,
    gfx::AcceleratedWidget widget,
    cc::SyntheticBeginFrameSource* synthetic_begin_frame_source,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    uint32_t target,
    uint32_t internalformat)
    : cc::OutputSurface(context_provider),
      gl_helper_(context_provider->ContextGL(),
                 context_provider->ContextSupport()),
      synthetic_begin_frame_source_(synthetic_begin_frame_source),
      weak_ptr_factory_(this) {
  buffer_queue_.reset(
      new BufferQueue(context_provider->ContextGL(), target, internalformat,
                      ui::DisplaySnapshot::PrimaryFormat(), &gl_helper_,
                      gpu_memory_buffer_manager, widget));

  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.flipped_output_surface = true;
  // Set |max_frames_pending| to 2 for surfaceless, which aligns scheduling
  // more closely with the previous surfaced behavior.
  // With a surface, swap buffer ack used to return early, before actually
  // presenting the back buffer, enabling the browser compositor to run ahead.
  // Surfaceless implementation acks at the time of actual buffer swap, which
  // shifts the start of the new frame forward relative to the old
  // implementation.
  capabilities_.max_frames_pending = 2;

  buffer_queue_->Initialize();

  context_provider->SetSwapBuffersCompletionCallback(
      base::Bind(&DisplayOutputSurfaceOzone::OnGpuSwapBuffersCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
  context_provider->SetUpdateVSyncParametersCallback(
      base::Bind(&DisplayOutputSurfaceOzone::OnVSyncParametersUpdated,
                 weak_ptr_factory_.GetWeakPtr()));
}

DisplayOutputSurfaceOzone::~DisplayOutputSurfaceOzone() {
  // TODO(rjkroege): Support cleanup.
}

void DisplayOutputSurfaceOzone::BindToClient(cc::OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void DisplayOutputSurfaceOzone::EnsureBackbuffer() {}

void DisplayOutputSurfaceOzone::DiscardBackbuffer() {
  context_provider()->ContextGL()->DiscardBackbufferCHROMIUM();
}

void DisplayOutputSurfaceOzone::BindFramebuffer() {
  DCHECK(buffer_queue_);
  buffer_queue_->BindFramebuffer();
}

// We call this on every frame that a value changes, but changing the size once
// we've allocated backing NativePixmapOzone instances will cause a DCHECK
// because Chrome never Reshape(s) after the first one from (0,0). NB: this
// implies that screen size changes need to be plumbed differently. In
// particular, we must create the native window in the size that the hardware
// reports.
void DisplayOutputSurfaceOzone::Reshape(const gfx::Size& size,
                                        float device_scale_factor,
                                        const gfx::ColorSpace& color_space,
                                        bool has_alpha) {
  reshape_size_ = size;
  context_provider()->ContextGL()->ResizeCHROMIUM(
      size.width(), size.height(), device_scale_factor, has_alpha);
  buffer_queue_->Reshape(size, device_scale_factor, color_space);
}

void DisplayOutputSurfaceOzone::SwapBuffers(cc::OutputSurfaceFrame frame) {
  DCHECK(buffer_queue_);

  // TODO(rjkroege): What if swap happens again before OnGpuSwapBuffersCompleted
  // then it would see the wrong size?
  DCHECK(reshape_size_ == frame.size);
  swap_size_ = reshape_size_;

  buffer_queue_->SwapBuffers(frame.sub_buffer_rect);

  // Code combining GpuBrowserCompositorOutputSurface + DisplayOutputSurface
  if (frame.sub_buffer_rect == gfx::Rect(frame.size)) {
    context_provider_->ContextSupport()->Swap();
  } else {
    context_provider_->ContextSupport()->PartialSwapBuffers(
        frame.sub_buffer_rect);
  }
}

uint32_t DisplayOutputSurfaceOzone::GetFramebufferCopyTextureFormat() {
  return buffer_queue_->internal_format();
}

cc::OverlayCandidateValidator*
DisplayOutputSurfaceOzone::GetOverlayCandidateValidator() const {
  return nullptr;
}

bool DisplayOutputSurfaceOzone::IsDisplayedAsOverlayPlane() const {
  // TODO(rjkroege): implement remaining overlay functionality.
  return true;
}

unsigned DisplayOutputSurfaceOzone::GetOverlayTextureId() const {
  return buffer_queue_->current_texture_id();
}

bool DisplayOutputSurfaceOzone::SurfaceIsSuspendForRecycle() const {
  return false;
}

bool DisplayOutputSurfaceOzone::HasExternalStencilTest() const {
  return false;
}

void DisplayOutputSurfaceOzone::ApplyExternalStencil() {}

void DisplayOutputSurfaceOzone::OnGpuSwapBuffersCompleted(
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::SwapResult result,
    const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) {
  bool force_swap = false;
  if (result == gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    // Even through the swap failed, this is a fixable error so we can pretend
    // it succeeded to the rest of the system.
    result = gfx::SwapResult::SWAP_ACK;
    buffer_queue_->RecreateBuffers();
    force_swap = true;
  }

  buffer_queue_->PageFlipComplete();
  client_->DidReceiveSwapBuffersAck();

  if (force_swap)
    client_->SetNeedsRedrawRect(gfx::Rect(swap_size_));
}

void DisplayOutputSurfaceOzone::OnVSyncParametersUpdated(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  // TODO(brianderson): We should not be receiving 0 intervals.
  synthetic_begin_frame_source_->OnUpdateVSyncParameters(
      timebase,
      interval.is_zero() ? cc::BeginFrameArgs::DefaultInterval() : interval);
}

}  // namespace ui
