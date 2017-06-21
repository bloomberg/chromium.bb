// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/testing/MockResource.h"

#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"

namespace blink {

namespace {

class MockResourceFactory final : public ResourceFactory {
 public:
  MockResourceFactory() : ResourceFactory(Resource::kMock) {}

  Resource* Create(const ResourceRequest& request,
                   const ResourceLoaderOptions& options,
                   const String&) const override {
    return new MockResource(request, options);
  }
};

}  // namespace

// static
MockResource* MockResource::Fetch(FetchParameters& params,
                                  ResourceFetcher* fetcher) {
  params.SetRequestContext(WebURLRequest::kRequestContextSubresource);
  Resource* resource = fetcher->RequestResource(params, MockResourceFactory());
  return static_cast<MockResource*>(resource);
}

// static
MockResource* MockResource::Create(const ResourceRequest& request) {
  ResourceLoaderOptions options;
  return new MockResource(request, options);
}

MockResource::MockResource(const ResourceRequest& request,
                           const ResourceLoaderOptions& options)
    : Resource(request, Resource::kMock, options) {}

}  // namespace blink
