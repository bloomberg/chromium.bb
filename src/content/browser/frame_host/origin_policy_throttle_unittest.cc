// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/origin_policy_throttle.h"

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_util.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class OriginPolicyThrottleTest : public RenderViewHostTestHarness,
                                 public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    // Some tests below should be run with the feature en- and disabled, since
    // they test the feature functionality when enabled and feature
    // non-funcionality (that is, that the feature is inert) when disabled.
    // Hence, we run this test in both variants.
    features_.InitWithFeatureState(features::kOriginPolicy, GetParam());

    RenderViewHostTestHarness::SetUp();
    OriginPolicyThrottle::GetKnownVersionsForTesting().clear();
  }
  void TearDown() override {
    OriginPolicyThrottle::GetKnownVersionsForTesting().clear();
    nav_handle_.reset();
    RenderViewHostTestHarness::TearDown();
  }
  bool enabled() {
    return base::FeatureList::IsEnabled(features::kOriginPolicy);
  }

  void CreateHandleFor(const GURL& url) {
    net::HttpRequestHeaders headers;
    if (OriginPolicyThrottle::ShouldRequestOriginPolicy(url, nullptr))
      headers.SetHeader(net::HttpRequestHeaders::kSecOriginPolicy, "0");

    nav_handle_ = std::make_unique<MockNavigationHandle>(web_contents());
    nav_handle_->set_url(url);
    nav_handle_->set_request_headers(headers);
  }

 protected:
  std::unique_ptr<MockNavigationHandle> nav_handle_;
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(OriginPolicyThrottleTests,
                         OriginPolicyThrottleTest,
                         testing::Bool());

TEST_P(OriginPolicyThrottleTest, ShouldRequestOriginPolicy) {
  struct {
    const char* url;
    bool expect;
  } test_cases[] = {
      {"https://example.org/bla", true},
      {"http://example.org/bla", false},
      {"file:///etc/passwd", false},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << "URL: " << test_case.url);
    EXPECT_EQ(enabled() && test_case.expect,
              OriginPolicyThrottle::ShouldRequestOriginPolicy(
                  GURL(test_case.url), nullptr));
  }
}

TEST_P(OriginPolicyThrottleTest, ShouldRequestLastKnownVersion) {
  if (!enabled())
    return;

  GURL url("https://example.org/bla");
  EXPECT_TRUE(OriginPolicyThrottle::ShouldRequestOriginPolicy(url, nullptr));

  std::string version;

  OriginPolicyThrottle::ShouldRequestOriginPolicy(url, &version);
  EXPECT_EQ(version, "0");

  OriginPolicyThrottle::GetKnownVersionsForTesting()[url::Origin::Create(url)] =
      "abcd";
  OriginPolicyThrottle::ShouldRequestOriginPolicy(url, &version);
  EXPECT_EQ(version, "abcd");
}

TEST_P(OriginPolicyThrottleTest, MaybeCreateThrottleFor) {
  CreateHandleFor(GURL("https://example.org/bla"));
  EXPECT_EQ(enabled(),
            !!OriginPolicyThrottle::MaybeCreateThrottleFor(nav_handle_.get()));

  CreateHandleFor(GURL("http://insecure.org/bla"));
  EXPECT_FALSE(
      !!OriginPolicyThrottle::MaybeCreateThrottleFor(nav_handle_.get()));
}

TEST_P(OriginPolicyThrottleTest, RunRequestEndToEnd) {
  if (!enabled())
    return;

  // Start the navigation.
  auto navigation = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.org/bla"), web_contents());
  navigation->SetAutoAdvance(false);
  navigation->Start();
  EXPECT_FALSE(navigation->IsDeferred());
  EXPECT_EQ(NavigationThrottle::PROCEED,
            navigation->GetLastThrottleCheckResult().action());

  // Fake a response with a policy header. Check whether the navigation
  // is deferred.
  const char* raw_headers =
      "HTTP/1.1 200 OK\nSec-Origin-Policy: policy=policy-1\n\n";
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders(
          net::HttpUtil::AssembleRawHeaders(raw_headers, strlen(raw_headers)));
  NavigationHandleImpl* nav_handle =
      static_cast<NavigationHandleImpl*>(navigation->GetNavigationHandle());
  nav_handle->set_response_headers_for_testing(headers);
  navigation->ReadyToCommit();
  EXPECT_TRUE(navigation->IsDeferred());

  // Set TestURLLoaderFactory because we could not make actual network requests
  // in this unit tests, but this test would make.
  OriginPolicyThrottle* policy_throttle = static_cast<OriginPolicyThrottle*>(
      nav_handle->GetDeferringThrottleForTesting());
  EXPECT_TRUE(policy_throttle);
  policy_throttle->SetURLLoaderFactoryForTesting(
      std::make_unique<network::TestURLLoaderFactory>());

  // For the purpose of this unit test we don't care about policy content,
  // only that it's non-empty. We check whether the throttle will pass it on.
  const char* policy = "{}";
  policy_throttle->InjectPolicyForTesting(policy);

  // At the end of the navigation, the navigation handle should have a copy
  // of the origin policy.
  EXPECT_EQ(policy,
            nav_handle->navigation_request()->common_params().origin_policy);
}

TEST_P(OriginPolicyThrottleTest, AddException) {
  if (!enabled())
    return;

  GURL url("https://example.org/bla");
  OriginPolicyThrottle::GetKnownVersionsForTesting()[url::Origin::Create(url)] =
      "abcd";

  std::string version;
  OriginPolicyThrottle::ShouldRequestOriginPolicy(url, &version);
  EXPECT_EQ(version, "abcd");

  OriginPolicyThrottle::AddExceptionFor(url);
  OriginPolicyThrottle::ShouldRequestOriginPolicy(url, &version);
  EXPECT_EQ(version, "0");
}

TEST_P(OriginPolicyThrottleTest, AddExceptionEndToEnd) {
  if (!enabled())
    return;

  OriginPolicyThrottle::AddExceptionFor(GURL("https://example.org/blubb"));

  // Start the navigation.
  auto navigation = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.org/bla"), web_contents());
  navigation->SetAutoAdvance(false);
  navigation->Start();
  EXPECT_FALSE(navigation->IsDeferred());
  EXPECT_EQ(NavigationThrottle::PROCEED,
            navigation->GetLastThrottleCheckResult().action());

  // Fake a response with a policy header.
  const char* raw_headers =
      "HTTP/1.1 200 OK\nSec-Origin-Policy: policy=policy-1\n\n";
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders(
          net::HttpUtil::AssembleRawHeaders(raw_headers, strlen(raw_headers)));
  NavigationHandleImpl* nav_handle =
      static_cast<NavigationHandleImpl*>(navigation->GetNavigationHandle());
  nav_handle->set_response_headers_for_testing(headers);
  navigation->ReadyToCommit();

  // Due to the exception, we expect the policy to not defer.
  EXPECT_FALSE(navigation->IsDeferred());

  // Also check that the header policy did not overwrite the exemption:
  std::string version;
  OriginPolicyThrottle::ShouldRequestOriginPolicy(
      GURL("https://example.org/bla"), &version);
  EXPECT_EQ(version, "0");
}

TEST(OriginPolicyThrottleTest, ParseHeaders) {
  const struct {
    const char* header;
    const char* policy_version;
    const char* report_to;
  } testcases[] = {
      // The common cases: We expect >99% of headers to look like these:
      {"policy=policy", "policy", ""},
      {"policy=policy, report-to=endpoint", "policy", "endpoint"},

      // Delete a policy. This better work.
      {"0", "0", ""},
      {"policy=0", "0", ""},
      {"policy=\"0\"", "0", ""},
      {"policy=0, report-to=endpoint", "0", "endpoint"},

      // Order, please!
      {"policy=policy, report-to=endpoint", "policy", "endpoint"},
      {"report-to=endpoint, policy=policy", "policy", "endpoint"},

      // Quoting:
      {"policy=\"policy\"", "policy", ""},
      {"policy=\"policy\", report-to=endpoint", "policy", "endpoint"},
      {"policy=\"policy\", report-to=\"endpoint\"", "policy", "endpoint"},
      {"policy=policy, report-to=\"endpoint\"", "policy", "endpoint"},

      // Whitespace, and funky but valid syntax:
      {"  policy  =   policy  ", "policy", ""},
      {" policy = \t policy ", "policy", ""},
      {" policy \t= \t \"policy\"  ", "policy", ""},
      {" policy = \" policy \" ", "policy", ""},
      {" , policy = policy , report-to=endpoint , ", "policy", "endpoint"},

      // Valid policy, invalid report-to:
      {"policy=policy, report-to endpoint", "", ""},
      {"policy=policy, report-to=here, report-to=there", "", ""},
      {"policy=policy, \"report-to\"=endpoint", "", ""},

      // Invalid policy, valid report-to:
      {"policy=policy1, policy=policy2", "", ""},
      {"policy, report-to=r", "", ""},
      {"report-to=endpoint", "", "endpoint"},

      // Invalid everything:
      {"one two three", "", ""},
      {"one, two, three", "", ""},
      {"policy report-to=endpoint", "", ""},
      {"policy=policy report-to=endpoint", "", ""},

      // Forward compatibility, ignore unknown keywords:
      {"policy=pol, report-to=endpoint, unknown=keyword", "pol", "endpoint"},
      {"unknown=keyword, policy=pol, report-to=endpoint", "pol", "endpoint"},
      {"policy=pol, unknown=keyword", "pol", ""},
      {"policy=policy, report_to=endpoint", "policy", ""},
      {"policy=policy, reportto=endpoint", "policy", ""},
  };
  for (const auto& testcase : testcases) {
    SCOPED_TRACE(testcase.header);
    const auto result = OriginPolicyThrottle::
        GetRequestedPolicyAndReportGroupFromHeaderStringForTesting(
            testcase.header);
    EXPECT_EQ(result.policy_version, testcase.policy_version);
    EXPECT_EQ(result.report_to, testcase.report_to);
  }
}

}  // namespace content
