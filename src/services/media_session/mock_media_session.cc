// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/mock_media_session.h"

#include <utility>

#include "services/media_session/public/cpp/switches.h"

namespace media_session {
namespace test {

MockMediaSession::MockMediaSession() = default;

MockMediaSession::MockMediaSession(bool force_duck) : force_duck_(force_duck) {}

MockMediaSession::~MockMediaSession() {}

void MockMediaSession::Suspend(SuspendType suspend_type) {
  DCHECK_EQ(SuspendType::kSystem, suspend_type);
  SetState(mojom::MediaSessionInfo::SessionState::kSuspended);
}

void MockMediaSession::Resume(SuspendType suspend_type) {
  DCHECK_EQ(SuspendType::kSystem, suspend_type);
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

void MockMediaSession::AddObserver(mojom::MediaSessionObserverPtr observer) {}

void MockMediaSession::GetDebugInfo(GetDebugInfoCallback callback) {
  mojom::MediaSessionDebugInfoPtr debug_info(
      mojom::MediaSessionDebugInfo::New());

  debug_info->name = "name";
  debug_info->owner = "owner";
  debug_info->state = "state";

  std::move(callback).Run(std::move(debug_info));
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

mojom::MediaSessionInfo::SessionState MockMediaSession::GetState() const {
  return GetMediaSessionInfoSync()->state;
}

void MockMediaSession::FlushForTesting() {
  afr_client_.FlushForTesting();
}

void MockMediaSession::SetState(mojom::MediaSessionInfo::SessionState state) {
  state_ = state;
  NotifyObservers();
}

void MockMediaSession::NotifyObservers() {
  if (afr_client_.is_bound())
    afr_client_->MediaSessionInfoChanged(GetMediaSessionInfoSync());
}

mojom::MediaSessionInfoPtr MockMediaSession::GetMediaSessionInfoSync() const {
  mojom::MediaSessionInfoPtr info(mojom::MediaSessionInfo::New());
  info->force_duck = force_duck_;
  info->state = state_;
  if (is_ducking_)
    info->state = mojom::MediaSessionInfo::SessionState::kDucking;
  return info;
}

}  // namespace test
}  // namespace media_session
