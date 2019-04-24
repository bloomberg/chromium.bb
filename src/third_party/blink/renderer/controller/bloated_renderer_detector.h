// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_BLOATED_RENDERER_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_BLOATED_RENDERER_DETECTOR_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

// Singleton class for detecting and handling bloated renderer conditions.
// Different parts of the renderer call the corresponding methods of this
// class to notify about potential bloat conditions. It currently works only
// for V8. In future it will be extended to the whole renderer.
class CONTROLLER_EXPORT BloatedRendererDetector {
  USING_FAST_MALLOC(BloatedRendererDetector);

 public:
  // Sets up the global singleton instance.
  static void Initialize();

  // Called when the main V8 isolate is close to reach its heap limit.
  static NearV8HeapLimitHandling OnNearV8HeapLimitOnMainThread();

 private:
  friend class BloatedRendererDetectorTest;
  FRIEND_TEST_ALL_PREFIXES(BloatedRendererDetectorTest, CooldownTime);
  FRIEND_TEST_ALL_PREFIXES(BloatedRendererDetectorTest, ForwardToBrowser);
  FRIEND_TEST_ALL_PREFIXES(BloatedRendererDetectorTest, MultipleDetections);

  // The time which should pass between 2 successive detections of bloated
  // renderer.
  static const int kMinimumCooldownInMinutes = 10;

  explicit BloatedRendererDetector(WTF::TimeTicks time)
      : last_detection_time_(time) {}

  NearV8HeapLimitHandling OnNearV8HeapLimitOnMainThreadImpl();

  WTF::TimeTicks last_detection_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_BLOATED_RENDERER_DETECTOR_H_
