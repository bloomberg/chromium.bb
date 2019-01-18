// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/test/mock_media_session.h"

#include <utility>

#include "base/stl_util.h"

namespace media_session {
namespace test {

namespace {

bool IsMetadataNonEmpty(const base::Optional<MediaMetadata>& metadata) {
  if (!metadata.has_value())
    return false;

  return !metadata->title.empty() || !metadata->artist.empty() ||
         !metadata->album.empty() || !metadata->source_title.empty() ||
         !metadata->artwork.empty();
}

}  // namespace

MockMediaSessionMojoObserver::MockMediaSessionMojoObserver(
    mojom::MediaSession& media_session)
    : binding_(this) {
  mojom::MediaSessionObserverPtr observer;
  binding_.Bind(mojo::MakeRequest(&observer));
  media_session.AddObserver(std::move(observer));
}

MockMediaSessionMojoObserver::MockMediaSessionMojoObserver(
    mojom::MediaControllerPtr& controller)
    : binding_(this) {
  mojom::MediaSessionObserverPtr observer;
  binding_.Bind(mojo::MakeRequest(&observer));
  controller->AddObserver(std::move(observer));
}

MockMediaSessionMojoObserver::~MockMediaSessionMojoObserver() = default;

void MockMediaSessionMojoObserver::MediaSessionInfoChanged(
    mojom::MediaSessionInfoPtr session) {
  session_info_ = std::move(session);

  if (wanted_state_ == session_info_->state ||
      session_info_->playback_state == wanted_playback_state_) {
    run_loop_->Quit();
  }
}

void MockMediaSessionMojoObserver::MediaSessionMetadataChanged(
    const base::Optional<MediaMetadata>& metadata) {
  session_metadata_ = metadata;

  if (waiting_for_metadata_) {
    run_loop_->Quit();
    waiting_for_metadata_ = false;
  } else if (waiting_for_non_empty_metadata_ && IsMetadataNonEmpty(metadata)) {
    run_loop_->Quit();
    waiting_for_non_empty_metadata_ = false;
  }
}

void MockMediaSessionMojoObserver::MediaSessionActionsChanged(
    const std::vector<mojom::MediaSessionAction>& actions) {
  session_actions_ = actions;
  session_actions_set_ =
      std::set<mojom::MediaSessionAction>(actions.begin(), actions.end());

  if (waiting_for_actions_) {
    run_loop_->Quit();
    waiting_for_actions_ = false;
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

const base::Optional<MediaMetadata>&
MockMediaSessionMojoObserver::WaitForMetadata() {
  if (!session_metadata_.has_value()) {
    waiting_for_metadata_ = true;
    StartWaiting();
  }

  return session_metadata_.value();
}

const MediaMetadata& MockMediaSessionMojoObserver::WaitForNonEmptyMetadata() {
  if (!session_metadata_.has_value() || !session_metadata_->has_value()) {
    waiting_for_non_empty_metadata_ = true;
    StartWaiting();
  }

  return session_metadata_->value();
}

void MockMediaSessionMojoObserver::WaitForActions() {
  waiting_for_actions_ = true;
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
