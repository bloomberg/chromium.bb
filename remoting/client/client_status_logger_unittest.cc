// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/client_status_logger.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/protocol/performance_tracker.h"
#include "remoting/signaling/mock_signal_strategy.h"
#include "remoting/signaling/server_log_entry_unittest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using buzz::XmlElement;
using buzz::QName;
using remoting::protocol::ConnectionToHost;
using testing::_;
using testing::DeleteArg;
using testing::InSequence;
using testing::Return;

namespace remoting {

namespace {

ACTION_P(QuitRunLoop, run_loop) {
  run_loop->QuitWhenIdle();
}

const char kTestBotJid[] = "remotingunittest@bot.talk.google.com";
const char kClientJid[] = "host@domain.com/1234";

MATCHER_P2(IsStateChange, new_state, error, "") {
  XmlElement* entry = GetSingleLogEntryFromStanza(arg);
  if (!entry) {
    return false;
  }

  bool is_state_change = (
      entry->Attr(QName(std::string(), "event-name")) == "session-state" &&
      entry->Attr(QName(std::string(), "session-state")) == new_state &&
      entry->Attr(QName(std::string(), "role")) == "client" &&
      entry->Attr(QName(std::string(), "mode")) == "me2me");
  if (!std::string(error).empty()) {
    is_state_change = is_state_change &&
        entry->Attr(QName(std::string(), "connection-error")) == error;
  }
  return is_state_change;
}

MATCHER(IsStatisticsLog, "") {
  XmlElement* entry = GetSingleLogEntryFromStanza(arg);
  if (!entry) {
    return false;
  }

  return entry->Attr(QName(std::string(), "event-name")) ==
      "connection-statistics";
}

}  // namespace

class ClientStatusLoggerTest : public testing::Test {
 public:
  ClientStatusLoggerTest() {}
  void SetUp() override {
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, RemoveListener(_));
    client_status_logger_.reset(
        new ClientStatusLogger(ServerLogEntry::ME2ME,
                               &signal_strategy_,
                               kTestBotJid));
  }

 protected:
  base::MessageLoop message_loop_;
  MockSignalStrategy signal_strategy_;
  std::unique_ptr<ClientStatusLogger> client_status_logger_;
};

TEST_F(ClientStatusLoggerTest, LogStateChange) {
  base::RunLoop run_loop;
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return(kClientJid));
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(
        IsStateChange("connected", std::string())))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitRunLoop(&run_loop))
        .RetiresOnSaturation();
  }
  client_status_logger_->LogSessionStateChange(ConnectionToHost::CONNECTED,
                                               protocol::OK);

  // Setting the state to CONNECTED causes the log to be sent. Setting the
  // state to DISCONNECTED causes |signal_strategy_| to be cleaned up,
  // which removes the listener and terminates the test.
  client_status_logger_->SetSignalingStateForTest(SignalStrategy::CONNECTED);
  client_status_logger_->SetSignalingStateForTest(SignalStrategy::DISCONNECTED);
  run_loop.Run();
}

TEST_F(ClientStatusLoggerTest, LogStateChangeError) {
  base::RunLoop run_loop;
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return(kClientJid));
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(
        IsStateChange("connection-failed", "host-is-offline")))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitRunLoop(&run_loop))
        .RetiresOnSaturation();
  }
  client_status_logger_->LogSessionStateChange(ConnectionToHost::FAILED,
                                               protocol::PEER_IS_OFFLINE);

  client_status_logger_->SetSignalingStateForTest(SignalStrategy::CONNECTED);
  client_status_logger_->SetSignalingStateForTest(SignalStrategy::DISCONNECTED);
  run_loop.Run();
}

TEST_F(ClientStatusLoggerTest, LogStatistics) {
  base::RunLoop run_loop;
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return(kClientJid));
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(
        IsStatisticsLog()))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitRunLoop(&run_loop))
        .RetiresOnSaturation();
  }

  protocol::PerformanceTracker perf_tracker;
  client_status_logger_->LogStatistics(&perf_tracker);

  client_status_logger_->SetSignalingStateForTest(SignalStrategy::CONNECTED);
  client_status_logger_->SetSignalingStateForTest(SignalStrategy::DISCONNECTED);
  run_loop.Run();
}

}  // namespace remoting
