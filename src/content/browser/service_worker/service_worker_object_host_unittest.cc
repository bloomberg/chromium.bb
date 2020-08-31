// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/fake_embedded_worker_instance_client.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_state.mojom.h"

namespace content {
namespace service_worker_object_host_unittest {

static void SaveStatusCallback(bool* called,
                               blink::ServiceWorkerStatusCode* out,
                               blink::ServiceWorkerStatusCode status) {
  *called = true;
  *out = status;
}

void SetUpDummyMessagePort(std::vector<blink::MessagePortChannel>* ports) {
  // Let the other end of the pipe close.
  blink::MessagePortDescriptorPair pipe;
  ports->push_back(blink::MessagePortChannel(pipe.TakePort0()));
}

// A worker that holds on to ExtendableMessageEventPtr so it doesn't get
// destroyed after the message event handler runs.
class MessageEventWorker : public FakeServiceWorker {
 public:
  MessageEventWorker(EmbeddedWorkerTestHelper* helper)
      : FakeServiceWorker(helper) {}

  void DispatchExtendableMessageEvent(
      blink::mojom::ExtendableMessageEventPtr event,
      blink::mojom::ServiceWorker::DispatchExtendableMessageEventCallback
          callback) override {
    events_.push_back(std::move(event));
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
  }

  const std::vector<blink::mojom::ExtendableMessageEventPtr>& events() {
    return events_;
  }

 private:
  std::vector<blink::mojom::ExtendableMessageEventPtr> events_;
};

// An instance client that breaks the Mojo connection upon receiving the
// Start() message.
class FailStartInstanceClient : public FakeEmbeddedWorkerInstanceClient {
 public:
  FailStartInstanceClient(EmbeddedWorkerTestHelper* helper)
      : FakeEmbeddedWorkerInstanceClient(helper) {}

  void StartWorker(blink::mojom::EmbeddedWorkerStartParamsPtr params) override {
    // Don't save the Mojo ptrs. The connection breaks.
  }
};

class MockServiceWorkerObject : public blink::mojom::ServiceWorkerObject {
 public:
  explicit MockServiceWorkerObject(
      blink::mojom::ServiceWorkerObjectInfoPtr info)
      : info_(std::move(info)),
        state_(info_->state),
        receiver_(this, std::move(info_->receiver)) {}
  ~MockServiceWorkerObject() override = default;

  blink::mojom::ServiceWorkerState state() const { return state_; }

 private:
  // Implements blink::mojom::ServiceWorkerObject.
  void StateChanged(blink::mojom::ServiceWorkerState state) override {
    state_ = state;
  }

  blink::mojom::ServiceWorkerObjectInfoPtr info_;
  blink::mojom::ServiceWorkerState state_;
  mojo::AssociatedReceiver<blink::mojom::ServiceWorkerObject> receiver_;
};

class ServiceWorkerObjectHostTest : public testing::Test {
 public:
  ServiceWorkerObjectHostTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void Initialize(std::unique_ptr<EmbeddedWorkerTestHelper> helper) {
    helper_ = std::move(helper);
  }

  void SetUpRegistration(const GURL& scope, const GURL& script_url) {
    helper_->context()->storage()->LazyInitializeForTest();

    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope;
    registration_ = CreateNewServiceWorkerRegistration(
        helper_->context()->registry(), options);
    version_ = CreateNewServiceWorkerVersion(
        helper_->context()->registry(), registration_.get(), script_url,
        blink::mojom::ScriptType::kClassic);
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
    records.push_back(storage::mojom::ServiceWorkerResourceRecord::New(
        10, version_->script_url(), 100));
    version_->script_cache_map()->SetResources(records);
    version_->SetMainScriptResponse(
        std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
            network::mojom::URLResponseHead()));
    version_->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    version_->SetStatus(ServiceWorkerVersion::INSTALLING);

    // Make the registration findable via storage functions.
    base::Optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop run_loop;
    helper_->context()->registry()->StoreRegistration(
        registration_.get(), version_.get(),
        ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());
  }

  void TearDown() override {
    registration_ = nullptr;
    version_ = nullptr;
    helper_.reset();
  }

  void CallDispatchExtendableMessageEvent(
      ServiceWorkerObjectHost* object_host,
      ::blink::TransferableMessage message,
      base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback) {
    object_host->DispatchExtendableMessageEvent(std::move(message),
                                                std::move(callback));
  }

  size_t GetReceiverCount(ServiceWorkerObjectHost* object_host) {
    return object_host->receivers_.size();
  }

  ServiceWorkerObjectHost* GetServiceWorkerObjectHost(
      ServiceWorkerContainerHost* container_host,
      int64_t version_id) {
    auto iter = container_host->service_worker_object_hosts_.find(version_id);
    if (iter != container_host->service_worker_object_hosts_.end())
      return iter->second.get();
    return nullptr;
  }

  void SetContainerHostRenderFrameId(ServiceWorkerContainerHost* container_host,
                                     int render_frame_id) {
    container_host->frame_id_ = render_frame_id;
  }

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
  GetRegistrationFromRemote(
      blink::mojom::ServiceWorkerContainerHost* container_host,
      const GURL& scope) {
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info;
    base::RunLoop run_loop;
    container_host->GetRegistration(
        scope, base::BindOnce(
                   [](base::OnceClosure quit_closure,
                      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr*
                          out_registration_info,
                      blink::mojom::ServiceWorkerErrorType error,
                      const base::Optional<std::string>& error_msg,
                      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
                          registration) {
                     ASSERT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
                               error);
                     *out_registration_info = std::move(registration);
                     std::move(quit_closure).Run();
                   },
                   run_loop.QuitClosure(), &registration_info));
    run_loop.Run();
    EXPECT_TRUE(registration_info);
    return registration_info;
  }

  void CallOnConnectionError(ServiceWorkerContainerHost* container_host,
                             int64_t version_id) {
    // ServiceWorkerObjectHost has the last reference to the version.
    ServiceWorkerObjectHost* object_host =
        GetServiceWorkerObjectHost(container_host, version_id);
    EXPECT_TRUE(object_host->version_->HasOneRef());

    // Make sure that OnConnectionError induces destruction of the version and
    // the object host.
    object_host->receivers_.Clear();
    object_host->OnConnectionError();
  }

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerObjectHostTest);
};

TEST_F(ServiceWorkerObjectHostTest, OnVersionStateChanged) {
  const GURL scope("https://www.example.com/");
  const GURL script_url("https://www.example.com/service_worker.js");
  Initialize(std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath()));
  SetUpRegistration(scope, script_url);
  registration_->SetInstallingVersion(version_);

  ServiceWorkerRemoteProviderEndpoint remote_endpoint;
  base::WeakPtr<ServiceWorkerContainerHost> container_host =
      CreateContainerHostForWindow(
          helper_->mock_render_process_id(), true /* is_parent_frame_secure */,
          helper_->context()->AsWeakPtr(), &remote_endpoint);
  container_host->UpdateUrls(scope, net::SiteForCookies::FromUrl(scope),
                             url::Origin::Create(scope));
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info =
      GetRegistrationFromRemote(remote_endpoint.host_remote()->get(), scope);
  // |version_| is the installing version of |registration_| now.
  EXPECT_TRUE(registration_info->installing);
  EXPECT_EQ(version_->version_id(), registration_info->installing->version_id);
  auto mock_object = std::make_unique<MockServiceWorkerObject>(
      std::move(registration_info->installing));

  // ...update state to installed.
  EXPECT_EQ(blink::mojom::ServiceWorkerState::kInstalling,
            mock_object->state());
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::mojom::ServiceWorkerState::kInstalled, mock_object->state());
}

// Tests postMessage() from a service worker to itself.
TEST_F(ServiceWorkerObjectHostTest,
       DispatchExtendableMessageEvent_FromServiceWorker) {
  const GURL scope("https://www.example.com/");
  const GURL script_url("https://www.example.com/service_worker.js");
  Initialize(std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath()));
  SetUpRegistration(scope, script_url);
  auto* worker =
      helper_->AddNewPendingServiceWorker<MessageEventWorker>(helper_.get());

  base::SimpleTestTickClock tick_clock;
  // Set mock clock on version_ to check timeout behavior.
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  version_->SetTickClockForTesting(&tick_clock);

  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));

  const base::TimeDelta kRequestTimeout = base::TimeDelta::FromMinutes(5);
  const base::TimeDelta kFourSeconds = base::TimeDelta::FromSeconds(4);

  // After startup, the remaining timeout is expected to be kRequestTimeout.
  EXPECT_EQ(kRequestTimeout, version_->remaining_timeout());

  // This test will simulate the worker calling postMessage() on itself.
  // Prepare |object_host|. This corresponds to the JavaScript ServiceWorker
  // object for the service worker, inside the service worker's own
  // execution context (e.g., self.registration.active inside the active
  // worker and self.serviceWorker).
  ServiceWorkerContainerHost* container_host =
      version_->provider_host()->container_host();
  blink::mojom::ServiceWorkerObjectInfoPtr info =
      container_host->GetOrCreateServiceWorkerObjectHost(version_)
          ->CreateCompleteObjectInfoToSend();
  ServiceWorkerObjectHost* object_host =
      GetServiceWorkerObjectHost(container_host, version_->version_id());
  EXPECT_EQ(2u, GetReceiverCount(object_host));

  {
    // Advance clock by four seconds.
    tick_clock.Advance(kFourSeconds);
    base::TimeDelta remaining_time = version_->remaining_timeout();
    EXPECT_EQ(kRequestTimeout - kFourSeconds, remaining_time);

    // Now simulate the service worker calling postMessage() to itself,
    // by calling DispatchExtendableMessageEvent on |object_host|.
    // Expected status is kOk.
    blink::TransferableMessage message;
    SetUpDummyMessagePort(&message.ports);
    base::RunLoop loop;
    CallDispatchExtendableMessageEvent(
        object_host, std::move(message),
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
          loop.Quit();
        }));
    loop.Run();

    // The dispatched ExtendableMessageEvent should be received
    // by the worker, and the source service worker object info
    // should be for its own version id.
    EXPECT_EQ(3u, GetReceiverCount(object_host));

    // Message event triggered by the service worker itself should not extend
    // the timeout.
    EXPECT_EQ(remaining_time, version_->remaining_timeout());
  }

  {
    // Advance clock by request timeout.
    tick_clock.Advance(kRequestTimeout);
    base::TimeDelta remaining_time = version_->remaining_timeout();
    EXPECT_EQ(kRequestTimeout - kFourSeconds - kRequestTimeout, remaining_time);

    // Now simulate the service worker calling postMessage() to itself,
    // by calling DispatchExtendableMessageEvent on |object_host|.
    // Expected status is kErrorTimeout.
    blink::TransferableMessage message;
    SetUpDummyMessagePort(&message.ports);
    base::RunLoop loop;
    CallDispatchExtendableMessageEvent(
        object_host, std::move(message),
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout, status);
          loop.Quit();
        }));
    loop.Run();

    // The dispatched ExtendableMessageEvent should not be received
    // by the worker, and the source service worker object info
    // should be for its own version id.
    EXPECT_EQ(3u, GetReceiverCount(object_host));

    // Message event triggered by the service worker itself should not extend
    // the timeout.
    EXPECT_EQ(remaining_time, version_->remaining_timeout());
  }

  const std::vector<blink::mojom::ExtendableMessageEventPtr>& events =
      worker->events();
  EXPECT_EQ(1u, events.size());
  EXPECT_FALSE(events[0]->source_info_for_client);
  EXPECT_TRUE(events[0]->source_info_for_service_worker);
  EXPECT_EQ(version_->version_id(),
            events[0]->source_info_for_service_worker->version_id);

  // Clean up.
  StopServiceWorker(version_.get());
}

// Tests postMessage() from a page to a service worker.
TEST_F(ServiceWorkerObjectHostTest, DispatchExtendableMessageEvent_FromClient) {
  const GURL scope("https://www.example.com/");
  const GURL script_url("https://www.example.com/service_worker.js");
  Initialize(std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath()));
  SetUpRegistration(scope, script_url);
  auto* worker =
      helper_->AddNewPendingServiceWorker<MessageEventWorker>(helper_.get());

  // Prepare a ServiceWorkerContainerHost for a window client. A
  // WebContents/RenderFrameHost must be created too because it's needed for
  // DispatchExtendableMessageEvent to populate ExtendableMessageEvent#source.
  RenderViewHostTestEnabler rvh_test_enabler;
  std::unique_ptr<WebContents> web_contents(
      WebContentsTester::CreateTestWebContents(helper_->browser_context(),
                                               nullptr));
  RenderFrameHost* frame_host = web_contents->GetMainFrame();
  ServiceWorkerRemoteProviderEndpoint remote_endpoint;
  base::WeakPtr<ServiceWorkerContainerHost> container_host =
      CreateContainerHostForWindow(
          frame_host->GetProcess()->GetID(), true /* is_parent_frame_secure */,
          helper_->context()->AsWeakPtr(), &remote_endpoint);
  SetContainerHostRenderFrameId(container_host.get(),
                                frame_host->GetRoutingID());
  container_host->UpdateUrls(scope, net::SiteForCookies::FromUrl(scope),
                             url::Origin::Create(scope));

  // Prepare a ServiceWorkerObjectHost for the worker.
  blink::mojom::ServiceWorkerObjectInfoPtr info =
      container_host->GetOrCreateServiceWorkerObjectHost(version_)
          ->CreateCompleteObjectInfoToSend();
  ServiceWorkerObjectHost* object_host =
      GetServiceWorkerObjectHost(container_host.get(), version_->version_id());

  // Simulate postMessage() from the window client to the worker.
  blink::TransferableMessage message;
  SetUpDummyMessagePort(&message.ports);
  bool called = false;
  blink::ServiceWorkerStatusCode status =
      blink::ServiceWorkerStatusCode::kErrorFailed;
  CallDispatchExtendableMessageEvent(
      object_host, std::move(message),
      base::BindOnce(&SaveStatusCallback, &called, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);

  // The worker should have received an ExtendableMessageEvent whose
  // source is |container_host|.
  const std::vector<blink::mojom::ExtendableMessageEventPtr>& events =
      worker->events();
  EXPECT_EQ(1u, events.size());
  EXPECT_FALSE(events[0]->source_info_for_service_worker);
  EXPECT_TRUE(events[0]->source_info_for_client);
  EXPECT_EQ(container_host->client_uuid(),
            events[0]->source_info_for_client->client_uuid);
  EXPECT_EQ(container_host->GetClientType(),
            events[0]->source_info_for_client->client_type);
}

// This is a regression test for https://crbug.com/1056598.
TEST_F(ServiceWorkerObjectHostTest, OnConnectionError) {
  const GURL scope("https://www.example.com/");
  const GURL script_url("https://www.example.com/service_worker.js");
  Initialize(std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath()));
  SetUpRegistration(scope, script_url);

  // Create the provider host.
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));

  // Set up the case where the last reference to the version is owned by the
  // service worker object host.
  ServiceWorkerContainerHost* container_host =
      version_->provider_host()->container_host();
  ServiceWorkerVersion* version_rawptr = version_.get();
  version_ = nullptr;
  ASSERT_TRUE(version_rawptr->HasOneRef());

  // Simulate the connection error that induces the object host destruction.
  // This shouldn't crash.
  CallOnConnectionError(container_host, version_rawptr->version_id());
  base::RunLoop().RunUntilIdle();
}

}  // namespace service_worker_object_host_unittest
}  // namespace content
