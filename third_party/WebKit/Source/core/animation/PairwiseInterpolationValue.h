// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PairwiseInterpolationValue_h
#define PairwiseInterpolationValue_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/NonInterpolableValue.h"
#include "platform/heap/Handle.h"
#include <memory>

namespace blink {

// Represents the smooth interpolation between an adjacent pair of
// PropertySpecificKeyframes.
struct PairwiseInterpolationValue {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

  PairwiseInterpolationValue(
      std::unique_ptr<InterpolableValue> start_interpolable_value,
      std::unique_ptr<InterpolableValue> end_interpolable_value,
      scoped_refptr<NonInterpolableValue> non_interpolable_value = nullptr)
      : start_interpolable_value(std::move(start_interpolable_value)),
        end_interpolable_value(std::move(end_interpolable_value)),
        non_interpolable_value(std::move(non_interpolable_value)) {}

  PairwiseInterpolationValue(std::nullptr_t) {}

  PairwiseInterpolationValue(PairwiseInterpolationValue&& other)
      : start_interpolable_value(std::move(other.start_interpolable_value)),
        end_interpolable_value(std::move(other.end_interpolable_value)),
        non_interpolable_value(std::move(other.non_interpolable_value)) {}

  operator bool() const { return start_interpolable_value.get(); }

  std::unique_ptr<InterpolableValue> start_interpolable_value;
  std::unique_ptr<InterpolableValue> end_interpolable_value;
  scoped_refptr<NonInterpolableValue> non_interpolable_value;
};

}  // namespace blink

#endif  // PairwiseInterpolationValue_h
