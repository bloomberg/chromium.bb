// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_PROPERTY_CHANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_PROPERTY_CHANGE_H_

#include <algorithm>

namespace blink {

// Used to report whether and how paint properties have changed.
// It is not a simple enum because it needs to manage the "undetermined" state
// when only animatable properties (i.e. transform, opacity, filter and backdrop
// filter) changed. The actual change type can be later determined according to
// whether we are running specific types of animations.
class PaintPropertyChange {
 public:
  static PaintPropertyChange Unchanged() {
    return PaintPropertyChange(kUnchanged);
  }

  static PaintPropertyChange ChangedTransformValue() {
    return PaintPropertyChange(kChangedTransform);
  }

  static PaintPropertyChange ChangedAnimatableEffectValues(
      bool opacity_changed,
      bool filter_changed,
      bool backdrop_filter_changed) {
    return PaintPropertyChange(
        (opacity_changed ? kChangedOpacity : 0) |
        (filter_changed ? kChangedFilter : 0) |
        (backdrop_filter_changed ? kChangedBackdropFilter : 0));
  }

  static PaintPropertyChange ChangedValues() {
    return PaintPropertyChange(kChangedOnlyValues);
  }

  static PaintPropertyChange NodeAddedOrRemoved() {
    return PaintPropertyChange(kNodeAddedOrRemoved);
  }

  void Merge(PaintPropertyChange other) {
    type_ = std::max(type_, other.type_);
    // TODO(wangxianzhu): This is to workaround crbug.com/941652 of the
    // compiler. Change this to DCHECK after the bug is fixed.
    CHECK(!HasUndeterminedChangeOfAnimationValues());
  }

  void Merge(PaintPropertyChange other,
             bool is_running_transform_animation_on_compositor,
             bool is_running_opacity_animation_on_compositor,
             bool is_running_filter_animation_on_compositor,
             bool is_running_backdrop_filter_animation_on_compositor) {
    DCHECK(!HasUndeterminedChangeOfAnimationValues());
    if (type_ > kChangedOnlyAnimationValues ||
        !other.HasUndeterminedChangeOfAnimationValues())
      return Merge(other);

    // If an animatable value changed but is not animating, then treat the
    // change as a normal value change.
    DCHECK(other.HasUndeterminedChangeOfAnimationValues());
    if (((other.type_ & kChangedTransform) &&
         !is_running_transform_animation_on_compositor) ||
        ((other.type_ & kChangedOpacity) &&
         !is_running_opacity_animation_on_compositor) ||
        ((other.type_ & kChangedFilter) &&
         !is_running_filter_animation_on_compositor) ||
        ((other.type_ & kChangedBackdropFilter) &&
         !is_running_backdrop_filter_animation_on_compositor)) {
      type_ = kChangedOnlyValues;
    } else {
      type_ = kChangedOnlyAnimationValues;
    }
  }

  bool IsUnchanged() const { return type_ == kUnchanged; }
  bool HasChangedNonCompositedAnimatingValues() const {
    return type_ >= kChangedOnlyValues;
  }
  bool HasNodeAddedOrRemoved() const { return type_ == kNodeAddedOrRemoved; }

 private:
  // The type of change. The order is important - it must go from no change
  // to the most significant change.
  enum Type {
    kUnchanged = 0,

    // The following 4 bits are for animatable values before the final change
    // type can be determined according to whether we are running specific types
    // of animations. If an animatable value changed but we are not running the
    // corresponding type of animation, the change type will be determined as
    // kChangedOnlyValues instead of kChangedOnlyAnimationValues.
    // We can test type_ against these bits only when
    // HasUndeterminedChangeOfAnimationValues() is true.
    kChangedTransform = 1 << 0,
    kChangedOpacity = 1 << 1,
    kChangedFilter = 1 << 2,
    kChangedBackdropFilter = 1 << 3,

    kChangedOnlyAnimationValues = 1 << 4,
    kChangedOnlyValues,
    // A paint property node is added or removed. This value is used only in
    // renderer/core classes.
    kNodeAddedOrRemoved,
  };

  PaintPropertyChange() = delete;
  PaintPropertyChange(unsigned type) : type_(type) {}

  bool HasUndeterminedChangeOfAnimationValues() const {
    return type_ > kUnchanged && type_ < kChangedOnlyAnimationValues;
  }

  unsigned char type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_PROPERTY_CHANGE_H_
