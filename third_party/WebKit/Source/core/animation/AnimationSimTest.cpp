// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/ElementAnimation.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/PropertyDescriptor.h"
#include "core/css/PropertyRegistration.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/page/Page.h"
#include "core/testing/sim/SimCompositor.h"
#include "core/testing/sim/SimDisplayItemList.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/wtf/CurrentTime.h"
#include "public/web/WebScriptSource.h"

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

  ScopedCSSVariables2ForTest css_variables2(true);
  ScopedCSSAdditiveAnimationsForTest css_additive_animation(true);
  ScopedStackedCSSPropertyAnimationsForTest stacked_css_property_animation(
      true);

  WebView().GetPage()->Animator().Clock().DisableSyntheticTimeForTesting();

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<div id=\"target\"></div>");

  Element* target = GetDocument().getElementById("target");

  // CSS.registerProperty({
  //   name: '--x',
  //   syntax: '<percentage>',
  //   initialValue: '0%',
  // })
  DummyExceptionStateForTesting exception_state;
  PropertyDescriptor property_descriptor;
  property_descriptor.setName("--x");
  property_descriptor.setSyntax("<percentage>");
  property_descriptor.setInitialValue("0%");
  PropertyRegistration::registerProperty(&GetDocument(), property_descriptor,
                                         exception_state);
  EXPECT_FALSE(exception_state.HadException());

  // target.style.setProperty('--x', '100%');
  target->style()->setProperty("--x", "100%", g_empty_string, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  // target.animate({'--x': '100%'}, 1000);
  RefPtr<StringKeyframe> keyframe = StringKeyframe::Create();
  keyframe->SetCSSPropertyValue("--x", GetDocument().GetPropertyRegistry(),
                                "100%",
                                GetDocument().ElementSheet().Contents());
  StringKeyframeVector keyframes;
  keyframes.push_back(std::move(keyframe));
  Timing timing;
  timing.iteration_duration = 1;  // Seconds.
  ElementAnimation::animateInternal(
      *target, StringKeyframeEffectModel::Create(keyframes), timing);

  // This sets the baseComputedStyle on the animation exit frame.
  Compositor().BeginFrame(1);
  Compositor().BeginFrame(1);

  // target.style.setProperty('--x', '0%');
  target->style()->setProperty("--x", "0%", g_empty_string, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  // target.animate({'--x': '100%'}, 1000);
  keyframe = StringKeyframe::Create();
  keyframe->SetCSSPropertyValue("--x", GetDocument().GetPropertyRegistry(),
                                "100%",
                                GetDocument().ElementSheet().Contents());
  keyframes.clear();
  keyframes.push_back(std::move(keyframe));
  timing = Timing::Defaults();
  timing.iteration_duration = 1;  // Seconds.
  ElementAnimation::animateInternal(
      *target, StringKeyframeEffectModel::Create(keyframes), timing);

  // This (previously) would not clear the existing baseComputedStyle and would
  // crash on the equality assertion in the exit frame when it tried to update
  // it.
  Compositor().BeginFrame(1);
  Compositor().BeginFrame(1);
}

}  // namespace blink
