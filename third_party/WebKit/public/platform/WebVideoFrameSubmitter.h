// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebVideoFrameSubmitter_h
#define WebVideoFrameSubmitter_h

#include "WebCommon.h"
#include "cc/layers/video_frame_provider.h"

namespace viz {
class FrameSinkId;
}  // namespace viz

namespace blink {

// Exposes the VideoFrameSubmitter, which submits CompositorFrames containing
// information from VideoFrames.
class BLINK_PLATFORM_EXPORT WebVideoFrameSubmitter
    : public cc::VideoFrameProvider::Client {
 public:
  static std::unique_ptr<WebVideoFrameSubmitter> Create(
      cc::VideoFrameProvider*);
  virtual ~WebVideoFrameSubmitter() = default;
  virtual void StartSubmitting(const viz::FrameSinkId&) = 0;
};

}  // namespace blink

#endif  // WebVideoFrameSubmitter_h
