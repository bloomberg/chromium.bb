// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include <set>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "remoting/base/constants.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/mock_signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"

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
const char kTestJid[] = "User@gmail.com/chromotingABC123";
const char kTestJidNormalized[] = "user@gmail.com/chromotingABC123";
const char kStanzaId[] = "123";
const int kTestInterval = 123;
const base::TimeDelta kTestTimeout = base::TimeDelta::FromSeconds(123);

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
  void SetUp() override {
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
    EXPECT_CALL(mock_unknown_host_id_error_callback_, Run())
        .Times(0);

    heartbeat_sender_.reset(
        new HeartbeatSender(mock_heartbeat_successful_callback_.Get(),
                            mock_unknown_host_id_error_callback_.Get(), kHostId,
                            &signal_strategy_, key_pair_, kTestBotJid));
  }

  void TearDown() override {
    heartbeat_sender_.reset();
    EXPECT_TRUE(signal_strategy_listeners_.empty());
  }

  void ValidateHeartbeatStanza(XmlElement* stanza,
                               const char* expected_sequence_id,
                               const char* expected_host_offline_reason);

  void ProcessResponseWithInterval(
    bool is_offline_heartbeat_response,
    int interval);

  base::MessageLoop message_loop_;
  MockSignalStrategy signal_strategy_;
  base::MockCallback<base::Closure> mock_heartbeat_successful_callback_;
  base::MockCallback<base::Closure> mock_unknown_host_id_error_callback_;
  std::set<SignalStrategy::Listener*> signal_strategy_listeners_;
  scoped_refptr<RsaKeyPair> key_pair_;
  std::unique_ptr<HeartbeatSender> heartbeat_sender_;
};

// Call Start() followed by Stop(), and make sure a valid heartbeat is sent.
TEST_F(HeartbeatSenderTest, DoSendStanza) {
  XmlElement* sent_iq = nullptr;
  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq), Return(true)));
  EXPECT_CALL(signal_strategy_, GetState())
      .WillRepeatedly(Return(SignalStrategy::CONNECTED));

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<XmlElement> stanza(sent_iq);
  ASSERT_TRUE(stanza != nullptr);
  ValidateHeartbeatStanza(stanza.get(), "0", nullptr);

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  base::RunLoop().RunUntilIdle();
}

// Call Start() followed by Stop(), twice, and make sure two valid heartbeats
// are sent, with the correct sequence IDs.
TEST_F(HeartbeatSenderTest, DoSendStanzaTwice) {
  XmlElement* sent_iq = nullptr;
  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq), Return(true)));
  EXPECT_CALL(signal_strategy_, GetState())
      .WillRepeatedly(Return(SignalStrategy::CONNECTED));

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<XmlElement> stanza(sent_iq);
  ASSERT_TRUE(stanza != nullptr);
  ValidateHeartbeatStanza(stanza.get(), "0", nullptr);

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

  std::unique_ptr<XmlElement> stanza2(sent_iq);
  ValidateHeartbeatStanza(stanza2.get(), "1", nullptr);

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  base::RunLoop().RunUntilIdle();
}

// Call Start() followed by Stop(), make sure a valid Iq stanza is sent,
// reply with an expected sequence ID, and make sure two valid heartbeats
// are sent, with the correct sequence IDs.
TEST_F(HeartbeatSenderTest, DoSendStanzaWithExpectedSequenceId) {
  XmlElement* sent_iq = nullptr;
  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq), Return(true)));
  EXPECT_CALL(signal_strategy_, GetState())
      .WillRepeatedly(Return(SignalStrategy::CONNECTED));

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<XmlElement> stanza(sent_iq);
  ASSERT_TRUE(stanza != nullptr);
  ValidateHeartbeatStanza(stanza.get(), "0", nullptr);

  XmlElement* sent_iq2 = nullptr;
  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId + 1));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq2), Return(true)));

  std::unique_ptr<XmlElement> response(new XmlElement(buzz::QN_IQ));
  response->AddAttr(QName(std::string(), "type"), "result");
  XmlElement* result =
      new XmlElement(QName(kChromotingXmlNamespace, "heartbeat-result"));
  response->AddElement(result);
  XmlElement* expected_sequence_id = new XmlElement(
      QName(kChromotingXmlNamespace, "expected-sequence-id"));
  result->AddElement(expected_sequence_id);
  const int kExpectedSequenceId = 456;
  expected_sequence_id->AddText(base::IntToString(kExpectedSequenceId));
  heartbeat_sender_->ProcessResponse(false, nullptr, response.get());
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<XmlElement> stanza2(sent_iq2);
  ASSERT_TRUE(stanza2 != nullptr);
  ValidateHeartbeatStanza(stanza2.get(),
                          base::IntToString(kExpectedSequenceId).c_str(),
                          nullptr);

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  base::RunLoop().RunUntilIdle();
}

void HeartbeatSenderTest::ProcessResponseWithInterval(
    bool is_offline_heartbeat_response,
    int interval) {
  std::unique_ptr<XmlElement> response(new XmlElement(buzz::QN_IQ));
  response->AddAttr(QName(std::string(), "type"), "result");

  XmlElement* result = new XmlElement(
      QName(kChromotingXmlNamespace, "heartbeat-result"));
  response->AddElement(result);

  XmlElement* set_interval = new XmlElement(
      QName(kChromotingXmlNamespace, "set-interval"));
  result->AddElement(set_interval);

  set_interval->AddText(base::IntToString(interval));

  heartbeat_sender_->ProcessResponse(
      is_offline_heartbeat_response, nullptr, response.get());
}

// Verify that ProcessResponse parses set-interval result.
TEST_F(HeartbeatSenderTest, ProcessResponseSetInterval) {
  EXPECT_CALL(mock_heartbeat_successful_callback_, Run());

  ProcessResponseWithInterval(false, kTestInterval);

  EXPECT_EQ(kTestInterval * 1000, heartbeat_sender_->interval_ms_);
}

// Make sure SetHostOfflineReason sends a correct stanza.
TEST_F(HeartbeatSenderTest, DoSetHostOfflineReason) {
  XmlElement* sent_iq = nullptr;
  base::MockCallback<base::Callback<void(bool success)>> mock_ack_callback;

  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq), Return(true)));
  EXPECT_CALL(signal_strategy_, GetState())
      .WillOnce(Return(SignalStrategy::DISCONNECTED))
      .WillRepeatedly(Return(SignalStrategy::CONNECTED));
  EXPECT_CALL(mock_ack_callback, Run(_)).Times(0);

  heartbeat_sender_->SetHostOfflineReason("test_error", kTestTimeout,
                                          mock_ack_callback.Get());
  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<XmlElement> stanza(sent_iq);
  ASSERT_TRUE(stanza != nullptr);
  ValidateHeartbeatStanza(stanza.get(), "0", "test_error");

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  base::RunLoop().RunUntilIdle();
}

// Make sure SetHostOfflineReason triggers a callback when bot responds.
TEST_F(HeartbeatSenderTest, ProcessHostOfflineResponses) {
  base::MockCallback<base::Callback<void(bool success)>> mock_ack_callback;

  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  EXPECT_CALL(signal_strategy_, GetState())
      .WillOnce(Return(SignalStrategy::DISCONNECTED))
      .WillRepeatedly(Return(SignalStrategy::CONNECTED));
  EXPECT_CALL(mock_heartbeat_successful_callback_, Run())
      .WillRepeatedly(Return());

  // Callback should not run, until response to offline-reason.
  EXPECT_CALL(mock_ack_callback, Run(_)).Times(0);

  heartbeat_sender_->SetHostOfflineReason("test_error", kTestTimeout,
                                          mock_ack_callback.Get());
  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  base::RunLoop().RunUntilIdle();

  ProcessResponseWithInterval(
      false,  // <- This is not a response to offline-reason.
      kTestInterval);
  base::RunLoop().RunUntilIdle();

  // Callback should run once, when we get response to offline-reason.
  EXPECT_CALL(mock_ack_callback, Run(true /* success */)).Times(1);
  ProcessResponseWithInterval(
      true,  // <- This is a response to offline-reason.
      kTestInterval);
  base::RunLoop().RunUntilIdle();

  // When subsequent responses to offline-reason come,
  // the callback should not be called again.
  EXPECT_CALL(mock_ack_callback, Run(_)).Times(0);
  ProcessResponseWithInterval(true, kTestInterval);
  base::RunLoop().RunUntilIdle();

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  base::RunLoop().RunUntilIdle();
}

// The first heartbeat should include host OS information.
TEST_F(HeartbeatSenderTest, HostOsInfo) {
  XmlElement* sent_iq = nullptr;
  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq), Return(true)));
  EXPECT_CALL(signal_strategy_, GetState())
      .WillRepeatedly(Return(SignalStrategy::CONNECTED));

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<XmlElement> stanza(sent_iq);
  ASSERT_TRUE(stanza != nullptr);

  XmlElement* heartbeat_stanza =
      stanza->FirstNamed(QName(kChromotingXmlNamespace, "heartbeat"));

  std::string host_os_name =
      heartbeat_stanza->TextNamed(
          QName(kChromotingXmlNamespace, "host-os-name"));
  EXPECT_TRUE(!host_os_name.empty());

  std::string host_os_version =
      heartbeat_stanza->TextNamed(
          QName(kChromotingXmlNamespace, "host-os-version"));
  EXPECT_TRUE(!host_os_version.empty());

  heartbeat_sender_->OnSignalStrategyStateChange(SignalStrategy::DISCONNECTED);
  base::RunLoop().RunUntilIdle();
}

// Validate a heartbeat stanza.
void HeartbeatSenderTest::ValidateHeartbeatStanza(
    XmlElement* stanza,
    const char* expected_sequence_id,
    const char* expected_host_offline_reason) {
  EXPECT_EQ(stanza->Attr(buzz::QName(std::string(), "to")),
            std::string(kTestBotJid));
  EXPECT_EQ(stanza->Attr(buzz::QName(std::string(), "type")), "set");
  XmlElement* heartbeat_stanza =
      stanza->FirstNamed(QName(kChromotingXmlNamespace, "heartbeat"));
  ASSERT_TRUE(heartbeat_stanza != nullptr);
  EXPECT_EQ(expected_sequence_id, heartbeat_stanza->Attr(
      buzz::QName(kChromotingXmlNamespace, "sequence-id")));
  if (expected_host_offline_reason == nullptr) {
    EXPECT_FALSE(heartbeat_stanza->HasAttr(
        buzz::QName(kChromotingXmlNamespace, "host-offline-reason")));
  } else {
    EXPECT_EQ(expected_host_offline_reason, heartbeat_stanza->Attr(
        buzz::QName(kChromotingXmlNamespace, "host-offline-reason")));
  }
  EXPECT_EQ(std::string(kHostId),
            heartbeat_stanza->Attr(QName(kChromotingXmlNamespace, "hostid")));

  QName signature_tag(kChromotingXmlNamespace, "signature");
  XmlElement* signature = heartbeat_stanza->FirstNamed(signature_tag);
  ASSERT_TRUE(signature != nullptr);
  EXPECT_TRUE(heartbeat_stanza->NextNamed(signature_tag) == nullptr);

  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::FromString(kTestRsaKeyPair);
  ASSERT_TRUE(key_pair.get());
  std::string expected_signature = key_pair->SignMessage(
      std::string(kTestJidNormalized) + ' ' + expected_sequence_id);
  EXPECT_EQ(expected_signature, signature->BodyText());
}

}  // namespace remoting
