// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "components/update_client/protocol_serializer.h"
#include "components/update_client/protocol_serializer_xml.h"
#include "components/update_client/updater_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

using std::string;

namespace update_client {

TEST(SerializeRequestXml, Serialize) {
  // When no updater state is provided, then check that the elements and
  // attributes related to the updater state are not serialized.
  const auto request =
      std::make_unique<ProtocolSerializerXml>()->Serialize(MakeProtocolRequest(
          "{15160585-8ADE-4D3C-839B-1281A6035D1F}", "prod_id", "1.0", "lang",
          "channel", "OS", "cacheable", {{"extra", "params"}}, nullptr, {}));
  constexpr char regex[] =
      R"(<\?xml version="1\.0" encoding="UTF-8"\?>)"
      R"(<request protocol="3\.1" )"
      R"(dedup="cr" acceptformat="crx2,crx3" extra="params" )"
      R"(sessionid="{[-\w]{36}}" requestid="{[-\w]{36}}" )"
      R"(updater="prod_id" updaterversion="1\.0" prodversion="1\.0" )"
      R"(lang="lang" os="\w+" arch="\w+" nacl_arch="[-\w]+" (wow64="1" )?)"
      R"(updaterchannel="channel" prodchannel="channel" dlpref="cacheable">)"
      R"(<hw physmemory="\d+"/>)"
      R"(<os platform="OS" arch="[,-.\w]+" version="[-.\w]+"( sp="[\s\w]+")?/>)"
      R"(</request>)";
  EXPECT_TRUE(RE2::FullMatch(request, regex)) << request;
}

TEST(SerializeRequestXml, DownloadPreference) {
  // Verifies that an empty |download_preference| is not serialized.
  const auto serializer = std::make_unique<ProtocolSerializerXml>();
  auto request = serializer->Serialize(
      MakeProtocolRequest("{15160585-8ADE-4D3C-839B-1281A6035D1F}", "", "", "",
                          "", "", "", {}, nullptr, {}));
  EXPECT_FALSE(RE2::PartialMatch(request, " dlpref=")) << request;

  // Verifies that |download_preference| is serialized.
  request = serializer->Serialize(
      MakeProtocolRequest("{15160585-8ADE-4D3C-839B-1281A6035D1F}", "", "", "",
                          "", "", "cacheable", {}, nullptr, {}));
  EXPECT_TRUE(RE2::PartialMatch(request, R"( dlpref="cacheable")")) << request;
}

// When present, updater state attributes are only serialized for Google builds,
// except the |domainjoined| attribute, which is serialized in all cases.
TEST(SerializeRequestXml, UpdaterStateAttributes) {
  const auto serializer = std::make_unique<ProtocolSerializerXml>();
  UpdaterState::Attributes attributes;
  attributes["ismachine"] = "1";
  attributes["domainjoined"] = "1";
  attributes["name"] = "Omaha";
  attributes["version"] = "1.2.3.4";
  attributes["laststarted"] = "1";
  attributes["lastchecked"] = "2";
  attributes["autoupdatecheckenabled"] = "0";
  attributes["updatepolicy"] = "-1";
  const auto request = serializer->Serialize(MakeProtocolRequest(
      "{15160585-8ADE-4D3C-839B-1281A6035D1F}", "prod_id", "1.0", "lang",
      "channel", "OS", "cacheable", {{"extra", "params"}}, &attributes, {}));
  constexpr char regex[] =
      R"(<\?xml version="1\.0" encoding="UTF-8"\?>)"
      R"(<request protocol="3\.1" )"
      R"(dedup="cr" acceptformat="crx2,crx3" extra="params" )"
      R"(sessionid="{[-\w]{36}}" requestid="{[-\w]{36}}" )"
      R"(updater="prod_id" updaterversion="1\.0" prodversion="1\.0" )"
      R"(lang="lang" os="\w+" arch="\w+" nacl_arch="[-\w]+" (wow64="1" )?)"
      R"(updaterchannel="channel" prodchannel="channel" dlpref="cacheable" )"
      R"(domainjoined="1">)"
      R"(<hw physmemory="\d+"/>)"
      R"(<os platform="OS" arch="[,-.\w]+" version="[-.\w]+"( sp="[\s\w]+")?/>)"
#if defined(GOOGLE_CHROME_BUILD)
      R"(<updater name="Omaha" version="1\.2\.3\.4" lastchecked="2" )"
      R"(laststarted="1" ismachine="1" autoupdatecheckenabled="0" )"
      R"(updatepolicy="-1"/>)"
#endif  // GOOGLE_CHROME_BUILD
      R"(</request>)";

  EXPECT_TRUE(RE2::FullMatch(request, regex)) << request;
}

}  // namespace update_client
