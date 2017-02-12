#include "core/frame/FrameView.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebHistoryItem.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"

namespace blink {

class ProgrammaticScrollTest : public ::testing::Test {
 public:
  ProgrammaticScrollTest() : m_baseURL("http://www.test.com/") {}

  void TearDown() override {
    Platform::current()
        ->getURLLoaderMockFactory()
        ->unregisterAllURLsAndClearMemoryCache();
  }

 protected:
  void registerMockedHttpURLLoad(const std::string& fileName) {
    URLTestHelpers::registerMockedURLLoadFromBase(
        WebString::fromUTF8(m_baseURL), testing::webTestDataPath(),
        WebString::fromUTF8(fileName));
  }

  std::string m_baseURL;
  FrameTestHelpers::TestWebFrameClient m_mockWebFrameClient;
};

TEST_F(ProgrammaticScrollTest, RestoreScrollPositionAndViewStateWithScale) {
  registerMockedHttpURLLoad("long_scroll.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "long_scroll.html", true, 0, 0);
  webView->resize(WebSize(1000, 1000));
  webView->updateAllLifecyclePhases();

  WebViewImpl* webViewImpl = toWebViewImpl(webView);
  FrameLoader& loader = webViewImpl->mainFrameImpl()->frame()->loader();
  loader.documentLoader()->setLoadType(FrameLoadTypeBackForward);

  webViewImpl->setPageScaleFactor(3.0f);
  webViewImpl->mainFrame()->setScrollOffset(WebSize(0, 500));
  loader.documentLoader()->initialScrollState().wasScrolledByUser = false;
  loader.currentItem()->setPageScaleFactor(2);
  loader.currentItem()->setScrollOffset(ScrollOffset(0, 200));

  // Flip back the wasScrolledByUser flag which was set to true by
  // setPageScaleFactor because otherwise
  // FrameLoader::restoreScrollPositionAndViewState does nothing.
  loader.documentLoader()->initialScrollState().wasScrolledByUser = false;
  loader.restoreScrollPositionAndViewState();

  // Expect that both scroll and scale were restored.
  EXPECT_EQ(2.0f, webViewImpl->pageScaleFactor());
  EXPECT_EQ(200, webViewImpl->mainFrameImpl()->getScrollOffset().height);
}

TEST_F(ProgrammaticScrollTest, RestoreScrollPositionAndViewStateWithoutScale) {
  registerMockedHttpURLLoad("long_scroll.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "long_scroll.html", true, 0, 0);
  webView->resize(WebSize(1000, 1000));
  webView->updateAllLifecyclePhases();

  WebViewImpl* webViewImpl = toWebViewImpl(webView);
  FrameLoader& loader = webViewImpl->mainFrameImpl()->frame()->loader();
  loader.documentLoader()->setLoadType(FrameLoadTypeBackForward);

  webViewImpl->setPageScaleFactor(3.0f);
  webViewImpl->mainFrame()->setScrollOffset(WebSize(0, 500));
  loader.documentLoader()->initialScrollState().wasScrolledByUser = false;
  loader.currentItem()->setPageScaleFactor(0);
  loader.currentItem()->setScrollOffset(ScrollOffset(0, 400));

  // FrameLoader::restoreScrollPositionAndViewState flows differently if scale
  // is zero.
  loader.restoreScrollPositionAndViewState();

  // Expect that only the scroll position was restored.
  EXPECT_EQ(3.0f, webViewImpl->pageScaleFactor());
  EXPECT_EQ(400, webViewImpl->mainFrameImpl()->getScrollOffset().height);
}

}  // namespace blink
