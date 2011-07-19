// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/stringprintf.h"
#include "remoting/jingle_glue/iq_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {

TEST(IqRequestTest, MakeIqStanza) {
  const char* kMessageId = "0";
  const char* kNamespace = "chromium:testns";
  const char* kNamespacePrefix = "tes";
  const char* kBodyTag = "test";
  const char* kType = "get";
  const char* kTo = "user@domain.com";

  std::string expected_xml_string =
      base::StringPrintf(
          "<cli:iq type=\"%s\" to=\"%s\" id=\"%s\" "
          "xmlns:cli=\"jabber:client\">"
          "<%s:%s xmlns:%s=\"%s\"/>"
          "</cli:iq>",
          kType, kTo, kMessageId, kNamespacePrefix, kBodyTag,
          kNamespacePrefix, kNamespace);

  buzz::XmlElement* iq_body =
      new buzz::XmlElement(buzz::QName(kNamespace, kBodyTag));
  scoped_ptr<buzz::XmlElement> stanza(
      IqRequest::MakeIqStanza(kType, kTo, iq_body, kMessageId));

  EXPECT_EQ(expected_xml_string, stanza->Str());
}

}  // namespace remoting
