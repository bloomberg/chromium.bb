// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/gpu/managed_memory_policy_convert.h"

namespace webkit {
namespace gpu {

static cc::ManagedMemoryPolicy::PriorityCutoff ConvertPriorityCutoff(
    WebKit::WebGraphicsMemoryAllocation::PriorityCutoff priority_cutoff) {
  // This is simple a 1:1 map, the names differ only because the WebKit names
  // should be to match the cc names.
  switch (priority_cutoff) {
    case WebKit::WebGraphicsMemoryAllocation::PriorityCutoffAllowNothing:
      return cc::ManagedMemoryPolicy::CUTOFF_ALLOW_NOTHING;
    case WebKit::WebGraphicsMemoryAllocation::PriorityCutoffAllowVisibleOnly:
      return cc::ManagedMemoryPolicy::CUTOFF_ALLOW_REQUIRED_ONLY;
    case WebKit::WebGraphicsMemoryAllocation::
        PriorityCutoffAllowVisibleAndNearby:
      return cc::ManagedMemoryPolicy::CUTOFF_ALLOW_NICE_TO_HAVE;
    case WebKit::WebGraphicsMemoryAllocation::PriorityCutoffAllowEverything:
      return cc::ManagedMemoryPolicy::CUTOFF_ALLOW_EVERYTHING;
  }
  NOTREACHED();
  return cc::ManagedMemoryPolicy::CUTOFF_ALLOW_NOTHING;
}

// static
cc::ManagedMemoryPolicy ManagedMemoryPolicyConvert::Convert(
    const WebKit::WebGraphicsMemoryAllocation& allocation,
    bool* discard_backbuffer_when_not_visible) {
  *discard_backbuffer_when_not_visible = !allocation.suggestHaveBackbuffer;
  return cc::ManagedMemoryPolicy(
      allocation.bytesLimitWhenVisible,
      ConvertPriorityCutoff(allocation.priorityCutoffWhenVisible),
      allocation.bytesLimitWhenNotVisible,
      ConvertPriorityCutoff(allocation.priorityCutoffWhenNotVisible),
      cc::ManagedMemoryPolicy::kDefaultNumResourcesLimit);
}

}  // namespace gpu
}  // namespace webkit
