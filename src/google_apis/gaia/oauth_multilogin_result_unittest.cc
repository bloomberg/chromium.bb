// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth_multilogin_result.h"

#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::net::CanonicalCookie;
using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Property;
using ::testing::_;

TEST(OAuthMultiloginResultTest, TryParseCookiesFromValue) {
  OAuthMultiloginResult result;
  // SID: typical response for a domain cookie
  // APISID: typical response for a host cookie
  // SSID: not canonical cookie because of the wrong path, should not be added
  // HSID: canonical but not valid because of the wrong host value, still will
  // be parsed but domain_ field will be empty. Also it is expired.
  std::string data =
      R"({
          "cookies":[
            {
              "name":"SID",
              "value":"vAlUe1",
              "domain":".google.ru",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            },
            {
              "name":"APISID",
              "value":"vAlUe2",
              "host":"google.com",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            },
            {
              "name":"SSID",
              "value":"vAlUe3",
              "domain":".google.de",
              "path":"path",
              "sSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            },
            {
              "name":"HSID",
              "value":"vAlUe4",
              "host":".google.fr",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":0
            }
          ]
        }
      )";

  std::unique_ptr<base::DictionaryValue> dictionary_value =
      base::DictionaryValue::From(base::JSONReader::Read(data));
  result.TryParseCookiesFromValue(dictionary_value.get());

  base::Time time_now = base::Time::Now();
  base::Time expiration_time =
      (time_now + base::TimeDelta::FromSecondsD(63070000.));
  double now = time_now.ToDoubleT();
  double expiration = expiration_time.ToDoubleT();
  const std::vector<CanonicalCookie> cookies = {
      CanonicalCookie("SID", "vAlUe1", ".google.ru", "/", time_now, time_now,
                      expiration_time, true, false,
                      net::CookieSameSite::NO_RESTRICTION,
                      net::CookiePriority::COOKIE_PRIORITY_HIGH),
      CanonicalCookie("APISID", "vAlUe2", "google.com", "/", time_now, time_now,
                      expiration_time, true, false,
                      net::CookieSameSite::NO_RESTRICTION,
                      net::CookiePriority::COOKIE_PRIORITY_HIGH),
      CanonicalCookie("HSID", "vAlUe4", "", "/", time_now, time_now, time_now,
                      true, false, net::CookieSameSite::NO_RESTRICTION,
                      net::CookiePriority::COOKIE_PRIORITY_HIGH)};

  EXPECT_EQ((int)result.cookies().size(), 3);

  EXPECT_TRUE(result.cookies()[0].IsEquivalent(cookies[0]));
  EXPECT_TRUE(result.cookies()[1].IsEquivalent(cookies[1]));
  EXPECT_TRUE(result.cookies()[2].IsEquivalent(cookies[2]));

  EXPECT_FALSE(result.cookies()[0].IsExpired(base::Time::Now()));
  EXPECT_FALSE(result.cookies()[1].IsExpired(base::Time::Now()));
  EXPECT_TRUE(result.cookies()[2].IsExpired(base::Time::Now()));

  EXPECT_THAT(
      result.cookies(),
      ElementsAre(Property(&CanonicalCookie::IsDomainCookie, Eq(true)),
                  Property(&CanonicalCookie::IsHostCookie, Eq(true)),
                  Property(&CanonicalCookie::IsDomainCookie, Eq(false))));
  EXPECT_THAT(result.cookies(),
              ElementsAre(Property(&CanonicalCookie::IsCanonical, Eq(true)),
                          Property(&CanonicalCookie::IsCanonical, Eq(true)),
                          Property(&CanonicalCookie::IsCanonical, Eq(true))));
  EXPECT_THAT(result.cookies(),
              ElementsAre(Property(&CanonicalCookie::IsHttpOnly, Eq(false)),
                          Property(&CanonicalCookie::IsHttpOnly, Eq(false)),
                          Property(&CanonicalCookie::IsHttpOnly, Eq(false))));
  EXPECT_THAT(result.cookies(),
              ElementsAre(Property(&CanonicalCookie::IsSecure, Eq(true)),
                          Property(&CanonicalCookie::IsSecure, Eq(true)),
                          Property(&CanonicalCookie::IsSecure, Eq(true))));
  EXPECT_THAT(result.cookies(),
              ElementsAre(Property(&CanonicalCookie::SameSite,
                                   Eq(net::CookieSameSite::NO_RESTRICTION)),
                          Property(&CanonicalCookie::SameSite,
                                   Eq(net::CookieSameSite::NO_RESTRICTION)),
                          Property(&CanonicalCookie::SameSite,
                                   Eq(net::CookieSameSite::NO_RESTRICTION))));
  EXPECT_THAT(
      result.cookies(),
      ElementsAre(Property(&CanonicalCookie::Priority,
                           Eq(net::CookiePriority::COOKIE_PRIORITY_HIGH)),
                  Property(&CanonicalCookie::Priority,
                           Eq(net::CookiePriority::COOKIE_PRIORITY_HIGH)),
                  Property(&CanonicalCookie::Priority,
                           Eq(net::CookiePriority::COOKIE_PRIORITY_HIGH))));

  EXPECT_THAT(result.cookies()[0].CreationDate().ToDoubleT(),
              DoubleNear(now, 0.5));
  EXPECT_THAT(result.cookies()[0].LastAccessDate().ToDoubleT(),
              DoubleNear(now, 0.5));
  EXPECT_THAT(result.cookies()[0].ExpiryDate().ToDoubleT(),
              DoubleNear(expiration, 0.5));
}

TEST(OAuthMultiloginResultTest, CreateOAuthMultiloginResultFromString) {
  OAuthMultiloginResult result1;
  EXPECT_TRUE(OAuthMultiloginResult::CreateOAuthMultiloginResultFromString(
      ")]}\'\n{}", &result1));
  EXPECT_TRUE(result1.cookies().empty());

  OAuthMultiloginResult result2;
  EXPECT_TRUE(OAuthMultiloginResult::CreateOAuthMultiloginResultFromString(
      "many_random_characters_before_newline\'\n{}", &result2));
  EXPECT_TRUE(result2.cookies().empty());

  OAuthMultiloginResult result3;
  EXPECT_FALSE(OAuthMultiloginResult::CreateOAuthMultiloginResultFromString(
      ")]}\'\n)]}'\n{}", &result3));
  EXPECT_TRUE(result3.cookies().empty());
}