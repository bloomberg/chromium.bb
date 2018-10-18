// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/media_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "services/media_session/media_session_service.h"
#include "services/media_session/mock_media_session.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_session {

class MediaControllerTest : public testing::Test {
 public:
  MediaControllerTest() = default;

  void SetUp() override {
    // Create an instance of the MediaSessionService.
    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            MediaSessionService::Create());
    connector_ = connector_factory_->CreateConnector();

    // Bind |audio_focus_ptr_| to AudioFocusManager.
    connector_->BindInterface("test", mojo::MakeRequest(&audio_focus_ptr_));

    // Bind |media_controller_ptr_| to MediaController.
    connector_->BindInterface("test",
                              mojo::MakeRequest(&media_controller_ptr_));
  }

  void TearDown() override {
    // Run pending tasks.
    base::RunLoop().RunUntilIdle();
  }

  void RequestAudioFocus(test::MockMediaSession& session) {
    session.RequestAudioFocusFromService(audio_focus_ptr_,
                                         mojom::AudioFocusType::kGainTransient);
  }

  mojom::MediaControllerPtr& controller() { return media_controller_ptr_; }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;
  mojom::AudioFocusManagerPtr audio_focus_ptr_;
  mojom::MediaControllerPtr media_controller_ptr_;

  DISALLOW_COPY_AND_ASSIGN(MediaControllerTest);
};

TEST_F(MediaControllerTest, ActiveController_Suspend) {
  test::MockMediaSession media_session;

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    controller()->Suspend();
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kSuspended);
  }
}

TEST_F(MediaControllerTest, ActiveController_Suspend_Multiple) {
  test::MockMediaSession media_session_1;

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    RequestAudioFocus(media_session_1);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  test::MockMediaSession media_session_2;

  {
    test::MockMediaSessionMojoObserver observer_1(media_session_1);
    test::MockMediaSessionMojoObserver observer_2(media_session_2);

    RequestAudioFocus(media_session_2);

    observer_1.WaitForState(mojom::MediaSessionInfo::SessionState::kSuspended);
    observer_2.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session_2);
    controller()->Suspend();
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kSuspended);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    media_session_2.AbandonAudioFocusFromClient();
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  {
    test::MockMediaSessionMojoObserver observer(media_session_1);
    controller()->Suspend();
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kSuspended);
  }
}

TEST_F(MediaControllerTest, ActiveController_Suspend_Noop) {
  controller()->Suspend();
}

TEST_F(MediaControllerTest, ActiveController_Suspend_Noop_Abandoned) {
  test::MockMediaSession media_session;

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }

  media_session.AbandonAudioFocusFromClient();

  controller()->Suspend();

  {
    test::MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session);
    observer.WaitForState(mojom::MediaSessionInfo::SessionState::kActive);
  }
}

}  // namespace media_session
