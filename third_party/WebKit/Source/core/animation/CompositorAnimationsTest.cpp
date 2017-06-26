/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "core/animation/CompositorAnimations.h"

#include <memory>
#include "core/animation/Animation.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/CompositorPendingAnimations.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/animatable/AnimatableDouble.h"
#include "core/animation/animatable/AnimatableFilterOperations.h"
#include "core/animation/animatable/AnimatableTransform.h"
#include "core/dom/Document.h"
#include "core/layout/LayoutObject.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/style/FilterOperations.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorFloatKeyframe.h"
#include "platform/geometry/FloatBox.h"
#include "platform/geometry/IntSize.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/transforms/TransformOperations.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "platform/wtf/HashFunctions.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class AnimationCompositorAnimationsTest : public ::testing::Test {
 protected:
  RefPtr<TimingFunction> linear_timing_function_;
  RefPtr<TimingFunction> cubic_ease_timing_function_;
  RefPtr<TimingFunction> cubic_custom_timing_function_;
  RefPtr<TimingFunction> step_timing_function_;
  RefPtr<TimingFunction> frames_timing_function_;

  Timing timing_;
  CompositorAnimations::CompositorTiming compositor_timing_;
  std::unique_ptr<AnimatableValueKeyframeVector> keyframe_vector2_;
  Persistent<AnimatableValueKeyframeEffectModel> keyframe_animation_effect2_;
  std::unique_ptr<AnimatableValueKeyframeVector> keyframe_vector5_;
  Persistent<AnimatableValueKeyframeEffectModel> keyframe_animation_effect5_;

  Persistent<Document> document_;
  Persistent<Element> element_;
  Persistent<AnimationTimeline> timeline_;
  std::unique_ptr<DummyPageHolder> page_holder_;

  void SetUp() override {
    linear_timing_function_ = LinearTimingFunction::Shared();
    cubic_ease_timing_function_ = CubicBezierTimingFunction::Preset(
        CubicBezierTimingFunction::EaseType::EASE);
    cubic_custom_timing_function_ =
        CubicBezierTimingFunction::Create(1, 2, 3, 4);
    step_timing_function_ =
        StepsTimingFunction::Create(1, StepsTimingFunction::StepPosition::END);
    frames_timing_function_ = FramesTimingFunction::Create(2);

    timing_ = CreateCompositableTiming();
    compositor_timing_ = CompositorAnimations::CompositorTiming();
    // Make sure the CompositableTiming is really compositable, otherwise
    // most other tests will fail.
    ASSERT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));

    keyframe_vector2_ = CreateCompositableFloatKeyframeVector(2);
    keyframe_animation_effect2_ =
        AnimatableValueKeyframeEffectModel::Create(*keyframe_vector2_);

    keyframe_vector5_ = CreateCompositableFloatKeyframeVector(5);
    keyframe_animation_effect5_ =
        AnimatableValueKeyframeEffectModel::Create(*keyframe_vector5_);

    page_holder_ = DummyPageHolder::Create();
    document_ = &page_holder_->GetDocument();
    document_->GetAnimationClock().ResetTimeForTesting();

    timeline_ = AnimationTimeline::Create(document_.Get());
    timeline_->ResetForTesting();
    element_ = document_->createElement("test");
  }

 public:
  bool ConvertTimingForCompositor(const Timing& t,
                                  CompositorAnimations::CompositorTiming& out) {
    return CompositorAnimations::ConvertTimingForCompositor(t, 0, out, 1);
  }
  bool CanStartEffectOnCompositor(const Timing& timing,
                                  const EffectModel& effect) {
    return CompositorAnimations::CheckCanStartEffectOnCompositor(
               timing, *element_.Get(), nullptr, effect, 1)
        .Ok();
  }
  void GetAnimationOnCompositor(
      Timing& timing,
      AnimatableValueKeyframeEffectModel& effect,
      Vector<std::unique_ptr<CompositorAnimation>>& animations) {
    GetAnimationOnCompositor(timing, effect, animations, 1);
  }
  void GetAnimationOnCompositor(
      Timing& timing,
      AnimatableValueKeyframeEffectModel& effect,
      Vector<std::unique_ptr<CompositorAnimation>>& animations,
      double player_playback_rate) {
    CompositorAnimations::GetAnimationOnCompositor(
        timing, 0, std::numeric_limits<double>::quiet_NaN(), 0, effect,
        animations, player_playback_rate);
  }
  bool GetAnimationBounds(FloatBox& bounding_box,
                          const EffectModel& effect,
                          double min_value,
                          double max_value) {
    return CompositorAnimations::GetAnimatedBoundingBox(bounding_box, effect,
                                                        min_value, max_value);
  }

  bool DuplicateSingleKeyframeAndTestIsCandidateOnResult(
      AnimatableValueKeyframe* frame) {
    EXPECT_EQ(frame->Offset(), 0);
    AnimatableValueKeyframeVector frames;
    RefPtr<Keyframe> second = frame->CloneWithOffset(1);

    frames.push_back(frame);
    frames.push_back(ToAnimatableValueKeyframe(second.Get()));
    return CanStartEffectOnCompositor(
        timing_, *AnimatableValueKeyframeEffectModel::Create(frames));
  }

  // -------------------------------------------------------------------

  Timing CreateCompositableTiming() {
    Timing timing;
    timing.start_delay = 0;
    timing.fill_mode = Timing::FillMode::NONE;
    timing.iteration_start = 0;
    timing.iteration_count = 1;
    timing.iteration_duration = 1.0;
    timing.playback_rate = 1.0;
    timing.direction = Timing::PlaybackDirection::NORMAL;
    timing.timing_function = linear_timing_function_;
    return timing;
  }

  PassRefPtr<AnimatableValueKeyframe> CreateReplaceOpKeyframe(
      CSSPropertyID id,
      AnimatableValue* value,
      double offset = 0) {
    RefPtr<AnimatableValueKeyframe> keyframe =
        AnimatableValueKeyframe::Create();
    keyframe->SetPropertyValue(id, value);
    keyframe->SetComposite(EffectModel::kCompositeReplace);
    keyframe->SetOffset(offset);
    keyframe->SetEasing(LinearTimingFunction::Shared());
    return keyframe;
  }

  PassRefPtr<AnimatableValueKeyframe> CreateDefaultKeyframe(
      CSSPropertyID id,
      EffectModel::CompositeOperation op,
      double offset = 0) {
    RefPtr<AnimatableValue> value = nullptr;
    if (id == CSSPropertyTransform)
      value = AnimatableTransform::Create(TransformOperations(), 1);
    else
      value = AnimatableDouble::Create(10.0);

    RefPtr<AnimatableValueKeyframe> keyframe =
        CreateReplaceOpKeyframe(id, value.Get(), offset);
    keyframe->SetComposite(op);
    return keyframe;
  }

  std::unique_ptr<AnimatableValueKeyframeVector>
  CreateCompositableFloatKeyframeVector(size_t n) {
    Vector<double> values;
    for (size_t i = 0; i < n; i++) {
      values.push_back(static_cast<double>(i));
    }
    return CreateCompositableFloatKeyframeVector(values);
  }

  std::unique_ptr<AnimatableValueKeyframeVector>
  CreateCompositableFloatKeyframeVector(Vector<double>& values) {
    std::unique_ptr<AnimatableValueKeyframeVector> frames =
        WTF::WrapUnique(new AnimatableValueKeyframeVector);
    for (size_t i = 0; i < values.size(); i++) {
      double offset = 1.0 / (values.size() - 1) * i;
      RefPtr<AnimatableDouble> value = AnimatableDouble::Create(values[i]);
      frames->push_back(
          CreateReplaceOpKeyframe(CSSPropertyOpacity, value.Get(), offset)
              .Get());
    }
    return frames;
  }

  std::unique_ptr<AnimatableValueKeyframeVector>
  CreateCompositableTransformKeyframeVector(
      const Vector<TransformOperations>& values) {
    std::unique_ptr<AnimatableValueKeyframeVector> frames =
        WTF::WrapUnique(new AnimatableValueKeyframeVector);
    for (size_t i = 0; i < values.size(); ++i) {
      double offset = 1.0f / (values.size() - 1) * i;
      RefPtr<AnimatableTransform> value =
          AnimatableTransform::Create(values[i], 1);
      frames->push_back(
          CreateReplaceOpKeyframe(CSSPropertyTransform, value.Get(), offset)
              .Get());
    }
    return frames;
  }

  AnimatableValueKeyframeEffectModel* CreateKeyframeEffectModel(
      RefPtr<AnimatableValueKeyframe> from,
      RefPtr<AnimatableValueKeyframe> to,
      RefPtr<AnimatableValueKeyframe> c = nullptr,
      RefPtr<AnimatableValueKeyframe> d = nullptr) {
    EXPECT_EQ(from->Offset(), 0);
    AnimatableValueKeyframeVector frames;
    frames.push_back(from);
    EXPECT_LE(from->Offset(), to->Offset());
    frames.push_back(to);
    if (c) {
      EXPECT_LE(to->Offset(), c->Offset());
      frames.push_back(c);
    }
    if (d) {
      frames.push_back(d);
      EXPECT_LE(c->Offset(), d->Offset());
      EXPECT_EQ(d->Offset(), 1.0);
    } else {
      EXPECT_EQ(to->Offset(), 1.0);
    }
    if (!HasFatalFailure()) {
      return AnimatableValueKeyframeEffectModel::Create(frames);
    }
    return nullptr;
  }

  void SimulateFrame(double time) {
    document_->GetAnimationClock().UpdateTime(time);
    document_->GetCompositorPendingAnimations().Update(
        Optional<CompositorElementIdSet>(), false);
    timeline_->ServiceAnimations(kTimingUpdateForAnimationFrame);
  }

  std::unique_ptr<CompositorAnimation> ConvertToCompositorAnimation(
      AnimatableValueKeyframeEffectModel& effect,
      double player_playback_rate) {
    Vector<std::unique_ptr<CompositorAnimation>> result;
    GetAnimationOnCompositor(timing_, effect, result, player_playback_rate);
    DCHECK_EQ(1U, result.size());
    return std::move(result[0]);
  }

  std::unique_ptr<CompositorAnimation> ConvertToCompositorAnimation(
      AnimatableValueKeyframeEffectModel& effect) {
    return ConvertToCompositorAnimation(effect, 1.0);
  }

  void ExpectKeyframeTimingFunctionCubic(
      const CompositorFloatKeyframe& keyframe,
      const CubicBezierTimingFunction::EaseType ease_type) {
    auto keyframe_timing_function = keyframe.GetTimingFunctionForTesting();
    DCHECK_EQ(keyframe_timing_function->GetType(),
              TimingFunction::Type::CUBIC_BEZIER);
    const auto& cubic_timing_function =
        ToCubicBezierTimingFunction(*keyframe_timing_function);
    EXPECT_EQ(cubic_timing_function.GetEaseType(), ease_type);
  }
};

class LayoutObjectProxy : public LayoutObject {
 public:
  static LayoutObjectProxy* Create(Node* node) {
    return new LayoutObjectProxy(node);
  }

  static void Dispose(LayoutObjectProxy* proxy) { proxy->Destroy(); }

  const char* GetName() const override { return nullptr; }
  void UpdateLayout() override {}
  FloatRect LocalBoundingBoxRectForAccessibility() const { return FloatRect(); }

 private:
  explicit LayoutObjectProxy(Node* node) : LayoutObject(node) {}
};

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

TEST_F(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorKeyframeMultipleCSSProperties) {
  RefPtr<AnimatableValueKeyframe> keyframe_good_multiple =
      CreateDefaultKeyframe(CSSPropertyOpacity, EffectModel::kCompositeReplace);
  keyframe_good_multiple->SetPropertyValue(
      CSSPropertyTransform,
      AnimatableTransform::Create(TransformOperations(), 1).Get());
  EXPECT_TRUE(DuplicateSingleKeyframeAndTestIsCandidateOnResult(
      keyframe_good_multiple.Get()));

  RefPtr<AnimatableValueKeyframe> keyframe_bad_multiple_id =
      CreateDefaultKeyframe(CSSPropertyColor, EffectModel::kCompositeReplace);
  keyframe_bad_multiple_id->SetPropertyValue(
      CSSPropertyOpacity, AnimatableDouble::Create(10.0).Get());
  EXPECT_FALSE(DuplicateSingleKeyframeAndTestIsCandidateOnResult(
      keyframe_bad_multiple_id.Get()));
}

TEST_F(AnimationCompositorAnimationsTest,
       isNotCandidateForCompositorAnimationTransformDependsOnBoxSize) {
  TransformOperations ops;
  ops.Operations().push_back(TranslateTransformOperation::Create(
      Length(2, kFixed), Length(2, kFixed), TransformOperation::kTranslateX));
  RefPtr<AnimatableValueKeyframe> good_keyframe = CreateReplaceOpKeyframe(
      CSSPropertyTransform, AnimatableTransform::Create(ops, 1).Get());
  EXPECT_TRUE(
      DuplicateSingleKeyframeAndTestIsCandidateOnResult(good_keyframe.Get()));

  ops.Operations().push_back(TranslateTransformOperation::Create(
      Length(50, kPercent), Length(2, kFixed),
      TransformOperation::kTranslateX));
  RefPtr<AnimatableValueKeyframe> bad_keyframe = CreateReplaceOpKeyframe(
      CSSPropertyTransform, AnimatableTransform::Create(ops, 1).Get());
  EXPECT_FALSE(
      DuplicateSingleKeyframeAndTestIsCandidateOnResult(bad_keyframe.Get()));

  TransformOperations ops2;
  Length calc_length =
      Length(100, kPercent).Blend(Length(100, kFixed), 0.5, kValueRangeAll);
  ops2.Operations().push_back(TranslateTransformOperation::Create(
      calc_length, Length(0, kFixed), TransformOperation::kTranslateX));
  RefPtr<AnimatableValueKeyframe> bad_keyframe2 = CreateReplaceOpKeyframe(
      CSSPropertyTransform, AnimatableTransform::Create(ops2, 1).Get());
  EXPECT_FALSE(
      DuplicateSingleKeyframeAndTestIsCandidateOnResult(bad_keyframe2.Get()));
}

TEST_F(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorKeyframeEffectModel) {
  AnimatableValueKeyframeVector frames_same;
  frames_same.push_back(CreateDefaultKeyframe(CSSPropertyColor,
                                              EffectModel::kCompositeReplace,
                                              0.0)
                            .Get());
  frames_same.push_back(CreateDefaultKeyframe(CSSPropertyColor,
                                              EffectModel::kCompositeReplace,
                                              1.0)
                            .Get());
  EXPECT_FALSE(CanStartEffectOnCompositor(
      timing_, *AnimatableValueKeyframeEffectModel::Create(frames_same)));

  AnimatableValueKeyframeVector frames_mixed_properties;
  RefPtr<AnimatableValueKeyframe> keyframe = AnimatableValueKeyframe::Create();
  keyframe->SetOffset(0);
  keyframe->SetPropertyValue(CSSPropertyColor, AnimatableDouble::Create(0));
  keyframe->SetPropertyValue(CSSPropertyOpacity, AnimatableDouble::Create(0));
  frames_mixed_properties.push_back(std::move(keyframe));
  keyframe = AnimatableValueKeyframe::Create();
  keyframe->SetOffset(1);
  keyframe->SetPropertyValue(CSSPropertyColor, AnimatableDouble::Create(1));
  keyframe->SetPropertyValue(CSSPropertyOpacity, AnimatableDouble::Create(1));
  frames_mixed_properties.push_back(std::move(keyframe));
  EXPECT_FALSE(CanStartEffectOnCompositor(
      timing_,
      *AnimatableValueKeyframeEffectModel::Create(frames_mixed_properties)));
}

TEST_F(AnimationCompositorAnimationsTest, AnimatedBoundingBox) {
  Vector<TransformOperations> transform_vector;
  transform_vector.push_back(TransformOperations());
  transform_vector.back().Operations().push_back(
      TranslateTransformOperation::Create(Length(0, kFixed), Length(0, kFixed),
                                          0.0,
                                          TransformOperation::kTranslate3D));
  transform_vector.push_back(TransformOperations());
  transform_vector.back().Operations().push_back(
      TranslateTransformOperation::Create(Length(200, kFixed),
                                          Length(200, kFixed), 0.0,
                                          TransformOperation::kTranslate3D));
  std::unique_ptr<AnimatableValueKeyframeVector> frames =
      CreateCompositableTransformKeyframeVector(transform_vector);
  FloatBox bounds;
  EXPECT_TRUE(GetAnimationBounds(
      bounds, *AnimatableValueKeyframeEffectModel::Create(*frames), 0, 1));
  EXPECT_EQ(FloatBox(0.0f, 0.f, 0.0f, 200.0f, 200.0f, 0.0f), bounds);
  bounds = FloatBox();
  EXPECT_TRUE(GetAnimationBounds(
      bounds, *AnimatableValueKeyframeEffectModel::Create(*frames), -1, 1));
  EXPECT_EQ(FloatBox(-200.0f, -200.0, 0.0, 400.0f, 400.0f, 0.0f), bounds);
  transform_vector.push_back(TransformOperations());
  transform_vector.back().Operations().push_back(
      TranslateTransformOperation::Create(Length(-300, kFixed),
                                          Length(-400, kFixed), 1.0f,
                                          TransformOperation::kTranslate3D));
  bounds = FloatBox();
  frames = CreateCompositableTransformKeyframeVector(transform_vector);
  EXPECT_TRUE(GetAnimationBounds(
      bounds, *AnimatableValueKeyframeEffectModel::Create(*frames), 0, 1));
  EXPECT_EQ(FloatBox(-300.0f, -400.f, 0.0f, 500.0f, 600.0f, 1.0f), bounds);
  bounds = FloatBox();
  EXPECT_TRUE(GetAnimationBounds(
      bounds, *AnimatableValueKeyframeEffectModel::Create(*frames), -1, 2));
  EXPECT_EQ(FloatBox(-1300.0f, -1600.f, 0.0f, 1500.0f, 1800.0f, 3.0f), bounds);
}

TEST_F(AnimationCompositorAnimationsTest,
       ConvertTimingForCompositorStartDelay) {
  timing_.iteration_duration = 20.0;

  timing_.start_delay = 2.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(-2.0, compositor_timing_.scaled_time_offset);

  timing_.start_delay = -2.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(2.0, compositor_timing_.scaled_time_offset);
}

TEST_F(AnimationCompositorAnimationsTest,
       ConvertTimingForCompositorIterationStart) {
  timing_.iteration_start = 2.2;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
}

TEST_F(AnimationCompositorAnimationsTest,
       ConvertTimingForCompositorIterationCount) {
  timing_.iteration_count = 5.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_EQ(5, compositor_timing_.adjusted_iteration_count);

  timing_.iteration_count = 5.5;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_EQ(5.5, compositor_timing_.adjusted_iteration_count);

  timing_.iteration_count = std::numeric_limits<double>::infinity();
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_EQ(-1, compositor_timing_.adjusted_iteration_count);

  timing_.iteration_count = std::numeric_limits<double>::infinity();
  timing_.iteration_duration = 5.0;
  timing_.start_delay = -6.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(6.0, compositor_timing_.scaled_time_offset);
  EXPECT_EQ(-1, compositor_timing_.adjusted_iteration_count);
}

TEST_F(AnimationCompositorAnimationsTest,
       ConvertTimingForCompositorIterationsAndStartDelay) {
  timing_.iteration_count = 4.0;
  timing_.iteration_duration = 5.0;

  timing_.start_delay = 6.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(-6.0, compositor_timing_.scaled_time_offset);
  EXPECT_DOUBLE_EQ(4.0, compositor_timing_.adjusted_iteration_count);

  timing_.start_delay = -6.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(6.0, compositor_timing_.scaled_time_offset);
  EXPECT_DOUBLE_EQ(4.0, compositor_timing_.adjusted_iteration_count);

  timing_.start_delay = 21.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
}

TEST_F(AnimationCompositorAnimationsTest,
       ConvertTimingForCompositorPlaybackRate) {
  timing_.playback_rate = 1.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(1.0, compositor_timing_.playback_rate);

  timing_.playback_rate = -2.3;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(-2.3, compositor_timing_.playback_rate);

  timing_.playback_rate = 1.6;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(1.6, compositor_timing_.playback_rate);
}

TEST_F(AnimationCompositorAnimationsTest, ConvertTimingForCompositorDirection) {
  timing_.direction = Timing::PlaybackDirection::NORMAL;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_EQ(compositor_timing_.direction, Timing::PlaybackDirection::NORMAL);

  timing_.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_NORMAL);

  timing_.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_REVERSE);

  timing_.direction = Timing::PlaybackDirection::REVERSE;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_EQ(compositor_timing_.direction, Timing::PlaybackDirection::REVERSE);
}

TEST_F(AnimationCompositorAnimationsTest,
       ConvertTimingForCompositorDirectionIterationsAndStartDelay) {
  timing_.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  timing_.iteration_count = 4.0;
  timing_.iteration_duration = 5.0;
  timing_.start_delay = -6.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(6.0, compositor_timing_.scaled_time_offset);
  EXPECT_EQ(4, compositor_timing_.adjusted_iteration_count);
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_NORMAL);

  timing_.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  timing_.iteration_count = 4.0;
  timing_.iteration_duration = 5.0;
  timing_.start_delay = -11.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(11.0, compositor_timing_.scaled_time_offset);
  EXPECT_EQ(4, compositor_timing_.adjusted_iteration_count);
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_NORMAL);

  timing_.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  timing_.iteration_count = 4.0;
  timing_.iteration_duration = 5.0;
  timing_.start_delay = -6.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(6.0, compositor_timing_.scaled_time_offset);
  EXPECT_EQ(4, compositor_timing_.adjusted_iteration_count);
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_REVERSE);

  timing_.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  timing_.iteration_count = 4.0;
  timing_.iteration_duration = 5.0;
  timing_.start_delay = -11.0;
  EXPECT_TRUE(ConvertTimingForCompositor(timing_, compositor_timing_));
  EXPECT_DOUBLE_EQ(11.0, compositor_timing_.scaled_time_offset);
  EXPECT_EQ(4, compositor_timing_.adjusted_iteration_count);
  EXPECT_EQ(compositor_timing_.direction,
            Timing::PlaybackDirection::ALTERNATE_REVERSE);
}

TEST_F(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionLinear) {
  timing_.timing_function = linear_timing_function_;
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_));
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_));
}

TEST_F(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionCubic) {
  timing_.timing_function = cubic_ease_timing_function_;
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_));
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_));

  timing_.timing_function = cubic_custom_timing_function_;
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_));
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_));
}

TEST_F(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionSteps) {
  timing_.timing_function = step_timing_function_;
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_));
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_));
}

TEST_F(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionFrames) {
  timing_.timing_function = frames_timing_function_;
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_));
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_));
}

TEST_F(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionChainedLinear) {
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_));
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_));
}

TEST_F(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorNonLinearTimingFunctionOnFirstOrLastFrame) {
  (*keyframe_vector2_)[0]->SetEasing(cubic_ease_timing_function_.Get());
  keyframe_animation_effect2_ =
      AnimatableValueKeyframeEffectModel::Create(*keyframe_vector2_);

  (*keyframe_vector5_)[3]->SetEasing(cubic_ease_timing_function_.Get());
  keyframe_animation_effect5_ =
      AnimatableValueKeyframeEffectModel::Create(*keyframe_vector5_);

  timing_.timing_function = cubic_ease_timing_function_;
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_));
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_));

  timing_.timing_function = cubic_custom_timing_function_;
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_));
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_));
}

TEST_F(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionChainedCubicMatchingOffsets) {
  (*keyframe_vector2_)[0]->SetEasing(cubic_ease_timing_function_.Get());
  keyframe_animation_effect2_ =
      AnimatableValueKeyframeEffectModel::Create(*keyframe_vector2_);
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_));

  (*keyframe_vector2_)[0]->SetEasing(cubic_custom_timing_function_.Get());
  keyframe_animation_effect2_ =
      AnimatableValueKeyframeEffectModel::Create(*keyframe_vector2_);
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_));

  (*keyframe_vector5_)[0]->SetEasing(cubic_ease_timing_function_.Get());
  (*keyframe_vector5_)[1]->SetEasing(cubic_custom_timing_function_.Get());
  (*keyframe_vector5_)[2]->SetEasing(cubic_custom_timing_function_.Get());
  (*keyframe_vector5_)[3]->SetEasing(cubic_custom_timing_function_.Get());
  keyframe_animation_effect5_ =
      AnimatableValueKeyframeEffectModel::Create(*keyframe_vector5_);
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_));
}

TEST_F(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionMixedGood) {
  (*keyframe_vector5_)[0]->SetEasing(linear_timing_function_.Get());
  (*keyframe_vector5_)[1]->SetEasing(cubic_ease_timing_function_.Get());
  (*keyframe_vector5_)[2]->SetEasing(cubic_ease_timing_function_.Get());
  (*keyframe_vector5_)[3]->SetEasing(linear_timing_function_.Get());
  keyframe_animation_effect5_ =
      AnimatableValueKeyframeEffectModel::Create(*keyframe_vector5_);
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_));
}

TEST_F(AnimationCompositorAnimationsTest,
       CanStartEffectOnCompositorTimingFunctionWithStepOrFrameOkay) {
  (*keyframe_vector2_)[0]->SetEasing(step_timing_function_.Get());
  keyframe_animation_effect2_ =
      AnimatableValueKeyframeEffectModel::Create(*keyframe_vector2_);
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_));

  (*keyframe_vector2_)[0]->SetEasing(frames_timing_function_.Get());
  keyframe_animation_effect2_ =
      AnimatableValueKeyframeEffectModel::Create(*keyframe_vector2_);
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect2_));

  (*keyframe_vector5_)[0]->SetEasing(step_timing_function_.Get());
  (*keyframe_vector5_)[1]->SetEasing(linear_timing_function_.Get());
  (*keyframe_vector5_)[2]->SetEasing(cubic_ease_timing_function_.Get());
  (*keyframe_vector5_)[3]->SetEasing(frames_timing_function_.Get());
  keyframe_animation_effect5_ =
      AnimatableValueKeyframeEffectModel::Create(*keyframe_vector5_);
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_));

  (*keyframe_vector5_)[0]->SetEasing(frames_timing_function_.Get());
  (*keyframe_vector5_)[1]->SetEasing(step_timing_function_.Get());
  (*keyframe_vector5_)[2]->SetEasing(cubic_ease_timing_function_.Get());
  (*keyframe_vector5_)[3]->SetEasing(linear_timing_function_.Get());
  keyframe_animation_effect5_ =
      AnimatableValueKeyframeEffectModel::Create(*keyframe_vector5_);
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_));

  (*keyframe_vector5_)[0]->SetEasing(linear_timing_function_.Get());
  (*keyframe_vector5_)[1]->SetEasing(frames_timing_function_.Get());
  (*keyframe_vector5_)[2]->SetEasing(cubic_ease_timing_function_.Get());
  (*keyframe_vector5_)[3]->SetEasing(step_timing_function_.Get());
  keyframe_animation_effect5_ =
      AnimatableValueKeyframeEffectModel::Create(*keyframe_vector5_);
  EXPECT_TRUE(
      CanStartEffectOnCompositor(timing_, *keyframe_animation_effect5_));
}

TEST_F(AnimationCompositorAnimationsTest, CanStartEffectOnCompositor) {
  AnimatableValueKeyframeVector basic_frames_vector;
  basic_frames_vector.push_back(
      CreateDefaultKeyframe(CSSPropertyOpacity, EffectModel::kCompositeReplace,
                            0.0)
          .Get());
  basic_frames_vector.push_back(
      CreateDefaultKeyframe(CSSPropertyOpacity, EffectModel::kCompositeReplace,
                            1.0)
          .Get());

  AnimatableValueKeyframeVector non_basic_frames_vector;
  non_basic_frames_vector.push_back(
      CreateDefaultKeyframe(CSSPropertyOpacity, EffectModel::kCompositeReplace,
                            0.0)
          .Get());
  non_basic_frames_vector.push_back(
      CreateDefaultKeyframe(CSSPropertyOpacity, EffectModel::kCompositeReplace,
                            0.5)
          .Get());
  non_basic_frames_vector.push_back(
      CreateDefaultKeyframe(CSSPropertyOpacity, EffectModel::kCompositeReplace,
                            1.0)
          .Get());

  basic_frames_vector[0]->SetEasing(linear_timing_function_.Get());
  AnimatableValueKeyframeEffectModel* basic_frames =
      AnimatableValueKeyframeEffectModel::Create(basic_frames_vector);
  EXPECT_TRUE(CanStartEffectOnCompositor(timing_, *basic_frames));

  basic_frames_vector[0]->SetEasing(CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE_IN));
  basic_frames =
      AnimatableValueKeyframeEffectModel::Create(basic_frames_vector);
  EXPECT_TRUE(CanStartEffectOnCompositor(timing_, *basic_frames));

  non_basic_frames_vector[0]->SetEasing(linear_timing_function_.Get());
  non_basic_frames_vector[1]->SetEasing(CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE_IN));
  AnimatableValueKeyframeEffectModel* non_basic_frames =
      AnimatableValueKeyframeEffectModel::Create(non_basic_frames_vector);
  EXPECT_TRUE(CanStartEffectOnCompositor(timing_, *non_basic_frames));
}

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

TEST_F(AnimationCompositorAnimationsTest, createSimpleOpacityAnimation) {
  // KeyframeEffect to convert
  AnimatableValueKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(2.0).Get(), 0),
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(5.0).Get(), 1.0));

  std::unique_ptr<CompositorAnimation> animation =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->TargetProperty());
  EXPECT_EQ(1.0, animation->Iterations());
  EXPECT_EQ(0, animation->TimeOffset());
  EXPECT_EQ(CompositorAnimation::Direction::NORMAL, animation->GetDirection());
  EXPECT_EQ(1.0, animation->PlaybackRate());

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      animation->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time());
  EXPECT_EQ(2.0f, keyframes[0]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[0]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(1.0, keyframes[1]->Time());
  EXPECT_EQ(5.0f, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[1]->GetTimingFunctionForTesting()->GetType());
}

TEST_F(AnimationCompositorAnimationsTest,
       createSimpleOpacityAnimationDuration) {
  // KeyframeEffect to convert
  AnimatableValueKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(2.0).Get(), 0),
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(5.0).Get(), 1.0));

  const double kDuration = 10.0;
  timing_.iteration_duration = kDuration;

  std::unique_ptr<CompositorAnimation> animation =
      ConvertToCompositorAnimation(*effect);
  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      animation->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(kDuration, keyframes[1]->Time() * kDuration);
}

TEST_F(AnimationCompositorAnimationsTest,
       createMultipleKeyframeOpacityAnimationLinear) {
  // KeyframeEffect to convert
  AnimatableValueKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(2.0).Get(), 0),
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(-1.0).Get(), 0.25),
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(20.0).Get(), 0.5),
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(5.0).Get(), 1.0));

  timing_.iteration_count = 5;
  timing_.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  timing_.playback_rate = 2.0;

  std::unique_ptr<CompositorAnimation> animation =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->TargetProperty());
  EXPECT_EQ(5.0, animation->Iterations());
  EXPECT_EQ(0, animation->TimeOffset());
  EXPECT_EQ(CompositorAnimation::Direction::ALTERNATE_NORMAL,
            animation->GetDirection());
  EXPECT_EQ(2.0, animation->PlaybackRate());

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      animation->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(4UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time());
  EXPECT_EQ(2.0f, keyframes[0]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[0]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(0.25, keyframes[1]->Time());
  EXPECT_EQ(-1.0f, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[1]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(0.5, keyframes[2]->Time());
  EXPECT_EQ(20.0f, keyframes[2]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[2]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(1.0, keyframes[3]->Time());
  EXPECT_EQ(5.0f, keyframes[3]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[3]->GetTimingFunctionForTesting()->GetType());
}

TEST_F(AnimationCompositorAnimationsTest,
       createSimpleOpacityAnimationStartDelay) {
  // KeyframeEffect to convert
  AnimatableValueKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(2.0).Get(), 0),
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(5.0).Get(), 1.0));

  const double kStartDelay = 3.25;

  timing_.iteration_count = 5.0;
  timing_.iteration_duration = 1.75;
  timing_.start_delay = kStartDelay;

  std::unique_ptr<CompositorAnimation> animation =
      ConvertToCompositorAnimation(*effect);

  EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->TargetProperty());
  EXPECT_EQ(5.0, animation->Iterations());
  EXPECT_EQ(-kStartDelay, animation->TimeOffset());

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      animation->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(1.75, keyframes[1]->Time() * timing_.iteration_duration);
  EXPECT_EQ(5.0f, keyframes[1]->Value());
}

TEST_F(AnimationCompositorAnimationsTest,
       createMultipleKeyframeOpacityAnimationChained) {
  // KeyframeEffect to convert
  AnimatableValueKeyframeVector frames;
  frames.push_back(CreateReplaceOpKeyframe(
      CSSPropertyOpacity, AnimatableDouble::Create(2.0).Get(), 0));
  frames.push_back(CreateReplaceOpKeyframe(
      CSSPropertyOpacity, AnimatableDouble::Create(-1.0).Get(), 0.25));
  frames.push_back(CreateReplaceOpKeyframe(
      CSSPropertyOpacity, AnimatableDouble::Create(20.0).Get(), 0.5));
  frames.push_back(CreateReplaceOpKeyframe(
      CSSPropertyOpacity, AnimatableDouble::Create(5.0).Get(), 1.0));
  frames[0]->SetEasing(cubic_ease_timing_function_.Get());
  frames[1]->SetEasing(linear_timing_function_.Get());
  frames[2]->SetEasing(cubic_custom_timing_function_.Get());
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(frames);

  timing_.timing_function = linear_timing_function_.Get();
  timing_.iteration_duration = 2.0;
  timing_.iteration_count = 10;
  timing_.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;

  std::unique_ptr<CompositorAnimation> animation =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->TargetProperty());
  EXPECT_EQ(10.0, animation->Iterations());
  EXPECT_EQ(0, animation->TimeOffset());
  EXPECT_EQ(CompositorAnimation::Direction::ALTERNATE_NORMAL,
            animation->GetDirection());
  EXPECT_EQ(1.0, animation->PlaybackRate());

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      animation->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(4UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time() * timing_.iteration_duration);
  EXPECT_EQ(2.0f, keyframes[0]->Value());
  ExpectKeyframeTimingFunctionCubic(*keyframes[0],
                                    CubicBezierTimingFunction::EaseType::EASE);

  EXPECT_EQ(0.5, keyframes[1]->Time() * timing_.iteration_duration);
  EXPECT_EQ(-1.0f, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[1]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(1.0, keyframes[2]->Time() * timing_.iteration_duration);
  EXPECT_EQ(20.0f, keyframes[2]->Value());
  ExpectKeyframeTimingFunctionCubic(
      *keyframes[2], CubicBezierTimingFunction::EaseType::CUSTOM);

  EXPECT_EQ(2.0, keyframes[3]->Time() * timing_.iteration_duration);
  EXPECT_EQ(5.0f, keyframes[3]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[3]->GetTimingFunctionForTesting()->GetType());
}

TEST_F(AnimationCompositorAnimationsTest, createReversedOpacityAnimation) {
  RefPtr<TimingFunction> cubic_easy_flip_timing_function =
      CubicBezierTimingFunction::Create(0.0, 0.0, 0.0, 1.0);

  // KeyframeEffect to convert
  AnimatableValueKeyframeVector frames;
  frames.push_back(CreateReplaceOpKeyframe(
      CSSPropertyOpacity, AnimatableDouble::Create(2.0).Get(), 0));
  frames.push_back(CreateReplaceOpKeyframe(
      CSSPropertyOpacity, AnimatableDouble::Create(-1.0).Get(), 0.25));
  frames.push_back(CreateReplaceOpKeyframe(
      CSSPropertyOpacity, AnimatableDouble::Create(20.0).Get(), 0.5));
  frames.push_back(CreateReplaceOpKeyframe(
      CSSPropertyOpacity, AnimatableDouble::Create(5.0).Get(), 1.0));
  frames[0]->SetEasing(CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE_IN));
  frames[1]->SetEasing(linear_timing_function_.Get());
  frames[2]->SetEasing(cubic_easy_flip_timing_function.Get());
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(frames);

  timing_.timing_function = linear_timing_function_.Get();
  timing_.iteration_count = 10;
  timing_.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;

  std::unique_ptr<CompositorAnimation> animation =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->TargetProperty());
  EXPECT_EQ(10.0, animation->Iterations());
  EXPECT_EQ(0, animation->TimeOffset());
  EXPECT_EQ(CompositorAnimation::Direction::ALTERNATE_REVERSE,
            animation->GetDirection());
  EXPECT_EQ(1.0, animation->PlaybackRate());

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      animation->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(4UL, keyframes.size());

  EXPECT_EQ(keyframed_float_curve->GetTimingFunctionForTesting()->GetType(),
            TimingFunction::Type::LINEAR);

  EXPECT_EQ(0, keyframes[0]->Time());
  EXPECT_EQ(2.0f, keyframes[0]->Value());
  ExpectKeyframeTimingFunctionCubic(
      *keyframes[0], CubicBezierTimingFunction::EaseType::EASE_IN);

  EXPECT_EQ(0.25, keyframes[1]->Time());
  EXPECT_EQ(-1.0f, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[1]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(0.5, keyframes[2]->Time());
  EXPECT_EQ(20.0f, keyframes[2]->Value());
  ExpectKeyframeTimingFunctionCubic(
      *keyframes[2], CubicBezierTimingFunction::EaseType::CUSTOM);

  EXPECT_EQ(1.0, keyframes[3]->Time());
  EXPECT_EQ(5.0f, keyframes[3]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[3]->GetTimingFunctionForTesting()->GetType());
}

TEST_F(AnimationCompositorAnimationsTest,
       createReversedOpacityAnimationNegativeStartDelay) {
  // KeyframeEffect to convert
  AnimatableValueKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(2.0).Get(), 0),
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(5.0).Get(), 1.0));

  const double kNegativeStartDelay = -3;

  timing_.iteration_count = 5.0;
  timing_.iteration_duration = 1.5;
  timing_.start_delay = kNegativeStartDelay;
  timing_.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;

  std::unique_ptr<CompositorAnimation> animation =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->TargetProperty());
  EXPECT_EQ(5.0, animation->Iterations());
  EXPECT_EQ(-kNegativeStartDelay, animation->TimeOffset());
  EXPECT_EQ(CompositorAnimation::Direction::ALTERNATE_REVERSE,
            animation->GetDirection());
  EXPECT_EQ(1.0, animation->PlaybackRate());

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      animation->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(2UL, keyframes.size());
}

TEST_F(AnimationCompositorAnimationsTest,
       createSimpleOpacityAnimationPlaybackRates) {
  // KeyframeEffect to convert
  AnimatableValueKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(2.0).Get(), 0),
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(5.0).Get(), 1.0));

  const double kPlaybackRate = 2;
  const double kPlayerPlaybackRate = -1.5;

  timing_.playback_rate = kPlaybackRate;

  std::unique_ptr<CompositorAnimation> animation =
      ConvertToCompositorAnimation(*effect, kPlayerPlaybackRate);
  EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->TargetProperty());
  EXPECT_EQ(1.0, animation->Iterations());
  EXPECT_EQ(0, animation->TimeOffset());
  EXPECT_EQ(CompositorAnimation::Direction::NORMAL, animation->GetDirection());
  EXPECT_EQ(kPlaybackRate * kPlayerPlaybackRate, animation->PlaybackRate());

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      animation->FloatCurveForTesting();

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(2UL, keyframes.size());
}

TEST_F(AnimationCompositorAnimationsTest,
       createSimpleOpacityAnimationFillModeNone) {
  // KeyframeEffect to convert
  AnimatableValueKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(2.0).Get(), 0),
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(5.0).Get(), 1.0));

  timing_.fill_mode = Timing::FillMode::NONE;

  std::unique_ptr<CompositorAnimation> animation =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(CompositorAnimation::FillMode::NONE, animation->GetFillMode());
}

TEST_F(AnimationCompositorAnimationsTest,
       createSimpleOpacityAnimationFillModeAuto) {
  // KeyframeEffect to convert
  AnimatableValueKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(2.0).Get(), 0),
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(5.0).Get(), 1.0));

  timing_.fill_mode = Timing::FillMode::AUTO;

  std::unique_ptr<CompositorAnimation> animation =
      ConvertToCompositorAnimation(*effect);
  EXPECT_EQ(CompositorTargetProperty::OPACITY, animation->TargetProperty());
  EXPECT_EQ(1.0, animation->Iterations());
  EXPECT_EQ(0, animation->TimeOffset());
  EXPECT_EQ(CompositorAnimation::Direction::NORMAL, animation->GetDirection());
  EXPECT_EQ(1.0, animation->PlaybackRate());
  EXPECT_EQ(CompositorAnimation::FillMode::NONE, animation->GetFillMode());
}

TEST_F(AnimationCompositorAnimationsTest,
       createSimpleOpacityAnimationWithTimingFunction) {
  // KeyframeEffect to convert
  AnimatableValueKeyframeEffectModel* effect = CreateKeyframeEffectModel(
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(2.0).Get(), 0),
      CreateReplaceOpKeyframe(CSSPropertyOpacity,
                              AnimatableDouble::Create(5.0).Get(), 1.0));

  timing_.timing_function = cubic_custom_timing_function_;

  std::unique_ptr<CompositorAnimation> animation =
      ConvertToCompositorAnimation(*effect);

  std::unique_ptr<CompositorFloatAnimationCurve> keyframed_float_curve =
      animation->FloatCurveForTesting();

  auto curve_timing_function =
      keyframed_float_curve->GetTimingFunctionForTesting();
  EXPECT_EQ(curve_timing_function->GetType(),
            TimingFunction::Type::CUBIC_BEZIER);
  const auto& cubic_timing_function =
      ToCubicBezierTimingFunction(*curve_timing_function);
  EXPECT_EQ(cubic_timing_function.GetEaseType(),
            CubicBezierTimingFunction::EaseType::CUSTOM);
  EXPECT_EQ(cubic_timing_function.X1(), 1.0);
  EXPECT_EQ(cubic_timing_function.Y1(), 2.0);
  EXPECT_EQ(cubic_timing_function.X2(), 3.0);
  EXPECT_EQ(cubic_timing_function.Y2(), 4.0);

  CompositorFloatAnimationCurve::Keyframes keyframes =
      keyframed_float_curve->KeyframesForTesting();
  ASSERT_EQ(2UL, keyframes.size());

  EXPECT_EQ(0, keyframes[0]->Time());
  EXPECT_EQ(2.0f, keyframes[0]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[0]->GetTimingFunctionForTesting()->GetType());

  EXPECT_EQ(1.0, keyframes[1]->Time());
  EXPECT_EQ(5.0f, keyframes[1]->Value());
  EXPECT_EQ(TimingFunction::Type::LINEAR,
            keyframes[1]->GetTimingFunctionForTesting()->GetType());
}

TEST_F(AnimationCompositorAnimationsTest,
       cancelIncompatibleCompositorAnimations) {
  Persistent<Element> element = document_->createElement("shared");

  LayoutObjectProxy* layout_object = LayoutObjectProxy::Create(element.Get());
  element->SetLayoutObject(layout_object);

  AnimatableValueKeyframeVector key_frames;
  key_frames.push_back(CreateDefaultKeyframe(CSSPropertyOpacity,
                                             EffectModel::kCompositeReplace,
                                             0.0)
                           .Get());
  key_frames.push_back(CreateDefaultKeyframe(CSSPropertyOpacity,
                                             EffectModel::kCompositeReplace,
                                             1.0)
                           .Get());
  EffectModel* animation_effect1 =
      AnimatableValueKeyframeEffectModel::Create(key_frames);
  EffectModel* animation_effect2 =
      AnimatableValueKeyframeEffectModel::Create(key_frames);

  Timing timing;
  timing.iteration_duration = 1.f;

  // The first animation for opacity is ok to run on compositor.
  KeyframeEffect* keyframe_effect1 =
      KeyframeEffect::Create(element.Get(), animation_effect1, timing);
  Animation* animation1 = timeline_->Play(keyframe_effect1);
  EXPECT_TRUE(CompositorAnimations::CheckCanStartEffectOnCompositor(
                  timing, *element.Get(), animation1, *animation_effect1, 1)
                  .Ok());

  // simulate KeyframeEffect::maybeStartAnimationOnCompositor
  Vector<int> compositor_animation_ids;
  compositor_animation_ids.push_back(1);
  keyframe_effect1->SetCompositorAnimationIdsForTesting(
      compositor_animation_ids);
  EXPECT_TRUE(animation1->HasActiveAnimationsOnCompositor());

  // The second animation for opacity is not ok to run on compositor.
  KeyframeEffect* keyframe_effect2 =
      KeyframeEffect::Create(element.Get(), animation_effect2, timing);
  Animation* animation2 = timeline_->Play(keyframe_effect2);
  EXPECT_FALSE(CompositorAnimations::CheckCanStartEffectOnCompositor(
                   timing, *element.Get(), animation2, *animation_effect2, 1)
                   .Ok());
  EXPECT_FALSE(animation2->HasActiveAnimationsOnCompositor());

  // A fallback to blink implementation needed, so cancel all compositor-side
  // opacity animations for this element.
  animation2->CancelIncompatibleAnimationsOnCompositor();

  EXPECT_FALSE(animation1->HasActiveAnimationsOnCompositor());
  EXPECT_FALSE(animation2->HasActiveAnimationsOnCompositor());

  SimulateFrame(0);
  EXPECT_EQ(2U, element->GetElementAnimations()->Animations().size());
  SimulateFrame(1.);

  element->SetLayoutObject(nullptr);
  LayoutObjectProxy::Dispose(layout_object);

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_TRUE(element->GetElementAnimations()->Animations().IsEmpty());
}

namespace {

void UpdateDummyTransformNode(ObjectPaintProperties& properties,
                              CompositingReasons reasons) {
  properties.UpdateTransform(TransformPaintPropertyNode::Root(),
                             TransformationMatrix(), FloatPoint3D(), false, 0,
                             reasons);
}

void UpdateDummyEffectNode(ObjectPaintProperties& properties,
                           CompositingReasons reasons) {
  properties.UpdateEffect(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver, reasons);
}

}  // namespace

TEST_F(AnimationCompositorAnimationsTest,
       canStartElementOnCompositorTransformSPv2) {
  Persistent<Element> element = document_->createElement("shared");
  LayoutObjectProxy* layout_object = LayoutObjectProxy::Create(element.Get());
  element->SetLayoutObject(layout_object);

  ScopedSlimmingPaintV2ForTest enable_s_pv2(true);
  auto& properties =
      layout_object->GetMutableForPainting().EnsurePaintProperties();

  // Add a transform with a compositing reason, which should allow starting
  // animation.
  UpdateDummyTransformNode(properties, kCompositingReasonActiveAnimation);
  EXPECT_TRUE(
      CompositorAnimations::CheckCanStartElementOnCompositor(*element).Ok());

  // Setting to CompositingReasonNone should produce false.
  UpdateDummyTransformNode(properties, kCompositingReasonNone);
  EXPECT_FALSE(
      CompositorAnimations::CheckCanStartElementOnCompositor(*element).Ok());

  // Clearing the transform node entirely should also produce false.
  properties.ClearTransform();
  EXPECT_FALSE(
      CompositorAnimations::CheckCanStartElementOnCompositor(*element).Ok());

  element->SetLayoutObject(nullptr);
  LayoutObjectProxy::Dispose(layout_object);
}

TEST_F(AnimationCompositorAnimationsTest,
       canStartElementOnCompositorEffectSPv2) {
  Persistent<Element> element = document_->createElement("shared");
  LayoutObjectProxy* layout_object = LayoutObjectProxy::Create(element.Get());
  element->SetLayoutObject(layout_object);

  ScopedSlimmingPaintV2ForTest enable_s_pv2(true);
  auto& properties =
      layout_object->GetMutableForPainting().EnsurePaintProperties();

  // Add an effect with a compositing reason, which should allow starting
  // animation.
  UpdateDummyEffectNode(properties, kCompositingReasonActiveAnimation);
  EXPECT_TRUE(
      CompositorAnimations::CheckCanStartElementOnCompositor(*element).Ok());

  // Setting to CompositingReasonNone should produce false.
  UpdateDummyEffectNode(properties, kCompositingReasonNone);
  EXPECT_FALSE(
      CompositorAnimations::CheckCanStartElementOnCompositor(*element).Ok());

  // Clearing the effect node entirely should also produce false.
  properties.ClearEffect();
  EXPECT_FALSE(
      CompositorAnimations::CheckCanStartElementOnCompositor(*element).Ok());

  element->SetLayoutObject(nullptr);
  LayoutObjectProxy::Dispose(layout_object);
}

}  // namespace blink
