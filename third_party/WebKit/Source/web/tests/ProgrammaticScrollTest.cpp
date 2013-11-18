#include "config.h"

#include <gtest/gtest.h>
#include "FrameTestHelpers.h"
#include "URLTestHelpers.h"
#include "WebFrame.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebHistoryItem.h"
#include "WebInputEvent.h"
#include "WebScriptSource.h"
#include "WebSettings.h"
#include "WebView.h"
#include "WebViewImpl.h"
#include "core/frame/FrameView.h"
#include "core/rendering/RenderView.h"
#include "public/platform/Platform.h"
#include "public/platform/WebUnitTestSupport.h"

using namespace WebCore;
using namespace blink;

namespace {

class MockWebFrameClient : public WebFrameClient {
};

class ProgrammaticScrollTest : public testing::Test {
public:
    ProgrammaticScrollTest()
        : m_baseURL("http://www.test.com/")
    {
    }

    virtual void TearDown()
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

protected:

    void registerMockedHttpURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8(fileName.c_str()));
    }

    std::string m_baseURL;
    MockWebFrameClient m_mockWebFrameClient;
};

TEST_F(ProgrammaticScrollTest, RestoreScrollPositionAndViewStateWithScale)
{
    registerMockedHttpURLLoad("long_scroll.html");

    FrameTestHelpers::WebViewHelper webViewHelper;
    WebView* webView = webViewHelper.initializeAndLoad(m_baseURL + "long_scroll.html", true, 0, 0);
    webView->resize(WebSize(1000, 1000));
    webView->layout();

    WebViewImpl* webViewImpl = toWebViewImpl(webView);
    Frame* frame = webViewImpl->mainFrameImpl()->frame();
    HistoryController* history = webViewImpl->page()->history();

    // Scale and scroll the page and save that state. Then scale and scroll again and restore.
    webViewImpl->setPageScaleFactor(2.0f, WebPoint(0, 200));
    history->saveDocumentAndScrollState(frame);
    webViewImpl->setPageScaleFactor(3.0f, WebPoint(0, 300));
    // Flip back the wasScrolledByUser flag which was set to true by setPageScaleFactor
    // because otherwise HistoryController::restoreScrollPositionAndViewState does nothing.
    frame->view()->setWasScrolledByUser(false);
    history->restoreScrollPositionAndViewState(frame);

    // Expect that both scroll and scale were restored, and that it was not a programmatic scroll.
    EXPECT_EQ(2.0f, webViewImpl->pageScaleFactor());
    EXPECT_EQ(200, webViewImpl->mainFrameImpl()->scrollOffset().height);
    EXPECT_TRUE(frame->view()->wasScrolledByUser());
}

TEST_F(ProgrammaticScrollTest, RestoreScrollPositionAndViewStateWithoutScale)
{
    registerMockedHttpURLLoad("long_scroll.html");

    FrameTestHelpers::WebViewHelper webViewHelper;
    WebView* webView = webViewHelper.initializeAndLoad(m_baseURL + "long_scroll.html", true, 0, 0);
    webView->resize(WebSize(1000, 1000));
    webView->layout();

    WebViewImpl* webViewImpl = toWebViewImpl(webView);
    Frame* frame = webViewImpl->mainFrameImpl()->frame();
    HistoryController* history = webViewImpl->page()->history();

    // Scale and scroll the page and save that state, but then set scale to zero. Then scale and
    // scroll again and restore.
    webViewImpl->setPageScaleFactor(2.0f, WebPoint(0, 400));
    history->saveDocumentAndScrollState(frame);
    webViewImpl->setPageScaleFactor(3.0f, WebPoint(0, 500));
    // Flip back the wasScrolledByUser flag which was set to true by setPageScaleFactor
    // because otherwise HistoryController::restoreScrollPositionAndViewState does nothing.
    frame->view()->setWasScrolledByUser(false);
    // HistoryController::restoreScrollPositionAndViewState flows differently if scale is zero.
    history->currentItem(frame)->setPageScaleFactor(0.0f);
    history->restoreScrollPositionAndViewState(frame);

    // Expect that only the scroll position was restored, and that it was not a programmatic scroll.
    EXPECT_EQ(3.0f, webViewImpl->pageScaleFactor());
    EXPECT_EQ(400, webViewImpl->mainFrameImpl()->scrollOffset().height);
    EXPECT_TRUE(frame->view()->wasScrolledByUser());
}

}
