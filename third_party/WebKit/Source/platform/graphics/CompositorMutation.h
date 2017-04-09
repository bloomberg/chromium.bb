// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutation_h
#define CompositorMutation_h

#include <memory>
#include "platform/graphics/CompositorMutableProperties.h"
#include "platform/wtf/HashMap.h"
#include "third_party/skia/include/core/SkMatrix44.h"

namespace blink {

class CompositorMutation {
 public:
  void SetOpacity(float opacity) {
    mutated_flags_ |= CompositorMutableProperty::kOpacity;
    opacity_ = opacity;
  }
  void SetScrollLeft(float scroll_left) {
    mutated_flags_ |= CompositorMutableProperty::kScrollLeft;
    scroll_left_ = scroll_left;
  }
  void SetScrollTop(float scroll_top) {
    mutated_flags_ |= CompositorMutableProperty::kScrollTop;
    scroll_top_ = scroll_top;
  }
  void SetTransform(const SkMatrix44& transform) {
    mutated_flags_ |= CompositorMutableProperty::kTransform;
    transform_ = transform;
  }

  bool IsOpacityMutated() const {
    return mutated_flags_ & CompositorMutableProperty::kOpacity;
  }
  bool IsScrollLeftMutated() const {
    return mutated_flags_ & CompositorMutableProperty::kScrollLeft;
  }
  bool IsScrollTopMutated() const {
    return mutated_flags_ & CompositorMutableProperty::kScrollTop;
  }
  bool IsTransformMutated() const {
    return mutated_flags_ & CompositorMutableProperty::kTransform;
  }

  float Opacity() const { return opacity_; }
  float ScrollLeft() const { return scroll_left_; }
  float ScrollTop() const { return scroll_top_; }
  SkMatrix44 Transform() const { return transform_; }

 private:
  uint32_t mutated_flags_ = 0;
  float opacity_ = 0;
  float scroll_left_ = 0;
  float scroll_top_ = 0;
  SkMatrix44 transform_;
};

struct CompositorMutations {
  HashMap<uint64_t, std::unique_ptr<CompositorMutation>> map;
};

}  // namespace blink

#endif  // CompositorMutation_h
