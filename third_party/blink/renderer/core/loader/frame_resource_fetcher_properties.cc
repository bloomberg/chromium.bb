// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/frame_resource_fetcher_properties.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

FrameResourceFetcherProperties::FrameResourceFetcherProperties(
    LocalFrame* frame)
    : frame_(frame) {
  DCHECK(frame);
}

void FrameResourceFetcherProperties::Trace(Visitor* visitor) {
  visitor->Trace(frame_);
  ResourceFetcherProperties::Trace(visitor);
}

bool FrameResourceFetcherProperties::IsMainFrame() const {
  return frame_->IsMainFrame();
}

}  // namespace blink
