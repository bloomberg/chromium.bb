// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/origin_util.h"

#include <memory>

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(OriginUtilTest, IsOriginSecureForUrlSchemes) {
  auto pref_service = std::make_unique<TestingPrefServiceSimple>();

  EXPECT_TRUE(
      IsOriginSecure(GURL("file:///test/fun.html"), pref_service.get()));
  EXPECT_TRUE(IsOriginSecure(GURL("file:///test/"), pref_service.get()));

  EXPECT_TRUE(
      IsOriginSecure(GURL("https://example.com/fun.html"), pref_service.get()));
  EXPECT_FALSE(
      IsOriginSecure(GURL("http://example.com/fun.html"), pref_service.get()));

  EXPECT_TRUE(
      IsOriginSecure(GURL("wss://example.com/fun.html"), pref_service.get()));
  EXPECT_FALSE(
      IsOriginSecure(GURL("ws://example.com/fun.html"), pref_service.get()));

  EXPECT_TRUE(
      IsOriginSecure(GURL("http://localhost/fun.html"), pref_service.get()));
  EXPECT_TRUE(IsOriginSecure(GURL("http://pumpkin.localhost/fun.html"),
                             pref_service.get()));
  EXPECT_TRUE(IsOriginSecure(GURL("http://crumpet.pumpkin.localhost/fun.html"),
                             pref_service.get()));
  EXPECT_TRUE(IsOriginSecure(GURL("http://pumpkin.localhost:8080/fun.html"),
                             pref_service.get()));
  EXPECT_TRUE(
      IsOriginSecure(GURL("http://crumpet.pumpkin.localhost:3000/fun.html"),
                     pref_service.get()));
  EXPECT_FALSE(IsOriginSecure(GURL("http://localhost.com/fun.html"),
                              pref_service.get()));
  EXPECT_TRUE(IsOriginSecure(GURL("https://localhost.com/fun.html"),
                             pref_service.get()));

  EXPECT_TRUE(
      IsOriginSecure(GURL("http://127.0.0.1/fun.html"), pref_service.get()));
  EXPECT_TRUE(
      IsOriginSecure(GURL("ftp://127.0.0.1/fun.html"), pref_service.get()));
  EXPECT_TRUE(
      IsOriginSecure(GURL("http://127.3.0.1/fun.html"), pref_service.get()));
  EXPECT_FALSE(IsOriginSecure(GURL("http://127.example.com/fun.html"),
                              pref_service.get()));
  EXPECT_TRUE(IsOriginSecure(GURL("https://127.example.com/fun.html"),
                             pref_service.get()));

  EXPECT_TRUE(
      IsOriginSecure(GURL("http://[::1]/fun.html"), pref_service.get()));
  EXPECT_FALSE(
      IsOriginSecure(GURL("http://[::2]/fun.html"), pref_service.get()));
  EXPECT_FALSE(IsOriginSecure(GURL("http://[::1].example.com/fun.html"),
                              pref_service.get()));

  EXPECT_FALSE(
      IsOriginSecure(GURL("filesystem:http://www.example.com/temporary/"),
                     pref_service.get()));
  EXPECT_FALSE(IsOriginSecure(
      GURL("filesystem:ftp://www.example.com/temporary/"), pref_service.get()));
  EXPECT_TRUE(IsOriginSecure(GURL("filesystem:ftp://127.0.0.1/temporary/"),
                             pref_service.get()));
  EXPECT_TRUE(
      IsOriginSecure(GURL("filesystem:https://www.example.com/temporary/"),
                     pref_service.get()));
}

TEST(OriginUtilTest, IsOriginSecureWithPolicyOverride) {
  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  pref_service->registry()->RegisterStringPref(
      prefs::kUnsafelyTreatInsecureOriginAsSecure, "");

  EXPECT_FALSE(IsOriginSecure(GURL("http://example.com"), pref_service.get()));

  pref_service->SetString(prefs::kUnsafelyTreatInsecureOriginAsSecure,
                          "http://example.com");
  EXPECT_TRUE(IsOriginSecure(GURL("http://example.com"), pref_service.get()));
  EXPECT_FALSE(IsOriginSecure(GURL("http://example.org"), pref_service.get()));

  pref_service->SetString(prefs::kUnsafelyTreatInsecureOriginAsSecure,
                          "http://example.com/, *.example.org");
  EXPECT_TRUE(IsOriginSecure(GURL("http://example.com"), pref_service.get()));
  EXPECT_TRUE(
      IsOriginSecure(GURL("http://www.example.org"), pref_service.get()));
  EXPECT_FALSE(IsOriginSecure(GURL("http://example.org"), pref_service.get()));
  EXPECT_FALSE(
      IsOriginSecure(GURL("http://othersite.com"), pref_service.get()));
}
