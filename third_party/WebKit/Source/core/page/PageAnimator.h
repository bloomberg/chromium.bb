// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PageAnimator_h
#define PageAnimator_h

#include "core/CoreExport.h"
#include "core/animation/AnimationClock.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class Page;

class CORE_EXPORT PageAnimator final : public GarbageCollected<PageAnimator> {
 public:
  static PageAnimator* Create(Page&);
  void Trace(blink::Visitor*);
  void ScheduleVisualUpdate(LocalFrame*);
  void ServiceScriptedAnimations(double monotonic_animation_start_time);

  bool IsServicingAnimations() const { return servicing_animations_; }

  // TODO(alancutter): Remove the need for this by implementing frame request
  // suppression logic at the BeginMainFrame level. This is a temporary
  // workaround to fix a perf regression.
  // DO NOT use this outside of crbug.com/704763.
  void SetSuppressFrameRequestsWorkaroundFor704763Only(bool);

  // See documents of methods with the same names in LocalFrameView class.
  void UpdateAllLifecyclePhases(LocalFrame& root_frame);
  AnimationClock& Clock() { return animation_clock_; }

 private:
  explicit PageAnimator(Page&);

  Member<Page> page_;
  bool servicing_animations_;
  bool updating_layout_and_style_for_painting_;
  bool suppress_frame_requests_workaround_for704763_only_ = false;
  AnimationClock animation_clock_;
};

}  // namespace blink

#endif  // PageAnimator_h
