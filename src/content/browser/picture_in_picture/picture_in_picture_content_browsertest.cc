// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_window_controller_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "services/media_session/public/cpp/features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom.h"

namespace content {

namespace {

class TestOverlayWindow : public OverlayWindow {
 public:
  TestOverlayWindow() = default;

  TestOverlayWindow(const TestOverlayWindow&) = delete;
  TestOverlayWindow& operator=(const TestOverlayWindow&) = delete;

  ~TestOverlayWindow() override = default;

  bool IsActive() override { return false; }
  void Close() override { visible_ = false; }
  void ShowInactive() override { visible_ = true; }
  void Hide() override { visible_ = false; }
  bool IsVisible() override { return visible_; }
  bool IsAlwaysOnTop() override { return false; }
  gfx::Rect GetBounds() override { return gfx::Rect(size_); }
  void UpdateVideoSize(const gfx::Size& natural_size) override {
    size_ = natural_size;
  }
  void SetPlaybackState(PlaybackState playback_state) override {
    playback_state_ = playback_state;

    if (expected_playback_state_) {
      CHECK(playback_state_changed_callback_);
      if (playback_state == expected_playback_state_) {
        expected_playback_state_.reset();
        std::move(playback_state_changed_callback_).Run();
      }
    }
  }
  void SetPlayPauseButtonVisibility(bool is_visible) override {
    play_pause_button_visible_ = is_visible;
  }
  void SetSkipAdButtonVisibility(bool is_visible) override {}
  void SetNextTrackButtonVisibility(bool is_visible) override {
    next_track_button_visible_ = is_visible;
  }
  void SetPreviousTrackButtonVisibility(bool is_visible) override {}
  void SetMicrophoneMuted(bool muted) override {}
  void SetCameraState(bool turned_on) override {}
  void SetToggleMicrophoneButtonVisibility(bool is_visible) override {}
  void SetToggleCameraButtonVisibility(bool is_visible) override {}
  void SetHangUpButtonVisibility(bool is_visible) override {}
  void SetSurfaceId(const viz::SurfaceId& surface_id) override {}
  cc::Layer* GetLayerForTesting() override { return nullptr; }

  const absl::optional<PlaybackState>& playback_state() const {
    return playback_state_;
  }

  void SetPlaybackStateChangedCallback(PlaybackState state,
                                       base::OnceClosure callback) {
    expected_playback_state_ = state;
    playback_state_changed_callback_ = std::move(callback);
  }

  const absl::optional<bool>& play_pause_button_visible() const {
    return play_pause_button_visible_;
  }

  const absl::optional<bool>& next_track_button_visible() const {
    return next_track_button_visible_;
  }

 private:
  // We maintain the visibility state so that
  // PictureInPictureWindowControllerImpl::Close() sees that the window is
  // visible and proceeds to initiate leaving PiP.
  bool visible_ = false;

  gfx::Size size_;
  absl::optional<PlaybackState> playback_state_;

  absl::optional<PlaybackState> expected_playback_state_;
  base::OnceClosure playback_state_changed_callback_;

  absl::optional<bool> play_pause_button_visible_;
  absl::optional<bool> next_track_button_visible_;
};

class TestContentBrowserClient : public ContentBrowserClient {
 public:
  std::unique_ptr<OverlayWindow> CreateWindowForPictureInPicture(
      PictureInPictureWindowController* controller) override {
    return std::make_unique<TestOverlayWindow>();
  }
  bool CanEnterFullscreenWithoutUserActivation() override { return true; }
};

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  explicit TestWebContentsDelegate(Shell* shell) : shell_(shell) {}

  void EnterFullscreenModeForTab(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override {
    shell_->EnterFullscreenModeForTab(requesting_frame, options);
  }
  void ExitFullscreenModeForTab(WebContents* web_contents) override {
    shell_->ExitFullscreenModeForTab(web_contents);
  }
  bool IsFullscreenForTabOrPending(const WebContents* web_contents) override {
    return shell_->IsFullscreenForTabOrPending(web_contents);
  }
  PictureInPictureResult EnterPictureInPicture(
      WebContents* web_contents,
      const viz::SurfaceId&,
      const gfx::Size& natural_size) override {
    is_in_picture_in_picture_ = true;
    return PictureInPictureResult::kSuccess;
  }
  void ExitPictureInPicture() override { is_in_picture_in_picture_ = false; }

  bool is_in_picture_in_picture() const { return is_in_picture_in_picture_; }

 private:
  const raw_ptr<Shell> shell_;
  bool is_in_picture_in_picture_ = false;
};

class PictureInPictureContentBrowserTest : public ContentBrowserTest {
 public:
  ~PictureInPictureContentBrowserTest() override {
    if (old_browser_client_.has_value())
      SetBrowserClientForTesting(old_browser_client_.value());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "PictureInPictureAPI");
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    old_browser_client_ = SetBrowserClientForTesting(&content_browser_client_);

    web_contents_delegate_ = std::make_unique<TestWebContentsDelegate>(shell());
    shell()->web_contents()->SetDelegate(web_contents_delegate_.get());
  }

  void WaitForPlaybackState(OverlayWindow::PlaybackState playback_state,
                            const base::Location& location = FROM_HERE) {
    if (overlay_window()->playback_state() == playback_state)
      return;

    base::RunLoop run_loop;
    overlay_window()->SetPlaybackStateChangedCallback(playback_state,
                                                      run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_EQ(overlay_window()->playback_state(), playback_state)
        << "The wait was started here: " << location.ToString();
  }

  // Waits until the Shell's WebContents has the expected title.
  void WaitForTitle(const std::u16string& expected_title,
                    const base::Location& location = FROM_HERE) {
    EXPECT_EQ(
        expected_title,
        TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle())
        << "The wait was started here: " << location.ToString();
  }

  TestWebContentsDelegate* web_contents_delegate() {
    return web_contents_delegate_.get();
  }

  PictureInPictureWindowControllerImpl* window_controller() {
    return PictureInPictureWindowControllerImpl::FromWebContents(
        shell()->web_contents());
  }

  TestOverlayWindow* overlay_window() {
    return static_cast<TestOverlayWindow*>(
        window_controller()->GetWindowForTesting());
  }

 private:
  std::unique_ptr<TestWebContentsDelegate> web_contents_delegate_;
  absl::optional<raw_ptr<ContentBrowserClient>> old_browser_client_;
  TestContentBrowserClient content_browser_client_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       RequestSecondVideoInSameRFHDoesNotCloseWindow) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "two-videos.html")));

  // Play first video.
  ASSERT_TRUE(ExecJs(shell(), "videos[0].play();"));

  WaitForTitle(u"videos[0] playing");

  // Play second video.
  ASSERT_TRUE(ExecJs(shell(), "videos[1].play();"));

  WaitForTitle(u"videos[1] playing");

  ASSERT_FALSE(web_contents_delegate()->is_in_picture_in_picture());

  // Send first video in Picture-in-Picture.
  ASSERT_TRUE(ExecJs(shell(), "videos[0].requestPictureInPicture();"));

  WaitForTitle(u"videos[0] entered picture-in-picture");
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());

  // Send second video in Picture-in-Picture.
  ASSERT_TRUE(ExecJs(shell(), "videos[1].requestPictureInPicture();"));

  WaitForTitle(u"videos[1] entered picture-in-picture");

  // The session should still be active and ExitPictureInPicture() never called.
  EXPECT_NE(nullptr, window_controller()->active_session_for_testing());
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       RequestSecondVideoInDifferentRFHDoesNotCloseWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL(
          "example.com", "/media/picture_in_picture/two-videos.html")));

  WaitForTitle(u"iframe loaded");

  // Play first video.
  ASSERT_TRUE(ExecJs(shell(), "videos[0].play();"));

  WaitForTitle(u"videos[0] playing");

  // Play second video (in iframe).
  ASSERT_TRUE(ExecJs(shell(), "iframeVideos[0].play();"));

  WaitForTitle(u"iframeVideos[0] playing");

  ASSERT_FALSE(web_contents_delegate()->is_in_picture_in_picture());

  // Send first video in Picture-in-Picture.
  ASSERT_TRUE(ExecJs(shell(), "videos[0].requestPictureInPicture();"));

  WaitForTitle(u"videos[0] entered picture-in-picture");
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());

  // Send second video in Picture-in-Picture.
  ASSERT_TRUE(ExecJs(shell(), "iframeVideos[0].requestPictureInPicture();"));

  WaitForTitle(u"iframeVideos[0] entered picture-in-picture");

  // The session should still be active and ExitPictureInPicture() never called.
  EXPECT_NE(nullptr, window_controller()->active_session_for_testing());
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       EnterPictureInPictureThenFullscreen) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  ASSERT_TRUE(web_contents_delegate()->is_in_picture_in_picture());

  // The Picture-in-Picture window should be closed upon entering fullscreen.
  ASSERT_EQ(true, EvalJs(shell(), "enterFullscreen();"));

  EXPECT_TRUE(shell()->web_contents()->IsFullscreen());
  EXPECT_FALSE(web_contents_delegate()->is_in_picture_in_picture());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       EnterFullscreenThenPictureInPicture) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_EQ(true, EvalJs(shell(), "enterFullscreen();"));
  ASSERT_TRUE(shell()->web_contents()->IsFullscreen());

  // We should leave fullscreen upon entering Picture-in-Picture.
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  EXPECT_FALSE(shell()->web_contents()->IsFullscreen());
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());
}

// Check that the playback state in the Picture-in-Picture window follows the
// state of the media player.
IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       EnterPictureInPictureForPausedPlayer) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  // Play and pause the player from script.
  ASSERT_EQ(true, EvalJs(shell(), "play();"));
  ASSERT_TRUE(ExecJs(shell()->web_contents(), "video.pause();"));

  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  EXPECT_EQ(overlay_window()->playback_state(),
            OverlayWindow::PlaybackState::kPaused);

  // Simulate resuming playback by interacting with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "addPlayEventListener();"));
  window_controller()->TogglePlayPause();

  WaitForTitle(u"play");
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPlaying);

  // Simulate pausing playback by interacting with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "addPauseEventListener();"));
  window_controller()->TogglePlayPause();

  WaitForTitle(u"pause");
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPaused);
}

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       CanvasCaptureRespondsToUserAction) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL test_page_url = embedded_test_server()->GetURL(
      "example.com", "/media/picture_in_picture/canvas-in-pip.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_page_url));
  ASSERT_EQ(true, EvalJs(shell(), "start();"));
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPlaying);

  window_controller()->TogglePlayPause();
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPaused);
}

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       PlayerRespondsToUserActionsAfterSrcUpdate) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_EQ(true, EvalJs(shell(), "play();"));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  ASSERT_EQ(true, EvalJs(shell(), "updateVideoSrcAndPlay();"));

  window_controller()->TogglePlayPause();
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPaused);
}

// Tests that when closing the window after the player was reset, the <video>
// element is still notified.
IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       ClosingWindowWithPlayerResetNotifiesElement) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  ASSERT_TRUE(ExecJs(shell(), "addPictureInPictureEventListeners();"));

  ASSERT_EQ(true, EvalJs(shell(), "resetVideo();"));

  window_controller()->Close(/*should_pause_video=*/true);

  WaitForTitle(u"leavepictureinpicture");
}

// When the player object associated with a video element is destroyed, closing
// the Picture-in-Picture window is the only interaction possible. Thus the
// play/pause/replay button should be hidden.
IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       ResettingPlayerHidesPlayPause) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  ASSERT_EQ(true, EvalJs(shell(), "resetVideo();"));

  EXPECT_EQ(overlay_window()->play_pause_button_visible().value_or(true),
            false);

  // Load new media on the video element. This creates a new player.
  ASSERT_EQ(true, EvalJs(shell(), "updateVideoSrcAndPlay();"));

  // The play/pause/replay button should be functional again.
  EXPECT_EQ(overlay_window()->play_pause_button_visible().value_or(false),
            true);
  window_controller()->TogglePlayPause();
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPaused);
}

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       PlaybackStateWhenReopenedAfterEndOfStream) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_TRUE(ExecJs(shell(), "addPictureInPictureEventListeners();"));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  ASSERT_EQ(true, EvalJs(shell(), "playToEnd();"));
  WaitForPlaybackState(OverlayWindow::PlaybackState::kEndOfVideo);

  window_controller()->Close(/*should_pause_video=*/false);
  WaitForTitle(u"leavepictureinpicture");

  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  WaitForPlaybackState(OverlayWindow::PlaybackState::kEndOfVideo);
}

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       PlaybackStateWhenSeekingWhilePausedAfterEndOfStream) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  ASSERT_EQ(true, EvalJs(shell(), "playToEnd();"));
  WaitForPlaybackState(OverlayWindow::PlaybackState::kEndOfVideo);

  ASSERT_TRUE(ExecJs(shell(), "video.currentTime = 0;"));

  WaitForPlaybackState(OverlayWindow::PlaybackState::kPaused);
}

class MediaSessionPictureInPictureContentBrowserTest
    : public PictureInPictureContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "PictureInPictureAPI,MediaSession");
    scoped_feature_list_.InitWithFeatures(
        {media_session::features::kMediaSessionService}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Check that the playback state in the Picture-in-Picture window follows the
// state of the media player.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureContentBrowserTest,
                       EnterPictureInPictureForPausedPlayer) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  // Play and pause the player from script.
  ASSERT_EQ(true, EvalJs(shell(), "play();"));
  ASSERT_TRUE(ExecJs(shell()->web_contents(), "video.pause();"));

  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  EXPECT_EQ(overlay_window()->playback_state(),
            OverlayWindow::PlaybackState::kPaused);

  // Simulate resuming playback by invoking the Media Session "play" action
  // through interaction with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionPlayActionHandler();"));
  ASSERT_TRUE(ExecJs(shell(), "addPlayEventListener();"));
  window_controller()->TogglePlayPause();

  WaitForTitle(u"play");
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPlaying);

  // Simulate pausing playback by invoking the Media Session "pause" action
  // through interaction with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionPauseActionHandler();"));
  ASSERT_TRUE(ExecJs(shell(), "addPauseEventListener();"));
  window_controller()->TogglePlayPause();

  WaitForTitle(u"pause");
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPaused);
}

// Tests that an audio player and a canvas-capture player in Picture-in-Picture
// running in one frame coexist peacefully: Media Session actions should work.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureContentBrowserTest,
                       AudioPlayerWithPictureInPictureCanvasPlayer) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL test_page_url = embedded_test_server()->GetURL(
      "example.com", "/media/picture_in_picture/audio-and-canvas.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_page_url));

  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionPlayActionHandler();"));
  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionPauseActionHandler();"));

  // Start the audio player and enter PiP for the canvas player. The PiP
  // window should be in the "playing" state.
  ASSERT_TRUE(ExecJs(shell(), "addPlayEventListener();"));
  ASSERT_EQ(true, EvalJs(shell(), "start();"));
  WaitForTitle(u"play");
  ASSERT_EQ(overlay_window()->playback_state(),
            OverlayWindow::PlaybackState::kPlaying);

  // Simulate pausing playback by invoking the Media Session "pause" action
  // through interaction with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "addPauseEventListener();"));
  window_controller()->TogglePlayPause();
  WaitForTitle(u"pause");
  ASSERT_EQ(overlay_window()->playback_state(),
            OverlayWindow::PlaybackState::kPaused);

  // Simulate resuming playback by invoking the Media Session "play" action
  // through interaction with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "addPlayEventListener();"));
  window_controller()->TogglePlayPause();
  WaitForTitle(u"play");
  ASSERT_EQ(overlay_window()->playback_state(),
            OverlayWindow::PlaybackState::kPlaying);
}

// Tests Media Session action availability upon reaching the end of stream by
// verifying that the "nexttrack" action can be invoked after playing through
// to the end of media.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureContentBrowserTest,
                       ActionAvailableAfterEndOfStreamAndSrcUpdate) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionNextTrackActionHandler();"));
  ASSERT_TRUE(ExecJs(shell(), "addPictureInPictureEventListeners();"));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  // Check twice, because the action handler updates the 'src' attribute and we
  // want to make sure this doesn't break the action handling.
  for (int i = 0; i < 2; ++i) {
    // Play through to the end of the media resource.
    ASSERT_EQ(true, EvalJs(shell(), "playToEnd();"));
    WaitForPlaybackState(OverlayWindow::PlaybackState::kEndOfVideo);

    // Simulate the user clicking the "next track" button and verify the
    // associated Media Session action that we set earlier is invoked.
    window_controller()->NextTrack();
    WaitForPlaybackState(OverlayWindow::PlaybackState::kPlaying);
  }

  // After closing and reopening the PiP window the Media Session action should
  // still work.
  ASSERT_EQ(true, EvalJs(shell(), "playToEnd();"));
  WaitForPlaybackState(OverlayWindow::PlaybackState::kEndOfVideo);

  window_controller()->Close(/*should_pause_video=*/false);
  WaitForTitle(u"leavepictureinpicture");

  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  window_controller()->NextTrack();
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPlaying);
}

// When the player object associated with a video element is destroyed, any
// Media Session actions that were set are no longer available.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureContentBrowserTest,
                       ResettingPlayerDisablesActions) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionPlayActionHandler();"));
  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionPauseActionHandler();"));
  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionNextTrackActionHandler();"));

  ASSERT_EQ(true, EvalJs(shell(), "resetVideo();"));

  // Media Session actions are unavailable with the player removed.
  EXPECT_EQ(overlay_window()->play_pause_button_visible().value_or(true),
            false);
  EXPECT_EQ(overlay_window()->next_track_button_visible().value_or(true),
            false);

  // Load new media on the video element. This creates a new player.
  ASSERT_EQ(true, EvalJs(shell(), "updateVideoSrcAndPlay();"));

  // The play/pause/replay and next buttons should be functional again.
  EXPECT_EQ(overlay_window()->play_pause_button_visible().value_or(false),
            true);
  window_controller()->TogglePlayPause();
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPaused);

  EXPECT_EQ(overlay_window()->next_track_button_visible().value_or(false),
            true);
  window_controller()->NextTrack();
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPlaying);
}

class AutoPictureInPictureContentBrowserTest
    : public PictureInPictureContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures,
        "PictureInPictureAPI,AutoPictureInPicture");
  }
};

// Show/hide fullscreen page and check that Auto Picture-in-Picture is
// triggered.
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureContentBrowserTest,
                       AutoPictureInPictureTriggeredWhenFullscreen) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_EQ(true, EvalJs(shell(), "enterFullscreen();"));

  ASSERT_TRUE(ExecJs(shell(), "video.autoPictureInPicture = true;"));
  ASSERT_TRUE(ExecJs(shell(), "addPictureInPictureEventListeners();"));
  ASSERT_EQ(true, EvalJs(shell(), "play();"));

  // Hide page and check that video entered Picture-in-Picture automatically.
  shell()->web_contents()->WasHidden();
  WaitForTitle(u"enterpictureinpicture");

  // Show page and check that video left Picture-in-Picture automatically.
  shell()->web_contents()->WasShown();
  WaitForTitle(u"leavepictureinpicture");
}

}  // namespace content
