// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/is_potentially_trustworthy.h"

#include "base/test/scoped_command_line.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

bool IsAllowlistedAsSecureOrigin(const url::Origin& origin) {
  return IsAllowlistedAsSecureOrigin(origin,
                                     network::GetSecureOriginAllowlist());
}

bool IsPotentiallyTrustworthy(const char* str) {
  return network::IsUrlPotentiallyTrustworthy(GURL(str));
}

TEST(IsPotentiallyTrustworthy, MainTest) {
  const url::Origin unique_origin;
  EXPECT_FALSE(network::IsOriginPotentiallyTrustworthy(unique_origin));
  const url::Origin opaque_origin =
      url::Origin::Create(GURL("https://www.example.com"))
          .DeriveNewOpaqueOrigin();
  EXPECT_FALSE(network::IsOriginPotentiallyTrustworthy(opaque_origin));

  EXPECT_TRUE(IsPotentiallyTrustworthy("about:blank"));
  EXPECT_TRUE(IsPotentiallyTrustworthy("about:blank#ref"));
  EXPECT_TRUE(IsPotentiallyTrustworthy("about:srcdoc"));

  EXPECT_FALSE(IsPotentiallyTrustworthy("data:test/plain;blah"));
  EXPECT_FALSE(IsPotentiallyTrustworthy("javascript:alert('blah')"));

  EXPECT_TRUE(IsPotentiallyTrustworthy("file:///test/fun.html"));
  EXPECT_TRUE(IsPotentiallyTrustworthy("file:///test/"));
  EXPECT_TRUE(IsPotentiallyTrustworthy("file://localhost/test/"));
  EXPECT_TRUE(IsPotentiallyTrustworthy("file://otherhost/test/"));

  EXPECT_TRUE(IsPotentiallyTrustworthy("https://example.com/fun.html"));
  EXPECT_FALSE(IsPotentiallyTrustworthy("http://example.com/fun.html"));

  EXPECT_TRUE(IsPotentiallyTrustworthy("wss://example.com/fun.html"));
  EXPECT_FALSE(IsPotentiallyTrustworthy("ws://example.com/fun.html"));

  EXPECT_TRUE(IsPotentiallyTrustworthy("http://localhost/fun.html"));
  EXPECT_TRUE(IsPotentiallyTrustworthy("http://pumpkin.localhost/fun.html"));
  EXPECT_TRUE(
      IsPotentiallyTrustworthy("http://crumpet.pumpkin.localhost/fun.html"));
  EXPECT_TRUE(
      IsPotentiallyTrustworthy("http://pumpkin.localhost:8080/fun.html"));
  EXPECT_TRUE(IsPotentiallyTrustworthy(
      "http://crumpet.pumpkin.localhost:3000/fun.html"));
  EXPECT_FALSE(IsPotentiallyTrustworthy("http://localhost.com/fun.html"));
  EXPECT_TRUE(IsPotentiallyTrustworthy("https://localhost.com/fun.html"));

  EXPECT_TRUE(IsPotentiallyTrustworthy("http://127.0.0.1/fun.html"));
  EXPECT_TRUE(IsPotentiallyTrustworthy("ftp://127.0.0.1/fun.html"));
  EXPECT_TRUE(IsPotentiallyTrustworthy("http://127.3.0.1/fun.html"));
  EXPECT_FALSE(IsPotentiallyTrustworthy("http://127.example.com/fun.html"));
  EXPECT_TRUE(IsPotentiallyTrustworthy("https://127.example.com/fun.html"));

  EXPECT_TRUE(IsPotentiallyTrustworthy("http://[::1]/fun.html"));
  EXPECT_FALSE(IsPotentiallyTrustworthy("http://[::2]/fun.html"));
  EXPECT_FALSE(IsPotentiallyTrustworthy("http://[::1].example.com/fun.html"));

  EXPECT_FALSE(
      IsPotentiallyTrustworthy("filesystem:http://www.example.com/temporary/"));
  EXPECT_FALSE(
      IsPotentiallyTrustworthy("filesystem:ftp://www.example.com/temporary/"));
  EXPECT_TRUE(
      IsPotentiallyTrustworthy("filesystem:ftp://127.0.0.1/temporary/"));
  EXPECT_TRUE(IsPotentiallyTrustworthy(
      "filesystem:https://www.example.com/temporary/"));

  EXPECT_FALSE(
      IsPotentiallyTrustworthy("blob:http://www.example.com/guid-goes-here"));
  EXPECT_FALSE(
      IsPotentiallyTrustworthy("blob:ftp://www.example.com/guid-goes-here"));
  EXPECT_TRUE(IsPotentiallyTrustworthy("blob:ftp://127.0.0.1/guid-goes-here"));
  EXPECT_TRUE(
      IsPotentiallyTrustworthy("blob:https://www.example.com/guid-goes-here"));
}

class SecureOriginAllowlistTest : public testing::Test {
  void TearDown() override {
    // Ensure that we reset the allowlisted origins without any flags applied.
    ResetSecureOriginAllowlistForTesting();
  }
};

TEST_F(SecureOriginAllowlistTest, UnsafelyTreatInsecureOriginAsSecure) {
  EXPECT_FALSE(IsAllowlistedAsSecureOrigin(
      url::Origin::Create(GURL("http://example.com/a.html"))));
  EXPECT_FALSE(IsAllowlistedAsSecureOrigin(
      url::Origin::Create(GURL("http://127.example.com/a.html"))));
  EXPECT_FALSE(IsPotentiallyTrustworthy("http://example.com/a.html"));
  EXPECT_FALSE(IsPotentiallyTrustworthy("http://127.example.com/a.html"));

  // Add http://example.com and http://127.example.com to allowlist by
  // command-line and see if they are now considered secure origins.
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(
      switches::kUnsafelyTreatInsecureOriginAsSecure,
      "http://example.com,http://127.example.com");
  ResetSecureOriginAllowlistForTesting();

  // They should be now allow-listed.
  EXPECT_TRUE(IsAllowlistedAsSecureOrigin(
      url::Origin::Create(GURL("http://example.com/a.html"))));
  EXPECT_TRUE(IsAllowlistedAsSecureOrigin(
      url::Origin::Create(GURL("http://127.example.com/a.html"))));
  EXPECT_TRUE(IsPotentiallyTrustworthy("http://example.com/a.html"));
  EXPECT_TRUE(IsPotentiallyTrustworthy("http://127.example.com/a.html"));

  // Check that similarly named sites are not considered secure.
  EXPECT_FALSE(IsPotentiallyTrustworthy("http://128.example.com/a.html"));
  EXPECT_FALSE(
      IsPotentiallyTrustworthy("http://foobar.127.example.com/a.html"));
}

TEST_F(SecureOriginAllowlistTest, HostnamePatterns) {
  const struct HostnamePatternCase {
    const char* pattern;
    const char* test_input;
    bool expected_secure;
  } kTestCases[] = {
      {"*.foo.com", "http://bar.foo.com", true},
      {"*.foo.*.bar.com", "http://a.foo.b.bar.com:8000", true},
      // For parsing/canonicalization simplicity, wildcard patterns can be
      // hostnames only, not full origins.
      {"http://*.foo.com", "http://bar.foo.com", false},
      {"*://foo.com", "http://foo.com", false},
      // Wildcards must be beyond eTLD+1.
      {"*.co.uk", "http://foo.co.uk", false},
      {"*.co.uk", "http://co.uk", false},
      {"*.baz", "http://foo.baz", false},
      {"foo.*.com", "http://foo.bar.com", false},
      {"*.foo.baz", "http://a.foo.baz", true},
      // Hostname patterns should be canonicalized.
      {"*.FoO.com", "http://a.foo.com", true},
      {"%2A.foo.com", "http://a.foo.com", false},
      // Hostname patterns must contain a wildcard and a wildcard can only
      // replace a component, not a part of a component.
      {"foo.com", "http://foo.com", false},
      {"test*.foo.com", "http://testblah.foo.com", false},
      {"*foo.com", "http://testfoo.com", false},
      {"foo*.com", "http://footest.com", false},
  };

  for (const auto& test : kTestCases) {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine* command_line =
        scoped_command_line.GetProcessCommandLine();
    command_line->AppendSwitchASCII(
        switches::kUnsafelyTreatInsecureOriginAsSecure, test.pattern);
    ResetSecureOriginAllowlistForTesting();
    GURL input_url(test.test_input);
    url::Origin input_origin = url::Origin::Create(input_url);
    EXPECT_EQ(test.expected_secure, IsAllowlistedAsSecureOrigin(input_origin));
    EXPECT_EQ(test.expected_secure, IsPotentiallyTrustworthy(test.test_input));
  }
}

}  // namespace network
