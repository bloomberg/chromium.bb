// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/media_controller.h"

namespace media_session {

MediaController::MediaController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

MediaController::~MediaController() = default;

void MediaController::Suspend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->Suspend(mojom::MediaSession::SuspendType::kUI);
}

void MediaController::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->Resume(mojom::MediaSession::SuspendType::kUI);
}

void MediaController::ToggleSuspendResume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (state_) {
    case mojom::MediaSessionInfo::SessionState::kInactive:
    case mojom::MediaSessionInfo::SessionState::kSuspended:
      Resume();
      break;
    case mojom::MediaSessionInfo::SessionState::kDucking:
    case mojom::MediaSessionInfo::SessionState::kActive:
      Suspend();
      break;
  }
}

void MediaController::SetMediaSession(
    mojom::MediaSession* session,
    mojom::MediaSessionInfo::SessionState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_ = session;
  state_ = state;
}

void MediaController::ClearMediaSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_ = nullptr;
  state_ = mojom::MediaSessionInfo::SessionState::kInactive;
}

void MediaController::BindToInterface(mojom::MediaControllerRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bindings_.AddBinding(this, std::move(request));
}

void MediaController::FlushForTesting() {
  bindings_.FlushForTesting();
}

}  // namespace media_session
