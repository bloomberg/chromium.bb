// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_MOCK_MEDIA_SESSION_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_MOCK_MEDIA_SESSION_H_

#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace media_session {
namespace test {

// A mock MediaSessionObsever that can be used for waiting for state changes.
class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP)
    MockMediaSessionMojoObserver : public mojom::MediaSessionObserver {
 public:
  explicit MockMediaSessionMojoObserver(mojom::MediaSession& media_session);

  ~MockMediaSessionMojoObserver() override;

  // mojom::MediaSessionObserver overrides.
  void MediaSessionInfoChanged(mojom::MediaSessionInfoPtr session) override;
  void MediaSessionMetadataChanged(
      const base::Optional<MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<mojom::MediaSessionAction>& actions) override;
  void MediaSessionImagesChanged(
      const base::flat_map<mojom::MediaSessionImageType,
                           std::vector<MediaImage>>& images) override;

  void WaitForState(mojom::MediaSessionInfo::SessionState wanted_state);
  void WaitForPlaybackState(mojom::MediaPlaybackState wanted_state);
  void WaitForControllable(bool is_controllable);

  void WaitForEmptyMetadata();
  void WaitForExpectedMetadata(const MediaMetadata& metadata);

  void WaitForEmptyActions();
  void WaitForExpectedActions(
      const std::set<mojom::MediaSessionAction>& actions);

  void WaitForExpectedImagesOfType(mojom::MediaSessionImageType type,
                                   const std::vector<MediaImage>& images);

  const mojom::MediaSessionInfoPtr& session_info() const {
    return session_info_;
  }

  const base::Optional<base::Optional<MediaMetadata>>& session_metadata()
      const {
    return session_metadata_;
  }

  const std::set<mojom::MediaSessionAction>& actions() const {
    return *session_actions_;
  }

 private:
  void StartWaiting();

  mojom::MediaSessionInfoPtr session_info_;
  base::Optional<base::Optional<MediaMetadata>> session_metadata_;
  base::Optional<std::set<mojom::MediaSessionAction>> session_actions_;
  base::Optional<
      base::flat_map<mojom::MediaSessionImageType, std::vector<MediaImage>>>
      session_images_;

  base::Optional<MediaMetadata> expected_metadata_;
  base::Optional<std::set<mojom::MediaSessionAction>> expected_actions_;
  base::Optional<bool> expected_controllable_;
  base::Optional<
      std::pair<mojom::MediaSessionImageType, std::vector<MediaImage>>>
      expected_images_of_type_;
  bool waiting_for_empty_metadata_ = false;

  base::Optional<mojom::MediaSessionInfo::SessionState> wanted_state_;
  base::Optional<mojom::MediaPlaybackState> wanted_playback_state_;
  std::unique_ptr<base::RunLoop> run_loop_;

  mojo::Binding<mojom::MediaSessionObserver> binding_;
};

// A mock MediaSession that can be used for interacting with the Media Session
// service during tests.
class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP) MockMediaSession
    : public mojom::MediaSession {
 public:
  MockMediaSession();
  explicit MockMediaSession(bool force_duck);

  ~MockMediaSession() override;

  // mojom::MediaSession overrides.
  void Suspend(SuspendType type) override;
  void Resume(SuspendType type) override;
  void StartDucking() override;
  void StopDucking() override;
  void GetMediaSessionInfo(GetMediaSessionInfoCallback callback) override;
  void AddObserver(mojom::MediaSessionObserverPtr observer) override;
  void GetDebugInfo(GetDebugInfoCallback callback) override;
  void PreviousTrack() override;
  void NextTrack() override;
  void SkipAd() override {}
  void Seek(base::TimeDelta seek_time) override;
  void Stop(SuspendType type) override;
  void GetMediaImageBitmap(const MediaImage& image,
                           int minimum_size_px,
                           int desired_size_px,
                           GetMediaImageBitmapCallback callback) override;

  void SetIsControllable(bool value);
  void SetPreferStop(bool value) { prefer_stop_ = value; }

  void AbandonAudioFocusFromClient();
  base::UnguessableToken GetRequestIdFromClient();

  base::UnguessableToken RequestAudioFocusFromService(
      mojom::AudioFocusManagerPtr& service,
      mojom::AudioFocusType audio_foucs_type);

  base::UnguessableToken RequestGroupedAudioFocusFromService(
      mojom::AudioFocusManagerPtr& service,
      mojom::AudioFocusType audio_focus_type,
      const base::UnguessableToken& group_id);

  mojom::MediaSessionInfo::SessionState GetState() const;

  mojom::AudioFocusRequestClient* audio_focus_request() const {
    return afr_client_.get();
  }
  void FlushForTesting();

  void SimulateMetadataChanged(const base::Optional<MediaMetadata>& metadata);

  void ClearAllImages();
  void SetImagesOfType(mojom::MediaSessionImageType type,
                       const std::vector<MediaImage>& images);

  void EnableAction(mojom::MediaSessionAction action);
  void DisableAction(mojom::MediaSessionAction action);

  int prev_track_count() const { return prev_track_count_; }
  int next_track_count() const { return next_track_count_; }
  int add_observer_count() const { return add_observer_count_; }
  int seek_count() const { return seek_count_; }

  const GURL& last_image_src() const { return last_image_src_; }

 private:
  void SetState(mojom::MediaSessionInfo::SessionState);
  void NotifyObservers();
  mojom::MediaSessionInfoPtr GetMediaSessionInfoSync() const;
  void NotifyActionObservers();

  mojom::AudioFocusRequestClientPtr afr_client_;

  const bool force_duck_ = false;
  bool is_ducking_ = false;
  bool is_controllable_ = false;
  bool prefer_stop_ = false;

  int prev_track_count_ = 0;
  int next_track_count_ = 0;
  int add_observer_count_ = 0;
  int seek_count_ = 0;

  std::set<mojom::MediaSessionAction> actions_;

  mojom::MediaSessionInfo::SessionState state_ =
      mojom::MediaSessionInfo::SessionState::kInactive;

  base::flat_map<mojom::MediaSessionImageType, std::vector<MediaImage>> images_;
  GURL last_image_src_;

  mojo::BindingSet<mojom::MediaSession> bindings_;

  mojo::InterfacePtrSet<mojom::MediaSessionObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaSession);
};

}  // namespace test
}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_MOCK_MEDIA_SESSION_H_
