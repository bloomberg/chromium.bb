// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animation_element.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_layer_animation_delegate.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace ui {

namespace {

// Verify that the TargetValue(TestLayerAnimationDelegate*) constructor
// correctly assigns values. See www.crbug.com/483134.
TEST(TargetValueTest, VerifyLayerAnimationDelegateConstructor) {
  const gfx::Rect kBounds(1, 2, 3, 5);
  const gfx::Transform kTransform(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f);
  const float kOpacity = 1.235f;
  const bool kVisibility = false;
  const float kBrightness = 2.358f;
  const float kGrayscale = 2.5813f;
  const SkColor kColor = SK_ColorCYAN;

  TestLayerAnimationDelegate delegate;
  delegate.SetBoundsFromAnimation(kBounds);
  delegate.SetTransformFromAnimation(kTransform);
  delegate.SetOpacityFromAnimation(kOpacity);
  delegate.SetVisibilityFromAnimation(kVisibility);
  delegate.SetBrightnessFromAnimation(kBrightness);
  delegate.SetGrayscaleFromAnimation(kGrayscale);
  delegate.SetColorFromAnimation(kColor);

  LayerAnimationElement::TargetValue target_value(&delegate);

  EXPECT_EQ(kBounds, target_value.bounds);
  EXPECT_EQ(kTransform, target_value.transform);
  EXPECT_FLOAT_EQ(kOpacity, target_value.opacity);
  EXPECT_EQ(kVisibility, target_value.visibility);
  EXPECT_FLOAT_EQ(kBrightness, target_value.brightness);
  EXPECT_FLOAT_EQ(kGrayscale, target_value.grayscale);
  EXPECT_EQ(SK_ColorCYAN, target_value.color);
}

// Check that the transformation element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, TransformElement) {
  TestLayerAnimationDelegate delegate;
  gfx::Transform start_transform, target_transform;
  start_transform.Rotate(-30.0);
  target_transform.Rotate(30.0);
  base::TimeTicks start_time;
  base::TimeTicks effective_start_time;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateTransformElement(target_transform, delta);
  element->set_animation_group_id(1);

  for (int i = 0; i < 2; ++i) {
    start_time = effective_start_time + delta;
    element->set_requested_start_time(start_time);
    delegate.SetTransformFromAnimation(start_transform);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    CheckApproximatelyEqual(start_transform,
                            delegate.GetTransformForAnimation());
    effective_start_time = start_time + delta;
    element->set_effective_start_time(effective_start_time);
    element->Progress(effective_start_time, &delegate);
    EXPECT_FLOAT_EQ(0.0, element->last_progressed_fraction());
    element->Progress(effective_start_time + delta/2, &delegate);
    EXPECT_FLOAT_EQ(0.5, element->last_progressed_fraction());

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(effective_start_time + delta,
                                    &element_duration));
    EXPECT_EQ(2 * delta, element_duration);

    element->Progress(effective_start_time + delta, &delegate);
    EXPECT_FLOAT_EQ(1.0, element->last_progressed_fraction());
    CheckApproximatelyEqual(target_transform,
                            delegate.GetTransformForAnimation());
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  CheckApproximatelyEqual(target_transform, target_value.transform);
}

// Check that the bounds element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, BoundsElement) {
  TestLayerAnimationDelegate delegate;
  gfx::Rect start, target, middle;
  start = target = middle = gfx::Rect(0, 0, 50, 50);
  start.set_x(-90);
  target.set_x(90);
  base::TimeTicks start_time;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateBoundsElement(target, delta);

  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    element->set_requested_start_time(start_time);
    delegate.SetBoundsFromAnimation(start);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    CheckApproximatelyEqual(start, delegate.GetBoundsForAnimation());
    element->Progress(start_time + delta/2, &delegate);
    CheckApproximatelyEqual(middle, delegate.GetBoundsForAnimation());

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(start_time + delta, &element_duration));
    EXPECT_EQ(delta, element_duration);

    element->Progress(start_time + delta, &delegate);
    CheckApproximatelyEqual(target, delegate.GetBoundsForAnimation());
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  CheckApproximatelyEqual(target, target_value.bounds);
}

// Check that the opacity element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, OpacityElement) {
  TestLayerAnimationDelegate delegate;
  float start = 0.0;
  float middle = 0.5;
  float target = 1.0;
  base::TimeTicks start_time;
  base::TimeTicks effective_start_time;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateOpacityElement(target, delta);

  for (int i = 0; i < 2; ++i) {
    start_time = effective_start_time + delta;
    element->set_requested_start_time(start_time);
    delegate.SetOpacityFromAnimation(start);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    EXPECT_FLOAT_EQ(start, element->last_progressed_fraction());
    effective_start_time = start_time + delta;
    element->set_effective_start_time(effective_start_time);
    element->Progress(effective_start_time, &delegate);
    EXPECT_FLOAT_EQ(start, element->last_progressed_fraction());
    element->Progress(effective_start_time + delta/2, &delegate);
    EXPECT_FLOAT_EQ(middle, element->last_progressed_fraction());

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(effective_start_time + delta,
                                    &element_duration));
    EXPECT_EQ(2 * delta, element_duration);

    element->Progress(effective_start_time + delta, &delegate);
    EXPECT_FLOAT_EQ(target, element->last_progressed_fraction());
    EXPECT_FLOAT_EQ(target, delegate.GetOpacityForAnimation());
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  EXPECT_FLOAT_EQ(target, target_value.opacity);
}

// Check that the visibility element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, VisibilityElement) {
  TestLayerAnimationDelegate delegate;
  bool start = true;
  bool target = false;
  base::TimeTicks start_time;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateVisibilityElement(target, delta);

  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    element->set_requested_start_time(start_time);
    delegate.SetVisibilityFromAnimation(start);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    EXPECT_TRUE(delegate.GetVisibilityForAnimation());
    element->Progress(start_time + delta/2, &delegate);
    EXPECT_TRUE(delegate.GetVisibilityForAnimation());

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(start_time + delta, &element_duration));
    EXPECT_EQ(delta, element_duration);

    element->Progress(start_time + delta, &delegate);
    EXPECT_FALSE(delegate.GetVisibilityForAnimation());
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  EXPECT_FALSE(target_value.visibility);
}

// Check that the Brightness element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, BrightnessElement) {
  TestLayerAnimationDelegate delegate;
  float start = 0.0;
  float middle = 0.5;
  float target = 1.0;
  base::TimeTicks start_time;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateBrightnessElement(target, delta);

  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    element->set_requested_start_time(start_time);
    delegate.SetBrightnessFromAnimation(start);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    EXPECT_FLOAT_EQ(start, delegate.GetBrightnessForAnimation());
    element->Progress(start_time + delta/2, &delegate);
    EXPECT_FLOAT_EQ(middle, delegate.GetBrightnessForAnimation());

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(start_time + delta, &element_duration));
    EXPECT_EQ(delta, element_duration);

    element->Progress(start_time + delta, &delegate);
    EXPECT_FLOAT_EQ(target, delegate.GetBrightnessForAnimation());
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  EXPECT_FLOAT_EQ(target, target_value.brightness);
}

// Check that the Grayscale element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, GrayscaleElement) {
  TestLayerAnimationDelegate delegate;
  float start = 0.0;
  float middle = 0.5;
  float target = 1.0;
  base::TimeTicks start_time;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateGrayscaleElement(target, delta);

  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    element->set_requested_start_time(start_time);
    delegate.SetGrayscaleFromAnimation(start);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    EXPECT_FLOAT_EQ(start, delegate.GetGrayscaleForAnimation());
    element->Progress(start_time + delta/2, &delegate);
    EXPECT_FLOAT_EQ(middle, delegate.GetGrayscaleForAnimation());

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(start_time + delta, &element_duration));
    EXPECT_EQ(delta, element_duration);

    element->Progress(start_time + delta, &delegate);
    EXPECT_FLOAT_EQ(target, delegate.GetGrayscaleForAnimation());
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  EXPECT_FLOAT_EQ(target, target_value.grayscale);
}

// Check that the pause element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, PauseElement) {
  LayerAnimationElement::AnimatableProperties properties =
      LayerAnimationElement::TRANSFORM | LayerAnimationElement::BOUNDS |
      LayerAnimationElement::OPACITY | LayerAnimationElement::BRIGHTNESS |
      LayerAnimationElement::GRAYSCALE;

  base::TimeTicks start_time;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreatePauseElement(properties, delta);

  TestLayerAnimationDelegate delegate;
  TestLayerAnimationDelegate copy = delegate;

  start_time += delta;
  element->set_requested_start_time(start_time);
  element->Start(&delegate, 1);

  // Pause should last for |delta|.
  base::TimeDelta element_duration;
  EXPECT_TRUE(element->IsFinished(start_time + delta, &element_duration));
  EXPECT_EQ(delta, element_duration);

  element->Progress(start_time + delta, &delegate);

  // Nothing should have changed.
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(),
                          copy.GetBoundsForAnimation());
  CheckApproximatelyEqual(delegate.GetTransformForAnimation(),
                          copy.GetTransformForAnimation());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(),
                  copy.GetOpacityForAnimation());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(),
                  copy.GetBrightnessForAnimation());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(),
                  copy.GetGrayscaleForAnimation());
}

// Check that a threaded opacity element updates the delegate as expected when
// aborted.
TEST(LayerAnimationElementTest, AbortOpacityElement) {
  TestLayerAnimationDelegate delegate;
  float start = 0.0;
  float target = 1.0;
  base::TimeTicks start_time;
  base::TimeTicks effective_start_time;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateOpacityElement(target, delta);

  // Choose a non-linear Tween type.
  gfx::Tween::Type tween_type = gfx::Tween::EASE_IN;
  element->set_tween_type(tween_type);

  delegate.SetOpacityFromAnimation(start);

  // Aborting the element before it has started should not update the delegate.
  element->Abort(&delegate);
  EXPECT_FLOAT_EQ(start, delegate.GetOpacityForAnimation());

  start_time += delta;
  element->set_requested_start_time(start_time);
  element->Start(&delegate, 1);
  element->Progress(start_time, &delegate);
  effective_start_time = start_time + delta;
  element->set_effective_start_time(effective_start_time);
  element->Progress(effective_start_time, &delegate);
  element->Progress(effective_start_time + delta/2, &delegate);

  // Since the element has started, it should update the delegate when
  // aborted.
  element->Abort(&delegate);
  EXPECT_FLOAT_EQ(gfx::Tween::CalculateValue(tween_type, 0.5),
                  delegate.GetOpacityForAnimation());
}

// Check that a threaded transform element updates the delegate as expected when
// aborted.
TEST(LayerAnimationElementTest, AbortTransformElement) {
  TestLayerAnimationDelegate delegate;
  gfx::Transform start_transform, target_transform;
  start_transform.Rotate(-30.0);
  target_transform.Rotate(30.0);
  base::TimeTicks start_time;
  base::TimeTicks effective_start_time;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateTransformElement(target_transform, delta);

  // Choose a non-linear Tween type.
  gfx::Tween::Type tween_type = gfx::Tween::EASE_IN;
  element->set_tween_type(tween_type);

  delegate.SetTransformFromAnimation(start_transform);

  // Aborting the element before it has started should not update the delegate.
  element->Abort(&delegate);
  CheckApproximatelyEqual(start_transform, delegate.GetTransformForAnimation());

  start_time += delta;
  element->set_requested_start_time(start_time);
  element->Start(&delegate, 1);
  element->Progress(start_time, &delegate);
  effective_start_time = start_time + delta;
  element->set_effective_start_time(effective_start_time);
  element->Progress(effective_start_time, &delegate);
  element->Progress(effective_start_time + delta/2, &delegate);

  // Since the element has started, it should update the delegate when
  // aborted.
  element->Abort(&delegate);
  target_transform.Blend(start_transform,
                         gfx::Tween::CalculateValue(tween_type, 0.5));
  CheckApproximatelyEqual(target_transform,
                          delegate.GetTransformForAnimation());
}

TEST(LayerAnimationElementTest, ToString) {
  float target = 1.0;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateOpacityElement(target, delta);
  element->set_animation_group_id(42);
  // TODO(wkorman): Test varying last_progressed_fraction and
  // start_frame_number.
  EXPECT_EQ(
      "LayerAnimationElement{name=ThreadedOpacityTransition, id=1, group=42, "
      "last_progressed_fraction=0.00, start_frame_number=0}",
      element->ToString());
}

} // namespace

} // namespace ui
