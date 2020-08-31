// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_
#define CHROMEOS_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "libassistant/shared/public/media_manager.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace assistant_client {
struct MediaStatus;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {

class AssistantManagerService;

// MediaSession manages the media session and audio focus for Assistant.
// MediaSession allows clients to observe its changes via MediaSessionObserver,
// and allows clients to resume/suspend/stop the managed players.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) AssistantMediaSession
    : public media_session::mojom::MediaSession {
 public:
  explicit AssistantMediaSession(
      AssistantManagerService* assistant_manager_service);
  ~AssistantMediaSession() override;

  // media_session.mojom.MediaSession overrides:
  void Suspend(SuspendType suspend_type) override;
  void Resume(SuspendType suspend_type) override;
  void StartDucking() override;
  void StopDucking() override;
  void GetMediaSessionInfo(GetMediaSessionInfoCallback callback) override;
  void GetDebugInfo(GetDebugInfoCallback callback) override;
  void AddObserver(
      mojo::PendingRemote<media_session::mojom::MediaSessionObserver> observer)
      override;
  void PreviousTrack() override {}
  void NextTrack() override {}
  void SkipAd() override {}
  void Seek(base::TimeDelta seek_time) override {}
  void Stop(SuspendType suspend_type) override {}
  void GetMediaImageBitmap(const media_session::MediaImage& image,
                           int minimum_size_px,
                           int desired_size_px,
                           GetMediaImageBitmapCallback callback) override {}
  void SeekTo(base::TimeDelta seek_time) override {}
  void ScrubTo(base::TimeDelta seek_time) override {}
  void EnterPictureInPicture() override {}
  void ExitPictureInPicture() override {}

  // Requests/abandons audio focus to the AudioFocusManager.
  void RequestAudioFocus(media_session::mojom::AudioFocusType audio_focus_type);
  void AbandonAudioFocusIfNeeded();

  void NotifyMediaSessionMetadataChanged(
      const assistant_client::MediaStatus& status);

  base::WeakPtr<AssistantMediaSession> GetWeakPtr();

  // Returns if the session is currently active.
  bool IsSessionStateActive() const;
  // Returns if the session is currently ducking.
  bool IsSessionStateDucking() const;
  // Returns if the session is currently suspended.
  bool IsSessionStateSuspended() const;
  // Returns if the session is currently inactive.
  bool IsSessionStateInactive() const;

  // Returns internal audio focus id.
  base::UnguessableToken internal_audio_focus_id() {
    return internal_audio_focus_id_;
  }

 private:
  // Ensures that |audio_focus_ptr_| is connected.
  void EnsureServiceConnection();

  // Called by AudioFocusManager when an async audio focus request is completed.
  void FinishAudioFocusRequest(media_session::mojom::AudioFocusType type);
  void FinishInitialAudioFocusRequest(media_session::mojom::AudioFocusType type,
                                      const base::UnguessableToken& request_id);

  // Sets |session_info_|, |audio_focus_type_| and notifies observers about
  // the state change.
  void SetAudioFocusInfo(
      media_session::mojom::MediaSessionInfo::SessionState audio_focus_state,
      media_session::mojom::AudioFocusType audio_focus_type);

  // Notifies mojo observers that the MediaSessionInfo has changed.
  void NotifyMediaSessionInfoChanged();

  // The current metadata associated with the current media session.
  media_session::MediaMetadata metadata_;

  AssistantManagerService* const assistant_manager_service_;

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  // Binding for Mojo pointer to |this| held by AudioFocusManager.
  mojo::Receiver<media_session::mojom::MediaSession> receiver_{this};

  assistant_client::TrackType current_track_;

  mojo::RemoteSet<media_session::mojom::MediaSessionObserver> observers_;

  // Holds a pointer to the MediaSessionService.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_manager_;

  // If the media session has acquired audio focus then this will contain a
  // pointer to that requests AudioFocusRequestClient.
  mojo::Remote<media_session::mojom::AudioFocusRequestClient>
      audio_focus_request_client_;

  media_session::mojom::MediaSessionInfo session_info_;

  media_session::mojom::AudioFocusType audio_focus_type_;

  // Audio focus request Id for the internal media which is playing.
  base::UnguessableToken internal_audio_focus_id_ =
      base::UnguessableToken::Null();

  base::WeakPtrFactory<AssistantMediaSession> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantMediaSession);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_
