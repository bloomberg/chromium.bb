// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "remoting/jingle_glue/mock_objects.h"
#include "remoting/host/log_to_server.h"
#include "testing/gmock_mutant.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using testing::_;
using testing::DeleteArg;
using testing::InSequence;
using testing::Return;

namespace remoting {

namespace {

ACTION_P(QuitMainMessageLoop, message_loop) {
  message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

}  // namespace

class LogToServerTest : public testing::Test {
 public:
  LogToServerTest() {}
  virtual void SetUp() OVERRIDE {
    message_loop_proxy_ = base::MessageLoopProxy::current();
    EXPECT_CALL(signal_strategy_, AddListener(_));
    log_to_server_.reset(new LogToServer(&signal_strategy_));
    EXPECT_CALL(signal_strategy_, RemoveListener(_));
  }

 protected:
  MessageLoop message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  MockSignalStrategy signal_strategy_;
  scoped_ptr<LogToServer> log_to_server_;
};

TEST_F(LogToServerTest, SendNow) {
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return("host@domain.com/1234"));
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanza(_))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitMainMessageLoop(&message_loop_))
        .RetiresOnSaturation();
  }
  log_to_server_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  log_to_server_->OnClientAuthenticated("client@domain.com/5678");
  log_to_server_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  message_loop_.Run();
}

TEST_F(LogToServerTest, SendLater) {
  log_to_server_->OnClientAuthenticated("client@domain.com/5678");
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return("host@domain.com/1234"));
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanza(_))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitMainMessageLoop(&message_loop_))
        .RetiresOnSaturation();
  }
  log_to_server_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  log_to_server_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  message_loop_.Run();
}

TEST_F(LogToServerTest, SendTwoEntriesLater) {
  log_to_server_->OnClientAuthenticated("client@domain.com/5678");
  log_to_server_->OnClientAuthenticated("client2@domain.com/6789");
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return("host@domain.com/1234"));
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanza(_))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitMainMessageLoop(&message_loop_))
        .RetiresOnSaturation();
  }
  log_to_server_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  log_to_server_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  message_loop_.Run();
}

}  // namespace remoting
