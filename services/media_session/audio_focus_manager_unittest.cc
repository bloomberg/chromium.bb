// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/audio_focus_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_session {

namespace {

static const base::Optional<AudioFocusManager::RequestId> kNoRequestId;

static const AudioFocusManager::RequestId kNoFocusedSession = -1;

class MockMediaSession : public mojom::MediaSession {
 public:
  MockMediaSession() = default;
  explicit MockMediaSession(bool force_duck) : force_duck_(force_duck) {}

  ~MockMediaSession() override {}

  void Suspend(SuspendType suspend_type) override {
    DCHECK_EQ(SuspendType::kSystem, suspend_type);
    SetState(mojom::MediaSessionInfo::SessionState::kSuspended);
  }

  void StartDucking() override {
    is_ducking_ = true;
    NotifyObservers();
  }

  void StopDucking() override {
    is_ducking_ = false;
    NotifyObservers();
  }

  void GetMediaSessionInfo(GetMediaSessionInfoCallback callback) override {
    std::move(callback).Run(GetSessionInfoSync());
  }

  void AddObserver(mojom::MediaSessionObserverPtr observer) override {
    observers_.AddPtr(std::move(observer));
  }

  void GetDebugInfo(GetDebugInfoCallback callback) override {}

  void BindToMojoRequest(mojo::InterfaceRequest<mojom::MediaSession> request) {
    bindings_.AddBinding(this, std::move(request));
  }

  void SetState(mojom::MediaSessionInfo::SessionState state) {
    state_ = state;
    NotifyObservers();
  }

  bool has_observers() const { return !observers_.empty(); }

  void CloseAllObservers() { observers_.CloseAll(); }

 private:
  mojom::MediaSessionInfoPtr GetSessionInfoSync() {
    mojom::MediaSessionInfoPtr info(mojom::MediaSessionInfo::New());
    info->force_duck = force_duck_;
    info->state = state_;
    if (is_ducking_)
      info->state = mojom::MediaSessionInfo::SessionState::kDucking;
    return info;
  }

  void NotifyObservers() {
    observers_.ForAllPtrs([this](mojom::MediaSessionObserver* observer) {
      observer->MediaSessionInfoChanged(GetSessionInfoSync());
    });

    // This will flush all pending async messages on the observers.
    observers_.FlushForTesting();
  }

  const bool force_duck_ = false;
  bool is_ducking_ = false;
  mojom::MediaSessionInfo::SessionState state_ =
      mojom::MediaSessionInfo::SessionState::kInactive;

  mojo::InterfacePtrSet<mojom::MediaSessionObserver> observers_;
  mojo::BindingSet<mojom::MediaSession> bindings_;
};

}  // anonymous namespace

class AudioFocusManagerTest : public testing::Test {
 public:
  AudioFocusManagerTest() = default;

  void SetUp() override {
    // AudioFocusManager is a singleton so we should make sure we reset any
    // state in between tests.
    AudioFocusManager::GetInstance()->ResetForTesting();
  }

  void TearDown() override {
    // Run pending tasks.
    base::RunLoop().RunUntilIdle();
  }

  AudioFocusManager::RequestId GetAudioFocusedSession() const {
    const AudioFocusManager* manager = AudioFocusManager::GetInstance();
    const auto& audio_focus_stack = manager->audio_focus_stack_;

    for (auto iter = audio_focus_stack.rbegin();
         iter != audio_focus_stack.rend(); ++iter) {
      if ((*iter)->audio_focus_type() == mojom::AudioFocusType::kGain)
        return (*iter)->id();
    }
    return kNoFocusedSession;
  }

  int GetTransientMaybeDuckCount() const {
    const AudioFocusManager* manager = AudioFocusManager::GetInstance();
    const auto& audio_focus_stack = manager->audio_focus_stack_;
    int count = 0;

    for (auto iter = audio_focus_stack.rbegin();
         iter != audio_focus_stack.rend(); ++iter) {
      if ((*iter)->audio_focus_type() ==
          mojom::AudioFocusType::kGainTransientMayDuck)
        ++count;
      else
        break;
    }

    return count;
  }

  void AbandonAudioFocus(AudioFocusManager::RequestId id) {
    AudioFocusManager::GetInstance()->AbandonAudioFocus(id);
    FlushForTesting();
  }

  AudioFocusManager::RequestId RequestAudioFocus(
      MockMediaSession* session,
      mojom::AudioFocusType audio_focus_type) {
    return RequestAudioFocus(session, audio_focus_type, kNoRequestId);
  }

  AudioFocusManager::RequestId RequestAudioFocus(
      MockMediaSession* session,
      mojom::AudioFocusType audio_focus_type,
      base::Optional<AudioFocusManager::RequestId> previous_id) {
    mojom::MediaSessionPtr media_session;
    session->BindToMojoRequest(mojo::MakeRequest(&media_session));

    AudioFocusManager::RequestResponse response =
        AudioFocusManager::GetInstance()->RequestAudioFocus(
            std::move(media_session), test::GetMediaSessionInfoSync(session),
            audio_focus_type, previous_id);

    // If the audio focus was granted then we should set the session state to
    // active.
    if (response.second)
      session->SetState(mojom::MediaSessionInfo::SessionState::kActive);

    FlushForTesting();
    return response.first;
  }

  mojom::MediaSessionInfo::SessionState GetState(MockMediaSession* session) {
    return test::GetMediaSessionInfoSync(session)->state;
  }

  std::unique_ptr<test::TestAudioFocusObserver> CreateObserver() {
    std::unique_ptr<test::TestAudioFocusObserver> observer =
        std::make_unique<test::TestAudioFocusObserver>();

    mojom::AudioFocusObserverPtr observer_ptr;
    observer->BindToMojoRequest(mojo::MakeRequest(&observer_ptr));
    AudioFocusManager::GetInstance()->AddObserver(std::move(observer_ptr));

    return observer;
  }

 private:
  void FlushForTesting() {
    AudioFocusManager::GetInstance()->FlushForTesting();
  }

  base::test::ScopedTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(AudioFocusManagerTest);
};

TEST_F(AudioFocusManagerTest, InstanceAvailableAndSame) {
  AudioFocusManager* audio_focus_manager = AudioFocusManager::GetInstance();
  EXPECT_TRUE(!!audio_focus_manager);
  EXPECT_EQ(audio_focus_manager, AudioFocusManager::GetInstance());
}

TEST_F(AudioFocusManagerTest, AddObserverOnRequest) {
  MockMediaSession media_session_1;
  EXPECT_FALSE(media_session_1.has_observers());

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain,
                    kNoRequestId);
  EXPECT_TRUE(media_session_1.has_observers());
}

TEST_F(AudioFocusManagerTest, RequestAudioFocusGain_ReplaceFocusedEntry) {
  MockMediaSession media_session_1;
  MockMediaSession media_session_2;
  MockMediaSession media_session_3;

  EXPECT_EQ(kNoFocusedSession, GetAudioFocusedSession());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kInactive,
            GetState(&media_session_1));
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kInactive,
            GetState(&media_session_2));
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kInactive,
            GetState(&media_session_3));

  AudioFocusManager::RequestId request_id_1 =
      RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id_1, GetAudioFocusedSession());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));

  AudioFocusManager::RequestId request_id_2 =
      RequestAudioFocus(&media_session_2, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id_2, GetAudioFocusedSession());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kSuspended,
            GetState(&media_session_1));

  AudioFocusManager::RequestId request_id_3 =
      RequestAudioFocus(&media_session_3, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id_3, GetAudioFocusedSession());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kSuspended,
            GetState(&media_session_2));
}

TEST_F(AudioFocusManagerTest, RequestAudioFocusGain_Duplicate) {
  MockMediaSession media_session;

  EXPECT_EQ(kNoFocusedSession, GetAudioFocusedSession());

  AudioFocusManager::RequestId request_id =
      RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id, GetAudioFocusedSession());

  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain, request_id);
  EXPECT_EQ(request_id, GetAudioFocusedSession());
}

TEST_F(AudioFocusManagerTest, RequestAudioFocusGain_FromTransient) {
  MockMediaSession media_session;

  AudioFocusManager::RequestId request_id = RequestAudioFocus(
      &media_session, mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(kNoFocusedSession, GetAudioFocusedSession());
  EXPECT_EQ(1, GetTransientMaybeDuckCount());

  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain, request_id);
  EXPECT_EQ(request_id, GetAudioFocusedSession());
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
}

TEST_F(AudioFocusManagerTest, RequestAudioFocusTransient_FromGain) {
  MockMediaSession media_session;

  AudioFocusManager::RequestId request_id =
      RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id, GetAudioFocusedSession());
  EXPECT_EQ(0, GetTransientMaybeDuckCount());

  RequestAudioFocus(&media_session,
                    mojom::AudioFocusType::kGainTransientMayDuck, request_id);
  EXPECT_EQ(kNoFocusedSession, GetAudioFocusedSession());
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session));
}

TEST_F(AudioFocusManagerTest, RequestAudioFocusTransient_FromGainWhileDucking) {
  MockMediaSession media_session_1;
  MockMediaSession media_session_2;

  AudioFocusManager::RequestId request_id =
      RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_2,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_1,
                    mojom::AudioFocusType::kGainTransientMayDuck, request_id);
  EXPECT_EQ(2, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));
}

TEST_F(AudioFocusManagerTest, AbandonAudioFocus_RemovesFocusedEntry) {
  MockMediaSession media_session;

  AudioFocusManager::RequestId request_id =
      RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id, GetAudioFocusedSession());

  AbandonAudioFocus(request_id);
  EXPECT_EQ(kNoFocusedSession, GetAudioFocusedSession());
}

TEST_F(AudioFocusManagerTest, AbandonAudioFocus_NoAssociatedEntry) {
  AbandonAudioFocus(kNoFocusedSession);
  EXPECT_EQ(kNoFocusedSession, GetAudioFocusedSession());
}

TEST_F(AudioFocusManagerTest, AbandonAudioFocus_RemovesTransientEntry) {
  MockMediaSession media_session;

  AudioFocusManager::RequestId request_id = RequestAudioFocus(
      &media_session, mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    AbandonAudioFocus(request_id);

    EXPECT_EQ(0, GetTransientMaybeDuckCount());
    EXPECT_TRUE(observer->focus_lost_session_.Equals(
        test::GetMediaSessionInfoSync(&media_session)));
  }
}

TEST_F(AudioFocusManagerTest, AbandonAudioFocus_WhileDuckingThenResume) {
  MockMediaSession media_session_1;
  MockMediaSession media_session_2;

  AudioFocusManager::RequestId request_id_1 =
      RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  AudioFocusManager::RequestId request_id_2 = RequestAudioFocus(
      &media_session_2, mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  AbandonAudioFocus(request_id_1);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());

  AbandonAudioFocus(request_id_2);
  EXPECT_EQ(0, GetTransientMaybeDuckCount());

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));
}

TEST_F(AudioFocusManagerTest, AbandonAudioFocus_StopsDucking) {
  MockMediaSession media_session_1;
  MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  AudioFocusManager::RequestId request_id_2 = RequestAudioFocus(
      &media_session_2, mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  AbandonAudioFocus(request_id_2);
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));
}

TEST_F(AudioFocusManagerTest, DuckWhilePlaying) {
  MockMediaSession media_session_1;
  MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_2,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));
}

TEST_F(AudioFocusManagerTest, GainSuspendsTransient) {
  MockMediaSession media_session_1;
  MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_2,
                    mojom::AudioFocusType::kGainTransientMayDuck);

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kSuspended,
            GetState(&media_session_2));
}

TEST_F(AudioFocusManagerTest, DuckWithMultipleTransients) {
  MockMediaSession media_session_1;
  MockMediaSession media_session_2;
  MockMediaSession media_session_3;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  AudioFocusManager::RequestId request_id_2 = RequestAudioFocus(
      &media_session_2, mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  AudioFocusManager::RequestId request_id_3 = RequestAudioFocus(
      &media_session_3, mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  AbandonAudioFocus(request_id_2);
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  AbandonAudioFocus(request_id_3);
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));
}

TEST_F(AudioFocusManagerTest, MediaSessionDestroyed_ReleasesFocus) {
  {
    MockMediaSession media_session;

    AudioFocusManager::RequestId request_id =
        RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
    EXPECT_EQ(request_id, GetAudioFocusedSession());
  }

  // If the media session is destroyed without abandoning audio focus we do not
  // know until we next interact with the manager.
  MockMediaSession media_session;
  RequestAudioFocus(&media_session,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(kNoFocusedSession, GetAudioFocusedSession());
}

TEST_F(AudioFocusManagerTest, MediaSessionDestroyed_ReleasesTransients) {
  {
    MockMediaSession media_session;
    RequestAudioFocus(&media_session,
                      mojom::AudioFocusType::kGainTransientMayDuck);
    EXPECT_EQ(1, GetTransientMaybeDuckCount());
  }

  // If the media session is destroyed without abandoning audio focus we do not
  // know until we next interact with the manager.
  MockMediaSession media_session;
  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
}

TEST_F(AudioFocusManagerTest, GainDucksForceDuck) {
  MockMediaSession media_session_1(true /* force_duck */);
  MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);

  AudioFocusManager::RequestId request_id_2 =
      RequestAudioFocus(&media_session_2, mojom::AudioFocusType::kGain);

  EXPECT_EQ(request_id_2, GetAudioFocusedSession());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));
}

TEST_F(AudioFocusManagerTest,
       AbandoningGainFocusRevokesTopMostForceDuckSession) {
  MockMediaSession media_session_1(true /* force_duck */);
  MockMediaSession media_session_2;
  MockMediaSession media_session_3;

  AudioFocusManager::RequestId request_id_1 =
      RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  RequestAudioFocus(&media_session_2, mojom::AudioFocusType::kGain);

  AudioFocusManager::RequestId request_id_3 =
      RequestAudioFocus(&media_session_3, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id_3, GetAudioFocusedSession());

  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kSuspended,
            GetState(&media_session_2));
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  AbandonAudioFocus(request_id_3);
  EXPECT_EQ(request_id_1, GetAudioFocusedSession());
}

TEST_F(AudioFocusManagerTest, AudioFocusObserver_AbandonNoop) {
  std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
  AbandonAudioFocus(kNoFocusedSession);

  EXPECT_EQ(kNoFocusedSession, GetAudioFocusedSession());
  EXPECT_TRUE(observer->focus_lost_session_.is_null());
}

TEST_F(AudioFocusManagerTest, AudioFocusObserver_RequestNoop) {
  MockMediaSession media_session;
  AudioFocusManager::RequestId request_id;

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    request_id =
        RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);

    EXPECT_EQ(request_id, GetAudioFocusedSession());
    EXPECT_EQ(mojom::AudioFocusType::kGain, observer->focus_gained_type());
    EXPECT_FALSE(observer->focus_gained_session_.is_null());
  }

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain, request_id);

    EXPECT_EQ(request_id, GetAudioFocusedSession());
    EXPECT_TRUE(observer->focus_gained_session_.is_null());
  }
}

TEST_F(AudioFocusManagerTest, AudioFocusObserver_TransientMayDuck) {
  MockMediaSession media_session;
  AudioFocusManager::RequestId request_id;

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    request_id = RequestAudioFocus(
        &media_session, mojom::AudioFocusType::kGainTransientMayDuck);

    EXPECT_EQ(1, GetTransientMaybeDuckCount());
    EXPECT_EQ(mojom::AudioFocusType::kGainTransientMayDuck,
              observer->focus_gained_type());
    EXPECT_FALSE(observer->focus_gained_session_.is_null());
  }

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    AbandonAudioFocus(request_id);

    EXPECT_EQ(0, GetTransientMaybeDuckCount());
    EXPECT_TRUE(observer->focus_lost_session_.Equals(
        test::GetMediaSessionInfoSync(&media_session)));
  }
}

}  // namespace media_session
