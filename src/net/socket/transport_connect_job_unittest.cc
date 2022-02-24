// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_connect_job.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log.h"
#include "net/socket/connect_job_test_util.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/stream_socket.h"
#include "net/socket/transport_client_socket_pool_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {
namespace {

const char kHostName[] = "unresolvable.host.name";

IPAddress ParseIP(const std::string& ip) {
  IPAddress address;
  CHECK(address.AssignFromIPLiteral(ip));
  return address;
}

class TransportConnectJobTest : public WithTaskEnvironment,
                                public testing::Test {
 public:
  TransportConnectJobTest()
      : WithTaskEnvironment(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        client_socket_factory_(NetLog::Get()),
        common_connect_job_params_(
            &client_socket_factory_,
            &host_resolver_,
            nullptr /* http_auth_cache */,
            nullptr /* http_auth_handler_factory */,
            nullptr /* spdy_session_pool */,
            nullptr /* quic_supported_versions */,
            nullptr /* quic_stream_factory */,
            nullptr /* proxy_delegate */,
            nullptr /* http_user_agent_settings */,
            nullptr /* ssl_client_context */,
            nullptr /* socket_performance_watcher_factory */,
            nullptr /* network_quality_estimator */,
            NetLog::Get(),
            nullptr /* websocket_endpoint_lock_manager */) {}

  ~TransportConnectJobTest() override {}

  static scoped_refptr<TransportSocketParams> DefaultParams() {
    return base::MakeRefCounted<TransportSocketParams>(
        url::SchemeHostPort(url::kHttpScheme, kHostName, 80),
        NetworkIsolationKey(), SecureDnsPolicy::kAllow,
        OnHostResolutionCallback(),
        /*supported_alpns=*/base::flat_set<std::string>());
  }

  static scoped_refptr<TransportSocketParams> DefaultHttpsParams() {
    return base::MakeRefCounted<TransportSocketParams>(
        url::SchemeHostPort(url::kHttpsScheme, kHostName, 443),
        NetworkIsolationKey(), SecureDnsPolicy::kAllow,
        OnHostResolutionCallback(),
        /*supported_alpns=*/base::flat_set<std::string>{"h2", "http/1.1"});
  }

 protected:
  MockHostResolver host_resolver_{/*default_result=*/MockHostResolverBase::
                                      RuleResolver::GetLocalhostResult()};
  MockTransportClientSocketFactory client_socket_factory_;
  const CommonConnectJobParams common_connect_job_params_;
};

TEST_F(TransportConnectJobTest, MakeAddrListStartWithIPv4) {
  IPEndPoint addrlist_v4_1(IPAddress(192, 168, 1, 1), 80);
  IPEndPoint addrlist_v4_2(IPAddress(192, 168, 1, 2), 80);
  IPAddress ip_address;
  ASSERT_TRUE(ip_address.AssignFromIPLiteral("2001:4860:b006::64"));
  IPEndPoint addrlist_v6_1(ip_address, 80);
  ASSERT_TRUE(ip_address.AssignFromIPLiteral("2001:4860:b006::66"));
  IPEndPoint addrlist_v6_2(ip_address, 80);

  AddressList addrlist;

  // Test 1: IPv4 only.  Expect no change.
  addrlist.clear();
  addrlist.push_back(addrlist_v4_1);
  addrlist.push_back(addrlist_v4_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(2u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[1].GetFamily());

  // Test 2: IPv6 only.  Expect no change.
  addrlist.clear();
  addrlist.push_back(addrlist_v6_1);
  addrlist.push_back(addrlist_v6_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(2u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[1].GetFamily());

  // Test 3: IPv4 then IPv6.  Expect no change.
  addrlist.clear();
  addrlist.push_back(addrlist_v4_1);
  addrlist.push_back(addrlist_v4_2);
  addrlist.push_back(addrlist_v6_1);
  addrlist.push_back(addrlist_v6_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(4u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[1].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[2].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[3].GetFamily());

  // Test 4: IPv6, IPv4, IPv6, IPv4.  Expect first IPv6 moved to the end.
  addrlist.clear();
  addrlist.push_back(addrlist_v6_1);
  addrlist.push_back(addrlist_v4_1);
  addrlist.push_back(addrlist_v6_2);
  addrlist.push_back(addrlist_v4_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(4u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[1].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[2].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[3].GetFamily());

  // Test 5: IPv6, IPv6, IPv4, IPv4.  Expect first two IPv6's moved to the end.
  addrlist.clear();
  addrlist.push_back(addrlist_v6_1);
  addrlist.push_back(addrlist_v6_2);
  addrlist.push_back(addrlist_v4_1);
  addrlist.push_back(addrlist_v4_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(4u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[1].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[2].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[3].GetFamily());
}

TEST_F(TransportConnectJobTest, HostResolutionFailure) {
  host_resolver_.rules()->AddSimulatedTimeoutFailure(kHostName);

  //  Check sync and async failures.
  for (bool host_resolution_synchronous : {false, true}) {
    host_resolver_.set_synchronous_mode(host_resolution_synchronous);
    TestConnectJobDelegate test_delegate;
    TransportConnectJob transport_connect_job(
        DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
        DefaultParams(), &test_delegate, nullptr /* net_log */);
    test_delegate.StartJobExpectingResult(&transport_connect_job,
                                          ERR_NAME_NOT_RESOLVED,
                                          host_resolution_synchronous);
    EXPECT_THAT(transport_connect_job.GetResolveErrorInfo().error,
                test::IsError(ERR_DNS_TIMED_OUT));
  }
}

TEST_F(TransportConnectJobTest, ConnectionFailure) {
  for (bool host_resolution_synchronous : {false, true}) {
    for (bool connection_synchronous : {false, true}) {
      host_resolver_.set_synchronous_mode(host_resolution_synchronous);
      client_socket_factory_.set_default_client_socket_type(
          connection_synchronous
              ? MockTransportClientSocketFactory::Type::kFailing
              : MockTransportClientSocketFactory::Type::kPendingFailing);
      TestConnectJobDelegate test_delegate;
      TransportConnectJob transport_connect_job(
          DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
          DefaultParams(), &test_delegate, nullptr /* net_log */);
      test_delegate.StartJobExpectingResult(
          &transport_connect_job, ERR_CONNECTION_FAILED,
          host_resolution_synchronous && connection_synchronous);
    }
  }
}

TEST_F(TransportConnectJobTest, HostResolutionTimeout) {
  const base::TimeDelta kTinyTime = base::Microseconds(1);

  // Make request hang.
  host_resolver_.set_ondemand_mode(true);

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultParams(), &test_delegate, nullptr /* net_log */);
  ASSERT_THAT(transport_connect_job.Connect(), test::IsError(ERR_IO_PENDING));

  // Right up until just before expiration, the job does not time out.
  FastForwardBy(TransportConnectJob::ConnectionTimeout() - kTinyTime);
  EXPECT_FALSE(test_delegate.has_result());

  // But at the exact time of expiration, the job fails.
  FastForwardBy(kTinyTime);
  EXPECT_TRUE(test_delegate.has_result());
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
}

TEST_F(TransportConnectJobTest, ConnectionTimeout) {
  const base::TimeDelta kTinyTime = base::Microseconds(1);

  // Half the timeout time. In the async case, spend half the time waiting on
  // host resolution, half on connecting.
  const base::TimeDelta kFirstHalfOfTimeout =
      TransportConnectJob::ConnectionTimeout() / 2;

  const base::TimeDelta kSecondHalfOfTimeout =
      TransportConnectJob::ConnectionTimeout() - kFirstHalfOfTimeout;
  ASSERT_LE(kTinyTime, kSecondHalfOfTimeout);

  // Make connection attempts hang.
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kStalled);

  for (bool host_resolution_synchronous : {false, true}) {
    host_resolver_.set_ondemand_mode(!host_resolution_synchronous);
    TestConnectJobDelegate test_delegate;
    TransportConnectJob transport_connect_job(
        DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
        DefaultParams(), &test_delegate, nullptr /* net_log */);
    EXPECT_THAT(transport_connect_job.Connect(), test::IsError(ERR_IO_PENDING));

    // After half the timeout, connection does not timeout.
    FastForwardBy(kFirstHalfOfTimeout);
    EXPECT_FALSE(test_delegate.has_result());

    // In the async case, the host resolution completes now.
    if (!host_resolution_synchronous)
      host_resolver_.ResolveOnlyRequestNow();

    // After (almost) the second half of timeout, just before the full timeout
    // period, the ConnectJob is still live.
    FastForwardBy(kSecondHalfOfTimeout - kTinyTime);
    EXPECT_FALSE(test_delegate.has_result());

    // But at the exact timeout time, the job fails.
    FastForwardBy(kTinyTime);
    EXPECT_TRUE(test_delegate.has_result());
    EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
  }
}

TEST_F(TransportConnectJobTest, ConnectionSuccess) {
  for (bool host_resolution_synchronous : {false, true}) {
    for (bool connection_synchronous : {false, true}) {
      host_resolver_.set_synchronous_mode(host_resolution_synchronous);
      client_socket_factory_.set_default_client_socket_type(
          connection_synchronous
              ? MockTransportClientSocketFactory::Type::kSynchronous
              : MockTransportClientSocketFactory::Type::kPending);
      TestConnectJobDelegate test_delegate;
      TransportConnectJob transport_connect_job(
          DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
          DefaultParams(), &test_delegate, nullptr /* net_log */);
      test_delegate.StartJobExpectingResult(
          &transport_connect_job, OK,
          host_resolution_synchronous && connection_synchronous);
    }
  }
}

// TODO(crbug.com/1206799): Set up `host_resolver_` to require the expected
// scheme.
TEST_F(TransportConnectJobTest, HandlesHttpsEndpoint) {
  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      base::MakeRefCounted<TransportSocketParams>(
          url::SchemeHostPort(url::kHttpsScheme, kHostName, 80),
          NetworkIsolationKey(), SecureDnsPolicy::kAllow,
          OnHostResolutionCallback(),
          /*supported_alpns=*/base::flat_set<std::string>{"h2", "http/1.1"}),
      &test_delegate, nullptr /* net_log */);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        false /* expect_sync_result */);
}

// TODO(crbug.com/1206799): Set up `host_resolver_` to require the expected
// lack of scheme.
TEST_F(TransportConnectJobTest, HandlesNonStandardEndpoint) {
  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      base::MakeRefCounted<TransportSocketParams>(
          HostPortPair(kHostName, 80), NetworkIsolationKey(),
          SecureDnsPolicy::kAllow, OnHostResolutionCallback(),
          /*supported_alpns=*/base::flat_set<std::string>()),
      &test_delegate, nullptr /* net_log */);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        false /* expect_sync_result */);
}

TEST_F(TransportConnectJobTest, SecureDnsPolicy) {
  for (auto secure_dns_policy :
       {SecureDnsPolicy::kAllow, SecureDnsPolicy::kDisable}) {
    TestConnectJobDelegate test_delegate;
    TransportConnectJob transport_connect_job(
        DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
        base::MakeRefCounted<TransportSocketParams>(
            url::SchemeHostPort(url::kHttpScheme, kHostName, 80),
            NetworkIsolationKey(), secure_dns_policy,
            OnHostResolutionCallback(),
            /*supported_alpns=*/base::flat_set<std::string>{}),
        &test_delegate, nullptr /* net_log */);
    test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                          false /* expect_sync_result */);
    EXPECT_EQ(secure_dns_policy, host_resolver_.last_secure_dns_policy());
  }
}

// Test the case of the IPv6 address stalling, and falling back to the IPv4
// socket which finishes first.
TEST_F(TransportConnectJobTest, IPv6FallbackSocketIPv4FinishesFirst) {
  MockTransportClientSocketFactory::Rule rules[] = {
      // This is the IPv6-preferring socket. It stalls, but presents one failed
      // connection attempt on GetConnectionAttempts.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kStalledFailing,
          std::vector{IPEndPoint(ParseIP("2:abcd::3:4:ff"), 80),
                      IPEndPoint(ParseIP("2.2.2.2"), 80)}),
      // This is the IPv4-preferring socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kPending,
          std::vector{IPEndPoint(ParseIP("2.2.2.2"), 80),
                      IPEndPoint(ParseIP("2:abcd::3:4:ff"), 80)})};

  client_socket_factory_.SetRules(rules);

  // Resolve an AddressList with a IPv6 address first and then a IPv4 address.
  host_resolver_.rules()->AddIPLiteralRule(kHostName, "2:abcd::3:4:ff,2.2.2.2",
                                           std::string());

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultParams(), &test_delegate, nullptr /* net_log */);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        false /* expect_sync_result */);

  IPEndPoint endpoint;
  test_delegate.socket()->GetLocalAddress(&endpoint);
  EXPECT_TRUE(endpoint.address().IsIPv4());

  // Check that the failed connection attempt on the main socket is collected.
  ConnectionAttempts attempts;
  test_delegate.socket()->GetConnectionAttempts(&attempts);
  ASSERT_EQ(1u, attempts.size());
  EXPECT_THAT(attempts[0].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_TRUE(attempts[0].endpoint.address().IsIPv6());

  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

// Test the case of the IPv6 address being slow, thus falling back to trying to
// connect to the IPv4 address, but having the connect to the IPv6 address
// finish first.
TEST_F(TransportConnectJobTest, IPv6FallbackSocketIPv6FinishesFirst) {
  MockTransportClientSocketFactory::Rule rules[] = {
      // This is the IPv6 socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kDelayed,
          std::vector{IPEndPoint(ParseIP("2:abcd::3:4:ff"), 80),
                      IPEndPoint(ParseIP("2.2.2.2"), 80)}),
      // This is the IPv4 socket. It stalls, but presents one failed connection
      // attempt on GetConnectionAttempts.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kStalledFailing,
          std::vector{IPEndPoint(ParseIP("2.2.2.2"), 80),
                      IPEndPoint(ParseIP("2:abcd::3:4:ff"), 80)})};

  client_socket_factory_.SetRules(rules);
  client_socket_factory_.set_delay(
      base::Milliseconds(TransportConnectJob::kIPv6FallbackTimerInMs + 50));

  // Resolve an AddressList with a IPv6 address first and then a IPv4 address.
  host_resolver_.rules()->AddIPLiteralRule(kHostName, "2:abcd::3:4:ff,2.2.2.2",
                                           std::string());

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultParams(), &test_delegate, nullptr /* net_log */);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        false /* expect_sync_result */);

  IPEndPoint endpoint;
  test_delegate.socket()->GetLocalAddress(&endpoint);
  EXPECT_TRUE(endpoint.address().IsIPv6());

  // Check that the failed connection attempt on the fallback socket is
  // collected.
  ConnectionAttempts attempts;
  test_delegate.socket()->GetConnectionAttempts(&attempts);
  ASSERT_EQ(1u, attempts.size());
  EXPECT_THAT(attempts[0].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_TRUE(attempts[0].endpoint.address().IsIPv4());

  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

TEST_F(TransportConnectJobTest, IPv6NoIPv4AddressesToFallbackTo) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kDelayed);

  // Resolve an AddressList with only IPv6 addresses.
  host_resolver_.rules()->AddIPLiteralRule(
      kHostName, "2:abcd::3:4:ff,3:abcd::3:4:ff", std::string());

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultParams(), &test_delegate, nullptr /* net_log */);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        false /* expect_sync_result */);

  IPEndPoint endpoint;
  test_delegate.socket()->GetLocalAddress(&endpoint);
  EXPECT_TRUE(endpoint.address().IsIPv6());
  ConnectionAttempts attempts;
  test_delegate.socket()->GetConnectionAttempts(&attempts);
  EXPECT_EQ(0u, attempts.size());
  EXPECT_EQ(1, client_socket_factory_.allocation_count());
}

TEST_F(TransportConnectJobTest, IPv4HasNoFallback) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kDelayed);

  // Resolve an AddressList with only IPv4 addresses.
  host_resolver_.rules()->AddIPLiteralRule(kHostName, "1.1.1.1", std::string());

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultParams(), &test_delegate, nullptr /* net_log */);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        false /* expect_sync_result */);

  IPEndPoint endpoint;
  test_delegate.socket()->GetLocalAddress(&endpoint);
  EXPECT_TRUE(endpoint.address().IsIPv4());
  ConnectionAttempts attempts;
  test_delegate.socket()->GetConnectionAttempts(&attempts);
  EXPECT_EQ(0u, attempts.size());
  EXPECT_EQ(1, client_socket_factory_.allocation_count());
}

TEST_F(TransportConnectJobTest, DnsAliases) {
  host_resolver_.set_synchronous_mode(true);
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kSynchronous);

  // Resolve an AddressList with DNS aliases.
  std::vector<std::string> aliases({"alias1", "alias2", kHostName});
  host_resolver_.rules()->AddIPLiteralRuleWithDnsAliases(kHostName, "2.2.2.2",
                                                         std::move(aliases));

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultParams(), &test_delegate, nullptr /* net_log */);

  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        true /* expect_sync_result */);

  // Verify that the elements of the alias list are those from the
  // parameter vector.
  EXPECT_THAT(test_delegate.socket()->GetDnsAliases(),
              testing::ElementsAre("alias1", "alias2", kHostName));
}

TEST_F(TransportConnectJobTest, NoAdditionalDnsAliases) {
  host_resolver_.set_synchronous_mode(true);
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kSynchronous);

  // Resolve an AddressList without additional DNS aliases. (The parameter
  // is an empty vector.)
  std::vector<std::string> aliases;
  host_resolver_.rules()->AddIPLiteralRuleWithDnsAliases(kHostName, "2.2.2.2",
                                                         std::move(aliases));

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultParams(), &test_delegate, nullptr /* net_log */);

  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        true /* expect_sync_result */);

  // Verify that the alias list only contains kHostName.
  EXPECT_THAT(test_delegate.socket()->GetDnsAliases(),
              testing::ElementsAre(kHostName));
}

// Test that `TransportConnectJob` will pick up options from
// `HostResolverEndpointResult`.
TEST_F(TransportConnectJobTest, EndpointResult) {
  HostResolverEndpointResult endpoint;
  endpoint.ip_endpoints = {IPEndPoint(ParseIP("1::"), 8443),
                           IPEndPoint(ParseIP("1.1.1.1"), 8443)};
  endpoint.metadata.supported_protocol_alpns = {"h2"};
  host_resolver_.rules()->AddRule(kHostName, std::vector{endpoint});

  MockTransportClientSocketFactory::Rule rule(
      MockTransportClientSocketFactory::Type::kSynchronous,
      std::vector{IPEndPoint(ParseIP("1::"), 8443),
                  IPEndPoint(ParseIP("1.1.1.1"), 8443)});
  client_socket_factory_.SetRules(base::make_span(&rule, 1));

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultHttpsParams(), &test_delegate, /*net_log=*/nullptr);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        /*expect_sync_result=*/false);

  IPEndPoint peer_address;
  test_delegate.socket()->GetPeerAddress(&peer_address);
  EXPECT_EQ(peer_address, IPEndPoint(ParseIP("1::"), 8443));

  EXPECT_EQ(1, client_socket_factory_.allocation_count());

  // There were no failed connection attempts to report.
  ConnectionAttempts attempts;
  test_delegate.socket()->GetConnectionAttempts(&attempts);
  EXPECT_EQ(0u, attempts.size());
}

// Test that, given multiple `HostResolverEndpointResult` results,
// `TransportConnectJob` tries each in succession.
TEST_F(TransportConnectJobTest, MultipleRoutesFallback) {
  std::vector<HostResolverEndpointResult> endpoints(3);
  endpoints[0].ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441),
                               IPEndPoint(ParseIP("1.1.1.1"), 8441)};
  endpoints[0].metadata.supported_protocol_alpns = {"h3", "h2", "http/1.1"};
  endpoints[1].ip_endpoints = {IPEndPoint(ParseIP("2::"), 8442),
                               IPEndPoint(ParseIP("2.2.2.2"), 8442)};
  endpoints[1].metadata.supported_protocol_alpns = {"h3"};
  endpoints[2].ip_endpoints = {IPEndPoint(ParseIP("4::"), 443),
                               IPEndPoint(ParseIP("4.4.4.4"), 443)};
  host_resolver_.rules()->AddRule(kHostName, endpoints);

  MockTransportClientSocketFactory::Rule rules[] = {
      // `endpoints[0]`'s IPv6-prefering socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing,
          endpoints[0].ip_endpoints),
      // `endpoints[1]` is skipped because the ALPN is not compatible.
      // `endpoints[2]`'s IPv6-preferring socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kSynchronous,
          endpoints[2].ip_endpoints),
  };

  client_socket_factory_.SetRules(rules);

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultHttpsParams(), &test_delegate, /*net_log=*/nullptr);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        /*expect_sync_result=*/false);

  IPEndPoint peer_address;
  test_delegate.socket()->GetPeerAddress(&peer_address);
  EXPECT_EQ(peer_address, IPEndPoint(ParseIP("4::"), 443));

  // Check that failed connection attempts are reported.
  ConnectionAttempts attempts;
  test_delegate.socket()->GetConnectionAttempts(&attempts);
  ASSERT_EQ(2u, attempts.size());
  EXPECT_THAT(attempts[0].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_EQ(attempts[0].endpoint, IPEndPoint(ParseIP("1::"), 8441));
  EXPECT_THAT(attempts[1].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_EQ(attempts[1].endpoint, IPEndPoint(ParseIP("1.1.1.1"), 8441));
}

// Test that the `HostResolverEndpointResult` fallback works in combination with
// the IPv4 fallback.
TEST_F(TransportConnectJobTest, MultipleRoutesIPV4Fallback) {
  HostResolverEndpointResult endpoint1, endpoint2, endpoint3;
  endpoint1.ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441),
                            IPEndPoint(ParseIP("1.1.1.1"), 8441)};
  endpoint1.metadata.supported_protocol_alpns = {"h3", "h2", "http/1.1"};
  endpoint2.ip_endpoints = {IPEndPoint(ParseIP("2::"), 8442),
                            IPEndPoint(ParseIP("2.2.2.2"), 8442)};
  endpoint2.metadata.supported_protocol_alpns = {"h3"};
  endpoint3.ip_endpoints = {IPEndPoint(ParseIP("3::"), 443),
                            IPEndPoint(ParseIP("3.3.3.3"), 443)};
  host_resolver_.rules()->AddRule(kHostName,
                                  std::vector{endpoint1, endpoint2, endpoint3});

  MockTransportClientSocketFactory::Rule rules[] = {
      // `endpoint1`'s IPv6-prefering socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kStalled,
          std::vector{IPEndPoint(ParseIP("1::"), 8441),
                      IPEndPoint(ParseIP("1.1.1.1"), 8441)}),

      // `endpoint1`'s IPv4-prefering socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing,
          std::vector{IPEndPoint(ParseIP("1.1.1.1"), 8441),
                      IPEndPoint(ParseIP("1::"), 8441)}),

      // `endpoint2` is skipped because the ALPN is not compatible.

      // `endpoint3`'s IPv6-preferring socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kStalled,
          std::vector{IPEndPoint(ParseIP("3::"), 443),
                      IPEndPoint(ParseIP("3.3.3.3"), 443)}),
      // `endpoint3`'s IPv4-preferring socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kSynchronous,
          std::vector{IPEndPoint(ParseIP("3.3.3.3"), 443),
                      IPEndPoint(ParseIP("3::"), 443)}),
  };

  client_socket_factory_.SetRules(rules);

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultHttpsParams(), &test_delegate, /*net_log=*/nullptr);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        /*expect_sync_result=*/false);

  IPEndPoint peer_address;
  test_delegate.socket()->GetPeerAddress(&peer_address);
  EXPECT_EQ(peer_address, IPEndPoint(ParseIP("3.3.3.3"), 443));

  // Check that failed connection attempts are reported.
  ConnectionAttempts attempts;
  test_delegate.socket()->GetConnectionAttempts(&attempts);
  ASSERT_EQ(2u, attempts.size());
  EXPECT_THAT(attempts[0].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_EQ(attempts[0].endpoint, IPEndPoint(ParseIP("1.1.1.1"), 8441));
  EXPECT_THAT(attempts[1].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_EQ(attempts[1].endpoint, IPEndPoint(ParseIP("1::"), 8441));
}

// Test that, if `HostResolver` supports SVCB for a scheme but the caller didn't
// pass in any ALPN protocols, `TransportConnectJob` ignores all protocol
// endpoints.
TEST_F(TransportConnectJobTest, NoAlpnProtocols) {
  std::vector<HostResolverEndpointResult> endpoints(3);
  endpoints[0].ip_endpoints = {IPEndPoint(ParseIP("1::"), 8081),
                               IPEndPoint(ParseIP("1.1.1.1"), 8081)};
  endpoints[0].metadata.supported_protocol_alpns = {"foo", "bar"};
  endpoints[1].ip_endpoints = {IPEndPoint(ParseIP("2::"), 8082),
                               IPEndPoint(ParseIP("2.2.2.2"), 8082)};
  endpoints[1].metadata.supported_protocol_alpns = {"baz"};
  endpoints[2].ip_endpoints = {IPEndPoint(ParseIP("3::"), 80),
                               IPEndPoint(ParseIP("3.3.3.3"), 80)};
  host_resolver_.rules()->AddRule(kHostName, endpoints);

  // `endpoints[2]`'s IPv6-preferring socket.
  MockTransportClientSocketFactory::Rule rule(
      MockTransportClientSocketFactory::Type::kSynchronous,
      endpoints[2].ip_endpoints);
  client_socket_factory_.SetRules(base::make_span(&rule, 1));

  // Use `DefaultParams()`, an http scheme. That it is http is not very
  // important, but `url::SchemeHostPort` is difficult to use with unknown
  // schemes. See https://crbug.com/869291.
  scoped_refptr<TransportSocketParams> params = DefaultParams();
  ASSERT_TRUE(params->supported_alpns().empty());

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      std::move(params), &test_delegate, /*net_log=*/nullptr);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        /*expect_sync_result=*/false);

  IPEndPoint peer_address;
  test_delegate.socket()->GetPeerAddress(&peer_address);
  EXPECT_EQ(peer_address, IPEndPoint(ParseIP("3::"), 80));
}

// Test that, given multiple `HostResolverEndpointResult` results,
// `TransportConnectJob` reports failure if each one fails.
TEST_F(TransportConnectJobTest, MultipleRoutesAllFailed) {
  std::vector<HostResolverEndpointResult> endpoints(3);
  endpoints[0].ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441),
                               IPEndPoint(ParseIP("1.1.1.1"), 8441)};
  endpoints[0].metadata.supported_protocol_alpns = {"h3", "h2", "http/1.1"};
  endpoints[1].ip_endpoints = {IPEndPoint(ParseIP("2::"), 8442),
                               IPEndPoint(ParseIP("2.2.2.2"), 8442)};
  endpoints[1].metadata.supported_protocol_alpns = {"h3"};
  endpoints[2].ip_endpoints = {IPEndPoint(ParseIP("3::"), 443),
                               IPEndPoint(ParseIP("3.3.3.3"), 443)};
  host_resolver_.rules()->AddRule(kHostName, endpoints);

  MockTransportClientSocketFactory::Rule rules[] = {
      // `endpoints[0]`'s IPv6-prefering socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing,
          endpoints[0].ip_endpoints),
      // `endpoints[1]` is skipped because the ALPN is not compatible.
      // `endpoints[2]`'s IPv6-preferring socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing,
          endpoints[2].ip_endpoints),
  };

  client_socket_factory_.SetRules(rules);

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultHttpsParams(), &test_delegate, /*net_log=*/nullptr);
  test_delegate.StartJobExpectingResult(&transport_connect_job,
                                        ERR_CONNECTION_FAILED,
                                        /*expect_sync_result=*/false);

  // Check that failed connection attempts are reported.
  ConnectionAttempts attempts = transport_connect_job.GetConnectionAttempts();
  ASSERT_EQ(4u, attempts.size());
  EXPECT_THAT(attempts[0].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_EQ(attempts[0].endpoint, IPEndPoint(ParseIP("1::"), 8441));
  EXPECT_THAT(attempts[1].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_EQ(attempts[1].endpoint, IPEndPoint(ParseIP("1.1.1.1"), 8441));
  EXPECT_THAT(attempts[2].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_EQ(attempts[2].endpoint, IPEndPoint(ParseIP("3::"), 443));
  EXPECT_THAT(attempts[3].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_EQ(attempts[3].endpoint, IPEndPoint(ParseIP("3.3.3.3"), 443));
}

// Test that `TransportConnectJob` reports failure if all provided routes were
// unusable.
TEST_F(TransportConnectJobTest, NoUsableRoutes) {
  std::vector<HostResolverEndpointResult> endpoints(2);
  endpoints[0].ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441),
                               IPEndPoint(ParseIP("1.1.1.1"), 8441)};
  endpoints[0].metadata.supported_protocol_alpns = {"h3"};
  endpoints[1].ip_endpoints = {IPEndPoint(ParseIP("2::"), 8442),
                               IPEndPoint(ParseIP("2.2.2.2"), 8442)};
  endpoints[1].metadata.supported_protocol_alpns = {"unrecognized-protocol"};
  host_resolver_.rules()->AddRule(kHostName, endpoints);

  // `TransportConnectJob` should not create any sockets.
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kUnexpected);

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultHttpsParams(), &test_delegate, /*net_log=*/nullptr);
  test_delegate.StartJobExpectingResult(&transport_connect_job,
                                        ERR_NAME_NOT_RESOLVED,
                                        /*expect_sync_result=*/false);
}

// Test that, if the last route is unusable, the error from the
// previously-attempted route is preserved.
TEST_F(TransportConnectJobTest, LastRouteUnusable) {
  std::vector<HostResolverEndpointResult> endpoints(2);
  endpoints[0].ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441),
                               IPEndPoint(ParseIP("1.1.1.1"), 8441)};
  endpoints[0].metadata.supported_protocol_alpns = {"h3", "h2", "http/1.1"};
  endpoints[1].ip_endpoints = {IPEndPoint(ParseIP("2::"), 8442),
                               IPEndPoint(ParseIP("2.2.2.2"), 8442)};
  endpoints[1].metadata.supported_protocol_alpns = {"h3"};
  host_resolver_.rules()->AddRule(kHostName, endpoints);

  MockTransportClientSocketFactory::Rule rules[] = {
      // `endpoints[0]`'s IPv6-prefering socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing,
          endpoints[0].ip_endpoints),
      // `endpoints[1]` is skipped because the ALPN is not compatible.
  };

  client_socket_factory_.SetRules(rules);

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultHttpsParams(), &test_delegate, /*net_log=*/nullptr);
  test_delegate.StartJobExpectingResult(&transport_connect_job,
                                        ERR_CONNECTION_FAILED,
                                        /*expect_sync_result=*/false);

  // Check that failed connection attempts are reported.
  ConnectionAttempts attempts = transport_connect_job.GetConnectionAttempts();
  ASSERT_EQ(2u, attempts.size());
  EXPECT_THAT(attempts[0].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_EQ(attempts[0].endpoint, IPEndPoint(ParseIP("1::"), 8441));
  EXPECT_THAT(attempts[1].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_EQ(attempts[1].endpoint, IPEndPoint(ParseIP("1.1.1.1"), 8441));
}

// `GetEndpointMetadata` should surface information about the endpoint that was
// actually used.
TEST_F(TransportConnectJobTest, GetEndpointMetadata) {
  std::vector<HostResolverEndpointResult> endpoints(4);
  // `endpoints[0]` will be skipped due to ALPN mismatch.
  endpoints[0].ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441)};
  endpoints[0].metadata.supported_protocol_alpns = {"h3"};
  endpoints[0].metadata.ech_config_list = {1, 2, 3, 4};
  // `endpoints[1]` will be skipped due to connection failure.
  endpoints[1].ip_endpoints = {IPEndPoint(ParseIP("2::"), 8442)};
  endpoints[1].metadata.supported_protocol_alpns = {"http/1.1"};
  endpoints[1].metadata.ech_config_list = {5, 6, 7, 8};
  // `endpoints[2]` will succeed.
  endpoints[2].ip_endpoints = {IPEndPoint(ParseIP("3::"), 8443)};
  endpoints[2].metadata.supported_protocol_alpns = {"http/1.1"};
  endpoints[2].metadata.ech_config_list = {9, 10, 11, 12};
  // `endpoints[3]` will be not be tried because `endpoints[2]` will already
  // have succeeded.
  endpoints[3].ip_endpoints = {IPEndPoint(ParseIP("4::"), 8444)};
  endpoints[3].metadata.supported_protocol_alpns = {"http/1.1"};
  endpoints[3].metadata.ech_config_list = {13, 14, 15, 16};
  host_resolver_.rules()->AddRule(kHostName, endpoints);

  MockTransportClientSocketFactory::Rule rules[] = {
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing,
          std::vector{IPEndPoint(ParseIP("2::"), 8442)}),
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kSynchronous,
          std::vector{IPEndPoint(ParseIP("3::"), 8443)}),
  };
  client_socket_factory_.SetRules(rules);

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultHttpsParams(), &test_delegate, /*net_log=*/nullptr);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        /*expect_sync_result=*/false);

  EXPECT_EQ(transport_connect_job.GetEndpointMetadata(), endpoints[2].metadata);
}

// If the client and server both support ECH, TransportConnectJob should switch
// to SVCB-reliant mode and disable the A/AAAA fallback.
TEST_F(TransportConnectJobTest, SvcbReliantIfEch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEncryptedClientHello);

  HostResolverEndpointResult endpoint1, endpoint2, endpoint3;
  endpoint1.ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441)};
  endpoint1.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint1.metadata.ech_config_list = {1, 2, 3, 4};
  endpoint2.ip_endpoints = {IPEndPoint(ParseIP("2::"), 8442)};
  endpoint2.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint2.metadata.ech_config_list = {1, 2, 3, 4};
  endpoint3.ip_endpoints = {IPEndPoint(ParseIP("3::"), 443)};
  // `endpoint3` has no `supported_protocol_alpns` and is thus a fallback route.
  host_resolver_.rules()->AddRule(kHostName,
                                  std::vector{endpoint1, endpoint2, endpoint3});

  // `TransportConnectJob` should not try `endpoint3`.
  MockTransportClientSocketFactory::Rule rules[] = {
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing,
          std::vector{IPEndPoint(ParseIP("1::"), 8441)}),
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing,
          std::vector{IPEndPoint(ParseIP("2::"), 8442)}),
  };
  client_socket_factory_.SetRules(rules);

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultHttpsParams(), &test_delegate, /*net_log=*/nullptr);
  test_delegate.StartJobExpectingResult(&transport_connect_job,
                                        ERR_CONNECTION_FAILED,
                                        /*expect_sync_result=*/false);

  ConnectionAttempts attempts = transport_connect_job.GetConnectionAttempts();
  ASSERT_EQ(2u, attempts.size());
  EXPECT_THAT(attempts[0].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_EQ(attempts[0].endpoint, IPEndPoint(ParseIP("1::"), 8441));
  EXPECT_THAT(attempts[1].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_EQ(attempts[1].endpoint, IPEndPoint(ParseIP("2::"), 8442));
}

// SVCB-reliant mode should be disabled for ECH servers when ECH is disabled.
TEST_F(TransportConnectJobTest, SvcbOptionalIfEchDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kEncryptedClientHello);

  HostResolverEndpointResult endpoint1, endpoint2, endpoint3;
  endpoint1.ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441)};
  endpoint1.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint1.metadata.ech_config_list = {1, 2, 3, 4};
  endpoint2.ip_endpoints = {IPEndPoint(ParseIP("2::"), 8442)};
  endpoint2.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint2.metadata.ech_config_list = {1, 2, 3, 4};
  endpoint3.ip_endpoints = {IPEndPoint(ParseIP("3::"), 443)};
  // `endpoint3` has no `supported_protocol_alpns` and is thus a fallback route.
  host_resolver_.rules()->AddRule(kHostName,
                                  std::vector{endpoint1, endpoint2, endpoint3});

  // `TransportConnectJob` should try `endpoint3`.
  MockTransportClientSocketFactory::Rule rules[] = {
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing,
          std::vector{IPEndPoint(ParseIP("1::"), 8441)}),
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing,
          std::vector{IPEndPoint(ParseIP("2::"), 8442)}),
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kSynchronous,
          std::vector{IPEndPoint(ParseIP("3::"), 443)}),
  };
  client_socket_factory_.SetRules(rules);

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultHttpsParams(), &test_delegate, /*net_log=*/nullptr);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        /*expect_sync_result=*/false);
}

// SVCB-reliant mode should be disabled if not all SVCB/HTTPS records include
// ECH.
TEST_F(TransportConnectJobTest, SvcbOptionalIfEchInconsistent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEncryptedClientHello);

  HostResolverEndpointResult endpoint1, endpoint2, endpoint3;
  endpoint1.ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441)};
  endpoint1.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint1.metadata.ech_config_list = {1, 2, 3, 4};
  endpoint2.ip_endpoints = {IPEndPoint(ParseIP("2::"), 8442)};
  endpoint2.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint2.metadata.ech_config_list = {};
  endpoint3.ip_endpoints = {IPEndPoint(ParseIP("3::"), 443)};
  // `endpoint3` has no `supported_protocol_alpns` and is thus a fallback route.
  host_resolver_.rules()->AddRule(kHostName,
                                  std::vector{endpoint1, endpoint2, endpoint3});

  // `TransportConnectJob` should try `endpoint3`.
  MockTransportClientSocketFactory::Rule rules[] = {
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing,
          std::vector{IPEndPoint(ParseIP("1::"), 8441)}),
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing,
          std::vector{IPEndPoint(ParseIP("2::"), 8442)}),
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kSynchronous,
          std::vector{IPEndPoint(ParseIP("3::"), 443)}),
  };
  client_socket_factory_.SetRules(rules);

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultHttpsParams(), &test_delegate, /*net_log=*/nullptr);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        /*expect_sync_result=*/false);
}

}  // namespace
}  // namespace net
