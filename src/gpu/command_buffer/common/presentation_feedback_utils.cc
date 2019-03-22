// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/presentation_feedback_utils.h"

#include "gpu/command_buffer/common/swap_buffers_flags.h"
#include "ui/gfx/presentation_feedback.h"

namespace gpu {

bool ShouldSendBufferPresented(uint32_t swap_buffer_flags,
                               uint32_t presentation_feedback_flags) {
  return swap_buffer_flags & SwapBuffersFlags::kPresentationFeedback ||
         (swap_buffer_flags & SwapBuffersFlags::kVSyncParams &&
          presentation_feedback_flags & gfx::PresentationFeedback::kVSync);
}

bool ShouldUpdateVsyncParams(const gfx::PresentationFeedback& feedback) {
  return feedback.flags & gfx::PresentationFeedback::kVSync &&
         feedback.timestamp != base::TimeTicks() &&
         feedback.interval != base::TimeDelta();
}

}  // namespace gpu
