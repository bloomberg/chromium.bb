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

void MediaController::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->Stop(mojom::MediaSession::SuspendType::kUI);
}

void MediaController::ToggleSuspendResume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_info_.is_null())
    return;

  switch (session_info_->playback_state) {
    case mojom::MediaPlaybackState::kPlaying:
      Suspend();
      break;
    case mojom::MediaPlaybackState::kPaused:
      Resume();
      break;
  }
}

void MediaController::AddObserver(mojom::MediaControllerObserverPtr observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Flush the new observer with the current state.
  observer->MediaSessionInfoChanged(session_info_.Clone());
  observer->MediaSessionMetadataChanged(session_metadata_);
  observer->MediaSessionActionsChanged(session_actions_);

  observers_.AddPtr(std::move(observer));
}

void MediaController::MediaSessionInfoChanged(mojom::MediaSessionInfoPtr info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.ForAllPtrs([&info](mojom::MediaControllerObserver* observer) {
    observer->MediaSessionInfoChanged(info.Clone());
  });

  session_info_ = std::move(info);
}

void MediaController::MediaSessionMetadataChanged(
    const base::Optional<MediaMetadata>& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.ForAllPtrs([&metadata](mojom::MediaControllerObserver* observer) {
    observer->MediaSessionMetadataChanged(metadata);
  });

  session_metadata_ = metadata;
}

void MediaController::MediaSessionActionsChanged(
    const std::vector<mojom::MediaSessionAction>& actions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.ForAllPtrs([&actions](mojom::MediaControllerObserver* observer) {
    observer->MediaSessionActionsChanged(actions);
  });

  session_actions_ = actions;
}

void MediaController::PreviousTrack() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->PreviousTrack();
}

void MediaController::NextTrack() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->NextTrack();
}

void MediaController::Seek(base::TimeDelta seek_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->Seek(seek_time);
}

bool MediaController::SetMediaSession(mojom::MediaSession* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool changed = session != session_;

  if (changed) {
    session_binding_.Close();
    session_info_.reset();
    session_metadata_.reset();
    session_actions_.clear();

    if (session) {
      // Add |this| as an observer for |session|.
      mojom::MediaSessionObserverPtr observer;
      session_binding_.Bind(mojo::MakeRequest(&observer));
      session->AddObserver(std::move(observer));
    } else {
      // If we are no longer bound to a session we should flush the observers
      // with empty data.
      observers_.ForAllPtrs([](mojom::MediaControllerObserver* observer) {
        observer->MediaSessionInfoChanged(nullptr);
        observer->MediaSessionMetadataChanged(base::nullopt);
        observer->MediaSessionActionsChanged(
            std::vector<mojom::MediaSessionAction>());
      });
    }
  }

  session_ = session;
  return changed;
}

void MediaController::BindToInterface(mojom::MediaControllerRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bindings_.AddBinding(this, std::move(request));
}

void MediaController::FlushForTesting() {
  bindings_.FlushForTesting();
}

}  // namespace media_session
