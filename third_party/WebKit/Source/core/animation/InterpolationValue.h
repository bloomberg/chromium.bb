// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationValue_h
#define InterpolationValue_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/NonInterpolableValue.h"
#include "platform/heap/Handle.h"
#include <memory>

namespace blink {

// Represents a (non-strict) subset of a PropertySpecificKeyframe's value broken
// down into interpolable and non-interpolable parts. InterpolationValues can be
// composed together to represent a whole PropertySpecificKeyframe value.
struct InterpolationValue {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

  explicit InterpolationValue(
      std::unique_ptr<InterpolableValue> interpolable_value,
      RefPtr<NonInterpolableValue> non_interpolable_value = nullptr)
      : interpolable_value(std::move(interpolable_value)),
        non_interpolable_value(std::move(non_interpolable_value)) {}

  InterpolationValue(std::nullptr_t) {}

  InterpolationValue(InterpolationValue&& other)
      : interpolable_value(std::move(other.interpolable_value)),
        non_interpolable_value(std::move(other.non_interpolable_value)) {}

  void operator=(InterpolationValue&& other) {
    interpolable_value = std::move(other.interpolable_value);
    non_interpolable_value = std::move(other.non_interpolable_value);
  }

  operator bool() const { return interpolable_value.get(); }

  InterpolationValue Clone() const {
    return InterpolationValue(
        interpolable_value ? interpolable_value->Clone() : nullptr,
        non_interpolable_value);
  }

  void Clear() {
    interpolable_value.reset();
    non_interpolable_value.Clear();
  }

  std::unique_ptr<InterpolableValue> interpolable_value;
  RefPtr<NonInterpolableValue> non_interpolable_value;
};

}  // namespace blink

#endif  // InterpolationValue_h
