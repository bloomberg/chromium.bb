// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/grpc_test_support/grpc_test_server.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "remoting/proto/remoting/v1/directory_service.grpc.pb.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "remoting/signaling/log_to_server.h"
#include "remoting/signaling/muxing_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;
using testing::AtMost;

constexpr char kOAuthAccessToken[] = "fake_access_token";
constexpr char kHostId[] = "fake_host_id";
constexpr char kUserEmail[] = "fake_user@domain.com";

constexpr char kFtlId[] = "fake_user@domain.com/chromoting_ftl_abc123";
constexpr char kJabberId[] = "fake_user@domain.com/chromotingABC123";

constexpr int32_t kGoodIntervalSeconds = 300;

constexpr int kExpectedSequenceIdUnset = -1;

constexpr base::TimeDelta kWaitForAllStrategiesConnectedTimeout =
    base::TimeDelta::FromSecondsD(5.5);
constexpr base::TimeDelta kOfflineReasonTimeout =
    base::TimeDelta::FromSeconds(123);
constexpr base::TimeDelta kTestHeartbeatDelay =
    base::TimeDelta::FromSeconds(350);

class MockDirectoryService final
    : public apis::v1::RemotingDirectoryService::Service {
 public:
  MOCK_METHOD3(Heartbeat,
               grpc::Status(grpc::ServerContext*,
                            const apis::v1::HeartbeatRequest*,
                            apis::v1::HeartbeatResponse*));
};

void ValidateHeartbeat(const apis::v1::HeartbeatRequest& request,
                       bool expects_ftl_id,
                       bool expects_jabber_id,
                       int expected_sequence_id,
                       const std::string& expected_host_offline_reason = {}) {
  ASSERT_TRUE(request.has_host_version());
  if (expected_sequence_id != kExpectedSequenceIdUnset) {
    ASSERT_EQ(expected_sequence_id, request.sequence_id());
  }
  if (expected_host_offline_reason.empty()) {
    ASSERT_FALSE(request.has_host_offline_reason());
  } else {
    ASSERT_EQ(expected_host_offline_reason, request.host_offline_reason());
  }
  ASSERT_EQ(kHostId, request.host_id());

  std::string signaling_id;
  if (expects_ftl_id) {
    ASSERT_EQ(kFtlId, request.tachyon_id());
    signaling_id = kFtlId;
  }
  if (expects_jabber_id) {
    ASSERT_EQ(kJabberId, request.jabber_id());
    if (signaling_id.empty()) {
      signaling_id = kJabberId;
    }
  }

  auto key_pair = RsaKeyPair::FromString(kTestRsaKeyPair);
  EXPECT_TRUE(key_pair.get());

  std::string expected_signature =
      key_pair->SignMessage(std::string(signaling_id) + ' ' +
                            base::NumberToString(request.sequence_id()));
  ASSERT_EQ(expected_signature, request.signature());
}

decltype(auto) DoValidateHeartbeatAndRespondOk(
    bool expects_ftl_id,
    bool expects_jabber_id,
    int expected_sequence_id,
    const std::string& expected_host_offline_reason = {}) {
  return [=](grpc::ServerContext*, const apis::v1::HeartbeatRequest* request,
             apis::v1::HeartbeatResponse* response) {
    ValidateHeartbeat(*request, expects_ftl_id, expects_jabber_id,
                      expected_sequence_id, expected_host_offline_reason);
    response->set_set_interval_seconds(kGoodIntervalSeconds);
    return grpc::Status::OK;
  };
}

}  // namespace

class HeartbeatSenderTest : public testing::Test, public LogToServer {
 public:
  HeartbeatSenderTest() {
    auto ftl_signal_strategy =
        std::make_unique<FakeSignalStrategy>(SignalingAddress(kFtlId));
    auto xmpp_signal_strategy =
        std::make_unique<FakeSignalStrategy>(SignalingAddress(kJabberId));
    ftl_signal_strategy_ = ftl_signal_strategy.get();
    xmpp_signal_strategy_ = xmpp_signal_strategy.get();

    // Start in disconnected state.
    ftl_signal_strategy_->Disconnect();
    xmpp_signal_strategy_->Disconnect();

    muxing_signal_strategy_ = std::make_unique<MuxingSignalStrategy>(
        std::move(ftl_signal_strategy), std::move(xmpp_signal_strategy));

    auto key_pair = RsaKeyPair::FromString(kTestRsaKeyPair);
    EXPECT_TRUE(key_pair.get());

    heartbeat_sender_ = std::make_unique<HeartbeatSender>(
        mock_heartbeat_successful_callback_.Get(),
        mock_unknown_host_id_error_callback_.Get(),
        mock_unauthenticated_error_callback_.Get(), kHostId,
        muxing_signal_strategy_.get(), key_pair, &oauth_token_getter_, this);
    heartbeat_sender_->SetGrpcChannelForTest(
        test_server_.CreateInProcessChannel());
  }

 protected:
  HeartbeatSender* heartbeat_sender() { return heartbeat_sender_.get(); }

  const net::BackoffEntry& GetBackoff() const {
    return heartbeat_sender_->backoff_;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME};
  test::GrpcTestServer<MockDirectoryService> test_server_;

  FakeSignalStrategy* ftl_signal_strategy_;
  FakeSignalStrategy* xmpp_signal_strategy_;

  base::MockCallback<base::OnceClosure> mock_heartbeat_successful_callback_;
  base::MockCallback<base::OnceClosure> mock_unknown_host_id_error_callback_;
  base::MockCallback<base::OnceClosure> mock_unauthenticated_error_callback_;

  std::vector<ServerLogEntry> received_log_entries_;

 private:
  // LogToServer interface.
  void Log(const ServerLogEntry& entry) override {
    received_log_entries_.push_back(entry);
  }

  ServerLogEntry::Mode mode() const override { return ServerLogEntry::ME2ME; }

  std::unique_ptr<MuxingSignalStrategy> muxing_signal_strategy_;

  // |heartbeat_sender_| must be deleted before |muxing_signal_strategy_|.
  std::unique_ptr<HeartbeatSender> heartbeat_sender_;

  FakeOAuthTokenGetter oauth_token_getter_{OAuthTokenGetter::Status::SUCCESS,
                                           kUserEmail, kOAuthAccessToken};
};

TEST_F(HeartbeatSenderTest, SendHeartbeat_OnlyXmpp) {
  base::RunLoop run_loop;

  EXPECT_CALL(*test_server_, Heartbeat(_, _, _))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ false,
                                                /* xmpp */ true, 0));

  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).WillOnce([&]() {
    run_loop.Quit();
  });

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindLambdaForTesting([&]() { xmpp_signal_strategy_->Connect(); }));
  run_loop.Run();
}

TEST_F(HeartbeatSenderTest, SendHeartbeat_OnlyFtl) {
  base::RunLoop run_loop;

  EXPECT_CALL(*test_server_, Heartbeat(_, _, _))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ false, 0));

  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).WillOnce([&]() {
    run_loop.Quit();
  });

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindLambdaForTesting([&]() { ftl_signal_strategy_->Connect(); }));
  run_loop.Run();
}

TEST_F(HeartbeatSenderTest, SendHeartbeat_XmppAndFtl) {
  base::RunLoop run_loop;

  EXPECT_CALL(*test_server_, Heartbeat(_, _, _))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ true, 0));

  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).WillOnce([&]() {
    run_loop.Quit();
  });

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ftl_signal_strategy_->Connect();
        xmpp_signal_strategy_->Connect();
      }));
  run_loop.Run();
}

TEST_F(HeartbeatSenderTest, SendHeartbeat_SendFtlThenBoth) {
  base::RunLoop run_loop;

  EXPECT_CALL(*test_server_, Heartbeat(_, _, _))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ false, 0))
      .WillOnce([&](grpc::ServerContext*,
                    const apis::v1::HeartbeatRequest* request,
                    apis::v1::HeartbeatResponse* response) {
        ValidateHeartbeat(*request, /* ftl */ true, /* xmpp */ true, 1);
        run_loop.QuitWhenIdle();
        return grpc::Status::OK;
      });

  // This may or may not be called depending on when the first heartbeat
  // response is delivered.
  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).Times(AtMost(1));

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ftl_signal_strategy_->Connect();
        scoped_task_environment_.FastForwardBy(
            kWaitForAllStrategiesConnectedTimeout);
        xmpp_signal_strategy_->Connect();
      }));
  run_loop.Run();
}

TEST_F(HeartbeatSenderTest, SignalingReconnect_NewHeartbeats) {
  base::RunLoop run_loop;

  EXPECT_CALL(*test_server_, Heartbeat(_, _, _))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ true, 0))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ false,
                                                /* xmpp */ true, 1))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ true, 2))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ false,
                                                /* xmpp */ true, 3))
      .WillOnce([&](grpc::ServerContext*,
                    const apis::v1::HeartbeatRequest* request,
                    apis::v1::HeartbeatResponse* response) {
        ValidateHeartbeat(*request, /* ftl */ true, /* xmpp */ true, 4);
        run_loop.QuitWhenIdle();
        return grpc::Status::OK;
      });

  // This may or may not be called depending on when the first heartbeat
  // response is delivered.
  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).Times(AtMost(1));

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ftl_signal_strategy_->Connect();
        xmpp_signal_strategy_->Connect();
        scoped_task_environment_.FastForwardUntilNoTasksRemain();
        ftl_signal_strategy_->Disconnect();
        scoped_task_environment_.FastForwardUntilNoTasksRemain();
        ftl_signal_strategy_->Connect();
        scoped_task_environment_.FastForwardUntilNoTasksRemain();
        ftl_signal_strategy_->Disconnect();
        scoped_task_environment_.FastForwardUntilNoTasksRemain();
        xmpp_signal_strategy_->Disconnect();
        scoped_task_environment_.FastForwardUntilNoTasksRemain();
        ftl_signal_strategy_->Connect();
        xmpp_signal_strategy_->Connect();
        scoped_task_environment_.FastForwardUntilNoTasksRemain();
      }));
  run_loop.Run();
}

TEST_F(HeartbeatSenderTest, SetHostOfflineReason) {
  base::MockCallback<base::OnceCallback<void(bool success)>> mock_ack_callback;
  EXPECT_CALL(mock_ack_callback, Run(_)).Times(0);

  heartbeat_sender()->SetHostOfflineReason("test_error", kOfflineReasonTimeout,
                                           mock_ack_callback.Get());

  testing::Mock::VerifyAndClearExpectations(&mock_ack_callback);

  base::RunLoop run_loop;

  EXPECT_CALL(*test_server_, Heartbeat(_, _, _))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ true, 0,
                                                "test_error"));

  // Callback should run once, when we get response to offline-reason.
  EXPECT_CALL(mock_ack_callback, Run(_)).Times(1);
  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).WillOnce([&]() {
    run_loop.Quit();
  });

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ftl_signal_strategy_->Connect();
        xmpp_signal_strategy_->Connect();
      }));
  run_loop.Run();
}

TEST_F(HeartbeatSenderTest, HostOsInfoOnFirstHeartbeat) {
  base::RunLoop run_loop_1;

  EXPECT_CALL(*test_server_, Heartbeat(_, _, _))
      .WillOnce([](grpc::ServerContext*,
                   const apis::v1::HeartbeatRequest* request,
                   apis::v1::HeartbeatResponse* response) {
        ValidateHeartbeat(*request, /* ftl */ true, /* xmpp */ true, 0);
        // First heartbeat has host OS info.
        EXPECT_TRUE(request->has_host_os_name());
        EXPECT_TRUE(request->has_host_os_version());
        response->set_set_interval_seconds(kGoodIntervalSeconds);
        return grpc::Status::OK;
      });
  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).WillOnce([&]() {
    run_loop_1.Quit();
  });
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ftl_signal_strategy_->Connect();
        xmpp_signal_strategy_->Connect();
      }));
  run_loop_1.Run();

  base::RunLoop run_loop_2;
  EXPECT_CALL(*test_server_, Heartbeat(_, _, _))
      .WillOnce([&](grpc::ServerContext*,
                    const apis::v1::HeartbeatRequest* request,
                    apis::v1::HeartbeatResponse* response) {
        ValidateHeartbeat(*request, /* ftl */ true, /* xmpp */ true, 1);
        // Subsequent heartbeat has no host OS info.
        EXPECT_FALSE(request->has_host_os_name());
        EXPECT_FALSE(request->has_host_os_version());
        run_loop_2.QuitWhenIdle();
        return grpc::Status::OK;
      });
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        scoped_task_environment_.FastForwardBy(kTestHeartbeatDelay);
      }));
  run_loop_2.Run();
}

TEST_F(HeartbeatSenderTest, UnknownHostId) {
  base::RunLoop run_loop;

  EXPECT_CALL(*test_server_, Heartbeat(_, _, _))
      .WillRepeatedly([](grpc::ServerContext*,
                         const apis::v1::HeartbeatRequest* request,
                         apis::v1::HeartbeatResponse* response) {
        ValidateHeartbeat(*request, /* ftl */ true, /* xmpp */ true,
                          kExpectedSequenceIdUnset);
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "not found");
      });

  EXPECT_CALL(mock_unknown_host_id_error_callback_, Run()).WillOnce([&]() {
    run_loop.Quit();
  });

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ftl_signal_strategy_->Connect();
        xmpp_signal_strategy_->Connect();
      }));
  run_loop.Run();
}

TEST_F(HeartbeatSenderTest, SendHeartbeatLogEntryOnHeartbeat) {
  base::RunLoop run_loop;

  EXPECT_CALL(*test_server_, Heartbeat(_, _, _))
      .WillOnce(DoValidateHeartbeatAndRespondOk(/* ftl */ true,
                                                /* xmpp */ true, 0));

  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).WillOnce([&]() {
    run_loop.Quit();
  });

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ftl_signal_strategy_->Connect();
        xmpp_signal_strategy_->Connect();
      }));
  run_loop.Run();

  ASSERT_EQ(1u, received_log_entries_.size());
}

TEST_F(HeartbeatSenderTest, FailedToHeartbeat_Backoff) {
  base::RunLoop run_loop;

  EXPECT_CALL(*test_server_, Heartbeat(_, _, _))
      .WillOnce([&](grpc::ServerContext*,
                    const apis::v1::HeartbeatRequest* request,
                    apis::v1::HeartbeatResponse* response) {
        EXPECT_EQ(0, GetBackoff().failure_count());
        ValidateHeartbeat(*request, /* ftl */ true, /* xmpp */ true, 0);
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "unavailable");
      })
      .WillOnce([&](grpc::ServerContext*,
                    const apis::v1::HeartbeatRequest* request,
                    apis::v1::HeartbeatResponse* response) {
        EXPECT_EQ(1, GetBackoff().failure_count());
        ValidateHeartbeat(*request, /* ftl */ true, /* xmpp */ true, 1);
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "unavailable");
      })
      .WillOnce([&](grpc::ServerContext*,
                    const apis::v1::HeartbeatRequest* request,
                    apis::v1::HeartbeatResponse* response) {
        EXPECT_EQ(2, GetBackoff().failure_count());
        ValidateHeartbeat(*request, /* ftl */ true, /* xmpp */ true, 2);
        return grpc::Status::OK;
      });

  EXPECT_CALL(mock_heartbeat_successful_callback_, Run()).WillOnce([&]() {
    run_loop.Quit();
  });

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ftl_signal_strategy_->Connect();
        xmpp_signal_strategy_->Connect();
      }));
  run_loop.Run();
  ASSERT_EQ(0, GetBackoff().failure_count());
}

TEST_F(HeartbeatSenderTest, Unauthenticated) {
  base::RunLoop run_loop;

  int heartbeat_count = 0;
  EXPECT_CALL(*test_server_, Heartbeat(_, _, _))
      .WillRepeatedly([&](grpc::ServerContext*,
                          const apis::v1::HeartbeatRequest* request,
                          apis::v1::HeartbeatResponse* response) {
        ValidateHeartbeat(*request, /* ftl */ true, /* xmpp */ true,
                          kExpectedSequenceIdUnset);
        heartbeat_count++;
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED,
                            "unauthenticated");
      });

  EXPECT_CALL(mock_unauthenticated_error_callback_, Run()).WillOnce([&]() {
    run_loop.Quit();
  });

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ftl_signal_strategy_->Connect();
        xmpp_signal_strategy_->Connect();
      }));
  run_loop.Run();

  // Should retry heartbeating at least once.
  ASSERT_LT(1, heartbeat_count);
}

}  // namespace remoting
