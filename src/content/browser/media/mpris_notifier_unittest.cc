// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/mpris_notifier.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mpris/mock_mpris_service.h"

namespace content {

using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;
using testing::Expectation;

class MprisNotifierTest : public testing::Test {
 public:
  MprisNotifierTest() = default;
  ~MprisNotifierTest() override = default;

  void SetUp() override {
    notifier_ = std::make_unique<MprisNotifier>(/*connector=*/nullptr);
    notifier_->SetMprisServiceForTesting(&mock_mpris_service_);
    notifier_->Initialize();
  }

 protected:
  void SimulatePlaying() {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->playback_state = MediaPlaybackState::kPlaying;
    notifier_->MediaSessionInfoChanged(std::move(session_info));
  }

  void SimulatePaused() {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->playback_state = MediaPlaybackState::kPaused;
    notifier_->MediaSessionInfoChanged(std::move(session_info));
  }

  void SimulateStopped() { notifier_->MediaSessionInfoChanged(nullptr); }

  MprisNotifier& notifier() { return *notifier_; }
  mpris::MockMprisService& mock_mpris_service() { return mock_mpris_service_; }

 private:
  std::unique_ptr<MprisNotifier> notifier_;
  mpris::MockMprisService mock_mpris_service_;

  DISALLOW_COPY_AND_ASSIGN(MprisNotifierTest);
};

TEST_F(MprisNotifierTest, ProperlyUpdatesPlaybackState) {
  Expectation playing = EXPECT_CALL(
      mock_mpris_service(),
      SetPlaybackStatus(mpris::MprisService::PlaybackStatus::kPlaying));
  Expectation paused =
      EXPECT_CALL(
          mock_mpris_service(),
          SetPlaybackStatus(mpris::MprisService::PlaybackStatus::kPaused))
          .After(playing);
  EXPECT_CALL(mock_mpris_service(),
              SetPlaybackStatus(mpris::MprisService::PlaybackStatus::kStopped))
      .After(paused);

  SimulatePlaying();
  SimulatePaused();
  SimulateStopped();
}

TEST_F(MprisNotifierTest, ProperlyUpdatesMetadata) {
  EXPECT_CALL(mock_mpris_service(), SetTitle(base::ASCIIToUTF16("Foo")));

  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("Foo");
  notifier().MediaSessionMetadataChanged(metadata);
}

}  // namespace content
