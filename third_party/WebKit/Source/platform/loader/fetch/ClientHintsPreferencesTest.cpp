// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/ClientHintsPreferences.h"

#include "platform/http_names.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Time.h"
#include "public/platform/WebRuntimeFeatures.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ClientHintsPreferencesTest : public ::testing::Test {};

TEST_F(ClientHintsPreferencesTest, Basic) {
  struct TestCase {
    const char* header_value;
    bool expectation_resource_width;
    bool expectation_dpr;
    bool expectation_viewport_width;
  } cases[] = {
      {"width, dpr, viewportWidth", true, true, false},
      {"WiDtH, dPr,     viewport-width", true, true, true},
      {"WIDTH, DPR, VIWEPROT-Width", true, true, false},
      {"VIewporT-Width, wutwut, width", true, false, true},
      {"dprw", false, false, false},
      {"DPRW", false, false, false},
  };

  for (const auto& test_case : cases) {
    ClientHintsPreferences preferences;
    preferences.UpdateFromAcceptClientHintsHeader(test_case.header_value,
                                                  nullptr);
    EXPECT_EQ(
        test_case.expectation_resource_width,
        preferences.ShouldSend(mojom::WebClientHintsType::kResourceWidth));
    EXPECT_EQ(test_case.expectation_dpr,
              preferences.ShouldSend(mojom::WebClientHintsType::kDpr));
    EXPECT_EQ(
        test_case.expectation_viewport_width,
        preferences.ShouldSend(mojom::WebClientHintsType::kViewportWidth));

    // Calling UpdateFromAcceptClientHintsHeader with empty header should have
    // no impact on client hint preferences.
    preferences.UpdateFromAcceptClientHintsHeader("", nullptr);
    EXPECT_EQ(
        test_case.expectation_resource_width,
        preferences.ShouldSend(mojom::WebClientHintsType::kResourceWidth));
    EXPECT_EQ(test_case.expectation_dpr,
              preferences.ShouldSend(mojom::WebClientHintsType::kDpr));
    EXPECT_EQ(
        test_case.expectation_viewport_width,
        preferences.ShouldSend(mojom::WebClientHintsType::kViewportWidth));

    // Calling UpdateFromAcceptClientHintsHeader with an invalid header should
    // have no impact on client hint preferences.
    preferences.UpdateFromAcceptClientHintsHeader("foobar", nullptr);
    EXPECT_EQ(
        test_case.expectation_resource_width,
        preferences.ShouldSend(mojom::WebClientHintsType::kResourceWidth));
    EXPECT_EQ(test_case.expectation_dpr,
              preferences.ShouldSend(mojom::WebClientHintsType::kDpr));
    EXPECT_EQ(
        test_case.expectation_viewport_width,
        preferences.ShouldSend(mojom::WebClientHintsType::kViewportWidth));
  }
}

TEST_F(ClientHintsPreferencesTest, PersistentHints) {
  struct TestCase {
    bool enable_persistent_runtime_feature;
    bool use_https_url;
    const char* accept_ch_header_value;
    const char* accept_lifetime_header_value;
    int64_t expect_persist_duration_seconds;
  } test_cases[] = {
      {true, true, "width, dpr, viewportWidth", "", 0},
      {true, true, "width, dpr, viewportWidth", "-1000", 0},
      {true, true, "width, dpr, viewportWidth", "1000s", 0},
      {true, true, "width, dpr, viewportWidth", "1000.5", 0},
      {false, true, "width, dpr, viewportWidth", "1000", 0},
      {true, false, "width, dpr, viewportWidth", "1000", 0},
      {true, true, "width, dpr, viewportWidth", "1000", 1000},
  };

  for (const auto& test : test_cases) {
    WebRuntimeFeatures::EnableClientHintsPersistent(
        test.enable_persistent_runtime_feature);
    WebEnabledClientHints enabled_types;
    TimeDelta persist_duration;

    const KURL kurl(kParsedURLString,
                    test.use_https_url
                        ? String::FromUTF8("https://www.google.com/")
                        : String::FromUTF8("http://www.google.com/"));

    ResourceResponse response;
    response.SetHTTPHeaderField(HTTPNames::Accept_CH,
                                test.accept_ch_header_value);
    response.SetHTTPHeaderField(HTTPNames::Accept_CH_Lifetime,
                                test.accept_lifetime_header_value);
    response.SetURL(kurl);

    ClientHintsPreferences::UpdatePersistentHintsFromHeaders(
        response, nullptr, enabled_types, &persist_duration);
    EXPECT_EQ(test.expect_persist_duration_seconds,
              persist_duration.InSeconds());
    if (test.expect_persist_duration_seconds > 0) {
      EXPECT_FALSE(
          enabled_types.IsEnabled(mojom::WebClientHintsType::kDeviceMemory));
      EXPECT_TRUE(enabled_types.IsEnabled(mojom::WebClientHintsType::kDpr));
      EXPECT_TRUE(
          enabled_types.IsEnabled(mojom::WebClientHintsType::kResourceWidth));
      EXPECT_FALSE(
          enabled_types.IsEnabled(mojom::WebClientHintsType::kViewportWidth));
    } else {
      EXPECT_FALSE(
          enabled_types.IsEnabled(mojom::WebClientHintsType::kDeviceMemory));
      EXPECT_FALSE(enabled_types.IsEnabled(mojom::WebClientHintsType::kDpr));
      EXPECT_FALSE(
          enabled_types.IsEnabled(mojom::WebClientHintsType::kResourceWidth));
      EXPECT_FALSE(
          enabled_types.IsEnabled(mojom::WebClientHintsType::kViewportWidth));
    }
  }
}

}  // namespace blink
