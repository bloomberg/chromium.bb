// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LEAK_DETECTOR_BLINK_LEAK_DETECTOR_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LEAK_DETECTOR_BLINK_LEAK_DETECTOR_CLIENT_H_

namespace blink {

// This class is a client for BlinkLeakDetector.
// Inherit this class and override the operations after leak detection.
class BlinkLeakDetectorClient {
 public:
  virtual void OnLeakDetectionComplete() = 0;
};

}  // namespace blink

#endif  // LeakDetectorClient_h
