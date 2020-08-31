// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_display_cutout_fullscreen_button_element.h"

#include "third_party/blink/public/mojom/page/display_cutout.mojom-blink.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_touch_event_init.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

namespace {

class MockDisplayCutoutChromeClient : public EmptyChromeClient {
 public:
  // ChromeClient overrides:
  void EnterFullscreen(LocalFrame& frame,
                       const FullscreenOptions*,
                       bool for_cross_process_descendant) override {
    Fullscreen::DidResolveEnterFullscreenRequest(*frame.GetDocument(),
                                                 true /* granted */);
  }
  void ExitFullscreen(LocalFrame& frame) override {
    Fullscreen::DidExitFullscreen(*frame.GetDocument());
  }
};

}  // namespace

class MediaControlDisplayCutoutFullscreenButtonElementTest
    : public PageTestBase,
      private ScopedDisplayCutoutAPIForTest {
 public:
  static TouchEventInit* GetValidTouchEventInit() {
    return TouchEventInit::Create();
  }

  MediaControlDisplayCutoutFullscreenButtonElementTest()
      : ScopedDisplayCutoutAPIForTest(true) {}
  void SetUp() override {
    chrome_client_ = MakeGarbageCollected<MockDisplayCutoutChromeClient>();

    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();
    SetupPageWithClients(&clients,
                         MakeGarbageCollected<EmptyLocalFrameClient>());
    video_ = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
    GetDocument().body()->AppendChild(video_);
    controls_ = MakeGarbageCollected<MediaControlsImpl>(*video_);
    controls_->InitializeControls();
    display_cutout_fullscreen_button_ =
        controls_->display_cutout_fullscreen_button_;
  }

  mojom::ViewportFit CurrentViewportFit() const {
    return GetDocument().GetViewportData().GetCurrentViewportFitForTests();
  }

  void SimulateEnterFullscreen() {
    {
      LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
      Fullscreen::RequestFullscreen(*video_);
    }

    test::RunPendingTasks();
    GetDocument().ServiceScriptedAnimations(base::TimeTicks());

    EXPECT_TRUE(video_->IsFullscreen());
  }

  void SimulateExitFullscreen() {
    Fullscreen::FullyExitFullscreen(GetDocument());

    GetDocument().ServiceScriptedAnimations(base::TimeTicks());

    EXPECT_FALSE(video_->IsFullscreen());
  }

 protected:
  Persistent<MockDisplayCutoutChromeClient> chrome_client_;
  Persistent<HTMLVideoElement> video_;
  Persistent<MediaControlDisplayCutoutFullscreenButtonElement>
      display_cutout_fullscreen_button_;
  Persistent<MediaControlsImpl> controls_;
};

TEST_F(MediaControlDisplayCutoutFullscreenButtonElementTest,
       Fullscreen_ButtonAccessibility) {
  EXPECT_EQ(display_cutout_fullscreen_button_->GetLocale().QueryString(
                IDS_AX_MEDIA_DISPLAY_CUT_OUT_FULL_SCREEN_BUTTON),
            display_cutout_fullscreen_button_->getAttribute(
                html_names::kAriaLabelAttr));
}

TEST_F(MediaControlDisplayCutoutFullscreenButtonElementTest,
       Fullscreen_ButtonVisiblilty) {
  EXPECT_FALSE(display_cutout_fullscreen_button_->IsWanted());

  SimulateEnterFullscreen();

  EXPECT_TRUE(display_cutout_fullscreen_button_->IsWanted());

  SimulateExitFullscreen();

  EXPECT_FALSE(display_cutout_fullscreen_button_->IsWanted());
}

TEST_F(MediaControlDisplayCutoutFullscreenButtonElementTest,
       Fullscreen_ButtonTogglesDisplayCutoutFullscreen) {
  SimulateEnterFullscreen();

  EXPECT_EQ(mojom::ViewportFit::kAuto, CurrentViewportFit());

  display_cutout_fullscreen_button_->DispatchSimulatedClick(
      nullptr, kSendNoEvents, SimulatedClickCreationScope::kFromUserAgent);
  EXPECT_EQ(mojom::ViewportFit::kCoverForcedByUserAgent, CurrentViewportFit());

  display_cutout_fullscreen_button_->DispatchSimulatedClick(
      nullptr, kSendNoEvents, SimulatedClickCreationScope::kFromUserAgent);
  EXPECT_EQ(mojom::ViewportFit::kAuto, CurrentViewportFit());
}

}  // namespace blink
