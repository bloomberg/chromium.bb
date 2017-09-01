// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VideoFrameResourceProvider_h
#define VideoFrameResourceProvider_h

namespace cc {
class RenderPass;
}

namespace blink {

// Placeholder class, to be implemented in full in later CL.
// VideoFrameResourceProvider obtains required GPU resources for the video
// frame.
class VideoFrameResourceProvider {
 public:
  VideoFrameResourceProvider();

  void AppendQuads(cc::RenderPass&);
};

}  // namespace blink

#endif  // VideoFrameResourceProvider_h
