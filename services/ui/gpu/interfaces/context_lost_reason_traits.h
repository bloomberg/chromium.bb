// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_GPU_INTERFACES_CONTEXT_LOST_REASON_TRAITS_H_
#define SERVICES_UI_GPU_INTERFACES_CONTEXT_LOST_REASON_TRAITS_H_

#include "gpu/command_buffer/common/constants.h"
#include "services/ui/gpu/interfaces/context_lost_reason.mojom.h"

namespace mojo {

template <>
struct EnumTraits<ui::mojom::ContextLostReason, gpu::error::ContextLostReason> {
  static ui::mojom::ContextLostReason ToMojom(
      gpu::error::ContextLostReason reason) {
    switch (reason) {
      case gpu::error::kGuilty:
        return ui::mojom::ContextLostReason::GUILTY;
      case gpu::error::kInnocent:
        return ui::mojom::ContextLostReason::INNOCENT;
      case gpu::error::kUnknown:
        return ui::mojom::ContextLostReason::UNKNOWN;
      case gpu::error::kOutOfMemory:
        return ui::mojom::ContextLostReason::OUT_OF_MEMORY;
      case gpu::error::kMakeCurrentFailed:
        return ui::mojom::ContextLostReason::MAKE_CURRENT_FAILED;
      case gpu::error::kGpuChannelLost:
        return ui::mojom::ContextLostReason::GPU_CHANNEL_LOST;
      case gpu::error::kInvalidGpuMessage:
        return ui::mojom::ContextLostReason::INVALID_GPU_MESSAGE;
    }
    NOTREACHED();
    return ui::mojom::ContextLostReason::UNKNOWN;
  }

  static bool FromMojom(ui::mojom::ContextLostReason reason,
                        gpu::error::ContextLostReason* out) {
    switch (reason) {
      case ui::mojom::ContextLostReason::GUILTY:
        *out = gpu::error::kGuilty;
        return true;
      case ui::mojom::ContextLostReason::INNOCENT:
        *out = gpu::error::kInnocent;
        return true;
      case ui::mojom::ContextLostReason::UNKNOWN:
        *out = gpu::error::kUnknown;
        return true;
      case ui::mojom::ContextLostReason::OUT_OF_MEMORY:
        *out = gpu::error::kOutOfMemory;
        return true;
      case ui::mojom::ContextLostReason::MAKE_CURRENT_FAILED:
        *out = gpu::error::kMakeCurrentFailed;
        return true;
      case ui::mojom::ContextLostReason::GPU_CHANNEL_LOST:
        *out = gpu::error::kGpuChannelLost;
        return true;
      case ui::mojom::ContextLostReason::INVALID_GPU_MESSAGE:
        *out = gpu::error::kInvalidGpuMessage;
        return true;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // SERVICES_UI_GPU_INTERFACES_CONTEXT_LOST_REASON_TRAITS_H_
