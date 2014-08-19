/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "public/web/WebPluginContainer.h"

#include "core/dom/Element.h"
#include "core/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebClipboard.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebDocument.h"
#include "public/web/WebElement.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebPluginParams.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebPluginContainerImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FakeWebPlugin.h"
#include "web/tests/FrameTestHelpers.h"
#include <gtest/gtest.h>

using namespace blink;

namespace {

class WebPluginContainerTest : public testing::Test {
public:
    WebPluginContainerTest()
        : m_baseURL("http://www.test.com/")
    {
    }

    virtual void TearDown()
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

protected:
    std::string m_baseURL;
};

// Subclass of FakeWebPlugin that has a selection of 'x' as plain text and 'y' as markup text.
class TestPlugin : public FakeWebPlugin {
public:
    TestPlugin(WebFrame* frame, const WebPluginParams& params)
        : FakeWebPlugin(frame, params)
    {
    }

    virtual bool hasSelection() const { return true; }
    virtual WebString selectionAsText() const { return WebString("x"); }
    virtual WebString selectionAsMarkup() const { return WebString("y"); }
};

class TestPluginWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
    virtual WebPlugin* createPlugin(WebLocalFrame* frame, const WebPluginParams& params) OVERRIDE
    {
        if (params.mimeType == WebString::fromUTF8("application/x-webkit-test-webplugin"))
            return new TestPlugin(frame, params);
        return WebFrameClient::createPlugin(frame, params);
    }
};

WebPluginContainer* getWebPluginContainer(WebView* webView, const WebString& id)
{
    WebElement element = webView->mainFrame()->document().getElementById(id);
    return element.pluginContainer();
}

TEST_F(WebPluginContainerTest, WindowToLocalPointTest)
{
    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("plugin_container.html"));
    FrameTestHelpers::WebViewHelper webViewHelper;
    WebView* webView = webViewHelper.initializeAndLoad(m_baseURL + "plugin_container.html", true, new TestPluginWebFrameClient());
    ASSERT(webView);
    webView->settings()->setPluginsEnabled(true);
    webView->resize(WebSize(300, 300));
    webView->layout();
    FrameTestHelpers::runPendingTasks();

    WebPluginContainer* pluginContainerOne = getWebPluginContainer(webView, WebString::fromUTF8("translated-plugin"));
    ASSERT(pluginContainerOne);
    WebPoint point1 = pluginContainerOne->windowToLocalPoint(WebPoint(10, 10));
    ASSERT_EQ(0, point1.x);
    ASSERT_EQ(0, point1.y);
    WebPoint point2 = pluginContainerOne->windowToLocalPoint(WebPoint(100, 100));
    ASSERT_EQ(90, point2.x);
    ASSERT_EQ(90, point2.y);

    WebPluginContainer* pluginContainerTwo = getWebPluginContainer(webView, WebString::fromUTF8("rotated-plugin"));
    ASSERT(pluginContainerTwo);
    WebPoint point3 = pluginContainerTwo->windowToLocalPoint(WebPoint(0, 10));
    ASSERT_EQ(10, point3.x);
    ASSERT_EQ(0, point3.y);
    WebPoint point4 = pluginContainerTwo->windowToLocalPoint(WebPoint(-10, 10));
    ASSERT_EQ(10, point4.x);
    ASSERT_EQ(10, point4.y);
}

TEST_F(WebPluginContainerTest, LocalToWindowPointTest)
{
    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("plugin_container.html"));
    FrameTestHelpers::WebViewHelper webViewHelper;
    WebView* webView = webViewHelper.initializeAndLoad(m_baseURL + "plugin_container.html", true, new TestPluginWebFrameClient());
    ASSERT(webView);
    webView->settings()->setPluginsEnabled(true);
    webView->resize(WebSize(300, 300));
    webView->layout();
    FrameTestHelpers::runPendingTasks();

    WebPluginContainer* pluginContainerOne = getWebPluginContainer(webView, WebString::fromUTF8("translated-plugin"));
    ASSERT(pluginContainerOne);
    WebPoint point1 = pluginContainerOne->localToWindowPoint(WebPoint(0, 0));
    ASSERT_EQ(10, point1.x);
    ASSERT_EQ(10, point1.y);
    WebPoint point2 = pluginContainerOne->localToWindowPoint(WebPoint(90, 90));
    ASSERT_EQ(100, point2.x);
    ASSERT_EQ(100, point2.y);

    WebPluginContainer* pluginContainerTwo = getWebPluginContainer(webView, WebString::fromUTF8("rotated-plugin"));
    ASSERT(pluginContainerTwo);
    WebPoint point3 = pluginContainerTwo->localToWindowPoint(WebPoint(10, 0));
    ASSERT_EQ(0, point3.x);
    ASSERT_EQ(10, point3.y);
    WebPoint point4 = pluginContainerTwo->localToWindowPoint(WebPoint(10, 10));
    ASSERT_EQ(-10, point4.x);
    ASSERT_EQ(10, point4.y);
}

// Verifies executing the command 'Copy' results in copying to the clipboard.
TEST_F(WebPluginContainerTest, Copy)
{
    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("plugin_container.html"));
    FrameTestHelpers::WebViewHelper webViewHelper;
    WebView* webView = webViewHelper.initializeAndLoad(m_baseURL + "plugin_container.html", true, new TestPluginWebFrameClient());
    ASSERT(webView);
    webView->settings()->setPluginsEnabled(true);
    webView->resize(WebSize(300, 300));
    webView->layout();
    FrameTestHelpers::runPendingTasks();

    WebElement pluginContainerOneElement = webView->mainFrame()->document().getElementById(WebString::fromUTF8("translated-plugin"));
    EXPECT_TRUE(webView->mainFrame()->executeCommand("Copy",  pluginContainerOneElement));
    EXPECT_EQ(WebString("x"), Platform::current()->clipboard()->readPlainText(WebClipboard::Buffer()));
}

}
