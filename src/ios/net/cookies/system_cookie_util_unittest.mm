// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/system_cookie_util.h"

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "net/cookies/cookie_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

namespace {

const char kCookieDomain[] = "domain";
const char kCookieName[] = "name";
const char kCookiePath[] = "path/";
const char kCookieValue[] = "value";
const char kCookieValueInvalidUtf8[] = "\x81r\xe4\xbd\xa0\xe5\xa5\xbd";
const char kGetCookiesResultHistogram[] =
    "IOS.Cookies.GetCookiesForURLCallResult";

void CheckSystemCookie(const base::Time& expires, bool secure, bool httponly) {
  // Generate a canonical cookie.
  net::CanonicalCookie canonical_cookie(
      kCookieName, kCookieValue, kCookieDomain, kCookiePath,
      base::Time(),  // creation
      expires,
      base::Time(),  // last_access
      secure, httponly, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT);
  // Convert it to system cookie.
  NSHTTPCookie* system_cookie =
      SystemCookieFromCanonicalCookie(canonical_cookie);

  // Check the attributes.
  EXPECT_TRUE(system_cookie);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kCookieName), [system_cookie name]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kCookieValue), [system_cookie value]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kCookieDomain), [system_cookie domain]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kCookiePath), [system_cookie path]);
  EXPECT_EQ(secure, [system_cookie isSecure]);
  EXPECT_EQ(httponly, [system_cookie isHTTPOnly]);
  EXPECT_EQ(expires.is_null(), [system_cookie isSessionOnly]);
  // Allow 1 second difference as iOS rounds expiry time to the nearest second.
  base::Time system_cookie_expire_date = base::Time::FromDoubleT(
      [[system_cookie expiresDate] timeIntervalSince1970]);
  EXPECT_LE(expires - base::TimeDelta::FromSeconds(1),
            system_cookie_expire_date);
  EXPECT_GE(expires + base::TimeDelta::FromSeconds(1),
            system_cookie_expire_date);
}

void VerifyGetCookiesResultHistogram(
    const base::HistogramTester& histogram_tester,
    GetCookiesForURLCallResult expected_value) {
  histogram_tester.ExpectBucketCount(
      kGetCookiesResultHistogram,
      static_cast<base::HistogramBase::Sample>(expected_value), 1);
}

}  // namespace

using CookieUtil = PlatformTest;

TEST_F(CookieUtil, CanonicalCookieFromSystemCookie) {
  base::Time creation_time = base::Time::Now();
  base::Time expire_date = creation_time + base::TimeDelta::FromHours(2);
  NSDate* system_expire_date =
      [NSDate dateWithTimeIntervalSince1970:expire_date.ToDoubleT()];
  NSHTTPCookie* system_cookie = [[NSHTTPCookie alloc] initWithProperties:@{
    NSHTTPCookieDomain : @"foo",
    NSHTTPCookieName : @"a",
    NSHTTPCookiePath : @"/",
    NSHTTPCookieValue : @"b",
    NSHTTPCookieExpires : system_expire_date,
    @"HttpOnly" : @YES,
  }];
  ASSERT_TRUE(system_cookie);
  net::CanonicalCookie chrome_cookie =
      CanonicalCookieFromSystemCookie(system_cookie, creation_time);
  EXPECT_EQ("a", chrome_cookie.Name());
  EXPECT_EQ("b", chrome_cookie.Value());
  EXPECT_EQ("foo", chrome_cookie.Domain());
  EXPECT_EQ("/", chrome_cookie.Path());
  EXPECT_EQ(creation_time, chrome_cookie.CreationDate());
  EXPECT_TRUE(chrome_cookie.LastAccessDate().is_null());
  EXPECT_TRUE(chrome_cookie.IsPersistent());
  // Allow 1 second difference as iOS rounds expiry time to the nearest second.
  EXPECT_LE(expire_date - base::TimeDelta::FromSeconds(1),
            chrome_cookie.ExpiryDate());
  EXPECT_GE(expire_date + base::TimeDelta::FromSeconds(1),
            chrome_cookie.ExpiryDate());
  EXPECT_FALSE(chrome_cookie.IsSecure());
  EXPECT_TRUE(chrome_cookie.IsHttpOnly());
  EXPECT_EQ(net::COOKIE_PRIORITY_DEFAULT, chrome_cookie.Priority());

  // Test session and secure cookie.
  system_cookie = [[NSHTTPCookie alloc] initWithProperties:@{
    NSHTTPCookieDomain : @"foo",
    NSHTTPCookieName : @"a",
    NSHTTPCookiePath : @"/",
    NSHTTPCookieValue : @"b",
    NSHTTPCookieSecure : @"Y",
  }];
  ASSERT_TRUE(system_cookie);
  chrome_cookie = CanonicalCookieFromSystemCookie(system_cookie, creation_time);
  EXPECT_FALSE(chrome_cookie.IsPersistent());
  EXPECT_TRUE(chrome_cookie.IsSecure());
}

// Tests that histogram is reported correctly based on the input.
TEST_F(CookieUtil, ReportGetCookiesForURLResult) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kGetCookiesResultHistogram, 0);
  ReportGetCookiesForURLResult(SystemCookieStoreType::kNSHTTPSystemCookieStore,
                               /*has_cookies=*/true);
  VerifyGetCookiesResultHistogram(
      histogram_tester,
      GetCookiesForURLCallResult::kCookiesFoundOnNSHTTPSystemCookieStore);
  histogram_tester.ExpectTotalCount(kGetCookiesResultHistogram, 1);

  ReportGetCookiesForURLResult(SystemCookieStoreType::kNSHTTPSystemCookieStore,
                               /*has_cookies=*/false);
  VerifyGetCookiesResultHistogram(
      histogram_tester,
      GetCookiesForURLCallResult::kNoCookiesOnNSHTTPSystemCookieStore);
  histogram_tester.ExpectTotalCount(kGetCookiesResultHistogram, 2);

  ReportGetCookiesForURLResult(SystemCookieStoreType::kCookieMonster,
                               /*has_cookies=*/false);
  VerifyGetCookiesResultHistogram(
      histogram_tester, GetCookiesForURLCallResult::kNoCookiesOnCookieMonster);
  histogram_tester.ExpectTotalCount(kGetCookiesResultHistogram, 3);

  ReportGetCookiesForURLResult(SystemCookieStoreType::kWKHTTPSystemCookieStore,
                               /*has_cookies=*/true);
  VerifyGetCookiesResultHistogram(
      histogram_tester,
      GetCookiesForURLCallResult::kCookiesFoundOnWKHTTPSystemCookieStore);
  histogram_tester.ExpectTotalCount(kGetCookiesResultHistogram, 4);
}

TEST_F(CookieUtil, SystemCookieFromCanonicalCookie) {
  base::Time expire_date = base::Time::Now() + base::TimeDelta::FromHours(2);

  // Test various combinations of session, secure and httponly attributes.
  CheckSystemCookie(expire_date, false, false);
  CheckSystemCookie(base::Time(), true, false);
  CheckSystemCookie(expire_date, false, true);
  CheckSystemCookie(base::Time(), true, true);
}

TEST_F(CookieUtil, SystemCookieFromBadCanonicalCookie) {
  // Generate a bad canonical cookie (value is invalid utf8).
  net::CanonicalCookie bad_canonical_cookie(
      kCookieName, kCookieValueInvalidUtf8, kCookieDomain, kCookiePath,
      base::Time(),  // creation
      base::Time(),  // expires
      base::Time(),  // last_access
      false,         // secure
      false,         // httponly
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT);
  // Convert it to system cookie.
  NSHTTPCookie* system_cookie =
      SystemCookieFromCanonicalCookie(bad_canonical_cookie);
  EXPECT_TRUE(system_cookie == nil);
}

}  // namespace net
