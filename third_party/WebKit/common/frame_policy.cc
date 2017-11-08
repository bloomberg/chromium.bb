// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/WebKit/common/frame_policy.h"

namespace blink {

FramePolicy::FramePolicy()
    : sandbox_flags(WebSandboxFlags::kNone), container_policy({}) {}

FramePolicy::FramePolicy(WebSandboxFlags sandbox_flags,
                         const ParsedFeaturePolicy& container_policy)
    : sandbox_flags(sandbox_flags), container_policy(container_policy) {}

FramePolicy::FramePolicy(const FramePolicy& lhs)
    : sandbox_flags(lhs.sandbox_flags),
      container_policy(lhs.container_policy) {}

FramePolicy::~FramePolicy() {}

}  // namespace blink
