// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_controllee_request_handler.h"

#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_response_type.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {
namespace service_worker_controllee_request_handler_unittest {

class ServiceWorkerControlleeRequestHandlerTest : public testing::Test {
 public:
  class ServiceWorkerRequestTestResources {
   public:
    ServiceWorkerRequestTestResources(
        ServiceWorkerControlleeRequestHandlerTest* test,
        const GURL& url,
        ResourceType type,
        network::mojom::FetchRequestMode fetch_type =
            network::mojom::FetchRequestMode::kNoCors)
        : resource_type_(type),
          request_(test->url_request_context_.CreateRequest(
              url,
              net::DEFAULT_PRIORITY,
              &test->url_request_delegate_,
              TRAFFIC_ANNOTATION_FOR_TESTS)),
          handler_(new ServiceWorkerControlleeRequestHandler(
              test->context()->AsWeakPtr(),
              test->provider_host_,
              fetch_type,
              network::mojom::FetchCredentialsMode::kOmit,
              network::mojom::FetchRedirectMode::kFollow,
              std::string() /* integrity */,
              false /* keepalive */,
              type,
              blink::mojom::RequestContextType::HYPERLINK,
              network::mojom::RequestContextFrameType::kTopLevel,
              scoped_refptr<network::ResourceRequestBody>())) {}

    ServiceWorkerNavigationLoader* MaybeCreateLoader() {
      network::ResourceRequest resource_request;
      resource_request.url = request_->url();
      resource_request.resource_type = static_cast<int>(resource_type_);
      resource_request.headers = request()->extra_request_headers();
      handler_->MaybeCreateLoader(resource_request, nullptr,
                                  base::DoNothing(), base::DoNothing());
      return handler_->loader();
    }

    void ResetHandler() { handler_.reset(nullptr); }

    net::URLRequest* request() const { return request_.get(); }

   private:
    const ResourceType resource_type_;
    std::unique_ptr<net::URLRequest> request_;
    std::unique_ptr<ServiceWorkerControlleeRequestHandler> handler_;
  };

  ServiceWorkerControlleeRequestHandlerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    SetUpWithHelper(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  void SetUpWithHelper(EmbeddedWorkerTestHelper* helper) {
    helper_.reset(helper);

    // A new unstored registration/version.
    scope_ = GURL("https://host/scope/");
    script_url_ = GURL("https://host/script.js");
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope_;
    registration_ =
        new ServiceWorkerRegistration(options, 1L, context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(registration_.get(), script_url_,
                                        blink::mojom::ScriptType::kClassic, 1L,
                                        context()->AsWeakPtr());

    context()->storage()->LazyInitializeForTest(base::DoNothing());
    base::RunLoop().RunUntilIdle();

    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(WriteToDiskCacheSync(
        context()->storage(), version_->script_url(),
        context()->storage()->NewResourceId(), {} /* headers */, "I'm a body",
        "I'm a meta data"));
    version_->script_cache_map()->SetResources(records);
    version_->SetMainScriptHttpResponseInfo(
        EmbeddedWorkerTestHelper::CreateHttpResponseInfo());

    // An empty host.
    remote_endpoints_.emplace_back();
    provider_host_ = CreateProviderHostForWindow(
        helper_->mock_render_process_id(), true /* is_parent_frame_secure */,
        helper_->context()->AsWeakPtr(), &remote_endpoints_.back());
  }

  void TearDown() override {
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }

  void SetProviderHostIsSecure(ServiceWorkerProviderHost* host,
                               bool is_secure) {
    host->is_parent_frame_secure_ = is_secure;
  }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  net::URLRequestContext url_request_context_;
  net::TestDelegate url_request_delegate_;
  GURL scope_;
  GURL script_url_;
  std::vector<ServiceWorkerRemoteProviderEndpoint> remote_endpoints_;
};

class ServiceWorkerTestContentBrowserClient : public TestContentBrowserClient {
 public:
  ServiceWorkerTestContentBrowserClient() {}
  bool AllowServiceWorker(
      const GURL& scope,
      const GURL& first_party,
      content::ResourceContext* context,
      base::RepeatingCallback<WebContents*()> wc_getter) override {
    return false;
  }
};

TEST_F(ServiceWorkerControlleeRequestHandlerTest, DisallowServiceWorker) {
  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);

  // Store an activated worker.
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  context()->storage()->StoreRegistration(registration_.get(), version_.get(),
                                          base::DoNothing());
  base::RunLoop().RunUntilIdle();

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  ServiceWorkerNavigationLoader* loader = test_resources.MaybeCreateLoader();
  ASSERT_TRUE(loader);

  EXPECT_FALSE(loader->ShouldFallbackToNetwork());
  EXPECT_FALSE(loader->ShouldForwardToServiceWorker());
  EXPECT_FALSE(version_->HasControllee());
  base::RunLoop().RunUntilIdle();

  // Verify we did not use the worker.
  EXPECT_TRUE(loader->ShouldFallbackToNetwork());
  EXPECT_FALSE(loader->ShouldForwardToServiceWorker());
  EXPECT_FALSE(version_->HasControllee());

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, InsecureContext) {
  // Store an activated worker.
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  context()->storage()->StoreRegistration(registration_.get(), version_.get(),
                                          base::DoNothing());
  base::RunLoop().RunUntilIdle();

  SetProviderHostIsSecure(provider_host_.get(), false);

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  ServiceWorkerNavigationLoader* loader = test_resources.MaybeCreateLoader();
  ASSERT_TRUE(loader);

  EXPECT_FALSE(loader->ShouldFallbackToNetwork());
  EXPECT_FALSE(loader->ShouldForwardToServiceWorker());
  EXPECT_FALSE(version_->HasControllee());
  base::RunLoop().RunUntilIdle();

  // Verify we did not use the worker.
  EXPECT_TRUE(loader->ShouldFallbackToNetwork());
  EXPECT_FALSE(loader->ShouldForwardToServiceWorker());
  EXPECT_FALSE(version_->HasControllee());
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, ActivateWaitingVersion) {
  // Store a registration that is installed but not activated yet.
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  registration_->SetWaitingVersion(version_);
  context()->storage()->StoreRegistration(registration_.get(), version_.get(),
                                          base::DoNothing());
  base::RunLoop().RunUntilIdle();

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  ServiceWorkerNavigationLoader* loader = test_resources.MaybeCreateLoader();
  ASSERT_TRUE(loader);

  EXPECT_FALSE(loader->ShouldFallbackToNetwork());
  EXPECT_FALSE(loader->ShouldForwardToServiceWorker());
  EXPECT_FALSE(version_->HasControllee());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, version_->status());
  EXPECT_FALSE(loader->ShouldFallbackToNetwork());
  EXPECT_TRUE(loader->ShouldForwardToServiceWorker());
  EXPECT_TRUE(version_->HasControllee());

  test_resources.ResetHandler();
}

// Test that an installing registration is associated with a provider host.
TEST_F(ServiceWorkerControlleeRequestHandlerTest, InstallingRegistration) {
  // Create an installing registration.
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  registration_->SetInstallingVersion(version_);
  context()->storage()->NotifyInstallingRegistration(registration_.get());

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  ServiceWorkerNavigationLoader* job = test_resources.MaybeCreateLoader();

  base::RunLoop().RunUntilIdle();

  // The handler should have fallen back to network and destroyed the job. The
  // provider host should not be controlled. However it should add the
  // registration as a matching registration so it can be used for .ready and
  // claim().
  EXPECT_FALSE(job);
  EXPECT_FALSE(version_->HasControllee());
  EXPECT_FALSE(provider_host_->controller());
  EXPECT_EQ(registration_.get(), provider_host_->MatchRegistration());
}

// Test to not regress crbug/414118.
TEST_F(ServiceWorkerControlleeRequestHandlerTest, DeletedProviderHost) {
  // Store a registration so the call to FindRegistrationForDocument will read
  // from the database.
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  context()->storage()->StoreRegistration(registration_.get(), version_.get(),
                                          base::DoNothing());
  base::RunLoop().RunUntilIdle();
  version_ = nullptr;
  registration_ = nullptr;

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  ServiceWorkerNavigationLoader* loader = test_resources.MaybeCreateLoader();
  ASSERT_TRUE(loader);

  EXPECT_FALSE(loader->ShouldFallbackToNetwork());
  EXPECT_FALSE(loader->ShouldForwardToServiceWorker());

  // Shouldn't crash if the ProviderHost is deleted prior to completion of
  // the database lookup.
  context()->RemoveProviderHost(provider_host_->provider_id());
  EXPECT_FALSE(provider_host_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(loader->ShouldFallbackToNetwork());
  EXPECT_FALSE(loader->ShouldForwardToServiceWorker());
}

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
TEST_F(ServiceWorkerControlleeRequestHandlerTest, FallbackWithOfflineHeader) {
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  context()->storage()->StoreRegistration(registration_.get(), version_.get(),
                                          base::DoNothing());
  base::RunLoop().RunUntilIdle();
  version_ = NULL;
  registration_ = NULL;

  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  // Sets an offline header to indicate force loading offline page.
  test_resources.request()->SetExtraRequestHeaderByName(
      "X-Chrome-offline", "reason=download", true);
  ServiceWorkerNavigationLoader* loader = test_resources.MaybeCreateLoader();

  EXPECT_FALSE(loader);
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, FallbackWithNoOfflineHeader) {
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  context()->storage()->StoreRegistration(registration_.get(), version_.get(),
                                          base::DoNothing());
  base::RunLoop().RunUntilIdle();
  version_ = NULL;
  registration_ = NULL;

  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  // Empty offline header value should not cause fallback.
  test_resources.request()->SetExtraRequestHeaderByName("X-Chrome-offline", "",
                                                        true);
  ServiceWorkerNavigationLoader* loader = test_resources.MaybeCreateLoader();

  EXPECT_TRUE(loader);
}
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGE)

}  // namespace service_worker_controllee_request_handler_unittest
}  // namespace content
