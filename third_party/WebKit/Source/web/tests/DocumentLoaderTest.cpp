// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/DocumentLoader.h"

#include <queue>
#include "core/page/Page.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderClient.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "wtf/AutoReset.h"

namespace blink {

// TODO(dcheng): Ideally, enough of FrameTestHelpers would be in core/ that
// placing a test for a core/ class in web/ wouldn't be necessary.
class DocumentLoaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_webViewHelper.initialize();
    URLTestHelpers::registerMockedURLLoad(
        URLTestHelpers::toKURL("https://example.com/foo.html"),
        testing::webTestDataPath("foo.html"));
  }

  void TearDown() override {
    Platform::current()
        ->getURLLoaderMockFactory()
        ->unregisterAllURLsAndClearMemoryCache();
  }

  WebLocalFrameImpl* mainFrame() {
    return m_webViewHelper.webView()->mainFrameImpl();
  }

  FrameTestHelpers::WebViewHelper m_webViewHelper;
};

TEST_F(DocumentLoaderTest, SingleChunk) {
  class TestDelegate : public WebURLLoaderTestDelegate {
   public:
    void didReceiveData(WebURLLoaderClient* originalClient,
                        const char* data,
                        int dataLength) override {
      EXPECT_EQ(34, dataLength) << "foo.html was not served in a single chunk";
      originalClient->didReceiveData(data, dataLength);
    }
  } delegate;

  Platform::current()->getURLLoaderMockFactory()->setLoaderDelegate(&delegate);
  FrameTestHelpers::loadFrame(mainFrame(), "https://example.com/foo.html");
  Platform::current()->getURLLoaderMockFactory()->setLoaderDelegate(nullptr);

  // TODO(dcheng): How should the test verify that the original callback is
  // invoked? The test currently still passes even if the test delegate
  // forgets to invoke the callback.
}

// Test normal case of DocumentLoader::dataReceived(): data in multiple chunks,
// with no reentrancy.
TEST_F(DocumentLoaderTest, MultiChunkNoReentrancy) {
  class TestDelegate : public WebURLLoaderTestDelegate {
   public:
    void didReceiveData(WebURLLoaderClient* originalClient,
                        const char* data,
                        int dataLength) override {
      EXPECT_EQ(34, dataLength) << "foo.html was not served in a single chunk";
      // Chunk the reply into one byte chunks.
      for (int i = 0; i < dataLength; ++i)
        originalClient->didReceiveData(&data[i], 1);
    }
  } delegate;

  Platform::current()->getURLLoaderMockFactory()->setLoaderDelegate(&delegate);
  FrameTestHelpers::loadFrame(mainFrame(), "https://example.com/foo.html");
  Platform::current()->getURLLoaderMockFactory()->setLoaderDelegate(nullptr);
}

// Finally, test reentrant callbacks to DocumentLoader::dataReceived().
TEST_F(DocumentLoaderTest, MultiChunkWithReentrancy) {
  // This test delegate chunks the response stage into three distinct stages:
  // 1. The first dataReceived() callback, which triggers frame detach due to
  //    commiting a provisional load.
  // 2.  The middle part of the response, which is dispatched to
  //    dataReceived() reentrantly.
  // 3. The final chunk, which is dispatched normally at the top-level.
  class TestDelegate : public WebURLLoaderTestDelegate,
                       public FrameTestHelpers::TestWebFrameClient {
   public:
    TestDelegate()
        : m_loaderClient(nullptr),
          m_dispatchingDidReceiveData(false),
          m_servedReentrantly(false) {}

    // WebURLLoaderTestDelegate overrides:
    void didReceiveData(WebURLLoaderClient* originalClient,
                        const char* data,
                        int dataLength) override {
      EXPECT_EQ(34, dataLength) << "foo.html was not served in a single chunk";

      m_loaderClient = originalClient;
      for (int i = 0; i < dataLength; ++i)
        m_data.push(data[i]);

      {
        // Serve the first byte to the real WebURLLoaderCLient, which
        // should trigger frameDetach() due to committing a provisional
        // load.
        AutoReset<bool> dispatching(&m_dispatchingDidReceiveData, true);
        dispatchOneByte();
      }
      // Serve the remaining bytes to complete the load.
      EXPECT_FALSE(m_data.empty());
      while (!m_data.empty())
        dispatchOneByte();
    }

    // WebFrameClient overrides:
    void frameDetached(WebLocalFrame* frame, DetachType detachType) override {
      if (m_dispatchingDidReceiveData) {
        // This should be called by the first didReceiveData() call, since
        // it should commit the provisional load.
        EXPECT_GT(m_data.size(), 10u);
        // Dispatch dataReceived() callbacks for part of the remaining
        // data, saving the rest to be dispatched at the top-level as
        // normal.
        while (m_data.size() > 10)
          dispatchOneByte();
        m_servedReentrantly = true;
      }
      TestWebFrameClient::frameDetached(frame, detachType);
    }

    void dispatchOneByte() {
      char c = m_data.front();
      m_data.pop();
      m_loaderClient->didReceiveData(&c, 1);
    }

    bool servedReentrantly() const { return m_servedReentrantly; }

   private:
    WebURLLoaderClient* m_loaderClient;
    std::queue<char> m_data;
    bool m_dispatchingDidReceiveData;
    bool m_servedReentrantly;
  } delegate;
  m_webViewHelper.initialize(false, &delegate);

  // This doesn't go through the mocked URL load path: it's just intended to
  // setup a situation where didReceiveData() can be invoked reentrantly.
  FrameTestHelpers::loadHTMLString(mainFrame(), "<iframe></iframe>",
                                   URLTestHelpers::toKURL("about:blank"));

  Platform::current()->getURLLoaderMockFactory()->setLoaderDelegate(&delegate);
  FrameTestHelpers::loadFrame(mainFrame(), "https://example.com/foo.html");
  Platform::current()->getURLLoaderMockFactory()->setLoaderDelegate(nullptr);

  EXPECT_TRUE(delegate.servedReentrantly());

  // delegate is a WebFrameClient and stack-allocated, so manually reset() the
  // WebViewHelper here.
  m_webViewHelper.reset();
}

TEST_F(DocumentLoaderTest, isCommittedButEmpty) {
  WebViewImpl* webViewImpl =
      m_webViewHelper.initializeAndLoad("about:blank", true);
  EXPECT_TRUE(toLocalFrame(webViewImpl->page()->mainFrame())
                  ->loader()
                  .documentLoader()
                  ->isCommittedButEmpty());
}

}  // namespace blink
