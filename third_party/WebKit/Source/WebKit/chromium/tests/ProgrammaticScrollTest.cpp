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
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "core/page/FrameView.h"
#include "core/rendering/RenderView.h"
#include <public/Platform.h>
#include <public/WebUnitTestSupport.h>

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

}
