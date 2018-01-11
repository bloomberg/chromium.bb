// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/LinkLoader.h"

#include <base/macros.h>
#include <memory>
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/frame/Settings.h"
#include "core/html/LinkRelAttribute.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/LinkLoaderClient.h"
#include "core/loader/NetworkHintsInterface.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/testing/DummyModulator.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoadPriority.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class MockLinkLoaderClient final
    : public GarbageCollectedFinalized<MockLinkLoaderClient>,
      public LinkLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(MockLinkLoaderClient);

 public:
  static MockLinkLoaderClient* Create(bool should_load) {
    return new MockLinkLoaderClient(should_load);
  }

  void Trace(blink::Visitor* visitor) override {
    LinkLoaderClient::Trace(visitor);
  }

  bool ShouldLoadLink() override { return should_load_; }

  void LinkLoaded() override {}
  void LinkLoadingErrored() override {}
  void DidStartLinkPrerender() override {}
  void DidStopLinkPrerender() override {}
  void DidSendLoadForLinkPrerender() override {}
  void DidSendDOMContentLoadedForLinkPrerender() override {}

  scoped_refptr<WebTaskRunner> GetLoadingTaskRunner() override {
    return Platform::Current()->CurrentThread()->GetWebTaskRunner();
  }

 private:
  explicit MockLinkLoaderClient(bool should_load) : should_load_(should_load) {}

  const bool should_load_;
};

class NetworkHintsMock : public NetworkHintsInterface {
 public:
  NetworkHintsMock() = default;

  void DnsPrefetchHost(const String& host) const override {
    did_dns_prefetch_ = true;
  }

  void PreconnectHost(
      const KURL& host,
      const CrossOriginAttributeValue cross_origin) const override {
    did_preconnect_ = true;
    is_https_ = host.ProtocolIs("https");
    is_cross_origin_ = (cross_origin == kCrossOriginAttributeAnonymous);
  }

  bool DidDnsPrefetch() { return did_dns_prefetch_; }
  bool DidPreconnect() { return did_preconnect_; }
  bool IsHTTPS() { return is_https_; }
  bool IsCrossOrigin() { return is_cross_origin_; }

 private:
  mutable bool did_dns_prefetch_ = false;
  mutable bool did_preconnect_ = false;
  mutable bool is_https_ = false;
  mutable bool is_cross_origin_ = false;
};

struct PreloadTestParams {
  const char* href;
  const char* as;
  const char* type;
  const char* media;
  const char* nonce;
  const ReferrerPolicy referrer_policy;
  const ResourceLoadPriority priority;
  const WebURLRequest::RequestContext context;
  const char* content_security_policy;
  const bool link_loader_should_load_value;
  const bool expecting_load;
  const ReferrerPolicy expected_referrer_policy;
};

class LinkLoaderPreloadTest
    : public ::testing::TestWithParam<PreloadTestParams> {
 public:
  ~LinkLoaderPreloadTest() {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }
};

TEST_P(LinkLoaderPreloadTest, Preload) {
  const auto& test_case = GetParam();
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(500, 500));
  ResourceFetcher* fetcher = dummy_page_holder->GetDocument().Fetcher();
  if (test_case.content_security_policy) {
    dummy_page_holder->GetDocument()
        .GetContentSecurityPolicy()
        ->DidReceiveHeader(test_case.content_security_policy,
                           kContentSecurityPolicyHeaderTypeEnforce,
                           kContentSecurityPolicyHeaderSourceHTTP);
  }
  ASSERT_TRUE(fetcher);
  dummy_page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
  Persistent<MockLinkLoaderClient> loader_client =
      MockLinkLoaderClient::Create(test_case.link_loader_should_load_value);
  LinkLoader* loader = LinkLoader::Create(loader_client.Get());
  KURL href_url = KURL(NullURL(), test_case.href);
  URLTestHelpers::RegisterMockedErrorURLLoad(href_url);
  loader->LoadLink(LinkRelAttribute("preload"), kCrossOriginAttributeNotSet,
                   test_case.type, test_case.as, test_case.media,
                   test_case.nonce, String() /* integrity */,
                   test_case.referrer_policy, href_url,
                   dummy_page_holder->GetDocument(), NetworkHintsMock());
  if (test_case.expecting_load &&
      test_case.priority != ResourceLoadPriority::kUnresolved) {
    ASSERT_EQ(1, fetcher->CountPreloads());
    Resource* resource = loader->GetResourceForTesting();
    ASSERT_NE(resource, nullptr);
    EXPECT_TRUE(fetcher->ContainsAsPreload(resource));
    EXPECT_EQ(test_case.priority, resource->GetResourceRequest().Priority());
    EXPECT_EQ(test_case.context,
              resource->GetResourceRequest().GetRequestContext());
    if (test_case.expected_referrer_policy != kReferrerPolicyDefault) {
      EXPECT_EQ(test_case.expected_referrer_policy,
                resource->GetResourceRequest().GetReferrerPolicy());
    }
  } else {
    ASSERT_EQ(0, fetcher->CountPreloads());
  }
}

constexpr PreloadTestParams kPreloadTestParams[] = {
    {"http://example.test/cat.jpg", "image", "", "", "", kReferrerPolicyDefault,
     ResourceLoadPriority::kLow, WebURLRequest::kRequestContextImage, nullptr,
     true, true, kReferrerPolicyDefault},
    {"http://example.test/cat.js", "script", "", "", "", kReferrerPolicyDefault,
     ResourceLoadPriority::kHigh, WebURLRequest::kRequestContextScript, nullptr,
     true, true, kReferrerPolicyDefault},
    {"http://example.test/cat.css", "style", "", "", "", kReferrerPolicyDefault,
     ResourceLoadPriority::kVeryHigh, WebURLRequest::kRequestContextStyle,
     nullptr, true, true, kReferrerPolicyDefault},
    // TODO(yoav): It doesn't seem like the audio context is ever used. That
    // should probably be fixed (or we can consolidate audio and video).
    {"http://example.test/cat.wav", "audio", "", "", "", kReferrerPolicyDefault,
     ResourceLoadPriority::kLow, WebURLRequest::kRequestContextAudio, nullptr,
     true, true, kReferrerPolicyDefault},
    {"http://example.test/cat.mp4", "video", "", "", "", kReferrerPolicyDefault,
     ResourceLoadPriority::kLow, WebURLRequest::kRequestContextVideo, nullptr,
     true, true, kReferrerPolicyDefault},
    {"http://example.test/cat.vtt", "track", "", "", "", kReferrerPolicyDefault,
     ResourceLoadPriority::kLow, WebURLRequest::kRequestContextTrack, nullptr,
     true, true, kReferrerPolicyDefault},
    {"http://example.test/cat.woff", "font", "", "", "", kReferrerPolicyDefault,
     ResourceLoadPriority::kHigh, WebURLRequest::kRequestContextFont, nullptr,
     true, true, kReferrerPolicyDefault},
    // TODO(yoav): subresource should be *very* low priority (rather than
    // low).
    {"http://example.test/cat.empty", "fetch", "", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kHigh,
     WebURLRequest::kRequestContextSubresource, nullptr, true, true,
     kReferrerPolicyDefault},
    {"http://example.test/cat.blob", "blabla", "", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextSubresource, nullptr, false, false,
     kReferrerPolicyDefault},
    {"http://example.test/cat.blob", "", "", "", "", kReferrerPolicyDefault,
     ResourceLoadPriority::kLow, WebURLRequest::kRequestContextSubresource,
     nullptr, false, false, kReferrerPolicyDefault},
    {"bla://example.test/cat.gif", "image", "", "", "", kReferrerPolicyDefault,
     ResourceLoadPriority::kUnresolved, WebURLRequest::kRequestContextImage,
     nullptr, false, false, kReferrerPolicyDefault},
    // MIME type tests
    {"http://example.test/cat.webp", "image", "image/webp", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextImage, nullptr, true, true,
     kReferrerPolicyDefault},
    {"http://example.test/cat.svg", "image", "image/svg+xml", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextImage, nullptr, true, true,
     kReferrerPolicyDefault},
    {"http://example.test/cat.jxr", "image", "image/jxr", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kUnresolved,
     WebURLRequest::kRequestContextImage, nullptr, false, false,
     kReferrerPolicyDefault},
    {"http://example.test/cat.js", "script", "text/javascript", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kHigh,
     WebURLRequest::kRequestContextScript, nullptr, true, true,
     kReferrerPolicyDefault},
    {"http://example.test/cat.js", "script", "text/coffeescript", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kUnresolved,
     WebURLRequest::kRequestContextScript, nullptr, false, false,
     kReferrerPolicyDefault},
    {"http://example.test/cat.css", "style", "text/css", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kVeryHigh,
     WebURLRequest::kRequestContextStyle, nullptr, true, true,
     kReferrerPolicyDefault},
    {"http://example.test/cat.css", "style", "text/sass", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kUnresolved,
     WebURLRequest::kRequestContextStyle, nullptr, false, false,
     kReferrerPolicyDefault},
    {"http://example.test/cat.wav", "audio", "audio/wav", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextAudio, nullptr, true, true,
     kReferrerPolicyDefault},
    {"http://example.test/cat.wav", "audio", "audio/mp57", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kUnresolved,
     WebURLRequest::kRequestContextAudio, nullptr, false, false,
     kReferrerPolicyDefault},
    {"http://example.test/cat.webm", "video", "video/webm", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextVideo, nullptr, true, true,
     kReferrerPolicyDefault},
    {"http://example.test/cat.mp199", "video", "video/mp199", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kUnresolved,
     WebURLRequest::kRequestContextVideo, nullptr, false, false,
     kReferrerPolicyDefault},
    {"http://example.test/cat.vtt", "track", "text/vtt", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextTrack, nullptr, true, true,
     kReferrerPolicyDefault},
    {"http://example.test/cat.vtt", "track", "text/subtitlething", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kUnresolved,
     WebURLRequest::kRequestContextTrack, nullptr, false, false,
     kReferrerPolicyDefault},
    {"http://example.test/cat.woff", "font", "font/woff2", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kHigh,
     WebURLRequest::kRequestContextFont, nullptr, true, true,
     kReferrerPolicyDefault},
    {"http://example.test/cat.woff", "font", "font/woff84", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kUnresolved,
     WebURLRequest::kRequestContextFont, nullptr, false, false,
     kReferrerPolicyDefault},
    {"http://example.test/cat.empty", "fetch", "foo/bar", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kHigh,
     WebURLRequest::kRequestContextSubresource, nullptr, true, true,
     kReferrerPolicyDefault},
    {"http://example.test/cat.blob", "blabla", "foo/bar", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextSubresource, nullptr, false, false,
     kReferrerPolicyDefault},
    {"http://example.test/cat.blob", "", "foo/bar", "", "",
     kReferrerPolicyDefault, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextSubresource, nullptr, false, false,
     kReferrerPolicyDefault},
    // Media tests
    {"http://example.test/cat.gif", "image", "image/gif", "(max-width: 600px)",
     "", kReferrerPolicyDefault, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextImage, nullptr, true, true,
     kReferrerPolicyDefault},
    {"http://example.test/cat.gif", "image", "image/gif", "(max-width: 400px)",
     "", kReferrerPolicyDefault, ResourceLoadPriority::kUnresolved,
     WebURLRequest::kRequestContextImage, nullptr, true, false,
     kReferrerPolicyDefault},
    {"http://example.test/cat.gif", "image", "image/gif", "(max-width: 600px)",
     "", kReferrerPolicyDefault, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextImage, nullptr, false, false,
     kReferrerPolicyDefault},
    // Referrer Policy
    {"http://example.test/cat.gif", "image", "image/gif", "", "",
     kReferrerPolicyOrigin, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextImage, nullptr, true, true,
     kReferrerPolicyOrigin},
    {"http://example.test/cat.gif", "image", "image/gif", "", "",
     kReferrerPolicyOriginWhenCrossOrigin, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextImage, nullptr, true, true,
     kReferrerPolicyOriginWhenCrossOrigin},
    {"http://example.test/cat.gif", "image", "image/gif", "", "",
     kReferrerPolicySameOrigin, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextImage, nullptr, true, true,
     kReferrerPolicySameOrigin},
    {"http://example.test/cat.gif", "image", "image/gif", "", "",
     kReferrerPolicyStrictOrigin, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextImage, nullptr, true, true,
     kReferrerPolicyStrictOrigin},
    {"http://example.test/cat.gif", "image", "image/gif", "", "",
     kReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin,
     ResourceLoadPriority::kLow, WebURLRequest::kRequestContextImage, nullptr,
     true, true, kReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin},
    {"http://example.test/cat.gif", "image", "image/gif", "", "",
     kReferrerPolicyNever, ResourceLoadPriority::kLow,
     WebURLRequest::kRequestContextImage, nullptr, true, true,
     kReferrerPolicyNever},
    {"http://example.test/cat.js", "script", "", "", "abc",
     kReferrerPolicyDefault, ResourceLoadPriority::kHigh,
     WebURLRequest::kRequestContextScript, "script-src 'nonce-abc'", true, true,
     kReferrerPolicyDefault},
    {"http://example.test/cat.js", "script", "", "", "", kReferrerPolicyDefault,
     ResourceLoadPriority::kHigh, WebURLRequest::kRequestContextScript,
     "script-src 'nonce-abc'", false, false, kReferrerPolicyDefault},
    {"http://example.test/cat.js", "script", "", "", "def",
     kReferrerPolicyDefault, ResourceLoadPriority::kHigh,
     WebURLRequest::kRequestContextScript, "script-src 'nonce-abc'", false,
     false, kReferrerPolicyDefault}};

INSTANTIATE_TEST_CASE_P(LinkLoaderPreloadTest,
                        LinkLoaderPreloadTest,
                        ::testing::ValuesIn(kPreloadTestParams));

struct ModulePreloadTestParams {
  const char* href;
  const char* nonce;
  const char* integrity;
  CrossOriginAttributeValue cross_origin;
  ReferrerPolicy referrer_policy;
  bool expecting_load;
  network::mojom::FetchCredentialsMode expected_credentials_mode;
};

constexpr ModulePreloadTestParams kModulePreloadTestParams[] = {
    {"", nullptr, nullptr, kCrossOriginAttributeNotSet, kReferrerPolicyDefault,
     false, network::mojom::FetchCredentialsMode::kOmit},
    {"http://example.test/cat.js", nullptr, nullptr,
     kCrossOriginAttributeNotSet, kReferrerPolicyDefault, true,
     network::mojom::FetchCredentialsMode::kOmit},
    {"http://example.test/cat.js", nullptr, nullptr,
     kCrossOriginAttributeAnonymous, kReferrerPolicyDefault, true,
     network::mojom::FetchCredentialsMode::kSameOrigin},
    {"http://example.test/cat.js", "nonce", nullptr,
     kCrossOriginAttributeNotSet, kReferrerPolicyNever, true,
     network::mojom::FetchCredentialsMode::kOmit},
    {"http://example.test/cat.js", nullptr, "sha384-abc",
     kCrossOriginAttributeNotSet, kReferrerPolicyDefault, true,
     network::mojom::FetchCredentialsMode::kOmit}};

class LinkLoaderModulePreloadTest
    : public ::testing::TestWithParam<ModulePreloadTestParams> {};

class ModulePreloadTestModulator final : public DummyModulator {
 public:
  ModulePreloadTestModulator(const ModulePreloadTestParams* params)
      : params_(params), fetched_(false) {}

  void FetchSingle(const ModuleScriptFetchRequest& request,
                   ModuleGraphLevel,
                   SingleModuleClient*) override {
    fetched_ = true;

    EXPECT_EQ(KURL(NullURL(), params_->href), request.Url());
    EXPECT_EQ(params_->nonce, request.Options().Nonce());
    EXPECT_EQ(kNotParserInserted, request.Options().ParserState());
    EXPECT_EQ(params_->expected_credentials_mode,
              request.Options().CredentialsMode());
    EXPECT_EQ(AtomicString(), request.GetReferrer());
    EXPECT_EQ(params_->referrer_policy, request.GetReferrerPolicy());
    EXPECT_EQ(params_->integrity,
              request.Options().GetIntegrityAttributeValue());
  }

  bool fetched() const { return fetched_; }

 private:
  const ModulePreloadTestParams* params_;
  bool fetched_;
};

TEST_P(LinkLoaderModulePreloadTest, ModulePreload) {
  const auto& test_case = GetParam();
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create();
  ModulePreloadTestModulator* modulator =
      new ModulePreloadTestModulator(&test_case);
  Modulator::SetModulator(
      ToScriptStateForMainWorld(dummy_page_holder->GetDocument().GetFrame()),
      modulator);
  Persistent<MockLinkLoaderClient> loader_client =
      MockLinkLoaderClient::Create(true);
  LinkLoader* loader = LinkLoader::Create(loader_client.Get());
  KURL href_url = KURL(NullURL(), test_case.href);
  loader->LoadLink(LinkRelAttribute("modulepreload"), test_case.cross_origin,
                   String() /* type */, String() /* as */, String() /* media */,
                   test_case.nonce, test_case.integrity,
                   test_case.referrer_policy, href_url,
                   dummy_page_holder->GetDocument(), NetworkHintsMock());
  ASSERT_EQ(test_case.expecting_load, modulator->fetched());
}

INSTANTIATE_TEST_CASE_P(LinkLoaderModulePreloadTest,
                        LinkLoaderModulePreloadTest,
                        ::testing::ValuesIn(kModulePreloadTestParams));

TEST(LinkLoaderTest, Prefetch) {
  struct TestCase {
    const char* href;
    // TODO(yoav): Add support for type and media crbug.com/662687
    const char* type;
    const char* media;
    const ReferrerPolicy referrer_policy;
    const bool link_loader_should_load_value;
    const bool expecting_load;
    const ReferrerPolicy expected_referrer_policy;
  } cases[] = {
      // Referrer Policy
      {"http://example.test/cat.jpg", "image/jpg", "", kReferrerPolicyOrigin,
       true, true, kReferrerPolicyOrigin},
      {"http://example.test/cat.jpg", "image/jpg", "",
       kReferrerPolicyOriginWhenCrossOrigin, true, true,
       kReferrerPolicyOriginWhenCrossOrigin},
      {"http://example.test/cat.jpg", "image/jpg", "", kReferrerPolicyNever,
       true, true, kReferrerPolicyNever},
  };

  // Test the cases with a single header
  for (const auto& test_case : cases) {
    std::unique_ptr<DummyPageHolder> dummy_page_holder =
        DummyPageHolder::Create(IntSize(500, 500));
    dummy_page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
    Persistent<MockLinkLoaderClient> loader_client =
        MockLinkLoaderClient::Create(test_case.link_loader_should_load_value);
    LinkLoader* loader = LinkLoader::Create(loader_client.Get());
    KURL href_url = KURL(NullURL(), test_case.href);
    URLTestHelpers::RegisterMockedErrorURLLoad(href_url);
    loader->LoadLink(LinkRelAttribute("prefetch"), kCrossOriginAttributeNotSet,
                     test_case.type, "", test_case.media, "", "",
                     test_case.referrer_policy, href_url,
                     dummy_page_holder->GetDocument(), NetworkHintsMock());
    ASSERT_TRUE(dummy_page_holder->GetDocument().Fetcher());
    Resource* resource = loader->GetResourceForTesting();
    if (test_case.expecting_load) {
      EXPECT_TRUE(resource);
    } else {
      EXPECT_FALSE(resource);
    }
    if (resource) {
      if (test_case.expected_referrer_policy != kReferrerPolicyDefault) {
        EXPECT_EQ(test_case.expected_referrer_policy,
                  resource->GetResourceRequest().GetReferrerPolicy());
      }
    }
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }
}

TEST(LinkLoaderTest, DNSPrefetch) {
  struct {
    const char* href;
    const bool should_load;
  } cases[] = {
      {"http://example.com/", true},
      {"https://example.com/", true},
      {"//example.com/", true},
      {"//example.com/", false},
  };

  // Test the cases with a single header
  for (const auto& test_case : cases) {
    std::unique_ptr<DummyPageHolder> dummy_page_holder =
        DummyPageHolder::Create(IntSize(500, 500));
    dummy_page_holder->GetDocument().GetSettings()->SetDNSPrefetchingEnabled(
        true);
    Persistent<MockLinkLoaderClient> loader_client =
        MockLinkLoaderClient::Create(test_case.should_load);
    LinkLoader* loader = LinkLoader::Create(loader_client.Get());
    KURL href_url = KURL(KURL(String("http://example.com")), test_case.href);
    NetworkHintsMock network_hints;
    loader->LoadLink(LinkRelAttribute("dns-prefetch"),
                     kCrossOriginAttributeNotSet, String(), String(), String(),
                     String(), String(), kReferrerPolicyDefault, href_url,
                     dummy_page_holder->GetDocument(), network_hints);
    EXPECT_FALSE(network_hints.DidPreconnect());
    EXPECT_EQ(test_case.should_load, network_hints.DidDnsPrefetch());
  }
}

TEST(LinkLoaderTest, Preconnect) {
  struct {
    const char* href;
    CrossOriginAttributeValue cross_origin;
    const bool should_load;
    const bool is_https;
    const bool is_cross_origin;
  } cases[] = {
      {"http://example.com/", kCrossOriginAttributeNotSet, true, false, false},
      {"https://example.com/", kCrossOriginAttributeNotSet, true, true, false},
      {"http://example.com/", kCrossOriginAttributeAnonymous, true, false,
       true},
      {"//example.com/", kCrossOriginAttributeNotSet, true, false, false},
      {"http://example.com/", kCrossOriginAttributeNotSet, false, false, false},
  };

  // Test the cases with a single header
  for (const auto& test_case : cases) {
    std::unique_ptr<DummyPageHolder> dummy_page_holder =
        DummyPageHolder::Create(IntSize(500, 500));
    Persistent<MockLinkLoaderClient> loader_client =
        MockLinkLoaderClient::Create(test_case.should_load);
    LinkLoader* loader = LinkLoader::Create(loader_client.Get());
    KURL href_url = KURL(KURL(String("http://example.com")), test_case.href);
    NetworkHintsMock network_hints;
    loader->LoadLink(LinkRelAttribute("preconnect"), test_case.cross_origin,
                     String(), String(), String(), String(), String(),
                     kReferrerPolicyDefault, href_url,
                     dummy_page_holder->GetDocument(), network_hints);
    EXPECT_EQ(test_case.should_load, network_hints.DidPreconnect());
    EXPECT_EQ(test_case.is_https, network_hints.IsHTTPS());
    EXPECT_EQ(test_case.is_cross_origin, network_hints.IsCrossOrigin());
  }
}

TEST(LinkLoaderTest, PreloadAndPrefetch) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(500, 500));
  ResourceFetcher* fetcher = dummy_page_holder->GetDocument().Fetcher();
  ASSERT_TRUE(fetcher);
  dummy_page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
  Persistent<MockLinkLoaderClient> loader_client =
      MockLinkLoaderClient::Create(true);
  LinkLoader* loader = LinkLoader::Create(loader_client.Get());
  KURL href_url = KURL(KURL(), "https://www.example.com/");
  URLTestHelpers::RegisterMockedErrorURLLoad(href_url);
  loader->LoadLink(LinkRelAttribute("preload prefetch"),
                   kCrossOriginAttributeNotSet, "application/javascript",
                   "script", "", "", "", kReferrerPolicyDefault, href_url,
                   dummy_page_holder->GetDocument(), NetworkHintsMock());
  ASSERT_EQ(1, fetcher->CountPreloads());
  Resource* resource = loader->GetResourceForTesting();
  ASSERT_NE(resource, nullptr);
  EXPECT_TRUE(resource->IsLinkPreload());
}

}  // namespace

}  // namespace blink
