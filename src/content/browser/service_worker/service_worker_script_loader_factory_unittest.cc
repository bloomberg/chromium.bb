// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_loader_factory.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

// A URLLoaderFactory that returns 200 OK with an empty javascript to any
// request.
// TODO(bashi): Avoid duplicated MockNetworkURLLoaderFactory. This is almost the
// same as EmbeddedWorkerTestHelper::MockNetworkURLLoaderFactory.
class MockNetworkURLLoaderFactory final
    : public network::mojom::URLLoaderFactory {
 public:
  MockNetworkURLLoaderFactory() = default;

  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    const std::string headers =
        "HTTP/1.1 200 OK\n"
        "Content-Type: application/javascript\n\n";
    net::HttpResponseInfo info;
    info.headers = new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.length()));
    network::ResourceResponseHead response;
    response.headers = info.headers;
    response.headers->GetMimeType(&response.mime_type);
    client->OnReceiveResponse(response);

    const std::string body = "/*this body came from the network*/";
    uint32_t bytes_written = body.size();
    mojo::DataPipe data_pipe;
    data_pipe.producer_handle->WriteData(body.data(), &bytes_written,
                                         MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
    client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));

    network::URLLoaderCompletionStatus status;
    status.error_code = net::OK;
    client->OnComplete(status);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;
  DISALLOW_COPY_AND_ASSIGN(MockNetworkURLLoaderFactory);
};

}  // namespace

class ServiceWorkerScriptLoaderFactoryTest : public testing::Test {
 public:
  ServiceWorkerScriptLoaderFactoryTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~ServiceWorkerScriptLoaderFactoryTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kServiceWorkerServicification);

    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
    ServiceWorkerContextCore* context = helper_->context();
    context->storage()->LazyInitializeForTest(base::DoNothing());
    base::RunLoop().RunUntilIdle();

    scope_ = GURL("https://host/scope");

    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope_;
    registration_ = base::MakeRefCounted<ServiceWorkerRegistration>(
        options, 1L /* registration_id */, context->AsWeakPtr());
    version_ = base::MakeRefCounted<ServiceWorkerVersion>(
        registration_.get(), GURL("https://host/script.js"),
        blink::mojom::ScriptType::kClassic, context->storage()->NewVersionId(),
        context->AsWeakPtr());

    provider_host_ = CreateProviderHostForServiceWorkerContext(
        helper_->mock_render_process_id(), true /* is_parent_frame_secure */,
        version_.get(), context->AsWeakPtr(), &remote_endpoint_);

    network_loader_factory_ = std::make_unique<MockNetworkURLLoaderFactory>();
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
    resource_request.url = scope_;
    resource_request.resource_type = RESOURCE_TYPE_SERVICE_WORKER;
    factory_->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        network::mojom::kURLLoadOptionNone, resource_request,
        client->CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    return loader;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  GURL scope_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  ServiceWorkerRemoteProviderEndpoint remote_endpoint_;
  std::unique_ptr<MockNetworkURLLoaderFactory> network_loader_factory_;
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
  helper_->context()->RemoveProviderHost(helper_->mock_render_process_id(),
                                         provider_host_->provider_id());

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

}  // namespace content
