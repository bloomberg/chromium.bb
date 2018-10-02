// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the RTCIceTransport Blink bindings, IceTransportProxy and
// IceTransportHost by mocking out the underlying IceTransportAdapter.
// Everything is run on a single thread but with separate TestSimpleTaskRunners
// for the main thread / worker thread.

#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"

#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_ice_transport_adapter.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate_init.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_gather_options.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_event.h"
#include "third_party/webrtc/pc/webrtcsdp.h"

namespace blink {
namespace {

using testing::_;
using testing::Assign;
using testing::AllOf;
using testing::ElementsAre;
using testing::Field;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::StrEq;
using testing::StrNe;

class MockEventListener final : public EventListener {
 public:
  MockEventListener() : EventListener(ListenerType::kCPPEventListenerType) {}

  bool operator==(const EventListener& other) const final {
    return this == &other;
  }

  MOCK_METHOD2(handleEvent, void(ExecutionContext*, Event*));
};

constexpr char kRemoteUsernameFragment1[] = "usernameFragment";
constexpr char kRemotePassword1[] = "password";

RTCIceParameters CreateRemoteRTCIceParameters1() {
  RTCIceParameters ice_parameters;
  ice_parameters.setUsernameFragment(kRemoteUsernameFragment1);
  ice_parameters.setPassword(kRemotePassword1);
  return ice_parameters;
}

constexpr char kRemoteIceCandidateStr1[] =
    "candidate:a0+B/1 1 udp 2130706432 192.168.1.5 1234 typ host generation 2";

RTCIceCandidate* RTCIceCandidateFromString(V8TestingScope& scope,
                                           const String& candidate_str) {
  RTCIceCandidateInit init;
  init.setCandidate(candidate_str);
  return RTCIceCandidate::Create(scope.GetExecutionContext(), init,
                                 ASSERT_NO_EXCEPTION);
}

class MockIceTransportAdapterCrossThreadFactory
    : public IceTransportAdapterCrossThreadFactory {
 public:
  MockIceTransportAdapterCrossThreadFactory(
      std::unique_ptr<MockIceTransportAdapter> mock_adapter,
      IceTransportAdapter::Delegate** delegate_out)
      : mock_adapter_(std::move(mock_adapter)), delegate_out_(delegate_out) {}

  void InitializeOnMainThread() override {}

  std::unique_ptr<IceTransportAdapter> ConstructOnWorkerThread(
      IceTransportAdapter::Delegate* delegate) override {
    DCHECK(mock_adapter_);
    if (delegate_out_) {
      *delegate_out_ = delegate;
    }
    return std::move(mock_adapter_);
  }

 private:
  std::unique_ptr<MockIceTransportAdapter> mock_adapter_;
  IceTransportAdapter::Delegate** delegate_out_;
};

}  // namespace

class RTCIceTransportTest : public testing::Test {
 public:
  RTCIceTransportTest()
      : main_thread_(new base::TestSimpleTaskRunner()),
        worker_thread_(new base::TestSimpleTaskRunner()) {}

  ~RTCIceTransportTest() override {
    // When the V8TestingScope is destroyed at the end of a test, it will call
    // ContextDestroyed on the RTCIceTransport which will queue a task to delete
    // the IceTransportAdapter. RunUntilIdle() here ensures that the task will
    // be executed and the IceTransportAdapter deleted before finishing the
    // test.
    RunUntilIdle();

    // Explicitly verify expectations of garbage collected mock objects.
    for (auto mock : mock_event_listeners_) {
      Mock::VerifyAndClear(mock);
    }
  }

  // Run the main thread and worker thread until both are idle.
  void RunUntilIdle() {
    while (worker_thread_->HasPendingTask() || main_thread_->HasPendingTask()) {
      worker_thread_->RunPendingTasks();
      main_thread_->RunPendingTasks();
    }
  }

  // Construct a new RTCIceTransport with a mock IceTransportAdapter.
  RTCIceTransport* CreateIceTransport(
      V8TestingScope& scope,
      IceTransportAdapter::Delegate** delegate_out) {
    return CreateIceTransport(
        scope, std::make_unique<MockIceTransportAdapter>(), delegate_out);
  }

  // Construct a new RTCIceTransport with the given mock IceTransportAdapter.
  // |delegate_out|, if non-null, will be populated once the IceTransportAdapter
  // is constructed on the worker thread.
  RTCIceTransport* CreateIceTransport(
      V8TestingScope& scope,
      std::unique_ptr<MockIceTransportAdapter> mock,
      IceTransportAdapter::Delegate** delegate_out = nullptr) {
    if (delegate_out) {
      // Ensure the caller has not left the delegate_out value floating.
      DCHECK_EQ(nullptr, *delegate_out);
    }
    return RTCIceTransport::Create(
        scope.GetExecutionContext(), main_thread_, worker_thread_,
        std::make_unique<MockIceTransportAdapterCrossThreadFactory>(
            std::move(mock), delegate_out));
  }

  // Use this method to construct a MockEventListener so that the expectations
  // can be explicitly checked at the end of the test. Normally the expectations
  // would be verified in the mock destructor, but since MockEventListener is
  // garbage collected this may happen after the test has finished, improperly
  // letting it pass.
  MockEventListener* CreateMockEventListener() {
    MockEventListener* event_listener = new MockEventListener();
    mock_event_listeners_.push_back(event_listener);
    return event_listener;
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> main_thread_;
  scoped_refptr<base::TestSimpleTaskRunner> worker_thread_;
  std::vector<Persistent<MockEventListener>> mock_event_listeners_;
};

// Test that calling gather({}) calls StartGathering with non-empty local
// parameters.
TEST_F(RTCIceTransportTest, GatherStartsGatheringWithNonEmptyLocalParameters) {
  V8TestingScope scope;

  auto mock = std::make_unique<MockIceTransportAdapter>();
  auto ice_parameters_not_empty =
      AllOf(Field(&cricket::IceParameters::ufrag, StrNe("")),
            Field(&cricket::IceParameters::pwd, StrNe("")));
  EXPECT_CALL(*mock, StartGathering(ice_parameters_not_empty, _, _, _))
      .Times(1);

  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(mock));
  RTCIceGatherOptions options;
  options.setGatherPolicy("all");
  ice_transport->gather(options, ASSERT_NO_EXCEPTION);
}

// Test that calling gather({ gatherPolicy: 'all' }) calls StartGathering with
// IceTransportPolicy::kAll.
TEST_F(RTCIceTransportTest, GatherIceTransportPolicyAll) {
  V8TestingScope scope;

  auto mock = std::make_unique<MockIceTransportAdapter>();
  EXPECT_CALL(*mock, StartGathering(_, _, _, IceTransportPolicy::kAll))
      .Times(1);

  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(mock));
  RTCIceGatherOptions options;
  options.setGatherPolicy("all");
  ice_transport->gather(options, ASSERT_NO_EXCEPTION);
}

// Test that calling gather({ gatherPolicy: 'relay' }) calls StartGathering with
// IceTransportPolicy::kRelay.
TEST_F(RTCIceTransportTest, GatherIceTransportPolicyRelay) {
  V8TestingScope scope;

  auto mock = std::make_unique<MockIceTransportAdapter>();
  EXPECT_CALL(*mock, StartGathering(_, _, _, IceTransportPolicy::kRelay))
      .Times(1);

  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(mock));
  RTCIceGatherOptions options;
  options.setGatherPolicy("relay");
  ice_transport->gather(options, ASSERT_NO_EXCEPTION);
}

// Test that calling stop() deletes the underlying IceTransportAdapter.
TEST_F(RTCIceTransportTest, StopDeletesIceTransportAdapter) {
  V8TestingScope scope;

  bool mock_deleted = false;
  auto mock = std::make_unique<MockIceTransportAdapter>();
  EXPECT_CALL(*mock, Die()).WillOnce(Assign(&mock_deleted, true));

  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(mock));
  RTCIceGatherOptions options;
  options.setGatherPolicy("all");
  ice_transport->gather(options, ASSERT_NO_EXCEPTION);

  ice_transport->stop();
  RunUntilIdle();

  EXPECT_TRUE(mock_deleted);
}

// Test that the IceTransportAdapter is deleted on ContextDestroyed.
TEST_F(RTCIceTransportTest, ContextDestroyedDeletesIceTransportAdapter) {
  V8TestingScope scope;

  bool mock_deleted = false;
  auto mock = std::make_unique<MockIceTransportAdapter>();
  EXPECT_CALL(*mock, Die()).WillOnce(Assign(&mock_deleted, true));

  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(mock));
  RTCIceGatherOptions options;
  options.setGatherPolicy("all");
  ice_transport->gather(options, ASSERT_NO_EXCEPTION);

  ice_transport->ContextDestroyed(scope.GetExecutionContext());
  RunUntilIdle();

  EXPECT_TRUE(mock_deleted);
}

// Test that calling OnGatheringStateChanged(complete) on the delegate fires a
// null icecandidate event and a gatheringstatechange event.
TEST_F(RTCIceTransportTest, OnGatheringStateChangedCompleteFiresEvents) {
  V8TestingScope scope;

  IceTransportAdapter::Delegate* delegate = nullptr;
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, &delegate);
  RTCIceGatherOptions options;
  options.setGatherPolicy("all");
  ice_transport->gather(options, ASSERT_NO_EXCEPTION);
  RunUntilIdle();
  ASSERT_TRUE(delegate);

  Persistent<MockEventListener> ice_candidate_listener =
      CreateMockEventListener();
  Persistent<MockEventListener> gathering_state_change_listener =
      CreateMockEventListener();
  {
    InSequence dummy;
    EXPECT_CALL(*ice_candidate_listener, handleEvent(_, _))
        .WillOnce(Invoke([ice_transport](ExecutionContext*, Event* event) {
          auto* ice_event = static_cast<RTCPeerConnectionIceEvent*>(event);
          EXPECT_EQ(nullptr, ice_event->candidate());
        }));
    EXPECT_CALL(*gathering_state_change_listener, handleEvent(_, _))
        .WillOnce(InvokeWithoutArgs([ice_transport] {
          EXPECT_EQ("complete", ice_transport->gatheringState());
        }));
  }
  ice_transport->addEventListener(EventTypeNames::icecandidate,
                                  ice_candidate_listener);
  ice_transport->addEventListener(EventTypeNames::gatheringstatechange,
                                  gathering_state_change_listener);
  delegate->OnGatheringStateChanged(cricket::kIceGatheringComplete);

  RunUntilIdle();
}

// Test that calling start() calls Start on the IceTransportAdapter with the
// correct arguments when no remote candidates had previously been added.
TEST_F(RTCIceTransportTest,
       StartPassesRemoteParametersAndRoleAndInitialRemoteCandidates) {
  V8TestingScope scope;

  auto mock = std::make_unique<MockIceTransportAdapter>();
  auto ice_parameters_equal = AllOf(
      Field(&cricket::IceParameters::ufrag, StrEq(kRemoteUsernameFragment1)),
      Field(&cricket::IceParameters::pwd, StrEq(kRemotePassword1)));
  EXPECT_CALL(*mock, Start(ice_parameters_equal, cricket::ICEROLE_CONTROLLING,
                           ElementsAre()))
      .Times(1);

  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(mock));
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
}

MATCHER_P(SerializedIceCandidateEq, candidate_str, "") {
  std::string arg_str = webrtc::SdpSerializeCandidate(arg);
  *result_listener << "Expected ICE candidate that serializes to: "
                   << candidate_str << "; got: " << arg_str;
  return arg_str == candidate_str;
}

// Test that remote candidates are not passed to the IceTransportAdapter until
// start() is called.
TEST_F(RTCIceTransportTest, RemoteCandidatesNotPassedUntilStartCalled) {
  V8TestingScope scope;

  auto mock = std::make_unique<MockIceTransportAdapter>();
  EXPECT_CALL(
      *mock,
      Start(_, _,
            ElementsAre(SerializedIceCandidateEq(kRemoteIceCandidateStr1))))
      .Times(1);
  EXPECT_CALL(*mock, AddRemoteCandidate(_)).Times(0);

  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(mock));
  ice_transport->addRemoteCandidate(
      RTCIceCandidateFromString(scope, kRemoteIceCandidateStr1),
      ASSERT_NO_EXCEPTION);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
}

}  // namespace blink
