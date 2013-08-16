// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_GPU_MANAGED_MEMORY_POLICY_CONVERT_H_
#define WEBKIT_COMMON_GPU_MANAGED_MEMORY_POLICY_CONVERT_H_

#include "cc/output/managed_memory_policy.h"
#include "third_party/WebKit/public/platform/WebGraphicsMemoryAllocation.h"
#include "webkit/common/gpu/webkit_gpu_export.h"

namespace webkit {
namespace gpu {

class WEBKIT_GPU_EXPORT ManagedMemoryPolicyConvert {
 public:
  static cc::ManagedMemoryPolicy Convert(
      const WebKit::WebGraphicsMemoryAllocation& allocation,
      bool* discard_backbuffer_when_not_visible);
};

}  // namespace gpu
}  // namespace webkit

#endif  // WEBKIT_COMMON_GPU_MANAGED_MEMORY_POLICY_CONVERT_H_
