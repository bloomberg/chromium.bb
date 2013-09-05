/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"

#include "WebFrame.h"

#include <gtest/gtest.h>
#include "FrameTestHelpers.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "URLTestHelpers.h"
#include "WebDataSource.h"
#include "WebDocument.h"
#include "WebFindOptions.h"
#include "WebFormElement.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebHistoryItem.h"
#include "WebRange.h"
#include "WebScriptSource.h"
#include "WebSearchableFormData.h"
#include "WebSecurityOrigin.h"
#include "WebSecurityPolicy.h"
#include "WebSettings.h"
#include "WebSpellCheckClient.h"
#include "WebTextCheckingCompletion.h"
#include "WebTextCheckingResult.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "core/dom/Clipboard.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/MouseEvent.h"
#include "core/dom/Range.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SpellCheckRequester.h"
#include "core/editing/VisiblePosition.h"
#include "core/html/HTMLFormElement.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Settings.h"
#include "core/platform/ScrollbarTheme.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/network/ResourceError.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/TextAutosizer.h"
#include "v8.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/platform/WebURLResponse.h"
#include "wtf/dtoa/utils.h"
#include "wtf/Forward.h"

using namespace WebKit;
using WebCore::Document;
using WebCore::DocumentMarker;
using WebCore::Element;
using WebCore::FloatRect;
using WebCore::HitTestRequest;
using WebCore::Range;
using WebKit::URLTestHelpers::toKURL;
using WebKit::FrameTestHelpers::runPendingTasks;

namespace {

const int touchPointPadding = 32;

#define EXPECT_EQ_RECT(a, b) \
    EXPECT_EQ(a.x(), b.x()); \
    EXPECT_EQ(a.y(), b.y()); \
    EXPECT_EQ(a.width(), b.width()); \
    EXPECT_EQ(a.height(), b.height());

class FakeWebFrameClient : public WebFrameClient {
    // To make the destructor public.
};

class FakeCompositingWebViewClient : public WebViewClient {
public:
    virtual ~FakeCompositingWebViewClient()
    {
    }

    virtual void initializeLayerTreeView() OVERRIDE
    {
        m_layerTreeView = adoptPtr(Platform::current()->unitTestSupport()->createLayerTreeViewForTesting(WebUnitTestSupport::TestViewTypeUnitTest));
        ASSERT(m_layerTreeView);
    }

    virtual WebLayerTreeView* layerTreeView() OVERRIDE
    {
        return m_layerTreeView.get();
    }

    FakeWebFrameClient m_fakeWebFrameClient;

private:
    OwnPtr<WebLayerTreeView> m_layerTreeView;
};

class WebFrameTest : public testing::Test {
public:
    WebFrameTest()
        : m_baseURL("http://www.test.com/")
        , m_chromeURL("chrome://")
        , m_webView(0)
    {
    }

    virtual ~WebFrameTest()
    {
        if (m_webView)
            m_webView->close();
    }

    virtual void TearDown()
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

    void registerMockedHttpURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8(fileName.c_str()));
    }

    void registerMockedChromeURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_chromeURL.c_str()), WebString::fromUTF8(fileName.c_str()));
    }

    void createCompositingWebView()
    {
        m_fakeCompositingWebViewClient = adoptPtr(new FakeCompositingWebViewClient());
        m_webView = WebView::create(m_fakeCompositingWebViewClient.get());
        m_webView->settings()->setJavaScriptEnabled(true);
        m_webView->settings()->setForceCompositingMode(true);
        m_webView->settings()->setAcceleratedCompositingEnabled(true);
        m_webView->settings()->setAcceleratedCompositingForFixedPositionEnabled(true);
        m_webView->settings()->setAcceleratedCompositingForOverflowScrollEnabled(true);
        m_webView->settings()->setAcceleratedCompositingForScrollableFramesEnabled(true);
        m_webView->settings()->setCompositedScrollingForFramesEnabled(true);
        m_webView->settings()->setFixedPositionCreatesStackingContext(true);
        m_webView->initializeMainFrame(&m_fakeCompositingWebViewClient->m_fakeWebFrameClient);
    }

protected:
    WebViewImpl* webViewImpl() const { return toWebViewImpl(m_webView); }

    std::string m_baseURL;
    std::string m_chromeURL;
    OwnPtr<FakeCompositingWebViewClient> m_fakeCompositingWebViewClient;

    WebView* m_webView;
};

TEST_F(WebFrameTest, ContentText)
{
    registerMockedHttpURLLoad("iframes_test.html");
    registerMockedHttpURLLoad("visible_iframe.html");
    registerMockedHttpURLLoad("invisible_iframe.html");
    registerMockedHttpURLLoad("zero_sized_iframe.html");

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "iframes_test.html");

    // Now retrieve the frames text and test it only includes visible elements.
    std::string content = std::string(m_webView->mainFrame()->contentAsText(1024).utf8().data());
    EXPECT_NE(std::string::npos, content.find(" visible paragraph"));
    EXPECT_NE(std::string::npos, content.find(" visible iframe"));
    EXPECT_EQ(std::string::npos, content.find(" invisible pararaph"));
    EXPECT_EQ(std::string::npos, content.find(" invisible iframe"));
    EXPECT_EQ(std::string::npos, content.find("iframe with zero size"));
}

TEST_F(WebFrameTest, FrameForEnteredContext)
{
    registerMockedHttpURLLoad("iframes_test.html");
    registerMockedHttpURLLoad("visible_iframe.html");
    registerMockedHttpURLLoad("invisible_iframe.html");
    registerMockedHttpURLLoad("zero_sized_iframe.html");

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "iframes_test.html", true);

    v8::HandleScope scope(v8::Isolate::GetCurrent());
    EXPECT_EQ(m_webView->mainFrame(),
              WebFrame::frameForContext(
                  m_webView->mainFrame()->mainWorldScriptContext()));
    EXPECT_EQ(m_webView->mainFrame()->firstChild(),
              WebFrame::frameForContext(
                  m_webView->mainFrame()->firstChild()->mainWorldScriptContext()));
}

TEST_F(WebFrameTest, FormWithNullFrame)
{
    registerMockedHttpURLLoad("form.html");

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "form.html");

    WebVector<WebFormElement> forms;
    m_webView->mainFrame()->document().forms(forms);
    m_webView->close();
    m_webView = 0;

    EXPECT_EQ(forms.size(), 1U);

    // This test passes if this doesn't crash.
    WebSearchableFormData searchableDataForm(forms[0]);
}

TEST_F(WebFrameTest, ChromePageJavascript)
{
    registerMockedChromeURLLoad("history.html");

    // Pass true to enable JavaScript.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_chromeURL + "history.html", true);

    // Try to run JS against the chrome-style URL.
    FrameTestHelpers::loadFrame(m_webView->mainFrame(), "javascript:document.body.appendChild(document.createTextNode('Clobbered'))");

    // Required to see any updates in contentAsText.
    m_webView->layout();

    // Now retrieve the frame's text and ensure it was modified by running javascript.
    std::string content = std::string(m_webView->mainFrame()->contentAsText(1024).utf8().data());
    EXPECT_NE(std::string::npos, content.find("Clobbered"));
}

TEST_F(WebFrameTest, ChromePageNoJavascript)
{
    registerMockedChromeURLLoad("history.html");

    /// Pass true to enable JavaScript.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_chromeURL + "history.html", true);

    // Try to run JS against the chrome-style URL after prohibiting it.
    WebSecurityPolicy::registerURLSchemeAsNotAllowingJavascriptURLs("chrome");
    FrameTestHelpers::loadFrame(m_webView->mainFrame(), "javascript:document.body.appendChild(document.createTextNode('Clobbered'))");

    // Required to see any updates in contentAsText.
    m_webView->layout();

    // Now retrieve the frame's text and ensure it wasn't modified by running javascript.
    std::string content = std::string(m_webView->mainFrame()->contentAsText(1024).utf8().data());
    EXPECT_EQ(std::string::npos, content.find("Clobbered"));
}

TEST_F(WebFrameTest, DispatchMessageEventWithOriginCheck)
{
    registerMockedHttpURLLoad("postmessage_test.html");

    // Pass true to enable JavaScript.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "postmessage_test.html", true);

    // Send a message with the correct origin.
    WebSecurityOrigin correctOrigin(WebSecurityOrigin::create(toKURL(m_baseURL)));
    WebDOMEvent event = m_webView->mainFrame()->document().createEvent("MessageEvent");
    WebDOMMessageEvent message = event.to<WebDOMMessageEvent>();
    WebSerializedScriptValue data(WebSerializedScriptValue::fromString("foo"));
    message.initMessageEvent("message", false, false, data, "http://origin.com", 0, "");
    m_webView->mainFrame()->dispatchMessageEventWithOriginCheck(correctOrigin, message);

    // Send another message with incorrect origin.
    WebSecurityOrigin incorrectOrigin(WebSecurityOrigin::create(toKURL(m_chromeURL)));
    m_webView->mainFrame()->dispatchMessageEventWithOriginCheck(incorrectOrigin, message);

    // Required to see any updates in contentAsText.
    m_webView->layout();

    // Verify that only the first addition is in the body of the page.
    std::string content = std::string(m_webView->mainFrame()->contentAsText(1024).utf8().data());
    EXPECT_NE(std::string::npos, content.find("Message 1."));
    EXPECT_EQ(std::string::npos, content.find("Message 2."));
}

class FixedLayoutTestWebViewClient : public WebViewClient {
 public:
    virtual WebScreenInfo screenInfo() OVERRIDE { return m_screenInfo; }

    WebScreenInfo m_screenInfo;
};

TEST_F(WebFrameTest, FrameViewNeedsLayoutOnFixedLayoutResize)
{
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    int viewportWidth = 640;
    int viewportHeight = 480;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->layout();

    webViewImpl()->mainFrameImpl()->frameView()->setFixedLayoutSize(WebCore::IntSize(100, 100));
    EXPECT_TRUE(webViewImpl()->mainFrameImpl()->frameView()->needsLayout());

    int prevLayoutCount = webViewImpl()->mainFrameImpl()->frameView()->layoutCount();
    webViewImpl()->mainFrameImpl()->frameView()->setFrameRect(WebCore::IntRect(0, 0, 641, 481));
    EXPECT_EQ(prevLayoutCount, webViewImpl()->mainFrameImpl()->frameView()->layoutCount());

    webViewImpl()->layout();
}

TEST_F(WebFrameTest, ChangeInFixedLayoutTriggersTextAutosizingRecalculate)
{
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    int viewportWidth = 640;
    int viewportHeight = 480;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);

    WebCore::Document* document = webViewImpl()->page()->mainFrame()->document();
    document->settings()->setTextAutosizingEnabled(true);
    EXPECT_TRUE(document->settings()->textAutosizingEnabled());
    webViewImpl()->resize(WebSize(viewportWidth, viewportHeight));
    webViewImpl()->layout();

    WebCore::RenderObject* renderer = document->renderer();
    bool multiplierSetAtLeastOnce = false;
    while (renderer) {
        if (renderer->style()) {
            renderer->style()->setTextAutosizingMultiplier(2);
            EXPECT_EQ(2, renderer->style()->textAutosizingMultiplier());
            multiplierSetAtLeastOnce = true;
        }
        renderer = renderer->nextInPreOrder();
    }
    EXPECT_TRUE(multiplierSetAtLeastOnce);

    WebCore::ViewportArguments arguments = document->viewportArguments();
    // Choose a width that's not going match the viewport width of the loaded document.
    arguments.minWidth = WebCore::Length(100, WebCore::Fixed);
    arguments.maxWidth = WebCore::Length(100, WebCore::Fixed);
    webViewImpl()->updatePageDefinedPageScaleConstraints(arguments);

    bool multiplierCheckedAtLeastOnce = false;
    renderer = document->renderer();
    while (renderer) {
        if (renderer->style()) {
            EXPECT_EQ(1, renderer->style()->textAutosizingMultiplier());
            multiplierCheckedAtLeastOnce = true;
        }
        renderer = renderer->nextInPreOrder();
    }
    EXPECT_TRUE(multiplierCheckedAtLeastOnce);
}

TEST_F(WebFrameTest, DeviceScaleFactorUsesDefaultWithoutViewportTag)
{
    registerMockedHttpURLLoad("no_viewport_tag.html");

    int viewportWidth = 640;
    int viewportHeight = 480;

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 2;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "no_viewport_tag.html", true, 0, &client);

    m_webView->settings()->setViewportEnabled(true);
    m_webView->enableFixedLayoutMode(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->layout();

    EXPECT_EQ(2, m_webView->deviceScaleFactor());

    // Device scale factor should be independent of page scale.
    m_webView->setPageScaleFactorLimits(1, 2);
    m_webView->setPageScaleFactorPreservingScrollOffset(0.5);
    m_webView->layout();
    EXPECT_EQ(1, m_webView->pageScaleFactor());

    // Force the layout to happen before leaving the test.
    m_webView->mainFrame()->contentAsText(1024).utf8();
}

TEST_F(WebFrameTest, FixedLayoutInitializeAtMinimumScale)
{
    WebCore::Settings::setMockScrollbarsEnabled(true);
    WebCore::Settings::setUsesOverlayScrollbars(true);

    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));

    int defaultFixedLayoutWidth = 980;
    float minimumPageScaleFactor = viewportWidth / (float) defaultFixedLayoutWidth;
    EXPECT_EQ(minimumPageScaleFactor, m_webView->pageScaleFactor());
    EXPECT_EQ(minimumPageScaleFactor, m_webView->minimumPageScaleFactor());

    // Assume the user has pinch zoomed to page scale factor 2.
    float userPinchPageScaleFactor = 2;
    m_webView->setPageScaleFactorPreservingScrollOffset(userPinchPageScaleFactor);
    m_webView->layout();

    // Make sure we don't reset to initial scale if the page continues to load.
    bool isNewNavigation;
    webViewImpl()->didCommitLoad(&isNewNavigation, false);
    webViewImpl()->didChangeContentsSize();
    EXPECT_EQ(userPinchPageScaleFactor, m_webView->pageScaleFactor());

    // Make sure we don't reset to initial scale if the viewport size changes.
    m_webView->resize(WebSize(viewportWidth, viewportHeight + 100));
    EXPECT_EQ(userPinchPageScaleFactor, m_webView->pageScaleFactor());
}

TEST_F(WebFrameTest, WideDocumentInitializeAtMinimumScale)
{
    WebCore::Settings::setMockScrollbarsEnabled(true);
    WebCore::Settings::setUsesOverlayScrollbars(true);

    registerMockedHttpURLLoad("wide_document.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "wide_document.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));

    int wideDocumentWidth = 1500;
    float minimumPageScaleFactor = viewportWidth / (float) wideDocumentWidth;
    EXPECT_EQ(minimumPageScaleFactor, m_webView->pageScaleFactor());
    EXPECT_EQ(minimumPageScaleFactor, m_webView->minimumPageScaleFactor());

    // Assume the user has pinch zoomed to page scale factor 2.
    float userPinchPageScaleFactor = 2;
    m_webView->setPageScaleFactorPreservingScrollOffset(userPinchPageScaleFactor);
    m_webView->layout();

    // Make sure we don't reset to initial scale if the page continues to load.
    bool isNewNavigation;
    webViewImpl()->didCommitLoad(&isNewNavigation, false);
    webViewImpl()->didChangeContentsSize();
    EXPECT_EQ(userPinchPageScaleFactor, m_webView->pageScaleFactor());

    // Make sure we don't reset to initial scale if the viewport size changes.
    m_webView->resize(WebSize(viewportWidth, viewportHeight + 100));
    EXPECT_EQ(userPinchPageScaleFactor, m_webView->pageScaleFactor());
}

TEST_F(WebFrameTest, setLoadWithOverviewModeToFalse)
{
    registerMockedHttpURLLoad("viewport-auto-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "viewport-auto-initial-scale.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->settings()->setLoadWithOverviewMode(false);
    m_webView->settings()->setSupportDeprecatedTargetDensityDPI(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));

    // The page must be displayed at 100% zoom.
    EXPECT_EQ(1.0f, m_webView->pageScaleFactor());
}

TEST_F(WebFrameTest, SetLoadWithOverviewModeToFalseAndNoWideViewport)
{
    registerMockedHttpURLLoad("large-div.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "large-div.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->settings()->setLoadWithOverviewMode(false);
    m_webView->settings()->setUseWideViewport(false);
    m_webView->settings()->setSupportDeprecatedTargetDensityDPI(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));

    // The page must be displayed at 100% zoom, despite that it hosts a wide div element.
    EXPECT_EQ(1.0f, m_webView->pageScaleFactor());
}

TEST_F(WebFrameTest, NoWideViewportIgnoresPageViewportWidth)
{
    registerMockedHttpURLLoad("viewport-auto-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "viewport-auto-initial-scale.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->settings()->setUseWideViewport(false);
    m_webView->settings()->setSupportDeprecatedTargetDensityDPI(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));

    // The page sets viewport width to 3000, but with UseWideViewport == false is must be ignored.
    EXPECT_EQ(viewportWidth, webViewImpl()->mainFrameImpl()->frameView()->contentsSize().width());
    EXPECT_EQ(viewportHeight, webViewImpl()->mainFrameImpl()->frameView()->contentsSize().height());
}

TEST_F(WebFrameTest, NoWideViewportIgnoresPageViewportWidthButAccountsScale)
{
    registerMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "viewport-wide-2x-initial-scale.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->settings()->setUseWideViewport(false);
    m_webView->settings()->setSupportDeprecatedTargetDensityDPI(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));

    // The page sets viewport width to 3000, but with UseWideViewport == false is must be ignored.
    // While the initial scale specified by the page must be accounted.
    EXPECT_EQ(viewportWidth / 2, webViewImpl()->mainFrameImpl()->frameView()->contentsSize().width());
    EXPECT_EQ(viewportHeight / 2, webViewImpl()->mainFrameImpl()->frameView()->contentsSize().height());
}

TEST_F(WebFrameTest, WideViewportSetsTo980WithAutoWidth)
{
    registerMockedHttpURLLoad("viewport-2x-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "viewport-2x-initial-scale.html", true, 0, &client);
    m_webView->settings()->setSupportDeprecatedTargetDensityDPI(true);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setUseWideViewport(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(980, webViewImpl()->mainFrameImpl()->frameView()->contentsSize().width());
    EXPECT_EQ(980.0 / viewportWidth * viewportHeight, webViewImpl()->mainFrameImpl()->frameView()->contentsSize().height());
}

TEST_F(WebFrameTest, PageViewportInitialScaleOverridesLoadWithOverviewMode)
{
    registerMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "viewport-wide-2x-initial-scale.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setSupportDeprecatedTargetDensityDPI(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->settings()->setLoadWithOverviewMode(false);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));

    // The page must be displayed at 200% zoom, as specified in its viewport meta tag.
    EXPECT_EQ(2.0f, m_webView->pageScaleFactor());
}

TEST_F(WebFrameTest, setInitialPageScaleFactorPermanently)
{
    WebCore::Settings::setMockScrollbarsEnabled(true);
    WebCore::Settings::setUsesOverlayScrollbars(true);

    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    float enforcedPageScaleFactor = 2.0f;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client);
    m_webView->settings()->setSupportDeprecatedTargetDensityDPI(true);
    m_webView->settings()->setLoadWithOverviewMode(false);
    m_webView->setInitialPageScaleOverride(enforcedPageScaleFactor);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->layout();

    EXPECT_EQ(enforcedPageScaleFactor, m_webView->pageScaleFactor());

    int viewportWidth = 640;
    int viewportHeight = 480;
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->layout();

    EXPECT_EQ(enforcedPageScaleFactor, m_webView->pageScaleFactor());

    m_webView->setInitialPageScaleOverride(-1);
    m_webView->layout();
    EXPECT_EQ(1.0, m_webView->pageScaleFactor());
}

TEST_F(WebFrameTest, PermanentInitialPageScaleFactorOverridesLoadWithOverviewMode)
{
    registerMockedHttpURLLoad("viewport-auto-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;
    float enforcedPageScalePactor = 0.5f;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "viewport-auto-initial-scale.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setSupportDeprecatedTargetDensityDPI(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->settings()->setLoadWithOverviewMode(false);
    m_webView->setInitialPageScaleOverride(enforcedPageScalePactor);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(enforcedPageScalePactor, m_webView->pageScaleFactor());
}

TEST_F(WebFrameTest, PermanentInitialPageScaleFactorOverridesPageViewportInitialScale)
{
    registerMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;
    float enforcedPageScalePactor = 0.5f;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "viewport-wide-2x-initial-scale.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->setInitialPageScaleOverride(enforcedPageScalePactor);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(enforcedPageScalePactor, m_webView->pageScaleFactor());
}

TEST_F(WebFrameTest, WideViewportInitialScaleDoesNotExpandFixedLayoutWidth)
{
    registerMockedHttpURLLoad("viewport-device-0.5x-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "viewport-device-0.5x-initial-scale.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->settings()->setUseWideViewport(true);
    m_webView->settings()->setSupportDeprecatedTargetDensityDPI(true);
    m_webView->settings()->setViewportMetaLayoutSizeQuirk(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(m_webView);
    EXPECT_EQ(viewportWidth, webViewImpl->mainFrameImpl()->frameView()->fixedLayoutSize().width());
}

TEST_F(WebFrameTest, ZeroValuesQuirk)
{
    registerMockedHttpURLLoad("viewport-zero-values.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    m_webView = FrameTestHelpers::createWebView(true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->settings()->setSupportDeprecatedTargetDensityDPI(true);
    m_webView->settings()->setViewportMetaZeroValuesQuirk(true);
    FrameTestHelpers::loadFrame(m_webView->mainFrame(), m_baseURL + "viewport-zero-values.html");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    m_webView->resize(WebSize(viewportWidth, viewportHeight));

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(m_webView);
    EXPECT_EQ(viewportWidth, webViewImpl->mainFrameImpl()->frameView()->fixedLayoutSize().width());
    EXPECT_EQ(1.0f, m_webView->pageScaleFactor());

    m_webView->settings()->setUseWideViewport(true);
    m_webView->layout();
    EXPECT_EQ(viewportWidth, webViewImpl->mainFrameImpl()->frameView()->fixedLayoutSize().width());
    EXPECT_EQ(1.0f, m_webView->pageScaleFactor());
}

TEST_F(WebFrameTest, ScaleFactorShouldNotOscillate)
{
    registerMockedHttpURLLoad("scale_oscillate.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = static_cast<float>(1.325);
    int viewportWidth = 800;
    int viewportHeight = 1057;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "scale_oscillate.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->layout();
}

TEST_F(WebFrameTest, setPageScaleFactorDoesNotLayout)
{
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    // Small viewport to ensure there are always scrollbars.
    int viewportWidth = 64;
    int viewportHeight = 48;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->layout();

    int prevLayoutCount = webViewImpl()->mainFrameImpl()->frameView()->layoutCount();
    webViewImpl()->setPageScaleFactor(3, WebPoint());
    EXPECT_FALSE(webViewImpl()->mainFrameImpl()->frameView()->needsLayout());
    EXPECT_EQ(prevLayoutCount, webViewImpl()->mainFrameImpl()->frameView()->layoutCount());
}

TEST_F(WebFrameTest, setPageScaleFactorWithOverlayScrollbarsDoesNotLayout)
{
    WebCore::Settings::setMockScrollbarsEnabled(true);
    WebCore::Settings::setUsesOverlayScrollbars(true);
    EXPECT_TRUE(WebCore::ScrollbarTheme::theme()->usesOverlayScrollbars());

    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->layout();

    int prevLayoutCount = webViewImpl()->mainFrameImpl()->frameView()->layoutCount();
    webViewImpl()->setPageScaleFactor(30, WebPoint());
    EXPECT_FALSE(webViewImpl()->mainFrameImpl()->frameView()->needsLayout());
    EXPECT_EQ(prevLayoutCount, webViewImpl()->mainFrameImpl()->frameView()->layoutCount());

    WebCore::Settings::setMockScrollbarsEnabled(false);
    WebCore::Settings::setUsesOverlayScrollbars(false);
}

TEST_F(WebFrameTest, setPageScaleFactorBeforeFrameHasView)
{
    registerMockedHttpURLLoad("fixed_layout.html");

    float pageScaleFactor = 3;
    m_webView = FrameTestHelpers::createWebViewAndLoad("about:html", true, 0, 0);
    m_webView->setPageScaleFactor(pageScaleFactor, WebPoint());

    FrameTestHelpers::loadFrame(m_webView->mainFrame(), m_baseURL + "fixed_layout.html");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    WebCore::FrameView* view = webViewImpl()->mainFrameImpl()->frameView();
    EXPECT_EQ(pageScaleFactor, view->visibleContentScaleFactor());
}

TEST_F(WebFrameTest, pageScaleFactorWrittenToHistoryItem)
{
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->layout();

    m_webView->setPageScaleFactor(3, WebPoint());
    webViewImpl()->page()->mainFrame()->loader()->history()->saveDocumentAndScrollState();
    m_webView->setPageScaleFactor(1, WebPoint());
    webViewImpl()->page()->mainFrame()->loader()->history()->restoreScrollPositionAndViewState();
    EXPECT_EQ(3, m_webView->pageScaleFactor());
}

TEST_F(WebFrameTest, pageScaleFactorShrinksViewport)
{
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    // Small viewport to ensure there are always scrollbars.
    int viewportWidth = 64;
    int viewportHeight = 48;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->layout();

    WebCore::FrameView* view = webViewImpl()->mainFrameImpl()->frameView();
    int viewportWidthMinusScrollbar = viewportWidth - (view->verticalScrollbar()->isOverlayScrollbar() ? 0 : 15);
    int viewportHeightMinusScrollbar = viewportHeight - (view->horizontalScrollbar()->isOverlayScrollbar() ? 0 : 15);

    m_webView->setPageScaleFactor(2, WebPoint());

    WebCore::IntSize unscaledSize = view->unscaledVisibleContentSize(WebCore::ScrollableArea::IncludeScrollbars);
    EXPECT_EQ(viewportWidth, unscaledSize.width());
    EXPECT_EQ(viewportHeight, unscaledSize.height());

    WebCore::IntSize unscaledSizeMinusScrollbar = view->unscaledVisibleContentSize(WebCore::ScrollableArea::ExcludeScrollbars);
    EXPECT_EQ(viewportWidthMinusScrollbar, unscaledSizeMinusScrollbar.width());
    EXPECT_EQ(viewportHeightMinusScrollbar, unscaledSizeMinusScrollbar.height());

    WebCore::IntSize scaledSize = view->visibleContentRect().size();
    EXPECT_EQ(ceil(viewportWidthMinusScrollbar / 2.0), scaledSize.width());
    EXPECT_EQ(ceil(viewportHeightMinusScrollbar / 2.0), scaledSize.height());
}

TEST_F(WebFrameTest, pageScaleFactorDoesNotApplyCssTransform)
{
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->layout();

    m_webView->setPageScaleFactor(2, WebPoint());

    EXPECT_EQ(980, webViewImpl()->page()->mainFrame()->contentRenderer()->unscaledDocumentRect().width());
    EXPECT_EQ(980, webViewImpl()->mainFrameImpl()->frameView()->contentsSize().width());
}

TEST_F(WebFrameTest, targetDensityDpiHigh)
{
    WebCore::Settings::setMockScrollbarsEnabled(true);
    WebCore::Settings::setUsesOverlayScrollbars(true);
    registerMockedHttpURLLoad("viewport-target-densitydpi-high.html");

    FixedLayoutTestWebViewClient client;
    // high-dpi = 240
    float targetDpi = 240.0f;
    float deviceScaleFactors[] = { 1.0f, 4.0f / 3.0f, 2.0f };
    int viewportWidth = 640;
    int viewportHeight = 480;

    for (size_t i = 0; i < ARRAY_SIZE(deviceScaleFactors); ++i) {
        float deviceScaleFactor = deviceScaleFactors[i];
        float deviceDpi = deviceScaleFactor * 160.0f;
        client.m_screenInfo.deviceScaleFactor = deviceScaleFactor;

        m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "viewport-target-densitydpi-high.html", true, 0, &client);
        m_webView->enableFixedLayoutMode(true);
        m_webView->settings()->setViewportEnabled(true);
        m_webView->settings()->setSupportDeprecatedTargetDensityDPI(true);
        m_webView->resize(WebSize(viewportWidth, viewportHeight));

        // We need to account for the fact that logical pixels are unconditionally multiplied by deviceScaleFactor to produce
        // physical pixels.
        float densityDpiScaleRatio = deviceScaleFactor * targetDpi / deviceDpi;
        EXPECT_NEAR(viewportWidth * densityDpiScaleRatio, m_webView->fixedLayoutSize().width, 1.0f);
        EXPECT_NEAR(viewportHeight * densityDpiScaleRatio, m_webView->fixedLayoutSize().height, 1.0f);
        EXPECT_NEAR(1.0f / densityDpiScaleRatio, m_webView->pageScaleFactor(), 0.01f);

        m_webView->close();
        m_webView = 0;
    }
}

TEST_F(WebFrameTest, targetDensityDpiDevice)
{
    WebCore::Settings::setMockScrollbarsEnabled(true);
    WebCore::Settings::setUsesOverlayScrollbars(true);
    registerMockedHttpURLLoad("viewport-target-densitydpi-device.html");

    float deviceScaleFactors[] = { 1.0f, 4.0f / 3.0f, 2.0f };

    FixedLayoutTestWebViewClient client;
    int viewportWidth = 640;
    int viewportHeight = 480;

    for (size_t i = 0; i < ARRAY_SIZE(deviceScaleFactors); ++i) {
        client.m_screenInfo.deviceScaleFactor = deviceScaleFactors[i];

        m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "viewport-target-densitydpi-device.html", true, 0, &client);
        m_webView->enableFixedLayoutMode(true);
        m_webView->settings()->setViewportEnabled(true);
        m_webView->settings()->setSupportDeprecatedTargetDensityDPI(true);
        m_webView->resize(WebSize(viewportWidth, viewportHeight));

        EXPECT_NEAR(viewportWidth * client.m_screenInfo.deviceScaleFactor, m_webView->fixedLayoutSize().width, 1.0f);
        EXPECT_NEAR(viewportHeight * client.m_screenInfo.deviceScaleFactor, m_webView->fixedLayoutSize().height, 1.0f);
        EXPECT_NEAR(1.0f / client.m_screenInfo.deviceScaleFactor, m_webView->pageScaleFactor(), 0.01f);

        m_webView->close();
        m_webView = 0;
    }
}

class WebFrameResizeTest : public WebFrameTest {
protected:

    static WebCore::FloatSize computeRelativeOffset(const WebCore::IntPoint& absoluteOffset, const WebCore::LayoutRect& rect)
    {
        WebCore::FloatSize relativeOffset = WebCore::FloatPoint(absoluteOffset) - rect.location();
        relativeOffset.scale(1.f / rect.width(), 1.f / rect.height());
        return relativeOffset;
    }

    void testResizeYieldsCorrectScrollAndScale(const char* url,
                                               const float initialPageScaleFactor,
                                               const WebSize scrollOffset,
                                               const WebSize viewportSize,
                                               const bool shouldScaleRelativeToViewportWidth) {
        registerMockedHttpURLLoad(url);

        const float aspectRatio = static_cast<float>(viewportSize.width) / viewportSize.height;

        m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + url, true);
        m_webView->settings()->setViewportEnabled(true);
        m_webView->enableFixedLayoutMode(true);

        // Origin scrollOffsets preserved under resize.
        {
            webViewImpl()->resize(WebSize(viewportSize.width, viewportSize.height));
            webViewImpl()->setPageScaleFactor(initialPageScaleFactor, WebPoint());
            ASSERT_EQ(viewportSize, webViewImpl()->size());
            ASSERT_EQ(initialPageScaleFactor, webViewImpl()->pageScaleFactor());
            webViewImpl()->resize(WebSize(viewportSize.height, viewportSize.width));
            float expectedPageScaleFactor = initialPageScaleFactor * (shouldScaleRelativeToViewportWidth ? 1 / aspectRatio : 1);
            EXPECT_NEAR(expectedPageScaleFactor, webViewImpl()->pageScaleFactor(), 0.05f);
            EXPECT_EQ(WebSize(), webViewImpl()->mainFrame()->scrollOffset());
        }

        // Resizing just the height should not affect pageScaleFactor or scrollOffset.
        {
            webViewImpl()->resize(WebSize(viewportSize.width, viewportSize.height));
            webViewImpl()->setPageScaleFactor(initialPageScaleFactor, WebPoint(scrollOffset.width, scrollOffset.height));
            webViewImpl()->layout();
            const WebSize expectedScrollOffset = webViewImpl()->mainFrame()->scrollOffset();
            webViewImpl()->resize(WebSize(viewportSize.width, viewportSize.height * 0.8f));
            EXPECT_EQ(initialPageScaleFactor, webViewImpl()->pageScaleFactor());
            EXPECT_EQ(expectedScrollOffset, webViewImpl()->mainFrame()->scrollOffset());
            webViewImpl()->resize(WebSize(viewportSize.width, viewportSize.height * 0.8f));
            EXPECT_EQ(initialPageScaleFactor, webViewImpl()->pageScaleFactor());
            EXPECT_EQ(expectedScrollOffset, webViewImpl()->mainFrame()->scrollOffset());
        }

        // Generic resize preserves scrollOffset relative to anchor node located
        // the top center of the screen.
        {
            webViewImpl()->resize(WebSize(viewportSize.height, viewportSize.width));
            float pageScaleFactor = webViewImpl()->pageScaleFactor();
            webViewImpl()->resize(WebSize(viewportSize.width, viewportSize.height));
            float expectedPageScaleFactor = pageScaleFactor * (shouldScaleRelativeToViewportWidth ? aspectRatio : 1);
            EXPECT_NEAR(expectedPageScaleFactor, webViewImpl()->pageScaleFactor(), 0.05f);
            webViewImpl()->mainFrame()->setScrollOffset(scrollOffset);

            WebCore::IntPoint anchorPoint = WebCore::IntPoint(scrollOffset) + WebCore::IntPoint(viewportSize.width / 2, 0);
            RefPtr<WebCore::Node> anchorNode = webViewImpl()->mainFrameImpl()->frame()->eventHandler()->hitTestResultAtPoint(anchorPoint, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent).innerNode();
            ASSERT(anchorNode);

            pageScaleFactor = webViewImpl()->pageScaleFactor();
            const WebCore::FloatSize preResizeRelativeOffset
                = computeRelativeOffset(anchorPoint, anchorNode->boundingBox());
            webViewImpl()->resize(WebSize(viewportSize.height, viewportSize.width));
            WebCore::IntPoint newAnchorPoint = WebCore::IntPoint(webViewImpl()->mainFrame()->scrollOffset()) + WebCore::IntPoint(viewportSize.height / 2, 0);
            const WebCore::FloatSize postResizeRelativeOffset
                = computeRelativeOffset(newAnchorPoint, anchorNode->boundingBox());
            EXPECT_NEAR(preResizeRelativeOffset.width(), postResizeRelativeOffset.width(), 0.15f);
            expectedPageScaleFactor = pageScaleFactor * (shouldScaleRelativeToViewportWidth ? 1 / aspectRatio : 1);
            EXPECT_NEAR(expectedPageScaleFactor, webViewImpl()->pageScaleFactor(), 0.05f);
        }
    }
};

TEST_F(WebFrameResizeTest, ResizeYieldsCorrectScrollAndScaleForWidthEqualsDeviceWidth)
{
    // With width=device-width, pageScaleFactor is preserved across resizes as
    // long as the content adjusts according to the device-width.
    const char* url = "resize_scroll_mobile.html";
    const float initialPageScaleFactor = 1;
    const WebSize scrollOffset(0, 50);
    const WebSize viewportSize(120, 160);
    const bool shouldScaleRelativeToViewportWidth = true;

    testResizeYieldsCorrectScrollAndScale(
        url, initialPageScaleFactor, scrollOffset, viewportSize, shouldScaleRelativeToViewportWidth);
}

TEST_F(WebFrameResizeTest, ResizeYieldsCorrectScrollAndScaleForFixedWidth)
{
    // With a fixed width, pageScaleFactor scales by the relative change in viewport width.
    const char* url = "resize_scroll_fixed_width.html";
    const float initialPageScaleFactor = 2;
    const WebSize scrollOffset(0, 200);
    const WebSize viewportSize(240, 320);
    const bool shouldScaleRelativeToViewportWidth = true;

    testResizeYieldsCorrectScrollAndScale(
        url, initialPageScaleFactor, scrollOffset, viewportSize, shouldScaleRelativeToViewportWidth);
}

TEST_F(WebFrameResizeTest, ResizeYieldsCorrectScrollAndScaleForFixedLayout)
{
    // With a fixed layout, pageScaleFactor scales by the relative change in viewport width.
    const char* url = "resize_scroll_fixed_layout.html";
    const float initialPageScaleFactor = 2;
    const WebSize scrollOffset(200, 400);
    const WebSize viewportSize(320, 240);
    const bool shouldScaleRelativeToViewportWidth = true;

    testResizeYieldsCorrectScrollAndScale(
        url, initialPageScaleFactor, scrollOffset, viewportSize, shouldScaleRelativeToViewportWidth);
}

TEST_F(WebFrameTest, pageScaleFactorScalesPaintClip)
{
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 50;
    int viewportHeight = 50;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->layout();

    // Set <1 page scale so that the clip rect should be larger than
    // the viewport size as passed into resize().
    m_webView->setPageScaleFactor(0.5, WebPoint());

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 200, 200);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    WebCore::GraphicsContext context(&canvas);
    context.setTrackOpaqueRegion(true);

    EXPECT_EQ_RECT(WebCore::IntRect(0, 0, 0, 0), context.opaqueRegion().asRect());

    WebCore::FrameView* view = webViewImpl()->mainFrameImpl()->frameView();
    WebCore::IntRect paintRect(0, 0, 200, 200);
    view->paint(&context, paintRect);

    int viewportWidthMinusScrollbar = 50 - (view->verticalScrollbar()->isOverlayScrollbar() ? 0 : 15);
    int viewportHeightMinusScrollbar = 50 - (view->horizontalScrollbar()->isOverlayScrollbar() ? 0 : 15);
    WebCore::IntRect clippedRect(0, 0, viewportWidthMinusScrollbar * 2, viewportHeightMinusScrollbar * 2);
    EXPECT_EQ_RECT(clippedRect, context.opaqueRegion().asRect());
}

TEST_F(WebFrameTest, pageScaleFactorUpdatesScrollbars)
{
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->layout();

    WebCore::FrameView* view = webViewImpl()->mainFrameImpl()->frameView();
    EXPECT_EQ(view->scrollSize(WebCore::HorizontalScrollbar), view->contentsSize().width() - view->visibleContentRect().width());
    EXPECT_EQ(view->scrollSize(WebCore::VerticalScrollbar), view->contentsSize().height() - view->visibleContentRect().height());

    m_webView->setPageScaleFactor(10, WebPoint());

    EXPECT_EQ(view->scrollSize(WebCore::HorizontalScrollbar), view->contentsSize().width() - view->visibleContentRect().width());
    EXPECT_EQ(view->scrollSize(WebCore::VerticalScrollbar), view->contentsSize().height() - view->visibleContentRect().height());
}

TEST_F(WebFrameTest, CanOverrideScaleLimits)
{
    WebCore::Settings::setMockScrollbarsEnabled(true);
    WebCore::Settings::setUsesOverlayScrollbars(true);

    registerMockedHttpURLLoad("no_scale_for_you.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "no_scale_for_you.html", true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(2.0f, m_webView->minimumPageScaleFactor());
    EXPECT_EQ(2.0f, m_webView->maximumPageScaleFactor());

    m_webView->setIgnoreViewportTagScaleLimits(true);
    m_webView->layout();

    EXPECT_EQ(1.0f, m_webView->minimumPageScaleFactor());
    EXPECT_EQ(5.0f, m_webView->maximumPageScaleFactor());

    m_webView->setIgnoreViewportTagScaleLimits(false);
    m_webView->layout();

    EXPECT_EQ(2.0f, m_webView->minimumPageScaleFactor());
    EXPECT_EQ(2.0f, m_webView->maximumPageScaleFactor());
}

TEST_F(WebFrameTest, updateOverlayScrollbarLayers)
{
    WebCore::Settings::setMockScrollbarsEnabled(true);
    WebCore::Settings::setUsesOverlayScrollbars(true);
    EXPECT_TRUE(WebCore::ScrollbarTheme::theme()->usesOverlayScrollbars());

    registerMockedHttpURLLoad("large-div.html");

    int viewWidth = 500;
    int viewHeight = 500;

    createCompositingWebView();
    m_webView->resize(WebSize(viewWidth, viewHeight));
    FrameTestHelpers::loadFrame(m_webView->mainFrame(), m_baseURL + "large-div.html");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    m_webView->layout();

    WebCore::FrameView* view = webViewImpl()->mainFrameImpl()->frameView();
    EXPECT_TRUE(view->renderView()->compositor()->layerForHorizontalScrollbar());
    EXPECT_TRUE(view->renderView()->compositor()->layerForVerticalScrollbar());

    m_webView->resize(WebSize(viewWidth * 10, viewHeight * 10));
    m_webView->layout();
    EXPECT_FALSE(view->renderView()->compositor()->layerForHorizontalScrollbar());
    EXPECT_FALSE(view->renderView()->compositor()->layerForVerticalScrollbar());
}

void setScaleAndScrollAndLayout(WebKit::WebView* webView, WebPoint scroll, float scale)
{
    webView->setPageScaleFactor(scale, WebPoint(scroll.x, scroll.y));
    webView->layout();
}

TEST_F(WebFrameTest, DivAutoZoomParamsTest)
{
    registerMockedHttpURLLoad("get_scale_for_auto_zoom_into_div_test.html");

    const float deviceScaleFactor = 2.0f;
    int viewportWidth = 640 / deviceScaleFactor;
    int viewportHeight = 1280 / deviceScaleFactor;
    float doubleTapZoomAlreadyLegibleRatio = 1.2f;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "get_scale_for_auto_zoom_into_div_test.html"); //
    m_webView->setDeviceScaleFactor(deviceScaleFactor);
    m_webView->setPageScaleFactorLimits(0.01f, 4);
    m_webView->setPageScaleFactor(0.5f, WebPoint(0, 0));
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->enableFixedLayoutMode(true);
    m_webView->layout();

    WebRect wideDiv(200, 100, 400, 150);
    WebRect tallDiv(200, 300, 400, 800);
    WebRect doubleTapPointWide(wideDiv.x + 50, wideDiv.y + 50, touchPointPadding, touchPointPadding);
    WebRect doubleTapPointTall(tallDiv.x + 50, tallDiv.y + 50, touchPointPadding, touchPointPadding);
    WebRect wideBlockBounds;
    WebRect tallBlockBounds;
    float scale;
    WebPoint scroll;

    float doubleTapZoomAlreadyLegibleScale = webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;

    // Test double-tap zooming into wide div.
    wideBlockBounds = webViewImpl()->computeBlockBounds(doubleTapPointWide, false);
    webViewImpl()->computeScaleAndScrollForBlockRect(wideBlockBounds, touchPointPadding, doubleTapZoomAlreadyLegibleScale, scale, scroll);
    // The div should horizontally fill the screen (modulo margins), and
    // vertically centered (modulo integer rounding).
    EXPECT_NEAR(viewportWidth / (float) wideDiv.width, scale, 0.1);
    EXPECT_NEAR(wideDiv.x, scroll.x, 20);
    EXPECT_EQ(0, scroll.y);

    setScaleAndScrollAndLayout(webViewImpl(), scroll, scale);

    // Test zoom out back to minimum scale.
    wideBlockBounds = webViewImpl()->computeBlockBounds(doubleTapPointWide, false);
    webViewImpl()->computeScaleAndScrollForBlockRect(wideBlockBounds, touchPointPadding, doubleTapZoomAlreadyLegibleScale, scale, scroll);

    scale = webViewImpl()->minimumPageScaleFactor();
    setScaleAndScrollAndLayout(webViewImpl(), WebPoint(0, 0), scale);

    // Test double-tap zooming into tall div.
    tallBlockBounds = webViewImpl()->computeBlockBounds(doubleTapPointTall, false);
    webViewImpl()->computeScaleAndScrollForBlockRect(tallBlockBounds, touchPointPadding, doubleTapZoomAlreadyLegibleScale, scale, scroll);
    // The div should start at the top left of the viewport.
    EXPECT_NEAR(viewportWidth / (float) tallDiv.width, scale, 0.1);
    EXPECT_NEAR(tallDiv.x, scroll.x, 20);
    EXPECT_NEAR(tallDiv.y, scroll.y, 20);

    // Test for Non-doubletap scaling
    // Test zooming into div.
    webViewImpl()->computeScaleAndScrollForBlockRect(webViewImpl()->computeBlockBounds(WebRect(250, 250, 10, 10), true), 0, doubleTapZoomAlreadyLegibleScale, scale, scroll);
    EXPECT_NEAR(viewportWidth / (float) wideDiv.width, scale, 0.1);
}

void simulatePageScale(WebViewImpl* webViewImpl, float& scale)
{
    WebCore::IntSize scrollDelta = webViewImpl->fakePageScaleAnimationTargetPositionForTesting() - webViewImpl->mainFrameImpl()->frameView()->scrollPosition();
    float scaleDelta = webViewImpl->fakePageScaleAnimationPageScaleForTesting() / webViewImpl->pageScaleFactor();
    webViewImpl->applyScrollAndScale(scrollDelta, scaleDelta);
    scale = webViewImpl->pageScaleFactor();
}

void simulateMultiTargetZoom(WebViewImpl* webViewImpl, const WebRect& rect, float& scale)
{
    if (webViewImpl->zoomToMultipleTargetsRect(rect))
        simulatePageScale(webViewImpl, scale);
}

void simulateDoubleTap(WebViewImpl* webViewImpl, WebPoint& point, float& scale)
{
    webViewImpl->animateDoubleTapZoom(point);
    EXPECT_TRUE(webViewImpl->fakeDoubleTapAnimationPendingForTesting());
    simulatePageScale(webViewImpl, scale);
}

TEST_F(WebFrameTest, DivAutoZoomWideDivTest)
{
    registerMockedHttpURLLoad("get_wide_div_for_auto_zoom_test.html");

    const float deviceScaleFactor = 2.0f;
    int viewportWidth = 640 / deviceScaleFactor;
    int viewportHeight = 1280 / deviceScaleFactor;
    float doubleTapZoomAlreadyLegibleRatio = 1.2f;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "get_wide_div_for_auto_zoom_test.html");
    m_webView->enableFixedLayoutMode(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->setPageScaleFactorLimits(1.0f, 4);
    m_webView->setDeviceScaleFactor(deviceScaleFactor);
    m_webView->setPageScaleFactor(1.0f, WebPoint(0, 0));
    m_webView->layout();

    webViewImpl()->enableFakePageScaleAnimationForTesting(true);

    float doubleTapZoomAlreadyLegibleScale = webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;

    WebRect div(0, 100, viewportWidth, 150);
    WebPoint point(div.x + 50, div.y + 50);
    float scale;
    setScaleAndScrollAndLayout(webViewImpl(), WebPoint(0, 0), (webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);

    simulateDoubleTap(webViewImpl(), point, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
    simulateDoubleTap(webViewImpl(), point, scale);
    EXPECT_FLOAT_EQ(webViewImpl()->minimumPageScaleFactor(), scale);
}

TEST_F(WebFrameTest, DivAutoZoomMultipleDivsTest)
{
    registerMockedHttpURLLoad("get_multiple_divs_for_auto_zoom_test.html");

    const float deviceScaleFactor = 2.0f;
    int viewportWidth = 640 / deviceScaleFactor;
    int viewportHeight = 1280 / deviceScaleFactor;
    float doubleTapZoomAlreadyLegibleRatio = 1.2f;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "get_multiple_divs_for_auto_zoom_test.html");
    m_webView->enableFixedLayoutMode(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->setPageScaleFactorLimits(0.5f, 4);
    m_webView->setDeviceScaleFactor(deviceScaleFactor);
    m_webView->setPageScaleFactor(0.5f, WebPoint(0, 0));
    m_webView->layout();

    webViewImpl()->enableFakePageScaleAnimationForTesting(true);

    WebRect topDiv(200, 100, 200, 150);
    WebRect bottomDiv(200, 300, 200, 150);
    WebPoint topPoint(topDiv.x + 50, topDiv.y + 50);
    WebPoint bottomPoint(bottomDiv.x + 50, bottomDiv.y + 50);
    float scale;
    setScaleAndScrollAndLayout(webViewImpl(), WebPoint(0, 0), (webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);

    // Test double tap on two different divs
    // After first zoom, we should go back to minimum page scale with a second double tap.
    simulateDoubleTap(webViewImpl(), topPoint, scale);
    EXPECT_FLOAT_EQ(1, scale);
    simulateDoubleTap(webViewImpl(), bottomPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl()->minimumPageScaleFactor(), scale);

    // If the user pinch zooms after double tap, a second double tap should zoom back to the div.
    simulateDoubleTap(webViewImpl(), topPoint, scale);
    EXPECT_FLOAT_EQ(1, scale);
    webViewImpl()->applyScrollAndScale(WebSize(), 0.6f);
    simulateDoubleTap(webViewImpl(), bottomPoint, scale);
    EXPECT_FLOAT_EQ(1, scale);
    simulateDoubleTap(webViewImpl(), bottomPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl()->minimumPageScaleFactor(), scale);

    // If we didn't yet get an auto-zoom update and a second double-tap arrives, should go back to minimum scale.
    webViewImpl()->applyScrollAndScale(WebSize(), 1.1f);
    webViewImpl()->animateDoubleTapZoom(topPoint);
    EXPECT_TRUE(webViewImpl()->fakeDoubleTapAnimationPendingForTesting());
    simulateDoubleTap(webViewImpl(), bottomPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl()->minimumPageScaleFactor(), scale);
}

TEST_F(WebFrameTest, DivAutoZoomScaleBoundsTest)
{
    registerMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

    int viewportWidth = 320;
    int viewportHeight = 480;
    float doubleTapZoomAlreadyLegibleRatio = 1.2f;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "get_scale_bounds_check_for_auto_zoom_test.html");
    m_webView->enableFixedLayoutMode(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->setDeviceScaleFactor(1.5f);
    m_webView->layout();

    webViewImpl()->enableFakePageScaleAnimationForTesting(true);

    WebRect div(200, 100, 200, 150);
    WebPoint doubleTapPoint(div.x + 50, div.y + 50);
    float scale;

    // Test double tap scale bounds.
    // minimumPageScale < doubleTapZoomAlreadyLegibleScale < 1
    m_webView->setPageScaleFactorLimits(0.5f, 4);
    m_webView->layout();
    float doubleTapZoomAlreadyLegibleScale = webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;
    setScaleAndScrollAndLayout(webViewImpl(), WebPoint(0, 0), (webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(1, scale);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl()->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(1, scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewImpl()->applyScrollAndScale(WebSize(), 1.1f);
    // 1 < minimumPageScale < doubleTapZoomAlreadyLegibleScale
    m_webView->setPageScaleFactorLimits(1.1f, 4);
    m_webView->layout();
    doubleTapZoomAlreadyLegibleScale = webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;
    setScaleAndScrollAndLayout(webViewImpl(), WebPoint(0, 0), (webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl()->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewImpl()->applyScrollAndScale(WebSize(), 1.1f);
    // minimumPageScale < 1 < doubleTapZoomAlreadyLegibleScale
    m_webView->setPageScaleFactorLimits(0.95f, 4);
    m_webView->layout();
    doubleTapZoomAlreadyLegibleScale = webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;
    setScaleAndScrollAndLayout(webViewImpl(), WebPoint(0, 0), (webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl()->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
}

TEST_F(WebFrameTest, DivAutoZoomScaleFontScaleFactorTest)
{
    registerMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

    int viewportWidth = 320;
    int viewportHeight = 480;
    float doubleTapZoomAlreadyLegibleRatio = 1.2f;
    float textAutosizingFontScaleFactor = 1.13f;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "get_scale_bounds_check_for_auto_zoom_test.html");
    m_webView->enableFixedLayoutMode(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->layout();

    webViewImpl()->enableFakePageScaleAnimationForTesting(true);
    webViewImpl()->page()->settings().setTextAutosizingEnabled(true);
    webViewImpl()->page()->settings().setTextAutosizingFontScaleFactor(textAutosizingFontScaleFactor);

    WebRect div(200, 100, 200, 150);
    WebPoint doubleTapPoint(div.x + 50, div.y + 50);
    float scale;

    // Test double tap scale bounds.
    // minimumPageScale < doubleTapZoomAlreadyLegibleScale < 1 < textAutosizingFontScaleFactor
    float legibleScale = textAutosizingFontScaleFactor;
    setScaleAndScrollAndLayout(webViewImpl(), WebPoint(0, 0), (webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    float doubleTapZoomAlreadyLegibleScale = webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;
    m_webView->setPageScaleFactorLimits(0.5f, 4);
    m_webView->layout();
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(legibleScale, scale);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl()->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(legibleScale, scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewImpl()->applyScrollAndScale(WebSize(), 1.1f);
    // 1 < textAutosizingFontScaleFactor < minimumPageScale < doubleTapZoomAlreadyLegibleScale
    m_webView->setPageScaleFactorLimits(1.0f, 4);
    m_webView->layout();
    doubleTapZoomAlreadyLegibleScale = webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;
    setScaleAndScrollAndLayout(webViewImpl(), WebPoint(0, 0), (webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl()->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewImpl()->applyScrollAndScale(WebSize(), 1.1f);
    // minimumPageScale < 1 < textAutosizingFontScaleFactor < doubleTapZoomAlreadyLegibleScale
    m_webView->setPageScaleFactorLimits(0.95f, 4);
    m_webView->layout();
    doubleTapZoomAlreadyLegibleScale = webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;
    setScaleAndScrollAndLayout(webViewImpl(), WebPoint(0, 0), (webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl()->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewImpl()->applyScrollAndScale(WebSize(), 1.1f);
    // minimumPageScale < 1 < doubleTapZoomAlreadyLegibleScale < textAutosizingFontScaleFactor
    m_webView->setPageScaleFactorLimits(0.9f, 4);
    m_webView->layout();
    doubleTapZoomAlreadyLegibleScale = webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;
    setScaleAndScrollAndLayout(webViewImpl(), WebPoint(0, 0), (webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(legibleScale, scale);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl()->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(legibleScale, scale);
}

TEST_F(WebFrameTest, DivMultipleTargetZoomMultipleDivsTest)
{
    registerMockedHttpURLLoad("get_multiple_divs_for_auto_zoom_test.html");

    const float deviceScaleFactor = 2.0f;
    int viewportWidth = 640 / deviceScaleFactor;
    int viewportHeight = 1280 / deviceScaleFactor;
    float doubleTapZoomAlreadyLegibleRatio = 1.2f;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "get_multiple_divs_for_auto_zoom_test.html");
    m_webView->enableFixedLayoutMode(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->setPageScaleFactorLimits(0.5f, 4);
    m_webView->setDeviceScaleFactor(deviceScaleFactor);
    m_webView->setPageScaleFactor(0.5f, WebPoint(0, 0));
    m_webView->layout();

    webViewImpl()->enableFakePageScaleAnimationForTesting(true);

    WebRect viewportRect(0, 0, viewportWidth, viewportHeight);
    WebRect topDiv(200, 100, 200, 150);
    WebRect bottomDiv(200, 300, 200, 150);
    float scale;
    setScaleAndScrollAndLayout(webViewImpl(), WebPoint(0, 0), (webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);

    simulateMultiTargetZoom(webViewImpl(), topDiv, scale);
    EXPECT_FLOAT_EQ(1, scale);
    simulateMultiTargetZoom(webViewImpl(), bottomDiv, scale);
    EXPECT_FLOAT_EQ(1, scale);
    simulateMultiTargetZoom(webViewImpl(), viewportRect, scale);
    EXPECT_FLOAT_EQ(1, scale);
    webViewImpl()->setPageScaleFactor(webViewImpl()->minimumPageScaleFactor(), WebPoint(0, 0));
    simulateMultiTargetZoom(webViewImpl(), topDiv, scale);
    EXPECT_FLOAT_EQ(1, scale);
}

TEST_F(WebFrameTest, DivScrollIntoEditableTest)
{
    registerMockedHttpURLLoad("get_scale_for_zoom_into_editable_test.html");

    int viewportWidth = 450;
    int viewportHeight = 300;
    float leftBoxRatio = 0.3f;
    int caretPadding = 10;
    float minReadableCaretHeight = 18.0f;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "get_scale_for_zoom_into_editable_test.html");
    m_webView->enableFixedLayoutMode(true);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->setPageScaleFactorLimits(1, 4);
    m_webView->layout();
    m_webView->setDeviceScaleFactor(1.5f);
    m_webView->settings()->setAutoZoomFocusedNodeToLegibleScale(true);

    webViewImpl()->enableFakePageScaleAnimationForTesting(true);

    WebRect editBoxWithText(200, 200, 250, 20);
    WebRect editBoxWithNoText(200, 250, 250, 20);

    // Test scrolling the focused node
    // The edit box is shorter and narrower than the viewport when legible.
    m_webView->advanceFocus(false);
    // Set the caret to the end of the input box.
    m_webView->mainFrame()->document().getElementById("EditBoxWithText").to<WebInputElement>().setSelectionRange(1000, 1000);
    setScaleAndScrollAndLayout(m_webView, WebPoint(0, 0), 1);
    WebRect rect, caret;
    webViewImpl()->selectionBounds(caret, rect);

    float scale;
    WebCore::IntPoint scroll;
    bool needAnimation;
    webViewImpl()->computeScaleAndScrollForFocusedNode(webViewImpl()->focusedElement(), scale, scroll, needAnimation);
    EXPECT_TRUE(needAnimation);
    // The edit box should be left aligned with a margin for possible label.
    int hScroll = editBoxWithText.x - leftBoxRatio * viewportWidth / scale;
    EXPECT_NEAR(hScroll, scroll.x(), 1);
    int vScroll = editBoxWithText.y - (viewportHeight / scale - editBoxWithText.height) / 2;
    EXPECT_NEAR(vScroll, scroll.y(), 1);
    EXPECT_NEAR(minReadableCaretHeight / caret.height, scale, 0.1);

    // The edit box is wider than the viewport when legible.
    viewportWidth = 200;
    viewportHeight = 150;
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    setScaleAndScrollAndLayout(m_webView, WebPoint(0, 0), 1);
    webViewImpl()->selectionBounds(caret, rect);
    webViewImpl()->computeScaleAndScrollForFocusedNode(webViewImpl()->focusedElement(), scale, scroll, needAnimation);
    EXPECT_TRUE(needAnimation);
    // The caret should be right aligned since the caret would be offscreen when the edit box is left aligned.
    hScroll = caret.x + caret.width + caretPadding - viewportWidth / scale;
    EXPECT_NEAR(hScroll, scroll.x(), 1);
    EXPECT_NEAR(minReadableCaretHeight / caret.height, scale, 0.1);

    setScaleAndScrollAndLayout(m_webView, WebPoint(0, 0), 1);
    // Move focus to edit box with text.
    m_webView->advanceFocus(false);
    webViewImpl()->selectionBounds(caret, rect);
    webViewImpl()->computeScaleAndScrollForFocusedNode(webViewImpl()->focusedElement(), scale, scroll, needAnimation);
    EXPECT_TRUE(needAnimation);
    // The edit box should be left aligned.
    hScroll = editBoxWithNoText.x;
    EXPECT_NEAR(hScroll, scroll.x(), 1);
    vScroll = editBoxWithNoText.y - (viewportHeight / scale - editBoxWithNoText.height) / 2;
    EXPECT_NEAR(vScroll, scroll.y(), 1);
    EXPECT_NEAR(minReadableCaretHeight / caret.height, scale, 0.1);

    setScaleAndScrollAndLayout(webViewImpl(), scroll, scale);

    // Move focus back to the first edit box.
    m_webView->advanceFocus(true);
    webViewImpl()->computeScaleAndScrollForFocusedNode(webViewImpl()->focusedElement(), scale, scroll, needAnimation);
    // The position should have stayed the same since this box was already on screen with the right scale.
    EXPECT_FALSE(needAnimation);
}

class TestReloadDoesntRedirectWebFrameClient : public WebFrameClient {
public:
    virtual WebNavigationPolicy decidePolicyForNavigation(
        WebFrame*, WebDataSource::ExtraData*, const WebURLRequest&, WebNavigationType,
        WebNavigationPolicy defaultPolicy, bool isRedirect) OVERRIDE
    {
        EXPECT_FALSE(isRedirect);
        return WebNavigationPolicyCurrentTab;
    }
};

TEST_F(WebFrameTest, ReloadDoesntSetRedirect)
{
    // Test for case in http://crbug.com/73104. Reloading a frame very quickly
    // would sometimes call decidePolicyForNavigation with isRedirect=true
    registerMockedHttpURLLoad("form.html");

    TestReloadDoesntRedirectWebFrameClient webFrameClient;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "form.html", false, &webFrameClient);

    m_webView->mainFrame()->reload(true);
    // start reload before request is delivered.
    m_webView->mainFrame()->reload(true);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, ReloadWithOverrideURLPreservesState)
{
    const std::string firstURL = "find.html";
    const std::string secondURL = "form.html";
    const std::string thirdURL = "history.html";
    const float pageScaleFactor = 1.1684f;
    const int pageWidth = 640;
    const int pageHeight = 480;

    registerMockedHttpURLLoad(firstURL);
    registerMockedHttpURLLoad(secondURL);
    registerMockedHttpURLLoad(thirdURL);

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + firstURL, true);
    webViewImpl()->resize(WebSize(pageWidth, pageHeight));
    webViewImpl()->mainFrame()->setScrollOffset(WebSize(pageWidth / 4, pageHeight / 4));
    webViewImpl()->setPageScaleFactorPreservingScrollOffset(pageScaleFactor);

    WebSize previousOffset = webViewImpl()->mainFrame()->scrollOffset();
    float previousScale = webViewImpl()->pageScaleFactor();

    // Reload the page using the cache.
    webViewImpl()->mainFrame()->reloadWithOverrideURL(toKURL(m_baseURL + secondURL), false);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    ASSERT_EQ(previousOffset, webViewImpl()->mainFrame()->scrollOffset());
    ASSERT_EQ(previousScale, webViewImpl()->pageScaleFactor());

    // Reload the page while ignoring the cache.
    webViewImpl()->mainFrame()->reloadWithOverrideURL(toKURL(m_baseURL + thirdURL), true);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    ASSERT_EQ(previousOffset, webViewImpl()->mainFrame()->scrollOffset());
    ASSERT_EQ(previousScale, webViewImpl()->pageScaleFactor());
}

class TestReloadWhileProvisionalFrameClient : public WebFrameClient {
};

TEST_F(WebFrameTest, ReloadWhileProvisional)
{
    // Test that reloading while the previous load is still pending does not cause the initial
    // request to get lost.
    registerMockedHttpURLLoad("fixed_layout.html");

    TestReloadWhileProvisionalFrameClient webFrameClient;
    m_webView = FrameTestHelpers::createWebView(false, &webFrameClient);
    FrameTestHelpers::loadFrame(m_webView->mainFrame(), m_baseURL + "fixed_layout.html");
    // start reload before first request is delivered.
    m_webView->mainFrame()->reload(true);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    ASSERT_EQ(WebURL(toKURL(m_baseURL + "fixed_layout.html")),
        m_webView->mainFrame()->dataSource()->request().url());

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, IframeRedirect)
{
    registerMockedHttpURLLoad("iframe_redirect.html");
    registerMockedHttpURLLoad("visible_iframe.html");

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "iframe_redirect.html", true);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests(); // Load the iframe.

    WebFrame* iframe = m_webView->findFrameByName(WebString::fromUTF8("ifr"));
    ASSERT_TRUE(iframe);
    WebDataSource* iframeDataSource = iframe->dataSource();
    ASSERT_TRUE(iframeDataSource);
    WebVector<WebURL> redirects;
    iframeDataSource->redirectChain(redirects);
    ASSERT_EQ(2U, redirects.size());
    EXPECT_EQ(toKURL("about:blank"), toKURL(redirects[0].spec().data()));
    EXPECT_EQ(toKURL("http://www.test.com/visible_iframe.html"), toKURL(redirects[1].spec().data()));
}

TEST_F(WebFrameTest, ClearFocusedNodeTest)
{
    registerMockedHttpURLLoad("iframe_clear_focused_node_test.html");
    registerMockedHttpURLLoad("autofocus_input_field_iframe.html");

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "iframe_clear_focused_node_test.html", true);

    // Clear the focused node.
    m_webView->clearFocusedNode();

    // Now retrieve the FocusedNode and test it should be null.
    EXPECT_EQ(0, webViewImpl()->focusedElement());
}

// Implementation of WebFrameClient that tracks the v8 contexts that are created
// and destroyed for verification.
class ContextLifetimeTestWebFrameClient : public WebFrameClient {
public:
    struct Notification {
    public:
        Notification(WebFrame* frame, v8::Handle<v8::Context> context, int worldId)
            : frame(frame)
            , context(context->GetIsolate(), context)
            , worldId(worldId)
        {
        }

        ~Notification()
        {
            context.Dispose();
        }

        bool Equals(Notification* other)
        {
            return other && frame == other->frame && context == other->context && worldId == other->worldId;
        }

        WebFrame* frame;
        v8::Persistent<v8::Context> context;
        int worldId;
    };

    virtual ~ContextLifetimeTestWebFrameClient()
    {
        reset();
    }

    void reset()
    {
        for (size_t i = 0; i < createNotifications.size(); ++i)
            delete createNotifications[i];

        for (size_t i = 0; i < releaseNotifications.size(); ++i)
            delete releaseNotifications[i];

        createNotifications.clear();
        releaseNotifications.clear();
    }

    std::vector<Notification*> createNotifications;
    std::vector<Notification*> releaseNotifications;

 private:
    virtual void didCreateScriptContext(WebFrame* frame, v8::Handle<v8::Context> context, int extensionGroup, int worldId) OVERRIDE
    {
        createNotifications.push_back(new Notification(frame, context, worldId));
    }

    virtual void willReleaseScriptContext(WebFrame* frame, v8::Handle<v8::Context> context, int worldId) OVERRIDE
    {
        releaseNotifications.push_back(new Notification(frame, context, worldId));
    }
};

// TODO(aa): Deflake this test.
TEST_F(WebFrameTest, FLAKY_ContextNotificationsLoadUnload)
{
    v8::HandleScope handleScope(v8::Isolate::GetCurrent());

    registerMockedHttpURLLoad("context_notifications_test.html");
    registerMockedHttpURLLoad("context_notifications_test_frame.html");

    // Load a frame with an iframe, make sure we get the right create notifications.
    ContextLifetimeTestWebFrameClient webFrameClient;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "context_notifications_test.html", true, &webFrameClient);

    WebFrame* mainFrame = m_webView->mainFrame();
    WebFrame* childFrame = mainFrame->firstChild();

    ASSERT_EQ(2u, webFrameClient.createNotifications.size());
    EXPECT_EQ(0u, webFrameClient.releaseNotifications.size());

    ContextLifetimeTestWebFrameClient::Notification* firstCreateNotification = webFrameClient.createNotifications[0];
    ContextLifetimeTestWebFrameClient::Notification* secondCreateNotification = webFrameClient.createNotifications[1];

    EXPECT_EQ(mainFrame, firstCreateNotification->frame);
    EXPECT_EQ(mainFrame->mainWorldScriptContext(), firstCreateNotification->context);
    EXPECT_EQ(0, firstCreateNotification->worldId);

    EXPECT_EQ(childFrame, secondCreateNotification->frame);
    EXPECT_EQ(childFrame->mainWorldScriptContext(), secondCreateNotification->context);
    EXPECT_EQ(0, secondCreateNotification->worldId);

    // Close the view. We should get two release notifications that are exactly the same as the create ones, in reverse order.
    m_webView->close();
    m_webView = 0;

    ASSERT_EQ(2u, webFrameClient.releaseNotifications.size());
    ContextLifetimeTestWebFrameClient::Notification* firstReleaseNotification = webFrameClient.releaseNotifications[0];
    ContextLifetimeTestWebFrameClient::Notification* secondReleaseNotification = webFrameClient.releaseNotifications[1];

    ASSERT_TRUE(firstCreateNotification->Equals(secondReleaseNotification));
    ASSERT_TRUE(secondCreateNotification->Equals(firstReleaseNotification));
}

TEST_F(WebFrameTest, ContextNotificationsReload)
{
    v8::HandleScope handleScope(v8::Isolate::GetCurrent());

    registerMockedHttpURLLoad("context_notifications_test.html");
    registerMockedHttpURLLoad("context_notifications_test_frame.html");

    ContextLifetimeTestWebFrameClient webFrameClient;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "context_notifications_test.html", true, &webFrameClient);

    // Refresh, we should get two release notifications and two more create notifications.
    m_webView->mainFrame()->reload(false);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    ASSERT_EQ(4u, webFrameClient.createNotifications.size());
    ASSERT_EQ(2u, webFrameClient.releaseNotifications.size());

    // The two release notifications we got should be exactly the same as the first two create notifications.
    for (size_t i = 0; i < webFrameClient.releaseNotifications.size(); ++i) {
      EXPECT_TRUE(webFrameClient.releaseNotifications[i]->Equals(
          webFrameClient.createNotifications[webFrameClient.createNotifications.size() - 3 - i]));
    }

    // The last two create notifications should be for the current frames and context.
    WebFrame* mainFrame = m_webView->mainFrame();
    WebFrame* childFrame = mainFrame->firstChild();
    ContextLifetimeTestWebFrameClient::Notification* firstRefreshNotification = webFrameClient.createNotifications[2];
    ContextLifetimeTestWebFrameClient::Notification* secondRefreshNotification = webFrameClient.createNotifications[3];

    EXPECT_EQ(mainFrame, firstRefreshNotification->frame);
    EXPECT_EQ(mainFrame->mainWorldScriptContext(), firstRefreshNotification->context);
    EXPECT_EQ(0, firstRefreshNotification->worldId);

    EXPECT_EQ(childFrame, secondRefreshNotification->frame);
    EXPECT_EQ(childFrame->mainWorldScriptContext(), secondRefreshNotification->context);
    EXPECT_EQ(0, secondRefreshNotification->worldId);

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, ContextNotificationsIsolatedWorlds)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);

    registerMockedHttpURLLoad("context_notifications_test.html");
    registerMockedHttpURLLoad("context_notifications_test_frame.html");

    ContextLifetimeTestWebFrameClient webFrameClient;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "context_notifications_test.html", true, &webFrameClient);

    // Add an isolated world.
    webFrameClient.reset();

    int isolatedWorldId = 42;
    WebScriptSource scriptSource("hi!");
    int numSources = 1;
    int extensionGroup = 0;
    m_webView->mainFrame()->executeScriptInIsolatedWorld(isolatedWorldId, &scriptSource, numSources, extensionGroup);

    // We should now have a new create notification.
    ASSERT_EQ(1u, webFrameClient.createNotifications.size());
    ContextLifetimeTestWebFrameClient::Notification* notification = webFrameClient.createNotifications[0];
    ASSERT_EQ(isolatedWorldId, notification->worldId);
    ASSERT_EQ(m_webView->mainFrame(), notification->frame);

    // We don't have an API to enumarate isolated worlds for a frame, but we can at least assert that the context we got is *not* the main world's context.
    ASSERT_NE(m_webView->mainFrame()->mainWorldScriptContext(), v8::Local<v8::Context>::New(isolate, notification->context));

    m_webView->close();
    m_webView = 0;

    // We should have gotten three release notifications (one for each of the frames, plus one for the isolated context).
    ASSERT_EQ(3u, webFrameClient.releaseNotifications.size());

    // And one of them should be exactly the same as the create notification for the isolated context.
    int matchCount = 0;
    for (size_t i = 0; i < webFrameClient.releaseNotifications.size(); ++i) {
      if (webFrameClient.releaseNotifications[i]->Equals(webFrameClient.createNotifications[0]))
        ++matchCount;
    }
    EXPECT_EQ(1, matchCount);
}

TEST_F(WebFrameTest, FindInPage)
{
    registerMockedHttpURLLoad("find.html");
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "find.html");
    WebFrame* frame = m_webView->mainFrame();
    const int findIdentifier = 12345;
    WebFindOptions options;

    // Find in a <div> element.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar1"), options, false, 0));
    frame->stopFinding(false);
    WebRange range = frame->selectionRange();
    EXPECT_EQ(5, range.startOffset());
    EXPECT_EQ(9, range.endOffset());
    EXPECT_TRUE(frame->document().focusedNode().isNull());

    // Find in an <input> value.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar2"), options, false, 0));
    // Confirm stopFinding(false) sets the selection on the found text.
    frame->stopFinding(false);
    range = frame->selectionRange();
    ASSERT_FALSE(range.isNull());
    EXPECT_EQ(5, range.startOffset());
    EXPECT_EQ(9, range.endOffset());
    EXPECT_EQ(WebString::fromUTF8("INPUT"), frame->document().focusedNode().nodeName());

    // Find in a <textarea> content.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar3"), options, false, 0));
    // Confirm stopFinding(false) sets the selection on the found text.
    frame->stopFinding(false);
    range = frame->selectionRange();
    ASSERT_FALSE(range.isNull());
    EXPECT_EQ(5, range.startOffset());
    EXPECT_EQ(9, range.endOffset());
    EXPECT_EQ(WebString::fromUTF8("TEXTAREA"), frame->document().focusedNode().nodeName());

    // Find in a contentEditable element.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar4"), options, false, 0));
    // Confirm stopFinding(false) sets the selection on the found text.
    frame->stopFinding(false);
    range = frame->selectionRange();
    ASSERT_FALSE(range.isNull());
    EXPECT_EQ(0, range.startOffset());
    EXPECT_EQ(4, range.endOffset());
    // "bar4" is surrounded by <span>, but the focusable node should be the parent <div>.
    EXPECT_EQ(WebString::fromUTF8("DIV"), frame->document().focusedNode().nodeName());

    // Find in <select> content.
    EXPECT_FALSE(frame->find(findIdentifier, WebString::fromUTF8("bar5"), options, false, 0));
    // If there are any matches, stopFinding will set the selection on the found text.
    // However, we do not expect any matches, so check that the selection is null.
    frame->stopFinding(false);
    range = frame->selectionRange();
    ASSERT_TRUE(range.isNull());
}

TEST_F(WebFrameTest, GetContentAsPlainText)
{
    m_webView = FrameTestHelpers::createWebViewAndLoad("about:blank", true);
    // We set the size because it impacts line wrapping, which changes the
    // resulting text value.
    m_webView->resize(WebSize(640, 480));
    WebFrame* frame = m_webView->mainFrame();

    // Generate a simple test case.
    const char simpleSource[] = "<div>Foo bar</div><div></div>baz";
    WebCore::KURL testURL = toKURL("about:blank");
    frame->loadHTMLString(simpleSource, testURL);
    runPendingTasks();

    // Make sure it comes out OK.
    const std::string expected("Foo bar\nbaz");
    WebString text = frame->contentAsText(std::numeric_limits<size_t>::max());
    EXPECT_EQ(expected, std::string(text.utf8().data()));

    // Try reading the same one with clipping of the text.
    const int length = 5;
    text = frame->contentAsText(length);
    EXPECT_EQ(expected.substr(0, length), std::string(text.utf8().data()));

    // Now do a new test with a subframe.
    const char outerFrameSource[] = "Hello<iframe></iframe> world";
    frame->loadHTMLString(outerFrameSource, testURL);
    runPendingTasks();

    // Load something into the subframe.
    WebFrame* subframe = frame->findChildByExpression(WebString::fromUTF8("/html/body/iframe"));
    ASSERT_TRUE(subframe);
    subframe->loadHTMLString("sub<p>text", testURL);
    runPendingTasks();

    text = frame->contentAsText(std::numeric_limits<size_t>::max());
    EXPECT_EQ("Hello world\n\nsub\ntext", std::string(text.utf8().data()));

    // Get the frame text where the subframe separator falls on the boundary of
    // what we'll take. There used to be a crash in this case.
    text = frame->contentAsText(12);
    EXPECT_EQ("Hello world", std::string(text.utf8().data()));
}

TEST_F(WebFrameTest, GetFullHtmlOfPage)
{
    m_webView = FrameTestHelpers::createWebViewAndLoad("about:blank", true);
    WebFrame* frame = m_webView->mainFrame();

    // Generate a simple test case.
    const char simpleSource[] = "<p>Hello</p><p>World</p>";
    WebCore::KURL testURL = toKURL("about:blank");
    frame->loadHTMLString(simpleSource, testURL);
    runPendingTasks();

    WebString text = frame->contentAsText(std::numeric_limits<size_t>::max());
    EXPECT_EQ("Hello\n\nWorld", std::string(text.utf8().data()));

    const std::string html = std::string(frame->contentAsMarkup().utf8().data());

    // Load again with the output html.
    frame->loadHTMLString(WebData(html.c_str(), html.length()), testURL);
    runPendingTasks();

    EXPECT_EQ(html, std::string(frame->contentAsMarkup().utf8().data()));

    text = frame->contentAsText(std::numeric_limits<size_t>::max());
    EXPECT_EQ("Hello\n\nWorld", std::string(text.utf8().data()));

    // Test selection check
    EXPECT_FALSE(frame->hasSelection());
    frame->executeCommand(WebString::fromUTF8("SelectAll"));
    EXPECT_TRUE(frame->hasSelection());
    frame->executeCommand(WebString::fromUTF8("Unselect"));
    EXPECT_FALSE(frame->hasSelection());
    WebString selectionHtml = frame->selectionAsMarkup();
    EXPECT_TRUE(selectionHtml.isEmpty());
}

class TestExecuteScriptDuringDidCreateScriptContext : public WebFrameClient {
public:
    virtual void didCreateScriptContext(WebFrame* frame, v8::Handle<v8::Context> context, int extensionGroup, int worldId) OVERRIDE
    {
        frame->executeScript(WebScriptSource("window.history = 'replaced';"));
    }
};

TEST_F(WebFrameTest, ExecuteScriptDuringDidCreateScriptContext)
{
    registerMockedHttpURLLoad("hello_world.html");

    TestExecuteScriptDuringDidCreateScriptContext webFrameClient;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "hello_world.html", true, &webFrameClient);

    m_webView->mainFrame()->reload();
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();

    m_webView->close();
    m_webView = 0;
}

class TestDidCreateFrameWebFrameClient : public WebFrameClient {
public:
    TestDidCreateFrameWebFrameClient() : m_frameCount(0), m_parent(0)
    {
    }

    virtual void didCreateFrame(WebFrame* parent, WebFrame* child)
    {
        m_frameCount++;
        if (!m_parent)
            m_parent = parent;
    }

    int m_frameCount;
    WebFrame* m_parent;
};

TEST_F(WebFrameTest, DidCreateFrame)
{
    registerMockedHttpURLLoad("iframes_test.html");
    registerMockedHttpURLLoad("visible_iframe.html");
    registerMockedHttpURLLoad("invisible_iframe.html");
    registerMockedHttpURLLoad("zero_sized_iframe.html");

    TestDidCreateFrameWebFrameClient webFrameClient;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "iframes_test.html", false, &webFrameClient);

    EXPECT_EQ(webFrameClient.m_frameCount, 3);
    EXPECT_EQ(webFrameClient.m_parent, m_webView->mainFrame());

    m_webView->close();
    m_webView = 0;
}

class FindUpdateWebFrameClient : public WebFrameClient {
public:
    FindUpdateWebFrameClient()
        : m_findResultsAreReady(false)
        , m_count(-1)
    {
    }

    virtual void reportFindInPageMatchCount(int, int count, bool finalUpdate) OVERRIDE
    {
        m_count = count;
        if (finalUpdate)
            m_findResultsAreReady = true;
    }

    bool findResultsAreReady() const { return m_findResultsAreReady; }
    int count() const { return m_count; }

private:
    bool m_findResultsAreReady;
    int m_count;
};

// This fails on Mac https://bugs.webkit.org/show_bug.cgi?id=108574
#if OS(DARWIN)
TEST_F(WebFrameTest, DISABLED_FindInPageMatchRects)
#else
TEST_F(WebFrameTest, FindInPageMatchRects)
#endif
{
    registerMockedHttpURLLoad("find_in_page.html");
    registerMockedHttpURLLoad("find_in_page_frame.html");

    FindUpdateWebFrameClient client;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "find_in_page.html", true, &client);
    m_webView->resize(WebSize(640, 480));
    m_webView->layout();
    runPendingTasks();

    // Note that the 'result 19' in the <select> element is not expected to produce a match.
    static const char* kFindString = "result";
    static const int kFindIdentifier = 12345;
    static const int kNumResults = 19;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = toWebFrameImpl(m_webView->mainFrame());
    EXPECT_TRUE(mainFrame->find(kFindIdentifier, searchText, options, false, 0));

    mainFrame->resetMatchCount();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        frame->scopeStringMatches(kFindIdentifier, searchText, options, true);

    runPendingTasks();
    EXPECT_TRUE(client.findResultsAreReady());

    WebVector<WebFloatRect> webMatchRects;
    mainFrame->findMatchRects(webMatchRects);
    ASSERT_EQ(webMatchRects.size(), static_cast<size_t>(kNumResults));
    int rectsVersion = mainFrame->findMatchMarkersVersion();

    for (int resultIndex = 0; resultIndex < kNumResults; ++resultIndex) {
        FloatRect resultRect = static_cast<FloatRect>(webMatchRects[resultIndex]);

        // Select the match by the center of its rect.
        EXPECT_EQ(mainFrame->selectNearestFindMatch(resultRect.center(), 0), resultIndex + 1);

        // Check that the find result ordering matches with our expectations.
        Range* result = mainFrame->activeMatchFrame()->activeMatch();
        ASSERT_TRUE(result);
        result->setEnd(result->endContainer(), result->endOffset() + 3);
        EXPECT_EQ(result->text(), String::format("%s %02d", kFindString, resultIndex));

        // Verify that the expected match rect also matches the currently active match.
        // Compare the enclosing rects to prevent precision issues caused by CSS transforms.
        FloatRect activeMatch = mainFrame->activeFindMatchRect();
        EXPECT_EQ(enclosingIntRect(activeMatch), enclosingIntRect(resultRect));

        // The rects version should not have changed.
        EXPECT_EQ(mainFrame->findMatchMarkersVersion(), rectsVersion);
    }

    // All results after the first two ones should be below between them in find-in-page coordinates.
    // This is because results 2 to 9 are inside an iframe located between results 0 and 1. This applies to the fixed div too.
    EXPECT_TRUE(webMatchRects[0].y < webMatchRects[1].y);
    for (int resultIndex = 2; resultIndex < kNumResults; ++resultIndex) {
        EXPECT_TRUE(webMatchRects[0].y < webMatchRects[resultIndex].y);
        EXPECT_TRUE(webMatchRects[1].y > webMatchRects[resultIndex].y);
    }

    // Result 3 should be below both 2 and 4. This is caused by the CSS transform in the containing div.
    // If the transform doesn't work then 3 will be between 2 and 4.
    EXPECT_TRUE(webMatchRects[3].y > webMatchRects[2].y);
    EXPECT_TRUE(webMatchRects[3].y > webMatchRects[4].y);

    // Results 6, 7, 8 and 9 should be one below the other in that same order.
    // If overflow:scroll is not properly handled then result 8 would be below result 9 or
    // result 7 above result 6 depending on the scroll.
    EXPECT_TRUE(webMatchRects[6].y < webMatchRects[7].y);
    EXPECT_TRUE(webMatchRects[7].y < webMatchRects[8].y);
    EXPECT_TRUE(webMatchRects[8].y < webMatchRects[9].y);

    // Results 11, 12, 13 and 14 should be between results 10 and 15, as they are inside the table.
    EXPECT_TRUE(webMatchRects[11].y > webMatchRects[10].y);
    EXPECT_TRUE(webMatchRects[12].y > webMatchRects[10].y);
    EXPECT_TRUE(webMatchRects[13].y > webMatchRects[10].y);
    EXPECT_TRUE(webMatchRects[14].y > webMatchRects[10].y);
    EXPECT_TRUE(webMatchRects[11].y < webMatchRects[15].y);
    EXPECT_TRUE(webMatchRects[12].y < webMatchRects[15].y);
    EXPECT_TRUE(webMatchRects[13].y < webMatchRects[15].y);
    EXPECT_TRUE(webMatchRects[14].y < webMatchRects[15].y);

    // Result 11 should be above 12, 13 and 14 as it's in the table header.
    EXPECT_TRUE(webMatchRects[11].y < webMatchRects[12].y);
    EXPECT_TRUE(webMatchRects[11].y < webMatchRects[13].y);
    EXPECT_TRUE(webMatchRects[11].y < webMatchRects[14].y);

    // Result 11 should also be right to 12, 13 and 14 because of the colspan.
    EXPECT_TRUE(webMatchRects[11].x > webMatchRects[12].x);
    EXPECT_TRUE(webMatchRects[11].x > webMatchRects[13].x);
    EXPECT_TRUE(webMatchRects[11].x > webMatchRects[14].x);

    // Result 12 should be left to results 11, 13 and 14 in the table layout.
    EXPECT_TRUE(webMatchRects[12].x < webMatchRects[11].x);
    EXPECT_TRUE(webMatchRects[12].x < webMatchRects[13].x);
    EXPECT_TRUE(webMatchRects[12].x < webMatchRects[14].x);

    // Results 13, 12 and 14 should be one above the other in that order because of the rowspan
    // and vertical-align: middle by default.
    EXPECT_TRUE(webMatchRects[13].y < webMatchRects[12].y);
    EXPECT_TRUE(webMatchRects[12].y < webMatchRects[14].y);

    // Result 16 should be below result 15.
    EXPECT_TRUE(webMatchRects[15].y > webMatchRects[14].y);

    // Result 18 should be normalized with respect to the position:relative div, and not it's
    // immediate containing div. Consequently, result 18 should be above result 17.
    EXPECT_TRUE(webMatchRects[17].y > webMatchRects[18].y);

    // Resizing should update the rects version.
    m_webView->resize(WebSize(800, 600));
    runPendingTasks();
    EXPECT_TRUE(mainFrame->findMatchMarkersVersion() != rectsVersion);

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, FindInPageSkipsHiddenFrames)
{
    registerMockedHttpURLLoad("find_in_hidden_frame.html");

    FindUpdateWebFrameClient client;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "find_in_hidden_frame.html", true, &client);
    m_webView->resize(WebSize(640, 480));
    m_webView->layout();
    runPendingTasks();

    static const char* kFindString = "hello";
    static const int kFindIdentifier = 12345;
    static const int kNumResults = 1;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = toWebFrameImpl(m_webView->mainFrame());
    EXPECT_TRUE(mainFrame->find(kFindIdentifier, searchText, options, false, 0));

    mainFrame->resetMatchCount();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        frame->scopeStringMatches(kFindIdentifier, searchText, options, true);

    runPendingTasks();
    EXPECT_TRUE(client.findResultsAreReady());
    EXPECT_EQ(kNumResults, client.count());

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, FindOnDetachedFrame)
{
    registerMockedHttpURLLoad("find_in_page.html");
    registerMockedHttpURLLoad("find_in_page_frame.html");

    FindUpdateWebFrameClient client;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "find_in_page.html", true, &client);
    m_webView->resize(WebSize(640, 480));
    m_webView->layout();
    runPendingTasks();

    static const char* kFindString = "result";
    static const int kFindIdentifier = 12345;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = toWebFrameImpl(m_webView->mainFrame());
    WebFrameImpl* secondFrame = toWebFrameImpl(mainFrame->traverseNext(false));
    RefPtr<WebCore::Frame> holdSecondFrame = secondFrame->frame();

    // Detach the frame before finding.
    EXPECT_TRUE(mainFrame->document().getElementById("frame").remove());

    EXPECT_TRUE(mainFrame->find(kFindIdentifier, searchText, options, false, 0));
    EXPECT_FALSE(secondFrame->find(kFindIdentifier, searchText, options, false, 0));

    runPendingTasks();
    EXPECT_FALSE(client.findResultsAreReady());

    mainFrame->resetMatchCount();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        frame->scopeStringMatches(kFindIdentifier, searchText, options, true);

    runPendingTasks();
    EXPECT_TRUE(client.findResultsAreReady());

    holdSecondFrame.release();

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, FindDetachFrameBeforeScopeStrings)
{
    registerMockedHttpURLLoad("find_in_page.html");
    registerMockedHttpURLLoad("find_in_page_frame.html");

    FindUpdateWebFrameClient client;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "find_in_page.html", true, &client);
    m_webView->resize(WebSize(640, 480));
    m_webView->layout();
    runPendingTasks();

    static const char* kFindString = "result";
    static const int kFindIdentifier = 12345;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = toWebFrameImpl(m_webView->mainFrame());
    WebFrameImpl* secondFrame = toWebFrameImpl(mainFrame->traverseNext(false));
    RefPtr<WebCore::Frame> holdSecondFrame = secondFrame->frame();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        EXPECT_TRUE(frame->find(kFindIdentifier, searchText, options, false, 0));

    runPendingTasks();
    EXPECT_FALSE(client.findResultsAreReady());

    // Detach the frame between finding and scoping.
    EXPECT_TRUE(mainFrame->document().getElementById("frame").remove());

    mainFrame->resetMatchCount();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        frame->scopeStringMatches(kFindIdentifier, searchText, options, true);

    runPendingTasks();
    EXPECT_TRUE(client.findResultsAreReady());

    holdSecondFrame.release();

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, FindDetachFrameWhileScopingStrings)
{
    registerMockedHttpURLLoad("find_in_page.html");
    registerMockedHttpURLLoad("find_in_page_frame.html");

    FindUpdateWebFrameClient client;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "find_in_page.html", true, &client);
    m_webView->resize(WebSize(640, 480));
    m_webView->layout();
    runPendingTasks();

    static const char* kFindString = "result";
    static const int kFindIdentifier = 12345;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = toWebFrameImpl(m_webView->mainFrame());
    WebFrameImpl* secondFrame = toWebFrameImpl(mainFrame->traverseNext(false));
    RefPtr<WebCore::Frame> holdSecondFrame = secondFrame->frame();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        EXPECT_TRUE(frame->find(kFindIdentifier, searchText, options, false, 0));

    runPendingTasks();
    EXPECT_FALSE(client.findResultsAreReady());

    mainFrame->resetMatchCount();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        frame->scopeStringMatches(kFindIdentifier, searchText, options, true);

    // The first scopeStringMatches will have reset the state. Detach before it actually scopes.
    EXPECT_TRUE(mainFrame->document().getElementById("frame").remove());

    runPendingTasks();
    EXPECT_TRUE(client.findResultsAreReady());

    holdSecondFrame.release();

    m_webView->close();
    m_webView = 0;
}

static WebView* createWebViewForTextSelection(const std::string& url)
{
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(url, true);
    webView->settings()->setDefaultFontSize(12);
    webView->enableFixedLayoutMode(false);
    webView->resize(WebSize(640, 480));
    return webView;
}

static WebPoint topLeft(const WebRect& rect)
{
    return WebPoint(rect.x, rect.y);
}

static WebPoint bottomRightMinusOne(const WebRect& rect)
{
    // FIXME: If we don't subtract 1 from the x- and y-coordinates of the
    // selection bounds, selectRange() will select the *next* element. That's
    // strictly correct, as hit-testing checks the pixel to the lower-right of
    // the input coordinate, but it's a wart on the API.
    return WebPoint(rect.x + rect.width - 1, rect.y + rect.height - 1);
}

static WebRect elementBounds(WebFrame* frame, const WebString& id)
{
    return frame->document().getElementById(id).boundsInViewportSpace();
}

static std::string selectionAsString(WebFrame* frame)
{
    return std::string(frame->selectionAsText().utf8().data());
}

TEST_F(WebFrameTest, SelectRange)
{
    WebFrame* frame;
    WebRect startWebRect;
    WebRect endWebRect;

    registerMockedHttpURLLoad("select_range_basic.html");
    registerMockedHttpURLLoad("select_range_scroll.html");

    m_webView = createWebViewForTextSelection(m_baseURL + "select_range_basic.html");
    frame = m_webView->mainFrame();
    EXPECT_EQ("Some test text for testing.", selectionAsString(frame));
    m_webView->selectionBounds(startWebRect, endWebRect);
    frame->executeCommand(WebString::fromUTF8("Unselect"));
    EXPECT_EQ("", selectionAsString(frame));
    frame->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
    EXPECT_EQ("Some test text for testing.", selectionAsString(frame));
    m_webView->close();
    m_webView = 0;

    m_webView = createWebViewForTextSelection(m_baseURL + "select_range_scroll.html");
    frame = m_webView->mainFrame();
    EXPECT_EQ("Some offscreen test text for testing.", selectionAsString(frame));
    m_webView->selectionBounds(startWebRect, endWebRect);
    frame->executeCommand(WebString::fromUTF8("Unselect"));
    EXPECT_EQ("", selectionAsString(frame));
    frame->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
    EXPECT_EQ("Some offscreen test text for testing.", selectionAsString(frame));
    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, SelectRangeInIframe)
{
    WebFrame* frame;
    WebRect startWebRect;
    WebRect endWebRect;

    registerMockedHttpURLLoad("select_range_iframe.html");
    registerMockedHttpURLLoad("select_range_basic.html");

    m_webView = createWebViewForTextSelection(m_baseURL + "select_range_iframe.html");
    frame = m_webView->mainFrame();
    WebFrame* subframe = frame->findChildByExpression(WebString::fromUTF8("/html/body/iframe"));
    EXPECT_EQ("Some test text for testing.", selectionAsString(subframe));
    m_webView->selectionBounds(startWebRect, endWebRect);
    subframe->executeCommand(WebString::fromUTF8("Unselect"));
    EXPECT_EQ("", selectionAsString(subframe));
    subframe->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
    EXPECT_EQ("Some test text for testing.", selectionAsString(subframe));
    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, SelectRangeDivContentEditable)
{
    WebFrame* frame;
    WebRect startWebRect;
    WebRect endWebRect;

    registerMockedHttpURLLoad("select_range_div_editable.html");

    // Select the middle of an editable element, then try to extend the selection to the top of the document.
    // The selection range should be clipped to the bounds of the editable element.
    m_webView = createWebViewForTextSelection(m_baseURL + "select_range_div_editable.html");
    frame = m_webView->mainFrame();
    EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
    m_webView->selectionBounds(startWebRect, endWebRect);

    frame->selectRange(bottomRightMinusOne(endWebRect), WebPoint(0, 0));
    EXPECT_EQ("16-char header. This text is initially selected.", selectionAsString(frame));
    m_webView->close();
    m_webView = 0;

    // As above, but extending the selection to the bottom of the document.
    m_webView = createWebViewForTextSelection(m_baseURL + "select_range_div_editable.html");
    frame = m_webView->mainFrame();

    m_webView->selectionBounds(startWebRect, endWebRect);
    frame->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
    EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
    m_webView->selectionBounds(startWebRect, endWebRect);

    m_webView->selectionBounds(startWebRect, endWebRect);
    frame->selectRange(topLeft(startWebRect), WebPoint(640, 480));
    EXPECT_EQ("This text is initially selected. 16-char footer.", selectionAsString(frame));
    m_webView->close();
    m_webView = 0;
}

// positionForPoint returns the wrong values for contenteditable spans. See
// http://crbug.com/238334.
TEST_F(WebFrameTest, DISABLED_SelectRangeSpanContentEditable)
{
    WebFrame* frame;
    WebRect startWebRect;
    WebRect endWebRect;

    registerMockedHttpURLLoad("select_range_span_editable.html");

    // Select the middle of an editable element, then try to extend the selection to the top of the document.
    // The selection range should be clipped to the bounds of the editable element.
    m_webView = createWebViewForTextSelection(m_baseURL + "select_range_span_editable.html");
    frame = m_webView->mainFrame();
    EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
    m_webView->selectionBounds(startWebRect, endWebRect);

    frame->selectRange(bottomRightMinusOne(endWebRect), WebPoint(0, 0));
    EXPECT_EQ("16-char header. This text is initially selected.", selectionAsString(frame));
    m_webView->close();
    m_webView = 0;

    // As above, but extending the selection to the bottom of the document.
    m_webView = createWebViewForTextSelection(m_baseURL + "select_range_span_editable.html");
    frame = m_webView->mainFrame();

    m_webView->selectionBounds(startWebRect, endWebRect);
    frame->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
    EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
    m_webView->selectionBounds(startWebRect, endWebRect);

    EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
    m_webView->selectionBounds(startWebRect, endWebRect);
    frame->selectRange(topLeft(startWebRect), WebPoint(640, 480));
    EXPECT_EQ("This text is initially selected. 16-char footer.", selectionAsString(frame));
    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, SelectRangeCanMoveSelectionStart)
{
    registerMockedHttpURLLoad("text_selection.html");
    m_webView = createWebViewForTextSelection(m_baseURL + "text_selection.html");
    WebFrame* frame = m_webView->mainFrame();

    // Select second span. We can move the start to include the first span.
    frame->executeScript(WebScriptSource("selectElement('header_2');"));
    EXPECT_EQ("Header 2.", selectionAsString(frame));
    frame->selectRange(bottomRightMinusOne(elementBounds(frame, "header_2")), topLeft(elementBounds(frame, "header_1")));
    EXPECT_EQ("Header 1. Header 2.", selectionAsString(frame));

    // We can move the start and end together.
    frame->executeScript(WebScriptSource("selectElement('header_1');"));
    EXPECT_EQ("Header 1.", selectionAsString(frame));
    frame->selectRange(bottomRightMinusOne(elementBounds(frame, "header_1")), bottomRightMinusOne(elementBounds(frame, "header_1")));
    EXPECT_EQ("", selectionAsString(frame));
    // Selection is a caret, not empty.
    EXPECT_FALSE(frame->selectionRange().isNull());

    // We can move the start across the end.
    frame->executeScript(WebScriptSource("selectElement('header_1');"));
    EXPECT_EQ("Header 1.", selectionAsString(frame));
    frame->selectRange(bottomRightMinusOne(elementBounds(frame, "header_1")), bottomRightMinusOne(elementBounds(frame, "header_2")));
    EXPECT_EQ(" Header 2.", selectionAsString(frame));

    // Can't extend the selection part-way into an editable element.
    frame->executeScript(WebScriptSource("selectElement('footer_2');"));
    EXPECT_EQ("Footer 2.", selectionAsString(frame));
    frame->selectRange(bottomRightMinusOne(elementBounds(frame, "footer_2")), topLeft(elementBounds(frame, "editable_2")));
    EXPECT_EQ(" [ Footer 1. Footer 2.", selectionAsString(frame));

    // Can extend the selection completely across editable elements.
    frame->executeScript(WebScriptSource("selectElement('footer_2');"));
    EXPECT_EQ("Footer 2.", selectionAsString(frame));
    frame->selectRange(bottomRightMinusOne(elementBounds(frame, "footer_2")), topLeft(elementBounds(frame, "header_2")));
    EXPECT_EQ("Header 2. ] [ Editable 1. Editable 2. ] [ Footer 1. Footer 2.", selectionAsString(frame));

    // If the selection is editable text, we can't extend it into non-editable text.
    frame->executeScript(WebScriptSource("selectElement('editable_2');"));
    EXPECT_EQ("Editable 2.", selectionAsString(frame));
    frame->selectRange(bottomRightMinusOne(elementBounds(frame, "editable_2")), topLeft(elementBounds(frame, "header_2")));
    // positionForPoint returns the wrong values for contenteditable spans. See
    // http://crbug.com/238334.
    // EXPECT_EQ("[ Editable 1. Editable 2.", selectionAsString(frame));
}

TEST_F(WebFrameTest, SelectRangeCanMoveSelectionEnd)
{
    registerMockedHttpURLLoad("text_selection.html");
    m_webView = createWebViewForTextSelection(m_baseURL + "text_selection.html");
    WebFrame* frame = m_webView->mainFrame();

    // Select first span. We can move the end to include the second span.
    frame->executeScript(WebScriptSource("selectElement('header_1');"));
    EXPECT_EQ("Header 1.", selectionAsString(frame));
    frame->selectRange(topLeft(elementBounds(frame, "header_1")), bottomRightMinusOne(elementBounds(frame, "header_2")));
    EXPECT_EQ("Header 1. Header 2.", selectionAsString(frame));

    // We can move the start and end together.
    frame->executeScript(WebScriptSource("selectElement('header_2');"));
    EXPECT_EQ("Header 2.", selectionAsString(frame));
    frame->selectRange(topLeft(elementBounds(frame, "header_2")), topLeft(elementBounds(frame, "header_2")));
    EXPECT_EQ("", selectionAsString(frame));
    // Selection is a caret, not empty.
    EXPECT_FALSE(frame->selectionRange().isNull());

    // We can move the end across the start.
    frame->executeScript(WebScriptSource("selectElement('header_2');"));
    EXPECT_EQ("Header 2.", selectionAsString(frame));
    frame->selectRange(topLeft(elementBounds(frame, "header_2")), topLeft(elementBounds(frame, "header_1")));
    EXPECT_EQ("Header 1. ", selectionAsString(frame));

    // Can't extend the selection part-way into an editable element.
    frame->executeScript(WebScriptSource("selectElement('header_1');"));
    EXPECT_EQ("Header 1.", selectionAsString(frame));
    frame->selectRange(topLeft(elementBounds(frame, "header_1")), bottomRightMinusOne(elementBounds(frame, "editable_1")));
    EXPECT_EQ("Header 1. Header 2. ] ", selectionAsString(frame));

    // Can extend the selection completely across editable elements.
    frame->executeScript(WebScriptSource("selectElement('header_1');"));
    EXPECT_EQ("Header 1.", selectionAsString(frame));
    frame->selectRange(topLeft(elementBounds(frame, "header_1")), bottomRightMinusOne(elementBounds(frame, "footer_1")));
    EXPECT_EQ("Header 1. Header 2. ] [ Editable 1. Editable 2. ] [ Footer 1.", selectionAsString(frame));

    // If the selection is editable text, we can't extend it into non-editable text.
    frame->executeScript(WebScriptSource("selectElement('editable_1');"));
    EXPECT_EQ("Editable 1.", selectionAsString(frame));
    frame->selectRange(topLeft(elementBounds(frame, "editable_1")), bottomRightMinusOne(elementBounds(frame, "footer_1")));
    // positionForPoint returns the wrong values for contenteditable spans. See
    // http://crbug.com/238334.
    // EXPECT_EQ("Editable 1. Editable 2. ]", selectionAsString(frame));
}

static int computeOffset(WebCore::RenderObject* renderer, int x, int y)
{
    return WebCore::VisiblePosition(renderer->positionForPoint(WebCore::LayoutPoint(x, y))).deepEquivalent().computeOffsetInContainerNode();
}

// positionForPoint returns the wrong values for contenteditable spans. See
// http://crbug.com/238334.
TEST_F(WebFrameTest, DISABLED_PositionForPointTest)
{
    registerMockedHttpURLLoad("select_range_span_editable.html");
    m_webView = createWebViewForTextSelection(m_baseURL + "select_range_span_editable.html");
    WebFrameImpl* mainFrame = toWebFrameImpl(m_webView->mainFrame());
    WebCore::RenderObject* renderer = mainFrame->frame()->selection().rootEditableElement()->renderer();
    EXPECT_EQ(0, computeOffset(renderer, -1, -1));
    EXPECT_EQ(64, computeOffset(renderer, 1000, 1000));

    registerMockedHttpURLLoad("select_range_div_editable.html");
    m_webView = createWebViewForTextSelection(m_baseURL + "select_range_div_editable.html");
    mainFrame = toWebFrameImpl(m_webView->mainFrame());
    renderer = mainFrame->frame()->selection().rootEditableElement()->renderer();
    EXPECT_EQ(0, computeOffset(renderer, -1, -1));
    EXPECT_EQ(64, computeOffset(renderer, 1000, 1000));
}

#if !OS(DARWIN)
TEST_F(WebFrameTest, SelectRangeStaysHorizontallyAlignedWhenMoved)
{
    WebFrameImpl* frame;
    registerMockedHttpURLLoad("move_caret.html");

    m_webView = createWebViewForTextSelection(m_baseURL + "move_caret.html");
    frame = (WebFrameImpl*)m_webView->mainFrame();

    WebRect initialStartRect;
    WebRect initialEndRect;
    WebRect startRect;
    WebRect endRect;

    frame->executeScript(WebScriptSource("selectRange();"));
    m_webView->selectionBounds(initialStartRect, initialEndRect);
    WebPoint movedStart(topLeft(initialStartRect));

    movedStart.y += 40;
    frame->selectRange(movedStart, bottomRightMinusOne(initialEndRect));
    m_webView->selectionBounds(startRect, endRect);
    EXPECT_EQ(startRect, initialStartRect);
    EXPECT_EQ(endRect, initialEndRect);

    movedStart.y -= 80;
    frame->selectRange(movedStart, bottomRightMinusOne(initialEndRect));
    m_webView->selectionBounds(startRect, endRect);
    EXPECT_EQ(startRect, initialStartRect);
    EXPECT_EQ(endRect, initialEndRect);

    WebPoint movedEnd(bottomRightMinusOne(initialEndRect));

    movedEnd.y += 40;
    frame->selectRange(topLeft(initialStartRect), movedEnd);
    m_webView->selectionBounds(startRect, endRect);
    EXPECT_EQ(startRect, initialStartRect);
    EXPECT_EQ(endRect, initialEndRect);

    movedEnd.y -= 80;
    frame->selectRange(topLeft(initialStartRect), movedEnd);
    m_webView->selectionBounds(startRect, endRect);
    EXPECT_EQ(startRect, initialStartRect);
    EXPECT_EQ(endRect, initialEndRect);
}

TEST_F(WebFrameTest, MoveCaretStaysHorizontallyAlignedWhenMoved)
{
    WebFrameImpl* frame;
    registerMockedHttpURLLoad("move_caret.html");

    m_webView = createWebViewForTextSelection(m_baseURL + "move_caret.html");
    frame = (WebFrameImpl*)m_webView->mainFrame();

    WebRect initialStartRect;
    WebRect initialEndRect;
    WebRect startRect;
    WebRect endRect;

    frame->executeScript(WebScriptSource("selectCaret();"));
    m_webView->selectionBounds(initialStartRect, initialEndRect);
    WebPoint moveTo(topLeft(initialStartRect));

    moveTo.y += 40;
    frame->moveCaretSelectionTowardsWindowPoint(moveTo);
    m_webView->selectionBounds(startRect, endRect);
    EXPECT_EQ(startRect, initialStartRect);
    EXPECT_EQ(endRect, initialEndRect);

    moveTo.y -= 80;
    frame->moveCaretSelectionTowardsWindowPoint(moveTo);
    m_webView->selectionBounds(startRect, endRect);
    EXPECT_EQ(startRect, initialStartRect);
    EXPECT_EQ(endRect, initialEndRect);
}
#endif

class DisambiguationPopupTestWebViewClient : public WebViewClient {
public:
    virtual bool didTapMultipleTargets(const WebGestureEvent&, const WebVector<WebRect>& targetRects) OVERRIDE
    {
        EXPECT_GE(targetRects.size(), 2u);
        m_triggered = true;
        return true;
    }

    bool triggered() const { return m_triggered; }
    void resetTriggered() { m_triggered = false; }
    bool m_triggered;
};

static WebGestureEvent fatTap(int x, int y)
{
    WebGestureEvent event;
    event.type = WebInputEvent::GestureTap;
    event.x = x;
    event.y = y;
    event.data.tap.width = 50;
    event.data.tap.height = 50;
    return event;
}

TEST_F(WebFrameTest, DisambiguationPopup)
{
    const std::string htmlFile = "disambiguation_popup.html";
    registerMockedHttpURLLoad(htmlFile);

    DisambiguationPopupTestWebViewClient client;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + htmlFile, true, 0, &client);
    m_webView->resize(WebSize(1000, 1000));
    m_webView->layout();

    client.resetTriggered();
    m_webView->handleInputEvent(fatTap(0, 0));
    EXPECT_FALSE(client.triggered());

    client.resetTriggered();
    m_webView->handleInputEvent(fatTap(200, 115));
    EXPECT_FALSE(client.triggered());

    for (int i = 0; i <= 46; i++) {
        client.resetTriggered();
        m_webView->handleInputEvent(fatTap(120, 230 + i * 5));

        int j = i % 10;
        if (j >= 7 && j <= 9)
            EXPECT_TRUE(client.triggered());
        else
            EXPECT_FALSE(client.triggered());
    }

    for (int i = 0; i <= 46; i++) {
        client.resetTriggered();
        m_webView->handleInputEvent(fatTap(10 + i * 5, 590));

        int j = i % 10;
        if (j >= 7 && j <= 9)
            EXPECT_TRUE(client.triggered());
        else
            EXPECT_FALSE(client.triggered());
    }

    m_webView->close();
    m_webView = 0;

}

TEST_F(WebFrameTest, DisambiguationPopupNoContainer)
{
    registerMockedHttpURLLoad("disambiguation_popup_no_container.html");

    DisambiguationPopupTestWebViewClient client;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "disambiguation_popup_no_container.html", true, 0, &client);
    m_webView->resize(WebSize(1000, 1000));
    m_webView->layout();

    client.resetTriggered();
    m_webView->handleInputEvent(fatTap(50, 50));
    EXPECT_FALSE(client.triggered());

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, DisambiguationPopupMobileSite)
{
    const std::string htmlFile = "disambiguation_popup_mobile_site.html";
    registerMockedHttpURLLoad(htmlFile);

    DisambiguationPopupTestWebViewClient client;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + htmlFile, true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(1000, 1000));
    m_webView->layout();

    client.resetTriggered();
    m_webView->handleInputEvent(fatTap(0, 0));
    EXPECT_FALSE(client.triggered());

    client.resetTriggered();
    m_webView->handleInputEvent(fatTap(200, 115));
    EXPECT_FALSE(client.triggered());

    for (int i = 0; i <= 46; i++) {
        client.resetTriggered();
        m_webView->handleInputEvent(fatTap(120, 230 + i * 5));
        EXPECT_FALSE(client.triggered());
    }

    for (int i = 0; i <= 46; i++) {
        client.resetTriggered();
        m_webView->handleInputEvent(fatTap(10 + i * 5, 590));
        EXPECT_FALSE(client.triggered());
    }

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, DisambiguationPopupViewportSite)
{
    const std::string htmlFile = "disambiguation_popup_viewport_site.html";
    registerMockedHttpURLLoad(htmlFile);

    DisambiguationPopupTestWebViewClient client;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + htmlFile, true, 0, &client);
    m_webView->enableFixedLayoutMode(true);
    m_webView->settings()->setViewportEnabled(true);
    m_webView->resize(WebSize(1000, 1000));
    m_webView->layout();

    client.resetTriggered();
    m_webView->handleInputEvent(fatTap(0, 0));
    EXPECT_FALSE(client.triggered());

    client.resetTriggered();
    m_webView->handleInputEvent(fatTap(200, 115));
    EXPECT_FALSE(client.triggered());

    for (int i = 0; i <= 46; i++) {
        client.resetTriggered();
        m_webView->handleInputEvent(fatTap(120, 230 + i * 5));
        EXPECT_FALSE(client.triggered());
    }

    for (int i = 0; i <= 46; i++) {
        client.resetTriggered();
        m_webView->handleInputEvent(fatTap(10 + i * 5, 590));
        EXPECT_FALSE(client.triggered());
    }

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, DisambiguationPopupBlacklist)
{
    const unsigned viewportWidth = 500;
    const unsigned viewportHeight = 1000;
    const unsigned divHeight = 100;
    const std::string htmlFile = "disambiguation_popup_blacklist.html";
    registerMockedHttpURLLoad(htmlFile);

    DisambiguationPopupTestWebViewClient client;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + htmlFile, true, 0, &client);
    m_webView->resize(WebSize(viewportWidth, viewportHeight));
    m_webView->layout();

    // Click somewhere where the popup shouldn't appear.
    client.resetTriggered();
    m_webView->handleInputEvent(fatTap(viewportWidth / 2, 0));
    EXPECT_FALSE(client.triggered());

    // Click directly in between two container divs with click handlers, with children that don't handle clicks.
    client.resetTriggered();
    m_webView->handleInputEvent(fatTap(viewportWidth / 2, divHeight));
    EXPECT_TRUE(client.triggered());

    // The third div container should be blacklisted if you click on the link it contains.
    client.resetTriggered();
    m_webView->handleInputEvent(fatTap(viewportWidth / 2, divHeight * 3.25));
    EXPECT_FALSE(client.triggered());

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, DisambiguationPopupPageScale)
{
    registerMockedHttpURLLoad("disambiguation_popup_page_scale.html");

    DisambiguationPopupTestWebViewClient client;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "disambiguation_popup_page_scale.html", true, 0, &client);
    m_webView->resize(WebSize(1000, 1000));
    m_webView->layout();

    client.resetTriggered();
    m_webView->handleInputEvent(fatTap(80, 80));
    EXPECT_TRUE(client.triggered());

    client.resetTriggered();
    m_webView->handleInputEvent(fatTap(230, 190));
    EXPECT_TRUE(client.triggered());

    m_webView->setPageScaleFactor(3.0f, WebPoint(0, 0));
    m_webView->layout();

    client.resetTriggered();
    m_webView->handleInputEvent(fatTap(240, 240));
    EXPECT_TRUE(client.triggered());

    client.resetTriggered();
    m_webView->handleInputEvent(fatTap(690, 570));
    EXPECT_FALSE(client.triggered());

    m_webView->close();
    m_webView = 0;
}

class TestSubstituteDataWebFrameClient : public WebFrameClient {
public:
    TestSubstituteDataWebFrameClient()
        : m_commitCalled(false)
    {
    }

    virtual void didFailProvisionalLoad(WebFrame* frame, const WebURLError& error)
    {
        frame->loadHTMLString("This should appear", toKURL("data:text/html,chromewebdata"), error.unreachableURL, true);
        runPendingTasks();
    }

    virtual void didCommitProvisionalLoad(WebFrame* frame, bool)
    {
        if (frame->dataSource()->response().url() != WebURL(URLTestHelpers::toKURL("about:blank")))
            m_commitCalled = true;
    }

    bool commitCalled() const { return m_commitCalled; }

private:
    bool m_commitCalled;
};

TEST_F(WebFrameTest, ReplaceNavigationAfterHistoryNavigation)
{
    TestSubstituteDataWebFrameClient webFrameClient;

    m_webView = FrameTestHelpers::createWebViewAndLoad("about:blank", true, &webFrameClient);
    runPendingTasks();
    WebFrame* frame = m_webView->mainFrame();

    // Load a url as a history navigation that will return an error. TestSubstituteDataWebFrameClient
    // will start a SubstituteData load in response to the load failure, which should get fully committed.
    // Due to https://bugs.webkit.org/show_bug.cgi?id=91685, FrameLoader::didReceiveData() wasn't getting
    // called in this case, which resulted in the SubstituteData document not getting displayed.
    WebURLError error;
    error.reason = 1337;
    error.domain = "WebFrameTest";
    std::string errorURL = "http://0.0.0.0";
    WebURLResponse response;
    response.initialize();
    response.setURL(URLTestHelpers::toKURL(errorURL));
    response.setMIMEType("text/html");
    response.setHTTPStatusCode(500);
    WebHistoryItem errorHistoryItem;
    errorHistoryItem.initialize();
    errorHistoryItem.setURLString(WebString::fromUTF8(errorURL.c_str(), errorURL.length()));
    errorHistoryItem.setOriginalURLString(WebString::fromUTF8(errorURL.c_str(), errorURL.length()));
    Platform::current()->unitTestSupport()->registerMockedErrorURL(URLTestHelpers::toKURL(errorURL), response, error);
    frame->loadHistoryItem(errorHistoryItem);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();

    WebString text = frame->contentAsText(std::numeric_limits<size_t>::max());
    EXPECT_EQ("This should appear", std::string(text.utf8().data()));
    EXPECT_TRUE(webFrameClient.commitCalled());

    m_webView->close();
    m_webView = 0;
}

class TestWillInsertBodyWebFrameClient : public WebFrameClient {
public:
    TestWillInsertBodyWebFrameClient() : m_numBodies(0), m_didLoad(false)
    {
    }

    virtual void didCommitProvisionalLoad(WebFrame*, bool) OVERRIDE
    {
        m_numBodies = 0;
        m_didLoad = true;
    }

    virtual void didCreateDocumentElement(WebFrame*) OVERRIDE
    {
        EXPECT_EQ(0, m_numBodies);
    }

    virtual void willInsertBody(WebFrame*) OVERRIDE
    {
        m_numBodies++;
    }

    int m_numBodies;
    bool m_didLoad;
};

TEST_F(WebFrameTest, HTMLDocument)
{
    registerMockedHttpURLLoad("clipped-body.html");

    TestWillInsertBodyWebFrameClient webFrameClient;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "clipped-body.html", false, &webFrameClient);

    EXPECT_TRUE(webFrameClient.m_didLoad);
    EXPECT_EQ(1, webFrameClient.m_numBodies);

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, EmptyDocument)
{
    registerMockedHttpURLLoad("pageserializer/green_rectangle.svg");

    TestWillInsertBodyWebFrameClient webFrameClient;
    m_webView = FrameTestHelpers::createWebView(false, &webFrameClient);

    EXPECT_FALSE(webFrameClient.m_didLoad);
    EXPECT_EQ(1, webFrameClient.m_numBodies); // The empty document that a new frame starts with triggers this.
    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, MoveCaretSelectionTowardsWindowPointWithNoSelection)
{
    m_webView = FrameTestHelpers::createWebViewAndLoad("about:blank", true);
    WebFrame* frame = m_webView->mainFrame();

    // This test passes if this doesn't crash.
    frame->moveCaretSelectionTowardsWindowPoint(WebPoint(0, 0));
}

class SpellCheckClient : public WebSpellCheckClient {
public:
    explicit SpellCheckClient(uint32_t hash = 0) : m_numberOfTimesChecked(0), m_hash(hash) { }
    virtual ~SpellCheckClient() { }
    virtual void requestCheckingOfText(const WebKit::WebString&, const WebKit::WebVector<uint32_t>&, const WebKit::WebVector<unsigned>&, WebKit::WebTextCheckingCompletion* completion) OVERRIDE
    {
        ++m_numberOfTimesChecked;
        Vector<WebTextCheckingResult> results;
        const int misspellingStartOffset = 1;
        const int misspellingLength = 8;
        results.append(WebTextCheckingResult(WebTextCheckingTypeSpelling, misspellingStartOffset, misspellingLength, WebString(), m_hash));
        completion->didFinishCheckingText(results);
    }
    int numberOfTimesChecked() const { return m_numberOfTimesChecked; }
private:
    int m_numberOfTimesChecked;
    uint32_t m_hash;
};

TEST_F(WebFrameTest, ReplaceMisspelledRange)
{
    registerMockedHttpURLLoad("spell.html");
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "spell.html");
    SpellCheckClient spellcheck;
    m_webView->setSpellCheckClient(&spellcheck);

    WebFrameImpl* frame = toWebFrameImpl(m_webView->mainFrame());
    Document* document = frame->frame()->document();
    Element* element = document->getElementById("data");

    m_webView->settings()->setAsynchronousSpellCheckingEnabled(true);
    m_webView->settings()->setUnifiedTextCheckerEnabled(true);
    m_webView->settings()->setEditingBehavior(WebSettings::EditingBehaviorWin);

    element->focus();
    document->execCommand("InsertText", false, "_wellcome_.");

    const int allTextBeginOffset = 0;
    const int allTextLength = 11;
    frame->selectRange(WebRange::fromDocumentRange(frame, allTextBeginOffset, allTextLength));
    RefPtr<Range> selectionRange = frame->frame()->selection().toNormalizedRange();

    EXPECT_EQ(1, spellcheck.numberOfTimesChecked());
    EXPECT_EQ(1U, document->markers()->markersInRange(selectionRange.get(), DocumentMarker::Spelling).size());

    frame->replaceMisspelledRange("welcome");
    EXPECT_EQ("_welcome_.", std::string(frame->contentAsText(std::numeric_limits<size_t>::max()).utf8().data()));

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, RemoveSpellingMarkers)
{
    registerMockedHttpURLLoad("spell.html");
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "spell.html");
    SpellCheckClient spellcheck;
    m_webView->setSpellCheckClient(&spellcheck);

    WebFrameImpl* frame = toWebFrameImpl(m_webView->mainFrame());
    Document* document = frame->frame()->document();
    Element* element = document->getElementById("data");

    m_webView->settings()->setAsynchronousSpellCheckingEnabled(true);
    m_webView->settings()->setUnifiedTextCheckerEnabled(true);
    m_webView->settings()->setEditingBehavior(WebSettings::EditingBehaviorWin);

    element->focus();
    document->execCommand("InsertText", false, "_wellcome_.");

    frame->removeSpellingMarkers();

    const int allTextBeginOffset = 0;
    const int allTextLength = 11;
    frame->selectRange(WebRange::fromDocumentRange(frame, allTextBeginOffset, allTextLength));
    RefPtr<Range> selectionRange = frame->frame()->selection().toNormalizedRange();

    EXPECT_EQ(0U, document->markers()->markersInRange(selectionRange.get(), DocumentMarker::Spelling).size());

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, MarkerHashIdentifiers) {
    registerMockedHttpURLLoad("spell.html");
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "spell.html");

    static const uint32_t kHash = 42;
    SpellCheckClient spellcheck(kHash);
    m_webView->setSpellCheckClient(&spellcheck);

    WebFrameImpl* frame = toWebFrameImpl(m_webView->mainFrame());
    Document* document = frame->frame()->document();
    Element* element = document->getElementById("data");

    m_webView->settings()->setAsynchronousSpellCheckingEnabled(true);
    m_webView->settings()->setUnifiedTextCheckerEnabled(true);
    m_webView->settings()->setEditingBehavior(WebSettings::EditingBehaviorWin);

    element->focus();
    document->execCommand("InsertText", false, "wellcome.");

    WebVector<uint32_t> documentMarkers;
    m_webView->spellingMarkers(&documentMarkers);
    EXPECT_EQ(1U, documentMarkers.size());
    EXPECT_EQ(kHash, documentMarkers[0]);

    m_webView->close();
    m_webView = 0;
}

class StubbornSpellCheckClient : public WebSpellCheckClient {
public:
    StubbornSpellCheckClient() : m_completion(0) { }
    virtual ~StubbornSpellCheckClient() { }

    virtual void requestCheckingOfText(
        const WebKit::WebString&,
        const WebKit::WebVector<uint32_t>&,
        const WebKit::WebVector<unsigned>&,
        WebKit::WebTextCheckingCompletion* completion) OVERRIDE
    {
        m_completion = completion;
    }

    void kick()
    {
        if (!m_completion)
            return;
        Vector<WebTextCheckingResult> results;
        const int misspellingStartOffset = 1;
        const int misspellingLength = 8;
        results.append(WebTextCheckingResult(WebTextCheckingTypeSpelling, misspellingStartOffset, misspellingLength));
        m_completion->didFinishCheckingText(results);
        m_completion = 0;
    }

private:
    WebKit::WebTextCheckingCompletion* m_completion;
};

TEST_F(WebFrameTest, SlowSpellcheckMarkerPosition)
{
    registerMockedHttpURLLoad("spell.html");
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "spell.html");

    StubbornSpellCheckClient spellcheck;
    m_webView->setSpellCheckClient(&spellcheck);

    WebFrameImpl* frame = toWebFrameImpl(m_webView->mainFrame());
    WebInputElement webInputElement = frame->document().getElementById("data").to<WebInputElement>();
    Document* document = frame->frame()->document();
    Element* element = document->getElementById("data");

    m_webView->settings()->setAsynchronousSpellCheckingEnabled(true);
    m_webView->settings()->setUnifiedTextCheckerEnabled(true);
    m_webView->settings()->setEditingBehavior(WebSettings::EditingBehaviorWin);

    element->focus();
    document->execCommand("InsertText", false, "wellcome ");
    webInputElement.setSelectionRange(0, 0);
    document->execCommand("InsertText", false, "he");

    spellcheck.kick();

    WebVector<uint32_t> documentMarkers;
    m_webView->spellingMarkers(&documentMarkers);
    EXPECT_EQ(0U, documentMarkers.size());

    m_webView->close();
    m_webView = 0;
}

// This test verifies that cancelling spelling request does not cause a
// write-after-free when there's no spellcheck client set.
TEST_F(WebFrameTest, CancelSpellingRequestCrash)
{
    registerMockedHttpURLLoad("spell.html");
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "spell.html");
    m_webView->setSpellCheckClient(0);

    WebFrameImpl* frame = toWebFrameImpl(m_webView->mainFrame());
    Document* document = frame->frame()->document();
    Element* element = document->getElementById("data");

    m_webView->settings()->setAsynchronousSpellCheckingEnabled(true);
    m_webView->settings()->setUnifiedTextCheckerEnabled(true);
    m_webView->settings()->setEditingBehavior(WebSettings::EditingBehaviorWin);

    element->focus();
    frame->frame()->editor().replaceSelectionWithText("A", false, false);
    frame->frame()->editor().spellCheckRequester().cancelCheck();

    m_webView->close();
    m_webView = 0;
}

class TestAccessInitialDocumentWebFrameClient : public WebFrameClient {
public:
    TestAccessInitialDocumentWebFrameClient() : m_didAccessInitialDocument(false)
    {
    }

    virtual void didAccessInitialDocument(WebFrame* frame)
    {
        EXPECT_TRUE(!m_didAccessInitialDocument);
        m_didAccessInitialDocument = true;
    }

    bool m_didAccessInitialDocument;
};

TEST_F(WebFrameTest, DidAccessInitialDocumentBody)
{
    TestAccessInitialDocumentWebFrameClient webFrameClient;
    m_webView = FrameTestHelpers::createWebView(true, &webFrameClient);
    runPendingTasks();
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Create another window that will try to access it.
    WebView* newView = FrameTestHelpers::createWebView(true);
    newView->mainFrame()->setOpener(m_webView->mainFrame());
    runPendingTasks();
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Access the initial document by modifying the body.
    newView->mainFrame()->executeScript(
        WebScriptSource("window.opener.document.body.innerHTML += 'Modified';"));
    runPendingTasks();
    EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);

    // Access the initial document again, to ensure we don't notify twice.
    newView->mainFrame()->executeScript(
        WebScriptSource("window.opener.document.body.innerHTML += 'Modified';"));
    runPendingTasks();
    EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);

    newView->close();
    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, DidAccessInitialDocumentNavigator)
{
    TestAccessInitialDocumentWebFrameClient webFrameClient;
    m_webView = FrameTestHelpers::createWebView(true, &webFrameClient);
    runPendingTasks();
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Create another window that will try to access it.
    WebView* newView = FrameTestHelpers::createWebView(true);
    newView->mainFrame()->setOpener(m_webView->mainFrame());
    runPendingTasks();
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Access the initial document to get to the navigator object.
    newView->mainFrame()->executeScript(
        WebScriptSource("console.log(window.opener.navigator);"));
    runPendingTasks();
    EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);

    newView->close();
    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, DidAccessInitialDocumentViaJavascriptUrl)
{
    TestAccessInitialDocumentWebFrameClient webFrameClient;
    m_webView = FrameTestHelpers::createWebView(true, &webFrameClient);
    runPendingTasks();
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Access the initial document from a javascript: URL.
    FrameTestHelpers::loadFrame(m_webView->mainFrame(), "javascript:document.body.appendChild(document.createTextNode('Modified'))");
    runPendingTasks();
    EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, DidAccessInitialDocumentBodyBeforeModalDialog)
{
    TestAccessInitialDocumentWebFrameClient webFrameClient;
    m_webView = FrameTestHelpers::createWebView(true, &webFrameClient);
    runPendingTasks();
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Create another window that will try to access it.
    WebView* newView = FrameTestHelpers::createWebView(true);
    newView->mainFrame()->setOpener(m_webView->mainFrame());
    runPendingTasks();
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Access the initial document by modifying the body. We normally set a
    // timer to notify the client.
    newView->mainFrame()->executeScript(
        WebScriptSource("window.opener.document.body.innerHTML += 'Modified';"));
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Make sure that a modal dialog forces us to notify right away.
    newView->mainFrame()->executeScript(
        WebScriptSource("window.opener.confirm('Modal');"));
    EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);

    // Ensure that we don't notify again later.
    runPendingTasks();
    EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);

    newView->close();
    m_webView->close();
    m_webView = 0;
}

class TestMainFrameUserOrProgrammaticScrollFrameClient : public WebFrameClient {
public:
    TestMainFrameUserOrProgrammaticScrollFrameClient() { reset(); }
    void reset()
    {
        m_didScrollMainFrame = false;
        m_wasProgrammaticScroll = false;
    }
    bool wasUserScroll() const { return m_didScrollMainFrame && !m_wasProgrammaticScroll; }
    bool wasProgrammaticScroll() const { return m_didScrollMainFrame && m_wasProgrammaticScroll; }

    // WebFrameClient:
    virtual void didChangeScrollOffset(WebFrame* frame) OVERRIDE
    {
        if (frame->parent())
            return;
        EXPECT_FALSE(m_didScrollMainFrame);
        WebCore::FrameView* view = toWebFrameImpl(frame)->frameView();
        // FrameView can be scrolled in FrameView::setFixedVisibleContentRect
        // which is called from Frame::createView (before the frame is associated
        // with the the view).
        if (view) {
            m_didScrollMainFrame = true;
            m_wasProgrammaticScroll = view->inProgrammaticScroll();
        }
    }
private:
    bool m_didScrollMainFrame;
    bool m_wasProgrammaticScroll;
};

TEST_F(WebFrameTest, CompositorScrollIsUserScrollLongPage)
{
    registerMockedHttpURLLoad("long_scroll.html");
    TestMainFrameUserOrProgrammaticScrollFrameClient client;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "long_scroll.html", true, &client);
    m_webView->resize(WebSize(1000, 1000));
    m_webView->layout();

    EXPECT_FALSE(client.wasUserScroll());
    EXPECT_FALSE(client.wasProgrammaticScroll());

    // Do a compositor scroll, verify that this is counted as a user scroll.
    webViewImpl()->applyScrollAndScale(WebSize(0, 1), 1.1f);
    EXPECT_TRUE(client.wasUserScroll());
    client.reset();

    EXPECT_FALSE(client.wasUserScroll());
    EXPECT_FALSE(client.wasProgrammaticScroll());

    // The page scale 1.0f and scroll.
    webViewImpl()->applyScrollAndScale(WebSize(0, 1), 1.0f);
    EXPECT_TRUE(client.wasUserScroll());
    client.reset();

    // No scroll event if there is no scroll delta.
    webViewImpl()->applyScrollAndScale(WebSize(), 1.0f);
    EXPECT_FALSE(client.wasUserScroll());
    EXPECT_FALSE(client.wasProgrammaticScroll());
    client.reset();

    // Non zero page scale and scroll.
    webViewImpl()->applyScrollAndScale(WebSize(9, 13), 0.6f);
    EXPECT_TRUE(client.wasUserScroll());
    client.reset();

    // Programmatic scroll.
    WebFrameImpl* frameImpl = webViewImpl()->mainFrameImpl();
    frameImpl->executeScript(WebScriptSource("window.scrollTo(0, 20);"));
    EXPECT_FALSE(client.wasUserScroll());
    EXPECT_TRUE(client.wasProgrammaticScroll());
    client.reset();

    // Programmatic scroll to same offset. No scroll event should be generated.
    frameImpl->executeScript(WebScriptSource("window.scrollTo(0, 20);"));
    EXPECT_FALSE(client.wasProgrammaticScroll());
    EXPECT_FALSE(client.wasUserScroll());
    client.reset();

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, CompositorScrollIsUserScrollShortPage)
{
    registerMockedHttpURLLoad("short_scroll.html");

    TestMainFrameUserOrProgrammaticScrollFrameClient client;

    // Short page tests.
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "short_scroll.html", true, &client);

    m_webView->resize(WebSize(1000, 1000));
    m_webView->layout();

    EXPECT_FALSE(client.wasUserScroll());
    EXPECT_FALSE(client.wasProgrammaticScroll());

    // Non zero page scale and scroll.
    webViewImpl()->applyScrollAndScale(WebSize(9, 13), 2.0f);
    EXPECT_FALSE(client.wasProgrammaticScroll());
    EXPECT_TRUE(client.wasUserScroll());
    client.reset();

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, FirstPartyForCookiesForRedirect)
{
    WTF::String filePath = Platform::current()->unitTestSupport()->webKitRootDir();
    filePath.append("/Source/web/tests/data/first_party.html");

    WebURL testURL(toKURL("http://www.test.com/first_party_redirect.html"));
    char redirect[] = "http://www.test.com/first_party.html";
    WebURL redirectURL(toKURL(redirect));
    WebURLResponse redirectResponse;
    redirectResponse.initialize();
    redirectResponse.setMIMEType("text/html");
    redirectResponse.setHTTPStatusCode(302);
    redirectResponse.setHTTPHeaderField("Location", redirect);
    Platform::current()->unitTestSupport()->registerMockedURL(testURL, redirectResponse, filePath);

    WebURLResponse finalResponse;
    finalResponse.initialize();
    finalResponse.setMIMEType("text/html");
    Platform::current()->unitTestSupport()->registerMockedURL(redirectURL, finalResponse, filePath);

    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "first_party_redirect.html", true);
    EXPECT_TRUE(m_webView->mainFrame()->document().firstPartyForCookies() == redirectURL);

    m_webView->close();
    m_webView = 0;
}

class TestNavigationPolicyWebFrameClient : public WebFrameClient {
public:

    virtual void didNavigateWithinPage(WebFrame*, bool)
    {
        EXPECT_TRUE(false);
    }
};

TEST_F(WebFrameTest, SimulateFragmentAnchorMiddleClick)
{
    registerMockedHttpURLLoad("fragment_middle_click.html");
    TestNavigationPolicyWebFrameClient client;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "fragment_middle_click.html", true, &client);

    WebCore::Document* document = webViewImpl()->page()->mainFrame()->document();
    WebCore::KURL destination = document->url();
    destination.setFragmentIdentifier("test");

    RefPtr<WebCore::Event> event = WebCore::MouseEvent::create(WebCore::eventNames().clickEvent, false, false,
        document->defaultView(), 0, 0, 0, 0, 0, 0, 0, false, false, false, false, 1, 0, 0);
    WebCore::FrameLoadRequest frameRequest(document->securityOrigin(), WebCore::ResourceRequest(destination));
    frameRequest.setTriggeringEvent(event);
    webViewImpl()->page()->mainFrame()->loader()->load(frameRequest);

    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, BackToReload)
{
    registerMockedHttpURLLoad("fragment_middle_click.html");
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "fragment_middle_click.html", true);
    WebFrame* frame = m_webView->mainFrame();
    WebHistoryItem firstItem = frame->currentHistoryItem();
    EXPECT_FALSE(firstItem.isNull());

    registerMockedHttpURLLoad("white-1x1.png");
    FrameTestHelpers::loadFrame(frame, m_baseURL + "white-1x1.png");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    EXPECT_FALSE(frame->previousHistoryItem().isNull());
    EXPECT_EQ(firstItem.urlString(), frame->previousHistoryItem().urlString());

    frame->loadHistoryItem(frame->previousHistoryItem());
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    EXPECT_EQ(firstItem.urlString(), frame->currentHistoryItem().urlString());

    frame->reload();
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    EXPECT_EQ(WebURLRequest::ReloadIgnoringCacheData, frame->dataSource()->request().cachePolicy());

    m_webView->close();
    m_webView = 0;
}

class TestSameDocumentWebFrameClient : public WebFrameClient {
public:
    TestSameDocumentWebFrameClient()
        : m_frameLoadTypeSameSeen(false)
    {
    }

    virtual void willSendRequest(WebFrame* frame, unsigned, WebURLRequest&, const WebURLResponse&)
    {
        if (toWebFrameImpl(frame)->frame()->loader()->loadType() == WebCore::FrameLoadTypeSame)
            m_frameLoadTypeSameSeen = true;
    }

    bool frameLoadTypeSameSeen() const { return m_frameLoadTypeSameSeen; }

private:
    bool m_frameLoadTypeSameSeen;
};

TEST_F(WebFrameTest, NavigateToSame)
{
    registerMockedHttpURLLoad("navigate_to_same.html");
    TestSameDocumentWebFrameClient client;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "navigate_to_same.html", true, &client);
    EXPECT_FALSE(client.frameLoadTypeSameSeen());

    WebCore::FrameLoadRequest frameRequest(0, WebCore::ResourceRequest(webViewImpl()->page()->mainFrame()->document()->url()));
    webViewImpl()->page()->mainFrame()->loader()->load(frameRequest);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();

    EXPECT_TRUE(client.frameLoadTypeSameSeen());
    m_webView->close();
    m_webView = 0;
}

TEST_F(WebFrameTest, WebNodeImageContents)
{
    m_webView = FrameTestHelpers::createWebViewAndLoad("about:blank", true);
    WebFrame* frame = m_webView->mainFrame();

    static const char bluePNG[] = "<img src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+9AAAAGElEQVQYV2NkYPj/n4EIwDiqEF8oUT94AFIQE/cCn90IAAAAAElFTkSuQmCC\">";

    // Load up the image and test that we can extract the contents.
    WebCore::KURL testURL = toKURL("about:blank");
    frame->loadHTMLString(bluePNG, testURL);
    runPendingTasks();

    WebNode node = frame->document().body().firstChild();
    EXPECT_TRUE(node.isElementNode());
    WebElement element = node.to<WebElement>();
    WebImage image = element.imageContents();
    ASSERT_FALSE(image.isNull());
    EXPECT_EQ(image.size().width, 10);
    EXPECT_EQ(image.size().height, 10);
//    FIXME: The rest of this test is disabled since the ImageDecodeCache state may be inconsistent when this test runs.
//    crbug.com/266088
//    SkBitmap bitmap = image.getSkBitmap();
//    SkAutoLockPixels locker(bitmap);
//    EXPECT_EQ(bitmap.getColor(0, 0), SK_ColorBLUE);
}

class TestStartStopCallbackWebViewClient : public WebViewClient {
public:
    TestStartStopCallbackWebViewClient()
        : m_startLoadingCount(0)
        , m_stopLoadingCount(0)
    {
    }

    virtual void didStartLoading()
    {
        m_startLoadingCount++;
    }

    virtual void didStopLoading()
    {
        m_stopLoadingCount++;
    }

    int startLoadingCount() const { return m_startLoadingCount; }
    int stopLoadingCount() const { return m_stopLoadingCount; }

private:
    int m_startLoadingCount;
    int m_stopLoadingCount;
};

TEST_F(WebFrameTest, PushStateStartsAndStops)
{
    registerMockedHttpURLLoad("push_state.html");
    TestStartStopCallbackWebViewClient client;
    m_webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "push_state.html", true, 0, &client);
    runPendingTasks();

    EXPECT_EQ(client.startLoadingCount(), 2);
    EXPECT_EQ(client.stopLoadingCount(), 2);
    m_webView->close();
    m_webView = 0;
}

} // namespace
