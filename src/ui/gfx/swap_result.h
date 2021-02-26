// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SWAP_RESULT_H_
#define UI_GFX_SWAP_RESULT_H_

#include <memory>

#include "base/time/time.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

struct CALayerParams;
class GpuFence;

enum class SwapResult {
  SWAP_ACK,
  SWAP_FAILED,
  SWAP_NAK_RECREATE_BUFFERS,
  SWAP_RESULT_LAST = SWAP_NAK_RECREATE_BUFFERS,
};

struct SwapTimings {
  // When the GPU service first started processing the SwapBuffers request.
  base::TimeTicks swap_start;

  // On most platforms, this is when the GPU service finished processing the
  // SwapBuffers request. On ChromeOS, this corresponds to the present time.
  // TODO(brianderson): Differentiate the concepts without introducing
  // dicontinuities in associated UMA data.
  base::TimeTicks swap_end;

  // When Display Compositor thread scheduled work to GPU Thread. For GLRenderer
  // it's when InProcessCommandBuffer::Flush() happens, for SkiaRenderer it's
  // PostTask time for FinishPaintRenderPass or SwapBuffers whichever comes
  // first.
  base::TimeTicks viz_scheduled_draw;

  // When GPU thread started draw submitted by Display Compositor thread. For
  // GLRenderer it's InProcessCommandBuffer::FlushOnGpuThread, for SkiaRenderer
  // it's FinishPaintRenderPass/SwapBuffers.
  base::TimeTicks gpu_started_draw;

  bool is_null() const { return swap_start.is_null() && swap_end.is_null(); }
};

// Sent by ImageTransportSurfaces to their clients in response to a SwapBuffers.
struct SwapResponse {
  // The swap's sequence id which helps clients determine which SwapBuffers
  // this corresponds to. We may receive responses out of order on platforms
  // that allow multiple swaps pending if a failed swap returns immediately
  // while a successful swap is still outstanding.
  uint64_t swap_id;

  // Indicates whether the swap succeeded or not.
  // TODO(https://crbug.com/894929): It may be more reasonable to add
  // a full SwapCompletionResult as a member.
  SwapResult result;

  // Timing information about the given swap.
  SwapTimings timings;
};

// Sent by GLImages to their GLImage::SwapCompletionCallbacks.
struct GFX_EXPORT SwapCompletionResult {
  explicit SwapCompletionResult(gfx::SwapResult swap_result);
  SwapCompletionResult(gfx::SwapResult swap_result,
                       std::unique_ptr<gfx::GpuFence> gpu_fence);
  SwapCompletionResult(gfx::SwapResult swap_result,
                       std::unique_ptr<gfx::CALayerParams> ca_layer_params);
  SwapCompletionResult(SwapCompletionResult&& other);
  ~SwapCompletionResult();

  SwapCompletionResult(const SwapCompletionResult& other) = delete;
  SwapCompletionResult& operator=(const SwapCompletionResult other) = delete;

  gfx::SwapResult swap_result = SwapResult::SWAP_FAILED;
  std::unique_ptr<GpuFence> gpu_fence;
  std::unique_ptr<CALayerParams> ca_layer_params;
};

}  // namespace gfx

#endif  // UI_GFX_SWAP_RESULT_H_
