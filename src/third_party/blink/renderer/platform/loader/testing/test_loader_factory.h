// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_TEST_LOADER_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_TEST_LOADER_FACTORY_H_

#include <memory>
#include <utility>
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

// ResourceFetcher::LoaderFactory implementation for tests.
class TestLoaderFactory : public ResourceFetcher::LoaderFactory {
 public:
  TestLoaderFactory()
      : url_loader_factory_(
            Platform::Current()->CreateDefaultURLLoaderFactory()) {}

  // LoaderFactory implementations
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest& request,
      const ResourceLoaderOptions& options,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    WrappedResourceRequest wrapped(request);
    return url_loader_factory_->CreateURLLoader(
        wrapped,
        scheduler::WebResourceLoadingTaskRunnerHandle::CreateUnprioritized(
            std::move(task_runner)));
  }
  std::unique_ptr<CodeCacheLoader> CreateCodeCacheLoader() override {
    return Platform::Current()->CreateCodeCacheLoader();
  }

 private:
  std::unique_ptr<WebURLLoaderFactory> url_loader_factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_TEST_LOADER_FACTORY_H_
