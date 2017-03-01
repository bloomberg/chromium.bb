// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/testing/MockResource.h"

#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"

namespace blink {

namespace {

class MockResourceFactory final : public ResourceFactory {
 public:
  MockResourceFactory() : ResourceFactory(Resource::Mock) {}

  Resource* create(const ResourceRequest& request,
                   const ResourceLoaderOptions& options,
                   const String&) const override {
    return new MockResource(request, options);
  }
};

}  // namespace

// static
MockResource* MockResource::fetch(FetchRequest& request,
                                  ResourceFetcher* fetcher) {
  request.mutableResourceRequest().setRequestContext(
      WebURLRequest::RequestContextSubresource);
  Resource* resource = fetcher->requestResource(request, MockResourceFactory());
  return static_cast<MockResource*>(resource);
}

// static
MockResource* MockResource::create(const ResourceRequest& request) {
  return new MockResource(request, ResourceLoaderOptions());
}

MockResource::MockResource(const ResourceRequest& request,
                           const ResourceLoaderOptions& options)
    : Resource(request, Resource::Mock, options) {}

}  // namespace blink
