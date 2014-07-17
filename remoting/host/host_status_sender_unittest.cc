// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_status_sender.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/signaling/mock_signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using buzz::QName;
using buzz::XmlElement;

using testing::DoAll;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;

namespace remoting {

namespace {

const char kTestBotJid[] = "remotingunittest@bot.talk.google.com";
const char kHostId[] = "0";
const char kTestJid[] = "user@gmail.com/chromoting123";
const char kStanzaId[] = "123";

const HostExitCodes kTestExitCode = kInvalidHostConfigurationExitCode;
const char kTestExitCodeString[] = "INVALID_HOST_CONFIGURATION";

}  // namespace

class HostStatusSenderTest
    : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    key_pair_ = RsaKeyPair::FromString(kTestRsaKeyPair);
    ASSERT_TRUE(key_pair_.get());

    host_status_sender_.reset(new HostStatusSender(
        kHostId, &signal_strategy_, key_pair_, kTestBotJid));
  }

  virtual void TearDown() OVERRIDE {
    host_status_sender_.reset();
  }

  void ValidateHostStatusStanza(XmlElement* stanza,
                                HostStatusSender::HostStatus status);

  void ValidateSignature(
      XmlElement* signature, HostStatusSender::HostStatus status);

  MockSignalStrategy signal_strategy_;
  scoped_refptr<RsaKeyPair> key_pair_;
  scoped_ptr<HostStatusSender> host_status_sender_;
};

TEST_F(HostStatusSenderTest, SendOnlineStatus) {
  XmlElement* sent_iq = NULL;
  EXPECT_CALL(signal_strategy_, GetState())
      .WillOnce(Return(SignalStrategy::DISCONNECTED))
      .WillRepeatedly(Return(SignalStrategy::CONNECTED));
  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq), Return(true)));

  // Call SendOnlineStatus twice. The first call should be a
  // no-op because |signal_strategy_| is diconnected.
  // So we expect SendStanza to be called only once.
  host_status_sender_->SendOnlineStatus();

  host_status_sender_->OnSignalStrategyStateChange(
      SignalStrategy::CONNECTED);
  host_status_sender_->SendOnlineStatus();

  scoped_ptr<XmlElement> stanza(sent_iq);

  ASSERT_TRUE(stanza != NULL);

  ValidateHostStatusStanza(stanza.get(), HostStatusSender::ONLINE);
}

TEST_F(HostStatusSenderTest, SendOfflineStatus) {
  XmlElement* sent_iq = NULL;
  EXPECT_CALL(signal_strategy_, GetState())
      .WillOnce(Return(SignalStrategy::DISCONNECTED))
      .WillRepeatedly(Return(SignalStrategy::CONNECTED));
  EXPECT_CALL(signal_strategy_, GetLocalJid())
      .WillRepeatedly(Return(kTestJid));
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq), Return(true)));

  // Call SendOfflineStatus twice. The first call should be a
  // no-op because |signal_strategy_| is diconnected.
  // So we expect SendStanza to be called only once.
  host_status_sender_->SendOfflineStatus(kTestExitCode);

  host_status_sender_->OnSignalStrategyStateChange(
      SignalStrategy::CONNECTED);
  host_status_sender_->SendOfflineStatus(kTestExitCode);

  scoped_ptr<XmlElement> stanza(sent_iq);

  ASSERT_TRUE(stanza != NULL);

  ValidateHostStatusStanza(stanza.get(), HostStatusSender::OFFLINE);
}

// Validate a host status stanza.
void HostStatusSenderTest::ValidateHostStatusStanza(
    XmlElement* stanza, HostStatusSender::HostStatus status) {
  EXPECT_EQ(stanza->Attr(QName(std::string(), "to")),
            std::string(kTestBotJid));
  EXPECT_EQ(stanza->Attr(QName(std::string(), "type")), "set");

  XmlElement* host_status_stanza =
      stanza->FirstNamed(QName(kChromotingXmlNamespace, "host-status"));
  ASSERT_TRUE(host_status_stanza != NULL);

  if (status == HostStatusSender::ONLINE) {
    EXPECT_EQ("ONLINE",
              host_status_stanza->Attr(
                  QName(kChromotingXmlNamespace, "status")));
    EXPECT_FALSE(host_status_stanza->HasAttr(
        QName(kChromotingXmlNamespace, "exit-code")));
  } else {
    EXPECT_EQ("OFFLINE",
              host_status_stanza->Attr(
                  QName(kChromotingXmlNamespace, "status")));
    EXPECT_EQ(kTestExitCodeString,
              host_status_stanza->Attr(
                  QName(kChromotingXmlNamespace, "exit-code")));
  }

  EXPECT_EQ(std::string(kHostId),
            host_status_stanza->Attr(
                QName(kChromotingXmlNamespace, "hostid")));

  QName signature_tag(kChromotingXmlNamespace, "signature");
  XmlElement* signature = host_status_stanza->FirstNamed(signature_tag);
  ASSERT_TRUE(signature != NULL);
  EXPECT_TRUE(host_status_stanza->NextNamed(signature_tag) == NULL);

  ValidateSignature(signature, status);
}

// Validate the signature.
void HostStatusSenderTest::ValidateSignature(
    XmlElement* signature, HostStatusSender::HostStatus status) {

  EXPECT_TRUE(signature->HasAttr(
      QName(kChromotingXmlNamespace, "time")));

  std::string time_str =
      signature->Attr(QName(kChromotingXmlNamespace, "time"));

  int64 time;
  ASSERT_TRUE(base::StringToInt64(time_str, &time));

  std::string message;
  message += kTestJid;
  message += " ";
  message += time_str;
  message += " ";

  if (status == HostStatusSender::OFFLINE) {
    message += "OFFLINE";
    message += " ";
    message += kTestExitCodeString;
  } else {
    message += "ONLINE";
  }

  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::FromString(kTestRsaKeyPair);
  ASSERT_TRUE(key_pair.get());

  std::string expected_signature =
      key_pair->SignMessage(message);
  EXPECT_EQ(expected_signature, signature->BodyText());
}

}  // namespace remoting
