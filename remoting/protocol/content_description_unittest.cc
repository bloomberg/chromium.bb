// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/content_description.h"

#include <string>

#include "base/logging.h"
#include "remoting/protocol/authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

TEST(ContentDescriptionTest, FormatAndParse) {
  scoped_ptr<CandidateSessionConfig> config =
      CandidateSessionConfig::CreateDefault();
  ContentDescription description(
      config.Pass(), Authenticator::CreateEmptyAuthenticatorMessage());
  scoped_ptr<buzz::XmlElement> xml(description.ToXml());
  LOG(ERROR) << xml->Str();
  scoped_ptr<ContentDescription> parsed(
      ContentDescription::ParseXml(xml.get()));
  ASSERT_TRUE(parsed.get());
  EXPECT_TRUE(description.config()->control_configs() ==
              parsed->config()->control_configs());
  EXPECT_TRUE(description.config()->video_configs() ==
              parsed->config()->video_configs());
  EXPECT_TRUE(description.config()->event_configs() ==
              parsed->config()->event_configs());
  EXPECT_TRUE(description.config()->audio_configs() ==
              parsed->config()->audio_configs());
}

// Verify that we can still parse configs with transports that we don't
// recognize.
TEST(ContentDescriptionTest, ParseUnknown) {
  std::string kTestDescription =
      "<description xmlns=\"google:remoting\">"
      "  <control transport=\"stream\" version=\"2\"/>"
      "  <event transport=\"stream\" version=\"2\"/>"
      "  <event transport=\"new_awesome_transport\" version=\"3\"/>"
      "  <video transport=\"stream\" version=\"2\" codec=\"vp8\"/>"
      "  <authentication/>"
      "</description>";
  scoped_ptr<buzz::XmlElement> xml(buzz::XmlElement::ForStr(kTestDescription));
  scoped_ptr<ContentDescription> parsed(
      ContentDescription::ParseXml(xml.get()));
  ASSERT_TRUE(parsed.get());
  EXPECT_EQ(1U, parsed->config()->event_configs().size());
  EXPECT_TRUE(parsed->config()->event_configs().front() ==
              ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                            kDefaultStreamVersion,
                            ChannelConfig::CODEC_UNDEFINED));
}

// Verify that we can parse configs with none transport without version and
// codec.
TEST(ContentDescriptionTest, NoneTransport) {
  std::string kTestDescription =
      "<description xmlns=\"google:remoting\">"
      "  <control transport=\"stream\" version=\"2\"/>"
      "  <event transport=\"stream\" version=\"2\"/>"
      "  <event transport=\"stream\" version=\"2\"/>"
      "  <video transport=\"stream\" version=\"2\" codec=\"vp8\"/>"
      "  <audio transport=\"none\"/>"
      "  <authentication/>"
      "</description>";
  scoped_ptr<buzz::XmlElement> xml(buzz::XmlElement::ForStr(kTestDescription));
  scoped_ptr<ContentDescription> parsed(
      ContentDescription::ParseXml(xml.get()));
  ASSERT_TRUE(parsed.get());
  EXPECT_EQ(1U, parsed->config()->audio_configs().size());
  EXPECT_TRUE(parsed->config()->audio_configs().front() == ChannelConfig());
}

// Verify that we can parse configs with none transport with version and
// codec.
TEST(ContentDescriptionTest, NoneTransportWithCodec) {
  std::string kTestDescription =
      "<description xmlns=\"google:remoting\">"
      "  <control transport=\"stream\" version=\"2\"/>"
      "  <event transport=\"stream\" version=\"2\"/>"
      "  <event transport=\"stream\" version=\"2\"/>"
      "  <video transport=\"stream\" version=\"2\" codec=\"vp8\"/>"
      "  <audio transport=\"none\" version=\"2\" codec=\"verbatim\"/>"
      "  <authentication/>"
      "</description>";
  scoped_ptr<buzz::XmlElement> xml(buzz::XmlElement::ForStr(kTestDescription));
  scoped_ptr<ContentDescription> parsed(
      ContentDescription::ParseXml(xml.get()));
  ASSERT_TRUE(parsed.get());
  EXPECT_EQ(1U, parsed->config()->audio_configs().size());
  EXPECT_TRUE(parsed->config()->audio_configs().front() == ChannelConfig());
}

}  // namespace protocol
}  // namespace remoting

