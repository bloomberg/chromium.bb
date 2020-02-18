// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fullscreen/scoped_allow_fullscreen.h"

#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

base::Optional<ScopedAllowFullscreen::Reason> ScopedAllowFullscreen::reason_;

ScopedAllowFullscreen::ScopedAllowFullscreen(Reason reason) {
  DCHECK(IsMainThread());
  previous_reason_ = reason_;
  reason_ = reason;
}

ScopedAllowFullscreen::~ScopedAllowFullscreen() {
  DCHECK(IsMainThread());
  reason_ = previous_reason_;
}

// static
base::Optional<ScopedAllowFullscreen::Reason>
ScopedAllowFullscreen::FullscreenAllowedReason() {
  DCHECK(IsMainThread());
  return reason_;
}

}  // namespace blink
