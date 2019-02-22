// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/test/test_mojo_proxy_resolver_factory.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_delegate_impl.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_entry.h"
#include "net/proxy_resolution/dhcp_pac_file_fetcher.h"
#include "net/proxy_resolution/mock_pac_file_fetcher.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/test/event_waiter.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/proxy_service_mojo.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::test::IsOk;

namespace content {

namespace {

const char kPacUrl[] = "http://example.com/proxy.pac";
const char kSimplePacScript[] =
    "function FindProxyForURL(url, host) {\n"
    "  return 'PROXY foo:1234';\n"
    "}";
const char kDnsResolvePacScript[] =
    "function FindProxyForURL(url, host) {\n"
    "  if (dnsResolveEx('example.com') != '1.2.3.4')\n"
    "    return 'DIRECT';\n"
    "  return 'QUIC bar:4321';\n"
    "}";
const char kThrowingPacScript[] =
    "function FindProxyForURL(url, host) {\n"
    "  alert('alert: ' + host);\n"
    "  throw new Error('error: ' + url);\n"
    "}";
const char kThrowingOnLoadPacScript[] =
    "function FindProxyForURL(url, host) {}\n"
    "alert('alert: foo');\n"
    "throw new Error('error: http://foo');";

class TestNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  enum Event {
    PAC_SCRIPT_ERROR,
  };

  net::EventWaiter<Event>& event_waiter() { return event_waiter_; }

  void OnPACScriptError(int line_number, const base::string16& error) override;

 private:
  net::EventWaiter<Event> event_waiter_;
};

void TestNetworkDelegate::OnPACScriptError(int line_number,
                                           const base::string16& error) {
  event_waiter_.NotifyEvent(PAC_SCRIPT_ERROR);
  EXPECT_EQ(3, line_number);
  EXPECT_TRUE(base::UTF16ToUTF8(error).find("error: http://foo") !=
              std::string::npos);
}

void CheckCapturedNetLogEntries(const net::TestNetLogEntry::List& entries) {
  ASSERT_GT(entries.size(), 2u);
  size_t i = 0;
  // ProxyResolutionService records its own NetLog entries, so skip forward
  // until the expected event type.
  while (i < entries.size() &&
         entries[i].type != net::NetLogEventType::PAC_JAVASCRIPT_ALERT) {
    i++;
  }
  ASSERT_LT(i, entries.size());
  std::string message;
  ASSERT_TRUE(entries[i].GetStringValue("message", &message));
  EXPECT_EQ("alert: foo", message);
  ASSERT_FALSE(entries[i].params->HasKey("line_number"));

  while (i < entries.size() &&
         entries[i].type != net::NetLogEventType::PAC_JAVASCRIPT_ERROR) {
    i++;
  }
  message.clear();
  ASSERT_TRUE(entries[i].GetStringValue("message", &message));
  EXPECT_THAT(message, testing::HasSubstr("error: http://foo"));
  int line_number = 0;
  ASSERT_TRUE(entries[i].GetIntegerValue("line_number", &line_number));
  EXPECT_EQ(3, line_number);
}

class LoggingMockHostResolver : public net::MockHostResolver {
 public:
  int Resolve(const RequestInfo& info,
              net::RequestPriority priority,
              net::AddressList* addresses,
              net::CompletionOnceCallback callback,
              std::unique_ptr<Request>* out_req,
              const net::NetLogWithSource& net_log) override {
    net_log.AddEvent(net::NetLogEventType::HOST_RESOLVER_IMPL_JOB);
    return net::MockHostResolver::Resolve(
        info, priority, addresses, std::move(callback), out_req, net_log);
  }
};

}  // namespace

class ProxyServiceMojoTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_host_resolver_.rules()->AddRule("example.com", "1.2.3.4");

    fetcher_ = new net::MockPacFileFetcher;
    proxy_resolution_service_ =
        network::CreateProxyResolutionServiceUsingMojoFactory(
            test_mojo_proxy_resolver_factory_.CreateFactoryInterface(),
            std::make_unique<net::ProxyConfigServiceFixed>(
                net::ProxyConfigWithAnnotation(
                    net::ProxyConfig::CreateFromCustomPacURL(GURL(kPacUrl)),
                    TRAFFIC_ANNOTATION_FOR_TESTS)),
            base::WrapUnique(fetcher_),
            std::make_unique<net::DoNothingDhcpPacFileFetcher>(),
            &mock_host_resolver_, &net_log_, &network_delegate_);
  }

  base::test::ScopedTaskEnvironment task_environment_;
  content::TestMojoProxyResolverFactory test_mojo_proxy_resolver_factory_;
  TestNetworkDelegate network_delegate_;
  LoggingMockHostResolver mock_host_resolver_;
  // Owned by |proxy_resolution_service_|.
  net::MockPacFileFetcher* fetcher_;
  net::TestNetLog net_log_;
  std::unique_ptr<net::ProxyResolutionService> proxy_resolution_service_;
};

TEST_F(ProxyServiceMojoTest, Basic) {
  net::ProxyInfo info;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::ProxyResolutionService::Request> request;
  EXPECT_EQ(net::ERR_IO_PENDING,
            proxy_resolution_service_->ResolveProxy(
                GURL("http://foo"), std::string(), &info, callback.callback(),
                &request, net::NetLogWithSource()));

  // PAC file fetcher should have a fetch triggered by the first
  // |ResolveProxy()| request.
  EXPECT_TRUE(fetcher_->has_pending_request());
  EXPECT_EQ(GURL(kPacUrl), fetcher_->pending_request_url());
  fetcher_->NotifyFetchCompletion(net::OK, kSimplePacScript);

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ("PROXY foo:1234", info.ToPacString());
  EXPECT_EQ(0u, mock_host_resolver_.num_resolve());
  proxy_resolution_service_.reset();
}

TEST_F(ProxyServiceMojoTest, DnsResolution) {
  net::ProxyInfo info;
  net::TestCompletionCallback callback;
  net::BoundTestNetLog test_net_log;
  std::unique_ptr<net::ProxyResolutionService::Request> request;
  EXPECT_EQ(net::ERR_IO_PENDING,
            proxy_resolution_service_->ResolveProxy(
                GURL("http://foo"), std::string(), &info, callback.callback(),
                &request, test_net_log.bound()));

  // PAC file fetcher should have a fetch triggered by the first
  // |ResolveProxy()| request.
  EXPECT_TRUE(fetcher_->has_pending_request());
  EXPECT_EQ(GURL(kPacUrl), fetcher_->pending_request_url());

  fetcher_->NotifyFetchCompletion(net::OK, kDnsResolvePacScript);

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ("QUIC bar:4321", info.ToPacString());
  EXPECT_EQ(1u, mock_host_resolver_.num_resolve());
  proxy_resolution_service_.reset();

  net::TestNetLogEntry::List entries;
  test_net_log.GetEntries(&entries);
  // There should be one entry with type TYPE_HOST_RESOLVER_IMPL_JOB.
  EXPECT_EQ(1,
            std::count_if(entries.begin(), entries.end(),
                          [](const net::TestNetLogEntry& entry) {
                            return entry.type ==
                                   net::NetLogEventType::HOST_RESOLVER_IMPL_JOB;
                          }));
}

TEST_F(ProxyServiceMojoTest, Error) {
  net::ProxyInfo info;
  net::TestCompletionCallback callback;
  net::BoundTestNetLog test_net_log;
  std::unique_ptr<net::ProxyResolutionService::Request> request;
  EXPECT_EQ(net::ERR_IO_PENDING,
            proxy_resolution_service_->ResolveProxy(
                GURL("http://foo"), std::string(), &info, callback.callback(),
                &request, test_net_log.bound()));

  // PAC file fetcher should have a fetch triggered by the first
  // |ResolveProxy()| request.
  EXPECT_TRUE(fetcher_->has_pending_request());
  EXPECT_EQ(GURL(kPacUrl), fetcher_->pending_request_url());
  fetcher_->NotifyFetchCompletion(net::OK, kThrowingPacScript);

  network_delegate_.event_waiter().WaitForEvent(
      TestNetworkDelegate::PAC_SCRIPT_ERROR);

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ("DIRECT", info.ToPacString());
  EXPECT_EQ(0u, mock_host_resolver_.num_resolve());

  net::TestNetLogEntry::List entries;
  test_net_log.GetEntries(&entries);
  CheckCapturedNetLogEntries(entries);
  entries.clear();
  net_log_.GetEntries(&entries);
  CheckCapturedNetLogEntries(entries);
}

TEST_F(ProxyServiceMojoTest, ErrorOnInitialization) {
  net::ProxyInfo info;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::ProxyResolutionService::Request> request;
  EXPECT_EQ(net::ERR_IO_PENDING,
            proxy_resolution_service_->ResolveProxy(
                GURL("http://foo"), std::string(), &info, callback.callback(),
                &request, net::NetLogWithSource()));

  // PAC file fetcher should have a fetch triggered by the first
  // |ResolveProxy()| request.
  EXPECT_TRUE(fetcher_->has_pending_request());
  EXPECT_EQ(GURL(kPacUrl), fetcher_->pending_request_url());
  fetcher_->NotifyFetchCompletion(net::OK, kThrowingOnLoadPacScript);

  network_delegate_.event_waiter().WaitForEvent(
      TestNetworkDelegate::PAC_SCRIPT_ERROR);

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ("DIRECT", info.ToPacString());
  EXPECT_EQ(0u, mock_host_resolver_.num_resolve());

  net::TestNetLogEntry::List entries;
  net_log_.GetEntries(&entries);
  CheckCapturedNetLogEntries(entries);
}

}  // namespace content
