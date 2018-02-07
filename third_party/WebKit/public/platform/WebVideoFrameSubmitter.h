// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebVideoFrameSubmitter_h
#define WebVideoFrameSubmitter_h

#include "WebCommon.h"
#include "cc/layers/video_frame_provider.h"
#include "media/base/video_rotation.h"

namespace cc {
class LayerTreeSettings;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace viz {
class ContextProvider;
class FrameSinkId;
class SharedBitmapManager;
}  // namespace viz

namespace blink {

// Callback to obtain the media ContextProvider.
using WebContextProviderCallback = base::RepeatingCallback<void(
    base::OnceCallback<void(viz::ContextProvider*)>)>;

// Exposes the VideoFrameSubmitter, which submits CompositorFrames containing
// decoded VideoFrames from the VideoFrameProvider to the compositor for
// display.
class BLINK_PLATFORM_EXPORT WebVideoFrameSubmitter
    : public cc::VideoFrameProvider::Client {
 public:
  static std::unique_ptr<WebVideoFrameSubmitter> Create(
      WebContextProviderCallback,
      viz::SharedBitmapManager*,
      gpu::GpuMemoryBufferManager*,
      const cc::LayerTreeSettings&);
  virtual ~WebVideoFrameSubmitter() = default;

  // Intialize must be called before submissions occur, pulled out of
  // StartSubmitting() to enable tests without the full mojo statck running.
  virtual void Initialize(cc::VideoFrameProvider*) = 0;

  virtual void StartSubmitting(const viz::FrameSinkId&) = 0;
  virtual void SetRotation(media::VideoRotation) = 0;
};

}  // namespace blink

#endif  // WebVideoFrameSubmitter_h
