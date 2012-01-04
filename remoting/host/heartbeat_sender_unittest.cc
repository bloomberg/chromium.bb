// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include <set>

#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/in_memory_host_config.h"
#include "remoting/host/test_key_pair.h"
#include "remoting/jingle_glue/iq_sender.h"
#include "remoting/jingle_glue/mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

using buzz::QName;
using buzz::XmlElement;

using testing::_;
using testing::DeleteArg;
using testing::DoAll;
using testing::Invoke;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;

namespace remoting {

namespace {
const char kHostId[] = "0";
const char kTestJid[] = "user@gmail.com/chromoting123";
const int64 kTestTime = 123123123;
const char kStanzaId[] = "123";
}  // namespace

ACTION_P(AddListener, list) {
  list->insert(arg0);
}
ACTION_P(RemoveListener, list) {
  EXPECT_TRUE(list->find(arg0) != list->end());
  list->erase(arg0);
}

class HeartbeatSenderTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    config_ = new InMemoryHostConfig();
    config_->SetString(kHostIdConfigPath, kHostId);
    config_->SetString(kPrivateKeyConfigPath, kTestHostKeyPair);

    EXPECT_CALL(signal_strategy_, GetState())
        .WillOnce(Return(SignalStrategy::DISCONNECTED));
    EXPECT_CALL(signal_strategy_, AddListener(NotNull()))
        .WillRepeatedly(AddListener(&signal_strategy_listeners_));
    EXPECT_CALL(signal_strategy_, RemoveListener(NotNull()))
        .WillRepeatedly(RemoveListener(&signal_strategy_listeners_));
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return(kTestJid));

    heartbeat_sender_.reset(new HeartbeatSender());
    ASSERT_TRUE(heartbeat_sender_->Init(&signal_strategy_, config_));
  }

  virtual void TearDown() OVERRIDE {
    heartbeat_sender_.reset();
    EXPECT_TRUE(signal_strategy_listeners_.empty());
  }

  MessageLoop message_loop_;
  MockSignalStrategy signal_strategy_;
  std::set<SignalStrategy::Listener*> signal_strategy_listeners_;
  scoped_refptr<InMemoryHostConfig> config_;
  scoped_ptr<HeartbeatSender> heartbeat_sender_;
};

// Call Start() followed by Stop(), and makes sure an Iq stanza is
// being sent.
TEST_F(HeartbeatSenderTest, DoSendStanza) {
  XmlElement* sent_iq = NULL;
  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanza(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq), Return(true)));

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  message_loop_.RunAllPending();

  scoped_ptr<XmlElement> stanza(sent_iq);
  ASSERT_TRUE(stanza != NULL);

  EXPECT_EQ(stanza->Attr(buzz::QName("", "to")),
            std::string(kChromotingBotJid));
  EXPECT_EQ(stanza->Attr(buzz::QName("", "type")), "set");

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  message_loop_.RunAllPending();
}

// Validate format of the heartbeat stanza.
TEST_F(HeartbeatSenderTest, CreateHeartbeatMessage) {
  int64 start_time = static_cast<int64>(base::Time::Now().ToDoubleT());

  scoped_ptr<XmlElement> stanza(heartbeat_sender_->CreateHeartbeatMessage());
  ASSERT_TRUE(stanza.get() != NULL);

  EXPECT_TRUE(QName(kChromotingXmlNamespace, "heartbeat") ==
              stanza->Name());
  EXPECT_EQ(std::string(kHostId),
            stanza->Attr(QName(kChromotingXmlNamespace, "hostid")));

  QName signature_tag(kChromotingXmlNamespace, "signature");
  XmlElement* signature = stanza->FirstNamed(signature_tag);
  ASSERT_TRUE(signature != NULL);
  EXPECT_TRUE(stanza->NextNamed(signature_tag) == NULL);

  std::string time_str =
      signature->Attr(QName(kChromotingXmlNamespace, "time"));
  int64 time;
  EXPECT_TRUE(base::StringToInt64(time_str, &time));
  int64 now = static_cast<int64>(base::Time::Now().ToDoubleT());
  EXPECT_LE(start_time, time);
  EXPECT_GE(now, time);

  HostKeyPair key_pair;
  key_pair.LoadFromString(kTestHostKeyPair);
  std::string expected_signature =
      key_pair.GetSignature(std::string(kTestJid) + ' ' + time_str);
  EXPECT_EQ(expected_signature, signature->BodyText());
}

// Verify that ProcessResponse parses set-interval result.
TEST_F(HeartbeatSenderTest, ProcessResponse) {
  scoped_ptr<XmlElement> response(new XmlElement(buzz::QN_IQ));
  response->AddAttr(QName("", "type"), "result");

  XmlElement* result = new XmlElement(
      QName(kChromotingXmlNamespace, "heartbeat-result"));
  response->AddElement(result);

  XmlElement* set_interval = new XmlElement(
      QName(kChromotingXmlNamespace, "set-interval"));
  result->AddElement(set_interval);

  const int kTestInterval = 123;
  set_interval->AddText(base::IntToString(kTestInterval));

  heartbeat_sender_->ProcessResponse(response.get());

  EXPECT_EQ(kTestInterval * 1000, heartbeat_sender_->interval_ms_);
}

}  // namespace remoting
