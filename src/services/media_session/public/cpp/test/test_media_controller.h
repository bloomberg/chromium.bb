// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_TEST_MEDIA_CONTROLLER_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_TEST_MEDIA_CONTROLLER_H_

#include <utility>

#include "base/component_export.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace media_session {
namespace test {

// A mock MediaControllerImageObserver than can be used for waiting for images.
class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP)
    TestMediaControllerImageObserver
    : public mojom::MediaControllerImageObserver {
 public:
  TestMediaControllerImageObserver(mojom::MediaControllerPtr& controller,
                                   int minimum_size_px,
                                   int desired_size_px);
  ~TestMediaControllerImageObserver() override;

  // mojom::MediaControllerImageObserver overrides.
  void MediaControllerImageChanged(mojom::MediaSessionImageType type,
                                   const SkBitmap& bitmap) override;

  void WaitForExpectedImageOfType(mojom::MediaSessionImageType type,
                                  bool expect_null_value);

 private:
  // The bool is whether the image type should be a null value.
  using ImageTypePair = std::pair<mojom::MediaSessionImageType, bool>;

  std::unique_ptr<base::RunLoop> run_loop_;

  base::Optional<ImageTypePair> expected_;
  base::Optional<ImageTypePair> current_;

  mojo::Binding<mojom::MediaControllerImageObserver> binding_{this};
};

// A mock MediaControllerObsever that can be used for waiting for state changes.
class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP)
    TestMediaControllerObserver : public mojom::MediaControllerObserver {
 public:
  explicit TestMediaControllerObserver(
      mojom::MediaControllerPtr& media_controller);

  ~TestMediaControllerObserver() override;

  // mojom::MediaControllerObserver overrides.
  void MediaSessionInfoChanged(mojom::MediaSessionInfoPtr session) override;
  void MediaSessionMetadataChanged(
      const base::Optional<MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<mojom::MediaSessionAction>& actions) override;

  void WaitForState(mojom::MediaSessionInfo::SessionState wanted_state);
  void WaitForPlaybackState(mojom::MediaPlaybackState wanted_state);
  void WaitForEmptyInfo();

  void WaitForEmptyMetadata();
  void WaitForExpectedMetadata(const MediaMetadata& metadata);

  void WaitForEmptyActions();
  void WaitForExpectedActions(
      const std::set<mojom::MediaSessionAction>& actions);

  const mojom::MediaSessionInfoPtr& session_info() const {
    return *session_info_;
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

  base::Optional<mojom::MediaSessionInfoPtr> session_info_;
  base::Optional<base::Optional<MediaMetadata>> session_metadata_;
  base::Optional<std::set<mojom::MediaSessionAction>> session_actions_;

  base::Optional<MediaMetadata> expected_metadata_;
  base::Optional<std::set<mojom::MediaSessionAction>> expected_actions_;
  bool waiting_for_empty_metadata_ = false;

  bool waiting_for_empty_info_ = false;
  base::Optional<mojom::MediaSessionInfo::SessionState> wanted_state_;
  base::Optional<mojom::MediaPlaybackState> wanted_playback_state_;

  std::unique_ptr<base::RunLoop> run_loop_;

  mojo::Binding<mojom::MediaControllerObserver> binding_;
};

// Implements the MediaController mojo interface for tests.
class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP) TestMediaController
    : public mojom::MediaController {
 public:
  TestMediaController();
  ~TestMediaController() override;

  mojom::MediaControllerPtr CreateMediaControllerPtr();

  // mojom::MediaController:
  void Suspend() override;
  void Resume() override;
  void Stop() override {}
  void ToggleSuspendResume() override;
  void AddObserver(mojom::MediaControllerObserverPtr observer) override;
  void PreviousTrack() override;
  void NextTrack() override;
  void Seek(base::TimeDelta seek_time) override;
  void ObserveImages(mojom::MediaSessionImageType type,
                     int minimum_size_px,
                     int desired_size_px,
                     mojom::MediaControllerImageObserverPtr observer) override {
  }

  int toggle_suspend_resume_count() const {
    return toggle_suspend_resume_count_;
  }

  int suspend_count() const { return suspend_count_; }
  int resume_count() const { return resume_count_; }
  int add_observer_count() const { return add_observer_count_; }
  int previous_track_count() const { return previous_track_count_; }
  int next_track_count() const { return next_track_count_; }
  int seek_backward_count() const { return seek_backward_count_; }
  int seek_forward_count() const { return seek_forward_count_; }

  void SimulateMediaSessionInfoChanged(mojom::MediaSessionInfoPtr session_info);
  void SimulateMediaSessionActionsChanged(
      const std::vector<mojom::MediaSessionAction>& actions);
  void Flush();

 private:
  int toggle_suspend_resume_count_ = 0;
  int suspend_count_ = 0;
  int resume_count_ = 0;
  int add_observer_count_ = 0;
  int previous_track_count_ = 0;
  int next_track_count_ = 0;
  int seek_backward_count_ = 0;
  int seek_forward_count_ = 0;

  mojo::InterfacePtrSet<mojom::MediaControllerObserver> observers_;

  mojo::Binding<mojom::MediaController> binding_{this};

  DISALLOW_COPY_AND_ASSIGN(TestMediaController);
};

}  // namespace test
}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_TEST_MEDIA_CONTROLLER_H_
