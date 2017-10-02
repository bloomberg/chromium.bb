// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VideoFrameResourceProvider_h
#define VideoFrameResourceProvider_h

#include "cc/resources/video_resource_updater.h"
#include "public/platform/WebVideoFrameSubmitter.h"

namespace viz {
class RenderPass;
}

namespace blink {

// Placeholder class, to be implemented in full in later CL.
// VideoFrameResourceProvider obtains required GPU resources for the video
// frame.
class VideoFrameResourceProvider {
 public:
  explicit VideoFrameResourceProvider(WebContextProviderCallback);

  void Initialize(viz::ContextProvider*);
  void AppendQuads(viz::RenderPass&);

 private:
  WebContextProviderCallback context_provider_callback_;
  std::unique_ptr<cc::VideoResourceUpdater> resource_updater_;
  base::WeakPtrFactory<VideoFrameResourceProvider> weak_ptr_factory_;
};

}  // namespace blink

#endif  // VideoFrameResourceProvider_h
