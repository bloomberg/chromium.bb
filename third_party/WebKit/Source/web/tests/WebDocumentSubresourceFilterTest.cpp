// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebDocumentSubresourceFilter.h"

#include "core/dom/Element.h"
#include "core/html/HTMLImageElement.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCache.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebDocument.h"
#include "public/web/WebElement.h"
#include "public/web/WebLocalFrame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/tests/FrameTestHelpers.h"

namespace blink {

namespace {

class TestDocumentSubresourceFilter : public WebDocumentSubresourceFilter {
 public:
  explicit TestDocumentSubresourceFilter(bool allowLoads)
      : m_allowLoads(allowLoads) {}

  LoadPolicy getLoadPolicy(const WebURL& resourceUrl,
                           WebURLRequest::RequestContext) override {
    std::string resourcePath = WebString(KURL(resourceUrl).path()).utf8();
    if (std::find(m_queriedSubresourcePaths.begin(),
                  m_queriedSubresourcePaths.end(),
                  resourcePath) == m_queriedSubresourcePaths.end())
      m_queriedSubresourcePaths.push_back(resourcePath);
    return m_allowLoads ? Allow : Disallow;
  }

  void reportDisallowedLoad() override {}

  const std::vector<std::string>& queriedSubresourcePaths() const {
    return m_queriedSubresourcePaths;
  }

 private:
  std::vector<std::string> m_queriedSubresourcePaths;
  bool m_allowLoads;
};

class SubresourceFilteringWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  void didStartProvisionalLoad(WebDataSource* dataSource) override {
    // Normally, the filter should be set when the load is committed. For
    // the sake of this test, however, inject it earlier to verify that it
    // is not consulted for the main resource load.
    m_subresourceFilter =
        new TestDocumentSubresourceFilter(m_allowSubresourcesFromNextLoad);
    dataSource->setSubresourceFilter(m_subresourceFilter);
  }

  void setAllowSubresourcesFromNextLoad(bool allow) {
    m_allowSubresourcesFromNextLoad = allow;
  }
  const TestDocumentSubresourceFilter* subresourceFilter() const {
    return m_subresourceFilter;
  }

 private:
  // Weak, owned by WebDataSource.
  TestDocumentSubresourceFilter* m_subresourceFilter = nullptr;
  bool m_allowSubresourcesFromNextLoad = false;
};

}  // namespace

class WebDocumentSubresourceFilterTest : public ::testing::Test {
 protected:
  WebDocumentSubresourceFilterTest() : m_baseURL("http://internal.test/") {
    registerMockedHttpURLLoad("white-1x1.png");
    registerMockedHttpURLLoad("foo_with_image.html");
    m_webViewHelper.initialize(false /* enableJavascript */, &m_client);
  }

  void loadDocument(bool allowSubresources) {
    m_client.setAllowSubresourcesFromNextLoad(allowSubresources);
    FrameTestHelpers::loadFrame(mainFrame(), baseURL() + "foo_with_image.html");
  }

  void expectSubresourceWasLoaded(bool loaded) {
    WebElement webElement = mainFrame()->document().querySelector("img");
    ASSERT_TRUE(isHTMLImageElement(webElement));
    HTMLImageElement* imageElement = toHTMLImageElement(webElement);
    EXPECT_EQ(loaded, !!imageElement->naturalWidth());
  }

  const std::string& baseURL() const { return m_baseURL; }
  WebFrame* mainFrame() { return m_webViewHelper.webView()->mainFrame(); }
  const std::vector<std::string>& queriedSubresourcePaths() const {
    return m_client.subresourceFilter()->queriedSubresourcePaths();
  }

 private:
  void registerMockedHttpURLLoad(const std::string& fileName) {
    URLTestHelpers::registerMockedURLLoadFromBase(
        WebString::fromUTF8(m_baseURL), testing::webTestDataPath(),
        WebString::fromUTF8(fileName));
  }

  // ::testing::Test:
  void TearDown() override {
    Platform::current()
        ->getURLLoaderMockFactory()
        ->unregisterAllURLsAndClearMemoryCache();
  }

  SubresourceFilteringWebFrameClient m_client;
  FrameTestHelpers::WebViewHelper m_webViewHelper;
  std::string m_baseURL;
};

TEST_F(WebDocumentSubresourceFilterTest, AllowedSubresource) {
  loadDocument(true /* allowSubresources */);
  expectSubresourceWasLoaded(true);
  // The filter should not be consulted for the main document resource.
  EXPECT_THAT(queriedSubresourcePaths(),
              ::testing::ElementsAre("/white-1x1.png"));
}

TEST_F(WebDocumentSubresourceFilterTest, DisallowedSubresource) {
  loadDocument(false /* allowSubresources */);
  expectSubresourceWasLoaded(false);
}

TEST_F(WebDocumentSubresourceFilterTest, FilteringDecisionIsMadeLoadByLoad) {
  for (const bool allowSubresources : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "First load allows subresources = "
                                      << allowSubresources);

    loadDocument(allowSubresources);
    expectSubresourceWasLoaded(allowSubresources);

    loadDocument(!allowSubresources);
    expectSubresourceWasLoaded(!allowSubresources);
    EXPECT_THAT(queriedSubresourcePaths(),
                ::testing::ElementsAre("/white-1x1.png"));

    WebCache::clear();
  }
}

}  // namespace blink
