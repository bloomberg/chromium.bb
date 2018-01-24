// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlToggleClosedCaptionsButtonElement.h"

#include "core/html/media/HTMLMediaElement.h"
#include "core/testing/PageTestBase.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

const char kTextTracksOffString[] = "Off";
const char kEnglishLabel[] = "English";

class LocalePlatformSupport : public TestingPlatformSupport {
 public:
  WebString QueryLocalizedString(WebLocalizedString::Name name) override {
    if (name == WebLocalizedString::kTextTracksOff)
      return kTextTracksOffString;
    return TestingPlatformSupport::QueryLocalizedString(name);
  }
};

}  // anonymous namespace

class MediaControlToggleClosedCaptionsButtonElementTest : public PageTestBase {
 public:
  void SetUp() final {
    RuntimeEnabledFeatures::SetModernMediaControlsEnabled(true);
    PageTestBase::SetUp();
    SetBodyInnerHTML("<video controls></video>");
    media_element_ =
        static_cast<HTMLMediaElement*>(GetDocument().body()->firstChild());
    media_controls_ =
        static_cast<MediaControlsImpl*>(media_element_->GetMediaControls());
    captions_overflow_button_ =
        new MediaControlToggleClosedCaptionsButtonElement(*media_controls_);
  }

 protected:
  HTMLMediaElement* MediaElement() { return media_element_; }
  void SelectTextTrack(unsigned index) {
    media_controls_->ShowTextTrackAtIndex(index);
  }
  void SelectOff() { media_controls_->DisableShowingTextTracks(); }
  String GetOverflowMenuSubtitleString() {
    return captions_overflow_button_->GetOverflowMenuSubtitleString();
  }

 private:
  Persistent<HTMLMediaElement> media_element_;
  Persistent<MediaControlsImpl> media_controls_;
  Persistent<MediaControlToggleClosedCaptionsButtonElement>
      captions_overflow_button_;
};

TEST_F(MediaControlToggleClosedCaptionsButtonElementTest,
       SubtitleStringMatchesSelectedTrack) {
  ScopedTestingPlatformSupport<LocalePlatformSupport> support;

  // Before any text tracks are added, the subtitle string should be null.
  EXPECT_EQ(String(), GetOverflowMenuSubtitleString());

  // After adding a text track, the subtitle string should be off.
  MediaElement()->addTextTrack("subtitles", kEnglishLabel, "en",
                               ASSERT_NO_EXCEPTION);
  EXPECT_EQ(kTextTracksOffString, GetOverflowMenuSubtitleString());

  // After selecting the text track, the subtitle string should match the label.
  SelectTextTrack(0);
  EXPECT_EQ(kEnglishLabel, GetOverflowMenuSubtitleString());

  // After selecting off, the subtitle string should be off again.
  SelectOff();
  EXPECT_EQ(kTextTracksOffString, GetOverflowMenuSubtitleString());
}

}  // namespace blink
