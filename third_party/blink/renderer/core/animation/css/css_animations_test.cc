// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/stub_chrome_client_for_spv2.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_delegate.h"

namespace blink {

class CSSAnimationsTest : public RenderingTest {
 public:
  CSSAnimationsTest()
      : chrome_client_(MakeGarbageCollected<StubChromeClientForSPv2>()) {
    EnablePlatform();
    platform()->SetThreadedAnimationEnabled(true);
  }

  void SetUp() override {
    platform()->SetAutoAdvanceNowToPendingTasks(false);
    // Advance timer manually as RenderingTest expects the time to be non-zero.
    platform()->AdvanceClockSeconds(1.);
    RenderingTest::SetUp();
    EnableCompositing();
  }

  void TearDown() override {
    platform()->SetAutoAdvanceNowToPendingTasks(true);
    platform()->RunUntilIdle();
  }

  ChromeClient& GetChromeClient() const override { return *chrome_client_; }

  void StartAnimationOnCompositor(Animation* animation) {
    static_cast<CompositorAnimationDelegate*>(animation)
        ->NotifyAnimationStarted(
            (CurrentTimeTicks() - base::TimeTicks()).InSecondsF(),
            animation->CompositorGroup());
  }

  void AdvanceClockSeconds(double seconds) {
    platform()->AdvanceClockSeconds(seconds);
    platform()->RunUntilIdle();
  }

  double GetContrastFilterAmount(Element* element) {
    EXPECT_EQ(1u, element->GetComputedStyle()->Filter().size());
    const FilterOperation* filter =
        element->GetComputedStyle()->Filter().Operations()[0];
    EXPECT_EQ(FilterOperation::OperationType::CONTRAST, filter->GetType());
    return static_cast<const BasicComponentTransferFilterOperation*>(filter)
        ->Amount();
  }

 private:
  Persistent<StubChromeClientForSPv2> chrome_client_;
};

// Verify that a composited animation is retargeted according to its composited
// time.
TEST_F(CSSAnimationsTest, RetargetedTransition) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #test { transition: filter linear 1s; }
      .contrast1 { filter: contrast(50%); }
      .contrast2 { filter: contrast(0%); }
    </style>
    <div id='test'></div>
  )HTML");
  Element* element = GetDocument().getElementById("test");
  element->setAttribute(html_names::kClassAttr, "contrast1");
  UpdateAllLifecyclePhasesForTest();
  ElementAnimations* animations = element->GetElementAnimations();
  EXPECT_EQ(1u, animations->Animations().size());
  Animation* animation = (*animations->Animations().begin()).key;
  // Start animation on compositor and advance .8 seconds.
  StartAnimationOnCompositor(animation);
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  AdvanceClockSeconds(0.8);

  // Starting the second transition should retarget the active transition.
  element->setAttribute(html_names::kClassAttr, "contrast2");
  GetPage().Animator().ServiceScriptedAnimations(CurrentTimeTicks());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_DOUBLE_EQ(0.6, GetContrastFilterAmount(element));

  // As it has been retargeted, advancing halfway should go to 0.3.
  AdvanceClockSeconds(0.5);
  GetPage().Animator().ServiceScriptedAnimations(CurrentTimeTicks());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_DOUBLE_EQ(0.3, GetContrastFilterAmount(element));
}

// Test that when an incompatible in progress compositor transition
// would be retargeted it does not incorrectly combine with a new
// transition target.
TEST_F(CSSAnimationsTest, IncompatibleRetargetedTransition) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #test { transition: filter 1s; }
      .saturate { filter: saturate(20%); }
      .contrast { filter: contrast(20%); }
    </style>
    <div id='test'></div>
  )HTML");
  Element* element = GetDocument().getElementById("test");
  element->setAttribute(html_names::kClassAttr, "saturate");
  UpdateAllLifecyclePhasesForTest();
  ElementAnimations* animations = element->GetElementAnimations();
  EXPECT_EQ(1u, animations->Animations().size());
  Animation* animation = (*animations->Animations().begin()).key;

  // Start animation on compositor and advance partially.
  StartAnimationOnCompositor(animation);
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  AdvanceClockSeconds(0.003);

  // The computed style still contains no filter until the next frame.
  EXPECT_TRUE(element->GetComputedStyle()->Filter().IsEmpty());

  // Now we start a contrast filter. Since it will try to combine with
  // the in progress saturate filter, and be incompatible, there should
  // be no transition and it should immediately apply on the next frame.
  element->setAttribute(html_names::kClassAttr, "contrast");
  EXPECT_TRUE(element->GetComputedStyle()->Filter().IsEmpty());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0.2, GetContrastFilterAmount(element));
}

}  // namespace blink
