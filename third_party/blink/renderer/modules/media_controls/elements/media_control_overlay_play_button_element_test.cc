// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overlay_play_button_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class MediaControlOverlayPlayButtonElementTest : public PageTestBase {
 public:
  void SetUp() final {
    // Create page and instance of AnimatedArrow to run tests on.
    PageTestBase::SetUp();
    arrow_element_ = new MediaControlOverlayPlayButtonElement::AnimatedArrow(
        "test", GetDocument());
    GetDocument().body()->AppendChild(arrow_element_);
  }

 protected:
  void ExpectNotPresent() { EXPECT_FALSE(SVGElementIsPresent()); }

  void ExpectPresentAndShown() {
    EXPECT_TRUE(SVGElementIsPresent());
    EXPECT_FALSE(SVGElementHasDisplayValue());
  }

  void ExpectPresentAndHidden() {
    EXPECT_TRUE(SVGElementIsPresent());
    EXPECT_TRUE(SVGElementHasDisplayValue());
  }

  void SimulateShow() { arrow_element_->Show(); }

  void SimulateAnimationIteration() {
    Event* event = Event::Create(EventTypeNames::animationiteration);
    GetElementById("arrow-3")->DispatchEvent(*event);
  }

  Document& CreateTestDocumentWithBody() {
    Document* document = Document::CreateForTest();
    HTMLHtmlElement* html = HTMLHtmlElement::Create(*document);
    document->AppendChild(html);
    document->documentElement()->SetInnerHTMLFromString("<body></body>");
    return *document;
  }

  void CreateTestOverlayPlayButton(Document& test_document) {
    // Create a video element so that a MediaControlsImpl is created.
    HTMLVideoElement* media_element = HTMLVideoElement::Create(test_document);
    media_element->SetBooleanAttribute(HTMLNames::controlsAttr, true);
    test_document.body()->AppendChild(media_element);

    MediaControlsImpl* media_controls =
        static_cast<MediaControlsImpl*>(media_element->GetMediaControls());
    ASSERT_NE(nullptr, media_controls);

    // Create a MediaControlOverlayPlayButtonElement for testing.
    overlay_play_button_ =
        new MediaControlOverlayPlayButtonElement(*media_controls);
  }

  void SimulateKeepEventInNode() {
    MouseEventInit mouse_initializer;
    mouse_initializer.setView(GetDocument().domWindow());
    mouse_initializer.setButton(1);

    MouseEvent* mouse_event =
        MouseEvent::Create(nullptr, EventTypeNames::click, mouse_initializer);
    overlay_play_button_->KeepEventInNode(*mouse_event);
  }

 private:
  bool SVGElementHasDisplayValue() {
    return GetElementById("jump")->InlineStyle()->HasProperty(
        CSSPropertyDisplay);
  }

  bool SVGElementIsPresent() { return GetElementById("jump"); }

  Element* GetElementById(const AtomicString& id) {
    return GetDocument().body()->getElementById(id);
  }

  Persistent<MediaControlOverlayPlayButtonElement> overlay_play_button_;
  Persistent<MediaControlOverlayPlayButtonElement::AnimatedArrow>
      arrow_element_;
};

TEST_F(MediaControlOverlayPlayButtonElementTest, ShowIncrementsCounter) {
  ExpectNotPresent();

  // Start a new show.
  SimulateShow();
  ExpectPresentAndShown();

  // Increment the counter and finish the first show.
  SimulateShow();
  SimulateAnimationIteration();
  ExpectPresentAndShown();

  // Finish the second show.
  SimulateAnimationIteration();
  ExpectPresentAndHidden();

  // Start a new show.
  SimulateShow();
  ExpectPresentAndShown();
}

TEST_F(MediaControlOverlayPlayButtonElementTest,
       KeepEventInNodeWithoutLayoutViewDoesntCrash) {
  ScopedModernMediaControlsForTest enable_modern_media_controls(true);
  Document& document_without_layout_view = CreateTestDocumentWithBody();
  CreateTestOverlayPlayButton(document_without_layout_view);
  SimulateKeepEventInNode();
}

}  // namespace blink
