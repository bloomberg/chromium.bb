// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/bloated_renderer_detector.h"

#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"

namespace blink {

static BloatedRendererDetector* g_bloated_renderer_detector = nullptr;

void BloatedRendererDetector::Initialize() {
  g_bloated_renderer_detector =
      new BloatedRendererDetector(WTF::CurrentTimeTicks());
}

NearV8HeapLimitHandling
BloatedRendererDetector::OnNearV8HeapLimitOnMainThread() {
  return g_bloated_renderer_detector->OnNearV8HeapLimitOnMainThreadImpl();
}

NearV8HeapLimitHandling
BloatedRendererDetector::OnNearV8HeapLimitOnMainThreadImpl() {
  WTF::TimeTicks now = WTF::CurrentTimeTicks();
  if (!RuntimeEnabledFeatures::
          BloatedRendererDetectionSkipUptimeCheckEnabled()) {
    WTF::TimeDelta delta = now - last_detection_time_;
    if (delta.InMinutes() < kMinimumCooldownInMinutes) {
      return NearV8HeapLimitHandling::kIgnoredDueToCooldownTime;
    }
  }
  last_detection_time_ = now;
  RendererResourceCoordinator::Get().OnRendererIsBloated();
  return NearV8HeapLimitHandling::kForwardedToBrowser;
}

}  // namespace blink
