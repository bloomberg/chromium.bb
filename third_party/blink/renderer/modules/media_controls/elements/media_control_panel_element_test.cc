// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_panel_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

namespace blink {

class MediaControlPanelElementTest : public PageTestBase {
 public:
  void SetUp() final {
    // Create page and add a video element with controls.
    PageTestBase::SetUp();
    media_element_ = HTMLVideoElement::Create(GetDocument());
    media_element_->SetBooleanAttribute(HTMLNames::controlsAttr, true);
    GetDocument().body()->AppendChild(media_element_);

    // Create instance of MediaControlInputElement to run tests on.
    media_controls_ =
        static_cast<MediaControlsImpl*>(media_element_->GetMediaControls());
    ASSERT_NE(media_controls_, nullptr);
    panel_element_ = new MediaControlPanelElement(*media_controls_);
  }

 protected:
  void SimulateTransitionEnd() { TriggerEvent(EventTypeNames::transitionend); }

  void ExpectPanelIsDisplayed() { EXPECT_TRUE(GetPanel().IsWanted()); }

  void ExpectPanelIsNotDisplayed() { EXPECT_FALSE(GetPanel().IsWanted()); }

  MediaControlPanelElement& GetPanel() { return *panel_element_.Get(); }

 private:
  void TriggerEvent(const AtomicString& name) {
    Event* event = Event::Create(name);
    GetPanel().DispatchEvent(event);
  }

  Persistent<HTMLMediaElement> media_element_;
  Persistent<MediaControlsImpl> media_controls_;
  Persistent<MediaControlPanelElement> panel_element_;
};

TEST_F(MediaControlPanelElementTest, StateTransitions) {
  GetPanel().SetIsDisplayed(true);
  GetPanel().MakeOpaque();

  ExpectPanelIsDisplayed();
  SimulateTransitionEnd();

  GetPanel().MakeTransparent();

  SimulateTransitionEnd();
  ExpectPanelIsNotDisplayed();
}

}  // namespace blink
