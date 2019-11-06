// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetch_browsertest_base.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/browser/loader/prefetch_url_loader_service.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_package/signed_exchange_handler.h"
#include "content/browser/web_package/signed_exchange_loader.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"

namespace content {

PrefetchBrowserTestBase::ResponseEntry::ResponseEntry() = default;

PrefetchBrowserTestBase::ResponseEntry::ResponseEntry(
    const std::string& content,
    const std::string& content_type,
    const std::vector<std::pair<std::string, std::string>>& headers)
    : content(content), content_type(content_type), headers(headers) {}

PrefetchBrowserTestBase::ResponseEntry::~ResponseEntry() = default;

PrefetchBrowserTestBase::ScopedSignedExchangeHandlerFactory::
    ScopedSignedExchangeHandlerFactory(SignedExchangeHandlerFactory* factory) {
  SignedExchangeLoader::SetSignedExchangeHandlerFactoryForTest(factory);
}

PrefetchBrowserTestBase::ScopedSignedExchangeHandlerFactory::
    ~ScopedSignedExchangeHandlerFactory() {
  SignedExchangeLoader::SetSignedExchangeHandlerFactoryForTest(nullptr);
}

PrefetchBrowserTestBase::PrefetchBrowserTestBase() = default;
PrefetchBrowserTestBase::~PrefetchBrowserTestBase() = default;

void PrefetchBrowserTestBase::SetUpOnMainThread() {
  ContentBrowserTest::SetUpOnMainThread();
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(
          shell()->web_contents()->GetBrowserContext()));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &PrefetchURLLoaderService::RegisterPrefetchLoaderCallbackForTest,
          base::RetainedRef(partition->GetPrefetchURLLoaderService()),
          base::BindRepeating(
              &PrefetchBrowserTestBase::OnPrefetchURLLoaderCalled,
              base::Unretained(this))));
}

void PrefetchBrowserTestBase::RegisterResponse(const std::string& url,
                                               const ResponseEntry& entry) {
  response_map_[url] = entry;
}

std::unique_ptr<net::test_server::HttpResponse>
PrefetchBrowserTestBase::ServeResponses(
    const net::test_server::HttpRequest& request) {
  auto found = response_map_.find(request.relative_url);
  if (found != response_map_.end()) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(found->second.content);
    response->set_content_type(found->second.content_type);
    for (const auto& header : found->second.headers) {
      response->AddCustomHeader(header.first, header.second);
    }
    return std::move(response);
  }
  return nullptr;
}

void PrefetchBrowserTestBase::WatchURLAndRunClosure(
    const std::string& relative_url,
    int* visit_count,
    base::OnceClosure closure,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == relative_url) {
    (*visit_count)++;
    if (closure)
      std::move(closure).Run();
  }
}

void PrefetchBrowserTestBase::OnPrefetchURLLoaderCalled() {
  prefetch_url_loader_called_++;
}

void PrefetchBrowserTestBase::RegisterRequestMonitor(
    net::EmbeddedTestServer* test_server,
    const std::string& path,
    int* count,
    base::RunLoop* waiter) {
  test_server->RegisterRequestMonitor(base::BindRepeating(
      &PrefetchBrowserTestBase::WatchURLAndRunClosure, base::Unretained(this),
      path, count, waiter ? waiter->QuitClosure() : base::RepeatingClosure()));
}

void PrefetchBrowserTestBase::RegisterRequestHandler(
    net::EmbeddedTestServer* test_server) {
  test_server->RegisterRequestHandler(base::BindRepeating(
      &PrefetchBrowserTestBase::ServeResponses, base::Unretained(this)));
}

void PrefetchBrowserTestBase::NavigateToURLAndWaitTitle(
    const GURL& url,
    const std::string& title) {
  base::string16 title16 = base::ASCIIToUTF16(title);
  TitleWatcher title_watcher(shell()->web_contents(), title16);
  // Execute the JavaScript code to triger the followup navigation from the
  // current page.
  EXPECT_TRUE(ExecuteScript(
      shell()->web_contents(),
      base::StringPrintf("location.href = '%s';", url.spec().c_str())));
  EXPECT_EQ(title16, title_watcher.WaitAndGetTitle());
}

void PrefetchBrowserTestBase::WaitUntilLoaded(const GURL& url) {
  bool result = false;
  ASSERT_TRUE(
      ExecuteScriptAndExtractBool(shell()->web_contents(),
                                  base::StringPrintf(R"(
new Promise((resolve) => {
  const url = '%s';
  if (performance.getEntriesByName(url).length > 0) {
    resolve();
    return;
  }
  new PerformanceObserver((list) => {
    if (list.getEntriesByName(url).length > 0) {
      resolve();
    }
  }).observe({ entryTypes: ['resource'] });
}).then(() => {
  window.domAutomationController.send(true);
}))",
                                                     url.spec().c_str()),
                                  &result));
  ASSERT_TRUE(result);
}

}  // namespace content
