// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_MOCK_MEDIA_SESSION_H_
#define SERVICES_MEDIA_SESSION_MOCK_MEDIA_SESSION_H_

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace media_session {
namespace test {

// A mock MediaSession that can be used for interacting with the Media Session
// service during tests.
class MockMediaSession : public mojom::MediaSession {
 public:
  MockMediaSession();
  explicit MockMediaSession(bool force_duck);

  ~MockMediaSession() override;

  // mojom::MediaSession overrides.
  void Suspend(SuspendType) override;
  void Resume(SuspendType) override;
  void StartDucking() override;
  void StopDucking() override;
  void GetMediaSessionInfo(GetMediaSessionInfoCallback) override;
  void AddObserver(mojom::MediaSessionObserverPtr) override;
  void GetDebugInfo(GetDebugInfoCallback) override;

  void AbandonAudioFocusFromClient();
  base::UnguessableToken GetRequestIdFromClient();

  base::UnguessableToken RequestAudioFocusFromService(
      mojom::AudioFocusManagerPtr&,
      mojom::AudioFocusType);

  mojom::MediaSessionInfo::SessionState GetState() const;

  mojom::AudioFocusRequestClient* audio_focus_request() const {
    return afr_client_.get();
  }
  void FlushForTesting();

 private:
  void SetState(mojom::MediaSessionInfo::SessionState);
  void NotifyObservers();
  mojom::MediaSessionInfoPtr GetMediaSessionInfoSync() const;

  mojom::AudioFocusRequestClientPtr afr_client_;

  const bool force_duck_ = false;
  bool is_ducking_ = false;

  mojom::MediaSessionInfo::SessionState state_ =
      mojom::MediaSessionInfo::SessionState::kInactive;

  mojo::BindingSet<mojom::MediaSession> bindings_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaSession);
};

}  // namespace test
}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_MOCK_MEDIA_SESSION_H_
