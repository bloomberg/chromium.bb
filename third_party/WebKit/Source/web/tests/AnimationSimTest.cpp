// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/ElementAnimation.h"
#include "core/css/PropertyDescriptor.h"
#include "core/css/PropertyRegistration.h"
#include "public/web/WebScriptSource.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"
#include "wtf/CurrentTime.h"

namespace blink {

class AnimationSimTest : public SimTest {};

TEST_F(AnimationSimTest, CustomPropertyBaseComputedStyle) {
  // This is a regression test for bug where custom property animations failed
  // to disable the baseComputedStyle optimisation. When custom property
  // animations are in effect we lose the guarantee that the baseComputedStyle
  // optimisation relies on where the non-animated style rules always produce
  // the same ComputedStyle. This is not the case if they use var() references
  // to custom properties that are being animated.
  // The bug was that we never cleared the existing baseComputedStyle during a
  // custom property animation so the stale ComputedStyle object would hang
  // around and not be valid in the exit frame of the next custom property
  // animation.

  RuntimeEnabledFeatures::setCSSVariables2Enabled(true);
  RuntimeEnabledFeatures::setCSSAdditiveAnimationsEnabled(true);
  RuntimeEnabledFeatures::setStackedCSSPropertyAnimationsEnabled(true);

  webView().page()->animator().clock().disableSyntheticTimeForTesting();

  SimRequest mainResource("https://example.com/", "text/html");
  loadURL("https://example.com/");
  mainResource.complete("<div id=\"target\"></div>");

  Element* target = document().getElementById("target");

  // CSS.registerProperty({
  //   name: '--x',
  //   syntax: '<percentage>',
  //   initialValue: '0%',
  // })
  DummyExceptionStateForTesting exceptionState;
  PropertyDescriptor propertyDescriptor;
  propertyDescriptor.setName("--x");
  propertyDescriptor.setSyntax("<percentage>");
  propertyDescriptor.setInitialValue("0%");
  PropertyRegistration::registerProperty(&document(), propertyDescriptor,
                                         exceptionState);
  EXPECT_FALSE(exceptionState.hadException());

  // target.style.setProperty('--x', '100%');
  target->style()->setProperty("--x", "100%", emptyString, exceptionState);
  EXPECT_FALSE(exceptionState.hadException());

  // target.animate({'--x': '100%'}, 1000);
  RefPtr<StringKeyframe> keyframe = StringKeyframe::create();
  keyframe->setCSSPropertyValue("--x", document().propertyRegistry(), "100%",
                                document().elementSheet().contents());
  StringKeyframeVector keyframes;
  keyframes.push_back(keyframe.release());
  Timing timing;
  timing.iterationDuration = 1;  // Seconds.
  ElementAnimation::animate(
      *target, StringKeyframeEffectModel::create(keyframes), timing);

  // This sets the baseComputedStyle on the animation exit frame.
  compositor().beginFrame(1);
  compositor().beginFrame(1);

  // target.style.setProperty('--x', '0%');
  target->style()->setProperty("--x", "0%", emptyString, exceptionState);
  EXPECT_FALSE(exceptionState.hadException());

  // target.animate({'--x': '100%'}, 1000);
  keyframe = StringKeyframe::create();
  keyframe->setCSSPropertyValue("--x", document().propertyRegistry(), "100%",
                                document().elementSheet().contents());
  keyframes.clear();
  keyframes.push_back(keyframe.release());
  timing = Timing::defaults();
  timing.iterationDuration = 1;  // Seconds.
  ElementAnimation::animate(
      *target, StringKeyframeEffectModel::create(keyframes), timing);

  // This (previously) would not clear the existing baseComputedStyle and would
  // crash on the equality assertion in the exit frame when it tried to update
  // it.
  compositor().beginFrame(1);
  compositor().beginFrame(1);
}

}  // namespace blink
