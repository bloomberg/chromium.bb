/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/loader/fetch/MemoryCache.h"

#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/testing/MockFetchContext.h"
#include "platform/loader/testing/MockResource.h"
#include "platform/network/ResourceRequest.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

// An URL for the original request.
constexpr char kResourceURL[] = "http://resource.com/";

// The origin time of our first request.
constexpr char kOriginalRequestDateAsString[] = "Thu, 25 May 1977 18:30:00 GMT";
constexpr char kOneDayBeforeOriginalRequest[] = "Wed, 24 May 1977 18:30:00 GMT";
constexpr char kOneDayAfterOriginalRequest[] = "Fri, 26 May 1977 18:30:00 GMT";

}  // namespace

class MemoryCacheCorrectnessTest : public ::testing::Test {
 protected:
  MockResource* resourceFromResourceResponse(ResourceResponse response) {
    if (response.url().isNull())
      response.setURL(KURL(ParsedURLString, kResourceURL));
    MockResource* resource =
        MockResource::create(ResourceRequest(response.url()));
    resource->setResponse(response);
    resource->finish();
    memoryCache()->add(resource);

    return resource;
  }
  MockResource* resourceFromResourceRequest(ResourceRequest request) {
    if (request.url().isNull())
      request.setURL(KURL(ParsedURLString, kResourceURL));
    MockResource* resource = MockResource::create(request);
    resource->setResponse(ResourceResponse(KURL(ParsedURLString, kResourceURL),
                                           "text/html", 0, nullAtom));
    resource->finish();
    memoryCache()->add(resource);

    return resource;
  }
  // TODO(toyoshim): Consider to use MockResource for all tests instead of
  // RawResource.
  RawResource* fetchRawResource() {
    ResourceRequest resourceRequest(KURL(ParsedURLString, kResourceURL));
    resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
    FetchRequest fetchRequest(resourceRequest, FetchInitiatorInfo());
    return RawResource::fetch(fetchRequest, fetcher());
  }
  MockResource* fetchMockResource() {
    FetchRequest fetchRequest(
        ResourceRequest(KURL(ParsedURLString, kResourceURL)),
        FetchInitiatorInfo());
    return MockResource::fetch(fetchRequest, fetcher());
  }
  ResourceFetcher* fetcher() const { return m_fetcher.get(); }
  void advanceClock(double seconds) {
    m_platform->advanceClockSeconds(seconds);
  }

 private:
  // Overrides ::testing::Test.
  void SetUp() override {
    // Save the global memory cache to restore it upon teardown.
    m_globalMemoryCache = replaceMemoryCacheForTesting(MemoryCache::create());

    m_fetcher = ResourceFetcher::create(
        MockFetchContext::create(MockFetchContext::kShouldNotLoadNewResource));
  }
  void TearDown() override {
    memoryCache()->evictResources();

    // Yield the ownership of the global memory cache back.
    replaceMemoryCacheForTesting(m_globalMemoryCache.release());
  }

  Persistent<MemoryCache> m_globalMemoryCache;
  Persistent<ResourceFetcher> m_fetcher;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      m_platform;
};

TEST_F(MemoryCacheCorrectnessTest, FreshFromLastModified) {
  ResourceResponse fresh200Response;
  fresh200Response.setHTTPStatusCode(200);
  fresh200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
  fresh200Response.setHTTPHeaderField("Last-Modified",
                                      kOneDayBeforeOriginalRequest);

  MockResource* fresh200 = resourceFromResourceResponse(fresh200Response);

  // Advance the clock within the implicit freshness period of this resource
  // before we make a request.
  advanceClock(600.);

  MockResource* fetched = fetchMockResource();
  EXPECT_EQ(fresh200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshFromExpires) {
  ResourceResponse fresh200Response;
  fresh200Response.setHTTPStatusCode(200);
  fresh200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
  fresh200Response.setHTTPHeaderField("Expires", kOneDayAfterOriginalRequest);

  MockResource* fresh200 = resourceFromResourceResponse(fresh200Response);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  advanceClock(24. * 60. * 60. - 15.);

  MockResource* fetched = fetchMockResource();
  EXPECT_EQ(fresh200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshFromMaxAge) {
  ResourceResponse fresh200Response;
  fresh200Response.setHTTPStatusCode(200);
  fresh200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
  fresh200Response.setHTTPHeaderField("Cache-Control", "max-age=600");

  MockResource* fresh200 = resourceFromResourceResponse(fresh200Response);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  advanceClock(500.);

  MockResource* fetched = fetchMockResource();
  EXPECT_EQ(fresh200, fetched);
}

// The strong validator causes a revalidation to be launched, and the proxy and
// original resources leak because of their reference loop.
TEST_F(MemoryCacheCorrectnessTest, DISABLED_ExpiredFromLastModified) {
  ResourceResponse expired200Response;
  expired200Response.setHTTPStatusCode(200);
  expired200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
  expired200Response.setHTTPHeaderField("Last-Modified",
                                        kOneDayBeforeOriginalRequest);

  MockResource* expired200 = resourceFromResourceResponse(expired200Response);

  // Advance the clock beyond the implicit freshness period.
  advanceClock(24. * 60. * 60. * 0.2);

  MockResource* fetched = fetchMockResource();
  EXPECT_NE(expired200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, ExpiredFromExpires) {
  ResourceResponse expired200Response;
  expired200Response.setHTTPStatusCode(200);
  expired200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
  expired200Response.setHTTPHeaderField("Expires", kOneDayAfterOriginalRequest);

  MockResource* expired200 = resourceFromResourceResponse(expired200Response);

  // Advance the clock within the expiredness period of this resource before we
  // make a request.
  advanceClock(24. * 60. * 60. + 15.);

  MockResource* fetched = fetchMockResource();
  EXPECT_NE(expired200, fetched);
}

// If the resource hasn't been loaded in this "document" before, then it
// shouldn't have list of available resources logic.
TEST_F(MemoryCacheCorrectnessTest, NewMockResourceExpiredFromExpires) {
  ResourceResponse expired200Response;
  expired200Response.setHTTPStatusCode(200);
  expired200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
  expired200Response.setHTTPHeaderField("Expires", kOneDayAfterOriginalRequest);

  MockResource* expired200 = resourceFromResourceResponse(expired200Response);

  // Advance the clock within the expiredness period of this resource before we
  // make a request.
  advanceClock(24. * 60. * 60. + 15.);

  MockResource* fetched = fetchMockResource();
  EXPECT_NE(expired200, fetched);
}

// If the resource has been loaded in this "document" before, then it should
// have list of available resources logic, and so normal cache testing should be
// bypassed.
TEST_F(MemoryCacheCorrectnessTest, ReuseMockResourceExpiredFromExpires) {
  ResourceResponse expired200Response;
  expired200Response.setHTTPStatusCode(200);
  expired200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
  expired200Response.setHTTPHeaderField("Expires", kOneDayAfterOriginalRequest);

  MockResource* expired200 = resourceFromResourceResponse(expired200Response);

  // Advance the clock within the freshness period, and make a request to add
  // this resource to the document resources.
  advanceClock(15.);
  MockResource* firstFetched = fetchMockResource();
  EXPECT_EQ(expired200, firstFetched);

  // Advance the clock within the expiredness period of this resource before we
  // make a request.
  advanceClock(24. * 60. * 60. + 15.);

  MockResource* fetched = fetchMockResource();
  EXPECT_EQ(expired200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, ExpiredFromMaxAge) {
  ResourceResponse expired200Response;
  expired200Response.setHTTPStatusCode(200);
  expired200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
  expired200Response.setHTTPHeaderField("Cache-Control", "max-age=600");

  MockResource* expired200 = resourceFromResourceResponse(expired200Response);

  // Advance the clock within the expiredness period of this resource before we
  // make a request.
  advanceClock(700.);

  MockResource* fetched = fetchMockResource();
  EXPECT_NE(expired200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshButNoCache) {
  ResourceResponse fresh200NocacheResponse;
  fresh200NocacheResponse.setHTTPStatusCode(200);
  fresh200NocacheResponse.setHTTPHeaderField(HTTPNames::Date,
                                             kOriginalRequestDateAsString);
  fresh200NocacheResponse.setHTTPHeaderField(HTTPNames::Expires,
                                             kOneDayAfterOriginalRequest);
  fresh200NocacheResponse.setHTTPHeaderField(HTTPNames::Cache_Control,
                                             "no-cache");

  MockResource* fresh200Nocache =
      resourceFromResourceResponse(fresh200NocacheResponse);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  advanceClock(24. * 60. * 60. - 15.);

  MockResource* fetched = fetchMockResource();
  EXPECT_NE(fresh200Nocache, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, RequestWithNoCache) {
  ResourceRequest noCacheRequest;
  noCacheRequest.setHTTPHeaderField(HTTPNames::Cache_Control, "no-cache");
  MockResource* noCacheResource = resourceFromResourceRequest(noCacheRequest);
  MockResource* fetched = fetchMockResource();
  EXPECT_NE(noCacheResource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshButNoStore) {
  ResourceResponse fresh200NostoreResponse;
  fresh200NostoreResponse.setHTTPStatusCode(200);
  fresh200NostoreResponse.setHTTPHeaderField(HTTPNames::Date,
                                             kOriginalRequestDateAsString);
  fresh200NostoreResponse.setHTTPHeaderField(HTTPNames::Expires,
                                             kOneDayAfterOriginalRequest);
  fresh200NostoreResponse.setHTTPHeaderField(HTTPNames::Cache_Control,
                                             "no-store");

  MockResource* fresh200Nostore =
      resourceFromResourceResponse(fresh200NostoreResponse);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  advanceClock(24. * 60. * 60. - 15.);

  MockResource* fetched = fetchMockResource();
  EXPECT_NE(fresh200Nostore, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, RequestWithNoStore) {
  ResourceRequest noStoreRequest;
  noStoreRequest.setHTTPHeaderField(HTTPNames::Cache_Control, "no-store");
  MockResource* noStoreResource = resourceFromResourceRequest(noStoreRequest);
  MockResource* fetched = fetchMockResource();
  EXPECT_NE(noStoreResource, fetched);
}

// FIXME: Determine if ignoring must-revalidate for blink is correct behaviour.
// See crbug.com/340088 .
TEST_F(MemoryCacheCorrectnessTest, DISABLED_FreshButMustRevalidate) {
  ResourceResponse fresh200MustRevalidateResponse;
  fresh200MustRevalidateResponse.setHTTPStatusCode(200);
  fresh200MustRevalidateResponse.setHTTPHeaderField(
      HTTPNames::Date, kOriginalRequestDateAsString);
  fresh200MustRevalidateResponse.setHTTPHeaderField(
      HTTPNames::Expires, kOneDayAfterOriginalRequest);
  fresh200MustRevalidateResponse.setHTTPHeaderField(HTTPNames::Cache_Control,
                                                    "must-revalidate");

  MockResource* fresh200MustRevalidate =
      resourceFromResourceResponse(fresh200MustRevalidateResponse);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  advanceClock(24. * 60. * 60. - 15.);

  MockResource* fetched = fetchMockResource();
  EXPECT_NE(fresh200MustRevalidate, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshWithFreshRedirect) {
  KURL redirectUrl(ParsedURLString, kResourceURL);
  const char redirectTargetUrlString[] = "http://redirect-target.com";
  KURL redirectTargetUrl(ParsedURLString, redirectTargetUrlString);

  MockResource* firstResource =
      MockResource::create(ResourceRequest(redirectUrl));

  ResourceResponse fresh301Response;
  fresh301Response.setURL(redirectUrl);
  fresh301Response.setHTTPStatusCode(301);
  fresh301Response.setHTTPHeaderField(HTTPNames::Date,
                                      kOriginalRequestDateAsString);
  fresh301Response.setHTTPHeaderField(HTTPNames::Location,
                                      redirectTargetUrlString);
  fresh301Response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=600");

  // Add the redirect to our request.
  ResourceRequest redirectRequest = ResourceRequest(redirectTargetUrl);
  firstResource->willFollowRedirect(redirectRequest, fresh301Response);

  // Add the final response to our request.
  ResourceResponse fresh200Response;
  fresh200Response.setURL(redirectTargetUrl);
  fresh200Response.setHTTPStatusCode(200);
  fresh200Response.setHTTPHeaderField(HTTPNames::Date,
                                      kOriginalRequestDateAsString);
  fresh200Response.setHTTPHeaderField(HTTPNames::Expires,
                                      kOneDayAfterOriginalRequest);

  firstResource->setResponse(fresh200Response);
  firstResource->finish();
  memoryCache()->add(firstResource);

  advanceClock(500.);

  MockResource* fetched = fetchMockResource();
  EXPECT_EQ(firstResource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshWithStaleRedirect) {
  KURL redirectUrl(ParsedURLString, kResourceURL);
  const char redirectTargetUrlString[] = "http://redirect-target.com";
  KURL redirectTargetUrl(ParsedURLString, redirectTargetUrlString);

  MockResource* firstResource =
      MockResource::create(ResourceRequest(redirectUrl));

  ResourceResponse stale301Response;
  stale301Response.setURL(redirectUrl);
  stale301Response.setHTTPStatusCode(301);
  stale301Response.setHTTPHeaderField(HTTPNames::Date,
                                      kOriginalRequestDateAsString);
  stale301Response.setHTTPHeaderField(HTTPNames::Location,
                                      redirectTargetUrlString);

  // Add the redirect to our request.
  ResourceRequest redirectRequest = ResourceRequest(redirectTargetUrl);
  firstResource->willFollowRedirect(redirectRequest, stale301Response);

  // Add the final response to our request.
  ResourceResponse fresh200Response;
  fresh200Response.setURL(redirectTargetUrl);
  fresh200Response.setHTTPStatusCode(200);
  fresh200Response.setHTTPHeaderField(HTTPNames::Date,
                                      kOriginalRequestDateAsString);
  fresh200Response.setHTTPHeaderField(HTTPNames::Expires,
                                      kOneDayAfterOriginalRequest);

  firstResource->setResponse(fresh200Response);
  firstResource->finish();
  memoryCache()->add(firstResource);

  advanceClock(500.);

  MockResource* fetched = fetchMockResource();
  EXPECT_NE(firstResource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, PostToSameURLTwice) {
  ResourceRequest request1(KURL(ParsedURLString, kResourceURL));
  request1.setHTTPMethod(HTTPNames::POST);
  RawResource* resource1 =
      RawResource::create(ResourceRequest(request1.url()), Resource::Raw);
  resource1->setStatus(ResourceStatus::Pending);
  memoryCache()->add(resource1);

  ResourceRequest request2(KURL(ParsedURLString, kResourceURL));
  request2.setHTTPMethod(HTTPNames::POST);
  FetchRequest fetch2(request2, FetchInitiatorInfo());
  RawResource* resource2 = RawResource::fetchSynchronously(fetch2, fetcher());

  EXPECT_EQ(resource2, memoryCache()->resourceForURL(request2.url()));
  EXPECT_NE(resource1, resource2);
}

TEST_F(MemoryCacheCorrectnessTest, 302RedirectNotImplicitlyFresh) {
  KURL redirectUrl(ParsedURLString, kResourceURL);
  const char redirectTargetUrlString[] = "http://redirect-target.com";
  KURL redirectTargetUrl(ParsedURLString, redirectTargetUrlString);

  RawResource* firstResource =
      RawResource::create(ResourceRequest(redirectUrl), Resource::Raw);

  ResourceResponse fresh302Response;
  fresh302Response.setURL(redirectUrl);
  fresh302Response.setHTTPStatusCode(302);
  fresh302Response.setHTTPHeaderField(HTTPNames::Date,
                                      kOriginalRequestDateAsString);
  fresh302Response.setHTTPHeaderField(HTTPNames::Last_Modified,
                                      kOneDayBeforeOriginalRequest);
  fresh302Response.setHTTPHeaderField(HTTPNames::Location,
                                      redirectTargetUrlString);

  // Add the redirect to our request.
  ResourceRequest redirectRequest = ResourceRequest(redirectTargetUrl);
  firstResource->willFollowRedirect(redirectRequest, fresh302Response);

  // Add the final response to our request.
  ResourceResponse fresh200Response;
  fresh200Response.setURL(redirectTargetUrl);
  fresh200Response.setHTTPStatusCode(200);
  fresh200Response.setHTTPHeaderField(HTTPNames::Date,
                                      kOriginalRequestDateAsString);
  fresh200Response.setHTTPHeaderField(HTTPNames::Expires,
                                      kOneDayAfterOriginalRequest);

  firstResource->setResponse(fresh200Response);
  firstResource->finish();
  memoryCache()->add(firstResource);

  advanceClock(500.);

  RawResource* fetched = fetchRawResource();
  EXPECT_NE(firstResource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, 302RedirectExplicitlyFreshMaxAge) {
  KURL redirectUrl(ParsedURLString, kResourceURL);
  const char redirectTargetUrlString[] = "http://redirect-target.com";
  KURL redirectTargetUrl(ParsedURLString, redirectTargetUrlString);

  MockResource* firstResource =
      MockResource::create(ResourceRequest(redirectUrl));

  ResourceResponse fresh302Response;
  fresh302Response.setURL(redirectUrl);
  fresh302Response.setHTTPStatusCode(302);
  fresh302Response.setHTTPHeaderField(HTTPNames::Date,
                                      kOriginalRequestDateAsString);
  fresh302Response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=600");
  fresh302Response.setHTTPHeaderField(HTTPNames::Location,
                                      redirectTargetUrlString);

  // Add the redirect to our request.
  ResourceRequest redirectRequest = ResourceRequest(redirectTargetUrl);
  firstResource->willFollowRedirect(redirectRequest, fresh302Response);

  // Add the final response to our request.
  ResourceResponse fresh200Response;
  fresh200Response.setURL(redirectTargetUrl);
  fresh200Response.setHTTPStatusCode(200);
  fresh200Response.setHTTPHeaderField(HTTPNames::Date,
                                      kOriginalRequestDateAsString);
  fresh200Response.setHTTPHeaderField(HTTPNames::Expires,
                                      kOneDayAfterOriginalRequest);

  firstResource->setResponse(fresh200Response);
  firstResource->finish();
  memoryCache()->add(firstResource);

  advanceClock(500.);

  MockResource* fetched = fetchMockResource();
  EXPECT_EQ(firstResource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, 302RedirectExplicitlyFreshExpires) {
  KURL redirectUrl(ParsedURLString, kResourceURL);
  const char redirectTargetUrlString[] = "http://redirect-target.com";
  KURL redirectTargetUrl(ParsedURLString, redirectTargetUrlString);

  MockResource* firstResource =
      MockResource::create(ResourceRequest(redirectUrl));

  ResourceResponse fresh302Response;
  fresh302Response.setURL(redirectUrl);
  fresh302Response.setHTTPStatusCode(302);
  fresh302Response.setHTTPHeaderField(HTTPNames::Date,
                                      kOriginalRequestDateAsString);
  fresh302Response.setHTTPHeaderField(HTTPNames::Expires,
                                      kOneDayAfterOriginalRequest);
  fresh302Response.setHTTPHeaderField(HTTPNames::Location,
                                      redirectTargetUrlString);

  // Add the redirect to our request.
  ResourceRequest redirectRequest = ResourceRequest(redirectTargetUrl);
  firstResource->willFollowRedirect(redirectRequest, fresh302Response);

  // Add the final response to our request.
  ResourceResponse fresh200Response;
  fresh200Response.setURL(redirectTargetUrl);
  fresh200Response.setHTTPStatusCode(200);
  fresh200Response.setHTTPHeaderField(HTTPNames::Date,
                                      kOriginalRequestDateAsString);
  fresh200Response.setHTTPHeaderField(HTTPNames::Expires,
                                      kOneDayAfterOriginalRequest);

  firstResource->setResponse(fresh200Response);
  firstResource->finish();
  memoryCache()->add(firstResource);

  advanceClock(500.);

  MockResource* fetched = fetchMockResource();
  EXPECT_EQ(firstResource, fetched);
}

}  // namespace blink
