// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animation_element.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/test/test_layer_animation_delegate.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace ui {

namespace {

// Check that the transformation element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, TransformElement) {
  TestLayerAnimationDelegate delegate;
  gfx::Transform start_transform, target_transform, middle_transform;
  start_transform.Rotate(-30.0);
  target_transform.Rotate(30.0);
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  scoped_ptr<LayerAnimationElement> element(
      LayerAnimationElement::CreateTransformElement(target_transform, delta));

  for (int i = 0; i < 2; ++i) {
    delegate.SetTransformFromAnimation(start_transform);
    element->Progress(base::TimeDelta(), &delegate);
    CheckApproximatelyEqual(start_transform,
                            delegate.GetTransformForAnimation());
    element->Progress(delta/2, &delegate);
    CheckApproximatelyEqual(middle_transform,
                            delegate.GetTransformForAnimation());
    element->Progress(delta, &delegate);
    CheckApproximatelyEqual(target_transform,
                            delegate.GetTransformForAnimation());
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  CheckApproximatelyEqual(target_transform, target_value.transform);

  base::TimeDelta element_duration;
  EXPECT_TRUE(element->IsFinished(delta, &element_duration));
  EXPECT_EQ(delta, element_duration);
}

// Check that the bounds element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, BoundsElement) {
  TestLayerAnimationDelegate delegate;
  gfx::Rect start, target, middle;
  start = target = middle = gfx::Rect(0, 0, 50, 50);
  start.set_x(-90);
  target.set_x(90);
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  scoped_ptr<LayerAnimationElement> element(
      LayerAnimationElement::CreateBoundsElement(target, delta));

  for (int i = 0; i < 2; ++i) {
    delegate.SetBoundsFromAnimation(start);
    element->Progress(base::TimeDelta(), &delegate);
    CheckApproximatelyEqual(start, delegate.GetBoundsForAnimation());
    element->Progress(delta/2, &delegate);
    CheckApproximatelyEqual(middle, delegate.GetBoundsForAnimation());
    element->Progress(delta, &delegate);
    CheckApproximatelyEqual(target, delegate.GetBoundsForAnimation());
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  CheckApproximatelyEqual(target, target_value.bounds);

  base::TimeDelta element_duration;
  EXPECT_TRUE(element->IsFinished(delta, &element_duration));
  EXPECT_EQ(delta, element_duration);
}

// Check that the opacity element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, OpacityElement) {
  TestLayerAnimationDelegate delegate;
  float start = 0.0;
  float middle = 0.5;
  float target = 1.0;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  scoped_ptr<LayerAnimationElement> element(
      LayerAnimationElement::CreateOpacityElement(target, delta));

  for (int i = 0; i < 2; ++i) {
    delegate.SetOpacityFromAnimation(start);
    element->Progress(base::TimeDelta(), &delegate);
    EXPECT_FLOAT_EQ(start, delegate.GetOpacityForAnimation());
    element->Progress(delta/2, &delegate);
    EXPECT_FLOAT_EQ(middle, delegate.GetOpacityForAnimation());
    element->Progress(delta, &delegate);
    EXPECT_FLOAT_EQ(target, delegate.GetOpacityForAnimation());
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  EXPECT_FLOAT_EQ(target, target_value.opacity);

  base::TimeDelta element_duration;
  EXPECT_TRUE(element->IsFinished(delta, &element_duration));
  EXPECT_EQ(delta, element_duration);
}

// Check that the visibility element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, VisibilityElement) {
  TestLayerAnimationDelegate delegate;
  bool start = true;
  bool target = false;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  scoped_ptr<LayerAnimationElement> element(
      LayerAnimationElement::CreateVisibilityElement(target, delta));

  for (int i = 0; i < 2; ++i) {
    delegate.SetVisibilityFromAnimation(start);
    element->Progress(base::TimeDelta(), &delegate);
    EXPECT_TRUE(delegate.GetVisibilityForAnimation());
    element->Progress(delta/2, &delegate);
    EXPECT_TRUE(delegate.GetVisibilityForAnimation());
    element->Progress(delta, &delegate);
    EXPECT_FALSE(delegate.GetVisibilityForAnimation());
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  EXPECT_FALSE(target_value.visibility);

  base::TimeDelta element_duration;
  EXPECT_TRUE(element->IsFinished(delta, &element_duration));
  EXPECT_EQ(delta, element_duration);
}

// Check that the Brightness element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, BrightnessElement) {
  TestLayerAnimationDelegate delegate;
  float start = 0.0;
  float middle = 0.5;
  float target = 1.0;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  scoped_ptr<LayerAnimationElement> element(
      LayerAnimationElement::CreateBrightnessElement(target, delta));

  for (int i = 0; i < 2; ++i) {
    delegate.SetBrightnessFromAnimation(start);
    element->Progress(base::TimeDelta(), &delegate);
    EXPECT_FLOAT_EQ(start, delegate.GetBrightnessForAnimation());
    element->Progress(delta/2, &delegate);
    EXPECT_FLOAT_EQ(middle, delegate.GetBrightnessForAnimation());
    element->Progress(delta, &delegate);
    EXPECT_FLOAT_EQ(target, delegate.GetBrightnessForAnimation());
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  EXPECT_FLOAT_EQ(target, target_value.brightness);

  base::TimeDelta element_duration;
  EXPECT_TRUE(element->IsFinished(delta, &element_duration));
  EXPECT_EQ(delta, element_duration);
}

// Check that the Grayscale element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, GrayscaleElement) {
  TestLayerAnimationDelegate delegate;
  float start = 0.0;
  float middle = 0.5;
  float target = 1.0;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  scoped_ptr<LayerAnimationElement> element(
      LayerAnimationElement::CreateGrayscaleElement(target, delta));

  for (int i = 0; i < 2; ++i) {
    delegate.SetGrayscaleFromAnimation(start);
    element->Progress(base::TimeDelta(), &delegate);
    EXPECT_FLOAT_EQ(start, delegate.GetGrayscaleForAnimation());
    element->Progress(delta/2, &delegate);
    EXPECT_FLOAT_EQ(middle, delegate.GetGrayscaleForAnimation());
    element->Progress(delta, &delegate);
    EXPECT_FLOAT_EQ(target, delegate.GetGrayscaleForAnimation());
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  EXPECT_FLOAT_EQ(target, target_value.grayscale);

  base::TimeDelta element_duration;
  EXPECT_TRUE(element->IsFinished(delta, &element_duration));
  EXPECT_EQ(delta, element_duration);
}

// Check that the pause element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, PauseElement) {
  LayerAnimationElement::AnimatableProperties properties;
  properties.insert(LayerAnimationElement::TRANSFORM);
  properties.insert(LayerAnimationElement::BOUNDS);
  properties.insert(LayerAnimationElement::OPACITY);
  properties.insert(LayerAnimationElement::BRIGHTNESS);
  properties.insert(LayerAnimationElement::GRAYSCALE);
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  scoped_ptr<LayerAnimationElement> element(
      LayerAnimationElement::CreatePauseElement(properties, delta));

  TestLayerAnimationDelegate delegate;
  TestLayerAnimationDelegate copy = delegate;

  element->Progress(delta, &delegate);

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

  // Pause should last for |delta|.
  base::TimeDelta element_duration;
  EXPECT_TRUE(element->IsFinished(delta, &element_duration));
  EXPECT_EQ(delta, element_duration);
}

} // namespace

} // namespace ui
