// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "remoting/host/host_status_monitor_fake.h"
#include "remoting/host/log_to_server.h"
#include "remoting/jingle_glue/mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using buzz::XmlElement;
using buzz::QName;
using testing::_;
using testing::DeleteArg;
using testing::InSequence;
using testing::Return;

namespace remoting {

namespace {

ACTION_P(QuitMainMessageLoop, message_loop) {
  message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

const char kJabberClientNamespace[] = "jabber:client";
const char kChromotingNamespace[] = "google:remoting";
const char kTestBotJid[] = "remotingunittest@bot.talk.google.com";
const char kClientJid1[] = "client@domain.com/1234";
const char kClientJid2[] = "client@domain.com/5678";
const char kHostJid[] = "host@domain.com/1234";

bool IsLogEntryForConnection(XmlElement* node, const char* connection_type) {
  return (node->Name() == QName(kChromotingNamespace, "entry") &&
          node->Attr(QName("", "event-name")) == "session-state" &&
          node->Attr(QName("", "session-state")) == "connected" &&
          node->Attr(QName("", "role")) == "host" &&
          node->Attr(QName("", "mode")) == "me2me" &&
          node->Attr(QName("", "connection-type")) == connection_type);
}

MATCHER_P(IsClientConnected, connection_type, "") {
  if (arg->Name() != QName(kJabberClientNamespace, "iq")) {
    return false;
  }
  buzz::XmlElement* log_stanza = arg->FirstChild()->AsElement();
  if (log_stanza->Name() !=QName(kChromotingNamespace, "log")) {
    return false;
  }
  if (log_stanza->NextChild()) {
    return false;
  }
  buzz::XmlElement* log_entry = log_stanza->FirstChild()->AsElement();
  if (!IsLogEntryForConnection(log_entry, connection_type)) {
    return false;
  }
  if (log_entry->NextChild()) {
    return false;
  }
  return true;
}

MATCHER_P2(IsTwoClientsConnected, connection_type1, connection_type2, "") {
  if (arg->Name() != QName(kJabberClientNamespace, "iq")) {
    return false;
  }
  buzz::XmlElement* log_stanza = arg->FirstChild()->AsElement();
  if (log_stanza->Name() !=QName(kChromotingNamespace, "log")) {
    return false;
  }
  if (log_stanza->NextChild()) {
    return false;
  }
  buzz::XmlElement* log_entry = log_stanza->FirstChild()->AsElement();
  if (!IsLogEntryForConnection(log_entry, connection_type1)) {
    return false;
  }
  log_entry = log_entry->NextChild()->AsElement();
  if (!IsLogEntryForConnection(log_entry, connection_type2)) {
    return false;
  }
  if (log_entry->NextChild()) {
    return false;
  }
  return true;
}

bool IsLogEntryForDisconnection(XmlElement* node) {
  return (node->Name() == QName(kChromotingNamespace, "entry") &&
          node->Attr(QName("", "event-name")) == "session-state" &&
          node->Attr(QName("", "session-state")) == "closed" &&
          node->Attr(QName("", "role")) == "host" &&
          node->Attr(QName("", "mode")) == "me2me");
}

MATCHER(IsClientDisconnected, "") {
  if (arg->Name() != QName(kJabberClientNamespace, "iq")) {
    return false;
  }
  buzz::XmlElement* log_stanza = arg->FirstChild()->AsElement();
  if (log_stanza->Name() !=QName(kChromotingNamespace, "log")) {
    return false;
  }
  if (log_stanza->NextChild()) {
    return false;
  }
  buzz::XmlElement* log_entry = log_stanza->FirstChild()->AsElement();
  if (!IsLogEntryForDisconnection(log_entry)) {
    return false;
  }
  if (log_entry->NextChild()) {
    return false;
  }
  return true;
}

}  // namespace

class LogToServerTest : public testing::Test {
 public:
  LogToServerTest() {}
  virtual void SetUp() OVERRIDE {
    message_loop_proxy_ = base::MessageLoopProxy::current();
    EXPECT_CALL(signal_strategy_, AddListener(_));
    log_to_server_.reset(
        new LogToServer(host_status_monitor_.AsWeakPtr(),
                        ServerLogEntry::ME2ME,
                        &signal_strategy_,
                        kTestBotJid));
    EXPECT_CALL(signal_strategy_, RemoveListener(_));
  }

 protected:
  MessageLoop message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  MockSignalStrategy signal_strategy_;
  scoped_ptr<LogToServer> log_to_server_;
  HostStatusMonitorFake host_status_monitor_;
};

TEST_F(LogToServerTest, SendNow) {
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return(kHostJid));
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(IsClientConnected("direct")))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitMainMessageLoop(&message_loop_))
        .RetiresOnSaturation();
  }
  log_to_server_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  protocol::TransportRoute route;
  route.type = protocol::TransportRoute::DIRECT;
  log_to_server_->OnClientRouteChange(kClientJid1, "video", route);
  log_to_server_->OnClientAuthenticated(kClientJid1);
  log_to_server_->OnClientConnected(kClientJid1);
  log_to_server_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  message_loop_.Run();
}

TEST_F(LogToServerTest, SendLater) {
  protocol::TransportRoute route;
  route.type = protocol::TransportRoute::DIRECT;
  log_to_server_->OnClientRouteChange(kClientJid1, "video", route);
  log_to_server_->OnClientAuthenticated(kClientJid1);
  log_to_server_->OnClientConnected(kClientJid1);
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return(kHostJid));
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(IsClientConnected("direct")))
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
  protocol::TransportRoute route1;
  route1.type = protocol::TransportRoute::DIRECT;
  log_to_server_->OnClientRouteChange(kClientJid1, "video", route1);
  log_to_server_->OnClientAuthenticated(kClientJid1);
  log_to_server_->OnClientConnected(kClientJid1);
  protocol::TransportRoute route2;
  route2.type = protocol::TransportRoute::STUN;
  log_to_server_->OnClientRouteChange(kClientJid2, "video", route2);
  log_to_server_->OnClientAuthenticated(kClientJid2);
  log_to_server_->OnClientConnected(kClientJid2);
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return(kHostJid));
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(
            IsTwoClientsConnected("direct", "stun")))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitMainMessageLoop(&message_loop_))
        .RetiresOnSaturation();
  }
  log_to_server_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  log_to_server_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  message_loop_.Run();
}

TEST_F(LogToServerTest, HandleRouteChangeInUnusualOrder) {
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return(kHostJid));
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(IsClientConnected("direct")))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(IsClientDisconnected()))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(IsClientConnected("stun")))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, RemoveListener(_))
        .WillOnce(QuitMainMessageLoop(&message_loop_))
        .RetiresOnSaturation();
  }
  log_to_server_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  protocol::TransportRoute route1;
  route1.type = protocol::TransportRoute::DIRECT;
  log_to_server_->OnClientRouteChange(kClientJid1, "video", route1);
  log_to_server_->OnClientAuthenticated(kClientJid1);
  log_to_server_->OnClientConnected(kClientJid1);
  protocol::TransportRoute route2;
  route2.type = protocol::TransportRoute::STUN;
  log_to_server_->OnClientRouteChange(kClientJid2, "video", route2);
  log_to_server_->OnClientDisconnected(kClientJid1);
  log_to_server_->OnClientAuthenticated(kClientJid2);
  log_to_server_->OnClientConnected(kClientJid2);
  log_to_server_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  message_loop_.Run();
}

}  // namespace remoting
