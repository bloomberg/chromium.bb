// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "core/animation/AnimationTestHelper.h"
#include "core/animation/InterpolableValue.h"
#include "core/animation/LegacyStyleInterpolation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class AnimationInterpolableValueTest : public ::testing::Test {
 protected:
  InterpolableValue* InterpolationValue(
      LegacyStyleInterpolation& interpolation) {
    return interpolation.GetCachedValueForTesting();
  }

  double InterpolateNumbers(double a, double b, double progress) {
    RefPtr<LegacyStyleInterpolation> i = SampleTestInterpolation::Create(
        InterpolableNumber::Create(a), InterpolableNumber::Create(b));
    i->Interpolate(0, progress);
    return ToInterpolableNumber(InterpolationValue(*i.Get()))->Value();
  }

  void ScaleAndAdd(InterpolableValue& base,
                   double scale,
                   const InterpolableValue& add) {
    base.ScaleAndAdd(scale, add);
  }

  RefPtr<LegacyStyleInterpolation> InterpolateLists(
      std::unique_ptr<InterpolableList> list_a,
      std::unique_ptr<InterpolableList> list_b,
      double progress) {
    RefPtr<LegacyStyleInterpolation> i =
        SampleTestInterpolation::Create(std::move(list_a), std::move(list_b));
    i->Interpolate(0, progress);
    return i;
  }
};

TEST_F(AnimationInterpolableValueTest, InterpolateNumbers) {
  EXPECT_FLOAT_EQ(126, InterpolateNumbers(42, 0, -2));
  EXPECT_FLOAT_EQ(42, InterpolateNumbers(42, 0, 0));
  EXPECT_FLOAT_EQ(29.4f, InterpolateNumbers(42, 0, 0.3));
  EXPECT_FLOAT_EQ(21, InterpolateNumbers(42, 0, 0.5));
  EXPECT_FLOAT_EQ(0, InterpolateNumbers(42, 0, 1));
  EXPECT_FLOAT_EQ(-21, InterpolateNumbers(42, 0, 1.5));
}

TEST_F(AnimationInterpolableValueTest, SimpleList) {
  std::unique_ptr<InterpolableList> list_a = InterpolableList::Create(3);
  list_a->Set(0, InterpolableNumber::Create(0));
  list_a->Set(1, InterpolableNumber::Create(42));
  list_a->Set(2, InterpolableNumber::Create(20.5));

  std::unique_ptr<InterpolableList> list_b = InterpolableList::Create(3);
  list_b->Set(0, InterpolableNumber::Create(100));
  list_b->Set(1, InterpolableNumber::Create(-200));
  list_b->Set(2, InterpolableNumber::Create(300));

  RefPtr<LegacyStyleInterpolation> i =
      InterpolateLists(std::move(list_a), std::move(list_b), 0.3);
  InterpolableList* out_list = ToInterpolableList(InterpolationValue(*i.Get()));
  EXPECT_FLOAT_EQ(30, ToInterpolableNumber(out_list->Get(0))->Value());
  EXPECT_FLOAT_EQ(-30.6f, ToInterpolableNumber(out_list->Get(1))->Value());
  EXPECT_FLOAT_EQ(104.35f, ToInterpolableNumber(out_list->Get(2))->Value());
}

TEST_F(AnimationInterpolableValueTest, NestedList) {
  std::unique_ptr<InterpolableList> list_a = InterpolableList::Create(3);
  list_a->Set(0, InterpolableNumber::Create(0));
  std::unique_ptr<InterpolableList> sub_list_a = InterpolableList::Create(1);
  sub_list_a->Set(0, InterpolableNumber::Create(100));
  list_a->Set(1, std::move(sub_list_a));
  list_a->Set(2, InterpolableNumber::Create(0));

  std::unique_ptr<InterpolableList> list_b = InterpolableList::Create(3);
  list_b->Set(0, InterpolableNumber::Create(100));
  std::unique_ptr<InterpolableList> sub_list_b = InterpolableList::Create(1);
  sub_list_b->Set(0, InterpolableNumber::Create(50));
  list_b->Set(1, std::move(sub_list_b));
  list_b->Set(2, InterpolableNumber::Create(1));

  RefPtr<LegacyStyleInterpolation> i =
      InterpolateLists(std::move(list_a), std::move(list_b), 0.5);
  InterpolableList* out_list = ToInterpolableList(InterpolationValue(*i.Get()));
  EXPECT_FLOAT_EQ(50, ToInterpolableNumber(out_list->Get(0))->Value());
  EXPECT_FLOAT_EQ(
      75, ToInterpolableNumber(ToInterpolableList(out_list->Get(1))->Get(0))
              ->Value());
  EXPECT_FLOAT_EQ(0.5, ToInterpolableNumber(out_list->Get(2))->Value());
}

TEST_F(AnimationInterpolableValueTest, ScaleAndAddNumbers) {
  std::unique_ptr<InterpolableNumber> base = InterpolableNumber::Create(10);
  ScaleAndAdd(*base, 2, *InterpolableNumber::Create(1));
  EXPECT_FLOAT_EQ(21, base->Value());

  base = InterpolableNumber::Create(10);
  ScaleAndAdd(*base, 0, *InterpolableNumber::Create(5));
  EXPECT_FLOAT_EQ(5, base->Value());

  base = InterpolableNumber::Create(10);
  ScaleAndAdd(*base, -1, *InterpolableNumber::Create(8));
  EXPECT_FLOAT_EQ(-2, base->Value());
}

TEST_F(AnimationInterpolableValueTest, ScaleAndAddLists) {
  std::unique_ptr<InterpolableList> base_list = InterpolableList::Create(3);
  base_list->Set(0, InterpolableNumber::Create(5));
  base_list->Set(1, InterpolableNumber::Create(10));
  base_list->Set(2, InterpolableNumber::Create(15));
  std::unique_ptr<InterpolableList> add_list = InterpolableList::Create(3);
  add_list->Set(0, InterpolableNumber::Create(1));
  add_list->Set(1, InterpolableNumber::Create(2));
  add_list->Set(2, InterpolableNumber::Create(3));
  ScaleAndAdd(*base_list, 2, *add_list);
  EXPECT_FLOAT_EQ(11, ToInterpolableNumber(base_list->Get(0))->Value());
  EXPECT_FLOAT_EQ(22, ToInterpolableNumber(base_list->Get(1))->Value());
  EXPECT_FLOAT_EQ(33, ToInterpolableNumber(base_list->Get(2))->Value());
}

}  // namespace blink
