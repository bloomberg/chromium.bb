// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_serializer_json.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/test_activity_data_service.h"
#include "components/update_client/updater_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"

using base::Value;
using std::string;

namespace update_client {

TEST(SerializeRequestJSON, Serialize) {
  // When no updater state is provided, then check that the elements and
  // attributes related to the updater state are not serialized.

  base::test::TaskEnvironment env;

  {
    auto pref = std::make_unique<TestingPrefServiceSimple>();
    PersistedData::RegisterPrefs(pref->registry());
    auto metadata = std::make_unique<PersistedData>(pref.get(), nullptr);
    std::vector<std::string> items = {"id1"};
    test::SetDateLastData(metadata.get(), items, 1234);

    std::vector<base::Value> events;
    events.emplace_back(Value::Type::DICTIONARY);
    events.emplace_back(Value::Type::DICTIONARY);
    events[0].SetKey("a", Value(1));
    events[0].SetKey("b", Value("2"));
    events[1].SetKey("error", Value(0));

    std::vector<protocol_request::App> apps;
    apps.push_back(MakeProtocolApp(
        "id1", base::Version("1.0"), "ap1", "BRND", "source1", "location1",
        "fp1", {{"attr1", "1"}, {"attr2", "2"}}, "c1", "ch1", "cn1", "test",
        {0, 1}, MakeProtocolUpdateCheck(true, "33.12", true, false),
        MakeProtocolPing("id1", metadata.get(), {}), std::move(events)));

    const auto request = std::make_unique<ProtocolSerializerJSON>()->Serialize(
        MakeProtocolRequest(false, "{15160585-8ADE-4D3C-839B-1281A6035D1F}",
                            "prod_id", "1.0", "lang", "channel", "OS",
                            "cacheable", {{"extra", "params"}}, nullptr,
                            std::move(apps)));
    constexpr char regex[] =
        R"({"request":{"@os":"\w+","@updater":"prod_id",)"
        R"("acceptformat":"crx3",)"
        R"("app":\[{"ap":"ap1","appid":"id1","attr1":"1","attr2":"2",)"
        R"("brand":"BRND","cohort":"c1","cohorthint":"ch1","cohortname":"cn1",)"
        R"("disabled":\[{"reason":0},{"reason":1}],"enabled":false,)"
        R"("event":\[{"a":1,"b":"2"},{"error":0}],)"
        R"("installedby":"location1","installsource":"source1",)"
        R"("packages":{"package":\[{"fp":"fp1"}]},)"
        R"("ping":{"ping_freshness":"{[-\w]{36}}","rd":1234},)"
        R"("release_channel":"test",)"
        R"("updatecheck":{"rollback_allowed":true,)"
        R"("targetversionprefix":"33.12",)"
        R"("updatedisabled":true},"version":"1.0"}],"arch":"\w+","dedup":"cr",)"
        R"("dlpref":"cacheable","extra":"params","hw":{"avx":(true|false),)"
        R"("physmemory":\d+,"sse":(true|false),"sse2":(true|false),)"
        R"("sse3":(true|false),"sse41":(true|false),"sse42":(true|false),)"
        R"("ssse3":(true|false)},)"
        R"("ismachine":false,"lang":"lang","nacl_arch":"[-\w]+",)"
        R"("os":{"arch":"[_,-.\w]+","platform":"OS",)"
        R"(("sp":"[\s\w]+",)?"version":"[+-.\w]+"},"prodchannel":"channel",)"
        R"("prodversion":"1.0","protocol":"3.1","requestid":"{[-\w]{36}}",)"
        R"("sessionid":"{[-\w]{36}}","updaterchannel":"channel",)"
        R"("updaterversion":"1.0"(,"wow64":true)?}})";
    EXPECT_TRUE(RE2::FullMatch(request, regex)) << request << "\n VS \n"
                                                << regex;
  }
  {
    // Tests `sameversionupdate` presence with a minimal request for one app.
    std::vector<protocol_request::App> apps;
    apps.push_back(MakeProtocolApp(
        "id1", base::Version("1.0"), "", "", "", "", "", {}, "", "", "", "", {},
        MakeProtocolUpdateCheck(false, "", false, true), absl::nullopt,
        absl::nullopt));

    const auto request = std::make_unique<ProtocolSerializerJSON>()->Serialize(
        MakeProtocolRequest(false, "{15160585-8ADE-4D3C-839B-1281A6035D1F}", "",
                            "", "", "", "", "", {}, nullptr, std::move(apps)));

    constexpr char regex[] =
        R"("app":\[{"appid":"id1","enabled":true,)"
        R"("updatecheck":{"sameversionupdate":true},"version":"1.0"}])";
    EXPECT_TRUE(RE2::PartialMatch(request, regex)) << request << "\n VS \n"
                                                   << regex;
  }
}

TEST(SerializeRequestJSON, DownloadPreference) {
  base::test::TaskEnvironment env;
  // Verifies that an empty |download_preference| is not serialized.
  const auto serializer = std::make_unique<ProtocolSerializerJSON>();
  auto request = serializer->Serialize(
      MakeProtocolRequest(false, "{15160585-8ADE-4D3C-839B-1281A6035D1F}", "",
                          "", "", "", "", "", {}, nullptr, {}));
  EXPECT_FALSE(RE2::PartialMatch(request, R"("dlpref":)")) << request;

  // Verifies that |download_preference| is serialized.
  request = serializer->Serialize(
      MakeProtocolRequest(false, "{15160585-8ADE-4D3C-839B-1281A6035D1F}", "",
                          "", "", "", "", "cacheable", {}, nullptr, {}));
  EXPECT_TRUE(RE2::PartialMatch(request, R"("dlpref":"cacheable")")) << request;
}

// When present, updater state attributes are only serialized for Google builds,
// except the |domainjoined| attribute, which is serialized in all cases.
TEST(SerializeRequestJSON, UpdaterStateAttributes) {
  base::test::TaskEnvironment env;
  const auto serializer = std::make_unique<ProtocolSerializerJSON>();
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
      true, "{15160585-8ADE-4D3C-839B-1281A6035D1F}", "prod_id", "1.0", "lang",
      "channel", "OS", "cacheable", {{"extra", "params"}}, &attributes, {}));
  constexpr char regex[] =
      R"({"request":{"@os":"\w+","@updater":"prod_id",)"
      R"("acceptformat":"crx3","arch":"\w+","dedup":"cr",)"
      R"("dlpref":"cacheable","domainjoined":true,"extra":"params",)"
      R"("hw":{"avx":(true|false),)"
      R"("physmemory":\d+,"sse":(true|false),"sse2":(true|false),)"
      R"("sse3":(true|false),"sse41":(true|false),"sse42":(true|false),)"
      R"("ssse3":(true|false)},)"
      R"("ismachine":true,"lang":"lang",)"
      R"("nacl_arch":"[-\w]+",)"
      R"("os":{"arch":"[,-.\w]+","platform":"OS",("sp":"[\s\w]+",)?)"
      R"("version":"[+-.\w]+"},"prodchannel":"channel","prodversion":"1.0",)"
      R"("protocol":"3.1","requestid":"{[-\w]{36}}","sessionid":"{[-\w]{36}}",)"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      R"("updater":{"autoupdatecheckenabled":false,"ismachine":true,)"
      R"("lastchecked":2,"laststarted":1,"name":"Omaha","updatepolicy":-1,)"
      R"("version":"1\.2\.3\.4"},)"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      R"("updaterchannel":"channel","updaterversion":"1.0"(,"wow64":true)?}})";
  EXPECT_TRUE(RE2::FullMatch(request, regex)) << request << "\n VS \n" << regex;
}

}  // namespace update_client
