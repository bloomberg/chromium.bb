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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <v8.h>
#include "FrameTestHelpers.h"
#include "RuntimeEnabledFeatures.h"
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
#include "core/clipboard/Clipboard.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/Range.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SpellChecker.h"
#include "core/editing/VisiblePosition.h"
#include "core/events/MouseEvent.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFormElement.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/EventHandler.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/TextAutosizer.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/FloatRect.h"
#include "platform/network/ResourceError.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebUnitTestSupport.h"
#include "wtf/Forward.h"
#include "wtf/dtoa/utils.h"
#include <map>

using namespace blink;
using WebCore::Document;
using WebCore::DocumentMarker;
using WebCore::Element;
using WebCore::FloatRect;
using WebCore::HitTestRequest;
using WebCore::Range;
using blink::URLTestHelpers::toKURL;
using blink::FrameTestHelpers::runPendingTasks;

namespace {

const int touchPointPadding = 32;

#define EXPECT_EQ_RECT(a, b) \
    EXPECT_EQ(a.x(), b.x()); \
    EXPECT_EQ(a.y(), b.y()); \
    EXPECT_EQ(a.width(), b.width()); \
    EXPECT_EQ(a.height(), b.height());

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

    virtual bool enterFullScreen() OVERRIDE { return true; }

private:
    OwnPtr<WebLayerTreeView> m_layerTreeView;
};

class WebFrameTest : public testing::Test {
protected:
    WebFrameTest()
        : m_baseURL("http://www.test.com/")
        , m_chromeURL("chrome://")
    {
    }

    virtual ~WebFrameTest()
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

    static void configueCompositingWebView(WebSettings* settings)
    {
        settings->setForceCompositingMode(true);
        settings->setAcceleratedCompositingEnabled(true);
        settings->setAcceleratedCompositingForFixedPositionEnabled(true);
        settings->setAcceleratedCompositingForOverflowScrollEnabled(true);
        settings->setAcceleratedCompositingForScrollableFramesEnabled(true);
        settings->setCompositedScrollingForFramesEnabled(true);
    }

    void initializeTextSelectionWebView(const std::string& url, FrameTestHelpers::WebViewHelper* webViewHelper)
    {
        webViewHelper->initializeAndLoad(url, true);
        webViewHelper->webView()->settings()->setDefaultFontSize(12);
        webViewHelper->webView()->resize(WebSize(640, 480));
    }

    std::string m_baseURL;
    std::string m_chromeURL;
};

class UseMockScrollbarSettings {
public:
    UseMockScrollbarSettings()
    {
        WebCore::Settings::setMockScrollbarsEnabled(true);
        WebCore::RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(true);
        EXPECT_TRUE(WebCore::ScrollbarTheme::theme()->usesOverlayScrollbars());
    }

    ~UseMockScrollbarSettings()
    {
        WebCore::Settings::setMockScrollbarsEnabled(false);
        WebCore::RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(false);
    }
};

TEST_F(WebFrameTest, ContentText)
{
    registerMockedHttpURLLoad("iframes_test.html");
    registerMockedHttpURLLoad("visible_iframe.html");
    registerMockedHttpURLLoad("invisible_iframe.html");
    registerMockedHttpURLLoad("zero_sized_iframe.html");

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "iframes_test.html");

    // Now retrieve the frames text and test it only includes visible elements.
    std::string content = webViewHelper.webView()->mainFrame()->contentAsText(1024).utf8();
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

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "iframes_test.html", true);

    v8::HandleScope scope(v8::Isolate::GetCurrent());
    EXPECT_EQ(webViewHelper.webView()->mainFrame(), WebFrame::frameForContext(webViewHelper.webView()->mainFrame()->mainWorldScriptContext()));
    EXPECT_EQ(webViewHelper.webView()->mainFrame()->firstChild(), WebFrame::frameForContext(webViewHelper.webView()->mainFrame()->firstChild()->mainWorldScriptContext()));
}

TEST_F(WebFrameTest, FormWithNullFrame)
{
    registerMockedHttpURLLoad("form.html");

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "form.html");

    WebVector<WebFormElement> forms;
    webViewHelper.webView()->mainFrame()->document().forms(forms);
    webViewHelper.reset();

    EXPECT_EQ(forms.size(), 1U);

    // This test passes if this doesn't crash.
    WebSearchableFormData searchableDataForm(forms[0]);
}

TEST_F(WebFrameTest, ChromePageJavascript)
{
    registerMockedChromeURLLoad("history.html");

    // Pass true to enable JavaScript.
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_chromeURL + "history.html", true);

    // Try to run JS against the chrome-style URL.
    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), "javascript:document.body.appendChild(document.createTextNode('Clobbered'))");

    // Required to see any updates in contentAsText.
    webViewHelper.webView()->layout();

    // Now retrieve the frame's text and ensure it was modified by running javascript.
    std::string content = webViewHelper.webView()->mainFrame()->contentAsText(1024).utf8();
    EXPECT_NE(std::string::npos, content.find("Clobbered"));
}

TEST_F(WebFrameTest, ChromePageNoJavascript)
{
    registerMockedChromeURLLoad("history.html");

    /// Pass true to enable JavaScript.
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_chromeURL + "history.html", true);

    // Try to run JS against the chrome-style URL after prohibiting it.
    WebSecurityPolicy::registerURLSchemeAsNotAllowingJavascriptURLs("chrome");
    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), "javascript:document.body.appendChild(document.createTextNode('Clobbered'))");

    // Required to see any updates in contentAsText.
    webViewHelper.webView()->layout();

    // Now retrieve the frame's text and ensure it wasn't modified by running javascript.
    std::string content = webViewHelper.webView()->mainFrame()->contentAsText(1024).utf8();
    EXPECT_EQ(std::string::npos, content.find("Clobbered"));
}

TEST_F(WebFrameTest, LocationSetHostWithMissingPort)
{
    std::string fileName = "print-location-href.html";
    registerMockedHttpURLLoad(fileName);
    URLTestHelpers::registerMockedURLLoad(toKURL("http://www.test.com:0/" + fileName), WebString::fromUTF8(fileName));

    FrameTestHelpers::WebViewHelper webViewHelper;

    /// Pass true to enable JavaScript.
    webViewHelper.initializeAndLoad(m_baseURL + fileName, true);

    // Setting host to "hostname:" should be treated as "hostname:0".
    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), "javascript:location.host = 'www.test.com:'; void 0;");

    runPendingTasks();
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();

    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), "javascript:document.body.textContent = location.href; void 0;");
    // Required to see any updates in contentAsText.
    runPendingTasks();
    webViewHelper.webView()->layout();

    std::string content = webViewHelper.webView()->mainFrame()->contentAsText(1024).utf8();
    EXPECT_EQ("http://www.test.com:0/" + fileName, content);
}

TEST_F(WebFrameTest, LocationSetEmptyPort)
{
    std::string fileName = "print-location-href.html";
    registerMockedHttpURLLoad(fileName);
    URLTestHelpers::registerMockedURLLoad(toKURL("http://www.test.com:0/" + fileName), WebString::fromUTF8(fileName));

    FrameTestHelpers::WebViewHelper webViewHelper;

    /// Pass true to enable JavaScript.
    webViewHelper.initializeAndLoad(m_baseURL + fileName, true);

    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), "javascript:location.port = ''; void 0;");

    runPendingTasks();
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();

    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), "javascript:document.body.textContent = location.href; void 0;");
    // Required to see any updates in contentAsText.
    runPendingTasks();
    webViewHelper.webView()->layout();

    std::string content = webViewHelper.webView()->mainFrame()->contentAsText(1024).utf8();
    EXPECT_EQ("http://www.test.com:0/" + fileName, content);
}

class CSSCallbackWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
public:
    CSSCallbackWebFrameClient() : m_updateCount(0) { }
    virtual void didMatchCSS(WebFrame*, const WebVector<WebString>& newlyMatchingSelectors, const WebVector<WebString>& stoppedMatchingSelectors) OVERRIDE;

    std::map<WebFrame*, std::set<std::string> > m_matchedSelectors;
    int m_updateCount;
};

void CSSCallbackWebFrameClient::didMatchCSS(WebFrame* frame, const WebVector<WebString>& newlyMatchingSelectors, const WebVector<WebString>& stoppedMatchingSelectors)
{
    ++m_updateCount;
    std::set<std::string>& frameSelectors = m_matchedSelectors[frame];
    for (size_t i = 0; i < newlyMatchingSelectors.size(); ++i) {
        std::string selector = newlyMatchingSelectors[i].utf8();
        EXPECT_EQ(0U, frameSelectors.count(selector)) << selector;
        frameSelectors.insert(selector);
    }
    for (size_t i = 0; i < stoppedMatchingSelectors.size(); ++i) {
        std::string selector = stoppedMatchingSelectors[i].utf8();
        EXPECT_EQ(1U, frameSelectors.count(selector)) << selector;
        frameSelectors.erase(selector);
    }
}

class WebFrameCSSCallbackTest : public testing::Test {
protected:
    WebFrameCSSCallbackTest()
    {

        m_frame = m_helper.initializeAndLoad("about:blank", true, &m_client)->mainFrame();
    }

    ~WebFrameCSSCallbackTest()
    {
        EXPECT_EQ(1U, m_client.m_matchedSelectors.size());
    }

    WebDocument doc() const
    {
        return m_frame->document();
    }

    int updateCount() const
    {
        return m_client.m_updateCount;
    }

    const std::set<std::string>& matchedSelectors()
    {
        return m_client.m_matchedSelectors[m_frame];
    }

    void loadHTML(const WebData& html)
    {
        m_frame->loadHTMLString(html, toKURL("about:blank"));
        runPendingTasks();
    }

    void executeScript(const WebString& code)
    {
        m_frame->executeScript(WebScriptSource(code));
        m_frame->view()->layout();
        runPendingTasks();
    }

    CSSCallbackWebFrameClient m_client;
    FrameTestHelpers::WebViewHelper m_helper;
    WebFrame* m_frame;
};

TEST_F(WebFrameCSSCallbackTest, AuthorStyleSheet)
{
    loadHTML(
        "<style>"
        // This stylesheet checks that the internal property and value can't be
        // set by a stylesheet, only WebDocument::watchCSSSelectors().
        "div.initial_on { -internal-callback: none; }"
        "div.initial_off { -internal-callback: -internal-presence; }"
        "</style>"
        "<div class=\"initial_on\"></div>"
        "<div class=\"initial_off\"></div>");

    std::vector<WebString> selectors;
    selectors.push_back(WebString::fromUTF8("div.initial_on"));
    m_frame->document().watchCSSSelectors(WebVector<WebString>(selectors));
    m_frame->view()->layout();
    runPendingTasks();
    EXPECT_EQ(1, updateCount());
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre("div.initial_on"));

    // Check that adding a watched selector calls back for already-present nodes.
    selectors.push_back(WebString::fromUTF8("div.initial_off"));
    doc().watchCSSSelectors(WebVector<WebString>(selectors));
    m_frame->view()->layout();
    runPendingTasks();
    EXPECT_EQ(2, updateCount());
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre("div.initial_off", "div.initial_on"));

    // Check that we can turn off callbacks for certain selectors.
    doc().watchCSSSelectors(WebVector<WebString>());
    m_frame->view()->layout();
    runPendingTasks();
    EXPECT_EQ(3, updateCount());
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre());
}

TEST_F(WebFrameCSSCallbackTest, SharedRenderStyle)
{
    // Check that adding an element calls back when it matches an existing rule.
    std::vector<WebString> selectors;
    selectors.push_back(WebString::fromUTF8("span"));
    doc().watchCSSSelectors(WebVector<WebString>(selectors));

    executeScript(
        "i1 = document.createElement('span');"
        "i1.id = 'first_span';"
        "document.body.appendChild(i1)");
    EXPECT_EQ(1, updateCount());
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre("span"));

    // Adding a second element that shares a RenderStyle shouldn't call back.
    // We use <span>s to avoid default style rules that can set
    // RenderStyle::unique().
    executeScript(
        "i2 = document.createElement('span');"
        "i2.id = 'second_span';"
        "i1 = document.getElementById('first_span');"
        "i1.parentNode.insertBefore(i2, i1.nextSibling);");
    EXPECT_EQ(1, updateCount());
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre("span"));

    // Removing the first element shouldn't call back.
    executeScript(
        "i1 = document.getElementById('first_span');"
        "i1.parentNode.removeChild(i1);");
    EXPECT_EQ(1, updateCount());
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre("span"));

    // But removing the second element *should* call back.
    executeScript(
        "i2 = document.getElementById('second_span');"
        "i2.parentNode.removeChild(i2);");
    EXPECT_EQ(2, updateCount());
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre());
}

TEST_F(WebFrameCSSCallbackTest, CatchesAttributeChange)
{
    loadHTML("<span></span>");

    std::vector<WebString> selectors;
    selectors.push_back(WebString::fromUTF8("span[attr=\"value\"]"));
    doc().watchCSSSelectors(WebVector<WebString>(selectors));
    runPendingTasks();

    EXPECT_EQ(0, updateCount());
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre());

    executeScript(
        "document.querySelector('span').setAttribute('attr', 'value');");
    EXPECT_EQ(1, updateCount());
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre("span[attr=\"value\"]"));
}

TEST_F(WebFrameCSSCallbackTest, DisplayNone)
{
    loadHTML("<div style='display:none'><span></span></div>");

    std::vector<WebString> selectors;
    selectors.push_back(WebString::fromUTF8("span"));
    doc().watchCSSSelectors(WebVector<WebString>(selectors));
    runPendingTasks();

    EXPECT_EQ(0, updateCount()) << "Don't match elements in display:none trees.";

    executeScript(
        "d = document.querySelector('div');"
        "d.style.display = 'block';");
    EXPECT_EQ(1, updateCount()) << "Match elements when they become displayed.";
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre("span"));

    executeScript(
        "d = document.querySelector('div');"
        "d.style.display = 'none';");
    EXPECT_EQ(2, updateCount()) << "Unmatch elements when they become undisplayed.";
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre());

    executeScript(
        "s = document.querySelector('span');"
        "s.style.display = 'none';");
    EXPECT_EQ(2, updateCount()) << "No effect from no-display'ing a span that's already undisplayed.";

    executeScript(
        "d = document.querySelector('div');"
        "d.style.display = 'block';");
    EXPECT_EQ(2, updateCount()) << "No effect from displaying a div whose span is display:none.";

    executeScript(
        "s = document.querySelector('span');"
        "s.style.display = 'inline';");
    EXPECT_EQ(3, updateCount()) << "Now the span is visible and produces a callback.";
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre("span"));

    executeScript(
        "s = document.querySelector('span');"
        "s.style.display = 'none';");
    EXPECT_EQ(4, updateCount()) << "Undisplaying the span directly should produce another callback.";
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre());
}

TEST_F(WebFrameCSSCallbackTest, Reparenting)
{
    loadHTML(
        "<div id='d1'><span></span></div>"
        "<div id='d2'></div>");

    std::vector<WebString> selectors;
    selectors.push_back(WebString::fromUTF8("span"));
    doc().watchCSSSelectors(WebVector<WebString>(selectors));
    m_frame->view()->layout();
    runPendingTasks();

    EXPECT_EQ(1, updateCount());
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre("span"));

    executeScript(
        "s = document.querySelector('span');"
        "d2 = document.getElementById('d2');"
        "d2.appendChild(s);");
    EXPECT_EQ(1, updateCount()) << "Just moving an element that continues to match shouldn't send a spurious callback.";
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre("span"));
}

TEST_F(WebFrameCSSCallbackTest, MultiSelector)
{
    loadHTML("<span></span>");

    // Check that selector lists match as the whole list, not as each element
    // independently.
    std::vector<WebString> selectors;
    selectors.push_back(WebString::fromUTF8("span"));
    selectors.push_back(WebString::fromUTF8("span,p"));
    doc().watchCSSSelectors(WebVector<WebString>(selectors));
    m_frame->view()->layout();
    runPendingTasks();

    EXPECT_EQ(1, updateCount());
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre("span", "span, p"));
}

TEST_F(WebFrameCSSCallbackTest, InvalidSelector)
{
    loadHTML("<p><span></span></p>");

    // Build a list with one valid selector and one invalid.
    std::vector<WebString> selectors;
    selectors.push_back(WebString::fromUTF8("span"));
    selectors.push_back(WebString::fromUTF8("[")); // Invalid.
    selectors.push_back(WebString::fromUTF8("p span")); // Not compound.
    doc().watchCSSSelectors(WebVector<WebString>(selectors));
    m_frame->view()->layout();
    runPendingTasks();

    EXPECT_EQ(1, updateCount());
    EXPECT_THAT(matchedSelectors(), testing::ElementsAre("span"))
        << "An invalid selector shouldn't prevent other selectors from matching.";
}

TEST_F(WebFrameTest, DispatchMessageEventWithOriginCheck)
{
    registerMockedHttpURLLoad("postmessage_test.html");

    // Pass true to enable JavaScript.
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "postmessage_test.html", true);

    // Send a message with the correct origin.
    WebSecurityOrigin correctOrigin(WebSecurityOrigin::create(toKURL(m_baseURL)));
    WebDOMEvent event = webViewHelper.webView()->mainFrame()->document().createEvent("MessageEvent");
    WebDOMMessageEvent message = event.to<WebDOMMessageEvent>();
    WebSerializedScriptValue data(WebSerializedScriptValue::fromString("foo"));
    message.initMessageEvent("message", false, false, data, "http://origin.com", 0, "");
    webViewHelper.webView()->mainFrame()->dispatchMessageEventWithOriginCheck(correctOrigin, message);

    // Send another message with incorrect origin.
    WebSecurityOrigin incorrectOrigin(WebSecurityOrigin::create(toKURL(m_chromeURL)));
    webViewHelper.webView()->mainFrame()->dispatchMessageEventWithOriginCheck(incorrectOrigin, message);

    // Required to see any updates in contentAsText.
    webViewHelper.webView()->layout();

    // Verify that only the first addition is in the body of the page.
    std::string content = webViewHelper.webView()->mainFrame()->contentAsText(1024).utf8();
    EXPECT_NE(std::string::npos, content.find("Message 1."));
    EXPECT_EQ(std::string::npos, content.find("Message 2."));
}

TEST_F(WebFrameTest, PostMessageThenDetach)
{
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad("about:blank");

    RefPtr<WebCore::LocalFrame> frame = webViewHelper.webViewImpl()->page()->mainFrame();
    WebCore::NonThrowableExceptionState exceptionState;
    frame->domWindow()->postMessage(WebCore::SerializedScriptValue::create("message"), 0, "*", frame->domWindow(), exceptionState);
    webViewHelper.reset();
    EXPECT_FALSE(exceptionState.hadException());

    // Success is not crashing.
    runPendingTasks();
}

class FixedLayoutTestWebViewClient : public WebViewClient {
 public:
    virtual WebScreenInfo screenInfo() OVERRIDE { return m_screenInfo; }

    WebScreenInfo m_screenInfo;
};

// Viewport settings need to be set before the page gets loaded
static void enableViewportSettings(WebSettings* settings)
{
    settings->setViewportMetaEnabled(true);
    settings->setViewportEnabled(true);
    settings->setMainFrameResizesAreOrientationChanges(true);
}

TEST_F(WebFrameTest, FrameViewNeedsLayoutOnFixedLayoutResize)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    int viewportWidth = 640;
    int viewportHeight = 480;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    webViewHelper.webViewImpl()->setFixedLayoutSize(WebCore::IntSize(100, 100));
    EXPECT_TRUE(webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->needsLayout());

    int prevLayoutCount = webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutCount();
    webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->setFrameRect(WebCore::IntRect(0, 0, 641, 481));
    EXPECT_EQ(prevLayoutCount, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutCount());

    webViewHelper.webViewImpl()->layout();
}

// Helper function to check or set text autosizing multipliers on a document.
static bool checkOrSetTextAutosizingMultiplier(Document* document, float multiplier, bool setMultiplier)
{
    bool multiplierCheckedOrSetAtLeastOnce = false;
    for (WebCore::RenderObject* renderer = document->renderer(); renderer; renderer = renderer->nextInPreOrder()) {
        if (renderer->style()) {
            if (setMultiplier)
                renderer->style()->setTextAutosizingMultiplier(multiplier);
            EXPECT_EQ(multiplier, renderer->style()->textAutosizingMultiplier());
            multiplierCheckedOrSetAtLeastOnce = true;
        }
    }
    return multiplierCheckedOrSetAtLeastOnce;

}

static bool setTextAutosizingMultiplier(Document* document, float multiplier)
{
    return checkOrSetTextAutosizingMultiplier(document, multiplier, true);
}

static bool checkTextAutosizingMultiplier(Document* document, float multiplier)
{
    return checkOrSetTextAutosizingMultiplier(document, multiplier, false);
}

TEST_F(WebFrameTest, ChangeInFixedLayoutResetsTextAutosizingMultipliers)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client, enableViewportSettings);

    WebCore::Document* document = webViewHelper.webViewImpl()->page()->mainFrame()->document();
    document->settings()->setTextAutosizingEnabled(true);
    EXPECT_TRUE(document->settings()->textAutosizingEnabled());
    webViewHelper.webViewImpl()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webViewImpl()->layout();

    EXPECT_TRUE(setTextAutosizingMultiplier(document, 2));

    WebCore::ViewportDescription description = document->viewportDescription();
    // Choose a width that's not going match the viewport width of the loaded document.
    description.minWidth = WebCore::Length(100, WebCore::Fixed);
    description.maxWidth = WebCore::Length(100, WebCore::Fixed);
    webViewHelper.webViewImpl()->updatePageDefinedViewportConstraints(description);

    EXPECT_TRUE(checkTextAutosizingMultiplier(document, 1));
}

TEST_F(WebFrameTest, SetFrameRectResetsTextAutosizingMultipliers)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("iframe_reload.html");
    registerMockedHttpURLLoad("visible_iframe.html");

    FixedLayoutTestWebViewClient client;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "iframe_reload.html", true, 0, &client, enableViewportSettings);

    WebCore::LocalFrame* mainFrame = webViewHelper.webViewImpl()->page()->mainFrame();
    WebCore::Document* document = mainFrame->document();
    WebCore::FrameView* frameView = webViewHelper.webViewImpl()->mainFrameImpl()->frameView();
    document->settings()->setTextAutosizingEnabled(true);
    EXPECT_TRUE(document->settings()->textAutosizingEnabled());
    webViewHelper.webViewImpl()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webViewImpl()->layout();

    for (WebCore::LocalFrame* frame = mainFrame; frame; frame = frame->tree().traverseNext())
        EXPECT_TRUE(setTextAutosizingMultiplier(frame->document(), 2));

    frameView->setFrameRect(WebCore::IntRect(0, 0, 200, 200));

    for (WebCore::LocalFrame* frame = mainFrame; frame; frame = frame->tree().traverseNext())
        EXPECT_TRUE(checkTextAutosizingMultiplier(frame->document(), 1));
}

TEST_F(WebFrameTest, FixedLayoutSizeStopsResizeFromChangingLayoutSize)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("fixed_layout.html");

    int viewportWidth = 640;
    int viewportHeight = 480;

    int fixedLayoutWidth = viewportWidth / 2;
    int fixedLayoutHeight = viewportHeight / 2;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true, 0, 0, enableViewportSettings);
    webViewHelper.webView()->setFixedLayoutSize(WebSize(fixedLayoutWidth, fixedLayoutHeight));
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    EXPECT_EQ(fixedLayoutWidth, webViewHelper.webViewImpl()->page()->mainFrame()->view()->layoutSize().width());
    EXPECT_EQ(fixedLayoutHeight, webViewHelper.webViewImpl()->page()->mainFrame()->view()->layoutSize().height());
}

TEST_F(WebFrameTest, FixedLayoutSizePreventsResizeFromChangingPageScale)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("fixed_layout.html");

    int viewportWidth = 640;
    int viewportHeight = 480;

    int fixedLayoutWidth = viewportWidth / 2;
    int fixedLayoutHeight = viewportHeight / 2;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true, 0, 0, enableViewportSettings);
    webViewHelper.webView()->setFixedLayoutSize(WebSize(fixedLayoutWidth, fixedLayoutHeight));
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();
    float pageScaleFactor = webViewHelper.webView()->pageScaleFactor();

    webViewHelper.webView()->resize(WebSize(viewportWidth * 2, viewportHeight * 2));

    EXPECT_EQ(pageScaleFactor, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, FixedLayoutSizePreventsLayoutFromChangingPageScale)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("fixed_layout.html");

    int viewportWidth = 640;
    int viewportHeight = 480;

    int fixedLayoutWidth = viewportWidth * 2;
    int fixedLayoutHeight = viewportHeight * 2;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true, 0, 0, enableViewportSettings);
    webViewHelper.webView()->setFixedLayoutSize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();
    float pageScaleFactor = webViewHelper.webView()->pageScaleFactor();

    webViewHelper.webView()->setFixedLayoutSize(WebSize(fixedLayoutWidth, fixedLayoutHeight));
    webViewHelper.webView()->layout();

    EXPECT_EQ(pageScaleFactor, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, PreferredSizeAndContentSizeReportedCorrectlyWithZeroHeightFixedLayout)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("200-by-300.html");

    int windowWidth = 100;
    int windowHeight = 100;
    int viewportWidth = 100;
    int viewportHeight = 0;
    int divWidth = 200;
    int divHeight = 300;

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "200-by-300.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(windowWidth, windowHeight));
    webViewHelper.webView()->setFixedLayoutSize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    EXPECT_EQ(divWidth, webViewHelper.webView()->mainFrame()->contentsSize().width);
    EXPECT_EQ(divHeight, webViewHelper.webView()->mainFrame()->contentsSize().height);

    EXPECT_EQ(divWidth, webViewHelper.webView()->contentsPreferredMinimumSize().width);
    EXPECT_EQ(divHeight, webViewHelper.webView()->contentsPreferredMinimumSize().height);
}

TEST_F(WebFrameTest, DisablingFixedLayoutSizeSetsCorrectLayoutSize)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("no_viewport_tag.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "no_viewport_tag.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setSupportDeprecatedTargetDensityDPI(true);
    webViewHelper.webView()->settings()->setUseWideViewport(true);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    webViewHelper.webView()->setFixedLayoutSize(WebSize(viewportWidth, viewportHeight));
    EXPECT_TRUE(webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->needsLayout());
    webViewHelper.webView()->layout();
    EXPECT_EQ(viewportWidth, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->contentsSize().width());

    webViewHelper.webView()->setFixedLayoutSize(WebSize(0, 0));
    EXPECT_TRUE(webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->needsLayout());
    webViewHelper.webView()->layout();
    EXPECT_EQ(980, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->contentsSize().width());
}

TEST_F(WebFrameTest, ZeroHeightPositiveWidthNotIgnored)
{
    UseMockScrollbarSettings mockScrollbarSettings;

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 1280;
    int viewportHeight = 0;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize(true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(viewportWidth, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width());
    EXPECT_EQ(viewportHeight, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().height());
}

TEST_F(WebFrameTest, DeviceScaleFactorUsesDefaultWithoutViewportTag)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("no_viewport_tag.html");

    int viewportWidth = 640;
    int viewportHeight = 480;

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 2;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "no_viewport_tag.html", true, 0, &client, enableViewportSettings);

    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    EXPECT_EQ(2, webViewHelper.webView()->deviceScaleFactor());

    // Device scale factor should be independent of page scale.
    webViewHelper.webView()->setPageScaleFactorLimits(1, 2);
    webViewHelper.webView()->setPageScaleFactorPreservingScrollOffset(0.5);
    webViewHelper.webView()->layout();
    EXPECT_EQ(1, webViewHelper.webView()->pageScaleFactor());

    // Force the layout to happen before leaving the test.
    webViewHelper.webView()->mainFrame()->contentAsText(1024).utf8();
}

TEST_F(WebFrameTest, FixedLayoutInitializeAtMinimumScale)
{
    UseMockScrollbarSettings mockScrollbarSettings;

    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    int defaultFixedLayoutWidth = 980;
    float minimumPageScaleFactor = viewportWidth / (float) defaultFixedLayoutWidth;
    EXPECT_EQ(minimumPageScaleFactor, webViewHelper.webView()->pageScaleFactor());
    EXPECT_EQ(minimumPageScaleFactor, webViewHelper.webView()->minimumPageScaleFactor());

    // Assume the user has pinch zoomed to page scale factor 2.
    float userPinchPageScaleFactor = 2;
    webViewHelper.webView()->setPageScaleFactorPreservingScrollOffset(userPinchPageScaleFactor);
    webViewHelper.webView()->layout();

    // Make sure we don't reset to initial scale if the page continues to load.
    webViewHelper.webViewImpl()->didCommitLoad(false, false);
    webViewHelper.webViewImpl()->didChangeContentsSize();
    EXPECT_EQ(userPinchPageScaleFactor, webViewHelper.webView()->pageScaleFactor());

    // Make sure we don't reset to initial scale if the viewport size changes.
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight + 100));
    EXPECT_EQ(userPinchPageScaleFactor, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, WideDocumentInitializeAtMinimumScale)
{
    UseMockScrollbarSettings mockScrollbarSettings;

    registerMockedHttpURLLoad("wide_document.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "wide_document.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    int wideDocumentWidth = 1500;
    float minimumPageScaleFactor = viewportWidth / (float) wideDocumentWidth;
    EXPECT_EQ(minimumPageScaleFactor, webViewHelper.webView()->pageScaleFactor());
    EXPECT_EQ(minimumPageScaleFactor, webViewHelper.webView()->minimumPageScaleFactor());

    // Assume the user has pinch zoomed to page scale factor 2.
    float userPinchPageScaleFactor = 2;
    webViewHelper.webView()->setPageScaleFactorPreservingScrollOffset(userPinchPageScaleFactor);
    webViewHelper.webView()->layout();

    // Make sure we don't reset to initial scale if the page continues to load.
    webViewHelper.webViewImpl()->didCommitLoad(false, false);
    webViewHelper.webViewImpl()->didChangeContentsSize();
    EXPECT_EQ(userPinchPageScaleFactor, webViewHelper.webView()->pageScaleFactor());

    // Make sure we don't reset to initial scale if the viewport size changes.
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight + 100));
    EXPECT_EQ(userPinchPageScaleFactor, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, DelayedViewportInitialScale)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-auto-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-auto-initial-scale.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(0.25f, webViewHelper.webView()->pageScaleFactor());

    WebCore::Document* document = webViewHelper.webViewImpl()->page()->mainFrame()->document();
    WebCore::ViewportDescription description = document->viewportDescription();
    description.zoom = 2;
    document->setViewportDescription(description);
    webViewHelper.webView()->layout();
    EXPECT_EQ(2, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, setLoadWithOverviewModeToFalse)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-auto-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-auto-initial-scale.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setLoadWithOverviewMode(false);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    // The page must be displayed at 100% zoom.
    EXPECT_EQ(1.0f, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, SetLoadWithOverviewModeToFalseAndNoWideViewport)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("large-div.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "large-div.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setLoadWithOverviewMode(false);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(false);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    // The page must be displayed at 100% zoom, despite that it hosts a wide div element.
    EXPECT_EQ(1.0f, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, NoWideViewportIgnoresPageViewportWidth)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-auto-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-auto-initial-scale.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(false);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    // The page sets viewport width to 3000, but with UseWideViewport == false is must be ignored.
    EXPECT_EQ(viewportWidth, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->contentsSize().width());
    EXPECT_EQ(viewportHeight, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->contentsSize().height());
}

TEST_F(WebFrameTest, NoWideViewportIgnoresPageViewportWidthButAccountsScale)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-wide-2x-initial-scale.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(false);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    // The page sets viewport width to 3000, but with UseWideViewport == false it must be ignored.
    // While the initial scale specified by the page must be accounted.
    EXPECT_EQ(viewportWidth / 2, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->contentsSize().width());
    EXPECT_EQ(viewportHeight / 2, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->contentsSize().height());
}

TEST_F(WebFrameTest, WideViewportSetsTo980WithoutViewportTag)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("no_viewport_tag.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "no_viewport_tag.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(true);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(980, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->contentsSize().width());
    EXPECT_EQ(980.0 / viewportWidth * viewportHeight, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->contentsSize().height());
}

TEST_F(WebFrameTest, NoWideViewportAndHeightInMeta)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-height-1000.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-height-1000.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(false);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(viewportWidth, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->contentsSize().width());
}

TEST_F(WebFrameTest, WideViewportSetsTo980WithAutoWidth)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-2x-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-2x-initial-scale.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(true);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(980, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->contentsSize().width());
    EXPECT_EQ(980.0 / viewportWidth * viewportHeight, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->contentsSize().height());
}

TEST_F(WebFrameTest, PageViewportInitialScaleOverridesLoadWithOverviewMode)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-wide-2x-initial-scale.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setLoadWithOverviewMode(false);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    // The page must be displayed at 200% zoom, as specified in its viewport meta tag.
    EXPECT_EQ(2.0f, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, setInitialPageScaleFactorPermanently)
{
    UseMockScrollbarSettings mockScrollbarSettings;

    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    float enforcedPageScaleFactor = 2.0f;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setLoadWithOverviewMode(false);
    webViewHelper.webView()->setInitialPageScaleOverride(enforcedPageScaleFactor);
    webViewHelper.webView()->layout();

    EXPECT_EQ(enforcedPageScaleFactor, webViewHelper.webView()->pageScaleFactor());

    int viewportWidth = 640;
    int viewportHeight = 480;
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    EXPECT_EQ(enforcedPageScaleFactor, webViewHelper.webView()->pageScaleFactor());

    webViewHelper.webView()->setInitialPageScaleOverride(-1);
    webViewHelper.webView()->layout();
    EXPECT_EQ(1.0, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, PermanentInitialPageScaleFactorOverridesLoadWithOverviewMode)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-auto-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;
    float enforcedPageScaleFactor = 0.5f;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-auto-initial-scale.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setLoadWithOverviewMode(false);
    webViewHelper.webView()->setInitialPageScaleOverride(enforcedPageScaleFactor);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(enforcedPageScaleFactor, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, PermanentInitialPageScaleFactorOverridesPageViewportInitialScale)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;
    float enforcedPageScaleFactor = 0.5f;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-wide-2x-initial-scale.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->setInitialPageScaleOverride(enforcedPageScaleFactor);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(enforcedPageScaleFactor, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, SmallPermanentInitialPageScaleFactorIsClobbered)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    const char* pages[] = {
        // These pages trigger the clobbering condition. There must be a matching item in "pageScaleFactors" array.
        "viewport-device-0.5x-initial-scale.html",
        "viewport-initial-scale-1.html",
        // These ones do not.
        "viewport-auto-initial-scale.html",
        "viewport-target-densitydpi-device-and-fixed-width.html"
    };
    float pageScaleFactors[] = { 0.5f, 1.0f };
    for (size_t i = 0; i < ARRAY_SIZE(pages); ++i)
        registerMockedHttpURLLoad(pages[i]);

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 400;
    int viewportHeight = 300;
    float enforcedPageScaleFactor = 0.75f;

    for (size_t i = 0; i < ARRAY_SIZE(pages); ++i) {
        for (int quirkEnabled = 0; quirkEnabled <= 1; ++quirkEnabled) {
            FrameTestHelpers::WebViewHelper webViewHelper;
            webViewHelper.initializeAndLoad(m_baseURL + pages[i], true, 0, &client, enableViewportSettings);
            webViewHelper.webView()->settings()->setClobberUserAgentInitialScaleQuirk(quirkEnabled);
            webViewHelper.webView()->setInitialPageScaleOverride(enforcedPageScaleFactor);
            webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

            float expectedPageScaleFactor = quirkEnabled && i < ARRAY_SIZE(pageScaleFactors) ? pageScaleFactors[i] : enforcedPageScaleFactor;
            EXPECT_EQ(expectedPageScaleFactor, webViewHelper.webView()->pageScaleFactor());
        }
    }
}

TEST_F(WebFrameTest, PermanentInitialPageScaleFactorAffectsLayoutWidth)
{
    UseMockScrollbarSettings mockScrollbarSettings;

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;
    float enforcedPageScaleFactor = 0.5;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad("about:blank", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(false);
    webViewHelper.webView()->settings()->setLoadWithOverviewMode(false);
    webViewHelper.webView()->setInitialPageScaleOverride(enforcedPageScaleFactor);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(viewportWidth / enforcedPageScaleFactor, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->contentsSize().width());
    EXPECT_EQ(enforcedPageScaleFactor, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, WideViewportInitialScaleDoesNotExpandFixedLayoutWidth)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-device-0.5x-initial-scale.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-device-0.5x-initial-scale.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(true);
    webViewHelper.webView()->settings()->setViewportMetaLayoutSizeQuirk(true);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(viewportWidth, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width());
    EXPECT_EQ(1, webViewHelper.webView()->pageScaleFactor());

    webViewHelper.webView()->setFixedLayoutSize(WebSize(2000, 1500));
    webViewHelper.webView()->layout();
    EXPECT_EQ(2000, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width());
    EXPECT_EQ(0.5f, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, WideViewportAndWideContentWithInitialScale)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("wide_document_width_viewport.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 600;
    int viewportHeight = 800;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad("about:blank", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(true);
    webViewHelper.webView()->settings()->setViewportMetaLayoutSizeQuirk(true);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), m_baseURL + "wide_document_width_viewport.html");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    int wideDocumentWidth = 800;
    float minimumPageScaleFactor = viewportWidth / (float) wideDocumentWidth;
    EXPECT_EQ(minimumPageScaleFactor, webViewHelper.webView()->pageScaleFactor());
    EXPECT_EQ(minimumPageScaleFactor, webViewHelper.webView()->minimumPageScaleFactor());
}

TEST_F(WebFrameTest, WideViewportQuirkClobbersHeight)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-height-1000.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 600;
    int viewportHeight = 800;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad("about:blank", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(false);
    webViewHelper.webView()->settings()->setViewportMetaLayoutSizeQuirk(true);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), m_baseURL + "viewport-height-1000.html");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(800, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().height());
    EXPECT_EQ(1, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, LayoutSize320Quirk)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport/viewport-30.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 600;
    int viewportHeight = 800;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad("about:blank", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(true);
    webViewHelper.webView()->settings()->setViewportMetaLayoutSizeQuirk(true);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), m_baseURL + "viewport/viewport-30.html");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(600, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width());
    EXPECT_EQ(800, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().height());
    EXPECT_EQ(1, webViewHelper.webView()->pageScaleFactor());

    // The magic number to snap to device-width is 320, so test that 321 is
    // respected.
    WebCore::Document* document = webViewHelper.webViewImpl()->page()->mainFrame()->document();
    WebCore::ViewportDescription description = document->viewportDescription();
    description.minWidth = WebCore::Length(321, WebCore::Fixed);
    description.maxWidth = WebCore::Length(321, WebCore::Fixed);
    document->setViewportDescription(description);
    webViewHelper.webView()->layout();
    EXPECT_EQ(321, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width());

    description.minWidth = WebCore::Length(320, WebCore::Fixed);
    description.maxWidth = WebCore::Length(320, WebCore::Fixed);
    document->setViewportDescription(description);
    webViewHelper.webView()->layout();
    EXPECT_EQ(600, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width());

    description = document->viewportDescription();
    description.maxHeight = WebCore::Length(1000, WebCore::Fixed);
    document->setViewportDescription(description);
    webViewHelper.webView()->layout();
    EXPECT_EQ(1000, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().height());

    description.maxHeight = WebCore::Length(320, WebCore::Fixed);
    document->setViewportDescription(description);
    webViewHelper.webView()->layout();
    EXPECT_EQ(800, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().height());
}

TEST_F(WebFrameTest, ZeroValuesQuirk)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-zero-values.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize(true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setViewportMetaZeroValuesQuirk(true);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setViewportMetaLayoutSizeQuirk(true);
    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), m_baseURL + "viewport-zero-values.html");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(viewportWidth, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width());
    EXPECT_EQ(1.0f, webViewHelper.webView()->pageScaleFactor());

    webViewHelper.webView()->settings()->setUseWideViewport(true);
    webViewHelper.webView()->layout();
    EXPECT_EQ(viewportWidth, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width());
    EXPECT_EQ(1.0f, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, OverflowHiddenDisablesScrolling)
{
    registerMockedHttpURLLoad("body-overflow-hidden.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize(true, 0, &client);
    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), m_baseURL + "body-overflow-hidden.html");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    WebCore::FrameView* view = webViewHelper.webViewImpl()->mainFrameImpl()->frameView();
    EXPECT_FALSE(view->userInputScrollable(WebCore::VerticalScrollbar));
}

TEST_F(WebFrameTest, IgnoreOverflowHiddenQuirk)
{
    registerMockedHttpURLLoad("body-overflow-hidden.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize(true, 0, &client);
    webViewHelper.webView()->settings()->setIgnoreMainFrameOverflowHiddenQuirk(true);
    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), m_baseURL + "body-overflow-hidden.html");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    WebCore::FrameView* view = webViewHelper.webViewImpl()->mainFrameImpl()->frameView();
    EXPECT_TRUE(view->userInputScrollable(WebCore::VerticalScrollbar));
}

TEST_F(WebFrameTest, NonZeroValuesNoQuirk)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-nonzero-values.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;
    float expectedPageScaleFactor = 0.5f;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize(true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setViewportMetaZeroValuesQuirk(true);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), m_baseURL + "viewport-nonzero-values.html");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(viewportWidth / expectedPageScaleFactor, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width());
    EXPECT_EQ(expectedPageScaleFactor, webViewHelper.webView()->pageScaleFactor());

    webViewHelper.webView()->settings()->setUseWideViewport(true);
    webViewHelper.webView()->layout();
    EXPECT_EQ(viewportWidth / expectedPageScaleFactor, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width());
    EXPECT_EQ(expectedPageScaleFactor, webViewHelper.webView()->pageScaleFactor());
}

TEST_F(WebFrameTest, setPageScaleFactorDoesNotLayout)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    // Small viewport to ensure there are always scrollbars.
    int viewportWidth = 64;
    int viewportHeight = 48;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    int prevLayoutCount = webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutCount();
    webViewHelper.webViewImpl()->setPageScaleFactor(3, WebPoint());
    EXPECT_FALSE(webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->needsLayout());
    EXPECT_EQ(prevLayoutCount, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutCount());
}

TEST_F(WebFrameTest, setPageScaleFactorWithOverlayScrollbarsDoesNotLayout)
{
    UseMockScrollbarSettings mockScrollbarSettings;

    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    int prevLayoutCount = webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutCount();
    webViewHelper.webViewImpl()->setPageScaleFactor(30, WebPoint());
    EXPECT_FALSE(webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->needsLayout());
    EXPECT_EQ(prevLayoutCount, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutCount());

}

TEST_F(WebFrameTest, setPageScaleFactorBeforeFrameHasView)
{
    registerMockedHttpURLLoad("fixed_layout.html");

    float pageScaleFactor = 3;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad("about:html", true, 0, 0);
    webViewHelper.webView()->setPageScaleFactor(pageScaleFactor, WebPoint());

    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), m_baseURL + "fixed_layout.html");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    WebCore::FrameView* view = webViewHelper.webViewImpl()->mainFrameImpl()->frameView();
    EXPECT_EQ(pageScaleFactor, view->visibleContentScaleFactor());
}

TEST_F(WebFrameTest, pageScaleFactorWrittenToHistoryItem)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    webViewHelper.webView()->setPageScaleFactor(3, WebPoint());
    EXPECT_EQ(3, webViewHelper.webViewImpl()->page()->mainFrame()->loader().currentItem()->pageScaleFactor());
}

TEST_F(WebFrameTest, initialScaleWrittenToHistoryItem)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    int defaultFixedLayoutWidth = 980;
    float minimumPageScaleFactor = viewportWidth / (float) defaultFixedLayoutWidth;
    EXPECT_EQ(minimumPageScaleFactor, webViewHelper.webViewImpl()->page()->mainFrame()->loader().currentItem()->pageScaleFactor());
}

TEST_F(WebFrameTest, pageScaleFactorShrinksViewport)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("large-div.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    // Small viewport to ensure there are always scrollbars.
    int viewportWidth = 64;
    int viewportHeight = 48;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "large-div.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    WebCore::FrameView* view = webViewHelper.webViewImpl()->mainFrameImpl()->frameView();
    int viewportWidthMinusScrollbar = viewportWidth - (view->verticalScrollbar()->isOverlayScrollbar() ? 0 : 15);
    int viewportHeightMinusScrollbar = viewportHeight - (view->horizontalScrollbar()->isOverlayScrollbar() ? 0 : 15);

    webViewHelper.webView()->setPageScaleFactor(2, WebPoint());

    WebCore::IntSize unscaledSize = view->unscaledVisibleContentSize(WebCore::IncludeScrollbars);
    EXPECT_EQ(viewportWidth, unscaledSize.width());
    EXPECT_EQ(viewportHeight, unscaledSize.height());

    WebCore::IntSize unscaledSizeMinusScrollbar = view->unscaledVisibleContentSize(WebCore::ExcludeScrollbars);
    EXPECT_EQ(viewportWidthMinusScrollbar, unscaledSizeMinusScrollbar.width());
    EXPECT_EQ(viewportHeightMinusScrollbar, unscaledSizeMinusScrollbar.height());

    WebCore::IntSize scaledSize = view->visibleContentRect().size();
    EXPECT_EQ(ceil(viewportWidthMinusScrollbar / 2.0), scaledSize.width());
    EXPECT_EQ(ceil(viewportHeightMinusScrollbar / 2.0), scaledSize.height());
}

TEST_F(WebFrameTest, pageScaleFactorDoesNotApplyCssTransform)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    webViewHelper.webView()->setPageScaleFactor(2, WebPoint());

    EXPECT_EQ(980, webViewHelper.webViewImpl()->page()->mainFrame()->contentRenderer()->unscaledDocumentRect().width());
    EXPECT_EQ(980, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->contentsSize().width());
}

TEST_F(WebFrameTest, targetDensityDpiHigh)
{
    UseMockScrollbarSettings mockScrollbarSettings;
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

        FrameTestHelpers::WebViewHelper webViewHelper;
        webViewHelper.initializeAndLoad(m_baseURL + "viewport-target-densitydpi-high.html", true, 0, &client, enableViewportSettings);
        webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
        webViewHelper.webView()->settings()->setSupportDeprecatedTargetDensityDPI(true);
        webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

        // We need to account for the fact that logical pixels are unconditionally multiplied by deviceScaleFactor to produce
        // physical pixels.
        float densityDpiScaleRatio = deviceScaleFactor * targetDpi / deviceDpi;
        EXPECT_NEAR(viewportWidth * densityDpiScaleRatio, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width(), 1.0f);
        EXPECT_NEAR(viewportHeight * densityDpiScaleRatio, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().height(), 1.0f);
        EXPECT_NEAR(1.0f / densityDpiScaleRatio, webViewHelper.webView()->pageScaleFactor(), 0.01f);
    }
}

TEST_F(WebFrameTest, targetDensityDpiDevice)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-target-densitydpi-device.html");

    float deviceScaleFactors[] = { 1.0f, 4.0f / 3.0f, 2.0f };

    FixedLayoutTestWebViewClient client;
    int viewportWidth = 640;
    int viewportHeight = 480;

    for (size_t i = 0; i < ARRAY_SIZE(deviceScaleFactors); ++i) {
        client.m_screenInfo.deviceScaleFactor = deviceScaleFactors[i];

        FrameTestHelpers::WebViewHelper webViewHelper;
        webViewHelper.initializeAndLoad(m_baseURL + "viewport-target-densitydpi-device.html", true, 0, &client, enableViewportSettings);
        webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
        webViewHelper.webView()->settings()->setSupportDeprecatedTargetDensityDPI(true);
        webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

        EXPECT_NEAR(viewportWidth * client.m_screenInfo.deviceScaleFactor, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width(), 1.0f);
        EXPECT_NEAR(viewportHeight * client.m_screenInfo.deviceScaleFactor, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().height(), 1.0f);
        EXPECT_NEAR(1.0f / client.m_screenInfo.deviceScaleFactor, webViewHelper.webView()->pageScaleFactor(), 0.01f);
    }
}

TEST_F(WebFrameTest, targetDensityDpiDeviceAndFixedWidth)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-target-densitydpi-device-and-fixed-width.html");

    float deviceScaleFactors[] = { 1.0f, 4.0f / 3.0f, 2.0f };

    FixedLayoutTestWebViewClient client;
    int viewportWidth = 640;
    int viewportHeight = 480;

    for (size_t i = 0; i < ARRAY_SIZE(deviceScaleFactors); ++i) {
        client.m_screenInfo.deviceScaleFactor = deviceScaleFactors[i];

        FrameTestHelpers::WebViewHelper webViewHelper;
        webViewHelper.initializeAndLoad(m_baseURL + "viewport-target-densitydpi-device-and-fixed-width.html", true, 0, &client, enableViewportSettings);
        webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
        webViewHelper.webView()->settings()->setSupportDeprecatedTargetDensityDPI(true);
        webViewHelper.webView()->settings()->setUseWideViewport(true);
        webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

        EXPECT_NEAR(viewportWidth, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width(), 1.0f);
        EXPECT_NEAR(viewportHeight, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().height(), 1.0f);
        EXPECT_NEAR(1.0f, webViewHelper.webView()->pageScaleFactor(), 0.01f);
    }
}

TEST_F(WebFrameTest, NoWideViewportAndScaleLessThanOne)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-initial-scale-less-than-1.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1.33f;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-initial-scale-less-than-1.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setSupportDeprecatedTargetDensityDPI(true);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(false);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    EXPECT_NEAR(viewportWidth * client.m_screenInfo.deviceScaleFactor, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width(), 1.0f);
    EXPECT_NEAR(viewportHeight * client.m_screenInfo.deviceScaleFactor, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().height(), 1.0f);
    EXPECT_NEAR(1.0f / client.m_screenInfo.deviceScaleFactor, webViewHelper.webView()->pageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest, NoWideViewportAndScaleLessThanOneWithDeviceWidth)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-initial-scale-less-than-1-device-width.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1.33f;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-initial-scale-less-than-1-device-width.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setSupportDeprecatedTargetDensityDPI(true);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(false);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    const float pageZoom = 0.25f;
    EXPECT_NEAR(viewportWidth * client.m_screenInfo.deviceScaleFactor / pageZoom, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width(), 1.0f);
    EXPECT_NEAR(viewportHeight * client.m_screenInfo.deviceScaleFactor / pageZoom, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().height(), 1.0f);
    EXPECT_NEAR(1.0f / client.m_screenInfo.deviceScaleFactor, webViewHelper.webView()->pageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest, NoWideViewportAndNoViewportWithInitialPageScaleOverride)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("large-div.html");

    FixedLayoutTestWebViewClient client;
    int viewportWidth = 640;
    int viewportHeight = 480;
    float enforcedPageScaleFactor = 5.0f;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "large-div.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(false);
    webViewHelper.webView()->setInitialPageScaleOverride(enforcedPageScaleFactor);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    EXPECT_NEAR(viewportWidth / enforcedPageScaleFactor, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width(), 1.0f);
    EXPECT_NEAR(viewportHeight / enforcedPageScaleFactor, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().height(), 1.0f);
    EXPECT_NEAR(enforcedPageScaleFactor, webViewHelper.webView()->pageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest, NoUserScalableQuirkIgnoresViewportScale)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-initial-scale-and-user-scalable-no.html");

    FixedLayoutTestWebViewClient client;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-initial-scale-and-user-scalable-no.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setViewportMetaNonUserScalableQuirk(true);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    EXPECT_NEAR(viewportWidth, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width(), 1.0f);
    EXPECT_NEAR(viewportHeight, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().height(), 1.0f);
    EXPECT_NEAR(1.0f, webViewHelper.webView()->pageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest, NoUserScalableQuirkIgnoresViewportScaleForNonWideViewport)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-initial-scale-and-user-scalable-no.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1.33f;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-initial-scale-and-user-scalable-no.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setSupportDeprecatedTargetDensityDPI(true);
    webViewHelper.webView()->settings()->setViewportMetaNonUserScalableQuirk(true);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(false);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    EXPECT_NEAR(viewportWidth * client.m_screenInfo.deviceScaleFactor, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width(), 1.0f);
    EXPECT_NEAR(viewportHeight * client.m_screenInfo.deviceScaleFactor, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().height(), 1.0f);
    EXPECT_NEAR(1.0f / client.m_screenInfo.deviceScaleFactor, webViewHelper.webView()->pageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest, NoUserScalableQuirkIgnoresViewportScaleForWideViewport)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("viewport-2x-initial-scale-non-user-scalable.html");

    FixedLayoutTestWebViewClient client;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "viewport-2x-initial-scale-non-user-scalable.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setViewportMetaNonUserScalableQuirk(true);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(true);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_NEAR(viewportWidth, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().width(), 1.0f);
    EXPECT_NEAR(viewportHeight, webViewHelper.webViewImpl()->mainFrameImpl()->frameView()->layoutSize().height(), 1.0f);
    EXPECT_NEAR(1.0f, webViewHelper.webView()->pageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest, DesktopPageCanBeZoomedInWhenWideViewportIsTurnedOff)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("no_viewport_tag.html");

    FixedLayoutTestWebViewClient client;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "no_viewport_tag.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setUseWideViewport(false);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_NEAR(1.0f, webViewHelper.webView()->pageScaleFactor(), 0.01f);
    EXPECT_NEAR(1.0f, webViewHelper.webView()->minimumPageScaleFactor(), 0.01f);
    EXPECT_NEAR(5.0f, webViewHelper.webView()->maximumPageScaleFactor(), 0.01f);
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
        UseMockScrollbarSettings mockScrollbarSettings;
        registerMockedHttpURLLoad(url);

        const float aspectRatio = static_cast<float>(viewportSize.width) / viewportSize.height;

        FrameTestHelpers::WebViewHelper webViewHelper;
        webViewHelper.initializeAndLoad(m_baseURL + url, true, 0, 0, enableViewportSettings);

        // Origin scrollOffsets preserved under resize.
        {
            webViewHelper.webViewImpl()->resize(WebSize(viewportSize.width, viewportSize.height));
            webViewHelper.webViewImpl()->setPageScaleFactor(initialPageScaleFactor, WebPoint());
            ASSERT_EQ(viewportSize, webViewHelper.webViewImpl()->size());
            ASSERT_EQ(initialPageScaleFactor, webViewHelper.webViewImpl()->pageScaleFactor());
            webViewHelper.webViewImpl()->resize(WebSize(viewportSize.height, viewportSize.width));
            float expectedPageScaleFactor = initialPageScaleFactor * (shouldScaleRelativeToViewportWidth ? 1 / aspectRatio : 1);
            EXPECT_NEAR(expectedPageScaleFactor, webViewHelper.webViewImpl()->pageScaleFactor(), 0.05f);
            EXPECT_EQ(WebSize(), webViewHelper.webViewImpl()->mainFrame()->scrollOffset());
        }

        // Resizing just the height should not affect pageScaleFactor or scrollOffset.
        {
            webViewHelper.webViewImpl()->resize(WebSize(viewportSize.width, viewportSize.height));
            webViewHelper.webViewImpl()->setPageScaleFactor(initialPageScaleFactor, WebPoint(scrollOffset.width, scrollOffset.height));
            webViewHelper.webViewImpl()->layout();
            const WebSize expectedScrollOffset = webViewHelper.webViewImpl()->mainFrame()->scrollOffset();
            webViewHelper.webViewImpl()->resize(WebSize(viewportSize.width, viewportSize.height * 0.8f));
            EXPECT_EQ(initialPageScaleFactor, webViewHelper.webViewImpl()->pageScaleFactor());
            EXPECT_EQ(expectedScrollOffset, webViewHelper.webViewImpl()->mainFrame()->scrollOffset());
            webViewHelper.webViewImpl()->resize(WebSize(viewportSize.width, viewportSize.height * 0.8f));
            EXPECT_EQ(initialPageScaleFactor, webViewHelper.webViewImpl()->pageScaleFactor());
            EXPECT_EQ(expectedScrollOffset, webViewHelper.webViewImpl()->mainFrame()->scrollOffset());
        }

        // Generic resize preserves scrollOffset relative to anchor node located
        // the top center of the screen.
        {
            webViewHelper.webViewImpl()->resize(WebSize(viewportSize.height, viewportSize.width));
            float pageScaleFactor = webViewHelper.webViewImpl()->pageScaleFactor();
            webViewHelper.webViewImpl()->resize(WebSize(viewportSize.width, viewportSize.height));
            float expectedPageScaleFactor = pageScaleFactor * (shouldScaleRelativeToViewportWidth ? aspectRatio : 1);
            EXPECT_NEAR(expectedPageScaleFactor, webViewHelper.webViewImpl()->pageScaleFactor(), 0.05f);
            webViewHelper.webViewImpl()->mainFrame()->setScrollOffset(scrollOffset);

            WebCore::IntPoint anchorPoint = WebCore::IntPoint(scrollOffset) + WebCore::IntPoint(viewportSize.width / 2, 0);
            RefPtr<WebCore::Node> anchorNode = webViewHelper.webViewImpl()->mainFrameImpl()->frame()->eventHandler().hitTestResultAtPoint(anchorPoint, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent).innerNode();
            ASSERT(anchorNode);

            pageScaleFactor = webViewHelper.webViewImpl()->pageScaleFactor();
            const WebCore::FloatSize preResizeRelativeOffset
                = computeRelativeOffset(anchorPoint, anchorNode->boundingBox());
            webViewHelper.webViewImpl()->resize(WebSize(viewportSize.height, viewportSize.width));
            WebCore::IntPoint newAnchorPoint = WebCore::IntPoint(webViewHelper.webViewImpl()->mainFrame()->scrollOffset()) + WebCore::IntPoint(viewportSize.height / 2, 0);
            const WebCore::FloatSize postResizeRelativeOffset
                = computeRelativeOffset(newAnchorPoint, anchorNode->boundingBox());
            EXPECT_NEAR(preResizeRelativeOffset.width(), postResizeRelativeOffset.width(), 0.15f);
            expectedPageScaleFactor = pageScaleFactor * (shouldScaleRelativeToViewportWidth ? 1 / aspectRatio : 1);
            EXPECT_NEAR(expectedPageScaleFactor, webViewHelper.webViewImpl()->pageScaleFactor(), 0.05f);
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
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("large-div.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 50;
    int viewportHeight = 50;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "large-div.html", true, 0, &client);
    // FIXME: This test breaks if the viewport is enabled before loading the page due to the paint
    // calls below not working on composited layers. For some reason, enabling the viewport here
    // doesn't cause compositing
    webViewHelper.webView()->settings()->setViewportEnabled(true);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    // Set <1 page scale so that the clip rect should be larger than
    // the viewport size as passed into resize().
    webViewHelper.webView()->setPageScaleFactor(0.5, WebPoint());

    SkBitmap bitmap;
    ASSERT_TRUE(bitmap.allocN32Pixels(200, 200));
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    WebCore::GraphicsContext context(&canvas);
    context.setTrackOpaqueRegion(true);

    EXPECT_EQ_RECT(WebCore::IntRect(0, 0, 0, 0), context.opaqueRegion().asRect());

    WebCore::FrameView* view = webViewHelper.webViewImpl()->mainFrameImpl()->frameView();
    WebCore::IntRect paintRect(0, 0, 200, 200);
    view->paint(&context, paintRect);

    int viewportWidthMinusScrollbar = 50 - (view->verticalScrollbar()->isOverlayScrollbar() ? 0 : 15);
    int viewportHeightMinusScrollbar = 50 - (view->horizontalScrollbar()->isOverlayScrollbar() ? 0 : 15);
    WebCore::IntRect clippedRect(0, 0, viewportWidthMinusScrollbar * 2, viewportHeightMinusScrollbar * 2);
    EXPECT_EQ_RECT(clippedRect, context.opaqueRegion().asRect());
}

TEST_F(WebFrameTest, pageScaleFactorUpdatesScrollbars)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    WebCore::FrameView* view = webViewHelper.webViewImpl()->mainFrameImpl()->frameView();
    EXPECT_EQ(view->scrollSize(WebCore::HorizontalScrollbar), view->contentsSize().width() - view->visibleContentRect().width());
    EXPECT_EQ(view->scrollSize(WebCore::VerticalScrollbar), view->contentsSize().height() - view->visibleContentRect().height());

    webViewHelper.webView()->setPageScaleFactor(10, WebPoint());

    EXPECT_EQ(view->scrollSize(WebCore::HorizontalScrollbar), view->contentsSize().width() - view->visibleContentRect().width());
    EXPECT_EQ(view->scrollSize(WebCore::VerticalScrollbar), view->contentsSize().height() - view->visibleContentRect().height());
}

TEST_F(WebFrameTest, CanOverrideScaleLimits)
{
    UseMockScrollbarSettings mockScrollbarSettings;

    registerMockedHttpURLLoad("no_scale_for_you.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "no_scale_for_you.html", true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(2.0f, webViewHelper.webView()->minimumPageScaleFactor());
    EXPECT_EQ(2.0f, webViewHelper.webView()->maximumPageScaleFactor());

    webViewHelper.webView()->setIgnoreViewportTagScaleLimits(true);
    webViewHelper.webView()->layout();

    EXPECT_EQ(1.0f, webViewHelper.webView()->minimumPageScaleFactor());
    EXPECT_EQ(5.0f, webViewHelper.webView()->maximumPageScaleFactor());

    webViewHelper.webView()->setIgnoreViewportTagScaleLimits(false);
    webViewHelper.webView()->layout();

    EXPECT_EQ(2.0f, webViewHelper.webView()->minimumPageScaleFactor());
    EXPECT_EQ(2.0f, webViewHelper.webView()->maximumPageScaleFactor());
}

TEST_F(WebFrameTest, updateOverlayScrollbarLayers)
{
    UseMockScrollbarSettings mockScrollbarSettings;

    registerMockedHttpURLLoad("large-div.html");

    int viewWidth = 500;
    int viewHeight = 500;

    OwnPtr<FakeCompositingWebViewClient> fakeCompositingWebViewClient = adoptPtr(new FakeCompositingWebViewClient());
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize(true, 0, fakeCompositingWebViewClient.get(), &configueCompositingWebView);

    webViewHelper.webView()->resize(WebSize(viewWidth, viewHeight));
    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), m_baseURL + "large-div.html");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    webViewHelper.webView()->layout();

    WebCore::FrameView* view = webViewHelper.webViewImpl()->mainFrameImpl()->frameView();
    EXPECT_TRUE(view->renderView()->compositor()->layerForHorizontalScrollbar());
    EXPECT_TRUE(view->renderView()->compositor()->layerForVerticalScrollbar());

    webViewHelper.webView()->resize(WebSize(viewWidth * 10, viewHeight * 10));
    webViewHelper.webView()->layout();
    EXPECT_FALSE(view->renderView()->compositor()->layerForHorizontalScrollbar());
    EXPECT_FALSE(view->renderView()->compositor()->layerForVerticalScrollbar());
}

void setScaleAndScrollAndLayout(blink::WebView* webView, WebPoint scroll, float scale)
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "get_scale_for_auto_zoom_into_div_test.html");
    webViewHelper.webView()->setDeviceScaleFactor(deviceScaleFactor);
    webViewHelper.webView()->setPageScaleFactorLimits(0.01f, 4);
    webViewHelper.webView()->setPageScaleFactor(0.5f, WebPoint(0, 0));
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    WebRect wideDiv(200, 100, 400, 150);
    WebRect tallDiv(200, 300, 400, 800);
    WebRect doubleTapPointWide(wideDiv.x + 50, wideDiv.y + 50, touchPointPadding, touchPointPadding);
    WebRect doubleTapPointTall(tallDiv.x + 50, tallDiv.y + 50, touchPointPadding, touchPointPadding);
    WebRect wideBlockBounds;
    WebRect tallBlockBounds;
    float scale;
    WebPoint scroll;

    float doubleTapZoomAlreadyLegibleScale = webViewHelper.webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;

    // Test double-tap zooming into wide div.
    wideBlockBounds = webViewHelper.webViewImpl()->computeBlockBounds(doubleTapPointWide, false);
    webViewHelper.webViewImpl()->computeScaleAndScrollForBlockRect(WebPoint(doubleTapPointWide.x, doubleTapPointWide.y), wideBlockBounds, touchPointPadding, doubleTapZoomAlreadyLegibleScale, scale, scroll);
    // The div should horizontally fill the screen (modulo margins), and
    // vertically centered (modulo integer rounding).
    EXPECT_NEAR(viewportWidth / (float) wideDiv.width, scale, 0.1);
    EXPECT_NEAR(wideDiv.x, scroll.x, 20);
    EXPECT_EQ(0, scroll.y);

    setScaleAndScrollAndLayout(webViewHelper.webViewImpl(), scroll, scale);

    // Test zoom out back to minimum scale.
    wideBlockBounds = webViewHelper.webViewImpl()->computeBlockBounds(doubleTapPointWide, false);
    webViewHelper.webViewImpl()->computeScaleAndScrollForBlockRect(WebPoint(doubleTapPointWide.x, doubleTapPointWide.y), wideBlockBounds, touchPointPadding, doubleTapZoomAlreadyLegibleScale, scale, scroll);

    scale = webViewHelper.webViewImpl()->minimumPageScaleFactor();
    setScaleAndScrollAndLayout(webViewHelper.webViewImpl(), WebPoint(0, 0), scale);

    // Test double-tap zooming into tall div.
    tallBlockBounds = webViewHelper.webViewImpl()->computeBlockBounds(doubleTapPointTall, false);
    webViewHelper.webViewImpl()->computeScaleAndScrollForBlockRect(WebPoint(doubleTapPointTall.x, doubleTapPointTall.y), tallBlockBounds, touchPointPadding, doubleTapZoomAlreadyLegibleScale, scale, scroll);
    // The div should start at the top left of the viewport.
    EXPECT_NEAR(viewportWidth / (float) tallDiv.width, scale, 0.1);
    EXPECT_NEAR(tallDiv.x, scroll.x, 20);
    EXPECT_NEAR(tallDiv.y, scroll.y, 20);

    // Test for Non-doubletap scaling
    // Test zooming into div.
    webViewHelper.webViewImpl()->computeScaleAndScrollForBlockRect(WebPoint(250, 250), webViewHelper.webViewImpl()->computeBlockBounds(WebRect(250, 250, 10, 10), true), 0, doubleTapZoomAlreadyLegibleScale, scale, scroll);
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "get_wide_div_for_auto_zoom_test.html");
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->setPageScaleFactorLimits(1.0f, 4);
    webViewHelper.webView()->setDeviceScaleFactor(deviceScaleFactor);
    webViewHelper.webView()->setPageScaleFactor(1.0f, WebPoint(0, 0));
    webViewHelper.webView()->layout();

    webViewHelper.webViewImpl()->enableFakePageScaleAnimationForTesting(true);

    float doubleTapZoomAlreadyLegibleScale = webViewHelper.webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;

    WebRect div(0, 100, viewportWidth, 150);
    WebPoint point(div.x + 50, div.y + 50);
    float scale;
    setScaleAndScrollAndLayout(webViewHelper.webViewImpl(), WebPoint(0, 0), (webViewHelper.webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);

    simulateDoubleTap(webViewHelper.webViewImpl(), point, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), point, scale);
    EXPECT_FLOAT_EQ(webViewHelper.webViewImpl()->minimumPageScaleFactor(), scale);
}

TEST_F(WebFrameTest, DivAutoZoomVeryTallTest)
{
    // When a block is taller than the viewport and a zoom targets a lower part
    // of it, then we should keep the target point onscreen instead of snapping
    // back up the top of the block.
    registerMockedHttpURLLoad("very_tall_div.html");

    const float deviceScaleFactor = 2.0f;
    int viewportWidth = 640 / deviceScaleFactor;
    int viewportHeight = 1280 / deviceScaleFactor;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "very_tall_div.html", true, 0, 0, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->setPageScaleFactorLimits(1.0f, 4);
    webViewHelper.webView()->setDeviceScaleFactor(deviceScaleFactor);
    webViewHelper.webView()->setPageScaleFactor(1.0f, WebPoint(0, 0));
    webViewHelper.webView()->layout();

    WebRect div(200, 300, 400, 5000);
    WebPoint point(div.x + 50, div.y + 3000);
    float scale;
    WebPoint scroll;

    WebRect blockBounds = webViewHelper.webViewImpl()->computeBlockBounds(WebRect(point.x, point.y, 0, 0), true);
    webViewHelper.webViewImpl()->computeScaleAndScrollForBlockRect(point, blockBounds, 0, 1.0f, scale, scroll);
    EXPECT_EQ(scale, 1.0f);
    EXPECT_EQ(scroll.y, 2660);
}

TEST_F(WebFrameTest, DivAutoZoomMultipleDivsTest)
{
    registerMockedHttpURLLoad("get_multiple_divs_for_auto_zoom_test.html");

    const float deviceScaleFactor = 2.0f;
    int viewportWidth = 640 / deviceScaleFactor;
    int viewportHeight = 1280 / deviceScaleFactor;
    float doubleTapZoomAlreadyLegibleRatio = 1.2f;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "get_multiple_divs_for_auto_zoom_test.html");
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->setPageScaleFactorLimits(0.5f, 4);
    webViewHelper.webView()->setDeviceScaleFactor(deviceScaleFactor);
    webViewHelper.webView()->setPageScaleFactor(0.5f, WebPoint(0, 0));
    webViewHelper.webView()->layout();

    webViewHelper.webViewImpl()->enableFakePageScaleAnimationForTesting(true);

    WebRect topDiv(200, 100, 200, 150);
    WebRect bottomDiv(200, 300, 200, 150);
    WebPoint topPoint(topDiv.x + 50, topDiv.y + 50);
    WebPoint bottomPoint(bottomDiv.x + 50, bottomDiv.y + 50);
    float scale;
    setScaleAndScrollAndLayout(webViewHelper.webViewImpl(), WebPoint(0, 0), (webViewHelper.webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);

    // Test double tap on two different divs
    // After first zoom, we should go back to minimum page scale with a second double tap.
    simulateDoubleTap(webViewHelper.webViewImpl(), topPoint, scale);
    EXPECT_FLOAT_EQ(1, scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), bottomPoint, scale);
    EXPECT_FLOAT_EQ(webViewHelper.webViewImpl()->minimumPageScaleFactor(), scale);

    // If the user pinch zooms after double tap, a second double tap should zoom back to the div.
    simulateDoubleTap(webViewHelper.webViewImpl(), topPoint, scale);
    EXPECT_FLOAT_EQ(1, scale);
    webViewHelper.webViewImpl()->applyScrollAndScale(WebSize(), 0.6f);
    simulateDoubleTap(webViewHelper.webViewImpl(), bottomPoint, scale);
    EXPECT_FLOAT_EQ(1, scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), bottomPoint, scale);
    EXPECT_FLOAT_EQ(webViewHelper.webViewImpl()->minimumPageScaleFactor(), scale);

    // If we didn't yet get an auto-zoom update and a second double-tap arrives, should go back to minimum scale.
    webViewHelper.webViewImpl()->applyScrollAndScale(WebSize(), 1.1f);
    webViewHelper.webViewImpl()->animateDoubleTapZoom(topPoint);
    EXPECT_TRUE(webViewHelper.webViewImpl()->fakeDoubleTapAnimationPendingForTesting());
    simulateDoubleTap(webViewHelper.webViewImpl(), bottomPoint, scale);
    EXPECT_FLOAT_EQ(webViewHelper.webViewImpl()->minimumPageScaleFactor(), scale);
}

TEST_F(WebFrameTest, DivAutoZoomScaleBoundsTest)
{
    registerMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

    int viewportWidth = 320;
    int viewportHeight = 480;
    float doubleTapZoomAlreadyLegibleRatio = 1.2f;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "get_scale_bounds_check_for_auto_zoom_test.html");
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->setDeviceScaleFactor(1.5f);
    webViewHelper.webView()->layout();

    webViewHelper.webViewImpl()->enableFakePageScaleAnimationForTesting(true);

    WebRect div(200, 100, 200, 150);
    WebPoint doubleTapPoint(div.x + 50, div.y + 50);
    float scale;

    // Test double tap scale bounds.
    // minimumPageScale < doubleTapZoomAlreadyLegibleScale < 1
    webViewHelper.webView()->setPageScaleFactorLimits(0.5f, 4);
    webViewHelper.webView()->layout();
    float doubleTapZoomAlreadyLegibleScale = webViewHelper.webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;
    setScaleAndScrollAndLayout(webViewHelper.webViewImpl(), WebPoint(0, 0), (webViewHelper.webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(1, scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewHelper.webViewImpl()->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(1, scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewHelper.webViewImpl()->applyScrollAndScale(WebSize(), 1.1f);
    // 1 < minimumPageScale < doubleTapZoomAlreadyLegibleScale
    webViewHelper.webView()->setPageScaleFactorLimits(1.1f, 4);
    webViewHelper.webView()->layout();
    doubleTapZoomAlreadyLegibleScale = webViewHelper.webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;
    setScaleAndScrollAndLayout(webViewHelper.webViewImpl(), WebPoint(0, 0), (webViewHelper.webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewHelper.webViewImpl()->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewHelper.webViewImpl()->applyScrollAndScale(WebSize(), 1.1f);
    // minimumPageScale < 1 < doubleTapZoomAlreadyLegibleScale
    webViewHelper.webView()->setPageScaleFactorLimits(0.95f, 4);
    webViewHelper.webView()->layout();
    doubleTapZoomAlreadyLegibleScale = webViewHelper.webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;
    setScaleAndScrollAndLayout(webViewHelper.webViewImpl(), WebPoint(0, 0), (webViewHelper.webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewHelper.webViewImpl()->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
}

TEST_F(WebFrameTest, DivAutoZoomScaleFontScaleFactorTest)
{
    registerMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

    int viewportWidth = 320;
    int viewportHeight = 480;
    float doubleTapZoomAlreadyLegibleRatio = 1.2f;
    float accessibilityFontScaleFactor = 1.13f;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "get_scale_bounds_check_for_auto_zoom_test.html");
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    webViewHelper.webViewImpl()->enableFakePageScaleAnimationForTesting(true);
    webViewHelper.webViewImpl()->page()->settings().setTextAutosizingEnabled(true);
    webViewHelper.webViewImpl()->page()->settings().setAccessibilityFontScaleFactor(accessibilityFontScaleFactor);

    WebRect div(200, 100, 200, 150);
    WebPoint doubleTapPoint(div.x + 50, div.y + 50);
    float scale;

    // Test double tap scale bounds.
    // minimumPageScale < doubleTapZoomAlreadyLegibleScale < 1 < accessibilityFontScaleFactor
    float legibleScale = accessibilityFontScaleFactor;
    setScaleAndScrollAndLayout(webViewHelper.webViewImpl(), WebPoint(0, 0), (webViewHelper.webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    float doubleTapZoomAlreadyLegibleScale = webViewHelper.webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;
    webViewHelper.webView()->setPageScaleFactorLimits(0.5f, 4);
    webViewHelper.webView()->layout();
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(legibleScale, scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewHelper.webViewImpl()->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(legibleScale, scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewHelper.webViewImpl()->applyScrollAndScale(WebSize(), 1.1f);
    // 1 < accessibilityFontScaleFactor < minimumPageScale < doubleTapZoomAlreadyLegibleScale
    webViewHelper.webView()->setPageScaleFactorLimits(1.0f, 4);
    webViewHelper.webView()->layout();
    doubleTapZoomAlreadyLegibleScale = webViewHelper.webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;
    setScaleAndScrollAndLayout(webViewHelper.webViewImpl(), WebPoint(0, 0), (webViewHelper.webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewHelper.webViewImpl()->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewHelper.webViewImpl()->applyScrollAndScale(WebSize(), 1.1f);
    // minimumPageScale < 1 < accessibilityFontScaleFactor < doubleTapZoomAlreadyLegibleScale
    webViewHelper.webView()->setPageScaleFactorLimits(0.95f, 4);
    webViewHelper.webView()->layout();
    doubleTapZoomAlreadyLegibleScale = webViewHelper.webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;
    setScaleAndScrollAndLayout(webViewHelper.webViewImpl(), WebPoint(0, 0), (webViewHelper.webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewHelper.webViewImpl()->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewHelper.webViewImpl()->applyScrollAndScale(WebSize(), 1.1f);
    // minimumPageScale < 1 < doubleTapZoomAlreadyLegibleScale < accessibilityFontScaleFactor
    webViewHelper.webView()->setPageScaleFactorLimits(0.9f, 4);
    webViewHelper.webView()->layout();
    doubleTapZoomAlreadyLegibleScale = webViewHelper.webViewImpl()->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;
    setScaleAndScrollAndLayout(webViewHelper.webViewImpl(), WebPoint(0, 0), (webViewHelper.webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(legibleScale, scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewHelper.webViewImpl()->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewHelper.webViewImpl(), doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(legibleScale, scale);
}

TEST_F(WebFrameTest, DivMultipleTargetZoomMultipleDivsTest)
{
    registerMockedHttpURLLoad("get_multiple_divs_for_auto_zoom_test.html");

    const float deviceScaleFactor = 2.0f;
    int viewportWidth = 640 / deviceScaleFactor;
    int viewportHeight = 1280 / deviceScaleFactor;
    float doubleTapZoomAlreadyLegibleRatio = 1.2f;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "get_multiple_divs_for_auto_zoom_test.html");
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->setPageScaleFactorLimits(0.5f, 4);
    webViewHelper.webView()->setDeviceScaleFactor(deviceScaleFactor);
    webViewHelper.webView()->setPageScaleFactor(0.5f, WebPoint(0, 0));
    webViewHelper.webView()->layout();

    webViewHelper.webViewImpl()->enableFakePageScaleAnimationForTesting(true);

    WebRect viewportRect(0, 0, viewportWidth, viewportHeight);
    WebRect topDiv(200, 100, 200, 150);
    WebRect bottomDiv(200, 300, 200, 150);
    float scale;
    setScaleAndScrollAndLayout(webViewHelper.webViewImpl(), WebPoint(0, 0), (webViewHelper.webViewImpl()->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);

    simulateMultiTargetZoom(webViewHelper.webViewImpl(), topDiv, scale);
    EXPECT_FLOAT_EQ(1, scale);
    simulateMultiTargetZoom(webViewHelper.webViewImpl(), bottomDiv, scale);
    EXPECT_FLOAT_EQ(1, scale);
    simulateMultiTargetZoom(webViewHelper.webViewImpl(), viewportRect, scale);
    EXPECT_FLOAT_EQ(1, scale);
    webViewHelper.webViewImpl()->setPageScaleFactor(webViewHelper.webViewImpl()->minimumPageScaleFactor(), WebPoint(0, 0));
    simulateMultiTargetZoom(webViewHelper.webViewImpl(), topDiv, scale);
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "get_scale_for_zoom_into_editable_test.html");
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->setPageScaleFactorLimits(1, 4);
    webViewHelper.webView()->layout();
    webViewHelper.webView()->setDeviceScaleFactor(1.5f);
    webViewHelper.webView()->settings()->setAutoZoomFocusedNodeToLegibleScale(true);

    webViewHelper.webViewImpl()->enableFakePageScaleAnimationForTesting(true);

    WebRect editBoxWithText(200, 200, 250, 20);
    WebRect editBoxWithNoText(200, 250, 250, 20);

    // Test scrolling the focused node
    // The edit box is shorter and narrower than the viewport when legible.
    webViewHelper.webView()->advanceFocus(false);
    // Set the caret to the end of the input box.
    webViewHelper.webView()->mainFrame()->document().getElementById("EditBoxWithText").to<WebInputElement>().setSelectionRange(1000, 1000);
    setScaleAndScrollAndLayout(webViewHelper.webView(), WebPoint(0, 0), 1);
    WebRect rect, caret;
    webViewHelper.webViewImpl()->selectionBounds(caret, rect);

    float scale;
    WebCore::IntPoint scroll;
    bool needAnimation;
    webViewHelper.webViewImpl()->computeScaleAndScrollForFocusedNode(webViewHelper.webViewImpl()->focusedElement(), scale, scroll, needAnimation);
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
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    setScaleAndScrollAndLayout(webViewHelper.webView(), WebPoint(0, 0), 1);
    webViewHelper.webViewImpl()->selectionBounds(caret, rect);
    webViewHelper.webViewImpl()->computeScaleAndScrollForFocusedNode(webViewHelper.webViewImpl()->focusedElement(), scale, scroll, needAnimation);
    EXPECT_TRUE(needAnimation);
    // The caret should be right aligned since the caret would be offscreen when the edit box is left aligned.
    hScroll = caret.x + caret.width + caretPadding - viewportWidth / scale;
    EXPECT_NEAR(hScroll, scroll.x(), 1);
    EXPECT_NEAR(minReadableCaretHeight / caret.height, scale, 0.1);

    setScaleAndScrollAndLayout(webViewHelper.webView(), WebPoint(0, 0), 1);
    // Move focus to edit box with text.
    webViewHelper.webView()->advanceFocus(false);
    webViewHelper.webViewImpl()->selectionBounds(caret, rect);
    webViewHelper.webViewImpl()->computeScaleAndScrollForFocusedNode(webViewHelper.webViewImpl()->focusedElement(), scale, scroll, needAnimation);
    EXPECT_TRUE(needAnimation);
    // The edit box should be left aligned.
    hScroll = editBoxWithNoText.x;
    EXPECT_NEAR(hScroll, scroll.x(), 1);
    vScroll = editBoxWithNoText.y - (viewportHeight / scale - editBoxWithNoText.height) / 2;
    EXPECT_NEAR(vScroll, scroll.y(), 1);
    EXPECT_NEAR(minReadableCaretHeight / caret.height, scale, 0.1);

    setScaleAndScrollAndLayout(webViewHelper.webViewImpl(), scroll, scale);

    // Move focus back to the first edit box.
    webViewHelper.webView()->advanceFocus(true);
    webViewHelper.webViewImpl()->computeScaleAndScrollForFocusedNode(webViewHelper.webViewImpl()->focusedElement(), scale, scroll, needAnimation);
    // The position should have stayed the same since this box was already on screen with the right scale.
    EXPECT_FALSE(needAnimation);
}

class TestReloadDoesntRedirectWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "form.html", false, &webFrameClient);

    webViewHelper.webView()->mainFrame()->reload(true);
    // start reload before request is delivered.
    webViewHelper.webView()->mainFrame()->reload(true);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
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

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + firstURL, true);
    webViewHelper.webViewImpl()->resize(WebSize(pageWidth, pageHeight));
    webViewHelper.webViewImpl()->mainFrame()->setScrollOffset(WebSize(pageWidth / 4, pageHeight / 4));
    webViewHelper.webViewImpl()->setPageScaleFactorPreservingScrollOffset(pageScaleFactor);

    WebSize previousOffset = webViewHelper.webViewImpl()->mainFrame()->scrollOffset();
    float previousScale = webViewHelper.webViewImpl()->pageScaleFactor();

    // Reload the page using the cache.
    webViewHelper.webViewImpl()->mainFrame()->reloadWithOverrideURL(toKURL(m_baseURL + secondURL), false);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    ASSERT_EQ(previousOffset, webViewHelper.webViewImpl()->mainFrame()->scrollOffset());
    ASSERT_EQ(previousScale, webViewHelper.webViewImpl()->pageScaleFactor());

    // Reload the page while ignoring the cache.
    webViewHelper.webViewImpl()->mainFrame()->reloadWithOverrideURL(toKURL(m_baseURL + thirdURL), true);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    ASSERT_EQ(previousOffset, webViewHelper.webViewImpl()->mainFrame()->scrollOffset());
    ASSERT_EQ(previousScale, webViewHelper.webViewImpl()->pageScaleFactor());
}

TEST_F(WebFrameTest, ReloadWhileProvisional)
{
    // Test that reloading while the previous load is still pending does not cause the initial
    // request to get lost.
    registerMockedHttpURLLoad("fixed_layout.html");

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize();
    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), m_baseURL + "fixed_layout.html");
    // start reload before first request is delivered.
    webViewHelper.webView()->mainFrame()->reload(true);

    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();

    WebDataSource* dataSource = webViewHelper.webView()->mainFrame()->dataSource();
    ASSERT_TRUE(dataSource);
    EXPECT_EQ(toKURL(m_baseURL + "fixed_layout.html"), toKURL(dataSource->request().url().spec()));
}

TEST_F(WebFrameTest, AppendRedirects)
{
    const std::string firstURL = "about:blank";
    const std::string secondURL = "http://www.test.com";

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(firstURL, true);

    WebDataSource* dataSource = webViewHelper.webView()->mainFrame()->dataSource();
    ASSERT_TRUE(dataSource);
    dataSource->appendRedirect(toKURL(secondURL));

    WebVector<WebURL> redirects;
    dataSource->redirectChain(redirects);
    ASSERT_EQ(2U, redirects.size());
    EXPECT_EQ(toKURL(firstURL), toKURL(redirects[0].spec().data()));
    EXPECT_EQ(toKURL(secondURL), toKURL(redirects[1].spec().data()));
}

TEST_F(WebFrameTest, IframeRedirect)
{
    registerMockedHttpURLLoad("iframe_redirect.html");
    registerMockedHttpURLLoad("visible_iframe.html");

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "iframe_redirect.html", true);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests(); // Load the iframe.

    WebFrame* iframe = webViewHelper.webView()->findFrameByName(WebString::fromUTF8("ifr"));
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

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "iframe_clear_focused_node_test.html", true);

    // Clear the focused node.
    webViewHelper.webView()->clearFocusedElement();

    // Now retrieve the FocusedNode and test it should be null.
    EXPECT_EQ(0, webViewHelper.webViewImpl()->focusedElement());
}

// Implementation of WebFrameClient that tracks the v8 contexts that are created
// and destroyed for verification.
class ContextLifetimeTestWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
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
            context.Reset();
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "context_notifications_test.html", true, &webFrameClient);

    WebFrame* mainFrame = webViewHelper.webView()->mainFrame();
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
    webViewHelper.reset();

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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "context_notifications_test.html", true, &webFrameClient);

    // Refresh, we should get two release notifications and two more create notifications.
    webViewHelper.webView()->mainFrame()->reload(false);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    ASSERT_EQ(4u, webFrameClient.createNotifications.size());
    ASSERT_EQ(2u, webFrameClient.releaseNotifications.size());

    // The two release notifications we got should be exactly the same as the first two create notifications.
    for (size_t i = 0; i < webFrameClient.releaseNotifications.size(); ++i) {
      EXPECT_TRUE(webFrameClient.releaseNotifications[i]->Equals(
          webFrameClient.createNotifications[webFrameClient.createNotifications.size() - 3 - i]));
    }

    // The last two create notifications should be for the current frames and context.
    WebFrame* mainFrame = webViewHelper.webView()->mainFrame();
    WebFrame* childFrame = mainFrame->firstChild();
    ContextLifetimeTestWebFrameClient::Notification* firstRefreshNotification = webFrameClient.createNotifications[2];
    ContextLifetimeTestWebFrameClient::Notification* secondRefreshNotification = webFrameClient.createNotifications[3];

    EXPECT_EQ(mainFrame, firstRefreshNotification->frame);
    EXPECT_EQ(mainFrame->mainWorldScriptContext(), firstRefreshNotification->context);
    EXPECT_EQ(0, firstRefreshNotification->worldId);

    EXPECT_EQ(childFrame, secondRefreshNotification->frame);
    EXPECT_EQ(childFrame->mainWorldScriptContext(), secondRefreshNotification->context);
    EXPECT_EQ(0, secondRefreshNotification->worldId);
}

TEST_F(WebFrameTest, ContextNotificationsIsolatedWorlds)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);

    registerMockedHttpURLLoad("context_notifications_test.html");
    registerMockedHttpURLLoad("context_notifications_test_frame.html");

    ContextLifetimeTestWebFrameClient webFrameClient;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "context_notifications_test.html", true, &webFrameClient);

    // Add an isolated world.
    webFrameClient.reset();

    int isolatedWorldId = 42;
    WebScriptSource scriptSource("hi!");
    int numSources = 1;
    int extensionGroup = 0;
    webViewHelper.webView()->mainFrame()->executeScriptInIsolatedWorld(isolatedWorldId, &scriptSource, numSources, extensionGroup);

    // We should now have a new create notification.
    ASSERT_EQ(1u, webFrameClient.createNotifications.size());
    ContextLifetimeTestWebFrameClient::Notification* notification = webFrameClient.createNotifications[0];
    ASSERT_EQ(isolatedWorldId, notification->worldId);
    ASSERT_EQ(webViewHelper.webView()->mainFrame(), notification->frame);

    // We don't have an API to enumarate isolated worlds for a frame, but we can at least assert that the context we got is *not* the main world's context.
    ASSERT_NE(webViewHelper.webView()->mainFrame()->mainWorldScriptContext(), v8::Local<v8::Context>::New(isolate, notification->context));

    webViewHelper.reset();

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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "find.html");
    WebFrame* frame = webViewHelper.webView()->mainFrame();
    const int findIdentifier = 12345;
    WebFindOptions options;

    // Find in a <div> element.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar1"), options, false, 0));
    frame->stopFinding(false);
    WebRange range = frame->selectionRange();
    EXPECT_EQ(5, range.startOffset());
    EXPECT_EQ(9, range.endOffset());
    EXPECT_TRUE(frame->document().focusedElement().isNull());

    // Find in an <input> value.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar2"), options, false, 0));
    // Confirm stopFinding(false) sets the selection on the found text.
    frame->stopFinding(false);
    range = frame->selectionRange();
    ASSERT_FALSE(range.isNull());
    EXPECT_EQ(5, range.startOffset());
    EXPECT_EQ(9, range.endOffset());
    EXPECT_EQ(WebString::fromUTF8("INPUT"), frame->document().focusedElement().tagName());

    // Find in a <textarea> content.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar3"), options, false, 0));
    // Confirm stopFinding(false) sets the selection on the found text.
    frame->stopFinding(false);
    range = frame->selectionRange();
    ASSERT_FALSE(range.isNull());
    EXPECT_EQ(5, range.startOffset());
    EXPECT_EQ(9, range.endOffset());
    EXPECT_EQ(WebString::fromUTF8("TEXTAREA"), frame->document().focusedElement().tagName());

    // Find in a contentEditable element.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar4"), options, false, 0));
    // Confirm stopFinding(false) sets the selection on the found text.
    frame->stopFinding(false);
    range = frame->selectionRange();
    ASSERT_FALSE(range.isNull());
    EXPECT_EQ(0, range.startOffset());
    EXPECT_EQ(4, range.endOffset());
    // "bar4" is surrounded by <span>, but the focusable node should be the parent <div>.
    EXPECT_EQ(WebString::fromUTF8("DIV"), frame->document().focusedElement().tagName());

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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad("about:blank", true);
    // We set the size because it impacts line wrapping, which changes the
    // resulting text value.
    webViewHelper.webView()->resize(WebSize(640, 480));
    WebFrame* frame = webViewHelper.webView()->mainFrame();

    // Generate a simple test case.
    const char simpleSource[] = "<div>Foo bar</div><div></div>baz";
    WebCore::KURL testURL = toKURL("about:blank");
    frame->loadHTMLString(simpleSource, testURL);
    runPendingTasks();

    // Make sure it comes out OK.
    const std::string expected("Foo bar\nbaz");
    WebString text = frame->contentAsText(std::numeric_limits<size_t>::max());
    EXPECT_EQ(expected, text.utf8());

    // Try reading the same one with clipping of the text.
    const int length = 5;
    text = frame->contentAsText(length);
    EXPECT_EQ(expected.substr(0, length), text.utf8());

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
    EXPECT_EQ("Hello world\n\nsub\ntext", text.utf8());

    // Get the frame text where the subframe separator falls on the boundary of
    // what we'll take. There used to be a crash in this case.
    text = frame->contentAsText(12);
    EXPECT_EQ("Hello world", text.utf8());
}

TEST_F(WebFrameTest, GetFullHtmlOfPage)
{
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad("about:blank", true);
    WebFrame* frame = webViewHelper.webView()->mainFrame();

    // Generate a simple test case.
    const char simpleSource[] = "<p>Hello</p><p>World</p>";
    WebCore::KURL testURL = toKURL("about:blank");
    frame->loadHTMLString(simpleSource, testURL);
    runPendingTasks();

    WebString text = frame->contentAsText(std::numeric_limits<size_t>::max());
    EXPECT_EQ("Hello\n\nWorld", text.utf8());

    const std::string html = frame->contentAsMarkup().utf8();

    // Load again with the output html.
    frame->loadHTMLString(WebData(html.c_str(), html.length()), testURL);
    runPendingTasks();

    EXPECT_EQ(html, frame->contentAsMarkup().utf8());

    text = frame->contentAsText(std::numeric_limits<size_t>::max());
    EXPECT_EQ("Hello\n\nWorld", text.utf8());

    // Test selection check
    EXPECT_FALSE(frame->hasSelection());
    frame->executeCommand(WebString::fromUTF8("SelectAll"));
    EXPECT_TRUE(frame->hasSelection());
    frame->executeCommand(WebString::fromUTF8("Unselect"));
    EXPECT_FALSE(frame->hasSelection());
    WebString selectionHtml = frame->selectionAsMarkup();
    EXPECT_TRUE(selectionHtml.isEmpty());
}

class TestExecuteScriptDuringDidCreateScriptContext : public FrameTestHelpers::TestWebFrameClient {
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "hello_world.html", true, &webFrameClient);

    webViewHelper.webView()->mainFrame()->reload();
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
}

class FindUpdateWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
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
// Also failing on Android: http://crbug.com/341314
#if OS(MACOSX) || OS(ANDROID)
TEST_F(WebFrameTest, DISABLED_FindInPageMatchRects)
#else
TEST_F(WebFrameTest, FindInPageMatchRects)
#endif
{
    registerMockedHttpURLLoad("find_in_page.html");
    registerMockedHttpURLLoad("find_in_page_frame.html");

    FindUpdateWebFrameClient client;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "find_in_page.html", true, &client);
    webViewHelper.webView()->resize(WebSize(640, 480));
    webViewHelper.webView()->layout();
    runPendingTasks();

    // Note that the 'result 19' in the <select> element is not expected to produce a match.
    static const char* kFindString = "result";
    static const int kFindIdentifier = 12345;
    static const int kNumResults = 19;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
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
    webViewHelper.webView()->resize(WebSize(800, 600));
    runPendingTasks();
    EXPECT_TRUE(mainFrame->findMatchMarkersVersion() != rectsVersion);
}

TEST_F(WebFrameTest, FindInPageSkipsHiddenFrames)
{
    registerMockedHttpURLLoad("find_in_hidden_frame.html");

    FindUpdateWebFrameClient client;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "find_in_hidden_frame.html", true, &client);
    webViewHelper.webView()->resize(WebSize(640, 480));
    webViewHelper.webView()->layout();
    runPendingTasks();

    static const char* kFindString = "hello";
    static const int kFindIdentifier = 12345;
    static const int kNumResults = 1;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
    EXPECT_TRUE(mainFrame->find(kFindIdentifier, searchText, options, false, 0));

    mainFrame->resetMatchCount();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        frame->scopeStringMatches(kFindIdentifier, searchText, options, true);

    runPendingTasks();
    EXPECT_TRUE(client.findResultsAreReady());
    EXPECT_EQ(kNumResults, client.count());
}

TEST_F(WebFrameTest, FindOnDetachedFrame)
{
    registerMockedHttpURLLoad("find_in_page.html");
    registerMockedHttpURLLoad("find_in_page_frame.html");

    FindUpdateWebFrameClient client;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "find_in_page.html", true, &client);
    webViewHelper.webView()->resize(WebSize(640, 480));
    webViewHelper.webView()->layout();
    runPendingTasks();

    static const char* kFindString = "result";
    static const int kFindIdentifier = 12345;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
    RefPtr<WebFrameImpl> secondFrame = toWebFrameImpl(mainFrame->traverseNext(false));
    RefPtr<WebCore::LocalFrame> holdSecondFrame = secondFrame->frame();

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
}

TEST_F(WebFrameTest, FindDetachFrameBeforeScopeStrings)
{
    registerMockedHttpURLLoad("find_in_page.html");
    registerMockedHttpURLLoad("find_in_page_frame.html");

    FindUpdateWebFrameClient client;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "find_in_page.html", true, &client);
    webViewHelper.webView()->resize(WebSize(640, 480));
    webViewHelper.webView()->layout();
    runPendingTasks();

    static const char* kFindString = "result";
    static const int kFindIdentifier = 12345;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
    WebFrameImpl* secondFrame = toWebFrameImpl(mainFrame->traverseNext(false));
    RefPtr<WebCore::LocalFrame> holdSecondFrame = secondFrame->frame();

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
}

TEST_F(WebFrameTest, FindDetachFrameWhileScopingStrings)
{
    registerMockedHttpURLLoad("find_in_page.html");
    registerMockedHttpURLLoad("find_in_page_frame.html");

    FindUpdateWebFrameClient client;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "find_in_page.html", true, &client);
    webViewHelper.webView()->resize(WebSize(640, 480));
    webViewHelper.webView()->layout();
    runPendingTasks();

    static const char* kFindString = "result";
    static const int kFindIdentifier = 12345;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
    WebFrameImpl* secondFrame = toWebFrameImpl(mainFrame->traverseNext(false));
    RefPtr<WebCore::LocalFrame> holdSecondFrame = secondFrame->frame();

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
}

TEST_F(WebFrameTest, SetTickmarks)
{
    registerMockedHttpURLLoad("find.html");

    FindUpdateWebFrameClient client;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "find.html", true, &client);
    webViewHelper.webView()->resize(WebSize(640, 480));
    webViewHelper.webView()->layout();
    runPendingTasks();

    static const char* kFindString = "foo";
    static const int kFindIdentifier = 12345;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
    EXPECT_TRUE(mainFrame->find(kFindIdentifier, searchText, options, false, 0));

    mainFrame->resetMatchCount();
    mainFrame->scopeStringMatches(kFindIdentifier, searchText, options, true);

    runPendingTasks();
    EXPECT_TRUE(client.findResultsAreReady());

    // Get the tickmarks for the original find request.
    WebCore::FrameView* frameView = webViewHelper.webViewImpl()->mainFrameImpl()->frameView();
    RefPtr<WebCore::Scrollbar> scrollbar = frameView->createScrollbar(WebCore::HorizontalScrollbar);
    Vector<WebCore::IntRect> originalTickmarks;
    scrollbar->getTickmarks(originalTickmarks);
    EXPECT_EQ(4u, originalTickmarks.size());

    // Override the tickmarks.
    Vector<WebCore::IntRect> overridingTickmarksExpected;
    overridingTickmarksExpected.append(WebCore::IntRect(0, 0, 100, 100));
    overridingTickmarksExpected.append(WebCore::IntRect(0, 20, 100, 100));
    overridingTickmarksExpected.append(WebCore::IntRect(0, 30, 100, 100));
    mainFrame->setTickmarks(overridingTickmarksExpected);

    // Check the tickmarks are overriden correctly.
    Vector<WebCore::IntRect> overridingTickmarksActual;
    scrollbar->getTickmarks(overridingTickmarksActual);
    EXPECT_EQ(overridingTickmarksExpected, overridingTickmarksActual);

    // Reset the tickmark behavior.
    Vector<WebCore::IntRect> resetTickmarks;
    mainFrame->setTickmarks(resetTickmarks);

    // Check that the original tickmarks are returned
    Vector<WebCore::IntRect> originalTickmarksAfterReset;
    scrollbar->getTickmarks(originalTickmarksAfterReset);
    EXPECT_EQ(originalTickmarks, originalTickmarksAfterReset);
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
    return frame->selectionAsText().utf8();
}

TEST_F(WebFrameTest, SelectRange)
{
    WebFrame* frame;
    WebRect startWebRect;
    WebRect endWebRect;

    registerMockedHttpURLLoad("select_range_basic.html");
    registerMockedHttpURLLoad("select_range_scroll.html");

    FrameTestHelpers::WebViewHelper webViewHelper;
    initializeTextSelectionWebView(m_baseURL + "select_range_basic.html", &webViewHelper);
    frame = webViewHelper.webView()->mainFrame();
    EXPECT_EQ("Some test text for testing.", selectionAsString(frame));
    webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
    frame->executeCommand(WebString::fromUTF8("Unselect"));
    EXPECT_EQ("", selectionAsString(frame));
    frame->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
    EXPECT_EQ("Some test text for testing.", selectionAsString(frame));

    initializeTextSelectionWebView(m_baseURL + "select_range_scroll.html", &webViewHelper);
    frame = webViewHelper.webView()->mainFrame();
    EXPECT_EQ("Some offscreen test text for testing.", selectionAsString(frame));
    webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
    frame->executeCommand(WebString::fromUTF8("Unselect"));
    EXPECT_EQ("", selectionAsString(frame));
    frame->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
    EXPECT_EQ("Some offscreen test text for testing.", selectionAsString(frame));
}

TEST_F(WebFrameTest, SelectRangeInIframe)
{
    WebFrame* frame;
    WebRect startWebRect;
    WebRect endWebRect;

    registerMockedHttpURLLoad("select_range_iframe.html");
    registerMockedHttpURLLoad("select_range_basic.html");

    FrameTestHelpers::WebViewHelper webViewHelper;
    initializeTextSelectionWebView(m_baseURL + "select_range_iframe.html", &webViewHelper);
    frame = webViewHelper.webView()->mainFrame();
    WebFrame* subframe = frame->findChildByExpression(WebString::fromUTF8("/html/body/iframe"));
    EXPECT_EQ("Some test text for testing.", selectionAsString(subframe));
    webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
    subframe->executeCommand(WebString::fromUTF8("Unselect"));
    EXPECT_EQ("", selectionAsString(subframe));
    subframe->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
    EXPECT_EQ("Some test text for testing.", selectionAsString(subframe));
}

TEST_F(WebFrameTest, SelectRangeDivContentEditable)
{
    WebFrame* frame;
    WebRect startWebRect;
    WebRect endWebRect;

    registerMockedHttpURLLoad("select_range_div_editable.html");

    // Select the middle of an editable element, then try to extend the selection to the top of the document.
    // The selection range should be clipped to the bounds of the editable element.
    FrameTestHelpers::WebViewHelper webViewHelper;
    initializeTextSelectionWebView(m_baseURL + "select_range_div_editable.html", &webViewHelper);
    frame = webViewHelper.webView()->mainFrame();
    EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
    webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);

    frame->selectRange(bottomRightMinusOne(endWebRect), WebPoint(0, 0));
    EXPECT_EQ("16-char header. This text is initially selected.", selectionAsString(frame));

    // As above, but extending the selection to the bottom of the document.
    initializeTextSelectionWebView(m_baseURL + "select_range_div_editable.html", &webViewHelper);
    frame = webViewHelper.webView()->mainFrame();

    webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
    frame->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
    EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
    webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);

    webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
    frame->selectRange(topLeft(startWebRect), WebPoint(640, 480));
    EXPECT_EQ("This text is initially selected. 16-char footer.", selectionAsString(frame));
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    initializeTextSelectionWebView(m_baseURL + "select_range_span_editable.html", &webViewHelper);
    frame = webViewHelper.webView()->mainFrame();
    EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
    webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);

    frame->selectRange(bottomRightMinusOne(endWebRect), WebPoint(0, 0));
    EXPECT_EQ("16-char header. This text is initially selected.", selectionAsString(frame));

    // As above, but extending the selection to the bottom of the document.
    initializeTextSelectionWebView(m_baseURL + "select_range_span_editable.html", &webViewHelper);
    frame = webViewHelper.webView()->mainFrame();

    webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
    frame->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
    EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
    webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);

    EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
    webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
    frame->selectRange(topLeft(startWebRect), WebPoint(640, 480));
    EXPECT_EQ("This text is initially selected. 16-char footer.", selectionAsString(frame));
}

TEST_F(WebFrameTest, SelectRangeCanMoveSelectionStart)
{
    registerMockedHttpURLLoad("text_selection.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    initializeTextSelectionWebView(m_baseURL + "text_selection.html", &webViewHelper);
    WebFrame* frame = webViewHelper.webView()->mainFrame();

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
    FrameTestHelpers::WebViewHelper webViewHelper;
    initializeTextSelectionWebView(m_baseURL + "text_selection.html", &webViewHelper);
    WebFrame* frame = webViewHelper.webView()->mainFrame();

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
    FrameTestHelpers::WebViewHelper webViewHelper;
    initializeTextSelectionWebView(m_baseURL + "select_range_span_editable.html", &webViewHelper);
    WebFrameImpl* mainFrame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
    WebCore::RenderObject* renderer = mainFrame->frame()->selection().rootEditableElement()->renderer();
    EXPECT_EQ(0, computeOffset(renderer, -1, -1));
    EXPECT_EQ(64, computeOffset(renderer, 1000, 1000));

    registerMockedHttpURLLoad("select_range_div_editable.html");
    initializeTextSelectionWebView(m_baseURL + "select_range_div_editable.html", &webViewHelper);
    mainFrame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
    renderer = mainFrame->frame()->selection().rootEditableElement()->renderer();
    EXPECT_EQ(0, computeOffset(renderer, -1, -1));
    EXPECT_EQ(64, computeOffset(renderer, 1000, 1000));
}

#if !OS(MACOSX)
TEST_F(WebFrameTest, SelectRangeStaysHorizontallyAlignedWhenMoved)
{
    registerMockedHttpURLLoad("move_caret.html");

    FrameTestHelpers::WebViewHelper webViewHelper;
    initializeTextSelectionWebView(m_baseURL + "move_caret.html", &webViewHelper);
    WebFrameImpl* frame = toWebFrameImpl(webViewHelper.webView()->mainFrame());

    WebRect initialStartRect;
    WebRect initialEndRect;
    WebRect startRect;
    WebRect endRect;

    frame->executeScript(WebScriptSource("selectRange();"));
    webViewHelper.webView()->selectionBounds(initialStartRect, initialEndRect);
    WebPoint movedStart(topLeft(initialStartRect));

    movedStart.y += 40;
    frame->selectRange(movedStart, bottomRightMinusOne(initialEndRect));
    webViewHelper.webView()->selectionBounds(startRect, endRect);
    EXPECT_EQ(startRect, initialStartRect);
    EXPECT_EQ(endRect, initialEndRect);

    movedStart.y -= 80;
    frame->selectRange(movedStart, bottomRightMinusOne(initialEndRect));
    webViewHelper.webView()->selectionBounds(startRect, endRect);
    EXPECT_EQ(startRect, initialStartRect);
    EXPECT_EQ(endRect, initialEndRect);

    WebPoint movedEnd(bottomRightMinusOne(initialEndRect));

    movedEnd.y += 40;
    frame->selectRange(topLeft(initialStartRect), movedEnd);
    webViewHelper.webView()->selectionBounds(startRect, endRect);
    EXPECT_EQ(startRect, initialStartRect);
    EXPECT_EQ(endRect, initialEndRect);

    movedEnd.y -= 80;
    frame->selectRange(topLeft(initialStartRect), movedEnd);
    webViewHelper.webView()->selectionBounds(startRect, endRect);
    EXPECT_EQ(startRect, initialStartRect);
    EXPECT_EQ(endRect, initialEndRect);
}

TEST_F(WebFrameTest, MoveCaretStaysHorizontallyAlignedWhenMoved)
{
    WebFrameImpl* frame;
    registerMockedHttpURLLoad("move_caret.html");

    FrameTestHelpers::WebViewHelper webViewHelper;
    initializeTextSelectionWebView(m_baseURL + "move_caret.html", &webViewHelper);
    frame = (WebFrameImpl*)webViewHelper.webView()->mainFrame();

    WebRect initialStartRect;
    WebRect initialEndRect;
    WebRect startRect;
    WebRect endRect;

    frame->executeScript(WebScriptSource("selectCaret();"));
    webViewHelper.webView()->selectionBounds(initialStartRect, initialEndRect);
    WebPoint moveTo(topLeft(initialStartRect));

    moveTo.y += 40;
    frame->moveCaretSelection(moveTo);
    webViewHelper.webView()->selectionBounds(startRect, endRect);
    EXPECT_EQ(startRect, initialStartRect);
    EXPECT_EQ(endRect, initialEndRect);

    moveTo.y -= 80;
    frame->moveCaretSelection(moveTo);
    webViewHelper.webView()->selectionBounds(startRect, endRect);
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + htmlFile, true, 0, &client);
    webViewHelper.webView()->resize(WebSize(1000, 1000));
    webViewHelper.webView()->layout();

    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(0, 0));
    EXPECT_FALSE(client.triggered());

    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(200, 115));
    EXPECT_FALSE(client.triggered());

    for (int i = 0; i <= 46; i++) {
        client.resetTriggered();
        webViewHelper.webView()->handleInputEvent(fatTap(120, 230 + i * 5));

        int j = i % 10;
        if (j >= 7 && j <= 9)
            EXPECT_TRUE(client.triggered());
        else
            EXPECT_FALSE(client.triggered());
    }

    for (int i = 0; i <= 46; i++) {
        client.resetTriggered();
        webViewHelper.webView()->handleInputEvent(fatTap(10 + i * 5, 590));

        int j = i % 10;
        if (j >= 7 && j <= 9)
            EXPECT_TRUE(client.triggered());
        else
            EXPECT_FALSE(client.triggered());
    }
}

TEST_F(WebFrameTest, DisambiguationPopupNoContainer)
{
    registerMockedHttpURLLoad("disambiguation_popup_no_container.html");

    DisambiguationPopupTestWebViewClient client;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "disambiguation_popup_no_container.html", true, 0, &client);
    webViewHelper.webView()->resize(WebSize(1000, 1000));
    webViewHelper.webView()->layout();

    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(50, 50));
    EXPECT_FALSE(client.triggered());
}

TEST_F(WebFrameTest, DisambiguationPopupMobileSite)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    const std::string htmlFile = "disambiguation_popup_mobile_site.html";
    registerMockedHttpURLLoad(htmlFile);

    DisambiguationPopupTestWebViewClient client;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + htmlFile, true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(1000, 1000));
    webViewHelper.webView()->layout();

    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(0, 0));
    EXPECT_FALSE(client.triggered());

    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(200, 115));
    EXPECT_FALSE(client.triggered());

    for (int i = 0; i <= 46; i++) {
        client.resetTriggered();
        webViewHelper.webView()->handleInputEvent(fatTap(120, 230 + i * 5));
        EXPECT_FALSE(client.triggered());
    }

    for (int i = 0; i <= 46; i++) {
        client.resetTriggered();
        webViewHelper.webView()->handleInputEvent(fatTap(10 + i * 5, 590));
        EXPECT_FALSE(client.triggered());
    }
}

TEST_F(WebFrameTest, DisambiguationPopupViewportSite)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    const std::string htmlFile = "disambiguation_popup_viewport_site.html";
    registerMockedHttpURLLoad(htmlFile);

    DisambiguationPopupTestWebViewClient client;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + htmlFile, true, 0, &client, enableViewportSettings);
    webViewHelper.webView()->resize(WebSize(1000, 1000));
    webViewHelper.webView()->layout();

    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(0, 0));
    EXPECT_FALSE(client.triggered());

    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(200, 115));
    EXPECT_FALSE(client.triggered());

    for (int i = 0; i <= 46; i++) {
        client.resetTriggered();
        webViewHelper.webView()->handleInputEvent(fatTap(120, 230 + i * 5));
        EXPECT_FALSE(client.triggered());
    }

    for (int i = 0; i <= 46; i++) {
        client.resetTriggered();
        webViewHelper.webView()->handleInputEvent(fatTap(10 + i * 5, 590));
        EXPECT_FALSE(client.triggered());
    }
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + htmlFile, true, 0, &client);
    webViewHelper.webView()->resize(WebSize(viewportWidth, viewportHeight));
    webViewHelper.webView()->layout();

    // Click somewhere where the popup shouldn't appear.
    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(viewportWidth / 2, 0));
    EXPECT_FALSE(client.triggered());

    // Click directly in between two container divs with click handlers, with children that don't handle clicks.
    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(viewportWidth / 2, divHeight));
    EXPECT_TRUE(client.triggered());

    // The third div container should be blacklisted if you click on the link it contains.
    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(viewportWidth / 2, divHeight * 3.25));
    EXPECT_FALSE(client.triggered());
}

TEST_F(WebFrameTest, DisambiguationPopupPageScale)
{
    registerMockedHttpURLLoad("disambiguation_popup_page_scale.html");

    DisambiguationPopupTestWebViewClient client;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "disambiguation_popup_page_scale.html", true, 0, &client);
    webViewHelper.webView()->resize(WebSize(1000, 1000));
    webViewHelper.webView()->layout();

    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(80, 80));
    EXPECT_TRUE(client.triggered());

    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(230, 190));
    EXPECT_TRUE(client.triggered());

    webViewHelper.webView()->setPageScaleFactor(3.0f, WebPoint(0, 0));
    webViewHelper.webView()->layout();

    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(240, 240));
    EXPECT_TRUE(client.triggered());

    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(690, 570));
    EXPECT_FALSE(client.triggered());
}

class TestSubstituteDataWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
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

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad("about:blank", true, &webFrameClient);
    runPendingTasks();
    WebFrame* frame = webViewHelper.webView()->mainFrame();

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
    Platform::current()->unitTestSupport()->registerMockedErrorURL(URLTestHelpers::toKURL(errorURL), response, error);
    frame->loadHistoryItem(errorHistoryItem);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();

    WebString text = frame->contentAsText(std::numeric_limits<size_t>::max());
    EXPECT_EQ("This should appear", text.utf8());
    EXPECT_TRUE(webFrameClient.commitCalled());
}

class TestWillInsertBodyWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "clipped-body.html", false, &webFrameClient);

    EXPECT_TRUE(webFrameClient.m_didLoad);
    EXPECT_EQ(1, webFrameClient.m_numBodies);
}

TEST_F(WebFrameTest, EmptyDocument)
{
    registerMockedHttpURLLoad("pageserializer/green_rectangle.svg");

    TestWillInsertBodyWebFrameClient webFrameClient;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize(false, &webFrameClient);

    EXPECT_FALSE(webFrameClient.m_didLoad);
    EXPECT_EQ(1, webFrameClient.m_numBodies); // The empty document that a new frame starts with triggers this.
}

TEST_F(WebFrameTest, MoveCaretSelectionTowardsWindowPointWithNoSelection)
{
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad("about:blank", true);
    WebFrame* frame = webViewHelper.webView()->mainFrame();

    // This test passes if this doesn't crash.
    frame->moveCaretSelection(WebPoint(0, 0));
}

class SpellCheckClient : public WebSpellCheckClient {
public:
    explicit SpellCheckClient(uint32_t hash = 0) : m_numberOfTimesChecked(0), m_hash(hash) { }
    virtual ~SpellCheckClient() { }
    virtual void requestCheckingOfText(const blink::WebString&, const blink::WebVector<uint32_t>&, const blink::WebVector<unsigned>&, blink::WebTextCheckingCompletion* completion) OVERRIDE
    {
        ++m_numberOfTimesChecked;
        Vector<WebTextCheckingResult> results;
        const int misspellingStartOffset = 1;
        const int misspellingLength = 8;
        results.append(WebTextCheckingResult(WebTextDecorationTypeSpelling, misspellingStartOffset, misspellingLength, WebString(), m_hash));
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "spell.html");
    SpellCheckClient spellcheck;
    webViewHelper.webView()->setSpellCheckClient(&spellcheck);

    WebFrameImpl* frame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
    Document* document = frame->frame()->document();
    Element* element = document->getElementById("data");

    webViewHelper.webView()->settings()->setAsynchronousSpellCheckingEnabled(true);
    webViewHelper.webView()->settings()->setUnifiedTextCheckerEnabled(true);
    webViewHelper.webView()->settings()->setEditingBehavior(WebSettings::EditingBehaviorWin);

    element->focus();
    document->execCommand("InsertText", false, "_wellcome_.");

    const int allTextBeginOffset = 0;
    const int allTextLength = 11;
    frame->selectRange(WebRange::fromDocumentRange(frame, allTextBeginOffset, allTextLength));
    RefPtr<Range> selectionRange = frame->frame()->selection().toNormalizedRange();

    EXPECT_EQ(1, spellcheck.numberOfTimesChecked());
    EXPECT_EQ(1U, document->markers().markersInRange(selectionRange.get(), DocumentMarker::Spelling).size());

    frame->replaceMisspelledRange("welcome");
    EXPECT_EQ("_welcome_.", frame->contentAsText(std::numeric_limits<size_t>::max()).utf8());
}

TEST_F(WebFrameTest, RemoveSpellingMarkers)
{
    registerMockedHttpURLLoad("spell.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "spell.html");
    SpellCheckClient spellcheck;
    webViewHelper.webView()->setSpellCheckClient(&spellcheck);

    WebFrameImpl* frame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
    Document* document = frame->frame()->document();
    Element* element = document->getElementById("data");

    webViewHelper.webView()->settings()->setAsynchronousSpellCheckingEnabled(true);
    webViewHelper.webView()->settings()->setUnifiedTextCheckerEnabled(true);
    webViewHelper.webView()->settings()->setEditingBehavior(WebSettings::EditingBehaviorWin);

    element->focus();
    document->execCommand("InsertText", false, "_wellcome_.");

    frame->removeSpellingMarkers();

    const int allTextBeginOffset = 0;
    const int allTextLength = 11;
    frame->selectRange(WebRange::fromDocumentRange(frame, allTextBeginOffset, allTextLength));
    RefPtr<Range> selectionRange = frame->frame()->selection().toNormalizedRange();

    EXPECT_EQ(0U, document->markers().markersInRange(selectionRange.get(), DocumentMarker::Spelling).size());
}

TEST_F(WebFrameTest, MarkerHashIdentifiers) {
    registerMockedHttpURLLoad("spell.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "spell.html");

    static const uint32_t kHash = 42;
    SpellCheckClient spellcheck(kHash);
    webViewHelper.webView()->setSpellCheckClient(&spellcheck);

    WebFrameImpl* frame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
    Document* document = frame->frame()->document();
    Element* element = document->getElementById("data");

    webViewHelper.webView()->settings()->setAsynchronousSpellCheckingEnabled(true);
    webViewHelper.webView()->settings()->setUnifiedTextCheckerEnabled(true);
    webViewHelper.webView()->settings()->setEditingBehavior(WebSettings::EditingBehaviorWin);

    element->focus();
    document->execCommand("InsertText", false, "wellcome.");

    WebVector<uint32_t> documentMarkers;
    webViewHelper.webView()->spellingMarkers(&documentMarkers);
    EXPECT_EQ(1U, documentMarkers.size());
    EXPECT_EQ(kHash, documentMarkers[0]);
}

class StubbornSpellCheckClient : public WebSpellCheckClient {
public:
    StubbornSpellCheckClient() : m_completion(0) { }
    virtual ~StubbornSpellCheckClient() { }

    virtual void requestCheckingOfText(
        const blink::WebString&,
        const blink::WebVector<uint32_t>&,
        const blink::WebVector<unsigned>&,
        blink::WebTextCheckingCompletion* completion) OVERRIDE
    {
        m_completion = completion;
    }

    void kickNoResults()
    {
        kick(-1, -1, WebTextDecorationTypeSpelling);
    }

    void kick()
    {
        kick(1, 8, WebTextDecorationTypeSpelling);
    }

    void kickGrammar()
    {
        kick(1, 8, WebTextDecorationTypeGrammar);
    }

    void kickInvisibleSpellcheck()
    {
        kick(1, 8, WebTextDecorationTypeInvisibleSpellcheck);
    }

private:
    void kick(int misspellingStartOffset, int misspellingLength, WebTextDecorationType type)
    {
        if (!m_completion)
            return;
        Vector<WebTextCheckingResult> results;
        if (misspellingStartOffset >= 0 && misspellingLength > 0)
            results.append(WebTextCheckingResult(type, misspellingStartOffset, misspellingLength));
        m_completion->didFinishCheckingText(results);
        m_completion = 0;
    }

    blink::WebTextCheckingCompletion* m_completion;
};

TEST_F(WebFrameTest, SlowSpellcheckMarkerPosition)
{
    registerMockedHttpURLLoad("spell.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "spell.html");

    StubbornSpellCheckClient spellcheck;
    webViewHelper.webView()->setSpellCheckClient(&spellcheck);

    WebFrameImpl* frame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
    WebInputElement webInputElement = frame->document().getElementById("data").to<WebInputElement>();
    Document* document = frame->frame()->document();
    Element* element = document->getElementById("data");

    webViewHelper.webView()->settings()->setAsynchronousSpellCheckingEnabled(true);
    webViewHelper.webView()->settings()->setUnifiedTextCheckerEnabled(true);
    webViewHelper.webView()->settings()->setEditingBehavior(WebSettings::EditingBehaviorWin);

    element->focus();
    document->execCommand("InsertText", false, "wellcome ");
    webInputElement.setSelectionRange(0, 0);
    document->execCommand("InsertText", false, "he");

    spellcheck.kick();

    WebVector<uint32_t> documentMarkers;
    webViewHelper.webView()->spellingMarkers(&documentMarkers);
    EXPECT_EQ(0U, documentMarkers.size());
}

// This test verifies that cancelling spelling request does not cause a
// write-after-free when there's no spellcheck client set.
TEST_F(WebFrameTest, CancelSpellingRequestCrash)
{
    registerMockedHttpURLLoad("spell.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "spell.html");
    webViewHelper.webView()->setSpellCheckClient(0);

    WebFrameImpl* frame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
    Document* document = frame->frame()->document();
    Element* element = document->getElementById("data");

    webViewHelper.webView()->settings()->setAsynchronousSpellCheckingEnabled(true);
    webViewHelper.webView()->settings()->setUnifiedTextCheckerEnabled(true);
    webViewHelper.webView()->settings()->setEditingBehavior(WebSettings::EditingBehaviorWin);

    element->focus();
    frame->frame()->editor().replaceSelectionWithText("A", false, false);
    frame->frame()->spellChecker().cancelCheck();
}

TEST_F(WebFrameTest, SpellcheckResultErasesMarkers)
{
    registerMockedHttpURLLoad("spell.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "spell.html");

    StubbornSpellCheckClient spellcheck;
    webViewHelper.webView()->setSpellCheckClient(&spellcheck);

    WebFrameImpl* frame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
    WebInputElement webInputElement = frame->document().getElementById("data").to<WebInputElement>();
    Document* document = frame->frame()->document();
    Element* element = document->getElementById("data");

    webViewHelper.webView()->settings()->setAsynchronousSpellCheckingEnabled(true);
    webViewHelper.webView()->settings()->setUnifiedTextCheckerEnabled(true);
    webViewHelper.webView()->settings()->setEditingBehavior(WebSettings::EditingBehaviorWin);

    element->focus();
    document->execCommand("InsertText", false, "welcome ");
    document->markers().addMarker(rangeOfContents(element->toNode()).get(), DocumentMarker::Spelling);
    document->markers().addMarker(rangeOfContents(element->toNode()).get(), DocumentMarker::Grammar);
    document->markers().addMarker(rangeOfContents(element->toNode()).get(), DocumentMarker::InvisibleSpellcheck);
    EXPECT_EQ(3U, document->markers().markers().size());

    spellcheck.kickNoResults();
    EXPECT_EQ(0U, document->markers().markers().size());
}

TEST_F(WebFrameTest, SpellcheckResultsSavedInDocument)
{
    registerMockedHttpURLLoad("spell.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "spell.html");

    StubbornSpellCheckClient spellcheck;
    webViewHelper.webView()->setSpellCheckClient(&spellcheck);

    WebFrameImpl* frame = toWebFrameImpl(webViewHelper.webView()->mainFrame());
    WebInputElement webInputElement = frame->document().getElementById("data").to<WebInputElement>();
    Document* document = frame->frame()->document();
    Element* element = document->getElementById("data");

    webViewHelper.webView()->settings()->setAsynchronousSpellCheckingEnabled(true);
    webViewHelper.webView()->settings()->setUnifiedTextCheckerEnabled(true);
    webViewHelper.webView()->settings()->setEditingBehavior(WebSettings::EditingBehaviorWin);

    element->focus();
    document->execCommand("InsertText", false, "wellcome ");

    spellcheck.kick();
    ASSERT_EQ(1U, document->markers().markers().size());
    ASSERT_NE(static_cast<DocumentMarker*>(0), document->markers().markers()[0]);
    EXPECT_EQ(DocumentMarker::Spelling, document->markers().markers()[0]->type());

    document->execCommand("InsertText", false, "wellcome ");

    spellcheck.kickGrammar();
    ASSERT_EQ(1U, document->markers().markers().size());
    ASSERT_NE(static_cast<DocumentMarker*>(0), document->markers().markers()[0]);
    EXPECT_EQ(DocumentMarker::Grammar, document->markers().markers()[0]->type());

    document->execCommand("InsertText", false, "wellcome ");

    spellcheck.kickInvisibleSpellcheck();
    ASSERT_EQ(1U, document->markers().markers().size());
    ASSERT_NE(static_cast<DocumentMarker*>(0), document->markers().markers()[0]);
    EXPECT_EQ(DocumentMarker::InvisibleSpellcheck, document->markers().markers()[0]->type());
}

class TestAccessInitialDocumentWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize(true, &webFrameClient);
    runPendingTasks();
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Create another window that will try to access it.
    FrameTestHelpers::WebViewHelper newWebViewHelper;
    WebView* newView = newWebViewHelper.initialize(true);
    newView->mainFrame()->setOpener(webViewHelper.webView()->mainFrame());
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
}

TEST_F(WebFrameTest, DidAccessInitialDocumentNavigator)
{
    TestAccessInitialDocumentWebFrameClient webFrameClient;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize(true, &webFrameClient);
    runPendingTasks();
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Create another window that will try to access it.
    FrameTestHelpers::WebViewHelper newWebViewHelper;
    WebView* newView = newWebViewHelper.initialize(true);
    newView->mainFrame()->setOpener(webViewHelper.webView()->mainFrame());
    runPendingTasks();
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Access the initial document to get to the navigator object.
    newView->mainFrame()->executeScript(
        WebScriptSource("console.log(window.opener.navigator);"));
    runPendingTasks();
    EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);
}

TEST_F(WebFrameTest, DidAccessInitialDocumentViaJavascriptUrl)
{
    TestAccessInitialDocumentWebFrameClient webFrameClient;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize(true, &webFrameClient);
    runPendingTasks();
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Access the initial document from a javascript: URL.
    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), "javascript:document.body.appendChild(document.createTextNode('Modified'))");
    runPendingTasks();
    EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);
}

// Fails on the WebKit XP (deps) bot. http://crbug.com/312192
#if OS(WIN)
TEST_F(WebFrameTest, DISABLED_DidAccessInitialDocumentBodyBeforeModalDialog)
#else
TEST_F(WebFrameTest, DidAccessInitialDocumentBodyBeforeModalDialog)
#endif
{
    TestAccessInitialDocumentWebFrameClient webFrameClient;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize(true, &webFrameClient);
    runPendingTasks();
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Create another window that will try to access it.
    FrameTestHelpers::WebViewHelper newWebViewHelper;
    WebView* newView = newWebViewHelper.initialize(true);
    newView->mainFrame()->setOpener(webViewHelper.webView()->mainFrame());
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
}

// Fails on the WebKit XP (deps) bot. http://crbug.com/312192
#if OS(WIN)
TEST_F(WebFrameTest, DISABLED_DidWriteToInitialDocumentBeforeModalDialog)
#else
TEST_F(WebFrameTest, DidWriteToInitialDocumentBeforeModalDialog)
#endif
{
    TestAccessInitialDocumentWebFrameClient webFrameClient;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize(true, &webFrameClient);
    runPendingTasks();
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Create another window that will try to access it.
    FrameTestHelpers::WebViewHelper newWebViewHelper;
    WebView* newView = newWebViewHelper.initialize(true);
    newView->mainFrame()->setOpener(webViewHelper.webView()->mainFrame());
    runPendingTasks();
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Access the initial document with document.write, which moves us past the
    // initial empty document state of the state machine. We normally set a
    // timer to notify the client.
    newView->mainFrame()->executeScript(
        WebScriptSource("window.opener.document.write('Modified');"));
    EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

    // Make sure that a modal dialog forces us to notify right away.
    newView->mainFrame()->executeScript(
        WebScriptSource("window.opener.confirm('Modal');"));
    EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);

    // Ensure that we don't notify again later.
    runPendingTasks();
    EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);
}

class TestMainFrameUserOrProgrammaticScrollFrameClient : public FrameTestHelpers::TestWebFrameClient {
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
        // which is called from LocalFrame::createView (before the frame is associated
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "long_scroll.html", true, &client);
    webViewHelper.webView()->resize(WebSize(1000, 1000));
    webViewHelper.webView()->layout();

    EXPECT_FALSE(client.wasUserScroll());
    EXPECT_FALSE(client.wasProgrammaticScroll());

    // Do a compositor scroll, verify that this is counted as a user scroll.
    webViewHelper.webViewImpl()->applyScrollAndScale(WebSize(0, 1), 1.1f);
    EXPECT_TRUE(client.wasUserScroll());
    client.reset();

    EXPECT_FALSE(client.wasUserScroll());
    EXPECT_FALSE(client.wasProgrammaticScroll());

    // The page scale 1.0f and scroll.
    webViewHelper.webViewImpl()->applyScrollAndScale(WebSize(0, 1), 1.0f);
    EXPECT_TRUE(client.wasUserScroll());
    client.reset();

    // No scroll event if there is no scroll delta.
    webViewHelper.webViewImpl()->applyScrollAndScale(WebSize(), 1.0f);
    EXPECT_FALSE(client.wasUserScroll());
    EXPECT_FALSE(client.wasProgrammaticScroll());
    client.reset();

    // Non zero page scale and scroll.
    webViewHelper.webViewImpl()->applyScrollAndScale(WebSize(9, 13), 0.6f);
    EXPECT_TRUE(client.wasUserScroll());
    client.reset();

    // Programmatic scroll.
    WebFrameImpl* frameImpl = webViewHelper.webViewImpl()->mainFrameImpl();
    frameImpl->executeScript(WebScriptSource("window.scrollTo(0, 20);"));
    EXPECT_FALSE(client.wasUserScroll());
    EXPECT_TRUE(client.wasProgrammaticScroll());
    client.reset();

    // Programmatic scroll to same offset. No scroll event should be generated.
    frameImpl->executeScript(WebScriptSource("window.scrollTo(0, 20);"));
    EXPECT_FALSE(client.wasProgrammaticScroll());
    EXPECT_FALSE(client.wasUserScroll());
    client.reset();
}

TEST_F(WebFrameTest, CompositorScrollIsUserScrollShortPage)
{
    registerMockedHttpURLLoad("short_scroll.html");

    TestMainFrameUserOrProgrammaticScrollFrameClient client;

    // Short page tests.
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "short_scroll.html", true, &client);

    webViewHelper.webView()->resize(WebSize(1000, 1000));
    webViewHelper.webView()->layout();

    EXPECT_FALSE(client.wasUserScroll());
    EXPECT_FALSE(client.wasProgrammaticScroll());

    // Non zero page scale and scroll.
    webViewHelper.webViewImpl()->applyScrollAndScale(WebSize(9, 13), 2.0f);
    EXPECT_FALSE(client.wasProgrammaticScroll());
    EXPECT_TRUE(client.wasUserScroll());
    client.reset();
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

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "first_party_redirect.html", true);
    EXPECT_TRUE(webViewHelper.webView()->mainFrame()->document().firstPartyForCookies() == redirectURL);
}

class TestNavigationPolicyWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fragment_middle_click.html", true, &client);

    WebCore::Document* document = webViewHelper.webViewImpl()->page()->mainFrame()->document();
    WebCore::KURL destination = document->url();
    destination.setFragmentIdentifier("test");

    RefPtr<WebCore::Event> event = WebCore::MouseEvent::create(WebCore::EventTypeNames::click, false, false,
        document->domWindow(), 0, 0, 0, 0, 0, 0, 0, false, false, false, false, 1, nullptr, nullptr);
    WebCore::FrameLoadRequest frameRequest(document, WebCore::ResourceRequest(destination));
    frameRequest.setTriggeringEvent(event);
    webViewHelper.webViewImpl()->page()->mainFrame()->loader().load(frameRequest);
}

class TestNewWindowWebViewClient : public WebViewClient {
public:
    virtual WebView* createView(WebFrame*, const WebURLRequest&, const WebWindowFeatures&,
        const WebString&, WebNavigationPolicy, bool) OVERRIDE
    {
        EXPECT_TRUE(false);
        return 0;
    }
};

class TestNewWindowWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
public:
    TestNewWindowWebFrameClient()
        : m_decidePolicyCallCount(0)
    {
    }

    virtual WebNavigationPolicy decidePolicyForNavigation(WebFrame*, WebDataSource::ExtraData*, const WebURLRequest&,
        WebNavigationType, WebNavigationPolicy policy, bool) OVERRIDE
    {
        m_decidePolicyCallCount++;
        return policy;
    }

    int decidePolicyCallCount() const { return m_decidePolicyCallCount; }

private:
    int m_decidePolicyCallCount;
};

TEST_F(WebFrameTest, ModifiedClickNewWindow)
{
    registerMockedHttpURLLoad("ctrl_click.html");
    registerMockedHttpURLLoad("hello_world.html");
    TestNewWindowWebViewClient webViewClient;
    TestNewWindowWebFrameClient webFrameClient;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "ctrl_click.html", true, &webFrameClient, &webViewClient);

    WebCore::Document* document = webViewHelper.webViewImpl()->page()->mainFrame()->document();
    WebCore::KURL destination = toKURL(m_baseURL + "hello_world.html");

    // ctrl+click event
    RefPtr<WebCore::Event> event = WebCore::MouseEvent::create(WebCore::EventTypeNames::click, false, false,
        document->domWindow(), 0, 0, 0, 0, 0, 0, 0, true, false, false, false, 0, nullptr, nullptr);
    WebCore::FrameLoadRequest frameRequest(document, WebCore::ResourceRequest(destination));
    frameRequest.setTriggeringEvent(event);
    WebCore::UserGestureIndicator gesture(WebCore::DefinitelyProcessingUserGesture);
    webViewHelper.webViewImpl()->page()->mainFrame()->loader().load(frameRequest);

    // decidePolicyForNavigation should be called both for the original request and the ctrl+click.
    EXPECT_EQ(2, webFrameClient.decidePolicyCallCount());
}

TEST_F(WebFrameTest, BackToReload)
{
    registerMockedHttpURLLoad("fragment_middle_click.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fragment_middle_click.html", true);
    WebFrame* frame = webViewHelper.webView()->mainFrame();
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
}

TEST_F(WebFrameTest, BackDuringChildFrameReload)
{
    registerMockedHttpURLLoad("page_with_blank_iframe.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "page_with_blank_iframe.html", true);
    WebFrame* mainFrame = webViewHelper.webView()->mainFrame();
    WebFrame* childFrame = mainFrame->firstChild();
    ASSERT_TRUE(childFrame);

    // Start a history navigation, then have a different frame commit a navigation.
    // In this case, reload an about:blank frame, which will commit synchronously.
    // After the history navigation completes, both the appropriate document url and
    // the current history item should reflect the history navigation.
    registerMockedHttpURLLoad("white-1x1.png");
    WebHistoryItem item;
    item.initialize();
    WebURL historyURL(toKURL(m_baseURL + "white-1x1.png"));
    item.setURLString(historyURL.string());
    mainFrame->loadHistoryItem(item);

    childFrame->reload();
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    EXPECT_EQ(item.urlString(), mainFrame->document().url().string());
    EXPECT_EQ(item.urlString(), mainFrame->currentHistoryItem().urlString());
}

TEST_F(WebFrameTest, ReloadPost)
{
    registerMockedHttpURLLoad("reload_post.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "reload_post.html", true);
    WebFrame* frame = webViewHelper.webView()->mainFrame();

    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), "javascript:document.forms[0].submit()");
    runPendingTasks();
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    EXPECT_FALSE(frame->previousHistoryItem().isNull());
    EXPECT_EQ(WebString::fromUTF8("POST"), frame->dataSource()->request().httpMethod());

    frame->reload();
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    EXPECT_EQ(WebURLRequest::ReloadIgnoringCacheData, frame->dataSource()->request().cachePolicy());
    EXPECT_EQ(WebNavigationTypeFormResubmitted, frame->dataSource()->navigationType());
}

TEST_F(WebFrameTest, LoadHistoryItemReload)
{
    registerMockedHttpURLLoad("fragment_middle_click.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fragment_middle_click.html", true);
    WebFrame* frame = webViewHelper.webView()->mainFrame();
    WebHistoryItem firstItem = frame->currentHistoryItem();
    EXPECT_FALSE(firstItem.isNull());

    registerMockedHttpURLLoad("white-1x1.png");
    FrameTestHelpers::loadFrame(frame, m_baseURL + "white-1x1.png");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    EXPECT_FALSE(frame->previousHistoryItem().isNull());
    EXPECT_EQ(firstItem.urlString(), frame->previousHistoryItem().urlString());

    // Cache policy overrides should take.
    frame->loadHistoryItem(frame->previousHistoryItem(), WebURLRequest::ReloadIgnoringCacheData);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    EXPECT_EQ(firstItem.urlString(), frame->currentHistoryItem().urlString());
    EXPECT_EQ(WebURLRequest::ReloadIgnoringCacheData, frame->dataSource()->request().cachePolicy());
}


class TestCachePolicyWebFrameClient : public WebFrameClient {
public:
    TestCachePolicyWebFrameClient()
        : m_policy(WebURLRequest::UseProtocolCachePolicy)
        , m_client(0)
        , m_willSendRequestCallCount(0)
        , m_childFrameCreationCount(0)
    {
    }

    void setChildWebFrameClient(WebFrameClient* client) { m_client = client; }
    WebURLRequest::CachePolicy cachePolicy() const { return m_policy; }
    int willSendRequestCallCount() const { return m_willSendRequestCallCount; }
    int childFrameCreationCount() const { return m_childFrameCreationCount; }

    virtual WebFrame* createChildFrame(WebFrame* parent, const WebString&)
    {
        m_childFrameCreationCount++;
        WebFrame* frame = WebFrame::create(m_client);
        parent->appendChild(frame);
        return frame;
    }

    virtual void frameDetached(WebFrame* frame) OVERRIDE
    {
        if (frame->parent())
            frame->parent()->removeChild(frame);
        frame->close();
    }

    virtual void willSendRequest(WebFrame* frame, unsigned, WebURLRequest& request, const WebURLResponse&) OVERRIDE
    {
        m_policy = request.cachePolicy();
        m_willSendRequestCallCount++;
    }

private:
    WebURLRequest::CachePolicy m_policy;
    WebFrameClient* m_client;
    int m_willSendRequestCallCount;
    int m_childFrameCreationCount;
};

TEST_F(WebFrameTest, ReloadIframe)
{
    registerMockedHttpURLLoad("iframe_reload.html");
    registerMockedHttpURLLoad("visible_iframe.html");
    TestCachePolicyWebFrameClient mainClient;
    TestCachePolicyWebFrameClient childClient;
    mainClient.setChildWebFrameClient(&childClient);

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "iframe_reload.html", true, &mainClient);

    WebFrameImpl* mainFrame = webViewHelper.webViewImpl()->mainFrameImpl();
    RefPtr<WebFrameImpl> childFrame = toWebFrameImpl(mainFrame->firstChild());
    ASSERT_EQ(childFrame->client(), &childClient);
    EXPECT_EQ(mainClient.childFrameCreationCount(), 1);
    EXPECT_EQ(childClient.willSendRequestCallCount(), 1);
    EXPECT_EQ(childClient.cachePolicy(), WebURLRequest::UseProtocolCachePolicy);

    mainFrame->reload(false);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();

    // A new WebFrame should have been created, but the child WebFrameClient should be reused.
    ASSERT_NE(childFrame, toWebFrameImpl(mainFrame->firstChild()));
    ASSERT_EQ(toWebFrameImpl(mainFrame->firstChild())->client(), &childClient);

    EXPECT_EQ(mainClient.childFrameCreationCount(), 2);
    EXPECT_EQ(childClient.willSendRequestCallCount(), 2);
    EXPECT_EQ(childClient.cachePolicy(), WebURLRequest::ReloadIgnoringCacheData);
}

TEST_F(WebFrameTest, ExportHistoryItemFromChildFrame)
{
    registerMockedHttpURLLoad("iframe_reload.html");
    registerMockedHttpURLLoad("visible_iframe.html");
    TestCachePolicyWebFrameClient mainClient;
    TestCachePolicyWebFrameClient childClient;
    mainClient.setChildWebFrameClient(&childClient);

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "iframe_reload.html", true, &mainClient);

    WebFrame* childFrame = webViewHelper.webViewImpl()->mainFrameImpl()->firstChild();
    WebHistoryItem item = childFrame->currentHistoryItem();
    EXPECT_EQ(item.urlString().utf8(), m_baseURL + "iframe_reload.html");
}

class TestSameDocumentWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
public:
    TestSameDocumentWebFrameClient()
        : m_frameLoadTypeSameSeen(false)
    {
    }

    virtual void willSendRequest(WebFrame* frame, unsigned, WebURLRequest&, const WebURLResponse&)
    {
        if (toWebFrameImpl(frame)->frame()->loader().loadType() == WebCore::FrameLoadTypeSame)
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
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "navigate_to_same.html", true, &client);
    EXPECT_FALSE(client.frameLoadTypeSameSeen());

    WebCore::FrameLoadRequest frameRequest(0, WebCore::ResourceRequest(webViewHelper.webViewImpl()->page()->mainFrame()->document()->url()));
    webViewHelper.webViewImpl()->page()->mainFrame()->loader().load(frameRequest);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();

    EXPECT_TRUE(client.frameLoadTypeSameSeen());
}

TEST_F(WebFrameTest, WebNodeImageContents)
{
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad("about:blank", true);
    WebFrame* frame = webViewHelper.webView()->mainFrame();

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
        , m_differentDocumentStartCount(0)
    {
    }

    virtual void didStartLoading(bool toDifferentDocument) OVERRIDE
    {
        m_startLoadingCount++;
        if (toDifferentDocument)
            m_differentDocumentStartCount++;
    }

    virtual void didStopLoading() OVERRIDE
    {
        m_stopLoadingCount++;
    }

    int startLoadingCount() const { return m_startLoadingCount; }
    int stopLoadingCount() const { return m_stopLoadingCount; }
    int differentDocumentStartCount() const { return m_differentDocumentStartCount; }

private:
    int m_startLoadingCount;
    int m_stopLoadingCount;
    int m_differentDocumentStartCount;
};

TEST_F(WebFrameTest, PushStateStartsAndStops)
{
    registerMockedHttpURLLoad("push_state.html");
    TestStartStopCallbackWebViewClient client;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "push_state.html", true, 0, &client);
    runPendingTasks();

    EXPECT_EQ(client.startLoadingCount(), 2);
    EXPECT_EQ(client.stopLoadingCount(), 2);
    EXPECT_EQ(client.differentDocumentStartCount(), 1);
}

class TestHistoryWebFrameClient : public WebFrameClient {
public:
    TestHistoryWebFrameClient()
    {
        m_replacesCurrentHistoryItem = false;
        m_frame = 0;
    }
    void didStartProvisionalLoad(WebFrame* frame)
    {
        WebDataSource* ds = frame->provisionalDataSource();
        m_replacesCurrentHistoryItem = ds->replacesCurrentHistoryItem();
        m_frame = frame;
    }

    bool replacesCurrentHistoryItem() { return m_replacesCurrentHistoryItem; }
    WebFrame* frame() { return m_frame; }

private:
    bool m_replacesCurrentHistoryItem;
    WebFrame* m_frame;
};

// Test which ensures that the first navigation in a subframe will always
// result in history entry being replaced and not a new one added.
TEST_F(WebFrameTest, DISABLED_FirstFrameNavigationReplacesHistory)
{
    registerMockedHttpURLLoad("history.html");
    registerMockedHttpURLLoad("find.html");

    FrameTestHelpers::WebViewHelper webViewHelper;
    TestHistoryWebFrameClient client;
    webViewHelper.initializeAndLoad("about:blank", true, &client);
    runPendingTasks();
    EXPECT_TRUE(client.replacesCurrentHistoryItem());

    WebFrame* frame = webViewHelper.webView()->mainFrame();

    FrameTestHelpers::loadFrame(frame,
        "javascript:document.body.appendChild(document.createElement('iframe'))");
    // Need to call runPendingTasks in order for the JavaScript above to be
    // evaluated and executed.
    runPendingTasks();
    WebFrame* iframe = frame->firstChild();
    EXPECT_EQ(client.frame(), iframe);
    EXPECT_TRUE(client.replacesCurrentHistoryItem());

    FrameTestHelpers::loadFrame(frame,
        "javascript:window.frames[0].location.assign('" + m_baseURL + "history.html')");
    runPendingTasks();
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    EXPECT_EQ(client.frame(), iframe);
    EXPECT_TRUE(client.replacesCurrentHistoryItem());

    FrameTestHelpers::loadFrame(frame,
        "javascript:window.frames[0].location.assign('" + m_baseURL + "find.html')");
    runPendingTasks();
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    EXPECT_EQ(client.frame(), iframe);
    EXPECT_FALSE(client.replacesCurrentHistoryItem());

    // Repeat the test, but start out the iframe with initial URL, which is not
    // "about:blank".
    FrameTestHelpers::loadFrame(frame,
        "javascript:var f = document.createElement('iframe'); "
        "f.src = '" + m_baseURL + "history.html';"
        "document.body.appendChild(f)");
    runPendingTasks();
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();

    iframe = frame->firstChild()->nextSibling();
    EXPECT_EQ(client.frame(), iframe);
    EXPECT_TRUE(client.replacesCurrentHistoryItem());

    FrameTestHelpers::loadFrame(frame,
        "javascript:window.frames[1].location.assign('" + m_baseURL + "find.html')");
    runPendingTasks();
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    EXPECT_EQ(client.frame(), iframe);
    EXPECT_FALSE(client.replacesCurrentHistoryItem());
}

// This tests the restore case where the first load in a page is
// via loadHistoryItem(). If multiple pages are in the same process
// and are restoring around the same time, they may not restore in
// the order they were created and may end up with different names
// than they were given when they were saved. If the initial item
// has a child with url "about:blank", we should still navigate the
// main frame to the parent, rather than incorrectly matching the
// blank child to the main frame.
TEST_F(WebFrameTest, firstNavigationIsHistoryWithBlankChild)
{
    registerMockedHttpURLLoad("history.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    WebViewImpl* webView = webViewHelper.initialize();
    ASSERT_TRUE(webView->mainFrame()->currentHistoryItem().isNull());

    WebHistoryItem item;
    item.initialize();
    WebURL destinationURL(toKURL(m_baseURL + "history.html"));
    item.setURLString(destinationURL.string());
    item.setTarget(WebString::fromUTF8("expectedButMissingMainFrameName"));

    WebHistoryItem childItem;
    childItem.initialize();
    childItem.setURLString("about:blank");
    item.appendToChildren(childItem);

    webView->mainFrame()->loadHistoryItem(item);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    EXPECT_EQ(destinationURL, webView->mainFrame()->document().url());
}

// Test verifies that layout will change a layer's scrollable attibutes
TEST_F(WebFrameTest, overflowHiddenRewrite)
{
    registerMockedHttpURLLoad("non-scrollable.html");
    TestMainFrameUserOrProgrammaticScrollFrameClient client;
    OwnPtr<FakeCompositingWebViewClient> fakeCompositingWebViewClient = adoptPtr(new FakeCompositingWebViewClient());
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize(true, 0, fakeCompositingWebViewClient.get(), &configueCompositingWebView);

    webViewHelper.webView()->resize(WebSize(100, 100));
    FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(), m_baseURL + "non-scrollable.html");
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    webViewHelper.webView()->layout();

    WebCore::RenderLayerCompositor* compositor =  webViewHelper.webViewImpl()->compositor();
    ASSERT_TRUE(compositor->scrollLayer());

    // Verify that the WebLayer is not scrollable initially.
    WebCore::GraphicsLayer* scrollLayer = compositor->scrollLayer();
    WebLayer* webScrollLayer = scrollLayer->platformLayer();
    ASSERT_FALSE(webScrollLayer->userScrollableHorizontal());
    ASSERT_FALSE(webScrollLayer->userScrollableVertical());

    // Call javascript to make the layer scrollable, and verify it.
    WebFrameImpl* frame = (WebFrameImpl*)webViewHelper.webView()->mainFrame();
    frame->executeScript(WebScriptSource("allowScroll();"));
    webViewHelper.webView()->layout();
    ASSERT_TRUE(webScrollLayer->userScrollableHorizontal());
    ASSERT_TRUE(webScrollLayer->userScrollableVertical());
}

// Test that currentHistoryItem reflects the current page, not the provisional load.
TEST_F(WebFrameTest, CurrentHistoryItem)
{
    registerMockedHttpURLLoad("fixed_layout.html");
    std::string url = m_baseURL + "fixed_layout.html";

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize();
    WebFrame* frame = webViewHelper.webView()->mainFrame();
    FrameTestHelpers::loadFrame(frame, url);

    // Before commit, there is no history item.
    EXPECT_TRUE(frame->currentHistoryItem().isNull());

    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();

    // After commit, there is.
    WebHistoryItem item = frame->currentHistoryItem();
    ASSERT_FALSE(item.isNull());
    EXPECT_EQ(url, item.urlString().utf8());
}

class FailCreateChildFrame : public WebFrameClient {
public:
    FailCreateChildFrame() : m_callCount(0) { }

    virtual WebFrame* createChildFrame(WebFrame* parent, const WebString& frameName) OVERRIDE
    {
        ++m_callCount;
        return 0;
    }

    virtual void frameDetached(WebFrame* frame) OVERRIDE
    {
        frame->close();
    }

    int callCount() const { return m_callCount; }

private:
    int m_callCount;
};

// Test that we don't crash if WebFrameClient::createChildFrame() fails.
TEST_F(WebFrameTest, CreateChildFrameFailure)
{
    registerMockedHttpURLLoad("create_child_frame_fail.html");
    FailCreateChildFrame client;
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "create_child_frame_fail.html", true, &client);

    EXPECT_EQ(1, client.callCount());
}

TEST_F(WebFrameTest, fixedPositionInFixedViewport)
{
    UseMockScrollbarSettings mockScrollbarSettings;
    registerMockedHttpURLLoad("fixed-position-in-fixed-viewport.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "fixed-position-in-fixed-viewport.html", true, 0, 0, enableViewportSettings);

    WebView* webView = webViewHelper.webView();
    webView->resize(WebSize(100, 100));
    webView->layout();

    Document* document = toWebFrameImpl(webView->mainFrame())->frame()->document();
    Element* bottomFixed = document->getElementById("bottom-fixed");
    Element* topBottomFixed = document->getElementById("top-bottom-fixed");
    Element* rightFixed = document->getElementById("right-fixed");
    Element* leftRightFixed = document->getElementById("left-right-fixed");

    webView->resize(WebSize(100, 200));
    webView->layout();
    EXPECT_EQ(200, bottomFixed->offsetTop() + bottomFixed->offsetHeight());
    EXPECT_EQ(200, topBottomFixed->offsetHeight());

    webView->settings()->setMainFrameResizesAreOrientationChanges(false);
    webView->resize(WebSize(200, 200));
    webView->layout();
    EXPECT_EQ(200, rightFixed->offsetLeft() + rightFixed->offsetWidth());
    EXPECT_EQ(200, leftRightFixed->offsetWidth());

    webView->settings()->setMainFrameResizesAreOrientationChanges(true);
    // Will scale the page by 1.5.
    webView->resize(WebSize(300, 330));
    webView->layout();
    EXPECT_EQ(220, bottomFixed->offsetTop() + bottomFixed->offsetHeight());
    EXPECT_EQ(220, topBottomFixed->offsetHeight());
    EXPECT_EQ(200, rightFixed->offsetLeft() + rightFixed->offsetWidth());
    EXPECT_EQ(200, leftRightFixed->offsetWidth());
}

TEST_F(WebFrameTest, FrameViewSetFrameRect)
{
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad("about:blank");

    WebCore::FrameView* frameView = webViewHelper.webViewImpl()->mainFrameImpl()->frameView();
    frameView->setFrameRect(WebCore::IntRect(0, 0, 200, 200));
    EXPECT_EQ_RECT(WebCore::IntRect(0, 0, 200, 200), frameView->frameRect());
    frameView->setFrameRect(WebCore::IntRect(100, 100, 200, 200));
    EXPECT_EQ_RECT(WebCore::IntRect(100, 100, 200, 200), frameView->frameRect());
}

TEST_F(WebFrameTest, FullscreenLayerNonScrollable)
{
    FakeCompositingWebViewClient client;
    registerMockedHttpURLLoad("fullscreen_div.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    int viewportWidth = 640;
    int viewportHeight = 480;
    WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(m_baseURL + "fullscreen_div.html", true, 0, &client, &configueCompositingWebView);
    webViewImpl->settings()->setFullScreenEnabled(true);
    webViewImpl->resize(WebSize(viewportWidth, viewportHeight));
    webViewImpl->layout();

    Document* document = toWebFrameImpl(webViewImpl->mainFrame())->frame()->document();
    WebCore::UserGestureIndicator gesture(WebCore::DefinitelyProcessingUserGesture);
    Element* divFullscreen = document->getElementById("div1");
    divFullscreen->webkitRequestFullscreen();
    webViewImpl->willEnterFullScreen();
    webViewImpl->didEnterFullScreen();
    webViewImpl->layout();

    // Verify that the main frame is not scrollable.
    ASSERT_TRUE(WebCore::FullscreenElementStack::isFullScreen(*document));
    WebLayer* webScrollLayer = webViewImpl->compositor()->scrollLayer()->platformLayer();
    ASSERT_FALSE(webScrollLayer->scrollable());

    // Verify that the main frame is scrollable upon exiting fullscreen.
    webViewImpl->willExitFullScreen();
    webViewImpl->didExitFullScreen();
    webViewImpl->layout();
    ASSERT_FALSE(WebCore::FullscreenElementStack::isFullScreen(*document));
    webScrollLayer = webViewImpl->compositor()->scrollLayer()->platformLayer();
    ASSERT_TRUE(webScrollLayer->scrollable());
}

TEST_F(WebFrameTest, RenderBlockPercentHeightDescendants)
{
    registerMockedHttpURLLoad("percent-height-descendants.html");
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + "percent-height-descendants.html");

    WebView* webView = webViewHelper.webView();
    webView->resize(WebSize(800, 800));
    webView->layout();

    Document* document = toWebFrameImpl(webView->mainFrame())->frame()->document();
    WebCore::RenderBlock* container = WebCore::toRenderBlock(document->getElementById("container")->renderer());
    WebCore::RenderBox* percentHeightInAnonymous = WebCore::toRenderBox(document->getElementById("percent-height-in-anonymous")->renderer());
    WebCore::RenderBox* percentHeightDirectChild = WebCore::toRenderBox(document->getElementById("percent-height-direct-child")->renderer());

    EXPECT_TRUE(WebCore::RenderBlock::hasPercentHeightDescendant(percentHeightInAnonymous));
    EXPECT_TRUE(WebCore::RenderBlock::hasPercentHeightDescendant(percentHeightDirectChild));

    ASSERT_TRUE(container->percentHeightDescendants());
    ASSERT_TRUE(container->hasPercentHeightDescendants());
    EXPECT_EQ(2U, container->percentHeightDescendants()->size());
    EXPECT_TRUE(container->percentHeightDescendants()->contains(percentHeightInAnonymous));
    EXPECT_TRUE(container->percentHeightDescendants()->contains(percentHeightDirectChild));

    WebCore::RenderBlock* anonymousBlock = percentHeightInAnonymous->containingBlock();
    EXPECT_TRUE(anonymousBlock->isAnonymous());
    EXPECT_FALSE(anonymousBlock->hasPercentHeightDescendants());
}

} // namespace
