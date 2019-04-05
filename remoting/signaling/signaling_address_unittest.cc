// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/signaling_address.h"

#include <memory>

#include "remoting/base/remoting_bot.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using jingle_xmpp::QName;

namespace remoting {

namespace {

std::unique_ptr<jingle_xmpp::XmlElement> GetEmptyJingleMessage() {
  return std::unique_ptr<jingle_xmpp::XmlElement>(jingle_xmpp::XmlElement::ForStr(
      "<iq xmlns='jabber:client'><jingle xmlns='urn:xmpp:jingle:1'/></iq>"));
}

constexpr char kLcsAddress[] =
    "user@domain.com/chromoting_lcs_KkMKIDB5NldsZndLalZZamZWVTZlYmhPT1RBa2p2TUl"
    "GX0lvEKT-HRhwIhB1V3QxYVkwdUptWlc3bnIxKVYHxmgZQ7i7";

constexpr char kFtlRegistrationId[] = "f6b43f10-566e-11e9-8647-d663bd873d93";

constexpr char kFtlAddress[] =
    "user@domain.com/chromoting_ftl_f6b43f10-566e-11e9-8647-d663bd873d93";

}  // namespace

TEST(SignalingAddressTest, ParseAddress) {
  const char kTestMessage[] =
      "<cli:iq from='remoting@bot.talk.google.com' "
      "      to='user@gmail.com/chromiumsy5C6A652D' type='set' "
      "      xmlns:cli='jabber:client'>"
      "  <jingle action='session-info' "
      "        sid='2227053353' xmlns='urn:xmpp:jingle:1' "
      "        from-channel='lcs' "
      "        from-endpoint-id='user@gmail.com/xBrnereror='>"
      "  </jingle>"
      "</cli:iq>";

  std::unique_ptr<jingle_xmpp::XmlElement> message(
      jingle_xmpp::XmlElement::ForStr(kTestMessage));
  std::string error;

  SignalingAddress from =
      SignalingAddress::Parse(message.get(), SignalingAddress::FROM, &error);
  EXPECT_FALSE(from.empty());
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("remoting@bot.talk.google.com", from.jid());
  EXPECT_EQ(SignalingAddress::Channel::LCS, from.channel());
  EXPECT_EQ("user@gmail.com/xBrnereror=", from.endpoint_id());
  EXPECT_EQ("user@gmail.com/xBrnereror=", from.id());

  SignalingAddress to =
      SignalingAddress::Parse(message.get(), SignalingAddress::TO, &error);
  EXPECT_FALSE(to.empty());
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("user@gmail.com/chromiumsy5C6A652D", to.jid());
  EXPECT_EQ(SignalingAddress::Channel::XMPP, to.channel());
  EXPECT_EQ("", to.endpoint_id());
  EXPECT_EQ("user@gmail.com/chromiumsy5C6A652D", to.id());
}

TEST(SignalingAddressTest, ParseErrorAddress) {
  const char kTestMessage[] =
      "<cli:iq from='remoting@bot.talk.google.com' "
      "to='user@gmail.com/chromiumsy5C6A652D' type='error' "
      "      xmlns:cli='jabber:client'>"
      "  <jingle action='session-info' "
      "        sid='2227053353' xmlns='urn:xmpp:jingle:1' "
      "        to-channel='lcs' "
      "        to-endpoint-id='user@gmail.com/xBrnereror='>"
      "  </jingle>"
      "<error/>"
      "</cli:iq>";

  std::unique_ptr<jingle_xmpp::XmlElement> message(
      jingle_xmpp::XmlElement::ForStr(kTestMessage));
  std::string error;

  SignalingAddress from =
      SignalingAddress::Parse(message.get(), SignalingAddress::FROM, &error);
  EXPECT_FALSE(from.empty());

  EXPECT_EQ("remoting@bot.talk.google.com", from.jid());
  EXPECT_EQ(SignalingAddress::Channel::LCS, from.channel());
  EXPECT_EQ("user@gmail.com/xBrnereror=", from.endpoint_id());
  EXPECT_EQ("user@gmail.com/xBrnereror=", from.id());

  SignalingAddress to =
      SignalingAddress::Parse(message.get(), SignalingAddress::TO, &error);
  EXPECT_FALSE(to.empty());

  EXPECT_EQ("user@gmail.com/chromiumsy5C6A652D", to.jid());
  EXPECT_EQ(SignalingAddress::Channel::XMPP, to.channel());
  EXPECT_EQ("", to.endpoint_id());
  EXPECT_EQ("user@gmail.com/chromiumsy5C6A652D", to.id());
}

TEST(SignalingAddressTest, ParseInvalidBotAddress) {
  // Parse a message with LCS address and invalid from field.
  const char kTestMessage[] =
      "<cli:iq from='invalid_address@google.com' "
      "to='user@gmail.com/chromiumsy5C6A652D' type='result' "
      "      xmlns:cli='jabber:client'>"
      "  <jingle action='session-info' "
      "        sid='2227053353' xmlns='urn:xmpp:jingle:1' "
      "        from-channel='lcs' "
      "        from-endpoint-id='user@gmail.com/xBrnereror='>"
      "  </jingle>"
      "</cli:iq>";

  std::unique_ptr<jingle_xmpp::XmlElement> message(
      jingle_xmpp::XmlElement::ForStr(kTestMessage));
  std::string error;

  SignalingAddress from =
      SignalingAddress::Parse(message.get(), SignalingAddress::FROM, &error);
  EXPECT_TRUE(from.empty());
  EXPECT_FALSE(error.empty());
}

TEST(SignalingAddressTest, ParseMissingEndpointId) {
  // Parse a message with a missing endpoint-id field.
  const char kTestMessage[] =
      "<cli:iq from='invalid_address@google.com' "
      "      to='user@gmail.com/chromiumsy5C6A652D' type='set' "
      "      xmlns:cli='jabber:client'>"
      "  <jingle action='session-info' "
      "        sid='2227053353' xmlns='urn:xmpp:jingle:1' "
      "        from-channel='lcs'>"
      "  </jingle>"
      "</cli:iq>";

  std::unique_ptr<jingle_xmpp::XmlElement> message(
      jingle_xmpp::XmlElement::ForStr(kTestMessage));
  std::string error;

  SignalingAddress from =
      SignalingAddress::Parse(message.get(), SignalingAddress::FROM, &error);
  EXPECT_TRUE(from.empty());
  EXPECT_FALSE(error.empty());
}

TEST(SignalingAddressTest, SetInMessageToXmpp) {
  std::unique_ptr<jingle_xmpp::XmlElement> message = GetEmptyJingleMessage();
  SignalingAddress addr("user@domain.com/chromoting12345");
  addr.SetInMessage(message.get(), SignalingAddress::TO);
  EXPECT_EQ("user@domain.com/chromoting12345", message->Attr(QName("", "to")));
  jingle_xmpp::XmlElement* jingle =
      message->FirstNamed(jingle_xmpp::QName("urn:xmpp:jingle:1", "jingle"));
  EXPECT_EQ("", jingle->Attr(QName("", "to-channel")));
  EXPECT_EQ("", jingle->Attr(QName("", "to-endpoint-id")));
}

TEST(SignalingAddressTest, SetInMessageToLcs) {
  std::unique_ptr<jingle_xmpp::XmlElement> message = GetEmptyJingleMessage();
  SignalingAddress addr(kLcsAddress);

  addr.SetInMessage(message.get(), SignalingAddress::TO);
  EXPECT_EQ(remoting::kRemotingBotJid, message->Attr(QName("", "to")));
  jingle_xmpp::XmlElement* jingle =
      message->FirstNamed(jingle_xmpp::QName("urn:xmpp:jingle:1", "jingle"));
  EXPECT_EQ("lcs", jingle->Attr(QName("", "to-channel")));
  EXPECT_EQ(kLcsAddress, jingle->Attr(QName("", "to-endpoint-id")));
}

TEST(SignalingAddressTest, SetInMessageToFtl) {
  std::unique_ptr<jingle_xmpp::XmlElement> message = GetEmptyJingleMessage();
  SignalingAddress addr(kFtlAddress);

  addr.SetInMessage(message.get(), SignalingAddress::TO);
  EXPECT_EQ(kFtlAddress, message->Attr(QName("", "to")));
  jingle_xmpp::XmlElement* jingle =
      message->FirstNamed(jingle_xmpp::QName("urn:xmpp:jingle:1", "jingle"));
  EXPECT_EQ("", jingle->Attr(QName("", "to-channel")));
  EXPECT_EQ("", jingle->Attr(QName("", "to-endpoint-id")));
}

TEST(SignalingAddressTest, SetInMessageFromXmpp) {
  std::unique_ptr<jingle_xmpp::XmlElement> message = GetEmptyJingleMessage();
  SignalingAddress addr("user@domain.com/resource");
  addr.SetInMessage(message.get(), SignalingAddress::FROM);
  EXPECT_EQ("user@domain.com/resource", message->Attr(QName("", "from")));
  jingle_xmpp::XmlElement* jingle =
      message->FirstNamed(jingle_xmpp::QName("urn:xmpp:jingle:1", "jingle"));
  EXPECT_EQ("", jingle->Attr(QName("", "from-channel")));
  EXPECT_EQ("", jingle->Attr(QName("", "from-endpoint-id")));
}

TEST(SignalingAddressTest, SetInMessageFromLcs) {
  std::unique_ptr<jingle_xmpp::XmlElement> message = GetEmptyJingleMessage();
  SignalingAddress addr(kLcsAddress);

  addr.SetInMessage(message.get(), SignalingAddress::FROM);
  EXPECT_EQ(kLcsAddress, message->Attr(QName("", "from")));
  jingle_xmpp::XmlElement* jingle =
      message->FirstNamed(jingle_xmpp::QName("urn:xmpp:jingle:1", "jingle"));
  EXPECT_EQ("lcs", jingle->Attr(QName("", "from-channel")));
  EXPECT_EQ(kLcsAddress, jingle->Attr(QName("", "from-endpoint-id")));
}

TEST(SignalingAddressTest, SetInMessageFromFtl) {
  std::unique_ptr<jingle_xmpp::XmlElement> message = GetEmptyJingleMessage();
  SignalingAddress addr(kFtlAddress);
  addr.SetInMessage(message.get(), SignalingAddress::FROM);
  EXPECT_EQ(kFtlAddress, message->Attr(QName("", "from")));
  jingle_xmpp::XmlElement* jingle =
      message->FirstNamed(jingle_xmpp::QName("urn:xmpp:jingle:1", "jingle"));
  EXPECT_EQ("", jingle->Attr(QName("", "from-channel")));
  EXPECT_EQ("", jingle->Attr(QName("", "from-endpoint-id")));
}

TEST(SignalingAddressTest, CreateFtlSignalingAddress) {
  SignalingAddress addr = SignalingAddress::CreateFtlSignalingAddress(
      "user@domain.com", kFtlRegistrationId);
  EXPECT_EQ(kFtlAddress, addr.jid());

  std::string username;
  std::string registration_id;
  EXPECT_TRUE(addr.GetFtlInfo(&username, &registration_id));
  EXPECT_EQ("user@domain.com", username);
  EXPECT_EQ(kFtlRegistrationId, registration_id);
}

TEST(SignalingAddressTest, GetFtlInfo_NotFtlInfo) {
  SignalingAddress addr(kLcsAddress);

  std::string username;
  std::string registration_id;
  EXPECT_FALSE(addr.GetFtlInfo(&username, &registration_id));
  EXPECT_TRUE(username.empty());
  EXPECT_TRUE(registration_id.empty());
}

}  // namespace remoting
