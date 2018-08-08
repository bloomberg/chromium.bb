// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/cors.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

using CORSTest = testing::Test;

TEST_F(CORSTest, CheckAccessDetectsInvalidResponse) {
  base::Optional<CORSErrorStatus> error_status = cors::CheckAccess(
      GURL(), 0 /* response_status_code */,
      base::nullopt /* allow_origin_header */,
      base::nullopt /* allow_credentials_header */,
      network::mojom::FetchCredentialsMode::kOmit, url::Origin());
  ASSERT_TRUE(error_status);
  EXPECT_EQ(mojom::CORSError::kInvalidResponse, error_status->cors_error);
}

// Tests if cors::CheckAccess detects kWildcardOriginNotAllowed error correctly.
TEST_F(CORSTest, CheckAccessDetectsWildcardOriginNotAllowed) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;
  const std::string allow_all_header("*");

  // Access-Control-Allow-Origin '*' works.
  base::Optional<CORSErrorStatus> error1 =
      cors::CheckAccess(response_url, response_status_code,
                        allow_all_header /* allow_origin_header */,
                        base::nullopt /* allow_credentials_header */,
                        network::mojom::FetchCredentialsMode::kOmit, origin);
  EXPECT_FALSE(error1);

  // Access-Control-Allow-Origin '*' should not be allowed if credentials mode
  // is kInclude.
  base::Optional<CORSErrorStatus> error2 =
      cors::CheckAccess(response_url, response_status_code,
                        allow_all_header /* allow_origin_header */,
                        base::nullopt /* allow_credentials_header */,
                        network::mojom::FetchCredentialsMode::kInclude, origin);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CORSError::kWildcardOriginNotAllowed, error2->cors_error);
}

// Tests if cors::CheckAccess detects kMissingAllowOriginHeader error correctly.
TEST_F(CORSTest, CheckAccessDetectsMissingAllowOriginHeader) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  // Access-Control-Allow-Origin is missed.
  base::Optional<CORSErrorStatus> error =
      cors::CheckAccess(response_url, response_status_code,
                        base::nullopt /* allow_origin_header */,
                        base::nullopt /* allow_credentials_header */,
                        network::mojom::FetchCredentialsMode::kOmit, origin);
  ASSERT_TRUE(error);
  EXPECT_EQ(mojom::CORSError::kMissingAllowOriginHeader, error->cors_error);
}

// Tests if cors::CheckAccess detects kMultipleAllowOriginValues error
// correctly.
TEST_F(CORSTest, CheckAccessDetectsMultipleAllowOriginValues) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  const std::string space_separated_multiple_origins(
      "http://example.com http://another.example.com");
  base::Optional<CORSErrorStatus> error1 = cors::CheckAccess(
      response_url, response_status_code,
      space_separated_multiple_origins /* allow_origin_header */,
      base::nullopt /* allow_credentials_header */,
      network::mojom::FetchCredentialsMode::kOmit, origin);
  ASSERT_TRUE(error1);
  EXPECT_EQ(mojom::CORSError::kMultipleAllowOriginValues, error1->cors_error);

  const std::string comma_separated_multiple_origins(
      "http://example.com,http://another.example.com");
  base::Optional<CORSErrorStatus> error2 = cors::CheckAccess(
      response_url, response_status_code,
      comma_separated_multiple_origins /* allow_origin_header */,
      base::nullopt /* allow_credentials_header */,
      network::mojom::FetchCredentialsMode::kOmit, origin);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CORSError::kMultipleAllowOriginValues, error2->cors_error);
}

// Tests if cors::CheckAccess detects kInvalidAllowOriginValue error correctly.
TEST_F(CORSTest, CheckAccessDetectsInvalidAllowOriginValue) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  base::Optional<CORSErrorStatus> error =
      cors::CheckAccess(response_url, response_status_code,
                        std::string("invalid.origin") /* allow_origin_header */,
                        base::nullopt /* allow_credentials_header */,
                        network::mojom::FetchCredentialsMode::kOmit, origin);
  ASSERT_TRUE(error);
  EXPECT_EQ(mojom::CORSError::kInvalidAllowOriginValue, error->cors_error);
  EXPECT_EQ("invalid.origin", error->failed_parameter);
}

// Tests if cors::CheckAccess detects kAllowOriginMismatch error correctly.
TEST_F(CORSTest, CheckAccessDetectsAllowOriginMismatch) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  base::Optional<CORSErrorStatus> error1 =
      cors::CheckAccess(response_url, response_status_code,
                        origin.Serialize() /* allow_origin_header */,
                        base::nullopt /* allow_credentials_header */,
                        network::mojom::FetchCredentialsMode::kOmit, origin);
  ASSERT_FALSE(error1);

  base::Optional<CORSErrorStatus> error2 = cors::CheckAccess(
      response_url, response_status_code,
      std::string("http://not.google.com") /* allow_origin_header */,
      base::nullopt /* allow_credentials_header */,
      network::mojom::FetchCredentialsMode::kOmit, origin);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CORSError::kAllowOriginMismatch, error2->cors_error);
  EXPECT_EQ("http://not.google.com", error2->failed_parameter);

  // Allow "null" value to match serialized unique origins.
  const std::string null_string("null");
  const url::Origin null_origin;
  EXPECT_EQ(null_string, null_origin.Serialize());

  base::Optional<CORSErrorStatus> error3 = cors::CheckAccess(
      response_url, response_status_code, null_string /* allow_origin_header */,
      base::nullopt /* allow_credentials_header */,
      network::mojom::FetchCredentialsMode::kOmit, null_origin);
  EXPECT_FALSE(error3);
}

// Tests if cors::CheckAccess detects kInvalidAllowCredentials error correctly.
TEST_F(CORSTest, CheckAccessDetectsInvalidAllowCredential) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  base::Optional<CORSErrorStatus> error1 =
      cors::CheckAccess(response_url, response_status_code,
                        origin.Serialize() /* allow_origin_header */,
                        std::string("true") /* allow_credentials_header */,
                        network::mojom::FetchCredentialsMode::kInclude, origin);
  ASSERT_FALSE(error1);

  base::Optional<CORSErrorStatus> error2 =
      cors::CheckAccess(response_url, response_status_code,
                        origin.Serialize() /* allow_origin_header */,
                        std::string("fuga") /* allow_credentials_header */,
                        network::mojom::FetchCredentialsMode::kInclude, origin);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CORSError::kInvalidAllowCredentials, error2->cors_error);
  EXPECT_EQ("fuga", error2->failed_parameter);
}

// Tests if cors::CheckRedirectLocation detects kCORSDisabledScheme and
// kRedirectContainsCredentials errors correctly.
TEST_F(CORSTest, CheckRedirectLocation) {
  struct TestCase {
    GURL url;
    mojom::FetchRequestMode request_mode;
    bool cors_flag;
    bool tainted;
    base::Optional<CORSErrorStatus> expectation;
  };

  const auto kCORS = mojom::FetchRequestMode::kCORS;
  const auto kCORSWithForcedPreflight =
      mojom::FetchRequestMode::kCORSWithForcedPreflight;
  const auto kNoCORS = mojom::FetchRequestMode::kNoCORS;

  const url::Origin origin = url::Origin::Create(GURL("http://example.com/"));
  const GURL same_origin_url("http://example.com/");
  const GURL cross_origin_url("http://example2.com/");
  const GURL data_url("data:,Hello");
  const GURL same_origin_url_with_user("http://yukari@example.com/");
  const GURL same_origin_url_with_pass("http://:tamura@example.com/");
  const GURL cross_origin_url_with_user("http://yukari@example2.com/");
  const GURL cross_origin_url_with_pass("http://:tamura@example2.com/");
  const auto ok = base::nullopt;
  const CORSErrorStatus kCORSDisabledScheme(
      mojom::CORSError::kCORSDisabledScheme);
  const CORSErrorStatus kRedirectContainsCredentials(
      mojom::CORSError::kRedirectContainsCredentials);

  TestCase cases[] = {
      // "cors", no credentials information
      {same_origin_url, kCORS, false, false, ok},
      {cross_origin_url, kCORS, false, false, ok},
      {data_url, kCORS, false, false, ok},
      {same_origin_url, kCORS, true, false, ok},
      {cross_origin_url, kCORS, true, false, ok},
      {data_url, kCORS, true, false, ok},
      {same_origin_url, kCORS, false, true, ok},
      {cross_origin_url, kCORS, false, true, ok},
      {data_url, kCORS, false, true, ok},
      {same_origin_url, kCORS, true, true, ok},
      {cross_origin_url, kCORS, true, true, ok},
      {data_url, kCORS, true, true, ok},

      // "cors" with forced preflight, no credentials information
      {same_origin_url, kCORSWithForcedPreflight, false, false, ok},
      {cross_origin_url, kCORSWithForcedPreflight, false, false, ok},
      {data_url, kCORSWithForcedPreflight, false, false, ok},
      {same_origin_url, kCORSWithForcedPreflight, true, false, ok},
      {cross_origin_url, kCORSWithForcedPreflight, true, false, ok},
      {data_url, kCORSWithForcedPreflight, true, false, ok},
      {same_origin_url, kCORSWithForcedPreflight, false, true, ok},
      {cross_origin_url, kCORSWithForcedPreflight, false, true, ok},
      {data_url, kCORSWithForcedPreflight, false, true, ok},
      {same_origin_url, kCORSWithForcedPreflight, true, true, ok},
      {cross_origin_url, kCORSWithForcedPreflight, true, true, ok},
      {data_url, kCORSWithForcedPreflight, true, true, ok},

      // "no-cors", no credentials information
      {same_origin_url, kNoCORS, false, false, ok},
      {cross_origin_url, kNoCORS, false, false, ok},
      {data_url, kNoCORS, false, false, ok},
      {same_origin_url, kNoCORS, false, true, ok},
      {cross_origin_url, kNoCORS, false, true, ok},
      {data_url, kNoCORS, false, true, ok},

      // with credentials information (same origin)
      {same_origin_url_with_user, kCORS, false, false, ok},
      {same_origin_url_with_user, kCORS, true, false,
       kRedirectContainsCredentials},
      {same_origin_url_with_user, kCORS, true, true,
       kRedirectContainsCredentials},
      {same_origin_url_with_user, kNoCORS, false, false, ok},
      {same_origin_url_with_user, kNoCORS, false, true, ok},
      {same_origin_url_with_pass, kCORS, false, false, ok},
      {same_origin_url_with_pass, kCORS, true, false,
       kRedirectContainsCredentials},
      {same_origin_url_with_pass, kCORS, true, true,
       kRedirectContainsCredentials},
      {same_origin_url_with_pass, kNoCORS, false, false, ok},
      {same_origin_url_with_pass, kNoCORS, false, true, ok},

      // with credentials information (cross origin)
      {cross_origin_url_with_user, kCORS, false, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_user, kCORS, true, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_user, kCORS, true, true,
       kRedirectContainsCredentials},
      {cross_origin_url_with_user, kNoCORS, false, true, ok},
      {cross_origin_url_with_user, kNoCORS, false, false, ok},
      {cross_origin_url_with_pass, kCORS, false, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_pass, kCORS, true, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_pass, kCORS, true, true,
       kRedirectContainsCredentials},
      {cross_origin_url_with_pass, kNoCORS, false, true, ok},
      {cross_origin_url_with_pass, kNoCORS, false, false, ok},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "url: " << test.url
                 << ", request mode: " << test.request_mode
                 << ", origin: " << origin << ", cors_flag: " << test.cors_flag
                 << ", tainted: " << test.tainted);

    EXPECT_EQ(test.expectation,
              cors::CheckRedirectLocation(test.url, test.request_mode, origin,
                                          test.cors_flag, test.tainted));
  }
}

TEST_F(CORSTest, CheckPreflightDetectsErrors) {
  EXPECT_FALSE(cors::CheckPreflight(200));
  EXPECT_FALSE(cors::CheckPreflight(299));

  base::Optional<mojom::CORSError> error1 = cors::CheckPreflight(300);
  ASSERT_TRUE(error1);
  EXPECT_EQ(mojom::CORSError::kPreflightInvalidStatus, *error1);

  EXPECT_FALSE(cors::CheckExternalPreflight(std::string("true")));

  base::Optional<CORSErrorStatus> error2 =
      cors::CheckExternalPreflight(base::nullopt);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CORSError::kPreflightMissingAllowExternal,
            error2->cors_error);
  EXPECT_EQ("", error2->failed_parameter);

  base::Optional<CORSErrorStatus> error3 =
      cors::CheckExternalPreflight(std::string("TRUE"));
  ASSERT_TRUE(error3);
  EXPECT_EQ(mojom::CORSError::kPreflightInvalidAllowExternal,
            error3->cors_error);
  EXPECT_EQ("TRUE", error3->failed_parameter);
}

TEST_F(CORSTest, CheckCORSSafelist) {
  // Method check should be case-insensitive.
  EXPECT_TRUE(cors::IsCORSSafelistedMethod("get"));
  EXPECT_TRUE(cors::IsCORSSafelistedMethod("Get"));
  EXPECT_TRUE(cors::IsCORSSafelistedMethod("GET"));
  EXPECT_TRUE(cors::IsCORSSafelistedMethod("HEAD"));
  EXPECT_TRUE(cors::IsCORSSafelistedMethod("POST"));
  EXPECT_FALSE(cors::IsCORSSafelistedMethod("OPTIONS"));

  // Content-Type check should be case-insensitive, and should ignore spaces and
  // parameters such as charset after a semicolon.
  EXPECT_TRUE(
      cors::IsCORSSafelistedContentType("application/x-www-form-urlencoded"));
  EXPECT_TRUE(cors::IsCORSSafelistedContentType("multipart/form-data"));
  EXPECT_TRUE(cors::IsCORSSafelistedContentType("text/plain"));
  EXPECT_TRUE(cors::IsCORSSafelistedContentType("TEXT/PLAIN"));
  EXPECT_TRUE(cors::IsCORSSafelistedContentType("text/plain;charset=utf-8"));
  EXPECT_TRUE(cors::IsCORSSafelistedContentType(" text/plain ;charset=utf-8"));
  EXPECT_FALSE(cors::IsCORSSafelistedContentType("text/html"));

  // Header check should be case-insensitive. Value must be considered only for
  // Content-Type.
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("accept", "text/html"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("Accept-Language", "en"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("Content-Language", "ja"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("SAVE-DATA", "on"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("Intervention", ""));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("Cache-Control", ""));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("Content-Type", "text/plain"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("Content-Type", "image/png"));
}

TEST_F(CORSTest, CheckCORSClientHintsSafelist) {
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("device-memory", ""));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("device-memory", "abc"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("device-memory", "1.25"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("DEVICE-memory", "1.25"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("device-memory", "1.25-2.5"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("device-memory", "-1.25"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("device-memory", "1e2"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("device-memory", "inf"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("device-memory", "-2.3"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("device-memory", "NaN"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("DEVICE-memory", "1.25.3"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("DEVICE-memory", "1."));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("DEVICE-memory", ".1"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("DEVICE-memory", "."));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("DEVICE-memory", "1"));

  EXPECT_FALSE(cors::IsCORSSafelistedHeader("dpr", ""));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("dpr", "abc"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("dpr", "1.25"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("Dpr", "1.25"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("dpr", "1.25-2.5"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("dpr", "-1.25"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("dpr", "1e2"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("dpr", "inf"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("dpr", "-2.3"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("dpr", "NaN"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("dpr", "1.25.3"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("dpr", "1."));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("dpr", ".1"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("dpr", "."));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("dpr", "1"));

  EXPECT_FALSE(cors::IsCORSSafelistedHeader("width", ""));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("width", "abc"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("width", "125"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("width", "1"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("WIDTH", "125"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("width", "125.2"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("width", "-125"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("width", "2147483648"));

  EXPECT_FALSE(cors::IsCORSSafelistedHeader("viewport-width", ""));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("viewport-width", "abc"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("viewport-width", "125"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("viewport-width", "1"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("viewport-Width", "125"));
  EXPECT_FALSE(cors::IsCORSSafelistedHeader("viewport-width", "125.2"));
  EXPECT_TRUE(cors::IsCORSSafelistedHeader("viewport-width", "2147483648"));
}

}  // namespace

}  // namespace network
