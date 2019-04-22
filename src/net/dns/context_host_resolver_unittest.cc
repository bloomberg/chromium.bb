// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/context_host_resolver.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/dns_protocol.h"
#include "net/log/net_log_with_source.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {
const IPEndPoint kEndpoint(IPAddress(1, 2, 3, 4), 100);
}

class ContextHostResolverTest : public TestWithScopedTaskEnvironment {
 protected:
  void SetUp() override {
    manager_ =
        std::make_unique<HostResolverManager>(HostResolver::Options(), nullptr);
  }

  void SetMockDnsRules(MockDnsClientRuleList rules) {
    // HostResolver expects DnsConfig to get set after setting DnsClient, so
    // create first with an empty config and then update the config.
    auto dns_client =
        std::make_unique<MockDnsClient>(DnsConfig(), std::move(rules));
    dns_client_ = dns_client.get();
    manager_->SetDnsClient(std::move(dns_client));

    scoped_refptr<HostResolverProc> proc = CreateCatchAllHostResolverProc();
    manager_->set_proc_params_for_test(ProcTaskParams(proc.get(), 1u));

    IPAddress dns_ip(192, 168, 1, 0);
    DnsConfig config;
    config.nameservers.push_back(
        IPEndPoint(dns_ip, dns_protocol::kDefaultPort));
    EXPECT_TRUE(config.IsValid());
    manager_->SetBaseDnsConfigForTesting(config);
  }

  MockDnsClient* dns_client_;
  std::unique_ptr<HostResolverManager> manager_;
};

TEST_F(ContextHostResolverTest, Resolve) {
  URLRequestContext context;

  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA,
                     SecureDnsMode::AUTOMATIC,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         "example.com", kEndpoint.address())),
                     false /* delay */, &context);
  rules.emplace_back("example.com", dns_protocol::kTypeAAAA,
                     SecureDnsMode::AUTOMATIC,
                     MockDnsClientRule::Result(MockDnsClientRule::EMPTY),
                     false /* delay */, &context);
  SetMockDnsRules(std::move(rules));

  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), nullptr /* host_cache */);
  resolver->SetRequestContext(&context);
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetLogWithSource(), base::nullopt);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  EXPECT_THAT(callback.GetResult(rv), test::IsOk());
  EXPECT_THAT(request->GetAddressResults().value().endpoints(),
              testing::ElementsAre(kEndpoint));
}

// Test that destroying a request silently cancels that request.
TEST_F(ContextHostResolverTest, DestroyRequest) {
  // Setup delayed results for "example.com".
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA,
                     SecureDnsMode::AUTOMATIC,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         "example.com", IPAddress(1, 2, 3, 4))),
                     true /* delay */);
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, SecureDnsMode::AUTOMATIC,
      MockDnsClientRule::Result(MockDnsClientRule::EMPTY), false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), nullptr /* host_cache */);
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetLogWithSource(), base::nullopt);
  EXPECT_EQ(1u, resolver->GetNumActiveRequestsForTesting());

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());

  // Cancel |request| before allowing delayed result to complete.
  request = nullptr;
  dns_client_->CompleteDelayedTransactions();

  // Ensure |request| never completes.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(rv, test::IsError(ERR_IO_PENDING));
  EXPECT_FALSE(callback.have_result());
  EXPECT_EQ(0u, resolver->GetNumActiveRequestsForTesting());
}

// Test that cancelling a resolver cancels its (and only its) requests.
TEST_F(ContextHostResolverTest, DestroyResolver) {
  // Setup delayed results for "example.com" and "google.com".
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA,
                     SecureDnsMode::AUTOMATIC,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         "example.com", IPAddress(2, 3, 4, 5))),
                     true /* delay */);
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, SecureDnsMode::AUTOMATIC,
      MockDnsClientRule::Result(MockDnsClientRule::EMPTY), false /* delay */);
  rules.emplace_back("google.com", dns_protocol::kTypeA,
                     SecureDnsMode::AUTOMATIC,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         "google.com", kEndpoint.address())),
                     true /* delay */);
  rules.emplace_back(
      "google.com", dns_protocol::kTypeAAAA, SecureDnsMode::AUTOMATIC,
      MockDnsClientRule::Result(MockDnsClientRule::EMPTY), false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto resolver1 = std::make_unique<ContextHostResolver>(
      manager_.get(), nullptr /* host_cache */);
  std::unique_ptr<HostResolver::ResolveHostRequest> request1 =
      resolver1->CreateRequest(HostPortPair("example.com", 100),
                               NetLogWithSource(), base::nullopt);
  auto resolver2 = std::make_unique<ContextHostResolver>(
      manager_.get(), nullptr /* host_cache */);
  std::unique_ptr<HostResolver::ResolveHostRequest> request2 =
      resolver2->CreateRequest(HostPortPair("google.com", 100),
                               NetLogWithSource(), base::nullopt);

  TestCompletionCallback callback1;
  int rv1 = request1->Start(callback1.callback());
  TestCompletionCallback callback2;
  int rv2 = request2->Start(callback2.callback());

  EXPECT_EQ(2u, manager_->num_jobs_for_testing());

  // Cancel |resolver1| before allowing delayed requests to complete.
  resolver1 = nullptr;
  dns_client_->CompleteDelayedTransactions();

  EXPECT_THAT(callback2.GetResult(rv2), test::IsOk());
  EXPECT_THAT(request2->GetAddressResults().value().endpoints(),
              testing::ElementsAre(kEndpoint));

  // Ensure |request1| never completes.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(rv1, test::IsError(ERR_IO_PENDING));
  EXPECT_FALSE(callback1.have_result());
}

// Test that cancelling a resolver cancels its (and only its) requests, even if
// those requests shared a job (same query) with another resolver's requests.
TEST_F(ContextHostResolverTest, DestroyResolver_RemainingRequests) {
  // Setup delayed results for "example.com".
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA,
                     SecureDnsMode::AUTOMATIC,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         "example.com", kEndpoint.address())),
                     true /* delay */);
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, SecureDnsMode::AUTOMATIC,
      MockDnsClientRule::Result(MockDnsClientRule::EMPTY), false /* delay */);
  SetMockDnsRules(std::move(rules));

  // Make ResolveHostRequests the same hostname for both resolvers.
  auto resolver1 = std::make_unique<ContextHostResolver>(
      manager_.get(), nullptr /* host_cache */);
  std::unique_ptr<HostResolver::ResolveHostRequest> request1 =
      resolver1->CreateRequest(HostPortPair("example.com", 100),
                               NetLogWithSource(), base::nullopt);
  auto resolver2 = std::make_unique<ContextHostResolver>(
      manager_.get(), nullptr /* host_cache */);
  std::unique_ptr<HostResolver::ResolveHostRequest> request2 =
      resolver2->CreateRequest(HostPortPair("example.com", 100),
                               NetLogWithSource(), base::nullopt);

  TestCompletionCallback callback1;
  int rv1 = request1->Start(callback1.callback());
  TestCompletionCallback callback2;
  int rv2 = request2->Start(callback2.callback());

  // Test relies on assumption that requests share jobs, so assert just 1.
  ASSERT_EQ(1u, manager_->num_jobs_for_testing());

  // Cancel |resolver1| before allowing delayed requests to complete.
  resolver1 = nullptr;
  dns_client_->CompleteDelayedTransactions();

  EXPECT_THAT(callback2.GetResult(rv2), test::IsOk());
  EXPECT_THAT(request2->GetAddressResults().value().endpoints(),
              testing::ElementsAre(kEndpoint));

  // Ensure |request1| never completes.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(rv1, test::IsError(ERR_IO_PENDING));
  EXPECT_FALSE(callback1.have_result());
}

TEST_F(ContextHostResolverTest, DestroyResolver_CompletedRequests) {
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA,
                     SecureDnsMode::AUTOMATIC,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         "example.com", kEndpoint.address())),
                     false /* delay */);
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, SecureDnsMode::AUTOMATIC,
      MockDnsClientRule::Result(MockDnsClientRule::EMPTY), false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), nullptr /* host_cache */);
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetLogWithSource(), base::nullopt);

  // Complete request and then destroy the resolver.
  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  ASSERT_THAT(callback.GetResult(rv), test::IsOk());
  resolver = nullptr;

  // Expect completed results are still available.
  EXPECT_THAT(request->GetAddressResults().value().endpoints(),
              testing::ElementsAre(kEndpoint));
}

}  // namespace net
