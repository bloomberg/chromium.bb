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
    : DisplayOutputSurface(context_provider, synthetic_begin_frame_source),
      gl_helper_(context_provider->ContextGL(),
                 context_provider->ContextSupport()) {
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

  buffer_queue_.reset(
      new BufferQueue(context_provider->ContextGL(), target, internalformat,
                      display::DisplaySnapshot::PrimaryFormat(), &gl_helper_,
                      gpu_memory_buffer_manager, widget));
  buffer_queue_->Initialize();
}

DisplayOutputSurfaceOzone::~DisplayOutputSurfaceOzone() {
  // TODO(rjkroege): Support cleanup.
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
                                        bool has_alpha,
                                        bool use_stencil) {
  reshape_size_ = size;
  DisplayOutputSurface::Reshape(size, device_scale_factor, color_space,
                                has_alpha, use_stencil);
  buffer_queue_->Reshape(size, device_scale_factor, color_space, use_stencil);
}

void DisplayOutputSurfaceOzone::SwapBuffers(cc::OutputSurfaceFrame frame) {
  DCHECK(buffer_queue_);

  // TODO(rjkroege): What if swap happens again before DidReceiveSwapBuffersAck
  // then it would see the wrong size?
  DCHECK(reshape_size_ == frame.size);
  swap_size_ = reshape_size_;

  buffer_queue_->SwapBuffers(frame.sub_buffer_rect ? *frame.sub_buffer_rect
                                                   : gfx::Rect(swap_size_));
  DisplayOutputSurface::SwapBuffers(std::move(frame));
}

uint32_t DisplayOutputSurfaceOzone::GetFramebufferCopyTextureFormat() {
  return buffer_queue_->internal_format();
}

bool DisplayOutputSurfaceOzone::IsDisplayedAsOverlayPlane() const {
  // TODO(rjkroege): implement remaining overlay functionality.
  return true;
}

unsigned DisplayOutputSurfaceOzone::GetOverlayTextureId() const {
  return buffer_queue_->current_texture_id();
}

void DisplayOutputSurfaceOzone::DidReceiveSwapBuffersAck(
    gfx::SwapResult result) {
  bool force_swap = false;
  if (result == gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    // Even through the swap failed, this is a fixable error so we can pretend
    // it succeeded to the rest of the system.
    result = gfx::SwapResult::SWAP_ACK;
    buffer_queue_->RecreateBuffers();
    force_swap = true;
  }

  buffer_queue_->PageFlipComplete();
  client()->DidReceiveSwapBuffersAck();

  if (force_swap)
    client()->SetNeedsRedrawRect(gfx::Rect(swap_size_));
}

}  // namespace ui
