// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_loader_factory.h"

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

class ServiceWorkerScriptLoaderFactoryTest : public testing::Test {
 public:
  ServiceWorkerScriptLoaderFactoryTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~ServiceWorkerScriptLoaderFactoryTest() override = default;

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
    ServiceWorkerContextCore* context = helper_->context();
    context->storage()->LazyInitializeForTest();

    scope_ = GURL("https://host/scope");
    script_url_ = GURL("https://host/script.js");

    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope_;
    registration_ = base::MakeRefCounted<ServiceWorkerRegistration>(
        options, 1L /* registration_id */, context->AsWeakPtr());
    version_ = base::MakeRefCounted<ServiceWorkerVersion>(
        registration_.get(), script_url_, blink::mojom::ScriptType::kClassic,
        context->storage()->NewVersionId(), context->AsWeakPtr());

    provider_host_ = CreateProviderHostForServiceWorkerContext(
        helper_->mock_render_process_id(), true /* is_parent_frame_secure */,
        version_.get(), context->AsWeakPtr(), &remote_endpoint_);

    network_loader_factory_ = std::make_unique<FakeNetworkURLLoaderFactory>();
    helper_->SetNetworkFactory(network_loader_factory_.get());

    factory_ = std::make_unique<ServiceWorkerScriptLoaderFactory>(
        helper_->context()->AsWeakPtr(), provider_host_,
        helper_->url_loader_factory_getter()->GetNetworkFactory());
  }

 protected:
  network::mojom::URLLoaderPtr CreateTestLoaderAndStart(
      network::TestURLLoaderClient* client) {
    network::mojom::URLLoaderPtr loader;
    network::ResourceRequest resource_request;
    resource_request.url = script_url_;
    resource_request.resource_type =
        static_cast<int>(ResourceType::kServiceWorker);
    factory_->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        network::mojom::kURLLoadOptionNone, resource_request,
        client->CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    return loader;
  }

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  GURL scope_;
  GURL script_url_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  ServiceWorkerRemoteProviderEndpoint remote_endpoint_;
  std::unique_ptr<FakeNetworkURLLoaderFactory> network_loader_factory_;
  std::unique_ptr<ServiceWorkerScriptLoaderFactory> factory_;
};

TEST_F(ServiceWorkerScriptLoaderFactoryTest, Success) {
  network::TestURLLoaderClient client;
  network::mojom::URLLoaderPtr loader = CreateTestLoaderAndStart(&client);
  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);
}

TEST_F(ServiceWorkerScriptLoaderFactoryTest, Redundant) {
  version_->SetStatus(ServiceWorkerVersion::REDUNDANT);

  network::TestURLLoaderClient client;
  network::mojom::URLLoaderPtr loader = CreateTestLoaderAndStart(&client);
  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
}

TEST_F(ServiceWorkerScriptLoaderFactoryTest, NoProviderHost) {
  helper_->context()->RemoveProviderHost(provider_host_->provider_id());

  network::TestURLLoaderClient client;
  network::mojom::URLLoaderPtr loader = CreateTestLoaderAndStart(&client);
  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
}

TEST_F(ServiceWorkerScriptLoaderFactoryTest, ContextDestroyed) {
  helper_->ShutdownContext();
  base::RunLoop().RunUntilIdle();

  network::TestURLLoaderClient client;
  network::mojom::URLLoaderPtr loader = CreateTestLoaderAndStart(&client);
  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
}

// This tests copying script and creating resume type
// ServiceWorkerNewScriptLoaders when ServiceWorkerImportedScriptUpdateCheck
// is enabled.
class ServiceWorkerScriptLoaderFactoryCopyResumeTest
    : public ServiceWorkerScriptLoaderFactoryTest {
 public:
  ServiceWorkerScriptLoaderFactoryCopyResumeTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kServiceWorkerImportedScriptUpdateCheck);
  }
  ~ServiceWorkerScriptLoaderFactoryCopyResumeTest() override = default;

  void SetUp() override {
    ServiceWorkerScriptLoaderFactoryTest::SetUp();
    WriteToDiskCacheSync(helper_->context()->storage(), script_url_,
                         kOldResourceId, kOldHeaders, kOldData, std::string());
  }

  void CheckResponse(const std::string& expected_body) {
    // The response should also be stored in the storage.
    EXPECT_TRUE(ServiceWorkerUpdateCheckTestUtils::VerifyStoredResponse(
        version_->script_cache_map()->LookupResourceId(script_url_),
        helper_->context()->storage(), expected_body));

    EXPECT_TRUE(client_.has_received_response());
    EXPECT_TRUE(client_.response_body().is_valid());

    std::string response;
    EXPECT_TRUE(
        mojo::BlockingCopyToString(client_.response_body_release(), &response));
    EXPECT_EQ(expected_body, response);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  network::TestURLLoaderClient client_;
  const std::vector<std::pair<std::string, std::string>> kOldHeaders = {
      {"Content-Type", "text/javascript"},
      {"Content-Length", "15"}};
  const std::string kOldData = "old-script-data";
  const int64_t kOldResourceId = 1;
  const int64_t kNewResourceId = 2;
};

// Tests scripts are copied and loaded locally when compared to be
// identical in update check.
TEST_F(ServiceWorkerScriptLoaderFactoryCopyResumeTest, CopyScript) {
  ServiceWorkerUpdateCheckTestUtils::SetComparedScriptInfoForVersion(
      script_url_, kOldResourceId,
      ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical, nullptr,
      version_.get());

  network::mojom::URLLoaderPtr loader = CreateTestLoaderAndStart(&client_);
  client_.RunUntilComplete();

  EXPECT_EQ(net::OK, client_.completion_status().error_code);

  // Checks the received response data.
  CheckResponse(kOldData);
}

// Tests loader factory creates resume type ServiceWorkerNewScriptLoader to
// continue paused download in update check.
TEST_F(ServiceWorkerScriptLoaderFactoryCopyResumeTest,
       CreateResumeTypeScriptLoader) {
  const std::string kNewHeaders =
      "HTTP/1.0 200 OK\0Content-Type: text/javascript\0Content-Length: 0\0\0";
  const std::string kNewData = "";

  mojo::ScopedDataPipeProducerHandle network_producer;
  mojo::ScopedDataPipeConsumerHandle network_consumer;

  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, &network_producer,
                                                 &network_consumer));
  ServiceWorkerUpdateCheckTestUtils::CreateAndSetComparedScriptInfoForVersion(
      script_url_, 0, kNewHeaders, kNewData, kOldResourceId, kNewResourceId,
      helper_.get(), ServiceWorkerUpdatedScriptLoader::LoaderState::kCompleted,
      ServiceWorkerUpdatedScriptLoader::WriterState::kCompleted,
      std::move(network_consumer),
      ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent,
      version_.get());

  network::mojom::URLLoaderPtr loader = CreateTestLoaderAndStart(&client_);
  network_producer.reset();
  client_.RunUntilComplete();

  EXPECT_EQ(net::OK, client_.completion_status().error_code);

  // The received response has no body because kNewData is empty.
  CheckResponse(kNewData);
}

}  // namespace content
