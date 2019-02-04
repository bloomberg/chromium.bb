// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/test/test_media_controller.h"

namespace media_session {
namespace test {

TestMediaControllerObserver::TestMediaControllerObserver(
    mojom::MediaControllerPtr& media_controller)
    : binding_(this) {
  mojom::MediaControllerObserverPtr observer;
  binding_.Bind(mojo::MakeRequest(&observer));
  media_controller->AddObserver(std::move(observer));
}

TestMediaControllerObserver::~TestMediaControllerObserver() = default;

void TestMediaControllerObserver::MediaSessionInfoChanged(
    mojom::MediaSessionInfoPtr session) {
  session_info_ = std::move(session);

  if (session_info_.has_value() && !session_info_->is_null()) {
    if (wanted_state_ == session_info()->state ||
        session_info()->playback_state == wanted_playback_state_) {
      run_loop_->Quit();
    }
  } else if (waiting_for_empty_info_) {
    waiting_for_empty_info_ = false;
    run_loop_->Quit();
  }
}

void TestMediaControllerObserver::MediaSessionMetadataChanged(
    const base::Optional<MediaMetadata>& metadata) {
  session_metadata_ = metadata;

  if (expected_metadata_.has_value() && expected_metadata_ == metadata) {
    run_loop_->Quit();
    expected_metadata_.reset();
  } else if (waiting_for_empty_metadata_ &&
             (!metadata.has_value() || metadata->IsEmpty())) {
    run_loop_->Quit();
    waiting_for_empty_metadata_ = false;
  }
}

void TestMediaControllerObserver::MediaSessionActionsChanged(
    const std::vector<mojom::MediaSessionAction>& actions) {
  session_actions_ =
      std::set<mojom::MediaSessionAction>(actions.begin(), actions.end());

  if (expected_actions_.has_value() && expected_actions_ == session_actions_) {
    run_loop_->Quit();
    expected_actions_.reset();
  }
}

void TestMediaControllerObserver::WaitForState(
    mojom::MediaSessionInfo::SessionState wanted_state) {
  if (session_info_ && session_info()->state == wanted_state)
    return;

  wanted_state_ = wanted_state;
  StartWaiting();
}

void TestMediaControllerObserver::WaitForPlaybackState(
    mojom::MediaPlaybackState wanted_state) {
  if (session_info_ && session_info()->playback_state == wanted_state)
    return;

  wanted_playback_state_ = wanted_state;
  StartWaiting();
}

void TestMediaControllerObserver::WaitForEmptyInfo() {
  if (session_info_.has_value() && session_info_->is_null())
    return;

  waiting_for_empty_info_ = true;
  StartWaiting();
}

void TestMediaControllerObserver::WaitForEmptyMetadata() {
  if (session_metadata_.has_value())
    return;

  waiting_for_empty_metadata_ = true;
  StartWaiting();
}

void TestMediaControllerObserver::WaitForExpectedMetadata(
    const MediaMetadata& metadata) {
  if (session_metadata_.has_value() && session_metadata_ == metadata)
    return;

  expected_metadata_ = metadata;
  StartWaiting();
}

void TestMediaControllerObserver::WaitForEmptyActions() {
  WaitForExpectedActions(std::set<mojom::MediaSessionAction>());
}

void TestMediaControllerObserver::WaitForExpectedActions(
    const std::set<mojom::MediaSessionAction>& actions) {
  if (session_actions_.has_value() && session_actions_ == actions)
    return;

  expected_actions_ = actions;
  StartWaiting();
}

void TestMediaControllerObserver::StartWaiting() {
  DCHECK(!run_loop_);

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

TestMediaController::TestMediaController() = default;

TestMediaController::~TestMediaController() = default;

mojom::MediaControllerPtr TestMediaController::CreateMediaControllerPtr() {
  mojom::MediaControllerPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void TestMediaController::Suspend() {
  ++suspend_count_;
}

void TestMediaController::Resume() {
  ++resume_count_;
}

void TestMediaController::ToggleSuspendResume() {
  ++toggle_suspend_resume_count_;
}

void TestMediaController::AddObserver(
    mojom::MediaControllerObserverPtr observer) {
  ++add_observer_count_;
  observers_.AddPtr(std::move(observer));
}

void TestMediaController::PreviousTrack() {
  ++previous_track_count_;
}

void TestMediaController::NextTrack() {
  ++next_track_count_;
}

void TestMediaController::Seek(base::TimeDelta seek_time) {
  DCHECK(!seek_time.is_zero());

  if (seek_time > base::TimeDelta()) {
    ++seek_forward_count_;
  } else if (seek_time < base::TimeDelta()) {
    ++seek_backward_count_;
  }
}

void TestMediaController::SimulateMediaSessionActionsChanged(
    const std::vector<mojom::MediaSessionAction>& actions) {
  observers_.ForAllPtrs([&actions](mojom::MediaControllerObserver* observer) {
    observer->MediaSessionActionsChanged(actions);
  });
}

void TestMediaController::Flush() {
  observers_.FlushForTesting();
}

}  // namespace test
}  // namespace media_session
