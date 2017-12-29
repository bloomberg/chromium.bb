// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaValuesInitialViewport.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"

namespace blink {

MediaValuesInitialViewport* MediaValuesInitialViewport::Create(
    LocalFrame& frame) {
  return new MediaValuesInitialViewport(frame);
}

MediaValuesInitialViewport::MediaValuesInitialViewport(LocalFrame& frame)
    : MediaValuesDynamic(&frame) {}

double MediaValuesInitialViewport::ViewportWidth() const {
  DCHECK(frame_->View());
  return frame_->View()->InitialViewportWidth();
}

double MediaValuesInitialViewport::ViewportHeight() const {
  DCHECK(frame_->View());
  return frame_->View()->InitialViewportHeight();
}

}  // namespace blink
