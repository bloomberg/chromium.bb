// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/test/mock_media_session.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"

namespace media_session {
namespace test {

MockMediaSessionMojoObserver::MockMediaSessionMojoObserver(
    mojom::MediaSession& media_session)
    : binding_(this) {
  mojom::MediaSessionObserverPtr observer;
  binding_.Bind(mojo::MakeRequest(&observer));
  media_session.AddObserver(std::move(observer));
}

MockMediaSessionMojoObserver::~MockMediaSessionMojoObserver() = default;

void MockMediaSessionMojoObserver::MediaSessionInfoChanged(
    mojom::MediaSessionInfoPtr session) {
  session_info_ = std::move(session);

  if (expected_controllable_.has_value() &&
      expected_controllable_ == session_info_->is_controllable) {
    run_loop_->Quit();
    expected_controllable_.reset();
  } else if (wanted_state_ == session_info_->state ||
             session_info_->playback_state == wanted_playback_state_) {
    run_loop_->Quit();
  }
}

void MockMediaSessionMojoObserver::MediaSessionMetadataChanged(
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

void MockMediaSessionMojoObserver::MediaSessionActionsChanged(
    const std::vector<mojom::MediaSessionAction>& actions) {
  session_actions_ =
      std::set<mojom::MediaSessionAction>(actions.begin(), actions.end());

  if (expected_actions_.has_value() && expected_actions_ == session_actions_) {
    run_loop_->Quit();
    expected_actions_.reset();
  }
}

void MockMediaSessionMojoObserver::MediaSessionImagesChanged(
    const base::flat_map<mojom::MediaSessionImageType, std::vector<MediaImage>>&
        images) {
  session_images_ = images;

  if (expected_images_of_type_.has_value()) {
    auto type = expected_images_of_type_->first;
    auto images = expected_images_of_type_->second;
    auto it = session_images_->find(type);

    if (it != session_images_->end() && it->second == images) {
      run_loop_->Quit();
      expected_images_of_type_.reset();
    }
  }
}

void MockMediaSessionMojoObserver::WaitForState(
    mojom::MediaSessionInfo::SessionState wanted_state) {
  if (session_info_ && session_info_->state == wanted_state)
    return;

  wanted_state_ = wanted_state;
  StartWaiting();
}

void MockMediaSessionMojoObserver::WaitForPlaybackState(
    mojom::MediaPlaybackState wanted_state) {
  if (session_info_ && session_info_->playback_state == wanted_state)
    return;

  wanted_playback_state_ = wanted_state;
  StartWaiting();
}

void MockMediaSessionMojoObserver::WaitForControllable(bool is_controllable) {
  if (session_info_ && session_info_->is_controllable == is_controllable)
    return;

  expected_controllable_ = is_controllable;
  StartWaiting();
}

void MockMediaSessionMojoObserver::WaitForEmptyMetadata() {
  if (session_metadata_.has_value() || !session_metadata_->has_value())
    return;

  waiting_for_empty_metadata_ = true;
  StartWaiting();
}

void MockMediaSessionMojoObserver::WaitForExpectedMetadata(
    const MediaMetadata& metadata) {
  if (session_metadata_.has_value() && session_metadata_ == metadata)
    return;

  expected_metadata_ = metadata;
  StartWaiting();
}

void MockMediaSessionMojoObserver::WaitForEmptyActions() {
  WaitForExpectedActions(std::set<mojom::MediaSessionAction>());
}

void MockMediaSessionMojoObserver::WaitForExpectedActions(
    const std::set<mojom::MediaSessionAction>& actions) {
  if (session_actions_.has_value() && session_actions_ == actions)
    return;

  expected_actions_ = actions;
  StartWaiting();
}

void MockMediaSessionMojoObserver::WaitForExpectedImagesOfType(
    mojom::MediaSessionImageType type,
    const std::vector<MediaImage>& images) {
  if (session_images_.has_value()) {
    auto it = session_images_->find(type);
    if (it != session_images_->end() && it->second == images)
      return;
  }

  expected_images_of_type_ = std::make_pair(type, images);
  StartWaiting();
}

void MockMediaSessionMojoObserver::StartWaiting() {
  DCHECK(!run_loop_);

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

MockMediaSession::MockMediaSession() = default;

MockMediaSession::MockMediaSession(bool force_duck) : force_duck_(force_duck) {}

MockMediaSession::~MockMediaSession() {}

void MockMediaSession::Suspend(SuspendType suspend_type) {
  SetState(mojom::MediaSessionInfo::SessionState::kSuspended);
}

void MockMediaSession::Resume(SuspendType suspend_type) {
  SetState(mojom::MediaSessionInfo::SessionState::kActive);
}

void MockMediaSession::StartDucking() {
  is_ducking_ = true;
  NotifyObservers();
}

void MockMediaSession::StopDucking() {
  is_ducking_ = false;
  NotifyObservers();
}

void MockMediaSession::GetMediaSessionInfo(
    GetMediaSessionInfoCallback callback) {
  std::move(callback).Run(GetMediaSessionInfoSync());
}

void MockMediaSession::AddObserver(mojom::MediaSessionObserverPtr observer) {
  ++add_observer_count_;

  observer->MediaSessionInfoChanged(GetMediaSessionInfoSync());

  std::vector<mojom::MediaSessionAction> actions(actions_.begin(),
                                                 actions_.end());
  observer->MediaSessionActionsChanged(actions);
  observer->MediaSessionImagesChanged(images_);

  observers_.AddPtr(std::move(observer));
}

void MockMediaSession::GetDebugInfo(GetDebugInfoCallback callback) {
  mojom::MediaSessionDebugInfoPtr debug_info(
      mojom::MediaSessionDebugInfo::New());

  debug_info->name = "name";
  debug_info->owner = "owner";
  debug_info->state = "state";

  std::move(callback).Run(std::move(debug_info));
}

void MockMediaSession::PreviousTrack() {
  prev_track_count_++;
}

void MockMediaSession::NextTrack() {
  next_track_count_++;
}

void MockMediaSession::Seek(base::TimeDelta seek_time) {
  seek_count_++;
}

void MockMediaSession::Stop(SuspendType type) {
  SetState(mojom::MediaSessionInfo::SessionState::kInactive);
}

void MockMediaSession::GetMediaImageBitmap(
    const MediaImage& image,
    int minimum_size_px,
    int desired_size_px,
    GetMediaImageBitmapCallback callback) {
  last_image_src_ = image.src;

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::Make(10, 10, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));

  std::move(callback).Run(bitmap);
}

void MockMediaSession::SetIsControllable(bool value) {
  is_controllable_ = value;
  NotifyObservers();
}

void MockMediaSession::AbandonAudioFocusFromClient() {
  DCHECK(afr_client_.is_bound());
  afr_client_->AbandonAudioFocus();
  afr_client_.FlushForTesting();
  afr_client_.reset();
}

base::UnguessableToken MockMediaSession::GetRequestIdFromClient() {
  DCHECK(afr_client_.is_bound());
  base::UnguessableToken id = base::UnguessableToken::Null();

  afr_client_->GetRequestId(base::BindOnce(
      [](base::UnguessableToken* id,
         const base::UnguessableToken& received_id) { *id = received_id; },
      &id));

  afr_client_.FlushForTesting();
  DCHECK_NE(base::UnguessableToken::Null(), id);
  return id;
}

base::UnguessableToken MockMediaSession::RequestAudioFocusFromService(
    mojom::AudioFocusManagerPtr& service,
    mojom::AudioFocusType audio_focus_type) {
  bool result;
  base::OnceClosure callback =
      base::BindOnce([](bool* out_result) { *out_result = true; }, &result);

  if (afr_client_.is_bound()) {
    // Request audio focus through the existing request.
    afr_client_->RequestAudioFocus(GetMediaSessionInfoSync(), audio_focus_type,
                                   std::move(callback));

    afr_client_.FlushForTesting();
  } else {
    // Build a new audio focus request.
    mojom::MediaSessionPtr media_session;
    bindings_.AddBinding(this, mojo::MakeRequest(&media_session));

    service->RequestAudioFocus(
        mojo::MakeRequest(&afr_client_), std::move(media_session),
        GetMediaSessionInfoSync(), audio_focus_type, std::move(callback));

    service.FlushForTesting();
  }

  // If the audio focus was granted then we should set the session state to
  // active.
  if (result)
    SetState(mojom::MediaSessionInfo::SessionState::kActive);

  return GetRequestIdFromClient();
}

base::UnguessableToken MockMediaSession::RequestGroupedAudioFocusFromService(
    mojom::AudioFocusManagerPtr& service,
    mojom::AudioFocusType audio_focus_type,
    const base::UnguessableToken& group_id) {
  bool result;
  base::OnceClosure callback =
      base::BindOnce([](bool* out_result) { *out_result = true; }, &result);

  if (afr_client_.is_bound()) {
    // Request audio focus through the existing request.
    afr_client_->RequestAudioFocus(GetMediaSessionInfoSync(), audio_focus_type,
                                   std::move(callback));

    afr_client_.FlushForTesting();
  } else {
    // Build a new audio focus request.
    mojom::MediaSessionPtr media_session;
    bindings_.AddBinding(this, mojo::MakeRequest(&media_session));

    service->RequestGroupedAudioFocus(
        mojo::MakeRequest(&afr_client_), std::move(media_session),
        GetMediaSessionInfoSync(), audio_focus_type, group_id,
        std::move(callback));

    service.FlushForTesting();
  }

  // If the audio focus was granted then we should set the session state to
  // active.
  if (result)
    SetState(mojom::MediaSessionInfo::SessionState::kActive);

  return GetRequestIdFromClient();
}

mojom::MediaSessionInfo::SessionState MockMediaSession::GetState() const {
  return GetMediaSessionInfoSync()->state;
}

void MockMediaSession::FlushForTesting() {
  afr_client_.FlushForTesting();
}

void MockMediaSession::SimulateMetadataChanged(
    const base::Optional<MediaMetadata>& metadata) {
  observers_.ForAllPtrs([&metadata](mojom::MediaSessionObserver* observer) {
    observer->MediaSessionMetadataChanged(metadata);
  });
}

void MockMediaSession::ClearAllImages() {
  images_.clear();

  observers_.ForAllPtrs([this](mojom::MediaSessionObserver* observer) {
    observer->MediaSessionImagesChanged(this->images_);
  });
}

void MockMediaSession::SetImagesOfType(mojom::MediaSessionImageType type,
                                       const std::vector<MediaImage>& images) {
  images_.insert_or_assign(type, images);

  observers_.ForAllPtrs([this](mojom::MediaSessionObserver* observer) {
    observer->MediaSessionImagesChanged(this->images_);
  });
}

void MockMediaSession::EnableAction(mojom::MediaSessionAction action) {
  if (base::ContainsKey(actions_, action))
    return;

  actions_.insert(action);
  NotifyActionObservers();
}

void MockMediaSession::DisableAction(mojom::MediaSessionAction action) {
  if (!base::ContainsKey(actions_, action))
    return;

  actions_.erase(action);
  NotifyActionObservers();
}

void MockMediaSession::SetState(mojom::MediaSessionInfo::SessionState state) {
  state_ = state;
  NotifyObservers();
}

void MockMediaSession::NotifyObservers() {
  mojom::MediaSessionInfoPtr session_info = GetMediaSessionInfoSync();

  if (afr_client_.is_bound())
    afr_client_->MediaSessionInfoChanged(session_info.Clone());

  observers_.ForAllPtrs([&session_info](mojom::MediaSessionObserver* observer) {
    observer->MediaSessionInfoChanged(session_info.Clone());
  });
}

mojom::MediaSessionInfoPtr MockMediaSession::GetMediaSessionInfoSync() const {
  mojom::MediaSessionInfoPtr info(mojom::MediaSessionInfo::New());
  info->force_duck = force_duck_;
  info->state = state_;
  if (is_ducking_)
    info->state = mojom::MediaSessionInfo::SessionState::kDucking;

  info->playback_state = mojom::MediaPlaybackState::kPaused;
  if (state_ == mojom::MediaSessionInfo::SessionState::kActive)
    info->playback_state = mojom::MediaPlaybackState::kPlaying;

  info->is_controllable = is_controllable_;
  info->prefer_stop_for_gain_focus_loss = prefer_stop_;

  return info;
}

void MockMediaSession::NotifyActionObservers() {
  std::vector<mojom::MediaSessionAction> actions(actions_.begin(),
                                                 actions_.end());

  observers_.ForAllPtrs([&actions](mojom::MediaSessionObserver* observer) {
    observer->MediaSessionActionsChanged(actions);
  });
}

}  // namespace test
}  // namespace media_session
