// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_context.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_back_forward_cache_loader_helper.h"
#include "third_party/blink/public/platform/web_resource_request_sender.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_response.h"

namespace blink {

namespace {

class TestSharedURLLoaderFactory : public network::TestURLLoaderFactory,
                                   public network::SharedURLLoaderFactory {
 public:
  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    network::TestURLLoaderFactory::CreateLoaderAndStart(
        std::move(receiver), routing_id, request_id, options, url_request,
        std::move(client), traffic_annotation);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>) override {
    NOTREACHED();
  }

  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    NOTREACHED();
    return nullptr;
  }

 private:
  friend class base::RefCounted<TestSharedURLLoaderFactory>;
  ~TestSharedURLLoaderFactory() override = default;
};

class MockPendingSharedURLLoaderFactory
    : public network::PendingSharedURLLoaderFactory {
 public:
  explicit MockPendingSharedURLLoaderFactory()
      : factory_(base::MakeRefCounted<TestSharedURLLoaderFactory>()) {}

  scoped_refptr<TestSharedURLLoaderFactory> factory() const { return factory_; }

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override {
    return factory_;
  }

  scoped_refptr<TestSharedURLLoaderFactory> factory_;
};

class MockResourceRequestSender : public WebResourceRequestSender {
 public:
  void CreatePendingRequest(scoped_refptr<WebRequestPeer> peer) {
    peer_ = std::move(peer);
  }

  void DeletePendingRequest(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    peer_.reset();
  }

 private:
  scoped_refptr<WebRequestPeer> peer_;
};

}  // namespace

class SyncLoadContextTest : public testing::Test {
 public:
  SyncLoadContextTest() : loading_thread_("loading thread") {}

  void SetUp() override {
    ASSERT_TRUE(loading_thread_.StartAndWaitForTesting());
  }

  void StartAsyncWithWaitableEventOnLoadingThread(
      std::unique_ptr<network::ResourceRequest> request,
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      SyncLoadResponse* out_response,
      SyncLoadContext** context_for_redirect,
      base::WaitableEvent* redirect_or_response_event) {
    loading_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SyncLoadContext::StartAsyncWithWaitableEvent, std::move(request),
            MSG_ROUTING_NONE, loading_thread_.task_runner(),
            TRAFFIC_ANNOTATION_FOR_TESTS, 0 /* loader_options */,
            std::move(pending_factory),
            WebVector<std::unique_ptr<URLLoaderThrottle>>(), out_response,
            context_for_redirect, redirect_or_response_event,
            nullptr /* terminate_sync_load_event */,
            base::TimeDelta::FromSeconds(60) /* timeout */,
            mojo::NullRemote() /* download_to_blob_registry */,
            WebVector<WebString>() /* cors_exempt_header_list */,
            std::make_unique<ResourceLoadInfoNotifierWrapper>(
                /*resource_load_info_notifier=*/nullptr,
                task_environment_.GetMainThreadTaskRunner())));
  }

  static void RunSyncLoadContextViaDataPipe(
      network::ResourceRequest* request,
      SyncLoadResponse* response,
      SyncLoadContext** context_for_redirect,
      std::string expected_data,
      base::WaitableEvent* redirect_or_response_event,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    DCHECK(task_runner->BelongsToCurrentThread());
    auto* context = new SyncLoadContext(
        request, std::make_unique<MockPendingSharedURLLoaderFactory>(),
        response, context_for_redirect, redirect_or_response_event,
        nullptr /* terminate_sync_load_event */,
        base::TimeDelta::FromSeconds(60) /* timeout */,
        mojo::NullRemote() /* download_to_blob_registry */, task_runner);

    auto mock_resource_request_sender =
        std::make_unique<MockResourceRequestSender>();
    mock_resource_request_sender->CreatePendingRequest(
        base::WrapRefCounted(context));
    context->resource_request_sender_ = std::move(mock_resource_request_sender);

    // Simulate the response.
    context->OnReceivedResponse(network::mojom::URLResponseHead::New());
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr /* options */, producer_handle,
                                   consumer_handle));
    context->OnStartLoadingResponseBody(std::move(consumer_handle));
    context->OnCompletedRequest(network::URLLoaderCompletionStatus(net::OK));

    mojo::BlockingCopyFromString(expected_data, producer_handle);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::Thread loading_thread_;
};

TEST_F(SyncLoadContextTest, StartAsyncWithWaitableEvent) {
  GURL expected_url = GURL("https://example.com");
  std::string expected_data = "foobarbaz";

  // Create and exercise SyncLoadContext on the |loading_thread_|.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = expected_url;
  auto pending_factory = std::make_unique<MockPendingSharedURLLoaderFactory>();
  pending_factory->factory()->AddResponse(expected_url.spec(), expected_data);
  SyncLoadResponse response;
  SyncLoadContext* context_for_redirect = nullptr;
  base::WaitableEvent redirect_or_response_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  StartAsyncWithWaitableEventOnLoadingThread(
      std::move(request), std::move(pending_factory), &response,
      &context_for_redirect, &redirect_or_response_event);

  // Wait until the response is received.
  redirect_or_response_event.Wait();

  // Check if |response| is set properly after the WaitableEvent fires.
  EXPECT_EQ(net::OK, response.error_code);
  const char* response_data = nullptr;
  size_t size = response.data.GetSomeData(response_data, 0);
  EXPECT_EQ(expected_data, std::string(response_data, size));
}

TEST_F(SyncLoadContextTest, ResponseBodyViaDataPipe) {
  GURL expected_url = GURL("https://example.com");
  std::string expected_data = "foobarbaz";

  // Create and exercise SyncLoadContext on the |loading_thread_|.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = expected_url;
  SyncLoadResponse response;
  base::WaitableEvent redirect_or_response_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  SyncLoadContext* context_for_redirect = nullptr;
  loading_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncLoadContextTest::RunSyncLoadContextViaDataPipe,
                     request.get(), &response, &context_for_redirect,
                     expected_data, &redirect_or_response_event,
                     loading_thread_.task_runner()));

  // Wait until the response is received.
  redirect_or_response_event.Wait();

  // Check if |response| is set properly after the WaitableEvent fires.
  EXPECT_EQ(net::OK, response.error_code);
  const char* response_data = nullptr;
  size_t size = response.data.GetSomeData(response_data, 0);
  EXPECT_EQ(expected_data, std::string(response_data, size));
}

}  // namespace blink
