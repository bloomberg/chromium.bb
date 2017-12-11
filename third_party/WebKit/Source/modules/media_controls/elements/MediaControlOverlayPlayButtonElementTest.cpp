// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlOverlayPlayButtonElement.h"

#include "core/css/CSSPropertyValueSet.h"
#include "core/dom/events/Event.h"
#include "core/event_type_names.h"
#include "core/testing/PageTestBase.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MediaControlOverlayPlayButtonElementTest : public PageTestBase {
 public:
  void SetUp() final {
    // Create page and instance of AnimatedArrow to run tests on.
    PageTestBase::SetUp();
    arrow_element_ = new MediaControlOverlayPlayButtonElement::AnimatedArrow(
        "test", *GetDocument().body());
  }

 protected:
  void ExpectIsHidden() { EXPECT_TRUE(SVGElementHasDisplayValue()); }

  void ExpectIsShown() { EXPECT_FALSE(SVGElementHasDisplayValue()); }

  void SimulateShow() { arrow_element_->Show(); }

  void SimulateAnimationIteration() {
    Event* event = Event::Create(EventTypeNames::animationiteration);
    GetElementById("arrow-3").DispatchEvent(event);
  }

 private:
  bool SVGElementHasDisplayValue() {
    return GetElementById("jump").InlineStyle()->HasProperty(
        CSSPropertyDisplay);
  }

  Element& GetElementById(const AtomicString& id) {
    return *GetDocument().body()->getElementById(id);
  }

  Persistent<MediaControlOverlayPlayButtonElement::AnimatedArrow>
      arrow_element_;
};

TEST_F(MediaControlOverlayPlayButtonElementTest, ShowIncrementsCounter) {
  ExpectIsHidden();

  // Start a new show.
  SimulateShow();
  ExpectIsShown();

  // Increment the counter and finish the first show.
  SimulateShow();
  SimulateAnimationIteration();
  ExpectIsShown();

  // Finish the second show.
  SimulateAnimationIteration();
  ExpectIsHidden();

  // Start a new show.
  SimulateShow();
  ExpectIsShown();
}

}  // namespace blink
