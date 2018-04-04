// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/network_session_configurator/browser/network_session_configurator.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/base/cache_type.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_manager.h"
#include "net/http/http_transaction_factory.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_job_factory.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/proxy_config.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/udp_socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace network {

namespace {

mojom::NetworkContextParamsPtr CreateContextParams() {
  mojom::NetworkContextParamsPtr params = mojom::NetworkContextParams::New();
  // Use a fixed proxy config, to avoid dependencies on local network
  // configuration.
  params->initial_proxy_config = net::ProxyConfigWithAnnotation::CreateDirect();
  return params;
}

class NetworkContextTest : public testing::Test {
 public:
  NetworkContextTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        network_service_(NetworkService::CreateForTesting()) {}
  ~NetworkContextTest() override {}

  std::unique_ptr<NetworkContext> CreateContextWithParams(
      mojom::NetworkContextParamsPtr context_params) {
    return std::make_unique<NetworkContext>(
        network_service_.get(), mojo::MakeRequest(&network_context_ptr_),
        std::move(context_params));
  }

  // Searches through |backend|'s stats to discover its type. Only supports
  // blockfile and simple caches.
  net::URLRequestContextBuilder::HttpCacheParams::Type GetBackendType(
      disk_cache::Backend* backend) {
    base::StringPairs stats;
    backend->GetStats(&stats);
    for (const auto& pair : stats) {
      if (pair.first != "Cache type")
        continue;

      if (pair.second == "Simple Cache")
        return net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE;
      if (pair.second == "Blockfile Cache")
        return net::URLRequestContextBuilder::HttpCacheParams::DISK_BLOCKFILE;
      break;
    }

    NOTREACHED();
    return net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
  }

  mojom::NetworkService* network_service() const {
    return network_service_.get();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<NetworkService> network_service_;
  // Stores the NetworkContextPtr of the most recently created NetworkContext,
  // since destroying this before the NetworkContext itself triggers deletion of
  // the NetworkContext. These tests are probably fine anyways, since the
  // message loop must be spun for that to happen.
  mojom::NetworkContextPtr network_context_ptr_;
};

TEST_F(NetworkContextTest, DestroyContextWithLiveRequest) {
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  ResourceRequest request;
  request.url = test_server.GetURL("/hung-after-headers");

  mojom::URLLoaderFactoryPtr loader_factory;
  network_context->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory),
                                          0);

  mojom::URLLoaderPtr loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, request, client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilResponseReceived();
  EXPECT_TRUE(client.has_received_response());
  EXPECT_FALSE(client.has_received_completion());

  // Destroying the loader factory should not delete the URLLoader.
  loader_factory.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(client.has_received_completion());

  // Destroying the NetworkContext should result in destroying the loader and
  // the client receiving a connection error.
  network_context.reset();

  client.RunUntilConnectionError();
  EXPECT_FALSE(client.has_received_completion());
  EXPECT_EQ(0u, client.download_data_length());
}

TEST_F(NetworkContextTest, DisableQuic) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableQuic);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  // By default, QUIC should be enabled for new NetworkContexts when the command
  // line indicates it should be.
  EXPECT_TRUE(network_context->GetURLRequestContext()
                  ->http_transaction_factory()
                  ->GetSession()
                  ->params()
                  .enable_quic);

  // Disabling QUIC should disable it on existing NetworkContexts.
  network_service()->DisableQuic();
  EXPECT_FALSE(network_context->GetURLRequestContext()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .enable_quic);

  // Disabling QUIC should disable it new NetworkContexts.
  std::unique_ptr<NetworkContext> network_context2 =
      CreateContextWithParams(CreateContextParams());
  EXPECT_FALSE(network_context2->GetURLRequestContext()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .enable_quic);

  // Disabling QUIC again should be harmless.
  network_service()->DisableQuic();
  std::unique_ptr<NetworkContext> network_context3 =
      CreateContextWithParams(CreateContextParams());
  EXPECT_FALSE(network_context3->GetURLRequestContext()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .enable_quic);
}

TEST_F(NetworkContextTest, UserAgentAndLanguage) {
  const char kUserAgent[] = "Chromium Unit Test";
  const char kAcceptLanguage[] = "en-US,en;q=0.9,uk;q=0.8";
  mojom::NetworkContextParamsPtr params = CreateContextParams();
  params->user_agent = kUserAgent;
  // Not setting accept_language, to test the default.
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(params));
  EXPECT_EQ(kUserAgent, network_context->GetURLRequestContext()
                            ->http_user_agent_settings()
                            ->GetUserAgent());
  EXPECT_EQ("en-us,en", network_context->GetURLRequestContext()
                            ->http_user_agent_settings()
                            ->GetAcceptLanguage());

  // Change accept-language.
  network_context->SetAcceptLanguage(kAcceptLanguage);
  EXPECT_EQ(kUserAgent, network_context->GetURLRequestContext()
                            ->http_user_agent_settings()
                            ->GetUserAgent());
  EXPECT_EQ(kAcceptLanguage, network_context->GetURLRequestContext()
                                 ->http_user_agent_settings()
                                 ->GetAcceptLanguage());

  // Create with custom accept-language configured.
  params = CreateContextParams();
  params->user_agent = kUserAgent;
  params->accept_language = kAcceptLanguage;
  std::unique_ptr<NetworkContext> network_context2 =
      CreateContextWithParams(std::move(params));
  EXPECT_EQ(kUserAgent, network_context2->GetURLRequestContext()
                            ->http_user_agent_settings()
                            ->GetUserAgent());
  EXPECT_EQ(kAcceptLanguage, network_context2->GetURLRequestContext()
                                 ->http_user_agent_settings()
                                 ->GetAcceptLanguage());
}

TEST_F(NetworkContextTest, EnableBrotli) {
  for (bool enable_brotli : {true, false}) {
    mojom::NetworkContextParamsPtr context_params =
        mojom::NetworkContextParams::New();
    context_params->enable_brotli = enable_brotli;
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));
    EXPECT_EQ(enable_brotli,
              network_context->GetURLRequestContext()->enable_brotli());
  }
}

TEST_F(NetworkContextTest, ContextName) {
  const char kContextName[] = "Jim";
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();
  context_params->context_name = std::string(kContextName);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_EQ(kContextName, network_context->GetURLRequestContext()->name());
}

TEST_F(NetworkContextTest, QuicUserAgentId) {
  const char kQuicUserAgentId[] = "007";
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->quic_user_agent_id = kQuicUserAgentId;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_EQ(kQuicUserAgentId, network_context->GetURLRequestContext()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->params()
                                  .quic_user_agent_id);
}

TEST_F(NetworkContextTest, DisableDataUrlSupport) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->enable_data_url_support = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(
      network_context->GetURLRequestContext()->job_factory()->IsHandledProtocol(
          url::kDataScheme));
}

TEST_F(NetworkContextTest, EnableDataUrlSupport) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->enable_data_url_support = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(
      network_context->GetURLRequestContext()->job_factory()->IsHandledProtocol(
          url::kDataScheme));
}

TEST_F(NetworkContextTest, DisableFileUrlSupport) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->enable_file_url_support = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(
      network_context->GetURLRequestContext()->job_factory()->IsHandledProtocol(
          url::kFileScheme));
}

#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
TEST_F(NetworkContextTest, EnableFileUrlSupport) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->enable_file_url_support = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(
      network_context->GetURLRequestContext()->job_factory()->IsHandledProtocol(
          url::kFileScheme));
}
#endif  // !BUILDFLAG(DISABLE_FILE_SUPPORT)

TEST_F(NetworkContextTest, DisableFtpUrlSupport) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->enable_ftp_url_support = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(
      network_context->GetURLRequestContext()->job_factory()->IsHandledProtocol(
          url::kFtpScheme));
}

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
TEST_F(NetworkContextTest, EnableFtpUrlSupport) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->enable_ftp_url_support = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(
      network_context->GetURLRequestContext()->job_factory()->IsHandledProtocol(
          url::kFtpScheme));
}
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

#if BUILDFLAG(ENABLE_REPORTING)
TEST_F(NetworkContextTest, DisableReporting) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(features::kReporting);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  EXPECT_FALSE(network_context->GetURLRequestContext()->reporting_service());
}

TEST_F(NetworkContextTest, EnableReporting) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kReporting);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  EXPECT_TRUE(network_context->GetURLRequestContext()->reporting_service());
}

TEST_F(NetworkContextTest, DisableNetworkErrorLogging) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(features::kNetworkErrorLogging);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  EXPECT_FALSE(
      network_context->GetURLRequestContext()->network_error_logging_service());
}

TEST_F(NetworkContextTest, EnableNetworkErrorLogging) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kNetworkErrorLogging);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());
  EXPECT_TRUE(
      network_context->GetURLRequestContext()->network_error_logging_service());
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

TEST_F(NetworkContextTest, Http09Disabled) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_09_on_non_default_ports_enabled = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(network_context->GetURLRequestContext()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .http_09_on_non_default_ports_enabled);
}

TEST_F(NetworkContextTest, Http09Enabled) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_09_on_non_default_ports_enabled = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(network_context->GetURLRequestContext()
                  ->http_transaction_factory()
                  ->GetSession()
                  ->params()
                  .http_09_on_non_default_ports_enabled);
}

TEST_F(NetworkContextTest, DefaultHttpNetworkSessionParams) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  const net::HttpNetworkSession::Params& params =
      network_context->GetURLRequestContext()
          ->http_transaction_factory()
          ->GetSession()
          ->params();

  EXPECT_TRUE(params.enable_http2);
  EXPECT_FALSE(params.enable_quic);
  EXPECT_EQ(1350u, params.quic_max_packet_length);
  EXPECT_TRUE(params.origins_to_force_quic_on.empty());
  EXPECT_FALSE(params.enable_user_alternate_protocol_ports);
  EXPECT_FALSE(params.ignore_certificate_errors);
  EXPECT_EQ(0, params.testing_fixed_http_port);
  EXPECT_EQ(0, params.testing_fixed_https_port);
}

// Make sure that network_session_configurator is hooked up.
TEST_F(NetworkContextTest, FixedHttpPort) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTestingFixedHttpPort, "800");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTestingFixedHttpsPort, "801");

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  const net::HttpNetworkSession::Params& params =
      network_context->GetURLRequestContext()
          ->http_transaction_factory()
          ->GetSession()
          ->params();

  EXPECT_EQ(800, params.testing_fixed_http_port);
  EXPECT_EQ(801, params.testing_fixed_https_port);
}

TEST_F(NetworkContextTest, NoCache) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(network_context->GetURLRequestContext()
                   ->http_transaction_factory()
                   ->GetCache());
}

TEST_F(NetworkContextTest, MemoryCache) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->GetURLRequestContext()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_TRUE(cache);

  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  EXPECT_EQ(net::MEMORY_CACHE, backend->GetCacheType());
}

TEST_F(NetworkContextTest, DiskCache) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = true;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->http_cache_path = temp_dir.GetPath();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->GetURLRequestContext()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_TRUE(cache);

  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  EXPECT_EQ(net::DISK_CACHE, backend->GetCacheType());
  EXPECT_EQ(network_session_configurator::ChooseCacheType(
                *base::CommandLine::ForCurrentProcess()),
            GetBackendType(backend));
}

// This makes sure that network_session_configurator::ChooseCacheType is
// connected to NetworkContext.
TEST_F(NetworkContextTest, SimpleCache) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kUseSimpleCacheBackend, "on");
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = true;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->http_cache_path = temp_dir.GetPath();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->GetURLRequestContext()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_TRUE(cache);

  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  base::StringPairs stats;
  backend->GetStats(&stats);
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
            GetBackendType(backend));
}

TEST_F(NetworkContextTest, HttpServerPropertiesToDisk) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("foo");
  EXPECT_FALSE(base::PathExists(file_path));

  const url::SchemeHostPort kSchemeHostPort("https", "foo", 443);

  // Create a context with on-disk storage of HTTP server properties.
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();
  context_params->http_server_properties_path = file_path;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Wait for properties to load from disk, and sanity check initial state.
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(network_context->GetURLRequestContext()
                   ->http_server_properties()
                   ->GetSupportsSpdy(kSchemeHostPort));

  // Set a property.
  network_context->GetURLRequestContext()
      ->http_server_properties()
      ->SetSupportsSpdy(kSchemeHostPort, true);
  // Deleting the context will cause it to flush state. Wait for the pref
  // service to flush to disk.
  network_context.reset();
  scoped_task_environment_.RunUntilIdle();

  // Create a new NetworkContext using the same path for HTTP server properties.
  context_params = mojom::NetworkContextParams::New();
  context_params->http_server_properties_path = file_path;
  network_context = CreateContextWithParams(std::move(context_params));

  // Wait for properties to load from disk.
  scoped_task_environment_.RunUntilIdle();

  EXPECT_TRUE(network_context->GetURLRequestContext()
                  ->http_server_properties()
                  ->GetSupportsSpdy(kSchemeHostPort));

  // Now check that ClearNetworkingHistorySince clears the data.
  base::RunLoop run_loop2;
  network_context->ClearNetworkingHistorySince(
      base::Time::Now() - base::TimeDelta::FromHours(1),
      run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_FALSE(network_context->GetURLRequestContext()
                   ->http_server_properties()
                   ->GetSupportsSpdy(kSchemeHostPort));

  // Clear destroy the network context and let any pending writes complete
  // before destroying |temp_dir|, to avoid leaking any files.
  network_context.reset();
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(temp_dir.Delete());
}

// Checks that ClearNetworkingHistorySince() works clears in-memory pref stores,
// and invokes the closure passed to it.
TEST_F(NetworkContextTest, ClearHttpServerPropertiesInMemory) {
  const url::SchemeHostPort kSchemeHostPort("https", "foo", 443);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(mojom::NetworkContextParams::New());

  EXPECT_FALSE(network_context->GetURLRequestContext()
                   ->http_server_properties()
                   ->GetSupportsSpdy(kSchemeHostPort));
  network_context->GetURLRequestContext()
      ->http_server_properties()
      ->SetSupportsSpdy(kSchemeHostPort, true);
  EXPECT_TRUE(network_context->GetURLRequestContext()
                  ->http_server_properties()
                  ->GetSupportsSpdy(kSchemeHostPort));

  base::RunLoop run_loop;
  network_context->ClearNetworkingHistorySince(
      base::Time::Now() - base::TimeDelta::FromHours(1),
      run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(network_context->GetURLRequestContext()
                   ->http_server_properties()
                   ->GetSupportsSpdy(kSchemeHostPort));
}

// Validates that clearing the HTTP cache when no cache exists does complete.
TEST_F(NetworkContextTest, ClearHttpCacheWithNoCache) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->GetURLRequestContext()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_EQ(nullptr, cache);
  base::RunLoop run_loop;
  network_context->ClearHttpCache(base::Time(), base::Time(),
                                  /*filter=*/nullptr,
                                  base::BindOnce(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(NetworkContextTest, ClearHttpCache) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = true;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->http_cache_path = temp_dir.GetPath();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->GetURLRequestContext()
                              ->http_transaction_factory()
                              ->GetCache();

  std::vector<std::string> entry_urls = {
      "http://www.google.com",    "https://www.google.com",
      "http://www.wikipedia.com", "https://www.wikipedia.com",
      "http://localhost:1234",    "https://localhost:1234",
  };
  ASSERT_TRUE(cache);
  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  for (const auto& url : entry_urls) {
    disk_cache::Entry* entry = nullptr;
    base::RunLoop run_loop;
    if (backend->CreateEntry(
            url, &entry,
            base::Bind([](base::OnceClosure quit_loop,
                          int rv) { std::move(quit_loop).Run(); },
                       run_loop.QuitClosure())) == net::ERR_IO_PENDING) {
      run_loop.Run();
    }
    entry->Close();
  }
  EXPECT_EQ(entry_urls.size(), static_cast<size_t>(backend->GetEntryCount()));
  base::RunLoop run_loop;
  network_context->ClearHttpCache(base::Time(), base::Time(),
                                  /*filter=*/nullptr,
                                  base::BindOnce(run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(0U, static_cast<size_t>(backend->GetEntryCount()));
}

// Checks that when multiple calls are made to clear the HTTP cache, all
// callbacks are invoked.
TEST_F(NetworkContextTest, MultipleClearHttpCacheCalls) {
  constexpr int kNumberOfClearCalls = 10;

  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = true;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->http_cache_path = temp_dir.GetPath();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      /*num_closures=*/kNumberOfClearCalls, run_loop.QuitClosure());
  for (int i = 0; i < kNumberOfClearCalls; i++) {
    network_context->ClearHttpCache(base::Time(), base::Time(),
                                    /*filter=*/nullptr,
                                    base::BindOnce(barrier_closure));
  }
  run_loop.Run();
  // If all the callbacks were invoked, we should terminate.
}

void SetCookieCallback(base::RunLoop* run_loop, bool* result_out, bool result) {
  *result_out = result;
  run_loop->Quit();
}

void GetCookieListCallback(base::RunLoop* run_loop,
                           net::CookieList* result_out,
                           const net::CookieList& result) {
  *result_out = result;
  run_loop->Quit();
}

TEST_F(NetworkContextTest, CookieManager) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(mojom::NetworkContextParams::New());

  mojom::CookieManagerPtr cookie_manager_ptr;
  mojom::CookieManagerRequest cookie_manager_request(
      mojo::MakeRequest(&cookie_manager_ptr));
  network_context->GetCookieManager(std::move(cookie_manager_request));

  // Set a cookie through the cookie interface.
  base::RunLoop run_loop1;
  bool result = false;
  cookie_manager_ptr->SetCanonicalCookie(
      net::CanonicalCookie("TestCookie", "1", "www.test.com", "/", base::Time(),
                           base::Time(), base::Time(), false, false,
                           net::CookieSameSite::NO_RESTRICTION,
                           net::COOKIE_PRIORITY_LOW),
      true, true, base::BindOnce(&SetCookieCallback, &run_loop1, &result));
  run_loop1.Run();
  EXPECT_TRUE(result);

  // Confirm that cookie is visible directly through the store associated with
  // the network context.
  base::RunLoop run_loop2;
  net::CookieList cookies;
  network_context->GetURLRequestContext()
      ->cookie_store()
      ->GetCookieListWithOptionsAsync(
          GURL("http://www.test.com/whatever"), net::CookieOptions(),
          base::Bind(&GetCookieListCallback, &run_loop2, &cookies));
  run_loop2.Run();
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("TestCookie", cookies[0].Name());
}

TEST_F(NetworkContextTest, ProxyConfig) {
  // Create a bunch of proxy rules to switch between. All that matters is that
  // they're all different. It's important that none of these configs require
  // fetching a PAC scripts, as this test checks
  // ProxyResolutionService::config(), which is only updated after fetching PAC
  // scripts (if applicable).
  net::ProxyConfig proxy_configs[3];
  proxy_configs[0].proxy_rules().ParseFromString("http=foopy:80");
  proxy_configs[1].proxy_rules().ParseFromString("http=foopy:80;ftp=foopy2");
  proxy_configs[2] = net::ProxyConfig::CreateDirect();

  // Sanity check.
  EXPECT_FALSE(proxy_configs[0].Equals(proxy_configs[1]));
  EXPECT_FALSE(proxy_configs[0].Equals(proxy_configs[2]));
  EXPECT_FALSE(proxy_configs[1].Equals(proxy_configs[2]));

  // Try each proxy config as the initial config, to make sure setting the
  // initial config works.
  for (const auto& initial_proxy_config : proxy_configs) {
    mojom::NetworkContextParamsPtr context_params = CreateContextParams();
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        initial_proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
    mojom::ProxyConfigClientPtr config_client;
    context_params->proxy_config_client_request =
        mojo::MakeRequest(&config_client);
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));

    net::ProxyResolutionService* proxy_resolution_service =
        network_context->GetURLRequestContext()->proxy_resolution_service();
    // Kick the ProxyResolutionService into action, as it doesn't start updating
    // its config until it's first used.
    proxy_resolution_service->ForceReloadProxyConfig();
    EXPECT_TRUE(proxy_resolution_service->config());
    EXPECT_TRUE(proxy_resolution_service->config()->value().Equals(
        initial_proxy_config));

    // Always go through the other configs in the same order. This has the
    // advantage of testing the case where there's no change, for
    // proxy_config[0].
    for (const auto& proxy_config : proxy_configs) {
      config_client->OnProxyConfigUpdated(net::ProxyConfigWithAnnotation(
          proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS));
      scoped_task_environment_.RunUntilIdle();
      EXPECT_TRUE(proxy_resolution_service->config());
      EXPECT_TRUE(
          proxy_resolution_service->config()->value().Equals(proxy_config));
    }
  }
}

// Verify that a proxy config works without a ProxyConfigClientRequest.
TEST_F(NetworkContextTest, StaticProxyConfig) {
  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString("http=foopy:80;ftp=foopy2");

  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
      proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  net::ProxyResolutionService* proxy_resolution_service =
      network_context->GetURLRequestContext()->proxy_resolution_service();
  // Kick the ProxyResolutionService into action, as it doesn't start updating
  // its config until it's first used.
  proxy_resolution_service->ForceReloadProxyConfig();
  EXPECT_TRUE(proxy_resolution_service->config());
  EXPECT_TRUE(proxy_resolution_service->config()->value().Equals(proxy_config));
}

TEST_F(NetworkContextTest, NoInitialProxyConfig) {
  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->initial_proxy_config.reset();
  mojom::ProxyConfigClientPtr config_client;
  context_params->proxy_config_client_request =
      mojo::MakeRequest(&config_client);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  net::ProxyResolutionService* proxy_resolution_service =
      network_context->GetURLRequestContext()->proxy_resolution_service();
  EXPECT_FALSE(proxy_resolution_service->config());
  EXPECT_FALSE(proxy_resolution_service->fetched_config());

  // Before there's a proxy configuration, proxy requests should hang.
  net::ProxyInfo proxy_info;
  net::TestCompletionCallback test_callback;
  net::ProxyResolutionService::Request* request = nullptr;
  ASSERT_EQ(net::ERR_IO_PENDING, proxy_resolution_service->ResolveProxy(
                                     GURL("http://bar/"), "GET", &proxy_info,
                                     test_callback.callback(), &request,
                                     nullptr, net::NetLogWithSource()));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(proxy_resolution_service->config());
  EXPECT_FALSE(proxy_resolution_service->fetched_config());
  ASSERT_FALSE(test_callback.have_result());

  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString("http=foopy:80");
  config_client->OnProxyConfigUpdated(net::ProxyConfigWithAnnotation(
      proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS));
  ASSERT_EQ(net::OK, test_callback.WaitForResult());

  EXPECT_TRUE(proxy_info.is_http());
  EXPECT_EQ("foopy", proxy_info.proxy_server().host_port_pair().host());
}

class TestProxyConfigLazyPoller : public mojom::ProxyConfigPollerClient {
 public:
  TestProxyConfigLazyPoller() : binding_(this) {}
  ~TestProxyConfigLazyPoller() override {}

  void OnLazyProxyConfigPoll() override { ++times_polled_; }

  mojom::ProxyConfigPollerClientPtr BindInterface() {
    mojom::ProxyConfigPollerClientPtr interface;
    binding_.Bind(MakeRequest(&interface));
    return interface;
  }

  int GetAndClearTimesPolled() {
    int out = times_polled_;
    times_polled_ = 0;
    return out;
  }

 private:
  int times_polled_ = 0;
  mojo::Binding<ProxyConfigPollerClient> binding_;

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestProxyConfigLazyPoller);
};

net::IPEndPoint GetLocalHostWithAnyPort() {
  return net::IPEndPoint(net::IPAddress(127, 0, 0, 1), 0);
}

std::vector<uint8_t> CreateTestMessage(uint8_t initial, size_t size) {
  std::vector<uint8_t> array(size);
  for (size_t i = 0; i < size; ++i)
    array[i] = static_cast<uint8_t>((i + initial) % 256);
  return array;
}

TEST_F(NetworkContextTest, CreateUDPSocket) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateContextParams());

  // Create a server socket to listen for incoming datagrams.
  test::UDPSocketReceiverImpl receiver;
  mojo::Binding<mojom::UDPSocketReceiver> receiver_binding(&receiver);
  mojom::UDPSocketReceiverPtr receiver_interface_ptr;
  receiver_binding.Bind(mojo::MakeRequest(&receiver_interface_ptr));

  net::IPEndPoint server_addr(GetLocalHostWithAnyPort());
  mojom::UDPSocketPtr server_socket;
  network_context->CreateUDPSocket(mojo::MakeRequest(&server_socket),
                                   std::move(receiver_interface_ptr));
  network::test::UDPSocketTestHelper helper(&server_socket);
  ASSERT_EQ(net::OK, helper.BindSync(server_addr, nullptr, &server_addr));

  // Create a client socket to send datagrams.
  mojom::UDPSocketPtr client_socket;
  mojom::UDPSocketRequest client_socket_request(
      mojo::MakeRequest(&client_socket));
  network_context->CreateUDPSocket(std::move(client_socket_request), nullptr);

  net::IPEndPoint client_addr(GetLocalHostWithAnyPort());
  network::test::UDPSocketTestHelper client_helper(&client_socket);
  ASSERT_EQ(net::OK,
            client_helper.ConnectSync(server_addr, nullptr, &client_addr));

  // This test assumes that the loopback interface doesn't drop UDP packets for
  // a small number of packets.
  const size_t kDatagramCount = 6;
  const size_t kDatagramSize = 255;
  server_socket->ReceiveMore(kDatagramCount);

  for (size_t i = 0; i < kDatagramCount; ++i) {
    std::vector<uint8_t> test_msg(
        CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize));
    int result = client_helper.SendSync(test_msg);
    EXPECT_EQ(net::OK, result);
  }

  receiver.WaitForReceivedResults(kDatagramCount);
  EXPECT_EQ(kDatagramCount, receiver.results().size());

  int i = 0;
  for (const auto& result : receiver.results()) {
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_EQ(result.src_addr, client_addr);
    EXPECT_EQ(CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize),
              result.data.value());
    i++;
  }
}

}  // namespace

}  // namespace network
