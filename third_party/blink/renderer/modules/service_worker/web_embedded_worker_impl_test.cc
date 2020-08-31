// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/exported/web_embedded_worker_impl.h"

#include <memory>

#include "base/feature_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker_mode.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"
#include "third_party/blink/public/web/web_embedded_worker_start_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {
namespace {

const char* kNotFoundScriptURL = "https://www.example.com/sw-404.js";

// A fake WebURLLoader which is used for off-main-thread script fetch tests.
class FakeWebURLLoader final : public WebURLLoader {
 public:
  FakeWebURLLoader() = default;
  ~FakeWebURLLoader() override = default;

  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequest::ExtraData> request_extra_data,
      int requestor_id,
      bool download_to_network_cache_only,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      WebURLLoaderClient*,
      WebURLResponse&,
      base::Optional<WebURLError>&,
      WebData&,
      int64_t& encoded_data_length,
      int64_t& encoded_body_length,
      WebBlobInfo& downloaded_blob) override {
    NOTREACHED();
  }

  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequest::ExtraData> request_extra_data,
      int requestor_id,
      bool download_to_network_cache_only,
      bool no_mime_sniffing,
      WebURLLoaderClient* client) override {
    if (request->url.spec() == kNotFoundScriptURL) {
      WebURLResponse response;
      response.SetMimeType("text/javascript");
      response.SetHttpStatusCode(404);
      client->DidReceiveResponse(response);
      client->DidFinishLoading(base::TimeTicks(), 0, 0, 0, false);
      return;
    }
    // Don't handle other requests intentionally to emulate ongoing load.
  }

  void SetDefersLoading(bool defers) override {}
  void DidChangePriority(WebURLRequest::Priority, int) override {}
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }
};

// A fake WebURLLoaderFactory which is used for off-main-thread script fetch
// tests.
class FakeWebURLLoaderFactory final : public WebURLLoaderFactory {
 public:
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest&,
      std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>) override {
    return std::make_unique<FakeWebURLLoader>();
  }
};

// A fake WebServiceWorkerFetchContext which is used for off-main-thread script
// fetch tests.
class FakeWebServiceWorkerFetchContext final
    : public WebServiceWorkerFetchContext {
 public:
  void SetTerminateSyncLoadEvent(base::WaitableEvent*) override {}
  void InitializeOnWorkerThread(AcceptLanguagesWatcher*) override {}
  WebURLLoaderFactory* GetURLLoaderFactory() override {
    return &fake_web_url_loader_factory_;
  }
  std::unique_ptr<WebURLLoaderFactory> WrapURLLoaderFactory(
      mojo::ScopedMessagePipeHandle url_loader_factory_handle) override {
    return nullptr;
  }
  void WillSendRequest(WebURLRequest&) override {}
  mojom::ControllerServiceWorkerMode GetControllerServiceWorkerMode()
      const override {
    return mojom::ControllerServiceWorkerMode::kNoController;
  }
  net::SiteForCookies SiteForCookies() const override {
    return net::SiteForCookies();
  }
  base::Optional<WebSecurityOrigin> TopFrameOrigin() const override {
    return base::Optional<WebSecurityOrigin>();
  }
  WebString GetAcceptLanguages() const override { return WebString(); }
  mojo::ScopedMessagePipeHandle TakePendingWorkerTimingReceiver(
      int request_id) override {
    return {};
  }
  void SetIsOfflineMode(bool is_offline_mode) override {}
  mojom::SubresourceLoaderUpdater* GetSubresourceLoaderUpdater() override {
    return nullptr;
  }

 private:
  FakeWebURLLoaderFactory fake_web_url_loader_factory_;
};

class MockServiceWorkerContextClient final
    : public WebServiceWorkerContextClient {
 public:
  MockServiceWorkerContextClient() = default;
  ~MockServiceWorkerContextClient() override = default;

  MOCK_METHOD2(WorkerReadyForInspectionOnInitiatorThread,
               void(mojo::ScopedMessagePipeHandle,
                    mojo::ScopedMessagePipeHandle));

  void WorkerContextStarted(WebServiceWorkerContextProxy* proxy,
                            scoped_refptr<base::SequencedTaskRunner>) override {
    mojo::PendingAssociatedRemote<mojom::blink::ServiceWorkerHost> host_remote;
    auto host_receiver = host_remote.InitWithNewEndpointAndPassReceiver();

    mojo::PendingAssociatedRemote<
        mojom::blink::ServiceWorkerRegistrationObjectHost>
        registration_object_host;
    auto registration_object_host_receiver =
        registration_object_host.InitWithNewEndpointAndPassReceiver();
    mojo::PendingAssociatedRemote<mojom::blink::ServiceWorkerRegistrationObject>
        registration_object;

    mojo::PendingAssociatedRemote<mojom::blink::ServiceWorkerObjectHost>
        service_worker_object_host;
    auto service_worker_object_host_receiver =
        service_worker_object_host.InitWithNewEndpointAndPassReceiver();
    mojo::PendingAssociatedRemote<mojom::blink::ServiceWorkerObject>
        service_worker_object;

    // Simulates calling blink.mojom.ServiceWorker.InitializeGlobalScope() to
    // unblock the service worker script evaluation.
    mojo::Remote<mojom::blink::ServiceWorker> service_worker;
    proxy->BindServiceWorker(
        service_worker.BindNewPipeAndPassReceiver().PassPipe());
    service_worker->InitializeGlobalScope(
        std::move(host_remote),
        mojom::blink::ServiceWorkerRegistrationObjectInfo::New(
            2 /* registration_id */, KURL("https://example.com"),
            mojom::blink::ServiceWorkerUpdateViaCache::kImports,
            std::move(registration_object_host),
            registration_object.InitWithNewEndpointAndPassReceiver(), nullptr,
            nullptr, nullptr),
        mojom::blink::ServiceWorkerObjectInfo::New(
            1 /* service_worker_version_id */,
            mojom::blink::ServiceWorkerState::kParsed,
            KURL("https://example.com"), std::move(service_worker_object_host),
            service_worker_object.InitWithNewEndpointAndPassReceiver()),
        mojom::blink::FetchHandlerExistence::EXISTS,
        /*subresource_loader_factories=*/nullptr,
        /*reporting_observer_receiver=*/mojo::NullReceiver());

    // To make the other side callable.
    mojo::AssociateWithDisconnectedPipe(host_receiver.PassHandle());
    mojo::AssociateWithDisconnectedPipe(
        registration_object_host_receiver.PassHandle());
    mojo::AssociateWithDisconnectedPipe(
        service_worker_object_host_receiver.PassHandle());
  }

  void FailedToFetchClassicScript() override {
    classic_script_load_failure_event_.Signal();
  }

  void DidEvaluateScript(bool /* success */) override {
    script_evaluated_event_.Signal();
  }

  scoped_refptr<WebServiceWorkerFetchContext>
  CreateWorkerFetchContextOnInitiatorThread() override {
    return base::MakeRefCounted<FakeWebServiceWorkerFetchContext>();
  }

  void WorkerContextDestroyed() override { termination_event_.Signal(); }

  // These methods must be called on the main thread.
  void WaitUntilScriptEvaluated() { script_evaluated_event_.Wait(); }
  void WaitUntilThreadTermination() { termination_event_.Wait(); }
  void WaitUntilFailedToLoadClassicScript() {
    classic_script_load_failure_event_.Wait();
  }

 private:
  base::WaitableEvent script_evaluated_event_;
  base::WaitableEvent termination_event_;
  base::WaitableEvent classic_script_load_failure_event_;
};

class WebEmbeddedWorkerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_client_ = std::make_unique<MockServiceWorkerContextClient>();
    worker_ = std::make_unique<WebEmbeddedWorkerImpl>(mock_client_.get());

    script_url_ = url_test_helpers::ToKURL("https://www.example.com/sw.js");
    WebURLResponse response(script_url_);
    response.SetMimeType("text/javascript");
    response.SetHttpStatusCode(200);
    url_test_helpers::RegisterMockedURLLoadWithCustomResponse(script_url_, "",
                                                              response);
  }

  std::unique_ptr<WebEmbeddedWorkerStartData> CreateStartData() {
    WebFetchClientSettingsObject outside_settings_object(
        network::mojom::ReferrerPolicy::kDefault,
        /*outgoing_referrer=*/WebURL(script_url_),
        blink::mojom::InsecureRequestsPolicy::kDoNotUpgrade);
    auto start_data = std::make_unique<WebEmbeddedWorkerStartData>(
        std::move(outside_settings_object));
    start_data->script_url = script_url_;
    start_data->user_agent = WebString("dummy user agent");
    start_data->script_type = mojom::ScriptType::kClassic;
    start_data->wait_for_debugger_mode =
        WebEmbeddedWorkerStartData::kDontWaitForDebugger;
    return start_data;
  }

  void TearDown() override {
    // Drain queued tasks posted from the worker thread in order to avoid tasks
    // bound with unretained objects from running after tear down. Worker
    // termination may post such tasks (see https://crbug,com/1007616).
    // TODO(nhiroki): Stop using synchronous WaitableEvent, and instead use
    // QuitClosure to wait until all the tasks run before test completion.
    test::RunPendingTasks();

    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  WebURL script_url_;
  std::unique_ptr<MockServiceWorkerContextClient> mock_client_;
  std::unique_ptr<WebEmbeddedWorkerImpl> worker_;
};

}  // namespace

TEST_F(WebEmbeddedWorkerImplTest, TerminateSoonAfterStart) {
  worker_->StartWorkerContext(
      CreateStartData(),
      /*installed_scripts_manager_params=*/nullptr,
      /*content_settings_proxy=*/mojo::ScopedMessagePipeHandle(),
      /*cache_storage_remote=*/mojo::ScopedMessagePipeHandle(),
      /*browser_interface_broker=*/mojo::ScopedMessagePipeHandle(),
      Thread::Current()->GetTaskRunner());
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  // Terminate the worker immediately after start.
  worker_->TerminateWorkerContext();
  worker_->WaitForShutdownForTesting();
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileWaitingForDebugger) {
  std::unique_ptr<WebEmbeddedWorkerStartData> start_data = CreateStartData();
  start_data->wait_for_debugger_mode =
      WebEmbeddedWorkerStartData::kWaitForDebugger;
  worker_->StartWorkerContext(
      std::move(start_data),
      /*installed_scripts_manager_params=*/nullptr,
      /*content_settings_proxy=*/mojo::ScopedMessagePipeHandle(),
      /*cache_storage_remote=*/mojo::ScopedMessagePipeHandle(),
      /*browser_interface_broker=*/mojo::ScopedMessagePipeHandle(),
      Thread::Current()->GetTaskRunner());
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  // Terminate the worker while waiting for the debugger.
  worker_->TerminateWorkerContext();
  worker_->WaitForShutdownForTesting();
}

TEST_F(WebEmbeddedWorkerImplTest, ScriptNotFound) {
  WebURL script_url = url_test_helpers::ToKURL(kNotFoundScriptURL);
  url_test_helpers::RegisterMockedErrorURLLoad(script_url);
  std::unique_ptr<WebEmbeddedWorkerStartData> start_data = CreateStartData();
  start_data->script_url = script_url;

  // Start worker and load the script.
  worker_->StartWorkerContext(
      std::move(start_data),
      /*installed_scripts_manager_params=*/nullptr,
      /*content_settings_proxy=*/mojo::ScopedMessagePipeHandle(),
      /*cache_storage_remote=*/mojo::ScopedMessagePipeHandle(),
      /*browser_interface_broker=*/mojo::ScopedMessagePipeHandle(),
      Thread::Current()->GetTaskRunner());
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  mock_client_->WaitUntilFailedToLoadClassicScript();

  // Terminate the worker for cleanup.
  worker_->TerminateWorkerContext();
  worker_->WaitForShutdownForTesting();
}

}  // namespace blink
