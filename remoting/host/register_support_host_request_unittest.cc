// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/register_support_host_request.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "remoting/host/in_memory_host_config.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/mock_signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

using buzz::QName;
using buzz::XmlElement;

using testing::_;
using testing::Invoke;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;

namespace remoting {

namespace {
const char kTestBotJid[] = "remotingunittest@bot.talk.google.com";
const char kTestJid[] = "user@gmail.com/chromoting123";
const char kSupportId[] = "AB4RF3";
const char kSupportIdLifetime[] = "300";
const char kStanzaId[] = "123";

ACTION_P(AddListener, list) {
  list->AddObserver(arg0);
}
ACTION_P(RemoveListener, list) {
  list->RemoveObserver(arg0);
}

class MockCallback {
 public:
  MOCK_METHOD3(OnResponse, void(bool result, const std::string& support_id,
                                const base::TimeDelta& lifetime));
};

}  // namespace

class RegisterSupportHostRequestTest : public testing::Test {
 public:
 protected:
  virtual void SetUp() {
    key_pair_ = RsaKeyPair::FromString(kTestRsaKeyPair);
    ASSERT_TRUE(key_pair_.get());

    EXPECT_CALL(signal_strategy_, AddListener(NotNull()))
        .WillRepeatedly(AddListener(&signal_strategy_listeners_));
    EXPECT_CALL(signal_strategy_, RemoveListener(NotNull()))
        .WillRepeatedly(RemoveListener(&signal_strategy_listeners_));
    EXPECT_CALL(signal_strategy_, GetLocalJid())
        .WillRepeatedly(Return(kTestJid));
  }

  base::MessageLoop message_loop_;
  MockSignalStrategy signal_strategy_;
  ObserverList<SignalStrategy::Listener, true> signal_strategy_listeners_;
  scoped_refptr<RsaKeyPair> key_pair_;
  MockCallback callback_;
};

TEST_F(RegisterSupportHostRequestTest, Send) {
  // |iq_request| is freed by RegisterSupportHostRequest.
  int64 start_time = static_cast<int64>(base::Time::Now().ToDoubleT());

  scoped_ptr<RegisterSupportHostRequest> request(
      new RegisterSupportHostRequest(&signal_strategy_, key_pair_,
                                     kTestBotJid,
                                     base::Bind(&MockCallback::OnResponse,
                                                base::Unretained(&callback_))));

  XmlElement* sent_iq = NULL;
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq), Return(true)));

  request->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  base::RunLoop().RunUntilIdle();

  // Verify format of the query.
  scoped_ptr<XmlElement> stanza(sent_iq);
  ASSERT_TRUE(stanza != NULL);

  EXPECT_EQ(stanza->Attr(buzz::QName(std::string(), "to")),
            std::string(kTestBotJid));
  EXPECT_EQ(stanza->Attr(buzz::QName(std::string(), "type")), "set");

  EXPECT_EQ(QName(kChromotingXmlNamespace, "register-support-host"),
            stanza->FirstElement()->Name());

  QName signature_tag(kChromotingXmlNamespace, "signature");
  XmlElement* signature = stanza->FirstElement()->FirstNamed(signature_tag);
  ASSERT_TRUE(signature != NULL);
  EXPECT_TRUE(stanza->NextNamed(signature_tag) == NULL);

  std::string time_str =
      signature->Attr(QName(kChromotingXmlNamespace, "time"));
  int64 time;
  EXPECT_TRUE(base::StringToInt64(time_str, &time));
  int64 now = static_cast<int64>(base::Time::Now().ToDoubleT());
  EXPECT_LE(start_time, time);
  EXPECT_GE(now, time);

  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::FromString(kTestRsaKeyPair);
  ASSERT_TRUE(key_pair.get());

  std::string expected_signature =
      key_pair->SignMessage(std::string(kTestJid) + ' ' + time_str);
  EXPECT_EQ(expected_signature, signature->BodyText());

  // Generate response and verify that callback is called.
  EXPECT_CALL(callback_, OnResponse(true, kSupportId,
                                    base::TimeDelta::FromSeconds(300)));

  scoped_ptr<XmlElement> response(new XmlElement(buzz::QN_IQ));
  response->AddAttr(QName(std::string(), "from"), kTestBotJid);
  response->AddAttr(QName(std::string(), "type"), "result");
  response->AddAttr(QName(std::string(), "id"), kStanzaId);

  XmlElement* result = new XmlElement(
      QName(kChromotingXmlNamespace, "register-support-host-result"));
  response->AddElement(result);

  XmlElement* support_id = new XmlElement(
      QName(kChromotingXmlNamespace, "support-id"));
  support_id->AddText(kSupportId);
  result->AddElement(support_id);

  XmlElement* support_id_lifetime = new XmlElement(
      QName(kChromotingXmlNamespace, "support-id-lifetime"));
  support_id_lifetime->AddText(kSupportIdLifetime);
  result->AddElement(support_id_lifetime);

  int consumed = 0;
  ObserverListBase<SignalStrategy::Listener>::Iterator it(
      signal_strategy_listeners_);
  SignalStrategy::Listener* listener;
  while ((listener = it.GetNext()) != NULL) {
    if (listener->OnSignalStrategyIncomingStanza(response.get()))
      consumed++;
  }
  EXPECT_EQ(1, consumed);

  base::RunLoop().RunUntilIdle();
}

}  // namespace remoting
