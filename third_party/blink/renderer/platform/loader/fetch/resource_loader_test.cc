// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"

#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class ResourceLoaderTest : public testing::Test {
  DISALLOW_COPY_AND_ASSIGN(ResourceLoaderTest);

 public:
  enum class From {
    kServiceWorker,
    kNetwork,
  };

  ResourceLoaderTest()
      : foo_url_("https://foo.test"), bar_url_("https://bar.test"){};

 protected:
  using FetchRequestMode = network::mojom::FetchRequestMode;
  using FetchResponseType = network::mojom::FetchResponseType;

  struct TestCase {
    const KURL url;
    const FetchRequestMode request_mode;
    const From from;
    const scoped_refptr<const SecurityOrigin> allowed_origin;
    const FetchResponseType original_response_type;
    const FetchResponseType expectation;
  };

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

  const KURL foo_url_;
  const KURL bar_url_;

  class TestWebURLLoaderFactory final : public WebURLLoaderFactory {
    std::unique_ptr<WebURLLoader> CreateURLLoader(
        const WebURLRequest& request,
        std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>)
        override {
      return std::make_unique<TestWebURLLoader>();
    }
  };

  static scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }

 private:
  class TestWebURLLoader final : public WebURLLoader {
   public:
    ~TestWebURLLoader() override = default;
    void LoadSynchronously(const WebURLRequest&,
                           WebURLLoaderClient*,
                           WebURLResponse&,
                           base::Optional<WebURLError>&,
                           WebData&,
                           int64_t& encoded_data_length,
                           int64_t& encoded_body_length,
                           WebBlobInfo& downloaded_blob) override {
      NOTREACHED();
    }
    void LoadAsynchronously(const WebURLRequest&,
                            WebURLLoaderClient*) override {}

    void Cancel() override {}
    void SetDefersLoading(bool) override {}
    void DidChangePriority(WebURLRequest::Priority, int) override {
      NOTREACHED();
    }
  };
};

std::ostream& operator<<(std::ostream& o, const ResourceLoaderTest::From& f) {
  switch (f) {
    case ResourceLoaderTest::From::kServiceWorker:
      o << "service worker";
      break;
    case ResourceLoaderTest::From::kNetwork:
      o << "network";
      break;
  }
  return o;
}

TEST_F(ResourceLoaderTest, ResponseType) {
  // This test will be trivial if EnableOutOfBlinkCors is enabled.
  WebRuntimeFeatures::EnableOutOfBlinkCors(false);

  const scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::Create(foo_url_);
  const scoped_refptr<const SecurityOrigin> no_origin = nullptr;
  const KURL same_origin_url = foo_url_;
  const KURL cross_origin_url = bar_url_;

  TestCase cases[] = {
      // Same origin response:
      {same_origin_url, FetchRequestMode::kNoCors, From::kNetwork, no_origin,
       FetchResponseType::kDefault, FetchResponseType::kBasic},
      {same_origin_url, FetchRequestMode::kCors, From::kNetwork, no_origin,
       FetchResponseType::kDefault, FetchResponseType::kBasic},

      // Cross origin, no-cors:
      {cross_origin_url, FetchRequestMode::kNoCors, From::kNetwork, no_origin,
       FetchResponseType::kDefault, FetchResponseType::kOpaque},

      // Cross origin, cors:
      {cross_origin_url, FetchRequestMode::kCors, From::kNetwork, origin,
       FetchResponseType::kDefault, FetchResponseType::kCors},
      {cross_origin_url, FetchRequestMode::kCors, From::kNetwork, no_origin,
       FetchResponseType::kDefault, FetchResponseType::kError},

      // From service worker, no-cors:
      {same_origin_url, FetchRequestMode::kNoCors, From::kServiceWorker,
       no_origin, FetchResponseType::kBasic, FetchResponseType::kBasic},
      {same_origin_url, FetchRequestMode::kNoCors, From::kServiceWorker,
       no_origin, FetchResponseType::kCors, FetchResponseType::kCors},
      {same_origin_url, FetchRequestMode::kNoCors, From::kServiceWorker,
       no_origin, FetchResponseType::kDefault, FetchResponseType::kDefault},
      {same_origin_url, FetchRequestMode::kNoCors, From::kServiceWorker,
       no_origin, FetchResponseType::kOpaque, FetchResponseType::kOpaque},
      {same_origin_url, FetchRequestMode::kNoCors, From::kServiceWorker,
       no_origin, FetchResponseType::kOpaqueRedirect,
       FetchResponseType::kOpaqueRedirect},

      // From service worker, cors:
      {same_origin_url, FetchRequestMode::kCors, From::kServiceWorker,
       no_origin, FetchResponseType::kBasic, FetchResponseType::kBasic},
      {same_origin_url, FetchRequestMode::kNoCors, From::kServiceWorker,
       no_origin, FetchResponseType::kCors, FetchResponseType::kCors},
      {same_origin_url, FetchRequestMode::kNoCors, From::kServiceWorker,
       no_origin, FetchResponseType::kDefault, FetchResponseType::kDefault},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "url: " << test.url.GetString()
                 << ", requets mode: " << test.request_mode
                 << ", from: " << test.from << ", allowed_origin: "
                 << (test.allowed_origin ? test.allowed_origin->ToString()
                                         : String("<no allowed origin>"))
                 << ", original_response_type: "
                 << test.original_response_type);

    auto* properties =
        MakeGarbageCollected<TestResourceFetcherProperties>(origin);
    FetchContext* context = MakeGarbageCollected<MockFetchContext>(
        nullptr, std::make_unique<TestWebURLLoaderFactory>());
    auto* fetcher = MakeGarbageCollected<ResourceFetcher>(
        ResourceFetcherInit(*properties, context, CreateTaskRunner()));

    ResourceRequest request;
    request.SetURL(test.url);
    request.SetFetchRequestMode(test.request_mode);
    request.SetRequestContext(mojom::RequestContextType::FETCH);

    FetchParameters fetch_parameters(request);
    if (test.request_mode == network::mojom::FetchRequestMode::kCors) {
      fetch_parameters.SetCrossOriginAccessControl(
          origin.get(), network::mojom::FetchCredentialsMode::kOmit);
    }
    Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
    ResourceLoader* loader = resource->Loader();

    ResourceResponse response(test.url);
    response.SetHTTPStatusCode(200);
    response.SetType(test.original_response_type);
    response.SetWasFetchedViaServiceWorker(test.from == From::kServiceWorker);
    if (test.allowed_origin) {
      response.SetHTTPHeaderField("access-control-allow-origin",
                                  test.allowed_origin->ToAtomicString());
    }
    response.SetType(test.original_response_type);

    loader->DidReceiveResponse(WrappedResourceResponse(response));
    EXPECT_EQ(test.expectation, resource->GetResponse().GetType());
  }
}

class ResourceLoaderIsolatedCodeCacheTest : public ResourceLoaderTest {
 protected:
  bool LoadAndCheckIsolatedCodeCache(ResourceResponse response) {
    const scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::Create(foo_url_);

    auto* properties =
        MakeGarbageCollected<TestResourceFetcherProperties>(origin);
    FetchContext* context = MakeGarbageCollected<MockFetchContext>(
        nullptr, std::make_unique<TestWebURLLoaderFactory>());
    auto* fetcher = MakeGarbageCollected<ResourceFetcher>(
        ResourceFetcherInit(*properties, context, CreateTaskRunner()));

    ResourceRequest request;
    request.SetURL(foo_url_);
    request.SetRequestContext(mojom::RequestContextType::FETCH);

    FetchParameters fetch_parameters(request);
    Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
    ResourceLoader* loader = resource->Loader();

    loader->DidReceiveResponse(WrappedResourceResponse(response));
    return loader->should_use_isolated_code_cache_;
  }
};

TEST_F(ResourceLoaderIsolatedCodeCacheTest, ResponseFromNetwork) {
  ResourceResponse response(foo_url_);
  response.SetHTTPStatusCode(200);
  EXPECT_EQ(true, LoadAndCheckIsolatedCodeCache(response));
}

TEST_F(ResourceLoaderIsolatedCodeCacheTest,
       SyntheticResponseFromServiceWorker) {
  ResourceResponse response(foo_url_);
  response.SetHTTPStatusCode(200);
  response.SetWasFetchedViaServiceWorker(true);
  EXPECT_EQ(false, LoadAndCheckIsolatedCodeCache(response));
}

TEST_F(ResourceLoaderIsolatedCodeCacheTest,
       PassThroughResponseFromServiceWorker) {
  ResourceResponse response(foo_url_);
  response.SetHTTPStatusCode(200);
  response.SetWasFetchedViaServiceWorker(true);
  response.SetURLListViaServiceWorker(Vector<KURL>(1, foo_url_));
  EXPECT_EQ(true, LoadAndCheckIsolatedCodeCache(response));
}

TEST_F(ResourceLoaderIsolatedCodeCacheTest,
       DifferentUrlResponseFromServiceWorker) {
  ResourceResponse response(foo_url_);
  response.SetHTTPStatusCode(200);
  response.SetWasFetchedViaServiceWorker(true);
  response.SetURLListViaServiceWorker(Vector<KURL>(1, bar_url_));
  EXPECT_EQ(false, LoadAndCheckIsolatedCodeCache(response));
}

TEST_F(ResourceLoaderIsolatedCodeCacheTest, CacheResponseFromServiceWorker) {
  ResourceResponse response(foo_url_);
  response.SetHTTPStatusCode(200);
  response.SetWasFetchedViaServiceWorker(true);
  response.SetCacheStorageCacheName("dummy");
  // The browser does support code cache for cache_storage Responses, but they
  // are loaded via a different mechanism.  So the ResourceLoader code caching
  // value should be false here.
  EXPECT_EQ(false, LoadAndCheckIsolatedCodeCache(response));
}

}  // namespace blink
