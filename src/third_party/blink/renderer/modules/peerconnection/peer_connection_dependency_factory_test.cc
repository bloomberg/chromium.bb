// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_client.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/webrtc_overrides/metronome_task_queue_factory.h"

namespace blink {

class PeerConnectionDependencyFactoryTest : public ::testing::Test {
 public:
  void EnsureDependencyFactory(ExecutionContext& context) {
    dependency_factory_ = &PeerConnectionDependencyFactory::From(context);
    ASSERT_TRUE(dependency_factory_);
  }

  std::unique_ptr<RTCPeerConnectionHandler> CreateRTCPeerConnectionHandler() {
    std::unique_ptr<RTCPeerConnectionHandler> handler =
        dependency_factory_->CreateRTCPeerConnectionHandler(
            &mock_client_,
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            /*force_encoded_audio_insertable_streams=*/false,
            /*force_encoded_video_insertable_streams=*/false);
    MediaConstraints constraints;
    DummyExceptionStateForTesting exception_state;
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    handler->InitializeForTest(config, constraints,
                               /*peer_connection_tracker=*/nullptr,
                               exception_state);
    return handler;
  }

 protected:
  Persistent<PeerConnectionDependencyFactory> dependency_factory_;
  MockRTCPeerConnectionHandlerClient mock_client_;
};

TEST_F(PeerConnectionDependencyFactoryTest, CreateRTCPeerConnectionHandler) {
  V8TestingScope scope;
  EnsureDependencyFactory(*scope.GetExecutionContext());

  std::unique_ptr<RTCPeerConnectionHandler> pc_handler =
      CreateRTCPeerConnectionHandler();
  EXPECT_TRUE(pc_handler);
}

}  // namespace blink
