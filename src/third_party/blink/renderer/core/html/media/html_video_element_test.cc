// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/html_video_element.h"

#include "cc/layers/layer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class HTMLVideoElementMockMediaPlayer : public EmptyWebMediaPlayer {
 public:
  MOCK_METHOD1(SetIsEffectivelyFullscreen, void(WebFullscreenVideoStatus));
  MOCK_METHOD1(OnDisplayTypeChanged, void(WebMediaPlayer::DisplayType));
};

class HTMLVideoElementTest : public PageTestBase {
 public:
  void SetUp() override {
    auto mock_media_player =
        std::make_unique<HTMLVideoElementMockMediaPlayer>();
    media_player_ = mock_media_player.get();

    SetupPageWithClients(
        nullptr,
        test::MediaStubLocalFrameClient::Create(std::move(mock_media_player)),
        nullptr);
    video_ = HTMLVideoElement::Create(GetDocument());
    GetDocument().body()->appendChild(video_);
  }

  void SetFakeCcLayer(cc::Layer* layer) { video_->SetCcLayer(layer); }

  HTMLVideoElement* video() { return video_.Get(); }

  HTMLVideoElementMockMediaPlayer* MockWebMediaPlayer() {
    return media_player_;
  }

 private:
  Persistent<HTMLVideoElement> video_;

  // Owned by HTMLVideoElementFrameClient.
  HTMLVideoElementMockMediaPlayer* media_player_;
};

TEST_F(HTMLVideoElementTest, PictureInPictureInterstitialAndTextContainer) {
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  SetFakeCcLayer(layer.get());

  video()->SetBooleanAttribute(html_names::kControlsAttr, true);
  video()->SetSrc("http://example.com/foo.mp4");
  test::RunPendingTasks();

  // Simulate the text track being displayed.
  video()->UpdateTextTrackDisplay();
  video()->UpdateTextTrackDisplay();

  // Simulate entering Picture-in-Picture.
  video()->OnEnteredPictureInPicture();

  // Simulate that text track are displayed again.
  video()->UpdateTextTrackDisplay();

  EXPECT_EQ(3u, video()->EnsureUserAgentShadowRoot().CountChildren());

  // Reset cc::layer to avoid crashes depending on timing.
  SetFakeCcLayer(nullptr);
}

TEST_F(HTMLVideoElementTest, PictureInPictureInterstitial_Reattach) {
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  SetFakeCcLayer(layer.get());

  video()->SetBooleanAttribute(html_names::kControlsAttr, true);
  video()->SetSrc("http://example.com/foo.mp4");
  test::RunPendingTasks();

  // Simulate entering Picture-in-Picture.
  video()->OnEnteredPictureInPicture();

  // Try detaching and reattaching. This should not crash.
  GetDocument().body()->removeChild(video());
  GetDocument().body()->appendChild(video());
  GetDocument().body()->removeChild(video());
}

TEST_F(HTMLVideoElementTest, EffectivelyFullscreen_DisplayType) {
  EXPECT_EQ(WebMediaPlayer::DisplayType::kInline, video()->DisplayType());

  // Vector of data to use for tests. First value is to be set when calling
  // SetIsEffectivelyFullscreen(). The second one is the expected DisplayType.
  // This is testing all possible values of WebFullscreenVideoStatus and then
  // sets the value back to a value that should put the DisplayType back to
  // inline.
  std::vector<std::pair<WebFullscreenVideoStatus, WebMediaPlayer::DisplayType>>
      tests = {
          {WebFullscreenVideoStatus::kNotEffectivelyFullscreen,
           WebMediaPlayer::DisplayType::kInline},
          {WebFullscreenVideoStatus::kFullscreenAndPictureInPictureEnabled,
           WebMediaPlayer::DisplayType::kFullscreen},
          {WebFullscreenVideoStatus::kFullscreenAndPictureInPictureDisabled,
           WebMediaPlayer::DisplayType::kFullscreen},
          {WebFullscreenVideoStatus::kNotEffectivelyFullscreen,
           WebMediaPlayer::DisplayType::kInline},
      };

  for (const auto& test : tests) {
    EXPECT_CALL(*MockWebMediaPlayer(), SetIsEffectivelyFullscreen(test.first));
    EXPECT_CALL(*MockWebMediaPlayer(), OnDisplayTypeChanged(test.second));
    video()->SetIsEffectivelyFullscreen(test.first);

    EXPECT_EQ(test.second, video()->DisplayType());
  }
}

}  // namespace blink
