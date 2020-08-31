// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/network_service_test_helper.h"
#include "extensions/browser/api/sockets_tcp/sockets_tcp_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/shell/test/shell_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "services/network/test/test_dns_util.h"

namespace extensions {

const char kHostname[] = "www.foo.test";

class SocketsTcpApiTest : public ShellApiTest {
 public:
  SocketsTcpApiTest() {
    // Enable kSplitHostCacheByNetworkIsolationKey so the test can verify that
    // the correct NetworkIsolationKey was used for the DNS lookup.
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kSplitHostCacheByNetworkIsolationKey);

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseMockCertVerifierForTesting);
  }

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();
    host_resolver()->AddRule(kHostname, "127.0.0.1");
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SocketsTcpApiTest, SocketsTcpCreateGood) {
  scoped_refptr<api::SocketsTcpCreateFunction> socket_create_function(
      new api::SocketsTcpCreateFunction());
  scoped_refptr<const Extension> empty_extension =
      ExtensionBuilder("Test").Build();

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  std::unique_ptr<base::Value> result(
      api_test_utils::RunFunctionAndReturnSingleResult(
          socket_create_function.get(), "[]", browser_context()));

  ASSERT_EQ(base::Value::Type::DICTIONARY, result->type());
  std::unique_ptr<base::DictionaryValue> value =
      base::DictionaryValue::From(std::move(result));
  int socketId = -1;
  EXPECT_TRUE(value->GetInteger("socketId", &socketId));
  ASSERT_TRUE(socketId > 0);
}

IN_PROC_BROWSER_TEST_F(SocketsTcpApiTest, SocketTcpExtension) {
  std::unique_ptr<net::SpawnedTestServer> test_server(
      new net::SpawnedTestServer(
          net::SpawnedTestServer::TYPE_TCP_ECHO,
          base::FilePath(FILE_PATH_LITERAL("net/data"))));
  EXPECT_TRUE(test_server->Start());

  net::HostPortPair host_port_pair = test_server->host_port_pair();
  int port = host_port_pair.port();
  ASSERT_TRUE(port > 0);

  // Test that connect() is properly resolving hostnames.
  host_port_pair.set_host(kHostname);

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());

  ExtensionTestMessageListener listener("info_please", true);

  scoped_refptr<const Extension> test_extension = LoadApp("sockets_tcp/api");
  ASSERT_TRUE(test_extension);

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(
      base::StringPrintf("tcp:%s:%d", host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Make sure the extension's NetworkIsolationKey was used. Do a cache only DNS
  // lookup using the expected NIK, and make sure the IP address is retrieved.
  network::mojom::NetworkContext* network_context =
      content::BrowserContext::GetDefaultStoragePartition(browser_context())
          ->GetNetworkContext();
  network::mojom::ResolveHostParametersPtr params =
      network::mojom::ResolveHostParameters::New();
  // Cache only lookup.
  params->source = net::HostResolverSource::LOCAL_ONLY;
  url::Origin origin = url::Origin::Create(test_extension->url());
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  network::DnsLookupResult result1 =
      network::BlockingDnsLookup(network_context, host_port_pair,
                                 std::move(params), network_isolation_key);
  EXPECT_EQ(net::OK, result1.error);
  ASSERT_TRUE(result1.resolved_addresses.has_value());
  ASSERT_EQ(1u, result1.resolved_addresses->size());
  EXPECT_EQ("127.0.0.1",
            result1.resolved_addresses.value()[0].ToStringWithoutPort());

  // Check that the entry isn't present in the cache with the empty
  // NetworkIsolationKey.
  params = network::mojom::ResolveHostParameters::New();
  // Cache only lookup.
  params->source = net::HostResolverSource::LOCAL_ONLY;
  network::DnsLookupResult result2 =
      network::BlockingDnsLookup(network_context, host_port_pair,
                                 std::move(params), net::NetworkIsolationKey());
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, result2.error);
}

IN_PROC_BROWSER_TEST_F(SocketsTcpApiTest, SocketTcpExtensionTLS) {
  // Because the network service runs in a utility process, the cert of the
  // SpawnedTestServer won't be recognized, so inject mock cert verifier through
  // the test helper interface.
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  content::GetNetworkService()->BindTestInterface(
      network_service_test.BindNewPipeAndPassReceiver());
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  network_service_test->MockCertVerifierSetDefaultResult(net::OK);

  std::unique_ptr<net::SpawnedTestServer> test_https_server(
      new net::SpawnedTestServer(
          net::SpawnedTestServer::TYPE_HTTPS, net::BaseTestServer::SSLOptions(),
          base::FilePath(FILE_PATH_LITERAL("net/data"))));
  EXPECT_TRUE(test_https_server->Start());

  net::HostPortPair https_host_port_pair = test_https_server->host_port_pair();
  int https_port = https_host_port_pair.port();
  ASSERT_GT(https_port, 0);

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());

  ExtensionTestMessageListener listener("info_please", true);

  ASSERT_TRUE(LoadApp("sockets_tcp/api"));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf(
      "https:%s:%d", https_host_port_pair.host().c_str(), https_port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
