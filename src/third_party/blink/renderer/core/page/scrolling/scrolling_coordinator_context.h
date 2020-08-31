// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLLING_COORDINATOR_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLLING_COORDINATOR_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace cc {
class AnimationHost;
}

namespace blink {

// This enscapsulates ScrollingCoordinator state for each local frame root.
// TODO(kenrb): This class could be temporary depending on how
// https://crbug.com/680606 is resolved.
class CORE_EXPORT ScrollingCoordinatorContext final {
  USING_FAST_MALLOC(ScrollingCoordinatorContext);

 public:
  ScrollingCoordinatorContext() {}
  virtual ~ScrollingCoordinatorContext() {}

  void SetAnimationTimeline(
      std::unique_ptr<CompositorAnimationTimeline> timeline) {
    animation_timeline_ = std::move(timeline);
  }
  void SetAnimationHost(cc::AnimationHost* host) { animation_host_ = host; }

  CompositorAnimationTimeline* GetCompositorAnimationTimeline() {
    return animation_timeline_.get();
  }
  cc::AnimationHost* GetCompositorAnimationHost() { return animation_host_; }

  // Non-fast scrollable regions need updating by ScrollingCoordinator.
  bool ScrollGestureRegionIsDirty() const {
    return scroll_gesture_region_is_dirty_;
  }
  // Touch event target rects need updating by ScrollingCoordinator.
  bool TouchEventTargetRectsAreDirty() const {
    return touch_event_target_rects_are_dirty_;
  }

  // Only ScrollingCoordinator should ever set |dirty| to |false|.
  void SetScrollGestureRegionIsDirty(bool dirty) {
    scroll_gesture_region_is_dirty_ = dirty;
  }
  void SetTouchEventTargetRectsAreDirty(bool dirty) {
    touch_event_target_rects_are_dirty_ = dirty;
  }

 private:
  std::unique_ptr<CompositorAnimationTimeline> animation_timeline_;
  cc::AnimationHost* animation_host_ = nullptr;

  bool scroll_gesture_region_is_dirty_ = false;
  bool touch_event_target_rects_are_dirty_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScrollingCoordinatorContext);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLLING_COORDINATOR_CONTEXT_H_
