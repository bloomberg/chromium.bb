// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "core/animation/AnimationTestHelper.h"
#include "core/animation/InterpolationEffect.h"
#include "core/animation/LegacyStyleInterpolation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

const double kInterpolationTestDuration = 1.0;

}  // namespace

class AnimationInterpolationEffectTest : public ::testing::Test {
 protected:
  InterpolableValue* InterpolationValue(
      LegacyStyleInterpolation& interpolation) {
    return interpolation.GetCachedValueForTesting();
  }

  double GetInterpolableNumber(RefPtr<Interpolation> value) {
    LegacyStyleInterpolation& interpolation =
        ToLegacyStyleInterpolation(*value.Get());
    return ToInterpolableNumber(InterpolationValue(interpolation))->Value();
  }
};

TEST_F(AnimationInterpolationEffectTest, SingleInterpolation) {
  InterpolationEffect interpolation_effect;
  interpolation_effect.AddInterpolation(
      SampleTestInterpolation::Create(InterpolableNumber::Create(0),
                                      InterpolableNumber::Create(10)),
      RefPtr<TimingFunction>(), 0, 1, -1, 2);

  Vector<RefPtr<Interpolation>> active_interpolations;
  interpolation_effect.GetActiveInterpolations(-2, kInterpolationTestDuration,
                                               active_interpolations);
  EXPECT_EQ(0ul, active_interpolations.size());

  interpolation_effect.GetActiveInterpolations(-0.5, kInterpolationTestDuration,
                                               active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_EQ(-5, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect.GetActiveInterpolations(0.5, kInterpolationTestDuration,
                                               active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(5, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect.GetActiveInterpolations(1.5, kInterpolationTestDuration,
                                               active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(15, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect.GetActiveInterpolations(3, kInterpolationTestDuration,
                                               active_interpolations);
  EXPECT_EQ(0ul, active_interpolations.size());
}

TEST_F(AnimationInterpolationEffectTest, MultipleInterpolations) {
  InterpolationEffect interpolation_effect;
  interpolation_effect.AddInterpolation(
      SampleTestInterpolation::Create(InterpolableNumber::Create(10),
                                      InterpolableNumber::Create(15)),
      RefPtr<TimingFunction>(), 1, 2, 1, 3);
  interpolation_effect.AddInterpolation(
      SampleTestInterpolation::Create(InterpolableNumber::Create(0),
                                      InterpolableNumber::Create(1)),
      LinearTimingFunction::Shared(), 0, 1, 0, 1);
  interpolation_effect.AddInterpolation(
      SampleTestInterpolation::Create(InterpolableNumber::Create(1),
                                      InterpolableNumber::Create(6)),
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE),
      0.5, 1.5, 0.5, 1.5);

  Vector<RefPtr<Interpolation>> active_interpolations;
  interpolation_effect.GetActiveInterpolations(-0.5, kInterpolationTestDuration,
                                               active_interpolations);
  EXPECT_EQ(0ul, active_interpolations.size());

  interpolation_effect.GetActiveInterpolations(0, kInterpolationTestDuration,
                                               active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(0, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect.GetActiveInterpolations(0.5, kInterpolationTestDuration,
                                               active_interpolations);
  EXPECT_EQ(2ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(0.5f, GetInterpolableNumber(active_interpolations.at(0)));
  EXPECT_FLOAT_EQ(1, GetInterpolableNumber(active_interpolations.at(1)));

  interpolation_effect.GetActiveInterpolations(1, kInterpolationTestDuration,
                                               active_interpolations);
  EXPECT_EQ(2ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(10, GetInterpolableNumber(active_interpolations.at(0)));
  EXPECT_FLOAT_EQ(5.0282884f,
                  GetInterpolableNumber(active_interpolations.at(1)));

  interpolation_effect.GetActiveInterpolations(
      1, kInterpolationTestDuration * 1000, active_interpolations);
  EXPECT_EQ(2ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(10, GetInterpolableNumber(active_interpolations.at(0)));
  EXPECT_FLOAT_EQ(5.0120168f,
                  GetInterpolableNumber(active_interpolations.at(1)));

  interpolation_effect.GetActiveInterpolations(1.5, kInterpolationTestDuration,
                                               active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(12.5f, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect.GetActiveInterpolations(2, kInterpolationTestDuration,
                                               active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(15, GetInterpolableNumber(active_interpolations.at(0)));
}

}  // namespace blink
