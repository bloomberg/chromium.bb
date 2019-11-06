// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_request_handler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/navigation_params.mojom.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_util.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace content {
namespace service_worker_request_handler_unittest {

class ServiceWorkerRequestHandlerTest : public testing::Test {
 public:
  ServiceWorkerRequestHandlerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  void TearDown() override { helper_.reset(); }

  ServiceWorkerContextCore* context() const { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() const {
    return helper_->context_wrapper();
  }

 protected:
  static std::unique_ptr<ServiceWorkerNavigationHandleCore>
  CreateNavigationHandleCore(ServiceWorkerContextWrapper* context_wrapper) {
    std::unique_ptr<ServiceWorkerNavigationHandleCore> navigation_handle_core;
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            [](ServiceWorkerContextWrapper* wrapper) {
              return std::make_unique<ServiceWorkerNavigationHandleCore>(
                  nullptr, wrapper);
            },
            base::RetainedRef(context_wrapper)),
        base::BindOnce(
            [](std::unique_ptr<ServiceWorkerNavigationHandleCore>* dest,
               std::unique_ptr<ServiceWorkerNavigationHandleCore> src) {
              *dest = std::move(src);
            },
            &navigation_handle_core));
    base::RunLoop().RunUntilIdle();
    return navigation_handle_core;
  }

  void InitializeHandlerForNavigationSimpleTest(const std::string& url,
                                                bool expected_handler_created) {
    std::unique_ptr<ServiceWorkerNavigationHandleCore> navigation_handle_core =
        CreateNavigationHandleCore(helper_->context_wrapper());
    base::WeakPtr<ServiceWorkerProviderHost> service_worker_provider_host;
    GURL gurl(url);
    CommonNavigationParams common_params;
    auto begin_params = mojom::BeginNavigationParams::New();
    begin_params->request_context_type =
        blink::mojom::RequestContextType::HYPERLINK;
    url::Origin origin = url::Origin::Create(gurl);
    NavigationRequestInfo request_info(
        common_params, std::move(begin_params), gurl,
        net::NetworkIsolationKey(origin, origin), true /* is_main_frame */,
        false /* parent_is_main_frame */, true /* are_ancestors_secure */,
        -1 /* frame_tree_node_id */, false /* is_for_guests_only */,
        false /* report_raw_headers */, false /* is_prerendering */,
        false /* upgrade_if_insecure */, nullptr /* blob_url_loader_factory */,
        base::UnguessableToken::Create() /* devtools_navigation_token */,
        base::UnguessableToken::Create() /* devtools_frame_token */);
    std::unique_ptr<NavigationLoaderInterceptor> interceptor =
        ServiceWorkerRequestHandler::CreateForNavigationIO(
            GURL(url), navigation_handle_core.get(), request_info,
            &service_worker_provider_host);
    EXPECT_EQ(expected_handler_created, !!interceptor.get());
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  net::URLRequestContext url_request_context_;
  net::TestDelegate url_request_delegate_;
  storage::BlobStorageContext blob_storage_context_;
  ServiceWorkerRemoteProviderEndpoint remote_endpoint_;
};

TEST_F(ServiceWorkerRequestHandlerTest, InitializeForNavigation_HTTP) {
  InitializeHandlerForNavigationSimpleTest("http://host/scope/doc", true);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeForNavigation_HTTPS) {
  InitializeHandlerForNavigationSimpleTest("https://host/scope/doc", true);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeForNavigation_FTP) {
  InitializeHandlerForNavigationSimpleTest("ftp://host/scope/doc", false);
}

TEST_F(ServiceWorkerRequestHandlerTest,
       InitializeForNavigation_ExternalFileScheme) {
  bool expected_handler_created = false;
#if defined(OS_CHROMEOS)
  expected_handler_created = true;
#endif  // OS_CHROMEOS
  InitializeHandlerForNavigationSimpleTest("externalfile:drive/doc",
                                           expected_handler_created);
}

}  // namespace service_worker_request_handler_unittest
}  // namespace content
