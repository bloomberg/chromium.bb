// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include <set>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/mock_signal_strategy.h"
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

const char kTestBotJid[] = "remotingunittest@bot.talk.google.com";
const char kHostId[] = "0";
const char kTestJid[] = "user@gmail.com/chromoting123";
const char kStanzaId[] = "123";

class MockListener : public HeartbeatSender::Listener {
 public:
  // Overridden from HeartbeatSender::Listener
  virtual void OnUnknownHostIdError() OVERRIDE {
    NOTREACHED();
  }

  // Overridden from HeartbeatSender::Listener
  MOCK_METHOD0(OnHeartbeatSuccessful, void());
};

}  // namespace

ACTION_P(AddListener, list) {
  list->insert(arg0);
}
ACTION_P(RemoveListener, list) {
  EXPECT_TRUE(list->find(arg0) != list->end());
  list->erase(arg0);
}

class HeartbeatSenderTest
    : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    key_pair_ = RsaKeyPair::FromString(kTestRsaKeyPair);
    ASSERT_TRUE(key_pair_.get());

    EXPECT_CALL(signal_strategy_, GetState())
        .WillOnce(Return(SignalStrategy::DISCONNECTED));
    EXPECT_CALL(signal_strategy_, AddListener(NotNull()))
        .WillRepeatedly(AddListener(&signal_strategy_listeners_));
    EXPECT_CALL(signal_strategy_, RemoveListener(NotNull()))
        .WillRepeatedly(RemoveListener(&signal_strategy_listeners_));
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return(kTestJid));

    heartbeat_sender_.reset(new HeartbeatSender(
        &mock_listener_, kHostId, &signal_strategy_, key_pair_, kTestBotJid));
  }

  virtual void TearDown() OVERRIDE {
    heartbeat_sender_.reset();
    EXPECT_TRUE(signal_strategy_listeners_.empty());
  }

  void ValidateHeartbeatStanza(XmlElement* stanza,
                               const char* expectedSequenceId);

  base::MessageLoop message_loop_;
  MockSignalStrategy signal_strategy_;
  MockListener mock_listener_;
  std::set<SignalStrategy::Listener*> signal_strategy_listeners_;
  scoped_refptr<RsaKeyPair> key_pair_;
  scoped_ptr<HeartbeatSender> heartbeat_sender_;
};

// Call Start() followed by Stop(), and make sure a valid heartbeat is sent.
TEST_F(HeartbeatSenderTest, DoSendStanza) {
  XmlElement* sent_iq = NULL;
  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq), Return(true)));

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  base::RunLoop().RunUntilIdle();

  scoped_ptr<XmlElement> stanza(sent_iq);
  ASSERT_TRUE(stanza != NULL);
  ValidateHeartbeatStanza(stanza.get(), "0");

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  base::RunLoop().RunUntilIdle();
}

// Call Start() followed by Stop(), twice, and make sure two valid heartbeats
// are sent, with the correct sequence IDs.
TEST_F(HeartbeatSenderTest, DoSendStanzaTwice) {
  XmlElement* sent_iq = NULL;
  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq), Return(true)));

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  base::RunLoop().RunUntilIdle();

  scoped_ptr<XmlElement> stanza(sent_iq);
  ASSERT_TRUE(stanza != NULL);
  ValidateHeartbeatStanza(stanza.get(), "0");

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId + 1));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq), Return(true)));

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  base::RunLoop().RunUntilIdle();

  scoped_ptr<XmlElement> stanza2(sent_iq);
  ValidateHeartbeatStanza(stanza2.get(), "1");

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  base::RunLoop().RunUntilIdle();
}

// Call Start() followed by Stop(), make sure a valid Iq stanza is sent,
// reply with an expected sequence ID, and make sure two valid heartbeats
// are sent, with the correct sequence IDs.
TEST_F(HeartbeatSenderTest, DoSendStanzaWithExpectedSequenceId) {
  XmlElement* sent_iq = NULL;
  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq), Return(true)));

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  base::RunLoop().RunUntilIdle();

  scoped_ptr<XmlElement> stanza(sent_iq);
  ASSERT_TRUE(stanza != NULL);
  ValidateHeartbeatStanza(stanza.get(), "0");

  XmlElement* sent_iq2 = NULL;
  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId + 1));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq2), Return(true)));
  EXPECT_CALL(mock_listener_, OnHeartbeatSuccessful());

  scoped_ptr<XmlElement> response(new XmlElement(buzz::QN_IQ));
  response->AddAttr(QName(std::string(), "type"), "result");
  XmlElement* result =
      new XmlElement(QName(kChromotingXmlNamespace, "heartbeat-result"));
  response->AddElement(result);
  XmlElement* expected_sequence_id = new XmlElement(
      QName(kChromotingXmlNamespace, "expected-sequence-id"));
  result->AddElement(expected_sequence_id);
  const int kExpectedSequenceId = 456;
  expected_sequence_id->AddText(base::IntToString(kExpectedSequenceId));
  heartbeat_sender_->ProcessResponse(NULL, response.get());
  base::RunLoop().RunUntilIdle();

  scoped_ptr<XmlElement> stanza2(sent_iq2);
  ASSERT_TRUE(stanza2 != NULL);
  ValidateHeartbeatStanza(stanza2.get(),
                          base::IntToString(kExpectedSequenceId).c_str());

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  base::RunLoop().RunUntilIdle();
}

// Verify that ProcessResponse parses set-interval result.
TEST_F(HeartbeatSenderTest, ProcessResponseSetInterval) {
  EXPECT_CALL(mock_listener_, OnHeartbeatSuccessful());

  scoped_ptr<XmlElement> response(new XmlElement(buzz::QN_IQ));
  response->AddAttr(QName(std::string(), "type"), "result");

  XmlElement* result = new XmlElement(
      QName(kChromotingXmlNamespace, "heartbeat-result"));
  response->AddElement(result);

  XmlElement* set_interval = new XmlElement(
      QName(kChromotingXmlNamespace, "set-interval"));
  result->AddElement(set_interval);

  const int kTestInterval = 123;
  set_interval->AddText(base::IntToString(kTestInterval));

  heartbeat_sender_->ProcessResponse(NULL, response.get());

  EXPECT_EQ(kTestInterval * 1000, heartbeat_sender_->interval_ms_);
}

// Validate a heartbeat stanza.
void HeartbeatSenderTest::ValidateHeartbeatStanza(
    XmlElement* stanza, const char* expectedSequenceId) {
  EXPECT_EQ(stanza->Attr(buzz::QName(std::string(), "to")),
            std::string(kTestBotJid));
  EXPECT_EQ(stanza->Attr(buzz::QName(std::string(), "type")), "set");
  XmlElement* heartbeat_stanza =
      stanza->FirstNamed(QName(kChromotingXmlNamespace, "heartbeat"));
  ASSERT_TRUE(heartbeat_stanza != NULL);
  EXPECT_EQ(expectedSequenceId, heartbeat_stanza->Attr(
      buzz::QName(kChromotingXmlNamespace, "sequence-id")));
  EXPECT_EQ(std::string(kHostId),
            heartbeat_stanza->Attr(QName(kChromotingXmlNamespace, "hostid")));

  QName signature_tag(kChromotingXmlNamespace, "signature");
  XmlElement* signature = heartbeat_stanza->FirstNamed(signature_tag);
  ASSERT_TRUE(signature != NULL);
  EXPECT_TRUE(heartbeat_stanza->NextNamed(signature_tag) == NULL);

  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::FromString(kTestRsaKeyPair);
  ASSERT_TRUE(key_pair.get());
  std::string expected_signature =
      key_pair->SignMessage(std::string(kTestJid) + ' ' + expectedSequenceId);
  EXPECT_EQ(expected_signature, signature->BodyText());
}

}  // namespace remoting
