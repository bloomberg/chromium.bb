// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/mock_signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;

using ::buzz::QName;
using ::buzz::XmlElement;

namespace remoting {

namespace {

const char kStanzaId[] = "123";
const char kNamespace[] = "chromium:testns";
const char kNamespacePrefix[] = "tes";
const char kBodyTag[] = "test";
const char kType[] = "get";
const char kTo[] = "user@domain.com";

class MockCallback {
 public:
  MOCK_METHOD2(OnReply, void(IqRequest* request, const XmlElement* reply));
};

MATCHER_P(XmlEq, expected, "") {
  return arg->Str() == expected->Str();
}

}  // namespace

class IqSenderTest : public testing::Test {
 public:
  IqSenderTest() {
    EXPECT_CALL(signal_strategy_, AddListener(NotNull()));
    sender_.reset(new IqSender(&signal_strategy_));
    EXPECT_CALL(signal_strategy_, RemoveListener(
        static_cast<SignalStrategy::Listener*>(sender_.get())));
  }

 protected:
  void SendTestMessage() {
    scoped_ptr<XmlElement> iq_body(
        new XmlElement(QName(kNamespace, kBodyTag)));
    XmlElement* sent_stanza;
    EXPECT_CALL(signal_strategy_, GetNextId())
        .WillOnce(Return(kStanzaId));
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(_))
        .WillOnce(DoAll(SaveArg<0>(&sent_stanza), Return(true)));
    request_ = sender_->SendIq(kType, kTo, iq_body.Pass(), base::Bind(
        &MockCallback::OnReply, base::Unretained(&callback_)));

    std::string expected_xml_string =
        base::StringPrintf(
            "<cli:iq type=\"%s\" to=\"%s\" id=\"%s\" "
            "xmlns:cli=\"jabber:client\">"
            "<%s:%s xmlns:%s=\"%s\"/>"
            "</cli:iq>",
            kType, kTo, kStanzaId, kNamespacePrefix, kBodyTag,
            kNamespacePrefix, kNamespace);
    EXPECT_EQ(expected_xml_string, sent_stanza->Str());
    delete sent_stanza;
  }

  base::MessageLoop message_loop_;
  MockSignalStrategy signal_strategy_;
  scoped_ptr<IqSender> sender_;
  MockCallback callback_;
  scoped_ptr<IqRequest> request_;
};

TEST_F(IqSenderTest, SendIq) {
  ASSERT_NO_FATAL_FAILURE({
    SendTestMessage();
  });

  scoped_ptr<XmlElement> response(new XmlElement(buzz::QN_IQ));
  response->AddAttr(QName(std::string(), "type"), "result");
  response->AddAttr(QName(std::string(), "id"), kStanzaId);
  response->AddAttr(QName(std::string(), "from"), kTo);

  XmlElement* result = new XmlElement(
      QName("test:namespace", "response-body"));
  response->AddElement(result);

  EXPECT_TRUE(sender_->OnSignalStrategyIncomingStanza(response.get()));

  EXPECT_CALL(callback_, OnReply(request_.get(), XmlEq(response.get())));
  base::RunLoop().RunUntilIdle();
}

TEST_F(IqSenderTest, Timeout) {
  ASSERT_NO_FATAL_FAILURE({
    SendTestMessage();
  });

  request_->SetTimeout(base::TimeDelta::FromMilliseconds(2));

  EXPECT_CALL(callback_, OnReply(request_.get(), NULL))
      .WillOnce(InvokeWithoutArgs(&message_loop_, &base::MessageLoop::Quit));
  message_loop_.Run();
}

TEST_F(IqSenderTest, InvalidFrom) {
  ASSERT_NO_FATAL_FAILURE({
    SendTestMessage();
  });

  scoped_ptr<XmlElement> response(new XmlElement(buzz::QN_IQ));
  response->AddAttr(QName(std::string(), "type"), "result");
  response->AddAttr(QName(std::string(), "id"), kStanzaId);
  response->AddAttr(QName(std::string(), "from"), "different_user@domain.com");

  XmlElement* result = new XmlElement(
      QName("test:namespace", "response-body"));
  response->AddElement(result);

  EXPECT_CALL(callback_, OnReply(_, _))
      .Times(0);
  EXPECT_FALSE(sender_->OnSignalStrategyIncomingStanza(response.get()));
  base::RunLoop().RunUntilIdle();
}

}  // namespace remoting
