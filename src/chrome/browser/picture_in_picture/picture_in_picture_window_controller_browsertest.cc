// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "content/public/browser/picture_in_picture_window_controller.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/overlay/hang_up_button.h"
#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "chrome/browser/ui/views/overlay/playback_image_button.h"
#include "chrome/browser/ui/views/overlay/skip_ad_label_button.h"
#include "chrome/browser/ui/views/overlay/toggle_camera_button.h"
#include "chrome/browser/ui/views/overlay/toggle_microphone_button.h"
#include "chrome/browser/ui/views/overlay/track_image_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/media_start_stop_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/media_session/public/cpp/features.h"
#include "skia/ext/image_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/display/display_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/base/hit_test.h"
#endif

using content::EvalJs;
using content::ExecJs;
using ::testing::_;

namespace {

class MockPictureInPictureWindowController
    : public content::PictureInPictureWindowController {
 public:
  MockPictureInPictureWindowController() = default;

  MockPictureInPictureWindowController(
      const MockPictureInPictureWindowController&) = delete;
  MockPictureInPictureWindowController& operator=(
      const MockPictureInPictureWindowController&) = delete;

  // PictureInPictureWindowController:
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(FocusInitiator, void());
  MOCK_METHOD1(Close, void(bool));
  MOCK_METHOD0(CloseAndFocusInitiator, void());
  MOCK_METHOD1(OnWindowDestroyed, void(bool));
  MOCK_METHOD0(GetWindowForTesting, content::OverlayWindow*());
  MOCK_METHOD0(UpdateLayerBounds, void());
  MOCK_METHOD0(IsPlayerActive, bool());
  MOCK_METHOD0(GetWebContents, content::WebContents*());
  MOCK_METHOD0(TogglePlayPause, bool());
  MOCK_METHOD0(SkipAd, void());
  MOCK_METHOD0(NextTrack, void());
  MOCK_METHOD0(PreviousTrack, void());
  MOCK_METHOD0(ToggleMicrophone, void());
  MOCK_METHOD0(ToggleCamera, void());
  MOCK_METHOD0(HangUp, void());
};

const base::FilePath::CharType kPictureInPictureWindowSizePage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/window-size.html");

const base::FilePath::CharType kPictureInPictureVideoConferencingPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/video-conferencing.html");

// Determines whether |control| is visible taking into account OverlayWindow's
// custom control hiding that includes setting the size to 0x0.
bool IsOverlayWindowControlVisible(views::View* control) {
  return control->GetVisible() && !control->size().IsEmpty();
}

// An observer used to notify about control visibility changes.
class ControlVisibilityObserver : views::ViewObserver {
 public:
  explicit ControlVisibilityObserver(views::View* observed_view,
                                     bool expected_visible,
                                     base::OnceClosure callback)
      : expected_visible_(expected_visible),
        visibility_change_callback_(std::move(callback)) {
    observation_.Observe(observed_view);

    MaybeNotifyOfVisibilityChange(observed_view);
  }

  // views::ViewObserver overrides.
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override {
    MaybeNotifyOfVisibilityChange(observed_view);
  }
  void OnViewBoundsChanged(views::View* observed_view) override {
    MaybeNotifyOfVisibilityChange(observed_view);
  }

 private:
  void MaybeNotifyOfVisibilityChange(views::View* observed_view) {
    if (visibility_change_callback_ &&
        IsOverlayWindowControlVisible(observed_view) == expected_visible_) {
      std::move(visibility_change_callback_).Run();
    }
  }

  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
  bool expected_visible_;
  base::OnceClosure visibility_change_callback_;
};

// A helper class to wait for widget size to change to the desired value.
class WidgetSizeChangeWaiter final : public views::WidgetObserver {
 public:
  WidgetSizeChangeWaiter(views::Widget* widget, const gfx::Size& expected_size)
      : widget_(widget), expected_size_(expected_size) {
    widget_->AddObserver(this);
  }
  ~WidgetSizeChangeWaiter() override { widget_->RemoveObserver(this); }

  WidgetSizeChangeWaiter(const WidgetSizeChangeWaiter&) = delete;
  WidgetSizeChangeWaiter& operator=(const WidgetSizeChangeWaiter&) = delete;

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    bounds_change_count_++;
    if (new_bounds.size() == expected_size_)
      run_loop_.Quit();
  }

  // Wait for changes to occur, or return immediately if they already have.
  void WaitForSize() {
    if (widget_->GetWindowBoundsInScreen().size() != expected_size_)
      run_loop_.Run();
  }

  int bounds_change_count() const { return bounds_change_count_; }

 private:
  const raw_ptr<views::Widget> widget_;
  const gfx::Size expected_size_;
  int bounds_change_count_ = 0;
  base::RunLoop run_loop_;
};

// Waits until the given WebContents has the expected title.
void WaitForTitle(content::WebContents* web_contents,
                  const std::u16string& expected_title) {
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents, expected_title).WaitAndGetTitle());
}

}  // namespace

class PictureInPictureWindowControllerBrowserTest
    : public InProcessBrowserTest {
 public:
  PictureInPictureWindowControllerBrowserTest() = default;

  PictureInPictureWindowControllerBrowserTest(
      const PictureInPictureWindowControllerBrowserTest&) = delete;
  PictureInPictureWindowControllerBrowserTest& operator=(
      const PictureInPictureWindowControllerBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpWindowController(content::WebContents* web_contents) {
    pip_window_controller_ =
        content::PictureInPictureWindowController::GetOrCreateForWebContents(
            web_contents);
  }

  content::PictureInPictureWindowController* window_controller() {
    return pip_window_controller_;
  }

  MockPictureInPictureWindowController& mock_controller() {
    return mock_controller_;
  }

  OverlayWindowViews* GetOverlayWindow() {
    return static_cast<OverlayWindowViews*>(
        window_controller()->GetWindowForTesting());
  }

  void LoadTabAndEnterPictureInPicture(Browser* browser,
                                       const base::FilePath& file_path) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory), file_path);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));

    content::WebContents* active_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_NE(nullptr, active_web_contents);

    SetUpWindowController(active_web_contents);

    ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
  }

  // The WebContents that is passed to this method must have a
  // "isInPictureInPicture()" method returning a boolean and when the video
  // leaves Picture-in-Picture, it must change the WebContents title to
  // "leavepictureinpicture".
  void ExpectLeavePictureInPicture(content::WebContents* web_contents) {
    // 'leavepictureinpicture' is the title of the tab when the event is
    // received.
    WaitForTitle(web_contents, u"leavepictureinpicture");
    EXPECT_EQ(false, EvalJs(web_contents, "isInPictureInPicture();"));
  }

  void WaitForPlaybackState(content::WebContents* web_contents,
                            OverlayWindowViews::PlaybackState playback_state) {
    // Make sure to wait if not yet in the |playback_state| state.
    if (GetOverlayWindow()->playback_state_for_testing() != playback_state) {
      content::MediaStartStopObserver observer(
          web_contents,
          playback_state == OverlayWindowViews::PlaybackState::kPlaying
              ? content::MediaStartStopObserver::Type::kStart
              : content::MediaStartStopObserver::Type::kStop);
      observer.Wait();
    }
  }

  // Makes sure all |controls| have the expected visibility state, waiting if
  // necessary.
  void AssertControlsVisible(std::vector<views::View*> controls,
                             bool expected_visible) {
    base::RunLoop run_loop;
    const auto barrier =
        base::BarrierClosure(controls.size(), run_loop.QuitClosure());
    std::vector<std::unique_ptr<ControlVisibilityObserver>> observers;
    for (views::View* control : controls) {
      observers.push_back(std::make_unique<ControlVisibilityObserver>(
          control, expected_visible, barrier));
    }
    run_loop.Run();

    for (views::View* control : controls)
      ASSERT_EQ(IsOverlayWindowControlVisible(control), expected_visible);
  }

  void MoveMouseOverOverlayWindow() {
    auto* const window = GetOverlayWindow();
    const gfx::Point p(window->GetBounds().origin());
    ui::MouseEvent moved_over(ui::ET_MOUSE_MOVED, p, p, ui::EventTimeForNow(),
                              ui::EF_NONE, ui::EF_NONE);
    window->OnMouseEvent(&moved_over);
  }

  void ClickButton(views::Button* button) {
    const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(button).NotifyClick(event);
  }

 private:
  raw_ptr<content::PictureInPictureWindowController> pip_window_controller_ =
      nullptr;
  MockPictureInPictureWindowController mock_controller_;
};

// Checks the creation of the window controller, as well as basic window
// creation, visibility and activation.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CreationAndVisibilityAndActivation) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller() != nullptr);

  ASSERT_TRUE(window_controller()->GetWindowForTesting() != nullptr);
  EXPECT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  gfx::NativeWindow native_window = GetOverlayWindow()->GetNativeWindow();
  EXPECT_FALSE(platform_util::IsWindowActive(native_window));
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       ControlsVisibility) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  EXPECT_FALSE(GetOverlayWindow()->AreControlsVisible());
  MoveMouseOverOverlayWindow();
  EXPECT_TRUE(GetOverlayWindow()->AreControlsVisible());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
class PictureInPicturePixelComparisonBrowserTest
    : public PictureInPictureWindowControllerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PictureInPictureWindowControllerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kDisableGpu);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1");

    // TODO(https://crbug.com/1201311): Refactor these pixel tests to use Skia
    // Gold and turn this feature back on.
    feature_list_.InitAndDisableFeature(media::kMediaSessionWebRTC);
  }

  base::FilePath GetFilePath(base::FilePath::StringPieceType relative_path) {
    base::FilePath base_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &base_dir));
    // The path relative to <chromium src> for pixel test data.
    const base::FilePath::StringPieceType kTestDataPath =
        FILE_PATH_LITERAL("chrome/test/data/media/picture-in-picture/");
    base::FilePath full_path =
        base_dir.Append(kTestDataPath).Append(relative_path);
    return full_path;
  }

  void ReadbackResult(base::RepeatingClosure quit_run_loop,
                      std::unique_ptr<viz::CopyOutputResult> result) {
    ASSERT_FALSE(result->IsEmpty());
    EXPECT_EQ(viz::CopyOutputResult::Format::RGBA, result->format());
    EXPECT_EQ(viz::CopyOutputResult::Destination::kSystemMemory,
              result->destination());
    auto scoped_sk_bitmap = result->ScopedAccessSkBitmap();
    result_bitmap_ =
        std::make_unique<SkBitmap>(scoped_sk_bitmap.GetOutScopedBitmap());
    EXPECT_TRUE(result_bitmap_->readyToDraw());
    quit_run_loop.Run();
  }

  bool ReadImageFile(const base::FilePath& file_path, SkBitmap* read_image) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string png_string;
    base::ReadFileToString(file_path, &png_string);
    return gfx::PNGCodec::Decode(
        reinterpret_cast<const unsigned char*>(png_string.data()),
        png_string.length(), read_image);
  }

  void TakeOverlayWindowScreenshot(const gfx::Size& window_size,
                                   bool controls_visible) {
    for (int i = 0; i < 2; ++i) {
      WidgetSizeChangeWaiter bounds_change_waiter(GetOverlayWindow(),
                                                  window_size);
      // Also move to the center to avoid spurious moves later on, which happen
      // on some platforms when the window is enlarged beyond the screen bounds.
      GetOverlayWindow()->CenterWindow(window_size);
      bounds_change_waiter.WaitForSize();
      const auto initial_count = bounds_change_waiter.bounds_change_count();

      // Make sure native widget events won't unexpectedly hide or show the
      // controls.
      GetOverlayWindow()->ForceControlsVisibleForTesting(controls_visible);

      ui::Layer* const layer = GetOverlayWindow()->GetRootView()->layer();
      layer->CompleteAllAnimations();
      layer->GetCompositor()->ScheduleFullRedraw();
      ui::DrawWaiterForTest::WaitForCompositingEnded(layer->GetCompositor());

      base::RunLoop run_loop;
      std::unique_ptr<viz::CopyOutputRequest> request =
          std::make_unique<viz::CopyOutputRequest>(
              viz::CopyOutputRequest::ResultFormat::RGBA,
              viz::CopyOutputRequest::ResultDestination::kSystemMemory,
              base::BindOnce(
                  &PictureInPicturePixelComparisonBrowserTest::ReadbackResult,
                  base::Unretained(this), run_loop.QuitClosure()));
      layer->RequestCopyOfOutput(std::move(request));
      run_loop.Run();

      if (bounds_change_waiter.bounds_change_count() == initial_count)
        break;

      // We get here on Linux/Wayland (maybe elsewhere too?) sometimes. The
      // native widget goes back to the previous bounds for an instant and then
      // reverts.
      LOG(INFO) << "The native widget bounds have changed while taking the "
                   "screenshot, retrying";
    }
  }

  bool CompareImages(const SkBitmap& actual_bmp, const SkBitmap& expected_bmp) {
    // Allowable error and thresholds because of small color shift by
    // video to image conversion and GPU issues.
    const int kAllowableError = 3;
    // Number of pixels with an error
    int error_pixels_count = 0;
    gfx::Rect error_bounding_rect;

    for (int x = 0; x < actual_bmp.width(); ++x) {
      for (int y = 0; y < actual_bmp.height(); ++y) {
        bool pixel_matches = false;
        if (x < expected_bmp.width() && y < expected_bmp.height()) {
          SkColor actual_color = actual_bmp.getColor(x, y);
          SkColor expected_color = expected_bmp.getColor(x, y);
          if ((fabs(SkColorGetR(actual_color) - SkColorGetR(expected_color)) <=
               kAllowableError) &&
              (fabs(SkColorGetG(actual_color) - SkColorGetG(expected_color)) <=
               kAllowableError) &&
              (fabs(SkColorGetB(actual_color) - SkColorGetB(expected_color))) <=
                  kAllowableError) {
            pixel_matches = true;
          }
        }
        if (!pixel_matches) {
          ++error_pixels_count;
          error_bounding_rect.Union(gfx::Rect(x, y, 1, 1));
        }
      }
    }
    if (error_pixels_count != 0) {
      LOG(ERROR) << "Number of pixel with an error: " << error_pixels_count;
      LOG(ERROR) << "Error Bounding Box : " << error_bounding_rect.ToString();
      return false;
    }
    return true;
  }

  SkBitmap& GetResultBitmap() { return *result_bitmap_; }

 private:
  std::unique_ptr<SkBitmap> result_bitmap_;

  base::test::ScopedFeatureList feature_list_;
};

// Plays a video in PiP. Grabs a screenshot of Picture-in-Picture window and
// verifies it's as expected.
IN_PROC_BROWSER_TEST_F(PictureInPicturePixelComparisonBrowserTest, VideoPlay) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(FILE_PATH_LITERAL(
                     "media/picture-in-picture/pixel_test.html")));
  ASSERT_TRUE(GetOverlayWindow()->IsVisible());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_EQ(true, EvalJs(active_web_contents, "play();"));

  TakeOverlayWindowScreenshot({402, 268}, /*controls_visible=*/false);

  SkBitmap expected_image;
  base::FilePath expected_image_path =
      GetFilePath(FILE_PATH_LITERAL("pixel_expected_video_play.png"));
  ASSERT_TRUE(ReadImageFile(expected_image_path, &expected_image));
  EXPECT_TRUE(CompareImages(GetResultBitmap(), expected_image));
}

// Plays a video in PiP. Trigger the play and pause control in PiP by using a
// mouse move. Capture the images and verify they are expected.
IN_PROC_BROWSER_TEST_F(PictureInPicturePixelComparisonBrowserTest,
                       PlayAndPauseControls) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(FILE_PATH_LITERAL(
                     "media/picture-in-picture/pixel_test.html")));
  ASSERT_TRUE(GetOverlayWindow()->IsVisible());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_EQ(true, EvalJs(active_web_contents, "play();"));

  WaitForPlaybackState(active_web_contents,
                       OverlayWindowViews::PlaybackState::kPlaying);

  constexpr gfx::Size kSize = {402, 268};
  TakeOverlayWindowScreenshot(kSize, /*controls_visible=*/true);

  base::FilePath expected_pause_image_path =
      GetFilePath(FILE_PATH_LITERAL("pixel_expected_pause_control.png"));
  base::FilePath expected_play_image_path =
      GetFilePath(FILE_PATH_LITERAL("pixel_expected_play_control.png"));
  // If the test image is cropped, usually off by 1 pixel, use another image.
  if (GetResultBitmap().width() < kSize.width() ||
      GetResultBitmap().height() < kSize.height()) {
    LOG(INFO) << "Actual image is cropped and backup images are used. "
              << "Test image dimension: "
              << "(" << GetResultBitmap().width() << "x"
              << GetResultBitmap().height() << "). "
              << "Expected image dimension: "
              << "(" << kSize.ToString() << ")";
    expected_pause_image_path =
        GetFilePath(FILE_PATH_LITERAL("pixel_expected_pause_control_crop.png"));
    expected_play_image_path =
        GetFilePath(FILE_PATH_LITERAL("pixel_expected_play_control_crop.png"));
  }

  SkBitmap expected_image;
  ASSERT_TRUE(ReadImageFile(expected_pause_image_path, &expected_image));
  EXPECT_TRUE(CompareImages(GetResultBitmap(), expected_image));

  ASSERT_TRUE(ExecJs(active_web_contents, "video.pause();"));
  WaitForPlaybackState(active_web_contents,
                       OverlayWindowViews::PlaybackState::kPaused);
  TakeOverlayWindowScreenshot(kSize, /*controls_visible=*/true);
  ASSERT_TRUE(ReadImageFile(expected_play_image_path, &expected_image));
  EXPECT_TRUE(CompareImages(GetResultBitmap(), expected_image));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that when an active WebContents accurately tracks whether a video
// is in Picture-in-Picture.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       TabIconUpdated) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  // First test there is no video playing in Picture-in-Picture.
  EXPECT_FALSE(active_web_contents->HasPictureInPictureVideo());

  // Start playing video in Picture-in-Picture and retest with the
  // opposite assertion.
  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));

  EXPECT_TRUE(active_web_contents->HasPictureInPictureVideo());

  // Stop video being played Picture-in-Picture and check if that's tracked.
  window_controller()->Close(true /* should_pause_video */);
  EXPECT_FALSE(active_web_contents->HasPictureInPictureVideo());

  // Reload page should not crash.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));
}

// Tests that when creating a Picture-in-Picture window a size is sent to the
// caller and if the window is resized, the caller is also notified.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       ResizeEventFired) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  ASSERT_NE(GetOverlayWindow(), nullptr);
  ASSERT_FALSE(GetOverlayWindow()->IsVisible());

  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));

  GetOverlayWindow()->SetSize(gfx::Size(400, 400));

  WaitForTitle(active_web_contents, u"resized");
}

// Tests that when closing a Picture-in-Picture window, the video element is
// reflected as no longer in Picture-in-Picture.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseWindowWhilePlaying) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  ASSERT_EQ(true, EvalJs(active_web_contents, "ensureVideoIsPlaying();"));
  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
  EXPECT_EQ(true, EvalJs(active_web_contents, "isInPictureInPicture();"));

  window_controller()->Close(/*should_pause_video=*/true);

  WaitForTitle(active_web_contents, u"leavepictureinpicture");
  EXPECT_EQ(true, EvalJs(active_web_contents, "isPaused();"));
}

// Ditto, when the video isn't playing.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseWindowWithoutPlaying) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
  EXPECT_EQ(true, EvalJs(active_web_contents, "isInPictureInPicture();"));

  ASSERT_TRUE(window_controller());
  window_controller()->Close(/*should_pause_video=*/true);

  WaitForTitle(active_web_contents, u"leavepictureinpicture");
}

// Tests that when closing a Picture-in-Picture window, the video element
// no longer in Picture-in-Picture can't enter Picture-in-Picture right away.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseWindowCantEnterPictureInPictureAgain) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));

  ASSERT_TRUE(ExecJs(active_web_contents,
                     "tryToEnterPictureInPictureAfterLeaving();",
                     content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_EQ(true, EvalJs(active_web_contents, "isInPictureInPicture();",
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  ASSERT_TRUE(window_controller());
  window_controller()->Close(/*should_pause_video=*/true);

  WaitForTitle(active_web_contents,
               u"failed to enter Picture-in-Picture after leaving");
}

// Tests that when closing a Picture-in-Picture window from the Web API, the
// video element is not paused.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseWindowFromWebAPIWhilePlaying) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  ASSERT_EQ(true, EvalJs(active_web_contents, "ensureVideoIsPlaying();"));
  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
  ASSERT_TRUE(ExecJs(active_web_contents, "exitPictureInPicture();"));

  WaitForTitle(active_web_contents, u"leavepictureinpicture");
  EXPECT_EQ(false, EvalJs(active_web_contents, "isPaused();"));
}

// Tests that when starting a new Picture-in-Picture session from the same
// video, the video stays in Picture-in-Picture mode.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       RequestPictureInPictureTwiceFromSameVideo) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  ASSERT_EQ(true, EvalJs(active_web_contents, "ensureVideoIsPlaying();"));
  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
  EXPECT_EQ(true, EvalJs(active_web_contents, "isInPictureInPicture();"));

  ASSERT_TRUE(ExecJs(active_web_contents, "exitPictureInPicture();"));

  WaitForTitle(active_web_contents, u"leavepictureinpicture");

  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
  EXPECT_EQ(true, EvalJs(active_web_contents, "isInPictureInPicture();"));
  EXPECT_EQ(false, EvalJs(active_web_contents, "isPaused();"));
}

// Tests that when starting a new Picture-in-Picture session from the same tab,
// the previous video is no longer in Picture-in-Picture mode.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       OpenSecondPictureInPictureStopsFirst) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  ASSERT_EQ(true, EvalJs(active_web_contents, "ensureVideoIsPlaying();"));
  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
  EXPECT_EQ(true, EvalJs(active_web_contents, "isInPictureInPicture();"));

  ASSERT_TRUE(ExecJs(active_web_contents, "secondPictureInPicture();"));

  ExpectLeavePictureInPicture(active_web_contents);

  EXPECT_EQ(false, EvalJs(active_web_contents, "isPaused();"));

  EXPECT_TRUE(GetOverlayWindow()->IsVisible());

  EXPECT_EQ(GetOverlayWindow()->playback_state_for_testing(),
            OverlayWindowViews::PlaybackState::kPaused);
}

// Tests that resetting video src when video is in Picture-in-Picture session
// keep Picture-in-Picture window opened.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       ResetVideoSrcKeepsPictureInPictureWindowOpened) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  EXPECT_TRUE(GetOverlayWindow()->video_layer_for_testing()->visible());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ExecJs(active_web_contents, "video.src = null;"));

  EXPECT_EQ(true, EvalJs(active_web_contents, "isInPictureInPicture();"));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_TRUE(GetOverlayWindow()->video_layer_for_testing()->visible());
}

// Tests that updating video src when video is in Picture-in-Picture session
// keep Picture-in-Picture window opened.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       UpdateVideoSrcKeepsPictureInPictureWindowOpened) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  EXPECT_TRUE(GetOverlayWindow()->video_layer_for_testing()->visible());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(true, EvalJs(active_web_contents, "changeVideoSrc();"));
  EXPECT_EQ(true, EvalJs(active_web_contents, "isInPictureInPicture();"));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_TRUE(GetOverlayWindow()->video_layer_for_testing()->visible());
}

// Tests that changing video src to media stream when video is in
// Picture-in-Picture session keep Picture-in-Picture window opened.
IN_PROC_BROWSER_TEST_F(
    PictureInPictureWindowControllerBrowserTest,
    ChangeVideoSrcToMediaStreamKeepsPictureInPictureWindowOpened) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  EXPECT_TRUE(GetOverlayWindow()->video_layer_for_testing()->visible());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(true,
            EvalJs(active_web_contents, "changeVideoSrcToMediaStream();"));

  EXPECT_EQ(true, EvalJs(active_web_contents, "isInPictureInPicture();"));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_TRUE(GetOverlayWindow()->video_layer_for_testing()->visible());
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->previous_track_controls_view_for_testing(),
       GetOverlayWindow()->play_pause_controls_view_for_testing(),
       GetOverlayWindow()->next_track_controls_view_for_testing()},
      false));
}

// Tests that we can enter Picture-in-Picture when a video is not preloaded,
// using the metadata optimizations. This test is checking that there are no
// crashes.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterMetadataPosterOptimisation) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL(
          "media/picture-in-picture/player_metadata_poster.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);

  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
}

// Tests that calling PictureInPictureWindowController::Close() twice has no
// side effect.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseTwiceSideEffects) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));

  window_controller()->Close(true /* should_pause_video */);

  // Wait for the window to close.
  WaitForTitle(active_web_contents, u"leavepictureinpicture");

  // Video is paused after Picture-in-Picture window was closed.
  EXPECT_EQ(true, EvalJs(active_web_contents, "isPaused();"));

  // Resume playback.
  ASSERT_TRUE(ExecJs(active_web_contents, "video.play();"));
  EXPECT_EQ(false, EvalJs(active_web_contents, "isPaused();"));

  // This should be a no-op because the window is not visible.
  window_controller()->Close(true /* should_pause_video */);
  EXPECT_EQ(false, EvalJs(active_web_contents, "isPaused();"));
}

// Checks entering Picture-in-Picture on multiple tabs, where the initial tab
// has been closed.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PictureInPictureAfterClosingTab) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);
  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
  ASSERT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  // Open a new tab in the browser.
  AddTabAtIndex(1, test_page_url, ui::PAGE_TRANSITION_TYPED);
  ASSERT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Once the initiator tab is closed, the controller should also be torn down.
  browser()->tab_strip_model()->CloseWebContentsAt(0, 0);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Open video in Picture-in-Picture mode again, on the new tab.
  active_web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);
  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
  ASSERT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
}

// Closing a tab that lost Picture-in-Picture because a new tab entered it
// should not close the current Picture-in-Picture window.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PictureInPictureDoNotCloseAfterClosingTab) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(initial_web_contents != nullptr);

  SetUpWindowController(initial_web_contents);
  ASSERT_EQ(true, EvalJs(initial_web_contents, "enterPictureInPicture();"));
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  // Open a new tab in the browser and starts Picture-in-Picture.
  AddTabAtIndex(1, test_page_url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_web_contents != nullptr);

  content::PictureInPictureWindowController* pip_window_controller =
      content::PictureInPictureWindowController::GetOrCreateForWebContents(
          new_web_contents);
  ASSERT_EQ(true, EvalJs(new_web_contents, "enterPictureInPicture();"));
  EXPECT_TRUE(pip_window_controller->GetWindowForTesting()->IsVisible());

  WaitForTitle(initial_web_contents, u"leavepictureinpicture");

  // Closing the initial tab should not get the new tab to leave
  // Picture-in-Picture.
  browser()->tab_strip_model()->CloseWebContentsAt(0, 0);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(true, EvalJs(new_web_contents, "isInPictureInPicture();"));
}

// Killing an iframe that lost Picture-in-Picture because the main frame entered
// it should not close the current Picture-in-Picture window.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PictureInPictureDoNotCloseAfterKillingFrame) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/iframe-test.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  std::vector<content::RenderFrameHost*> render_frame_hosts =
      CollectAllRenderFrameHosts(active_web_contents);
  ASSERT_EQ(2u, render_frame_hosts.size());

  content::RenderFrameHost* iframe =
      render_frame_hosts[0] == active_web_contents->GetMainFrame()
          ? render_frame_hosts[1]
          : render_frame_hosts[0];

  ASSERT_EQ(true, EvalJs(iframe, "enterPictureInPicture();"));
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(false, EvalJs(iframe, "document.pictureInPictureElement == video"));

  // Removing the iframe should not lead to the main frame leaving
  // Picture-in-Picture.
  ASSERT_TRUE(ExecJs(active_web_contents, "removeFrame();"));

  EXPECT_EQ(1u, CollectAllRenderFrameHosts(active_web_contents).size());
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(true, EvalJs(active_web_contents,
                         "document.pictureInPictureElement == video"));
}

// Checks setting disablePictureInPicture on video just after requesting
// Picture-in-Picture doesn't result in a window opened.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       RequestPictureInPictureAfterDisablePictureInPicture) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  ASSERT_EQ("rejected", EvalJs(active_web_contents,
                               "requestPictureInPictureAndDisable();"));

  ASSERT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
}

// Checks that a video in Picture-in-Picture stops if its iframe is removed.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       FrameEnterLeaveClosesWindow) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/iframe-test.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  std::vector<content::RenderFrameHost*> render_frame_hosts =
      CollectAllRenderFrameHosts(active_web_contents);
  ASSERT_EQ(2u, render_frame_hosts.size());

  content::RenderFrameHost* iframe =
      render_frame_hosts[0] == active_web_contents->GetMainFrame()
          ? render_frame_hosts[1]
          : render_frame_hosts[0];

  ASSERT_EQ(true, EvalJs(iframe, "enterPictureInPicture();"));
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  ASSERT_TRUE(ExecJs(active_web_contents, "removeFrame();"));

  EXPECT_EQ(1u, CollectAllRenderFrameHosts(active_web_contents).size());
  EXPECT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CrossOriginFrameEnterLeaveCloseWindow) {
  GURL embed_url = embedded_test_server()->GetURL(
      "a.com", "/media/picture-in-picture/iframe-content.html");
  GURL main_url = embedded_test_server()->GetURL(
      "example.com", "/media/picture-in-picture/iframe-test.html?embed_url=" +
                         embed_url.spec());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  std::vector<content::RenderFrameHost*> render_frame_hosts =
      CollectAllRenderFrameHosts(active_web_contents);
  ASSERT_EQ(2u, render_frame_hosts.size());

  content::RenderFrameHost* iframe =
      render_frame_hosts[0] == active_web_contents->GetMainFrame()
          ? render_frame_hosts[1]
          : render_frame_hosts[0];

  ASSERT_EQ(true, EvalJs(iframe, "enterPictureInPicture();"));
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  ASSERT_TRUE(ExecJs(active_web_contents, "removeFrame();"));

  EXPECT_EQ(1u, CollectAllRenderFrameHosts(active_web_contents).size());
  EXPECT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       MultipleBrowserWindowOnePIPWindow) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::PictureInPictureWindowController* first_controller =
      window_controller();
  EXPECT_TRUE(first_controller->GetWindowForTesting()->IsVisible());

  Browser* second_browser = CreateBrowser(browser()->profile());
  LoadTabAndEnterPictureInPicture(
      second_browser, base::FilePath(kPictureInPictureWindowSizePage));

  content::PictureInPictureWindowController* second_controller =
      window_controller();
  EXPECT_FALSE(first_controller->GetWindowForTesting()->IsVisible());
  EXPECT_TRUE(second_controller->GetWindowForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterPictureInPictureThenNavigateAwayCloseWindow) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  // Same document navigations should not close Picture-in-Picture window.
  ASSERT_TRUE(ExecJs(active_web_contents,
                     "window.location = '#foo'; window.history.back();"));
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  // Picture-in-Picture window should be closed after navigating away.
  GURL another_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/iframe-size.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), another_page_url));
  EXPECT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
}

// Tests that the Picture-in-Picture state is properly updated when the window
// is closed at a system level.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseWindowNotifiesController) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_NE(GetOverlayWindow(), nullptr);
  ASSERT_TRUE(GetOverlayWindow()->IsVisible());

  GetOverlayWindow()->CloseNow();

  ExpectLeavePictureInPicture(active_web_contents);
}

// Tests that the play/pause icon state is properly updated when a
// Picture-in-Picture is created after a reload.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PlayPauseStateAtCreation) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ExecJs(active_web_contents, "video.play();"));

  EXPECT_EQ(false, EvalJs(active_web_contents, "isPaused();"));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  EXPECT_EQ(GetOverlayWindow()->playback_state_for_testing(),
            OverlayWindowViews::PlaybackState::kPlaying);

  EXPECT_TRUE(GetOverlayWindow()->video_layer_for_testing()->visible());

  ASSERT_TRUE(ExecJs(active_web_contents, "exitPictureInPicture();"));

  content::TestNavigationObserver observer(active_web_contents, 1);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();

  active_web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));

  EXPECT_EQ(true, EvalJs(active_web_contents, "isPaused();"));
  EXPECT_EQ(GetOverlayWindow()->playback_state_for_testing(),
            OverlayWindowViews::PlaybackState::kPaused);
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterUsingControllerShowsWindow) {
  auto* pip_window_manager = PictureInPictureWindowManager::GetInstance();
  ASSERT_NE(nullptr, pip_window_manager);

  // Show the non-WebContents based Picture-in-Picture window controller.
  EXPECT_CALL(mock_controller(), Show());
  pip_window_manager->EnterPictureInPictureWithController(&mock_controller());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterUsingWebContentsThenUsingController) {
  // Enter using WebContents.
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  ASSERT_NE(nullptr, GetOverlayWindow());
  EXPECT_TRUE(GetOverlayWindow()->IsVisible());

  auto* pip_window_manager = PictureInPictureWindowManager::GetInstance();
  ASSERT_NE(nullptr, pip_window_manager);

  // The new Picture-in-Picture window should be shown.
  EXPECT_CALL(mock_controller(), Show());
  pip_window_manager->EnterPictureInPictureWithController(&mock_controller());

  // WebContents sourced Picture-in-Picture should stop.
  ExpectLeavePictureInPicture(
      browser()->tab_strip_model()->GetActiveWebContents());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterUsingControllerThenEnterUsingWebContents) {
  auto* pip_window_manager = PictureInPictureWindowManager::GetInstance();
  ASSERT_NE(nullptr, pip_window_manager);

  // Show the non-WebContents based Picture-in-Picture window controller.
  EXPECT_CALL(mock_controller(), GetWebContents())
      .WillOnce(testing::Return(nullptr));
  EXPECT_CALL(mock_controller(), Show());
  pip_window_manager->EnterPictureInPictureWithController(&mock_controller());

  // Now show the WebContents based Picture-in-Picture window controller.
  // This should close the existing window and show the new one.
  EXPECT_CALL(mock_controller(), Close(_));
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  ASSERT_TRUE(GetOverlayWindow());
  EXPECT_TRUE(GetOverlayWindow()->IsVisible());
}

// This checks that a video in Picture-in-Picture with preload none, when
// changing source willproperly update the associated media player id. This is
// checked by closing the window because the test it at a too high level to be
// able to check the actual media player id being used.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PreloadNoneSrcChangeThenLoad) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL(
          "media/picture-in-picture/player_preload_none.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);

  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
  ASSERT_EQ(true, EvalJs(active_web_contents, "changeSrcAndLoad();"));

  window_controller()->Close(true /* should_pause_video */);

  // The video should leave Picture-in-Picture mode.
  ExpectLeavePictureInPicture(active_web_contents);
}

// Tests that opening a Picture-in-Picture window from a video in an iframe
// will not lead to a crash when the tab is closed while devtools is open.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       OpenInFrameWithDevToolsDoesNotCrash) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/iframe-test.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  std::vector<content::RenderFrameHost*> render_frame_hosts =
      CollectAllRenderFrameHosts(active_web_contents);
  ASSERT_EQ(2u, render_frame_hosts.size());

  content::RenderFrameHost* iframe =
      render_frame_hosts[0] == active_web_contents->GetMainFrame()
          ? render_frame_hosts[1]
          : render_frame_hosts[0];

  // Attaching devtools triggers the change in timing that leads to the crash.
  DevToolsWindow* window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser(), true /*is_docked=*/);

  ASSERT_EQ(true, EvalJs(iframe, "enterPictureInPicture();"));
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  EXPECT_EQ(2u, CollectAllRenderFrameHosts(active_web_contents).size());

  // Open a new tab in the browser.
  AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED);
  ASSERT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Closing the initiator should not crash Chrome.
  content::WebContentsDestroyedWatcher destroyed_watcher(active_web_contents);
  browser()->tab_strip_model()->CloseWebContentsAt(0, 0);
  destroyed_watcher.Wait();

  // Make sure the window and therefore Chrome_DevToolsADBThread shutdown
  // gracefully.
  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that the back-to-tab, close, and resize controls move properly as
// the window changes quadrants.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       MovingQuadrantsMovesBackToTabAndResizeControls) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  ASSERT_NE(GetOverlayWindow(), nullptr);
  ASSERT_FALSE(GetOverlayWindow()->IsVisible());

  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));

  // The PiP window starts in the bottom-right quadrant of the screen.
  gfx::Rect bottom_right_bounds = GetOverlayWindow()->GetBounds();
  // The relative center point of the window.
  gfx::Point center(bottom_right_bounds.width() / 2,
                    bottom_right_bounds.height() / 2);
  gfx::Point close_button_position =
      GetOverlayWindow()->close_image_position_for_testing();
  gfx::Point resize_button_position =
      GetOverlayWindow()->resize_handle_position_for_testing();

  // The close button should be in the top right corner.
  EXPECT_LT(center.x(), close_button_position.x());
  EXPECT_GT(center.y(), close_button_position.y());
  // The resize button should be in the top left corner.
  EXPECT_GT(center.x(), resize_button_position.x());
  EXPECT_GT(center.y(), resize_button_position.y());
  // The resize button hit test should start a top left resizing drag.
  EXPECT_EQ(HTTOPLEFT, GetOverlayWindow()->GetResizeHTComponent());

  // Move the window to the bottom left corner.
  gfx::Rect bottom_left_bounds(0, bottom_right_bounds.y(),
                               bottom_right_bounds.width(),
                               bottom_right_bounds.height());
  GetOverlayWindow()->SetBounds(bottom_left_bounds);
  close_button_position =
      GetOverlayWindow()->close_image_position_for_testing();
  resize_button_position =
      GetOverlayWindow()->resize_handle_position_for_testing();

  // The close button should be in the top left corner.
  EXPECT_GT(center.x(), close_button_position.x());
  EXPECT_GT(center.y(), close_button_position.y());
  // The resize button should be in the top right corner.
  EXPECT_LT(center.x(), resize_button_position.x());
  EXPECT_GT(center.y(), resize_button_position.y());
  // The resize button hit test should start a top right resizing drag.
  EXPECT_EQ(HTTOPRIGHT, GetOverlayWindow()->GetResizeHTComponent());

  // Move the window to the top right corner.
  gfx::Rect top_right_bounds(bottom_right_bounds.x(), 0,
                             bottom_right_bounds.width(),
                             bottom_right_bounds.height());
  GetOverlayWindow()->SetBounds(top_right_bounds);
  close_button_position =
      GetOverlayWindow()->close_image_position_for_testing();
  resize_button_position =
      GetOverlayWindow()->resize_handle_position_for_testing();

  // The close button should be in the top right corner.
  EXPECT_LT(center.x(), close_button_position.x());
  EXPECT_GT(center.y(), close_button_position.y());
  // The resize button should be in the bottom left corner.
  EXPECT_GT(center.x(), resize_button_position.x());
  EXPECT_LT(center.y(), resize_button_position.y());
  // The resize button hit test should start a bottom left resizing drag.
  EXPECT_EQ(HTBOTTOMLEFT, GetOverlayWindow()->GetResizeHTComponent());

  // Move the window to the top left corner.
  gfx::Rect top_left_bounds(0, 0, bottom_right_bounds.width(),
                            bottom_right_bounds.height());
  GetOverlayWindow()->SetBounds(top_left_bounds);
  close_button_position =
      GetOverlayWindow()->close_image_position_for_testing();
  resize_button_position =
      GetOverlayWindow()->resize_handle_position_for_testing();

  // The close button should be in the top right corner.
  EXPECT_LT(center.x(), close_button_position.x());
  EXPECT_GT(center.y(), close_button_position.y());
  // The resize button should be in the bottom right corner.
  EXPECT_LT(center.x(), resize_button_position.x());
  EXPECT_LT(center.y(), resize_button_position.y());
  // The resize button hit test should start a bottom right resizing drag.
  EXPECT_EQ(HTBOTTOMRIGHT, GetOverlayWindow()->GetResizeHTComponent());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that the Play/Pause button is displayed appropriately in the
// Picture-in-Picture window.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PlayPauseButtonVisibility) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  ASSERT_NE(nullptr, GetOverlayWindow());

  // Play/Pause button is displayed if video is not a mediastream.
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->play_pause_controls_view_for_testing()}, true));

  // Play/Pause button is hidden if video is a mediastream.
  ASSERT_EQ(true,
            EvalJs(active_web_contents, "changeVideoSrcToMediaStream();"));
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->play_pause_controls_view_for_testing()}, false));

  // Play/Pause button is not hidden anymore when video is not a mediastream.
  ASSERT_EQ(true, EvalJs(active_web_contents, "changeVideoSrc();"));
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->play_pause_controls_view_for_testing()}, true));
}

// Check that page visibility API events are fired when tab is hidden, shown,
// and even occluded.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PageVisibilityEventsFired) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  ASSERT_TRUE(
      ExecJs(active_web_contents, "addVisibilityChangeEventListener();"));

  // Hide page and check that the document visibility is hidden.
  active_web_contents->WasHidden();
  WaitForTitle(active_web_contents, u"hidden");

  // Show page and check that the document visibility is visible.
  active_web_contents->WasShown();
  WaitForTitle(active_web_contents, u"visible");

  // Occlude page and check that the document visibility is hidden.
  active_web_contents->WasOccluded();
  WaitForTitle(active_web_contents, u"hidden");
}

// Check that page visibility API events are fired even when video is in
// Picture-in-Picture and video playback is not disrupted.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PageVisibilityEventsFiredWhenPictureInPicture) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_NE(nullptr, active_web_contents);

  ASSERT_TRUE(ExecJs(active_web_contents, "video.play();"));

  EXPECT_EQ(false, EvalJs(active_web_contents, "isPaused();"));

  ASSERT_TRUE(
      ExecJs(active_web_contents, "addVisibilityChangeEventListener();"));

  // Hide page and check that the document visibility is hidden.
  active_web_contents->WasHidden();
  WaitForTitle(active_web_contents, u"hidden");

  // Check that the video is still in Picture-in-Picture and playing.
  EXPECT_EQ(true, EvalJs(active_web_contents, "isInPictureInPicture();"));
  EXPECT_EQ(false, EvalJs(active_web_contents, "isPaused();"));

  // Show page and check that the document visibility is visible.
  active_web_contents->WasShown();
  WaitForTitle(active_web_contents, u"visible");

  // Check that the video is still in Picture-in-Picture and playing.
  EXPECT_EQ(true, EvalJs(active_web_contents, "isInPictureInPicture();"));
  EXPECT_EQ(false, EvalJs(active_web_contents, "isPaused();"));

  // Occlude page and check that the document visibility is hidden.
  active_web_contents->WasOccluded();
  WaitForTitle(active_web_contents, u"hidden");

  // Check that the video is still in Picture-in-Picture and playing.
  EXPECT_EQ(true, EvalJs(active_web_contents, "isInPictureInPicture();"));
  EXPECT_EQ(false, EvalJs(active_web_contents, "isPaused();"));
}

class PictureInPictureWindowControllerPrerenderBrowserTest
    : public PictureInPictureWindowControllerBrowserTest {
 public:
  PictureInPictureWindowControllerPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PictureInPictureWindowControllerPrerenderBrowserTest::
                GetWebContents,
            base::Unretained(this))) {}

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerPrerenderBrowserTest,
                       EnterPipThenNavigateAwayCloseWindow) {
  GURL test_page_url = embedded_test_server()->GetURL(
      "example.com", "/media/picture-in-picture/window-size.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  ASSERT_TRUE(GetWebContents());

  SetUpWindowController(GetWebContents());
  ASSERT_TRUE(window_controller());

  // Open Picture-in-Picture window
  ASSERT_EQ(true, EvalJs(GetWebContents(), "enterPictureInPicture();"));
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  // Navigation to prerendered page should not close Picture-in-Picture window.
  GURL prerendering_page_url = embedded_test_server()->GetURL(
      "example.com", "/media/picture-in-picture/window-size.html?prerender");
  prerender_test_helper().AddPrerender(prerendering_page_url);
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  // Picture-in-Picture window should be closed after navigating away.
  prerender_test_helper().NavigatePrimaryPage(prerendering_page_url);
  EXPECT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
}

class PictureInPictureWindowControllerFencedFrameBrowserTest
    : public PictureInPictureWindowControllerBrowserTest {
 public:
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerFencedFrameBrowserTest,
                       FencedFrameShouldNotCloseWindow) {
  GURL test_page_url = embedded_test_server()->GetURL(
      "example.com", "/media/picture-in-picture/window-size.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller() != nullptr);

  // Open Picture-in-Picture window
  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  // Navigation to fenced frame page should not close Picture-in-Picture window.
  GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          active_web_contents->GetMainFrame(), fenced_frame_url);
  EXPECT_NE(nullptr, fenced_frame_host);
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
}

class MediaSessionPictureInPictureWindowControllerBrowserTest
    : public PictureInPictureWindowControllerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PictureInPictureWindowControllerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MediaSession,SkipAd");
    scoped_feature_list_.InitWithFeatures(
        {media_session::features::kMediaSessionService}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a Skip Ad button is displayed in the Picture-in-Picture window
// when Media Session Action "skipad" is handled by the website.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       SkipAdButtonVisibility) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_NE(GetOverlayWindow(), nullptr);

  // Skip Ad button is not displayed initially.
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->skip_ad_controls_view_for_testing()}, false));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Skip Ad button is displayed if a media session action handler has been set.
  ASSERT_TRUE(
      ExecJs(active_web_contents, "setMediaSessionActionHandler('skipad');"));
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->skip_ad_controls_view_for_testing()}, true));

  // Unset action handler and check that Skip Ad button is not displayed.
  ASSERT_TRUE(
      ExecJs(active_web_contents, "unsetMediaSessionActionHandler('skipad');"));
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->skip_ad_controls_view_for_testing()}, false));
}

// Tests that the Play/Plause button is displayed in the Picture-in-Picture
// window when Media Session actions "play" and "pause" are handled by the
// website even if video is a media stream.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       PlayPauseButtonVisibility) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  ASSERT_NE(GetOverlayWindow(), nullptr);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Play/Pause button is hidden if playing video is a mediastream.
  ASSERT_EQ(true,
            EvalJs(active_web_contents, "changeVideoSrcToMediaStream();"));
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->play_pause_controls_view_for_testing()}, false));

  // Set Media Session action "play" handler and check that Play/Pause button
  // is still hidden.
  ASSERT_TRUE(
      ExecJs(active_web_contents, "setMediaSessionActionHandler('play');"));
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->play_pause_controls_view_for_testing()}, false));

  // Set Media Session action "pause" handler and check that Play/Pause button
  // is now displayed.
  ASSERT_TRUE(
      ExecJs(active_web_contents, "setMediaSessionActionHandler('pause');"));
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->play_pause_controls_view_for_testing()}, true));

  // Unset Media Session action "pause" handler and check that Play/Pause button
  // is hidden.
  ASSERT_TRUE(
      ExecJs(active_web_contents, "unsetMediaSessionActionHandler('pause');"));
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->play_pause_controls_view_for_testing()}, false));

  ASSERT_TRUE(ExecJs(active_web_contents, "exitPictureInPicture();"));

  // Reset Media Session action "pause" handler and check that Play/Pause
  // button is now displayed Picture-in-Picture is entered again.
  ASSERT_TRUE(
      ExecJs(active_web_contents, "setMediaSessionActionHandler('pause');"));
  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->play_pause_controls_view_for_testing()}, true));
}

// Tests that a Next Track button is displayed in the Picture-in-Picture window
// when Media Session Action "nexttrack" is handled by the website.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       NextTrackButtonVisibility) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_NE(GetOverlayWindow(), nullptr);

  // Next Track button is not displayed initially.
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->next_track_controls_view_for_testing()}, false));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Next Track button is displayed if a media session action handler has been
  // set.
  ASSERT_TRUE(ExecJs(active_web_contents,
                     "setMediaSessionActionHandler('nexttrack');"));
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->next_track_controls_view_for_testing()}, true));

  // Unset action handler and check that Next Track button is not displayed.
  ASSERT_TRUE(ExecJs(active_web_contents,
                     "unsetMediaSessionActionHandler('nexttrack');"));
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->next_track_controls_view_for_testing()}, false));
}

// Tests that a Previous Track button is displayed in the Picture-in-Picture
// window when Media Session Action "previoustrack" is handled by the website.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       PreviousTrackButtonVisibility) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  // Previous Track button is not displayed initially.
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->previous_track_controls_view_for_testing()}, false));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Previous Track button is displayed if a media session action handler has
  // been set.
  ASSERT_TRUE(ExecJs(active_web_contents,
                     "setMediaSessionActionHandler('previoustrack');"));
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->previous_track_controls_view_for_testing()}, true));

  // Unset action handler and check that Previous Track button is not displayed.
  ASSERT_TRUE(ExecJs(active_web_contents,
                     "unsetMediaSessionActionHandler('previoustrack');"));
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->previous_track_controls_view_for_testing()}, false));
}

// Tests that clicking the Skip Ad button in the Picture-in-Picture window
// calls the Media Session Action "skipad" handler function.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       SkipAdHandlerCalled) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      ExecJs(active_web_contents, "setMediaSessionActionHandler('skipad');"));
  ASSERT_EQ(true, EvalJs(active_web_contents, "ensureVideoIsPlaying();"));
  WaitForPlaybackState(active_web_contents,
                       OverlayWindowViews::PlaybackState::kPlaying);

  // Make sure the action handler is set before trying to invoke the action.
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->skip_ad_controls_view_for_testing()}, true));

  // Simulates user clicking "Skip Ad" and check the handler function is called.
  window_controller()->SkipAd();
  WaitForTitle(active_web_contents, u"skipad");
}

// Tests that clicking the Play/Pause button in the Picture-in-Picture window
// calls the Media Session actions "play" and "pause" handler functions.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       PlayPauseHandlersCalled) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Move the second player out of the way to simplify the "active/inactive"
  // media session state handling.
  ASSERT_TRUE(ExecJs(active_web_contents, "secondVideo.src = '';"));

  ASSERT_TRUE(
      ExecJs(active_web_contents, "setMediaSessionActionHandler('play');"));
  ASSERT_TRUE(
      ExecJs(active_web_contents, "setMediaSessionActionHandler('pause');"));

  // Make sure the action handlers are set before trying to invoke the actions.
  // In the case of MediaStream, the play/pause button is only visible if the
  // action handlers are set.
  ASSERT_EQ(true,
            EvalJs(active_web_contents, "changeVideoSrcToMediaStream();"));
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->play_pause_controls_view_for_testing()}, true));
  // Now we can switch back to regular src=, which is needed to be able to
  // unpause the video later (the MediaStream player leaves the media session
  // when paused, so no more Media Session action handling there).
  ASSERT_EQ(true, EvalJs(active_web_contents, "changeVideoSrc();"));
  WaitForPlaybackState(active_web_contents,
                       OverlayWindowViews::PlaybackState::kPlaying);

  // Simulates user clicking "Play/Pause" and check that the "pause" handler
  // function is called.
  window_controller()->TogglePlayPause();
  WaitForTitle(active_web_contents, u"pause");

  ASSERT_TRUE(ExecJs(active_web_contents, "video.pause();"));
  WaitForPlaybackState(active_web_contents,
                       OverlayWindowViews::PlaybackState::kPaused);

  // Simulates user clicking "Play/Pause" and check that the "play" handler
  // function is called.
  window_controller()->TogglePlayPause();
  WaitForTitle(active_web_contents, u"play");
}

// Tests that clicking the Next Track button in the Picture-in-Picture window
// calls the Media Session Action "nexttrack" handler function.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       NextTrackHandlerCalled) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ExecJs(active_web_contents,
                     "setMediaSessionActionHandler('nexttrack');"));
  ASSERT_EQ(true, EvalJs(active_web_contents, "ensureVideoIsPlaying();"));
  WaitForPlaybackState(active_web_contents,
                       OverlayWindowViews::PlaybackState::kPlaying);

  // Make sure the action handler is set before trying to invoke the action.
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->next_track_controls_view_for_testing()}, true));

  // Simulates user clicking "Next Track" and check the handler function is
  // called.
  window_controller()->NextTrack();
  WaitForTitle(active_web_contents, u"nexttrack");
}

// Tests that clicking the Previous Track button in the Picture-in-Picture
// window calls the Media Session Action "previoustrack" handler function.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       PreviousTrackHandlerCalled) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ExecJs(active_web_contents,
                     "setMediaSessionActionHandler('previoustrack');"));
  ASSERT_EQ(true, EvalJs(active_web_contents, "ensureVideoIsPlaying();"));
  WaitForPlaybackState(active_web_contents,
                       OverlayWindowViews::PlaybackState::kPlaying);

  // Make sure the action handler is set before trying to invoke the action.
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {GetOverlayWindow()->previous_track_controls_view_for_testing()}, true));

  // Simulates user clicking "Previous Track" and check the handler function is
  // called.
  window_controller()->PreviousTrack();
  WaitForTitle(active_web_contents, u"previoustrack");
}

// Tests that stopping Media Sessions closes the Picture-in-Picture window.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       StopMediaSessionClosesPictureInPictureWindow) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(true, EvalJs(active_web_contents, "ensureVideoIsPlaying();"));
  WaitForPlaybackState(active_web_contents,
                       OverlayWindowViews::PlaybackState::kPlaying);

  content::MediaSession::Get(active_web_contents)
      ->Stop(content::MediaSession::SuspendType::kUI);
  WaitForTitle(active_web_contents, u"leavepictureinpicture");
}

class AutoPictureInPictureWindowControllerBrowserTest
    : public PictureInPictureWindowControllerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PictureInPictureWindowControllerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "AutoPictureInPicture");
  }
};

// Hide page and check that entering Auto Picture-in-Picture is not triggered.
// This test is most likely going to be flaky the day the tested thing fails.
// Do NOT disable test. Ping /chrome/browser/picture_in_picture/OWNERS instead.
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWindowControllerBrowserTest,
                       AutoEnterPictureInPictureIsNotTriggeredInRegularWebApp) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  ASSERT_TRUE(ExecJs(active_web_contents, "video.play();"));
  ASSERT_TRUE(
      ExecJs(active_web_contents, "video.autoPictureInPicture = true;"));
  ASSERT_TRUE(
      ExecJs(active_web_contents, "addVisibilityChangeEventListener();"));

  // Hide page and check that there is no video that enters Picture-in-Picture
  // automatically.
  active_web_contents->WasHidden();
  WaitForTitle(active_web_contents, u"hidden");

  EXPECT_EQ(false, EvalJs(active_web_contents, "isInPictureInPicture();"));
}

// Show page and check that exiting Auto Picture-in-Picture is triggered.
// This test is most likely going to be flaky the day the tested thing fails.
// Do NOT disable test. Ping /chrome/browser/picture_in_picture/OWNERS instead.
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWindowControllerBrowserTest,
                       AutoExitPictureInPictureIsTriggeredInRegularWebApp) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  ASSERT_TRUE(ExecJs(active_web_contents, "video.play();"));
  ASSERT_TRUE(
      ExecJs(active_web_contents, "video.autoPictureInPicture = true;"));

  // Enter Picture-in-Picture manually.
  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));

  active_web_contents->WasHidden();

  // Show page and check that video left Picture-in-Picture automatically.
  active_web_contents->WasShown();
  WaitForTitle(active_web_contents, u"leavepictureinpicture");

  EXPECT_EQ(false, EvalJs(active_web_contents, "isInPictureInPicture();"));
}

namespace {

class ChromeContentBrowserClientOverrideWebAppScope
    : public ChromeContentBrowserClient {
 public:
  ChromeContentBrowserClientOverrideWebAppScope() = default;
  ~ChromeContentBrowserClientOverrideWebAppScope() override = default;

  void OverrideWebkitPrefs(
      content::WebContents* web_contents,
      blink::web_pref::WebPreferences* web_prefs) override {
    ChromeContentBrowserClient::OverrideWebkitPrefs(web_contents, web_prefs);

    web_prefs->web_app_scope = web_app_scope_;
  }

  void set_web_app_scope(const GURL& web_app_scope) {
    web_app_scope_ = web_app_scope;
  }

 private:
  GURL web_app_scope_;
};

}  // namespace

class WebAppPictureInPictureWindowControllerBrowserTest
    : public web_app::WebAppControllerBrowserTest {
 public:
  WebAppPictureInPictureWindowControllerBrowserTest() = default;

  WebAppPictureInPictureWindowControllerBrowserTest(
      const WebAppPictureInPictureWindowControllerBrowserTest&) = delete;
  WebAppPictureInPictureWindowControllerBrowserTest& operator=(
      const WebAppPictureInPictureWindowControllerBrowserTest&) = delete;

  ~WebAppPictureInPictureWindowControllerBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    web_app::WebAppControllerBrowserTest::SetUpCommandLine(command_line);
  }

  GURL main_url() {
    return https_server()->GetURL(
        "/extensions/auto_picture_in_picture/main.html");
  }

  Browser* InstallAndLaunchPWA(const GURL& start_url) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.DeprecatedGetOriginAsURL();
    web_app_info->user_display_mode = blink::mojom::DisplayMode::kStandalone;
    const web_app::AppId app_id = InstallWebApp(std::move(web_app_info));

    Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);
    web_contents_ = app_browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(web_contents_));
    return app_browser;
  }

  content::WebContents* web_contents() { return web_contents_; }

 private:
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

// Hide pwa page and check that Picture-in-Picture is entered automatically.
IN_PROC_BROWSER_TEST_F(WebAppPictureInPictureWindowControllerBrowserTest,
                       AutoEnterPictureInPicture) {
  InstallAndLaunchPWA(main_url());
  ASSERT_EQ(true, EvalJs(web_contents(), "playVideo();"));
  ASSERT_TRUE(ExecJs(web_contents(), "video.autoPictureInPicture = true;"));

  // Hide page and check that video entered Picture-in-Picture automatically.
  web_contents()->WasHidden();
  WaitForTitle(web_contents(), u"video.enterpictureinpicture");
}

// Show pwa page and check that Auto Picture-in-Picture is exited automatically.
IN_PROC_BROWSER_TEST_F(WebAppPictureInPictureWindowControllerBrowserTest,
                       AutoExitPictureInPicture) {
  InstallAndLaunchPWA(main_url());
  ASSERT_EQ(true, EvalJs(web_contents(), "playVideo();"));
  ASSERT_TRUE(ExecJs(web_contents(), "video.autoPictureInPicture = true;"));
  ASSERT_EQ(true, EvalJs(web_contents(), "enterPictureInPicture();"));

  web_contents()->WasHidden();

  // Show page and check that video left Picture-in-Picture automatically.
  web_contents()->WasShown();
  WaitForTitle(web_contents(), u"video.leavepictureinpicture");
}

// Show pwa page and check that Auto Picture-in-Picture is not triggered if
// document is not inside the scope specified in the Web App Manifest.
IN_PROC_BROWSER_TEST_F(
    WebAppPictureInPictureWindowControllerBrowserTest,
    AutoPictureInPictureNotTriggeredIfDocumentNotInWebAppScope) {
  // We open a web app with a different scope
  // Then go to our usual test page.
  Browser* app_browser = InstallAndLaunchPWA(
      https_server()->GetURL("www.foobar.com", "/web_apps/basic.html"));
  web_app::NavigateToURLAndWait(app_browser, main_url());
  EXPECT_TRUE(app_browser->app_controller()->ShouldShowCustomTabBar());

  ASSERT_EQ(true, EvalJs(web_contents(), "playVideo();"));
  ASSERT_TRUE(ExecJs(web_contents(), "video.autoPictureInPicture = true;"));

  // Hide page and check that the video did not entered
  // Picture-in-Picture automatically.
  web_contents()->WasHidden();
  WaitForTitle(web_contents(), u"hidden");

  EXPECT_EQ(false, EvalJs(web_contents(), "isInPictureInPicture();"));
}

// Show pwa page and check that Auto Picture-in-Picture is not triggered if
// video is not playing.
IN_PROC_BROWSER_TEST_F(WebAppPictureInPictureWindowControllerBrowserTest,
                       AutoPictureInPictureNotTriggeredIfVideoNotPlaying) {
  InstallAndLaunchPWA(main_url());
  ASSERT_TRUE(ExecJs(web_contents(), "video.autoPictureInPicture = true;"));
  EXPECT_EQ(true, EvalJs(web_contents(), "isPaused();"));

  // Hide page and check that the video did not entered
  // Picture-in-Picture automatically.
  web_contents()->WasHidden();
  WaitForTitle(web_contents(), u"hidden");

  EXPECT_EQ(false, EvalJs(web_contents(), "isInPictureInPicture();"));
}

// Check that Auto Picture-in-Picture is not triggered if there's already a
// video in Picture-in-Picture.
IN_PROC_BROWSER_TEST_F(
    WebAppPictureInPictureWindowControllerBrowserTest,
    AutoPictureInPictureWhenPictureInPictureWindowAlreadyVisible) {
  InstallAndLaunchPWA(main_url());

  // Enter Picture-in-Picture for the first video and set Auto
  // Picture-in-Picture for the second video.
  ASSERT_EQ(true, EvalJs(web_contents(), "playVideo();"));
  ASSERT_EQ(true, EvalJs(web_contents(), "enterPictureInPicture();"));
  ASSERT_EQ(true, EvalJs(web_contents(), "playSecondVideo();"));
  ASSERT_TRUE(
      ExecJs(web_contents(), "secondVideo.autoPictureInPicture = true;"));

  // Hide page and check that the second video did not entered
  // Picture-in-Picture automatically.
  web_contents()->WasHidden();
  WaitForTitle(web_contents(), u"hidden");

  // Check that the first video is still in Picture-in-Picture.
  EXPECT_EQ(true, EvalJs(web_contents(), "isInPictureInPicture();"));
}

// Check that video does not leave Picture-in-Picture automatically when it
// doesn't have the Auto Picture-in-Picture attribute set.
IN_PROC_BROWSER_TEST_F(
    WebAppPictureInPictureWindowControllerBrowserTest,
    AutoPictureInPictureNotTriggeredOnPageShownIfNoAttribute) {
  InstallAndLaunchPWA(main_url());
  ASSERT_TRUE(ExecJs(web_contents(), "video.autoPictureInPicture = false;"));

  ASSERT_EQ(true, EvalJs(web_contents(), "playVideo();"));
  ASSERT_EQ(true, EvalJs(web_contents(), "enterPictureInPicture();"));

  web_contents()->WasHidden();

  // Show page and check that video did not leave Picture-in-Picture
  // automatically as it doesn't have the Auto Picture-in-Picture attribute
  // set.
  web_contents()->WasShown();
  WaitForTitle(web_contents(), u"visible");

  // Check that the video is still in Picture-in-Picture.
  EXPECT_EQ(true, EvalJs(web_contents(), "isInPictureInPicture();"));
}

// Check that Auto Picture-in-Picture applies only to the video element whose
// autoPictureInPicture attribute was set most recently.
IN_PROC_BROWSER_TEST_F(WebAppPictureInPictureWindowControllerBrowserTest,
                       AutoPictureInPictureAttributeAppliesToLastElement) {
  InstallAndLaunchPWA(main_url());
  ASSERT_TRUE(ExecJs(web_contents(),
                     "video.autoPictureInPicture = true;"
                     "secondVideo.autoPictureInPicture = true;"));
  ASSERT_EQ(true, EvalJs(web_contents(), "playVideo();"));
  ASSERT_EQ(true, EvalJs(web_contents(), "playSecondVideo();"));

  // Hide page and check that second video is the video that enters
  // Picture-in-Picture automatically.
  web_contents()->WasHidden();
  WaitForTitle(web_contents(), u"secondVideo.enterpictureinpicture");
}

IN_PROC_BROWSER_TEST_F(WebAppPictureInPictureWindowControllerBrowserTest,
                       AutoPictureInPictureAttributeAppliesToInsertedElement) {
  InstallAndLaunchPWA(main_url());
  ASSERT_TRUE(ExecJs(web_contents(), "video.autoPictureInPicture = true;"));
  ASSERT_EQ(true, EvalJs(web_contents(), "playVideo();"));
  ASSERT_EQ(true,
            EvalJs(web_contents(), "addHtmlVideoWithAutoPictureInPicture();"));

  // Hide the page and check that the inserted video is the video that enters
  // Picture-in-Picture automatically.
  web_contents()->WasHidden();
  WaitForTitle(web_contents(), u"htmlVideo.enterpictureinpicture");
}

IN_PROC_BROWSER_TEST_F(WebAppPictureInPictureWindowControllerBrowserTest,
                       AutoPictureInPictureCanBeUnset) {
  InstallAndLaunchPWA(main_url());
  ASSERT_TRUE(ExecJs(web_contents(), "video.autoPictureInPicture = true;"));
  ASSERT_EQ(true, EvalJs(web_contents(), "playVideo();"));

  // Unset autoPictureInPicture and check that the video doesn't enter
  // Picture-in-Picture automatically.
  ASSERT_TRUE(ExecJs(web_contents(), "video.autoPictureInPicture = false;"));

  EXPECT_EQ(false, EvalJs(web_contents(), "isInPictureInPicture();"));

  web_contents()->WasHidden();
  WaitForTitle(web_contents(), u"hidden");

  EXPECT_EQ(false, EvalJs(web_contents(), "isInPictureInPicture();"));
}

IN_PROC_BROWSER_TEST_F(WebAppPictureInPictureWindowControllerBrowserTest,
                       AutoPictureInPictureAppliesAfterUnsetOnAnotherVideo) {
  InstallAndLaunchPWA(main_url());
  ASSERT_TRUE(ExecJs(web_contents(),
                     "video.autoPictureInPicture = true;"
                     "secondVideo.autoPictureInPicture = true;"));
  ASSERT_EQ(true, EvalJs(web_contents(), "playVideo();"));
  ASSERT_EQ(true, EvalJs(web_contents(), "playSecondVideo();"));

  // Unset autoPictureInPicture on the element where it was set last, and check
  // that the first video enters Picture-in-Picture automatically.
  ASSERT_TRUE(
      ExecJs(web_contents(), "secondVideo.autoPictureInPicture = false;"));

  EXPECT_EQ(false, EvalJs(web_contents(), "isInPictureInPicture();"));

  web_contents()->WasHidden();
  WaitForTitle(web_contents(), u"video.enterpictureinpicture");

  EXPECT_EQ(true, EvalJs(web_contents(), "isInPictureInPicture();"));
}

// Check that video does not leave Picture-in-Picture automatically when it is
// not the most recent element with the Auto Picture-in-Picture attribute set.
IN_PROC_BROWSER_TEST_F(
    WebAppPictureInPictureWindowControllerBrowserTest,
    AutoPictureInPictureNotTriggeredOnPageShownIfNotEnteredAutoPictureInPicture) {
  InstallAndLaunchPWA(main_url());
  ASSERT_TRUE(ExecJs(web_contents(),
                     "video.autoPictureInPicture = true;"
                     "secondVideo.autoPictureInPicture = true;"));
  ASSERT_EQ(true, EvalJs(web_contents(), "playVideo();"));
  ASSERT_EQ(true, EvalJs(web_contents(), "playSecondVideo();"));
  ASSERT_EQ(true, EvalJs(web_contents(), "enterPictureInPicture();"));

  web_contents()->WasHidden();

  // Show page and check that video did not leave Picture-in-Picture
  // automatically as it's not the most recent element with the Auto
  // Picture-in-Picture attribute set.
  web_contents()->WasShown();
  WaitForTitle(web_contents(), u"visible");

  // Check that the video is still in Picture-in-Picture.
  EXPECT_EQ(true, EvalJs(web_contents(), "isInPictureInPicture();"));
}

// Check that video with no audio that is paused when hidden is still eligible
// to enter Auto Picture-in-Picture and resumes playback.
IN_PROC_BROWSER_TEST_F(
    WebAppPictureInPictureWindowControllerBrowserTest,
    AutoPictureInPictureTriggeredOnPageHiddenIfVideoPausedWhenHidden) {
  InstallAndLaunchPWA(main_url());

  ASSERT_EQ(true,
            EvalJs(web_contents(), "changeVideoSrcToNoAudioTrackVideo();"));
  ASSERT_TRUE(ExecJs(web_contents(), "video.autoPictureInPicture = true;"));

  // Hide page and check that video entered Picture-in-Picture automatically
  // and is playing.
  web_contents()->WasHidden();
  WaitForTitle(web_contents(), u"video.enterpictureinpicture");

  // Check that video playback is still playing.
  EXPECT_EQ(false, EvalJs(web_contents(), "isPaused();"));
}

// Check that video with no audio that is paused when hidden resumes playback
// when it enters Picture-in-Picture.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       VideoWithNoAudioPausedWhenHiddenResumesPlayback) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  ASSERT_EQ(true, EvalJs(active_web_contents,
                         "changeVideoSrcToNoAudioTrackVideo();"));

  ASSERT_TRUE(ExecJs(active_web_contents, "addPauseEventListener();"));

  // Hide page and check that the video is paused first.
  active_web_contents->WasHidden();
  WaitForTitle(active_web_contents, u"pause");

  ASSERT_TRUE(ExecJs(active_web_contents, "addPlayEventListener();"));

  // Enter Picture-in-Picture.
  ASSERT_EQ(true, EvalJs(active_web_contents, "enterPictureInPicture();"));

  // Check that video playback has resumed.
  WaitForTitle(active_web_contents, u"play");
}

// Tests that exiting Picture-in-Picture when the video has no source fires the
// event and resolves the callback.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       ExitFireEventAndCallbackWhenNoSource) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      ExecJs(active_web_contents, "video.src=''; exitPictureInPicture();"));

  WaitForTitle(active_web_contents, u"leavepictureinpicture");
}

// Tests that play/pause video playback is toggled if there are no focus
// afforfances on the Picture-in-Picture window buttons when user hits space
// keyboard key.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       SpaceKeyTogglePlayPause) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ExecJs(active_web_contents, "video.play();"));
  ASSERT_TRUE(ExecJs(active_web_contents, "addPauseEventListener();"));

  ASSERT_NE(GetOverlayWindow(), nullptr);
  ASSERT_FALSE(GetOverlayWindow()->GetFocusManager()->GetFocusedView());

  ui::KeyEvent space_key_pressed(ui::ET_KEY_PRESSED, ui::VKEY_SPACE,
                                 ui::DomCode::SPACE, ui::EF_NONE);
  GetOverlayWindow()->OnKeyEvent(&space_key_pressed);

  WaitForTitle(active_web_contents, u"pause");
}

class VideoConferencingPictureInPictureWindowControllerBrowserTest
    : public PictureInPictureWindowControllerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitAndEnableFeature(media::kMediaSessionWebRTC);
    PictureInPictureWindowControllerBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that video conferencing action buttons function correctly.
IN_PROC_BROWSER_TEST_F(
    VideoConferencingPictureInPictureWindowControllerBrowserTest,
    VideoConferencingActions) {
  // Enter PiP.
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureVideoConferencingPage));

  // The overlay window should exist.
  ASSERT_NE(GetOverlayWindow(), nullptr);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ToggleMicrophoneButton* toggle_microphone_button =
      GetOverlayWindow()->toggle_microphone_button_for_testing();
  ToggleCameraButton* toggle_camera_button =
      GetOverlayWindow()->toggle_camera_button_for_testing();
  HangUpButton* hang_up_button =
      GetOverlayWindow()->hang_up_button_for_testing();

  // Wait for the controls to become visible.
  GetOverlayWindow()->ForceControlsVisibleForTesting(true);
  EXPECT_NO_FATAL_FAILURE(AssertControlsVisible(
      {toggle_microphone_button, toggle_camera_button, hang_up_button}, true));

  // The ToggleMicrophoneButton should be in the muted state (set by the page).
  EXPECT_TRUE(toggle_microphone_button->is_muted_for_testing());

  // Pressing the ToggleMicrophoneButton should call the page's handler, which
  // sets the page title and sets the muted state to unmuted.
  ClickButton(toggle_microphone_button);
  WaitForTitle(active_web_contents, u"togglemicrophone");
  EXPECT_FALSE(toggle_microphone_button->is_muted_for_testing());

  // The ToggleCameraButton should be in the off state (set by the page).
  EXPECT_FALSE(toggle_camera_button->is_turned_on_for_testing());

  // Pressing the ToggleCameraButton should call the page's handler, which sets
  // the page title and sets the button state the turned on.
  ClickButton(toggle_camera_button);
  WaitForTitle(active_web_contents, u"togglecamera");
  EXPECT_TRUE(toggle_camera_button->is_turned_on_for_testing());

  // Pressing the HangUpButton should call the page's handler, which sets the
  // page title.
  ClickButton(hang_up_button);
  WaitForTitle(active_web_contents, u"hangup");
}
