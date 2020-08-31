// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/html_video_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/fake_local_frame_host.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

// Override a FakeLocalFrameHost so that we can enter and exit the fullscreen
// on the appropriate request calls.
class VideoAutoFullscreenFrameHost : public FakeLocalFrameHost {
 public:
  VideoAutoFullscreenFrameHost() = default;

  void EnterFullscreen(mojom::blink::FullscreenOptionsPtr options,
                       EnterFullscreenCallback callback) override {
    std::move(callback).Run(true);
    Thread::Current()->GetTaskRunner()->PostTask(
        FROM_HERE,
        WTF::Bind(
            [](WebWidget* web_widget) { web_widget->DidEnterFullscreen(); },
            WTF::Unretained(web_widget_)));
  }

  void ExitFullscreen() override {
    Thread::Current()->GetTaskRunner()->PostTask(
        FROM_HERE,
        WTF::Bind(
            [](WebWidget* web_widget) { web_widget->DidExitFullscreen(); },
            WTF::Unretained(web_widget_)));
  }

  void set_frame_widget(WebWidget* web_widget) { web_widget_ = web_widget; }

 private:
  WebWidget* web_widget_;
};

class VideoAutoFullscreenFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  WebMediaPlayer* CreateMediaPlayer(const WebMediaPlayerSource&,
                                    WebMediaPlayerClient*,
                                    blink::MediaInspectorContext*,
                                    WebMediaPlayerEncryptedMediaClient*,
                                    WebContentDecryptionModule*,
                                    const WebString& sink_id) final {
    return new EmptyWebMediaPlayer();
  }
};

class VideoAutoFullscreen : public testing::Test,
                            private ScopedVideoAutoFullscreenForTest {
 public:
  VideoAutoFullscreen() : ScopedVideoAutoFullscreenForTest(true) {}
  void SetUp() override {
    web_view_helper_.Initialize(&web_frame_client_);
    frame_host_.Init(
        web_frame_client_.GetRemoteNavigationAssociatedInterfaces());
    GetWebView()->GetSettings()->SetAutoplayPolicy(
        WebSettings::AutoplayPolicy::kUserGestureRequired);

    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(), "about:blank");
    GetDocument()->write("<body><video></video></body>");

    video_ = To<HTMLVideoElement>(*GetDocument()->QuerySelector("video"));

    frame_host_.set_frame_widget(GetWebView()->MainFrameWidget());
  }

  WebViewImpl* GetWebView() { return web_view_helper_.GetWebView(); }

  Document* GetDocument() {
    return web_view_helper_.GetWebView()->MainFrameImpl()->GetDocument();
  }

  LocalFrame* GetFrame() { return GetDocument()->GetFrame(); }

  HTMLVideoElement* Video() const { return video_.Get(); }

 private:
  Persistent<HTMLVideoElement> video_;
  VideoAutoFullscreenFrameHost frame_host_;
  VideoAutoFullscreenFrameClient web_frame_client_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(VideoAutoFullscreen, PlayTriggersFullscreenWithoutPlaysInline) {
  Video()->SetSrc("http://example.com/foo.mp4");

  LocalFrame::NotifyUserActivation(GetFrame());
  Video()->Play();

  MakeGarbageCollected<WaitForEvent>(Video(), event_type_names::kPlay);
  test::RunPendingTasks();

  EXPECT_TRUE(Video()->IsFullscreen());
}

TEST_F(VideoAutoFullscreen, PlayDoesNotTriggerFullscreenWithPlaysInline) {
  Video()->SetBooleanAttribute(html_names::kPlaysinlineAttr, true);
  Video()->SetSrc("http://example.com/foo.mp4");

  LocalFrame::NotifyUserActivation(GetFrame());
  Video()->Play();

  MakeGarbageCollected<WaitForEvent>(Video(), event_type_names::kPlay);
  test::RunPendingTasks();

  EXPECT_FALSE(Video()->IsFullscreen());
}

TEST_F(VideoAutoFullscreen, ExitFullscreenPausesWithoutPlaysInline) {
  Video()->SetSrc("http://example.com/foo.mp4");

  LocalFrame::NotifyUserActivation(GetFrame());
  Video()->Play();

  MakeGarbageCollected<WaitForEvent>(Video(), event_type_names::kPlay);
  test::RunPendingTasks();
  ASSERT_TRUE(Video()->IsFullscreen());

  EXPECT_FALSE(Video()->paused());

  GetWebView()->ExitFullscreen(*GetFrame());
  test::RunPendingTasks();

  EXPECT_TRUE(Video()->paused());
}

TEST_F(VideoAutoFullscreen, ExitFullscreenDoesNotPauseWithPlaysInline) {
  Video()->SetBooleanAttribute(html_names::kPlaysinlineAttr, true);
  Video()->SetSrc("http://example.com/foo.mp4");

  LocalFrame::NotifyUserActivation(GetFrame());
  Video()->Play();

  MakeGarbageCollected<WaitForEvent>(Video(), event_type_names::kPlay);
  Video()->webkitEnterFullscreen();
  test::RunPendingTasks();
  ASSERT_TRUE(Video()->IsFullscreen());

  EXPECT_FALSE(Video()->paused());

  GetWebView()->ExitFullscreen(*GetFrame());
  test::RunPendingTasks();

  EXPECT_FALSE(Video()->paused());
}

TEST_F(VideoAutoFullscreen, OnPlayTriggersFullscreenWithoutGesture) {
  Video()->SetSrc("http://example.com/foo.mp4");

  LocalFrame::NotifyUserActivation(GetFrame());
  Video()->Play();
  MakeGarbageCollected<WaitForEvent>(Video(), event_type_names::kPlay);
  test::RunPendingTasks();

  EXPECT_TRUE(Video()->IsFullscreen());

  GetWebView()->ExitFullscreen(*GetFrame());
  test::RunPendingTasks();

  EXPECT_TRUE(Video()->paused());
  EXPECT_FALSE(Video()->IsFullscreen());

  Video()->Play();
  test::RunPendingTasks();

  EXPECT_FALSE(Video()->paused());
  EXPECT_TRUE(Video()->IsFullscreen());
}

}  // namespace blink
