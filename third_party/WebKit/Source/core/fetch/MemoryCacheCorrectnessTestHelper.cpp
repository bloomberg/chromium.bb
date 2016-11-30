// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/MemoryCacheCorrectnessTestHelper.h"

#include "core/fetch/FetchContext.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/MockFetchContext.h"
#include "core/fetch/RawResource.h"
#include "core/fetch/Resource.h"
#include "platform/network/ResourceRequest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"

namespace blink {

// An URL for the original request.
const char MemoryCacheCorrectnessTestHelper::kResourceURL[] =
    "http://resource.com/";

// The origin time of our first request.
const char MemoryCacheCorrectnessTestHelper::kOriginalRequestDateAsString[] =
    "Thu, 25 May 1977 18:30:00 GMT";
const double MemoryCacheCorrectnessTestHelper::kOriginalRequestDateAsDouble =
    233433000.;

const char MemoryCacheCorrectnessTestHelper::kOneDayBeforeOriginalRequest[] =
    "Wed, 24 May 1977 18:30:00 GMT";
const char MemoryCacheCorrectnessTestHelper::kOneDayAfterOriginalRequest[] =
    "Fri, 26 May 1977 18:30:00 GMT";

double MemoryCacheCorrectnessTestHelper::s_timeElapsed;

Resource* MemoryCacheCorrectnessTestHelper::resourceFromResourceResponse(
    ResourceResponse response,
    Resource::Type type) {
  if (response.url().isNull())
    response.setURL(KURL(ParsedURLString, kResourceURL));
  Resource* resource = createResource(ResourceRequest(response.url()), type);
  resource->setResponse(response);
  resource->finish();
  // Because we didn't give any real data, an image will have set its status
  // to DecodeError. Override it so the resource is cacaheable for testing
  // purposes.
  if (type == Resource::Image)
    resource->setStatus(Resource::Cached);
  memoryCache()->add(resource);

  return resource;
}

Resource* MemoryCacheCorrectnessTestHelper::resourceFromResourceRequest(
    ResourceRequest request) {
  if (request.url().isNull())
    request.setURL(KURL(ParsedURLString, kResourceURL));
  Resource* resource = createResource(request, Resource::Raw);
  resource->setResponse(ResourceResponse(KURL(ParsedURLString, kResourceURL),
                                         "text/html", 0, nullAtom, String()));
  resource->finish();
  memoryCache()->add(resource);

  return resource;
}

Resource* MemoryCacheCorrectnessTestHelper::fetch() {
  ResourceRequest resourceRequest(KURL(ParsedURLString, kResourceURL));
  resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
  FetchRequest fetchRequest(resourceRequest, FetchInitiatorInfo());
  return RawResource::fetch(fetchRequest, fetcher());
}

// static
double MemoryCacheCorrectnessTestHelper::returnMockTime() {
  return kOriginalRequestDateAsDouble + s_timeElapsed;
}

void MemoryCacheCorrectnessTestHelper::SetUp() {
  // Save the global memory cache to restore it upon teardown.
  m_globalMemoryCache = replaceMemoryCacheForTesting(MemoryCache::create());

  m_fetcher = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldNotLoadNewResource));

  s_timeElapsed = 0.0;
  m_originalTimeFunction = setTimeFunctionsForTesting(returnMockTime);
}

void MemoryCacheCorrectnessTestHelper::TearDown() {
  memoryCache()->evictResources();

  // Yield the ownership of the global memory cache back.
  replaceMemoryCacheForTesting(m_globalMemoryCache.release());

  setTimeFunctionsForTesting(m_originalTimeFunction);
}

}  // namespace blink
