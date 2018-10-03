// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/audio_focus_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/media_session/media_session_service.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_session {

class AudioFocusManagerTest;

namespace {

const AudioFocusManager::RequestId kNoFocusedSession = -1;

const char kExampleDebugInfoName[] = "name";
const char kExampleDebugInfoOwner[] = "owner";
const char kExampleDebugInfoState[] = "state";

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

  void AddObserver(mojom::MediaSessionObserverPtr observer) override {}

  void GetDebugInfo(GetDebugInfoCallback callback) override {
    mojom::MediaSessionDebugInfoPtr debug_info(
        mojom::MediaSessionDebugInfo::New());

    debug_info->name = kExampleDebugInfoName;
    debug_info->owner = kExampleDebugInfoOwner;
    debug_info->state = kExampleDebugInfoState;

    std::move(callback).Run(std::move(debug_info));
  }

  void BindToMojoRequest(mojo::InterfaceRequest<mojom::MediaSession> request) {
    bindings_.AddBinding(this, std::move(request));
  }

 protected:
  // Audio Focus methods should be called from AudioFocusManagerTest.
  friend class media_session::AudioFocusManagerTest;

  mojom::AudioFocusRequestClient* audio_focus_request() {
    return afr_client_.get();
  }

  bool HasAudioFocusRequest() const { return afr_client_.is_bound(); }

  void ResetAudioFocusRequest() { afr_client_.reset(); }

  void SetAudioFocusRequestClient(
      mojom::AudioFocusRequestClientPtr afr_client) {
    afr_client_ = std::move(afr_client);
  }

  void FlushForTesting() { afr_client_.FlushForTesting(); }

 private:
  void SetState(mojom::MediaSessionInfo::SessionState state) {
    state_ = state;
    NotifyObservers();
  }

  void NotifyObservers() {
    if (afr_client_.is_bound())
      afr_client_->MediaSessionInfoChanged(GetSessionInfoSync());
  }

  mojom::MediaSessionInfoPtr GetSessionInfoSync() {
    mojom::MediaSessionInfoPtr info(mojom::MediaSessionInfo::New());
    info->force_duck = force_duck_;
    info->state = state_;
    if (is_ducking_)
      info->state = mojom::MediaSessionInfo::SessionState::kDucking;
    return info;
  }

  mojom::AudioFocusRequestClientPtr afr_client_;

  const bool force_duck_ = false;
  bool is_ducking_ = false;
  mojom::MediaSessionInfo::SessionState state_ =
      mojom::MediaSessionInfo::SessionState::kInactive;

  mojo::BindingSet<mojom::MediaSession> bindings_;
};

}  // anonymous namespace

class AudioFocusManagerTest : public testing::Test {
 public:
  AudioFocusManagerTest() = default;

  void SetUp() override {
    // Create an instance of the MediaSessionService.
    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            MediaSessionService::Create());
    connector_ = connector_factory_->CreateConnector();

    // Bind |audio_focus_ptr_| to AudioFocusManager.
    connector_->BindInterface("test", mojo::MakeRequest(&audio_focus_ptr_));

    // AudioFocusManager is a singleton so we should make sure we reset any
    // state in between tests.
    AudioFocusManager::GetInstance()->ResetForTesting();
  }

  void TearDown() override {
    // Run pending tasks.
    base::RunLoop().RunUntilIdle();
  }

  AudioFocusManager::RequestId GetAudioFocusedSession() {
    const auto audio_focus_requests = GetRequests();
    for (auto iter = audio_focus_requests.rbegin();
         iter != audio_focus_requests.rend(); ++iter) {
      if ((*iter)->audio_focus_type == mojom::AudioFocusType::kGain)
        return (*iter)->request_id;
    }
    return kNoFocusedSession;
  }

  int GetTransientMaybeDuckCount() {
    const auto audio_focus_requests = GetRequests();
    int count = 0;

    for (auto iter = audio_focus_requests.rbegin();
         iter != audio_focus_requests.rend(); ++iter) {
      if ((*iter)->audio_focus_type ==
          mojom::AudioFocusType::kGainTransientMayDuck)
        ++count;
      else
        break;
    }

    return count;
  }

  void AbandonAudioFocus(MockMediaSession* session) {
    AbandonAudioFocusNoReset(session);
    session->ResetAudioFocusRequest();
  }

  void AbandonAudioFocusNoReset(MockMediaSession* session) {
    DCHECK(session->HasAudioFocusRequest());
    session->audio_focus_request()->AbandonAudioFocus();
    session->FlushForTesting();

    FlushForTesting();
  }

  AudioFocusManager::RequestId RequestAudioFocus(
      MockMediaSession* session,
      mojom::AudioFocusType audio_focus_type) {
    bool result;
    base::OnceClosure callback =
        base::BindOnce([](bool* out_result) { *out_result = true; }, &result);

    if (session->HasAudioFocusRequest()) {
      // Request audio focus through the existing request.
      session->audio_focus_request()->RequestAudioFocus(
          test::GetMediaSessionInfoSync(session), audio_focus_type,
          std::move(callback));

      session->FlushForTesting();
    } else {
      // Build a new audio focus request.
      mojom::AudioFocusRequestClientPtr afr_client;

      mojom::MediaSessionPtr media_session;
      session->BindToMojoRequest(mojo::MakeRequest(&media_session));

      GetService()->RequestAudioFocus(mojo::MakeRequest(&afr_client),
                                      std::move(media_session),
                                      test::GetMediaSessionInfoSync(session),
                                      audio_focus_type, std::move(callback));

      session->SetAudioFocusRequestClient(std::move(afr_client));
      FlushForTesting();
    }

    // If the audio focus was granted then we should set the session state to
    // active.
    if (result)
      session->SetState(mojom::MediaSessionInfo::SessionState::kActive);

    FlushForTesting();
    return GetRequestIdForSession(session);
  }

  mojom::MediaSessionDebugInfoPtr GetDebugInfo(
      AudioFocusManager::RequestId request_id) {
    mojom::MediaSessionDebugInfoPtr result;
    base::OnceCallback<void(mojom::MediaSessionDebugInfoPtr)> callback =
        base::BindOnce(
            [](mojom::MediaSessionDebugInfoPtr* out_result,
               mojom::MediaSessionDebugInfoPtr result) {
              *out_result = std::move(result);
            },
            &result);

    GetService()->GetDebugInfoForRequest(request_id, std::move(callback));
    FlushForTesting();

    return result;
  }

  mojom::MediaSessionInfo::SessionState GetState(MockMediaSession* session) {
    return test::GetMediaSessionInfoSync(session)->state;
  }

  std::unique_ptr<test::TestAudioFocusObserver> CreateObserver() {
    std::unique_ptr<test::TestAudioFocusObserver> observer =
        std::make_unique<test::TestAudioFocusObserver>();

    mojom::AudioFocusObserverPtr observer_ptr;
    observer->BindToMojoRequest(mojo::MakeRequest(&observer_ptr));
    GetService()->AddObserver(std::move(observer_ptr));

    FlushForTesting();
    return observer;
  }

 private:
  std::vector<mojom::AudioFocusRequestStatePtr> GetRequests() {
    std::vector<mojom::AudioFocusRequestStatePtr> result;

    GetService()->GetFocusRequests(base::BindOnce(
        [](std::vector<mojom::AudioFocusRequestStatePtr>* out,
           std::vector<mojom::AudioFocusRequestStatePtr> requests) {
          for (auto& request : requests)
            out->push_back(request.Clone());
        },
        &result));

    FlushForTesting();
    return result;
  }

  AudioFocusManager::RequestId GetRequestIdForSession(
      MockMediaSession* session) {
    DCHECK(session->HasAudioFocusRequest());
    AudioFocusManager::RequestId id = kNoFocusedSession;

    session->audio_focus_request()->GetRequestId(
        base::BindOnce([](AudioFocusManager::RequestId* id,
                          uint64_t received_id) { *id = received_id; },
                       &id));

    session->FlushForTesting();
    EXPECT_NE(kNoFocusedSession, id);
    return id;
  }

  mojom::AudioFocusManager* GetService() const {
    return audio_focus_ptr_.get();
  }

  void FlushForTesting() {
    audio_focus_ptr_.FlushForTesting();
    AudioFocusManager::GetInstance()->FlushForTesting();
  }

  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;
  mojom::AudioFocusManagerPtr audio_focus_ptr_;

  DISALLOW_COPY_AND_ASSIGN(AudioFocusManagerTest);
};

TEST_F(AudioFocusManagerTest, InstanceAvailableAndSame) {
  AudioFocusManager* audio_focus_manager = AudioFocusManager::GetInstance();
  EXPECT_TRUE(!!audio_focus_manager);
  EXPECT_EQ(audio_focus_manager, AudioFocusManager::GetInstance());
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

  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id, GetAudioFocusedSession());
}

TEST_F(AudioFocusManagerTest, RequestAudioFocusGain_FromTransient) {
  MockMediaSession media_session;

  AudioFocusManager::RequestId request_id = RequestAudioFocus(
      &media_session, mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(kNoFocusedSession, GetAudioFocusedSession());
  EXPECT_EQ(1, GetTransientMaybeDuckCount());

  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
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
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(kNoFocusedSession, GetAudioFocusedSession());
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session));
}

TEST_F(AudioFocusManagerTest, RequestAudioFocusTransient_FromGainWhileDucking) {
  MockMediaSession media_session_1;
  MockMediaSession media_session_2;

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
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(2, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));
}

TEST_F(AudioFocusManagerTest, AbandonAudioFocus_RemovesFocusedEntry) {
  MockMediaSession media_session;

  AudioFocusManager::RequestId request_id =
      RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id, GetAudioFocusedSession());

  AbandonAudioFocus(&media_session);
  EXPECT_EQ(kNoFocusedSession, GetAudioFocusedSession());
}

TEST_F(AudioFocusManagerTest, AbandonAudioFocus_MultipleCalls) {
  MockMediaSession media_session;

  AudioFocusManager::RequestId request_id =
      RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id, GetAudioFocusedSession());

  AbandonAudioFocusNoReset(&media_session);

  std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
  AbandonAudioFocus(&media_session);

  EXPECT_EQ(kNoFocusedSession, GetAudioFocusedSession());
  EXPECT_TRUE(observer->focus_lost_session_.is_null());
}

TEST_F(AudioFocusManagerTest, AbandonAudioFocus_RemovesTransientEntry) {
  MockMediaSession media_session;

  RequestAudioFocus(&media_session,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    AbandonAudioFocus(&media_session);

    EXPECT_EQ(0, GetTransientMaybeDuckCount());
    EXPECT_TRUE(observer->focus_lost_session_.Equals(
        test::GetMediaSessionInfoSync(&media_session)));
  }
}

TEST_F(AudioFocusManagerTest, AbandonAudioFocus_WhileDuckingThenResume) {
  MockMediaSession media_session_1;
  MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_2,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  AbandonAudioFocus(&media_session_1);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());

  AbandonAudioFocus(&media_session_2);
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

  RequestAudioFocus(&media_session_2,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  AbandonAudioFocus(&media_session_2);
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

  RequestAudioFocus(&media_session_2,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_3,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  AbandonAudioFocus(&media_session_2);
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  AbandonAudioFocus(&media_session_3);
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

  AbandonAudioFocus(&media_session_3);
  EXPECT_EQ(request_id_1, GetAudioFocusedSession());
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
    RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);

    EXPECT_EQ(request_id, GetAudioFocusedSession());
    EXPECT_TRUE(observer->focus_gained_session_.is_null());
  }
}

TEST_F(AudioFocusManagerTest, AudioFocusObserver_TransientMayDuck) {
  MockMediaSession media_session;

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    RequestAudioFocus(&media_session,
                      mojom::AudioFocusType::kGainTransientMayDuck);

    EXPECT_EQ(1, GetTransientMaybeDuckCount());
    EXPECT_EQ(mojom::AudioFocusType::kGainTransientMayDuck,
              observer->focus_gained_type());
    EXPECT_FALSE(observer->focus_gained_session_.is_null());
  }

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    AbandonAudioFocus(&media_session);

    EXPECT_EQ(0, GetTransientMaybeDuckCount());
    EXPECT_TRUE(observer->focus_lost_session_.Equals(
        test::GetMediaSessionInfoSync(&media_session)));
  }
}

TEST_F(AudioFocusManagerTest, GetDebugInfo) {
  MockMediaSession media_session;
  AudioFocusManager::RequestId request_id =
      RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);

  mojom::MediaSessionDebugInfoPtr debug_info = GetDebugInfo(request_id);
  EXPECT_EQ(kExampleDebugInfoName, debug_info->name);
  EXPECT_EQ(kExampleDebugInfoOwner, debug_info->owner);
  EXPECT_EQ(kExampleDebugInfoState, debug_info->state);
}

TEST_F(AudioFocusManagerTest, GetDebugInfo_BadRequestId) {
  mojom::MediaSessionDebugInfoPtr debug_info = GetDebugInfo(kNoFocusedSession);
  EXPECT_TRUE(debug_info->name.empty());
}

}  // namespace media_session
