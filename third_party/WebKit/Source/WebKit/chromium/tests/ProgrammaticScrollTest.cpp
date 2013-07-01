#include "config.h"

#include <gtest/gtest.h>
#include "FrameTestHelpers.h"
#include "RuntimeEnabledFeatures.h"
#include "URLTestHelpers.h"
#include "WebFrame.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebHistoryItem.h"
#include "WebInputEvent.h"
#include "WebScriptSource.h"
#include "WebSettings.h"
#include "WebView.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "core/page/FrameView.h"
#include "core/rendering/RenderView.h"
#include "public/platform/Platform.h"
#include "public/platform/WebUnitTestSupport.h"

using namespace WebCore;
using namespace WebKit;

namespace {

class MockWebFrameClient : public WebFrameClient {
};

class ProgrammaticScrollTest : public testing::Test {
public:
    ProgrammaticScrollTest()
        : m_baseURL("http://www.test.com/")
    {
    }

    virtual void SetUp()
    {
        RuntimeEnabledFeatures::setProgrammaticScrollNotificationsEnabled(true);
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

class TestProgrammaticScrollClient : public WebViewClient {
public:
    TestProgrammaticScrollClient()
    {
        reset();
    }
    void reset()
    {
        m_eventReceived = false;
    }
    bool eventReceived() const { return m_eventReceived; }

    // WebWidgetClient:
    virtual void didProgrammaticallyScroll(const WebPoint&) OVERRIDE
    {
        m_eventReceived = true;
    }

private:
    bool m_eventReceived;
};

TEST_F(ProgrammaticScrollTest, UserScroll)
{
    registerMockedHttpURLLoad("short_scroll.html");
    TestProgrammaticScrollClient client;

    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "short_scroll.html", false, 0, &client);
    webView->resize(WebSize(1000, 1000));
    webView->layout();

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(webView);
    EXPECT_FALSE(client.eventReceived());

    // Non zero page scale and scroll.
    webViewImpl->applyScrollAndScale(WebSize(9, 13), 2.0f);
    EXPECT_FALSE(client.eventReceived());

    webView->close();
}

TEST_F(ProgrammaticScrollTest, ProgrammaticScroll)
{
    registerMockedHttpURLLoad("long_scroll.html");
    TestProgrammaticScrollClient client;

    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "long_scroll.html", true, 0, &client);
    webView->resize(WebSize(1000, 1000));
    webView->layout();

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(webView);
    WebFrameImpl* frameImpl = webViewImpl->mainFrameImpl();
    FrameView* frameView = frameImpl->frameView();

    // Slow scroll path.
    frameView->setCanBlitOnScroll(false);
    EXPECT_FALSE(client.eventReceived());
    frameImpl->executeScript(WebScriptSource("window.scrollTo(0, 20);"));
    EXPECT_TRUE(client.eventReceived());
    client.reset();
    frameImpl->executeScript(WebScriptSource("window.scrollBy(0, 0);"));
    EXPECT_FALSE(client.eventReceived());
    client.reset();

    // Fast scroll path.
    frameImpl->frameView()->setCanBlitOnScroll(true);
    EXPECT_FALSE(client.eventReceived());
    frameImpl->executeScript(WebScriptSource("window.scrollTo(0, 21);"));
    EXPECT_TRUE(client.eventReceived());
    client.reset();
    frameImpl->executeScript(WebScriptSource("window.scrollBy(0, 0);"));
    EXPECT_FALSE(client.eventReceived());
    client.reset();

    webView->close();
}

TEST_F(ProgrammaticScrollTest, UserScrollOnMainThread)
{
    registerMockedHttpURLLoad("long_scroll.html");
    TestProgrammaticScrollClient client;

    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "long_scroll.html", true, 0, &client);
    webView->resize(WebSize(1000, 1000));
    webView->layout();

    WebGestureEvent gesture;
    gesture.type = WebInputEvent::GestureScrollBegin;
    webView->handleInputEvent(gesture);
    FrameTestHelpers::runPendingTasks();
    EXPECT_FALSE(client.eventReceived());

    gesture.type = WebInputEvent::GestureScrollUpdate;
    gesture.data.scrollUpdate.deltaY = 40;
    webView->handleInputEvent(gesture);
    FrameTestHelpers::runPendingTasks();
    EXPECT_FALSE(client.eventReceived());

    gesture.type = WebInputEvent::GestureScrollEnd;
    gesture.data.scrollUpdate.deltaY = 0;
    webView->handleInputEvent(gesture);
    FrameTestHelpers::runPendingTasks();
    EXPECT_FALSE(client.eventReceived());

    webView->close();
}

TEST_F(ProgrammaticScrollTest, RestoreScrollPositionAndViewStateWithScale)
{
    registerMockedHttpURLLoad("long_scroll.html");
    TestProgrammaticScrollClient client;

    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "long_scroll.html", true, 0, &client);
    webView->resize(WebSize(1000, 1000));
    webView->layout();

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(webView);
    FrameView* frameView = webViewImpl->mainFrameImpl()->frameView();
    HistoryController* history = webViewImpl->page()->mainFrame()->loader()->history();

    // Scale and scroll the page and save that state. Then scale and scroll again and restore.
    webViewImpl->setPageScaleFactor(2.0f, WebPoint(0, 200));
    history->saveDocumentAndScrollState();
    webViewImpl->setPageScaleFactor(3.0f, WebPoint(0, 300));
    // Flip back the wasScrolledByUser flag which was set to true by setPageScaleFactor
    // because otherwise HistoryController::restoreScrollPositionAndViewState does nothing.
    frameView->setWasScrolledByUser(false);
    history->restoreScrollPositionAndViewState();

    // Expect that both scroll and scale were restored, and that it was not a programmatic scroll.
    EXPECT_EQ(2.0f, webViewImpl->pageScaleFactor());
    EXPECT_EQ(200, webViewImpl->mainFrameImpl()->scrollOffset().height);
    EXPECT_TRUE(frameView->wasScrolledByUser());
    EXPECT_FALSE(client.eventReceived());

    webView->close();
}

TEST_F(ProgrammaticScrollTest, RestoreScrollPositionAndViewStateWithoutScale)
{
    registerMockedHttpURLLoad("long_scroll.html");
    TestProgrammaticScrollClient client;

    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "long_scroll.html", true, 0, &client);
    webView->resize(WebSize(1000, 1000));
    webView->layout();

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(webView);
    FrameView* frameView = webViewImpl->mainFrameImpl()->frameView();
    HistoryController* history = webViewImpl->page()->mainFrame()->loader()->history();

    // Scale and scroll the page and save that state, but then set scale to zero. Then scale and
    // scroll again and restore.
    webViewImpl->setPageScaleFactor(2.0f, WebPoint(0, 400));
    history->saveDocumentAndScrollState();
    webViewImpl->setPageScaleFactor(3.0f, WebPoint(0, 500));
    // Flip back the wasScrolledByUser flag which was set to true by setPageScaleFactor
    // because otherwise HistoryController::restoreScrollPositionAndViewState does nothing.
    frameView->setWasScrolledByUser(false);
    // HistoryController::restoreScrollPositionAndViewState flows differently if scale is zero.
    history->currentItem()->setPageScaleFactor(0.0f);
    history->restoreScrollPositionAndViewState();

    // Expect that only the scroll position was restored, and that it was not a programmatic scroll.
    EXPECT_EQ(3.0f, webViewImpl->pageScaleFactor());
    EXPECT_EQ(400, webViewImpl->mainFrameImpl()->scrollOffset().height);
    EXPECT_TRUE(frameView->wasScrolledByUser());
    EXPECT_FALSE(client.eventReceived());

    webView->close();
}

}
