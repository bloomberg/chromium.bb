// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_controller.h"

#include <memory>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"

namespace ash {

class MediaControllerTest : public AshTestBase {
 public:
  MediaControllerTest() = default;
  ~MediaControllerTest() override = default;

  // AshTestBase
  void SetUp() override {
    AshTestBase::SetUp();

    controller_ = std::make_unique<media_session::test::TestMediaController>();

    MediaController* media_controller = Shell::Get()->media_controller();
    media_controller->SetMediaSessionControllerForTest(
        controller_->CreateMediaControllerPtr());
    media_controller->FlushForTesting();

    {
      std::vector<media_session::mojom::MediaSessionAction> actions;
      actions.push_back(media_session::mojom::MediaSessionAction::kPlay);
      controller_->SimulateMediaSessionActionsChanged(actions);
    }

    {
      media_session::mojom::MediaSessionInfoPtr session_info(
        media_session::mojom::MediaSessionInfo::New());

      session_info->state =
          media_session::mojom::MediaSessionInfo::SessionState::kActive;
      session_info->playback_state =
          media_session::mojom::MediaPlaybackState::kPlaying;
      controller_->SimulateMediaSessionInfoChanged(std::move(session_info));
    }

    Flush();
  }

  media_session::test::TestMediaController* controller() const {
    return controller_.get();
  }

  void SimulateSessionLock() {
    SessionInfo info;
    info.state = session_manager::SessionState::LOCKED;
    Shell::Get()->session_controller()->SetSessionInfo(info);
  }

  void Flush() {
    controller_->Flush();
    Shell::Get()->media_controller()->FlushForTesting();
  }

 private:
  std::unique_ptr<media_session::test::TestMediaController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaControllerTest);
};

TEST_F(MediaControllerTest, DisablePlayPauseWhenLocked) {
  EXPECT_EQ(0, controller()->suspend_count());

  Shell::Get()->media_controller()->HandleMediaPlayPause();
  Flush();

  EXPECT_EQ(1, controller()->suspend_count());

  SimulateSessionLock();

  Shell::Get()->media_controller()->HandleMediaPlayPause();
  Flush();

  EXPECT_EQ(1, controller()->suspend_count());
}

TEST_F(MediaControllerTest, DisablePreviousTrackWhenLocked) {
  EXPECT_EQ(0, controller()->previous_track_count());

  Shell::Get()->media_controller()->HandleMediaPrevTrack();
  Flush();

  EXPECT_EQ(1, controller()->previous_track_count());

  SimulateSessionLock();

  Shell::Get()->media_controller()->HandleMediaPrevTrack();
  Flush();

  EXPECT_EQ(1, controller()->previous_track_count());
}

TEST_F(MediaControllerTest, DisableNextTrackWhenLocked) {
  EXPECT_EQ(0, controller()->next_track_count());

  Shell::Get()->media_controller()->HandleMediaNextTrack();
  Flush();

  EXPECT_EQ(1, controller()->next_track_count());

  SimulateSessionLock();

  Shell::Get()->media_controller()->HandleMediaNextTrack();
  Flush();

  EXPECT_EQ(1, controller()->next_track_count());
}

}  // namespace ash
