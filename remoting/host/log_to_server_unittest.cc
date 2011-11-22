// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "remoting/jingle_glue/mock_objects.h"
#include "remoting/host/log_to_server.h"
#include "testing/gmock_mutant.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;

namespace remoting {

namespace {

ACTION_P(QuitMainMessageLoop, message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

}  // namespace

class LogToServerTest : public testing::Test {
 public:
  LogToServerTest() {}
  virtual void SetUp() OVERRIDE {
    message_loop_proxy_ = base::MessageLoopProxy::current();
    log_to_server_.reset(new LogToServer(message_loop_proxy_));
  }

 protected:
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  scoped_ptr<LogToServer> log_to_server_;
  MessageLoop message_loop_;
  MockSignalStrategy signal_strategy_;
};

TEST_F(LogToServerTest, SendNow) {
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanza(_));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitMainMessageLoop(&message_loop_))
        .RetiresOnSaturation();
  }
  log_to_server_->OnSignallingConnected(&signal_strategy_,
                                        "host@domain.com/1234");
  log_to_server_->OnClientAuthenticated("client@domain.com/5678");
  log_to_server_->OnSignallingDisconnected();
  message_loop_.Run();
}

TEST_F(LogToServerTest, SendLater) {
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanza(_));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitMainMessageLoop(&message_loop_))
        .RetiresOnSaturation();
  }
  log_to_server_->OnClientAuthenticated("client@domain.com/5678");
  log_to_server_->OnSignallingConnected(&signal_strategy_,
                                        "host@domain.com/1234");
  log_to_server_->OnSignallingDisconnected();
  message_loop_.Run();
}

TEST_F(LogToServerTest, SendTwoEntriesLater) {
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanza(_));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitMainMessageLoop(&message_loop_))
        .RetiresOnSaturation();
  }
  log_to_server_->OnClientAuthenticated("client@domain.com/5678");
  log_to_server_->OnClientAuthenticated("client2@domain.com/6789");
  log_to_server_->OnSignallingConnected(&signal_strategy_,
                                        "host@domain.com/1234");
  log_to_server_->OnSignallingDisconnected();
  message_loop_.Run();
}

}  // namespace remoting
