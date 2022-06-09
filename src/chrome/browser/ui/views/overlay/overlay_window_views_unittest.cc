// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/overlay/close_image_button.h"
#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "chrome/browser/ui/views/overlay/track_image_button.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/layer.h"
#include "ui/display/test/scoped_screen_override.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/test/button_test_api.h"

class TestPictureInPictureWindowController
    : public content::PictureInPictureWindowController {
 public:
  TestPictureInPictureWindowController() = default;

  // PictureInPictureWindowController:
  void Show() override {}
  void FocusInitiator() override {}
  MOCK_METHOD(void, Close, (bool));
  void CloseAndFocusInitiator() override {}
  MOCK_METHOD(void, OnWindowDestroyed, (bool));
  content::OverlayWindow* GetWindowForTesting() override { return nullptr; }
  void UpdateLayerBounds() override {}
  bool IsPlayerActive() override { return false; }
  void set_web_contents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }
  content::WebContents* GetWebContents() override { return web_contents_; }
  bool TogglePlayPause() override { return false; }
  void SkipAd() override {}
  void NextTrack() override {}
  void PreviousTrack() override {}
  void ToggleMicrophone() override {}
  void ToggleCamera() override {}
  void HangUp() override {}

 private:
  raw_ptr<content::WebContents> web_contents_;
};

class OverlayWindowViewsTest : public ChromeViewsTestBase {
 public:
  OverlayWindowViewsTest() = default;
  // ChromeViewsTestBase:
  void SetUp() override {
    // Purposely skip ChromeViewsTestBase::SetUp() as that creates ash::Shell
    // on ChromeOS, which we don't want.
    ViewsTestBase::SetUp();
    // web_contents_ needs to be created after the constructor, so that
    // |feature_list_| can be initialized before other threads check if a
    // feature is enabled.
    web_contents_ = web_contents_factory_.CreateWebContents(&profile_);
    pip_window_controller_.set_web_contents(web_contents_);

#if defined(OS_CHROMEOS)
    test_views_delegate()->set_context(GetContext());
#endif
    test_views_delegate()->set_use_desktop_native_widgets(true);

    // The default work area must be big enough to fit the minimum
    // OverlayWindowViews size.
    SetDisplayWorkArea({0, 0, 1000, 1000});

    overlay_window_ = OverlayWindowViews::Create(&pip_window_controller_);
    overlay_window_->set_minimum_size_for_testing({200, 100});
  }

  void TearDown() override {
    overlay_window_.reset();
    ViewsTestBase::TearDown();
  }

  void SetDisplayWorkArea(const gfx::Rect& work_area) {
    display::Display display = test_screen_.GetPrimaryDisplay();
    display.set_work_area(work_area);
    test_screen_.display_list().UpdateDisplay(display);
  }

  OverlayWindowViews& overlay_window() { return *overlay_window_; }

  content::WebContents* web_contents() { return web_contents_; }

  TestPictureInPictureWindowController& pip_window_controller() {
    return pip_window_controller_;
  }

 private:
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  TestPictureInPictureWindowController pip_window_controller_;

  display::test::TestScreen test_screen_;
  display::test::ScopedScreenOverride scoped_screen_override_{&test_screen_};

  std::unique_ptr<OverlayWindowViews> overlay_window_;
};

TEST_F(OverlayWindowViewsTest, InitialWindowSize_Square) {
  // Fit the window taking 1/5 (both dimensions) of the work area as the
  // starting size, and applying the size and aspect ratio constraints.
  overlay_window().UpdateVideoSize({400, 400});
  EXPECT_EQ(gfx::Size(200, 200), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(200, 200),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(OverlayWindowViewsTest, InitialWindowSize_Horizontal) {
  // Fit the window taking 1/5 (both dimensions) of the work area as the
  // starting size, and applying the size and aspect ratio constraints.
  overlay_window().UpdateVideoSize({400, 200});
  EXPECT_EQ(gfx::Size(400, 200), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(400, 200),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(OverlayWindowViewsTest, InitialWindowSize_Vertical) {
  // Fit the window taking 1/5 (both dimensions) of the work area as the
  // starting size, and applying the size and aspect ratio constraints.
  overlay_window().UpdateVideoSize({400, 500});
  EXPECT_EQ(gfx::Size(200, 250), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(200, 250),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(OverlayWindowViewsTest, Letterboxing) {
  overlay_window().UpdateVideoSize({400, 10});

  // Must fit within the minimum height of 146. But with the aspect ratio of
  // 40:1 the width gets exceedingly big and must be limited to the maximum of
  // 500. Thus, letterboxing is unavoidable.
  EXPECT_EQ(gfx::Size(500, 100), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(500, 13),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(OverlayWindowViewsTest, Pillarboxing) {
  overlay_window().UpdateVideoSize({10, 400});

  // Must fit within the minimum width of 260. But with the aspect ratio of
  // 1:40 the height gets exceedingly big and must be limited to the maximum of
  // 500. Thus, pillarboxing is unavoidable.
  EXPECT_EQ(gfx::Size(200, 500), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(13, 500),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(OverlayWindowViewsTest, Pillarboxing_Square) {
  overlay_window().UpdateVideoSize({100, 100});

  // Pillarboxing also occurs on Linux even with the square aspect ratio,
  // because the user is allowed to size the window to the rectangular minimum
  // size.
  overlay_window().SetSize({200, 100});
  EXPECT_EQ(gfx::Size(100, 100),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(OverlayWindowViewsTest, ApproximateAspectRatio_Horizontal) {
  // "Horizontal" video.
  overlay_window().UpdateVideoSize({320, 240});

  // The user drags the window resizer horizontally and now the integer window
  // dimensions can't reproduce the video aspect ratio exactly. The video
  // should still fill the entire window area.
  overlay_window().SetSize({320, 240});
  EXPECT_EQ(gfx::Size(320, 240),
            overlay_window().video_layer_for_testing()->size());

  overlay_window().SetSize({321, 241});
  EXPECT_EQ(gfx::Size(321, 241),
            overlay_window().video_layer_for_testing()->size());

  // Wide video.
  overlay_window().UpdateVideoSize({1600, 900});

  overlay_window().SetSize({444, 250});
  EXPECT_EQ(gfx::Size(444, 250),
            overlay_window().video_layer_for_testing()->size());

  overlay_window().SetSize({445, 250});
  EXPECT_EQ(gfx::Size(445, 250),
            overlay_window().video_layer_for_testing()->size());

  // Very wide video.
  overlay_window().UpdateVideoSize({400, 100});

  overlay_window().SetSize({478, 120});
  EXPECT_EQ(gfx::Size(478, 120),
            overlay_window().video_layer_for_testing()->size());

  overlay_window().SetSize({481, 120});
  EXPECT_EQ(gfx::Size(481, 120),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(OverlayWindowViewsTest, ApproximateAspectRatio_Vertical) {
  // "Vertical" video.
  overlay_window().UpdateVideoSize({240, 320});

  // The user dragged the window resizer vertically and now the integer window
  // dimensions can't reproduce the video aspect ratio exactly. The video
  // should still fill the entire window area.
  overlay_window().SetSize({240, 320});
  EXPECT_EQ(gfx::Size(240, 320),
            overlay_window().video_layer_for_testing()->size());

  overlay_window().SetSize({239, 319});
  EXPECT_EQ(gfx::Size(239, 319),
            overlay_window().video_layer_for_testing()->size());

  // Narrow video.
  overlay_window().UpdateVideoSize({900, 1600});

  overlay_window().SetSize({250, 444});
  EXPECT_EQ(gfx::Size(250, 444),
            overlay_window().video_layer_for_testing()->size());

  overlay_window().SetSize({250, 445});
  EXPECT_EQ(gfx::Size(250, 445),
            overlay_window().video_layer_for_testing()->size());

  // Very narrow video.
  // NOTE: Window width is bounded by the minimum size.
  overlay_window().UpdateVideoSize({100, 400});

  overlay_window().SetSize({200, 478});
  EXPECT_EQ(gfx::Size(120, 478),
            overlay_window().video_layer_for_testing()->size());

  overlay_window().SetSize({200, 481});
  EXPECT_EQ(gfx::Size(120, 481),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(OverlayWindowViewsTest, UpdateMaximumSize) {
  SetDisplayWorkArea({0, 0, 4000, 4000});

  overlay_window().UpdateVideoSize({480, 320});

  // The initial size is determined by the work area and the video natural size
  // (aspect ratio).
  EXPECT_EQ(gfx::Size(1200, 800), overlay_window().GetBounds().size());
  // The initial maximum size is a quarter of the work area.
  EXPECT_EQ(gfx::Size(2000, 2000), overlay_window().GetMaximumSize());

  // If the maximum size increases then we should keep the existing window size.
  SetDisplayWorkArea({0, 0, 8000, 8000});
  overlay_window().OnNativeWidgetMove();
  EXPECT_EQ(gfx::Size(1200, 800), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(4000, 4000), overlay_window().GetMaximumSize());

  // If the maximum size decreases then we should shrink to fit.
  SetDisplayWorkArea({0, 0, 1000, 2000});
  overlay_window().OnNativeWidgetMove();
  EXPECT_EQ(gfx::Size(500, 800), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(500, 1000), overlay_window().GetMaximumSize());
}

TEST_F(OverlayWindowViewsTest, IgnoreInvalidMaximumSize) {
  ASSERT_EQ(gfx::Size(500, 500), overlay_window().GetMaximumSize());

  SetDisplayWorkArea({0, 0, 0, 0});
  overlay_window().OnNativeWidgetMove();
  EXPECT_EQ(gfx::Size(500, 500), overlay_window().GetMaximumSize());
}

// Tests that Next Track button bounds are updated right away when window
// controls are hidden.
TEST_F(OverlayWindowViewsTest, NextTrackButtonAddedWhenControlsHidden) {
  ASSERT_FALSE(overlay_window().AreControlsVisible());
  ASSERT_TRUE(overlay_window()
                  .next_track_controls_view_for_testing()
                  ->size()
                  .IsEmpty());

  const auto origin_before_layout =
      overlay_window().next_track_controls_view_for_testing()->origin();

  overlay_window().SetNextTrackButtonVisibility(true);
  EXPECT_NE(overlay_window().next_track_controls_view_for_testing()->origin(),
            origin_before_layout);
  EXPECT_FALSE(overlay_window().IsLayoutPendingForTesting());
}

// Tests that Previous Track button bounds are updated right away when window
// controls are hidden.
TEST_F(OverlayWindowViewsTest, PreviousTrackButtonAddedWhenControlsHidden) {
  ASSERT_FALSE(overlay_window().AreControlsVisible());
  ASSERT_TRUE(overlay_window()
                  .previous_track_controls_view_for_testing()
                  ->size()
                  .IsEmpty());

  const auto origin_before_layout =
      overlay_window().previous_track_controls_view_for_testing()->origin();

  overlay_window().SetPreviousTrackButtonVisibility(true);
  EXPECT_NE(
      overlay_window().previous_track_controls_view_for_testing()->origin(),
      origin_before_layout);
  EXPECT_FALSE(overlay_window().IsLayoutPendingForTesting());
}

TEST_F(OverlayWindowViewsTest, UpdateVideoSizeDoesNotMoveWindow) {
  // Enter PiP.
  overlay_window().UpdateVideoSize({300, 200});
  overlay_window().ShowInactive();

  // Resize the window and move it toward the top-left corner of the work area.
  // In production, resizing preserves the aspect ratio if possible, so we
  // preserve it here too.
  overlay_window().SetBounds({100, 100, 450, 300});

  // Simulate a new surface layer and a change in the aspect ratio.
  overlay_window().UpdateVideoSize({400, 200});

  // The window should not move.
  // The window size will be adjusted according to the new aspect ratio, and
  // clamped to 500x250 to fit within the maximum size for the work area of
  // 1000x1000.
  EXPECT_EQ(gfx::Rect(100, 100, 500, 250), overlay_window().GetBounds());
}

// Tests that the OverlayWindowFrameView does not accept events so they can
// propagate to the overlay.
TEST_F(OverlayWindowViewsTest, HitTestFrameView) {
  // Since the NonClientFrameView is the only non-custom direct descendent of
  // the NonClientView, we can assume that if the frame does not accept the
  // point but the NonClientView does, then it will be handled by one of the
  // custom overlay views.
  auto point = gfx::Point(50, 50);
  views::NonClientView* non_client_view = overlay_window().non_client_view();
  EXPECT_EQ(non_client_view->frame_view()->HitTestPoint(point), false);
  EXPECT_EQ(non_client_view->HitTestPoint(point), true);
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// With pillarboxing, the close button doesn't cover the video area. Make sure
// hovering the button doesn't get handled like normal mouse exit events
// causing the controls to hide.
TEST_F(OverlayWindowViewsTest, NoMouseExitWithinWindowBounds) {
  overlay_window().UpdateVideoSize({10, 400});

  const auto close_button_bounds = overlay_window().GetCloseControlsBounds();
  const auto video_bounds =
      overlay_window().video_layer_for_testing()->bounds();
  ASSERT_FALSE(video_bounds.Contains(close_button_bounds));

  const gfx::Point moved_location(video_bounds.origin() + gfx::Vector2d(5, 5));
  ui::MouseEvent moved_event(ui::ET_MOUSE_MOVED, moved_location, moved_location,
                             ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  overlay_window().OnMouseEvent(&moved_event);
  ASSERT_TRUE(overlay_window().AreControlsVisible());

  const gfx::Point exited_location(close_button_bounds.CenterPoint());
  ui::MouseEvent exited_event(ui::ET_MOUSE_EXITED, exited_location,
                              exited_location, ui::EventTimeForNow(),
                              ui::EF_NONE, ui::EF_NONE);
  overlay_window().OnMouseEvent(&exited_event);
  EXPECT_TRUE(overlay_window().AreControlsVisible());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_F(OverlayWindowViewsTest, ShowControlsOnFocus) {
  EXPECT_FALSE(overlay_window().AreControlsVisible());
  overlay_window().OnNativeFocus();
  EXPECT_TRUE(overlay_window().AreControlsVisible());
}

TEST_F(OverlayWindowViewsTest, OnlyPauseOnCloseWhenPauseIsAvailable) {
  views::test::ButtonTestApi close_button_clicker(
      overlay_window().close_button_for_testing());
  ui::MouseEvent dummy_event(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0),
                             gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);

  // When the play/pause controls are visible, closing via the close button
  // should pause the video.
  overlay_window().SetPlayPauseButtonVisibility(true);
  EXPECT_CALL(pip_window_controller(), Close(true));
  close_button_clicker.NotifyClick(dummy_event);
  testing::Mock::VerifyAndClearExpectations(&pip_window_controller());

  // When the play/pause controls are not visible, closing via the close button
  // should not pause the video.
  overlay_window().SetPlayPauseButtonVisibility(false);
  EXPECT_CALL(pip_window_controller(), Close(false));
  close_button_clicker.NotifyClick(dummy_event);
  testing::Mock::VerifyAndClearExpectations(&pip_window_controller());
}

TEST_F(OverlayWindowViewsTest, PauseOnWidgetCloseWhenPauseAvailable) {
  // When the play/pause controls are visible, when the native widget is
  // destroyed we should pause the underlying video.
  overlay_window().SetPlayPauseButtonVisibility(true);
  EXPECT_CALL(pip_window_controller(), OnWindowDestroyed(true));
  overlay_window().CloseNow();
  testing::Mock::VerifyAndClearExpectations(&pip_window_controller());
}

TEST_F(OverlayWindowViewsTest, DontPauseOnWidgetCloseWhenPauseNotAvailable) {
  // When the play/pause controls are not visible, when the native widget is
  // destroyed we should not pause the underlying video.
  overlay_window().SetPlayPauseButtonVisibility(false);
  EXPECT_CALL(pip_window_controller(), OnWindowDestroyed(false));
  overlay_window().CloseNow();
  testing::Mock::VerifyAndClearExpectations(&pip_window_controller());
}
