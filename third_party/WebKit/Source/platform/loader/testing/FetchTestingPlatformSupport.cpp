// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/testing/FetchTestingPlatformSupport.h"

#include <memory>
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/testing/MockFetchContext.h"
#include "platform/testing/weburl_loader_mock_factory_impl.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLLoaderMockFactory.h"

namespace blink {

FetchTestingPlatformSupport::FetchTestingPlatformSupport()
    : url_loader_mock_factory_(new WebURLLoaderMockFactoryImpl(this)) {}

FetchTestingPlatformSupport::~FetchTestingPlatformSupport() {
  // Shutdowns WebURLLoaderMockFactory gracefully, serving all pending requests
  // first, then flushing all registered URLs.
  url_loader_mock_factory_->ServeAsynchronousRequests();
  url_loader_mock_factory_->UnregisterAllURLsAndClearMemoryCache();
}

MockFetchContext* FetchTestingPlatformSupport::Context() {
  if (!context_) {
    context_ =
        MockFetchContext::Create(MockFetchContext::kShouldLoadNewResource);
  }
  return context_;
}

WebURLLoaderMockFactory*
FetchTestingPlatformSupport::GetURLLoaderMockFactory() {
  return url_loader_mock_factory_.get();
}

std::unique_ptr<WebURLLoader> FetchTestingPlatformSupport::CreateURLLoader(
    const blink::WebURLRequest& request,
    SingleThreadTaskRunnerRefPtr task_runner) {
  return url_loader_mock_factory_->CreateURLLoader(nullptr);
}

}  // namespace blink
