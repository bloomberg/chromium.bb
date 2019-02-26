// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_impl.h"

#include <memory>

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/browser/media/session/mock_media_session_player_observer.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_service_manager_context.h"
#include "media/base/media_content_type.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/media_session/public/cpp/switches.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

using media_session::mojom::AudioFocusType;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;
using media_session::mojom::MediaPlaybackState;
using media_session::test::MockMediaSessionMojoObserver;
using media_session::test::TestAudioFocusObserver;

namespace {

class MockAudioFocusDelegate : public AudioFocusDelegate {
 public:
  MockAudioFocusDelegate() = default;
  ~MockAudioFocusDelegate() override = default;

  void AbandonAudioFocus() override {}

  AudioFocusResult RequestAudioFocus(AudioFocusType type) override {
    request_audio_focus_count_++;
    return AudioFocusResult::kSuccess;
  }

  base::Optional<AudioFocusType> GetCurrentFocusType() const override {
    return AudioFocusType::kGain;
  }

  void MediaSessionInfoChanged(MediaSessionInfoPtr session_info) override {
    session_info_ = std::move(session_info);
  }

  MediaSessionInfo::SessionState GetState() const {
    DCHECK(!session_info_.is_null());
    return session_info_->state;
  }

  int request_audio_focus_count() const { return request_audio_focus_count_; }

 private:
  int request_audio_focus_count_ = 0;

  MediaSessionInfoPtr session_info_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioFocusDelegate);
};

}  // anonymous namespace

class MediaSessionImplTest : public RenderViewHostTestHarness {
 public:
  MediaSessionImplTest() = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        media_session::switches::kEnableAudioFocus);

    RenderViewHostTestHarness::SetUp();

    player_observer_.reset(new MockMediaSessionPlayerObserver());

    // Connect to the Media Session service and bind |audio_focus_ptr_| to it.
    service_manager_context_ = std::make_unique<TestServiceManagerContext>();
    service_manager::Connector* connector =
        ServiceManagerConnection::GetForProcess()->GetConnector();
    connector->BindInterface(media_session::mojom::kServiceName,
                             mojo::MakeRequest(&audio_focus_ptr_));
  }

  void TearDown() override {
    service_manager_context_.reset();

    RenderViewHostTestHarness::TearDown();
  }

  void RequestAudioFocus(MediaSessionImpl* session,
                         AudioFocusType audio_focus_type) {
    session->RequestSystemAudioFocus(audio_focus_type);
  }

  void AbandonAudioFocus(MediaSessionImpl* session) {
    session->AbandonSystemAudioFocusIfNeeded();
  }

  bool GetForceDuck(MediaSessionImpl* session) {
    return media_session::test::GetMediaSessionInfoSync(session)->force_duck;
  }

  MediaSessionInfo::SessionState GetState(MediaSessionImpl* session) {
    return media_session::test::GetMediaSessionInfoSync(session)->state;
  }

  bool HasMojoObservers(MediaSessionImpl* session) {
    return !session->mojo_observers_.empty();
  }

  void FlushForTesting(MediaSessionImpl* session) {
    session->FlushForTesting();
  }

  std::unique_ptr<TestAudioFocusObserver> CreateAudioFocusObserver() {
    std::unique_ptr<TestAudioFocusObserver> observer =
        std::make_unique<TestAudioFocusObserver>();

    media_session::mojom::AudioFocusObserverPtr observer_ptr;
    observer->BindToMojoRequest(mojo::MakeRequest(&observer_ptr));
    audio_focus_ptr_->AddObserver(std::move(observer_ptr));
    audio_focus_ptr_.FlushForTesting();

    return observer;
  }

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer_;

  void SetDelegateForTests(MediaSessionImpl* session,
                           AudioFocusDelegate* delegate) {
    session->SetDelegateForTests(base::WrapUnique(delegate));
  }

 private:
  media_session::mojom::AudioFocusManagerPtr audio_focus_ptr_;

  std::unique_ptr<TestServiceManagerContext> service_manager_context_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionImplTest);
};

TEST_F(MediaSessionImplTest, SessionInfoState) {
  std::unique_ptr<WebContents> web_contents(CreateTestWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());
  EXPECT_EQ(MediaSessionInfo::SessionState::kInactive, GetState(media_session));

  {
    MockMediaSessionMojoObserver observer(*media_session);
    RequestAudioFocus(media_session, AudioFocusType::kGain);
    FlushForTesting(media_session);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);

    EXPECT_TRUE(observer.session_info().Equals(
        media_session::test::GetMediaSessionInfoSync(media_session)));
  }

  {
    MockMediaSessionMojoObserver observer(*media_session);
    media_session->StartDucking();
    FlushForTesting(media_session);
    observer.WaitForState(MediaSessionInfo::SessionState::kDucking);

    EXPECT_TRUE(observer.session_info().Equals(
        media_session::test::GetMediaSessionInfoSync(media_session)));
  }

  {
    MockMediaSessionMojoObserver observer(*media_session);
    media_session->StopDucking();
    FlushForTesting(media_session);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);

    EXPECT_TRUE(observer.session_info().Equals(
        media_session::test::GetMediaSessionInfoSync(media_session)));
  }

  {
    MockMediaSessionMojoObserver observer(*media_session);
    media_session->Suspend(MediaSession::SuspendType::kSystem);
    FlushForTesting(media_session);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);

    EXPECT_TRUE(observer.session_info().Equals(
        media_session::test::GetMediaSessionInfoSync(media_session)));
  }

  {
    MockMediaSessionMojoObserver observer(*media_session);
    media_session->Resume(MediaSession::SuspendType::kSystem);
    FlushForTesting(media_session);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);

    EXPECT_TRUE(observer.session_info().Equals(
        media_session::test::GetMediaSessionInfoSync(media_session)));
  }

  {
    MockMediaSessionMojoObserver observer(*media_session);
    AbandonAudioFocus(media_session);
    FlushForTesting(media_session);
    observer.WaitForState(MediaSessionInfo::SessionState::kInactive);

    EXPECT_TRUE(observer.session_info().Equals(
        media_session::test::GetMediaSessionInfoSync(media_session)));
  }
}

TEST_F(MediaSessionImplTest, NotifyDelegateOnStateChange) {
  std::unique_ptr<WebContents> web_contents(CreateTestWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());
  MockAudioFocusDelegate* delegate = new MockAudioFocusDelegate();
  SetDelegateForTests(media_session, delegate);

  RequestAudioFocus(media_session, AudioFocusType::kGain);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaSessionInfo::SessionState::kActive, delegate->GetState());

  media_session->StartDucking();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaSessionInfo::SessionState::kDucking, delegate->GetState());

  media_session->StopDucking();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaSessionInfo::SessionState::kActive, delegate->GetState());

  media_session->Suspend(MediaSession::SuspendType::kSystem);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaSessionInfo::SessionState::kSuspended, delegate->GetState());

  media_session->Resume(MediaSession::SuspendType::kSystem);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaSessionInfo::SessionState::kActive, delegate->GetState());

  AbandonAudioFocus(media_session);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaSessionInfo::SessionState::kInactive, delegate->GetState());
}

TEST_F(MediaSessionImplTest, PepperForcesDuckAndRequestsFocus) {
  std::unique_ptr<WebContents> web_contents(CreateTestWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());

  int player_id = player_observer_->StartNewPlayer();

  {
    MockMediaSessionMojoObserver observer(*media_session);
    media_session->AddPlayer(player_observer_.get(), player_id,
                             media::MediaContentType::Pepper);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
  }

  EXPECT_TRUE(GetForceDuck(media_session));

  {
    MockMediaSessionMojoObserver observer(*media_session);
    media_session->RemovePlayer(player_observer_.get(), player_id);
    observer.WaitForState(MediaSessionInfo::SessionState::kInactive);
  }

  EXPECT_FALSE(GetForceDuck(media_session));
}

TEST_F(MediaSessionImplTest, RegisterMojoObserver) {
  std::unique_ptr<WebContents> web_contents(CreateTestWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());

  EXPECT_FALSE(HasMojoObservers(media_session));

  MockMediaSessionMojoObserver observer(*media_session);
  FlushForTesting(media_session);

  EXPECT_TRUE(HasMojoObservers(media_session));
}

TEST_F(MediaSessionImplTest, SessionInfo_PlaybackState) {
  std::unique_ptr<WebContents> web_contents(CreateTestWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());

  EXPECT_EQ(MediaPlaybackState::kPaused,
            media_session::test::GetMediaSessionInfoSync(media_session)
                ->playback_state);

  int player_id = player_observer_->StartNewPlayer();

  {
    MockMediaSessionMojoObserver observer(*media_session);
    media_session->AddPlayer(player_observer_.get(), player_id,
                             media::MediaContentType::Persistent);
    observer.WaitForPlaybackState(MediaPlaybackState::kPlaying);
  }

  {
    MockMediaSessionMojoObserver observer(*media_session);
    media_session->OnPlayerPaused(player_observer_.get(), player_id);
    observer.WaitForPlaybackState(MediaPlaybackState::kPaused);
  }
}

#if !defined(OS_ANDROID)

TEST_F(MediaSessionImplTest, WebContentsDestroyed_ReleasesFocus) {
  std::unique_ptr<WebContents> web_contents(CreateTestWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());

  {
    std::unique_ptr<TestAudioFocusObserver> observer =
        CreateAudioFocusObserver();
    RequestAudioFocus(media_session, AudioFocusType::kGain);
    observer->WaitForGainedEvent();
  }

  {
    MockMediaSessionMojoObserver observer(*media_session);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
  }

  {
    std::unique_ptr<TestAudioFocusObserver> observer =
        CreateAudioFocusObserver();
    web_contents.reset();
    observer->WaitForLostEvent();
  }
}

TEST_F(MediaSessionImplTest, WebContentsDestroyed_ReleasesTransients) {
  std::unique_ptr<WebContents> web_contents(CreateTestWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());

  {
    std::unique_ptr<TestAudioFocusObserver> observer =
        CreateAudioFocusObserver();
    RequestAudioFocus(media_session, AudioFocusType::kGainTransientMayDuck);
    observer->WaitForGainedEvent();
  }

  {
    MockMediaSessionMojoObserver observer(*media_session);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
  }

  {
    std::unique_ptr<TestAudioFocusObserver> observer =
        CreateAudioFocusObserver();
    web_contents.reset();
    observer->WaitForLostEvent();
  }
}

TEST_F(MediaSessionImplTest, WebContentsDestroyed_StopsDucking) {
  std::unique_ptr<WebContents> web_contents_1(CreateTestWebContents());
  MediaSessionImpl* media_session_1 =
      MediaSessionImpl::Get(web_contents_1.get());

  std::unique_ptr<WebContents> web_contents_2(CreateTestWebContents());
  MediaSessionImpl* media_session_2 =
      MediaSessionImpl::Get(web_contents_2.get());

  {
    std::unique_ptr<TestAudioFocusObserver> observer =
        CreateAudioFocusObserver();
    RequestAudioFocus(media_session_1, AudioFocusType::kGain);
    observer->WaitForGainedEvent();
  }

  {
    MockMediaSessionMojoObserver observer(*media_session_1);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
  }

  {
    std::unique_ptr<TestAudioFocusObserver> observer =
        CreateAudioFocusObserver();
    RequestAudioFocus(media_session_2, AudioFocusType::kGainTransientMayDuck);
    observer->WaitForGainedEvent();
  }


  {
    MockMediaSessionMojoObserver observer(*media_session_1);
    observer.WaitForState(MediaSessionInfo::SessionState::kDucking);
  }

  {
    std::unique_ptr<TestAudioFocusObserver> observer =
        CreateAudioFocusObserver();
    web_contents_2.reset();
    observer->WaitForLostEvent();
  }

  {
    MockMediaSessionMojoObserver observer(*media_session_1);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
  }
}

TEST_F(MediaSessionImplTest, RequestAudioFocus_OnFocus_Active) {
  std::unique_ptr<WebContents> web_contents(CreateTestWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());
  MockAudioFocusDelegate* delegate = new MockAudioFocusDelegate();
  SetDelegateForTests(media_session, delegate);

  {
    MockMediaSessionMojoObserver observer(*media_session);
    RequestAudioFocus(media_session, AudioFocusType::kGain);
    FlushForTesting(media_session);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
  }

  EXPECT_EQ(1, delegate->request_audio_focus_count());
  media_session->OnWebContentsFocused(nullptr);
  EXPECT_EQ(2, delegate->request_audio_focus_count());
}

TEST_F(MediaSessionImplTest, RequestAudioFocus_OnFocus_Inactive) {
  std::unique_ptr<WebContents> web_contents(CreateTestWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());
  MockAudioFocusDelegate* delegate = new MockAudioFocusDelegate();
  SetDelegateForTests(media_session, delegate);
  EXPECT_EQ(MediaSessionInfo::SessionState::kInactive, GetState(media_session));

  EXPECT_EQ(0, delegate->request_audio_focus_count());
  media_session->OnWebContentsFocused(nullptr);
  EXPECT_EQ(0, delegate->request_audio_focus_count());
}

TEST_F(MediaSessionImplTest, RequestAudioFocus_OnFocus_Suspended) {
  std::unique_ptr<WebContents> web_contents(CreateTestWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());
  MockAudioFocusDelegate* delegate = new MockAudioFocusDelegate();
  SetDelegateForTests(media_session, delegate);

  {
    MockMediaSessionMojoObserver observer(*media_session);
    RequestAudioFocus(media_session, AudioFocusType::kGain);
    FlushForTesting(media_session);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
  }

  {
    MockMediaSessionMojoObserver observer(*media_session);
    media_session->Suspend(MediaSession::SuspendType::kSystem);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);
  }

  EXPECT_EQ(1, delegate->request_audio_focus_count());
  media_session->OnWebContentsFocused(nullptr);
  EXPECT_EQ(1, delegate->request_audio_focus_count());
}

#endif  // !defined(OS_ANDROID)

}  // namespace content
