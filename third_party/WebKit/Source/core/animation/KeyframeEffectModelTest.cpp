/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/animation/KeyframeEffectModel.h"

#include "core/animation/LegacyStyleInterpolation.h"
#include "core/animation/animatable/AnimatableDouble.h"
#include "core/animation/animatable/AnimatableUnknown.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/dom/Element.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

const double kDuration = 1.0;

PassRefPtr<AnimatableValue> UnknownAnimatableValue(double n) {
  return AnimatableUnknown::Create(
      CSSPrimitiveValue::Create(n, CSSPrimitiveValue::UnitType::kUnknown));
}

AnimatableValueKeyframeVector KeyframesAtZeroAndOne(
    PassRefPtr<AnimatableValue> zero_value,
    PassRefPtr<AnimatableValue> one_value) {
  AnimatableValueKeyframeVector keyframes(2);
  keyframes[0] = AnimatableValueKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetPropertyValue(CSSPropertyLeft, zero_value.Get());
  keyframes[1] = AnimatableValueKeyframe::Create();
  keyframes[1]->SetOffset(1.0);
  keyframes[1]->SetPropertyValue(CSSPropertyLeft, one_value.Get());
  return keyframes;
}

void ExpectProperty(CSSPropertyID property,
                    PassRefPtr<Interpolation> interpolation_value) {
  LegacyStyleInterpolation* interpolation =
      ToLegacyStyleInterpolation(interpolation_value.Get());
  ASSERT_EQ(property, interpolation->Id());
}

void ExpectDoubleValue(double expected_value,
                       PassRefPtr<Interpolation> interpolation_value) {
  LegacyStyleInterpolation* interpolation =
      ToLegacyStyleInterpolation(interpolation_value.Get());
  RefPtr<AnimatableValue> value = interpolation->CurrentValue();

  ASSERT_TRUE(value->IsDouble() || value->IsUnknown());

  double actual_value;
  if (value->IsDouble())
    actual_value = ToAnimatableDouble(value.Get())->ToDouble();
  else
    actual_value =
        ToCSSPrimitiveValue(ToAnimatableUnknown(value.Get())->ToCSSValue())
            ->GetDoubleValue();

  EXPECT_FLOAT_EQ(static_cast<float>(expected_value), actual_value);
}

Interpolation* FindValue(Vector<RefPtr<Interpolation>>& values,
                         CSSPropertyID id) {
  for (auto& value : values) {
    if (ToLegacyStyleInterpolation(value.Get())->Id() == id)
      return value.Get();
  }
  return 0;
}

TEST(AnimationKeyframeEffectModel, BasicOperation) {
  AnimatableValueKeyframeVector keyframes = KeyframesAtZeroAndOne(
      UnknownAnimatableValue(3.0), UnknownAnimatableValue(5.0));
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ASSERT_EQ(1UL, values.size());
  ExpectProperty(CSSPropertyLeft, values.at(0));
  ExpectDoubleValue(5.0, values.at(0));
}

TEST(AnimationKeyframeEffectModel, CompositeReplaceNonInterpolable) {
  AnimatableValueKeyframeVector keyframes = KeyframesAtZeroAndOne(
      UnknownAnimatableValue(3.0), UnknownAnimatableValue(5.0));
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectDoubleValue(5.0, values.at(0));
}

TEST(AnimationKeyframeEffectModel, CompositeReplace) {
  AnimatableValueKeyframeVector keyframes = KeyframesAtZeroAndOne(
      AnimatableDouble::Create(3.0), AnimatableDouble::Create(5.0));
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectDoubleValue(3.0 * 0.4 + 5.0 * 0.6, values.at(0));
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST(AnimationKeyframeEffectModel, DISABLED_CompositeAdd) {
  AnimatableValueKeyframeVector keyframes = KeyframesAtZeroAndOne(
      AnimatableDouble::Create(3.0), AnimatableDouble::Create(5.0));
  keyframes[0]->SetComposite(EffectModel::kCompositeAdd);
  keyframes[1]->SetComposite(EffectModel::kCompositeAdd);
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectDoubleValue((7.0 + 3.0) * 0.4 + (7.0 + 5.0) * 0.6, values.at(0));
}

TEST(AnimationKeyframeEffectModel, CompositeEaseIn) {
  AnimatableValueKeyframeVector keyframes = KeyframesAtZeroAndOne(
      AnimatableDouble::Create(3.0), AnimatableDouble::Create(5.0));
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[0]->SetEasing(CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE_IN));
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectDoubleValue(3.8579516, values.at(0));
  effect->Sample(0, 0.6, kDuration * 100, values);
  ExpectDoubleValue(3.8582394, values.at(0));
}

TEST(AnimationKeyframeEffectModel, CompositeCubicBezier) {
  AnimatableValueKeyframeVector keyframes = KeyframesAtZeroAndOne(
      AnimatableDouble::Create(3.0), AnimatableDouble::Create(5.0));
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[0]->SetEasing(CubicBezierTimingFunction::Create(0.42, 0, 0.58, 1));
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectDoubleValue(4.3363357, values.at(0));
  effect->Sample(0, 0.6, kDuration * 1000, values);
  ExpectDoubleValue(4.3362322, values.at(0));
}

TEST(AnimationKeyframeEffectModel, ExtrapolateReplaceNonInterpolable) {
  AnimatableValueKeyframeVector keyframes = KeyframesAtZeroAndOne(
      UnknownAnimatableValue(3.0), UnknownAnimatableValue(5.0));
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 1.6, kDuration, values);
  ExpectDoubleValue(5.0, values.at(0));
}

TEST(AnimationKeyframeEffectModel, ExtrapolateReplace) {
  AnimatableValueKeyframeVector keyframes = KeyframesAtZeroAndOne(
      AnimatableDouble::Create(3.0), AnimatableDouble::Create(5.0));
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 1.6, kDuration, values);
  ExpectDoubleValue(3.0 * -0.6 + 5.0 * 1.6, values.at(0));
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST(AnimationKeyframeEffectModel, DISABLED_ExtrapolateAdd) {
  AnimatableValueKeyframeVector keyframes = KeyframesAtZeroAndOne(
      AnimatableDouble::Create(3.0), AnimatableDouble::Create(5.0));
  keyframes[0]->SetComposite(EffectModel::kCompositeAdd);
  keyframes[1]->SetComposite(EffectModel::kCompositeAdd);
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 1.6, kDuration, values);
  ExpectDoubleValue((7.0 + 3.0) * -0.6 + (7.0 + 5.0) * 1.6, values.at(0));
}

TEST(AnimationKeyframeEffectModel, ZeroKeyframes) {
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(
          AnimatableValueKeyframeVector());
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.5, kDuration, values);
  EXPECT_TRUE(values.IsEmpty());
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST(AnimationKeyframeEffectModel, DISABLED_SingleKeyframeAtOffsetZero) {
  AnimatableValueKeyframeVector keyframes(1);
  keyframes[0] = AnimatableValueKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(3.0).Get());

  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectDoubleValue(3.0, values.at(0));
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST(AnimationKeyframeEffectModel, DISABLED_SingleKeyframeAtOffsetOne) {
  AnimatableValueKeyframeVector keyframes(1);
  keyframes[0] = AnimatableValueKeyframe::Create();
  keyframes[0]->SetOffset(1.0);
  keyframes[0]->SetPropertyValue(CSSPropertyLeft,
                                 AnimatableDouble::Create(5.0).Get());

  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectDoubleValue(7.0 * 0.4 + 5.0 * 0.6, values.at(0));
}

TEST(AnimationKeyframeEffectModel, MoreThanTwoKeyframes) {
  AnimatableValueKeyframeVector keyframes(3);
  keyframes[0] = AnimatableValueKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(3.0).Get());
  keyframes[1] = AnimatableValueKeyframe::Create();
  keyframes[1]->SetOffset(0.5);
  keyframes[1]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(4.0).Get());
  keyframes[2] = AnimatableValueKeyframe::Create();
  keyframes[2]->SetOffset(1.0);
  keyframes[2]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(5.0).Get());

  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.3, kDuration, values);
  ExpectDoubleValue(4.0, values.at(0));
  effect->Sample(0, 0.8, kDuration, values);
  ExpectDoubleValue(5.0, values.at(0));
}

TEST(AnimationKeyframeEffectModel, EndKeyframeOffsetsUnspecified) {
  AnimatableValueKeyframeVector keyframes(3);
  keyframes[0] = AnimatableValueKeyframe::Create();
  keyframes[0]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(3.0).Get());
  keyframes[1] = AnimatableValueKeyframe::Create();
  keyframes[1]->SetOffset(0.5);
  keyframes[1]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(4.0).Get());
  keyframes[2] = AnimatableValueKeyframe::Create();
  keyframes[2]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(5.0).Get());

  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.1, kDuration, values);
  ExpectDoubleValue(3.0, values.at(0));
  effect->Sample(0, 0.6, kDuration, values);
  ExpectDoubleValue(4.0, values.at(0));
  effect->Sample(0, 0.9, kDuration, values);
  ExpectDoubleValue(5.0, values.at(0));
}

TEST(AnimationKeyframeEffectModel, SampleOnKeyframe) {
  AnimatableValueKeyframeVector keyframes(3);
  keyframes[0] = AnimatableValueKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(3.0).Get());
  keyframes[1] = AnimatableValueKeyframe::Create();
  keyframes[1]->SetOffset(0.5);
  keyframes[1]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(4.0).Get());
  keyframes[2] = AnimatableValueKeyframe::Create();
  keyframes[2]->SetOffset(1.0);
  keyframes[2]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(5.0).Get());

  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.0, kDuration, values);
  ExpectDoubleValue(3.0, values.at(0));
  effect->Sample(0, 0.5, kDuration, values);
  ExpectDoubleValue(4.0, values.at(0));
  effect->Sample(0, 1.0, kDuration, values);
  ExpectDoubleValue(5.0, values.at(0));
}

TEST(AnimationKeyframeEffectModel, MultipleKeyframesWithSameOffset) {
  AnimatableValueKeyframeVector keyframes(9);
  keyframes[0] = AnimatableValueKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(0.0).Get());
  keyframes[1] = AnimatableValueKeyframe::Create();
  keyframes[1]->SetOffset(0.1);
  keyframes[1]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(1.0).Get());
  keyframes[2] = AnimatableValueKeyframe::Create();
  keyframes[2]->SetOffset(0.1);
  keyframes[2]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(2.0).Get());
  keyframes[3] = AnimatableValueKeyframe::Create();
  keyframes[3]->SetOffset(0.5);
  keyframes[3]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(3.0).Get());
  keyframes[4] = AnimatableValueKeyframe::Create();
  keyframes[4]->SetOffset(0.5);
  keyframes[4]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(4.0).Get());
  keyframes[5] = AnimatableValueKeyframe::Create();
  keyframes[5]->SetOffset(0.5);
  keyframes[5]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(5.0).Get());
  keyframes[6] = AnimatableValueKeyframe::Create();
  keyframes[6]->SetOffset(0.9);
  keyframes[6]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(6.0).Get());
  keyframes[7] = AnimatableValueKeyframe::Create();
  keyframes[7]->SetOffset(0.9);
  keyframes[7]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(7.0).Get());
  keyframes[8] = AnimatableValueKeyframe::Create();
  keyframes[8]->SetOffset(1.0);
  keyframes[8]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(7.0).Get());

  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.0, kDuration, values);
  ExpectDoubleValue(0.0, values.at(0));
  effect->Sample(0, 0.2, kDuration, values);
  ExpectDoubleValue(2.0, values.at(0));
  effect->Sample(0, 0.4, kDuration, values);
  ExpectDoubleValue(3.0, values.at(0));
  effect->Sample(0, 0.5, kDuration, values);
  ExpectDoubleValue(5.0, values.at(0));
  effect->Sample(0, 0.6, kDuration, values);
  ExpectDoubleValue(5.0, values.at(0));
  effect->Sample(0, 0.8, kDuration, values);
  ExpectDoubleValue(6.0, values.at(0));
  effect->Sample(0, 1.0, kDuration, values);
  ExpectDoubleValue(7.0, values.at(0));
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST(AnimationKeyframeEffectModel, DISABLED_PerKeyframeComposite) {
  AnimatableValueKeyframeVector keyframes(2);
  keyframes[0] = AnimatableValueKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetPropertyValue(CSSPropertyLeft,
                                 AnimatableDouble::Create(3.0).Get());
  keyframes[1] = AnimatableValueKeyframe::Create();
  keyframes[1]->SetOffset(1.0);
  keyframes[1]->SetPropertyValue(CSSPropertyLeft,
                                 AnimatableDouble::Create(5.0).Get());
  keyframes[1]->SetComposite(EffectModel::kCompositeAdd);

  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectDoubleValue(3.0 * 0.4 + (7.0 + 5.0) * 0.6, values.at(0));
}

TEST(AnimationKeyframeEffectModel, MultipleProperties) {
  AnimatableValueKeyframeVector keyframes(2);
  keyframes[0] = AnimatableValueKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(3.0).Get());
  keyframes[0]->SetPropertyValue(CSSPropertyRight,
                                 UnknownAnimatableValue(4.0).Get());
  keyframes[1] = AnimatableValueKeyframe::Create();
  keyframes[1]->SetOffset(1.0);
  keyframes[1]->SetPropertyValue(CSSPropertyLeft,
                                 UnknownAnimatableValue(5.0).Get());
  keyframes[1]->SetPropertyValue(CSSPropertyRight,
                                 UnknownAnimatableValue(6.0).Get());

  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  EXPECT_EQ(2UL, values.size());
  Interpolation* left_value = FindValue(values, CSSPropertyLeft);
  ASSERT_TRUE(left_value);
  ExpectDoubleValue(5.0, left_value);
  Interpolation* right_value = FindValue(values, CSSPropertyRight);
  ASSERT_TRUE(right_value);
  ExpectDoubleValue(6.0, right_value);
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST(AnimationKeyframeEffectModel, DISABLED_RecompositeCompositableValue) {
  AnimatableValueKeyframeVector keyframes = KeyframesAtZeroAndOne(
      AnimatableDouble::Create(3.0), AnimatableDouble::Create(5.0));
  keyframes[0]->SetComposite(EffectModel::kCompositeAdd);
  keyframes[1]->SetComposite(EffectModel::kCompositeAdd);
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectDoubleValue((7.0 + 3.0) * 0.4 + (7.0 + 5.0) * 0.6, values.at(0));
  ExpectDoubleValue((9.0 + 3.0) * 0.4 + (9.0 + 5.0) * 0.6, values.at(1));
}

TEST(AnimationKeyframeEffectModel, MultipleIterations) {
  AnimatableValueKeyframeVector keyframes = KeyframesAtZeroAndOne(
      AnimatableDouble::Create(1.0), AnimatableDouble::Create(3.0));
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0.5, kDuration, values);
  ExpectDoubleValue(2.0, values.at(0));
  effect->Sample(1, 0.5, kDuration, values);
  ExpectDoubleValue(2.0, values.at(0));
  effect->Sample(2, 0.5, kDuration, values);
  ExpectDoubleValue(2.0, values.at(0));
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST(AnimationKeyframeEffectModel, DISABLED_DependsOnUnderlyingValue) {
  AnimatableValueKeyframeVector keyframes(3);
  keyframes[0] = AnimatableValueKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetPropertyValue(CSSPropertyLeft,
                                 AnimatableDouble::Create(1.0).Get());
  keyframes[0]->SetComposite(EffectModel::kCompositeAdd);
  keyframes[1] = AnimatableValueKeyframe::Create();
  keyframes[1]->SetOffset(0.5);
  keyframes[1]->SetPropertyValue(CSSPropertyLeft,
                                 AnimatableDouble::Create(1.0).Get());
  keyframes[2] = AnimatableValueKeyframe::Create();
  keyframes[2]->SetOffset(1.0);
  keyframes[2]->SetPropertyValue(CSSPropertyLeft,
                                 AnimatableDouble::Create(1.0).Get());

  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  Vector<RefPtr<Interpolation>> values;
  effect->Sample(0, 0, kDuration, values);
  EXPECT_TRUE(values.at(0));
  effect->Sample(0, 0.1, kDuration, values);
  EXPECT_TRUE(values.at(0));
  effect->Sample(0, 0.25, kDuration, values);
  EXPECT_TRUE(values.at(0));
  effect->Sample(0, 0.4, kDuration, values);
  EXPECT_TRUE(values.at(0));
  effect->Sample(0, 0.5, kDuration, values);
  EXPECT_FALSE(values.at(0));
  effect->Sample(0, 0.6, kDuration, values);
  EXPECT_FALSE(values.at(0));
  effect->Sample(0, 0.75, kDuration, values);
  EXPECT_FALSE(values.at(0));
  effect->Sample(0, 0.8, kDuration, values);
  EXPECT_FALSE(values.at(0));
  effect->Sample(0, 1, kDuration, values);
  EXPECT_FALSE(values.at(0));
}

TEST(AnimationKeyframeEffectModel, AddSyntheticKeyframes) {
  StringKeyframeVector keyframes(1);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetOffset(0.5);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyLeft, "4px", nullptr);

  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  const StringPropertySpecificKeyframeVector& property_specific_keyframes =
      effect->GetPropertySpecificKeyframes(PropertyHandle(CSSPropertyLeft));
  EXPECT_EQ(3U, property_specific_keyframes.size());
  EXPECT_DOUBLE_EQ(0.0, property_specific_keyframes[0]->Offset());
  EXPECT_DOUBLE_EQ(0.5, property_specific_keyframes[1]->Offset());
  EXPECT_DOUBLE_EQ(1.0, property_specific_keyframes[2]->Offset());
}

TEST(AnimationKeyframeEffectModel, ToKeyframeEffectModel) {
  AnimatableValueKeyframeVector keyframes(0);
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(keyframes);

  EffectModel* base_effect = effect;
  EXPECT_TRUE(ToAnimatableValueKeyframeEffectModel(base_effect));
}

}  // namespace blink

namespace blink {

class KeyframeEffectModelTest : public ::testing::Test {
 public:
  static KeyframeVector NormalizedKeyframes(const KeyframeVector& keyframes) {
    return KeyframeEffectModelBase::NormalizedKeyframes(keyframes);
  }
};

TEST_F(KeyframeEffectModelTest, EvenlyDistributed1) {
  KeyframeVector keyframes(5);
  keyframes[0] = AnimatableValueKeyframe::Create();
  keyframes[0]->SetOffset(0.125);
  keyframes[1] = AnimatableValueKeyframe::Create();
  keyframes[2] = AnimatableValueKeyframe::Create();
  keyframes[3] = AnimatableValueKeyframe::Create();
  keyframes[4] = AnimatableValueKeyframe::Create();
  keyframes[4]->SetOffset(0.625);

  const KeyframeVector result = NormalizedKeyframes(keyframes);
  EXPECT_EQ(5U, result.size());
  EXPECT_DOUBLE_EQ(0.125, result[0]->Offset());
  EXPECT_DOUBLE_EQ(0.25, result[1]->Offset());
  EXPECT_DOUBLE_EQ(0.375, result[2]->Offset());
  EXPECT_DOUBLE_EQ(0.5, result[3]->Offset());
  EXPECT_DOUBLE_EQ(0.625, result[4]->Offset());
}

TEST_F(KeyframeEffectModelTest, EvenlyDistributed2) {
  KeyframeVector keyframes(6);
  keyframes[0] = AnimatableValueKeyframe::Create();
  keyframes[1] = AnimatableValueKeyframe::Create();
  keyframes[2] = AnimatableValueKeyframe::Create();
  keyframes[3] = AnimatableValueKeyframe::Create();
  keyframes[3]->SetOffset(0.75);
  keyframes[4] = AnimatableValueKeyframe::Create();
  keyframes[5] = AnimatableValueKeyframe::Create();

  const KeyframeVector result = NormalizedKeyframes(keyframes);
  EXPECT_EQ(6U, result.size());
  EXPECT_DOUBLE_EQ(0.0, result[0]->Offset());
  EXPECT_DOUBLE_EQ(0.25, result[1]->Offset());
  EXPECT_DOUBLE_EQ(0.5, result[2]->Offset());
  EXPECT_DOUBLE_EQ(0.75, result[3]->Offset());
  EXPECT_DOUBLE_EQ(0.875, result[4]->Offset());
  EXPECT_DOUBLE_EQ(1.0, result[5]->Offset());
}

TEST_F(KeyframeEffectModelTest, EvenlyDistributed3) {
  KeyframeVector keyframes(12);
  keyframes[0] = AnimatableValueKeyframe::Create();
  keyframes[0]->SetOffset(0);
  keyframes[1] = AnimatableValueKeyframe::Create();
  keyframes[2] = AnimatableValueKeyframe::Create();
  keyframes[3] = AnimatableValueKeyframe::Create();
  keyframes[4] = AnimatableValueKeyframe::Create();
  keyframes[4]->SetOffset(0.5);
  keyframes[5] = AnimatableValueKeyframe::Create();
  keyframes[6] = AnimatableValueKeyframe::Create();
  keyframes[7] = AnimatableValueKeyframe::Create();
  keyframes[7]->SetOffset(0.8);
  keyframes[8] = AnimatableValueKeyframe::Create();
  keyframes[9] = AnimatableValueKeyframe::Create();
  keyframes[10] = AnimatableValueKeyframe::Create();
  keyframes[11] = AnimatableValueKeyframe::Create();

  const KeyframeVector result = NormalizedKeyframes(keyframes);
  EXPECT_EQ(12U, result.size());
  EXPECT_DOUBLE_EQ(0.0, result[0]->Offset());
  EXPECT_DOUBLE_EQ(0.125, result[1]->Offset());
  EXPECT_DOUBLE_EQ(0.25, result[2]->Offset());
  EXPECT_DOUBLE_EQ(0.375, result[3]->Offset());
  EXPECT_DOUBLE_EQ(0.5, result[4]->Offset());
  EXPECT_DOUBLE_EQ(0.6, result[5]->Offset());
  EXPECT_DOUBLE_EQ(0.7, result[6]->Offset());
  EXPECT_DOUBLE_EQ(0.8, result[7]->Offset());
  EXPECT_DOUBLE_EQ(0.85, result[8]->Offset());
  EXPECT_DOUBLE_EQ(0.9, result[9]->Offset());
  EXPECT_DOUBLE_EQ(0.95, result[10]->Offset());
  EXPECT_DOUBLE_EQ(1.0, result[11]->Offset());
}

}  // namespace blink
