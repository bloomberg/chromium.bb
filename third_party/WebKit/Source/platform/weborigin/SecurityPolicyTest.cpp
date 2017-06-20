/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/weborigin/SecurityPolicy.h"

#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(SecurityPolicyTest, EmptyReferrerForUnauthorizedScheme) {
  const KURL example_http_url = KURL(kParsedURLString, "http://example.com/");
  EXPECT_TRUE(String() == SecurityPolicy::GenerateReferrer(
                              kReferrerPolicyAlways, example_http_url,
                              String::FromUTF8("chrome://somepage/"))
                              .referrer);
}

TEST(SecurityPolicyTest, GenerateReferrerRespectsReferrerSchemesRegistry) {
  const KURL example_http_url = KURL(kParsedURLString, "http://example.com/");
  const String foobar_url = String::FromUTF8("foobar://somepage/");
  const String foobar_scheme = String::FromUTF8("foobar");

  EXPECT_EQ(String(), SecurityPolicy::GenerateReferrer(
                          kReferrerPolicyAlways, example_http_url, foobar_url)
                          .referrer);
  SchemeRegistry::RegisterURLSchemeAsAllowedForReferrer(foobar_scheme);
  EXPECT_EQ(foobar_url, SecurityPolicy::GenerateReferrer(
                            kReferrerPolicyAlways, example_http_url, foobar_url)
                            .referrer);
  SchemeRegistry::RemoveURLSchemeAsAllowedForReferrer(foobar_scheme);
}

TEST(SecurityPolicyTest, ShouldHideReferrerRespectsReferrerSchemesRegistry) {
  const KURL example_http_url = KURL(kParsedURLString, "http://example.com/");
  const KURL foobar_url = KURL(KURL(), "foobar://somepage/");
  const String foobar_scheme = String::FromUTF8("foobar");

  EXPECT_TRUE(SecurityPolicy::ShouldHideReferrer(example_http_url, foobar_url));
  SchemeRegistry::RegisterURLSchemeAsAllowedForReferrer(foobar_scheme);
  EXPECT_FALSE(
      SecurityPolicy::ShouldHideReferrer(example_http_url, foobar_url));
  SchemeRegistry::RemoveURLSchemeAsAllowedForReferrer(foobar_scheme);
}

TEST(SecurityPolicyTest, GenerateReferrer) {
  struct TestCase {
    ReferrerPolicy policy;
    const char* referrer;
    const char* destination;
    const char* expected;
  };

  const char kInsecureURLA[] = "http://a.test/path/to/file.html";
  const char kInsecureURLB[] = "http://b.test/path/to/file.html";
  const char kInsecureOriginA[] = "http://a.test/";

  const char kSecureURLA[] = "https://a.test/path/to/file.html";
  const char kSecureURLB[] = "https://b.test/path/to/file.html";
  const char kSecureOriginA[] = "https://a.test/";

  const char kBlobURL[] =
      "blob:http://a.test/b3aae9c8-7f90-440d-8d7c-43aa20d72fde";
  const char kFilesystemURL[] = "filesystem:http://a.test/path/t/file.html";

  TestCase inputs[] = {
      // HTTP -> HTTP: Same Origin
      {kReferrerPolicyAlways, kInsecureURLA, kInsecureURLA, kInsecureURLA},
      {kReferrerPolicyDefault, kInsecureURLA, kInsecureURLA, kInsecureURLA},
      {kReferrerPolicyNoReferrerWhenDowngrade, kInsecureURLA, kInsecureURLA,
       kInsecureURLA},
      {kReferrerPolicyNever, kInsecureURLA, kInsecureURLA, 0},
      {kReferrerPolicyOrigin, kInsecureURLA, kInsecureURLA, kInsecureOriginA},
      {kReferrerPolicyOriginWhenCrossOrigin, kInsecureURLA, kInsecureURLA,
       kInsecureURLA},
      {kReferrerPolicySameOrigin, kInsecureURLA, kInsecureURLA, kInsecureURLA},
      {kReferrerPolicyStrictOrigin, kInsecureURLA, kInsecureURLA,
       kInsecureOriginA},
      {kReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin,
       kInsecureURLA, kInsecureURLA, kInsecureURLA},

      // HTTP -> HTTP: Cross Origin
      {kReferrerPolicyAlways, kInsecureURLA, kInsecureURLB, kInsecureURLA},
      {kReferrerPolicyDefault, kInsecureURLA, kInsecureURLB, kInsecureURLA},
      {kReferrerPolicyNoReferrerWhenDowngrade, kInsecureURLA, kInsecureURLB,
       kInsecureURLA},
      {kReferrerPolicyNever, kInsecureURLA, kInsecureURLB, 0},
      {kReferrerPolicyOrigin, kInsecureURLA, kInsecureURLB, kInsecureOriginA},
      {kReferrerPolicyOriginWhenCrossOrigin, kInsecureURLA, kInsecureURLB,
       kInsecureOriginA},
      {kReferrerPolicySameOrigin, kInsecureURLA, kInsecureURLB, 0},
      {kReferrerPolicyStrictOrigin, kInsecureURLA, kInsecureURLB,
       kInsecureOriginA},
      {kReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin,
       kInsecureURLA, kInsecureURLB, kInsecureOriginA},

      // HTTPS -> HTTPS: Same Origin
      {kReferrerPolicyAlways, kSecureURLA, kSecureURLA, kSecureURLA},
      {kReferrerPolicyDefault, kSecureURLA, kSecureURLA, kSecureURLA},
      {kReferrerPolicyNoReferrerWhenDowngrade, kSecureURLA, kSecureURLA,
       kSecureURLA},
      {kReferrerPolicyNever, kSecureURLA, kSecureURLA, 0},
      {kReferrerPolicyOrigin, kSecureURLA, kSecureURLA, kSecureOriginA},
      {kReferrerPolicyOriginWhenCrossOrigin, kSecureURLA, kSecureURLA,
       kSecureURLA},
      {kReferrerPolicySameOrigin, kSecureURLA, kSecureURLA, kSecureURLA},
      {kReferrerPolicyStrictOrigin, kSecureURLA, kSecureURLA, kSecureOriginA},
      {kReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin, kSecureURLA,
       kSecureURLA, kSecureURLA},

      // HTTPS -> HTTPS: Cross Origin
      {kReferrerPolicyAlways, kSecureURLA, kSecureURLB, kSecureURLA},
      {kReferrerPolicyDefault, kSecureURLA, kSecureURLB, kSecureURLA},
      {kReferrerPolicyNoReferrerWhenDowngrade, kSecureURLA, kSecureURLB,
       kSecureURLA},
      {kReferrerPolicyNever, kSecureURLA, kSecureURLB, 0},
      {kReferrerPolicyOrigin, kSecureURLA, kSecureURLB, kSecureOriginA},
      {kReferrerPolicyOriginWhenCrossOrigin, kSecureURLA, kSecureURLB,
       kSecureOriginA},
      {kReferrerPolicySameOrigin, kSecureURLA, kSecureURLB, 0},
      {kReferrerPolicyStrictOrigin, kSecureURLA, kSecureURLB, kSecureOriginA},
      {kReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin, kSecureURLA,
       kSecureURLB, kSecureOriginA},

      // HTTP -> HTTPS
      {kReferrerPolicyAlways, kInsecureURLA, kSecureURLB, kInsecureURLA},
      {kReferrerPolicyDefault, kInsecureURLA, kSecureURLB, kInsecureURLA},
      {kReferrerPolicyNoReferrerWhenDowngrade, kInsecureURLA, kSecureURLB,
       kInsecureURLA},
      {kReferrerPolicyNever, kInsecureURLA, kSecureURLB, 0},
      {kReferrerPolicyOrigin, kInsecureURLA, kSecureURLB, kInsecureOriginA},
      {kReferrerPolicyOriginWhenCrossOrigin, kInsecureURLA, kSecureURLB,
       kInsecureOriginA},
      {kReferrerPolicySameOrigin, kInsecureURLA, kSecureURLB, 0},
      {kReferrerPolicyStrictOrigin, kInsecureURLA, kSecureURLB,
       kInsecureOriginA},
      {kReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin,
       kInsecureURLA, kSecureURLB, kInsecureOriginA},

      // HTTPS -> HTTP
      {kReferrerPolicyAlways, kSecureURLA, kInsecureURLB, kSecureURLA},
      {kReferrerPolicyDefault, kSecureURLA, kInsecureURLB, 0},
      {kReferrerPolicyNoReferrerWhenDowngrade, kSecureURLA, kInsecureURLB, 0},
      {kReferrerPolicyNever, kSecureURLA, kInsecureURLB, 0},
      {kReferrerPolicyOrigin, kSecureURLA, kInsecureURLB, kSecureOriginA},
      {kReferrerPolicyOriginWhenCrossOrigin, kSecureURLA, kSecureURLB,
       kSecureOriginA},
      {kReferrerPolicySameOrigin, kSecureURLA, kInsecureURLB, 0},
      {kReferrerPolicyStrictOrigin, kSecureURLA, kInsecureURLB, 0},
      {kReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin, kSecureURLA,
       kInsecureURLB, 0},

      // blob and filesystem URL handling
      {kReferrerPolicyAlways, kInsecureURLA, kBlobURL, 0},
      {kReferrerPolicyAlways, kBlobURL, kInsecureURLA, 0},
      {kReferrerPolicyAlways, kInsecureURLA, kFilesystemURL, 0},
      {kReferrerPolicyAlways, kFilesystemURL, kInsecureURLA, 0},
  };

  for (TestCase test : inputs) {
    KURL destination(kParsedURLString, test.destination);
    Referrer result = SecurityPolicy::GenerateReferrer(
        test.policy, destination, String::FromUTF8(test.referrer));
    if (test.expected) {
      EXPECT_EQ(String::FromUTF8(test.expected), result.referrer)
          << "'" << test.referrer << "' to '" << test.destination
          << "' should have been '" << test.expected << "': was '"
          << result.referrer.Utf8().data() << "'.";
    } else {
      EXPECT_TRUE(result.referrer.IsEmpty())
          << "'" << test.referrer << "' to '" << test.destination
          << "' should have been empty: was '" << result.referrer.Utf8().data()
          << "'.";
    }
    EXPECT_EQ(test.policy == kReferrerPolicyDefault
                  ? kReferrerPolicyNoReferrerWhenDowngrade
                  : test.policy,
              result.referrer_policy);
  }
}

TEST(SecurityPolicyTest, TrustworthyWhiteList) {
  const char* insecure_urls[] = {
      "http://a.test/path/to/file.html", "http://b.test/path/to/file.html",
      "blob:http://c.test/b3aae9c8-7f90-440d-8d7c-43aa20d72fde",
      "filesystem:http://d.test/path/t/file.html",
  };

  for (const char* url : insecure_urls) {
    RefPtr<SecurityOrigin> origin = SecurityOrigin::CreateFromString(url);
    EXPECT_FALSE(origin->IsPotentiallyTrustworthy());
    SecurityPolicy::AddOriginTrustworthyWhiteList(*origin);
    EXPECT_TRUE(origin->IsPotentiallyTrustworthy());
  }

  // Tests that adding URLs that have inner-urls to the whitelist
  // takes effect on the origins of the inner-urls (and vice versa).
  struct TestCase {
    const char* url;
    const char* another_url_in_origin;
  };
  TestCase insecure_urls_with_inner_origin[] = {
      {"blob:http://e.test/b3aae9c8-7f90-440d-8d7c-43aa20d72fde",
       "http://e.test/foo.html"},
      {"filesystem:http://f.test/path/t/file.html", "http://f.test/bar.html"},
      {"http://g.test/foo.html",
       "blob:http://g.test/b3aae9c8-7f90-440d-8d7c-43aa20d72fde"},
      {"http://h.test/bar.html", "filesystem:http://h.test/path/t/file.html"},
  };
  for (const TestCase& test : insecure_urls_with_inner_origin) {
    // Actually origins of both URLs should be same.
    RefPtr<SecurityOrigin> origin1 = SecurityOrigin::CreateFromString(test.url);
    RefPtr<SecurityOrigin> origin2 =
        SecurityOrigin::CreateFromString(test.another_url_in_origin);

    EXPECT_FALSE(origin1->IsPotentiallyTrustworthy());
    EXPECT_FALSE(origin2->IsPotentiallyTrustworthy());
    SecurityPolicy::AddOriginTrustworthyWhiteList(*origin1);
    EXPECT_TRUE(origin1->IsPotentiallyTrustworthy());
    EXPECT_TRUE(origin2->IsPotentiallyTrustworthy());
  }
}

}  // namespace blink
