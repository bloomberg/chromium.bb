// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/vulkan_ycbcr_info.h"

namespace gpu {

VulkanYCbCrInfo::VulkanYCbCrInfo() = default;

VulkanYCbCrInfo::VulkanYCbCrInfo(uint32_t suggested_ycbcr_model,
                                 uint32_t suggested_ycbcr_range,
                                 uint32_t suggested_xchroma_offset,
                                 uint32_t suggested_ychroma_offset,
                                 uint64_t external_format,
                                 uint32_t format_features)
    : suggested_ycbcr_model(suggested_ycbcr_model),
      suggested_ycbcr_range(suggested_ycbcr_range),
      suggested_xchroma_offset(suggested_xchroma_offset),
      suggested_ychroma_offset(suggested_ychroma_offset),
      external_format(external_format),
      format_features(format_features) {}

}  // namespace gpu
