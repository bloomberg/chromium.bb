// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/non_touch/media_controls_non_touch_impl.h"

#include <memory>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"

namespace blink {

namespace {

class MockWebMediaPlayerForNonTouchImpl : public EmptyWebMediaPlayer {
 public:
  bool HasVideo() const override { return true; }
};

class MediaControlsNonTouchImplTest : public PageTestBase {
 protected:
  void SetUp() override { InitializePage(); }

  void InitializePage() {
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = MakeGarbageCollected<EmptyChromeClient>();
    SetupPageWithClients(
        &clients, test::MediaStubLocalFrameClient::Create(
                      std::make_unique<MockWebMediaPlayerForNonTouchImpl>()));

    GetDocument().write("<video>");
    HTMLMediaElement& video =
        ToHTMLVideoElement(*GetDocument().QuerySelector("video"));
    media_controls_ = MediaControlsNonTouchImpl::Create(
        video, video.EnsureUserAgentShadowRoot());
  }

  MediaControlsNonTouchImpl& MediaControls() { return *media_controls_; }

  void SimulateKeydownEvent(Element& element, int key_code) {
    KeyboardEventInit* keyboard_event_init = KeyboardEventInit::Create();
    keyboard_event_init->setKeyCode(key_code);

    Event* keyboard_event =
        MakeGarbageCollected<KeyboardEvent>("keydown", keyboard_event_init);
    element.DispatchEvent(*keyboard_event);
  }

 private:
  Persistent<MediaControlsNonTouchImpl> media_controls_;
};

TEST_F(MediaControlsNonTouchImplTest, PlayPause) {
  MediaControls().MediaElement().SetFocused(true,
                                            WebFocusType::kWebFocusTypeNone);
  MediaControls().MediaElement().Play();
  ASSERT_FALSE(MediaControls().MediaElement().paused());

  // Press center key and video should be paused.
  SimulateKeydownEvent(MediaControls().MediaElement(), VKEY_RETURN);
  ASSERT_TRUE(MediaControls().MediaElement().paused());

  // Press center key and video should be played.
  SimulateKeydownEvent(MediaControls().MediaElement(), VKEY_RETURN);
  ASSERT_FALSE(MediaControls().MediaElement().paused());
}

}  // namespace

}  // namespace blink
