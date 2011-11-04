// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/stringprintf.h"
#include "remoting/jingle_glue/iq_sender.h"
#include "remoting/jingle_glue/mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

using ::testing::_;
using ::testing::DeleteArg;
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
  MOCK_METHOD1(OnReply, void(const XmlElement* reply));
};

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
  MockSignalStrategy signal_strategy_;
  scoped_ptr<IqSender> sender_;
  MockCallback callback_;
};

TEST_F(IqSenderTest, SendIq) {
  XmlElement* iq_body =
      new XmlElement(QName(kNamespace, kBodyTag));
  XmlElement* sent_stanza;
  EXPECT_CALL(signal_strategy_, GetNextId())
      .WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanza(_))
      .WillOnce(DoAll(SaveArg<0>(&sent_stanza), Return(true)));
  scoped_ptr<IqRequest> request(
      sender_->SendIq(kType, kTo, iq_body, base::Bind(
          &MockCallback::OnReply, base::Unretained(&callback_))));

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

  scoped_ptr<XmlElement> response(new XmlElement(buzz::QN_IQ));
  response->AddAttr(QName("", "type"), "result");
  response->AddAttr(QName("", "id"), kStanzaId);

  XmlElement* result = new XmlElement(
      QName("test:namespace", "response-body"));
  response->AddElement(result);

  EXPECT_CALL(callback_, OnReply(response.get()));
  EXPECT_TRUE(sender_->OnIncomingStanza(response.get()));
}

}  // namespace remoting
