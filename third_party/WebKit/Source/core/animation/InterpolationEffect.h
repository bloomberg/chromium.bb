// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationEffect_h
#define InterpolationEffect_h

#include "core/CoreExport.h"
#include "core/animation/Interpolation.h"
#include "core/animation/Keyframe.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/animation/TimingFunction.h"

namespace blink {

// Stores all adjacent pairs of keyframes (represented by Interpolations) in a
// KeyframeEffectModel with keyframe offset data preprocessed for more efficient
// active keyframe pair sampling.
class CORE_EXPORT InterpolationEffect {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  InterpolationEffect() : is_populated_(false) {}

  bool IsPopulated() const { return is_populated_; }
  void SetPopulated() { is_populated_ = true; }

  void Clear() {
    is_populated_ = false;
    interpolations_.clear();
  }

  void GetActiveInterpolations(double fraction,
                               double iteration_duration,
                               Vector<RefPtr<Interpolation>>&) const;

  void AddInterpolation(PassRefPtr<Interpolation> interpolation,
                        PassRefPtr<TimingFunction> easing,
                        double start,
                        double end,
                        double apply_from,
                        double apply_to) {
    interpolations_.push_back(InterpolationRecord(std::move(interpolation),
                                                  std::move(easing), start, end,
                                                  apply_from, apply_to));
  }

  void AddInterpolationsFromKeyframes(
      const PropertyHandle&,
      const Keyframe::PropertySpecificKeyframe& keyframe_a,
      const Keyframe::PropertySpecificKeyframe& keyframe_b,
      double apply_from,
      double apply_to);

 private:
  struct InterpolationRecord {
    InterpolationRecord(RefPtr<Interpolation> interpolation,
                        RefPtr<TimingFunction> easing,
                        double start,
                        double end,
                        double apply_from,
                        double apply_to)
        : interpolation_(std::move(interpolation)),
          easing_(std::move(easing)),
          start_(start),
          end_(end),
          apply_from_(apply_from),
          apply_to_(apply_to) {}

    RefPtr<Interpolation> interpolation_;
    RefPtr<TimingFunction> easing_;
    double start_;
    double end_;
    double apply_from_;
    double apply_to_;
  };

  bool is_populated_;
  Vector<InterpolationRecord> interpolations_;
};

}  // namespace blink

#endif  // InterpolationEffect_h
