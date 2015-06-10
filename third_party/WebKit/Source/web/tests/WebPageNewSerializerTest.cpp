/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebString.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrame.h"
#include "public/web/WebPageSerializer.h"
#include "public/web/WebPageSerializerClient.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "web/tests/FrameTestHelpers.h"
#include <gtest/gtest.h>

namespace {

using blink::URLTestHelpers::toKURL;
using blink::URLTestHelpers::registerMockedURLLoad;
using blink::testing::runPendingTasks;
using namespace blink;

class LineReader {
public:
    LineReader(const std::string& text) : m_text(text), m_index(0) { }
    bool getNextLine(std::string* line)
    {
        line->clear();
        if (m_index >= m_text.length())
            return false;

        size_t endOfLineIndex = m_text.find("\r\n", m_index);
        if (endOfLineIndex == std::string::npos) {
            *line = m_text.substr(m_index);
            m_index = m_text.length();
        } else {
            *line = m_text.substr(m_index, endOfLineIndex - m_index);
            m_index = endOfLineIndex + 2;
        }
        return true;
    }

private:
    std::string m_text;
    size_t m_index;
};

class LengthCountingWebPageSerializerClient : public WebPageSerializerClient {
public:
    LengthCountingWebPageSerializerClient(size_t* counter)
        : m_counter(counter)
    {
    }

    virtual void didSerializeDataForFrame(const WebURL& frameURL, const WebCString& data, PageSerializationStatus status) {
        *m_counter += data.length();
    }

private:
    size_t* m_counter;
};

class FrameDataWebPageSerializerClient : public WebPageSerializerClient {
public:
    FrameDataWebPageSerializerClient(const WebURL& frameURL, WebString* serializationData)
        : m_frameURL(frameURL)
        , m_serializationData(serializationData)
    {
    }

    virtual void didSerializeDataForFrame(const WebURL& frameURL, const WebCString& data, PageSerializationStatus status)
    {
        if (frameURL != m_frameURL)
            return;
        *m_serializationData = data.utf16();
    }

private:
    WebURL m_frameURL;
    WebString* m_serializationData;
};

class WebPageNewSerializeTest : public ::testing::Test {
public:
    WebPageNewSerializeTest()
        : m_baseURL("http://internal.test/")
        , m_htmlMimeType(WebString::fromUTF8("text/html"))
        , m_xhtmlMimeType(WebString::fromUTF8("application/xhtml+xml"))
        , m_cssMimeType(WebString::fromUTF8("text/css"))
        , m_pngMimeType(WebString::fromUTF8("image/png"))
        , m_svgMimeType(WebString::fromUTF8("image/svg+xml"))
    {
    }

protected:
    virtual void SetUp()
    {
        // We want the images to load and JavaScript to be on.
        m_helper.initialize(true, 0, 0, &configureSettings);
    }

    virtual void TearDown()
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

    KURL toTestURL(std::string relativeURL)
    {
        return toKURL(m_baseURL + relativeURL);
    }

    WebURL setUpCSSTestPage()
    {
        registerMockedURLLoad(toTestURL(""), WebString::fromUTF8("css_test_page.html"), WebString::fromUTF8("pageserializer/"), htmlMimeType());
        registerMockedURLLoad(toTestURL("link_styles.css"), WebString::fromUTF8("link_styles.css"), WebString::fromUTF8("pageserializer/"), cssMimeType());
        registerMockedURLLoad(toTestURL("import_style_from_link.css"), WebString::fromUTF8("import_style_from_link.css"), WebString::fromUTF8("pageserializer/"), cssMimeType());
        registerMockedURLLoad(toTestURL("import_styles.css"), WebString::fromUTF8("import_styles.css"), WebString::fromUTF8("pageserializer/"), cssMimeType());
        registerMockedURLLoad(toTestURL("red_background.png"), WebString::fromUTF8("red_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        registerMockedURLLoad(toTestURL("orange_background.png"), WebString::fromUTF8("orange_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        registerMockedURLLoad(toTestURL("yellow_background.png"), WebString::fromUTF8("yellow_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        registerMockedURLLoad(toTestURL("green_background.png"), WebString::fromUTF8("green_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        registerMockedURLLoad(toTestURL("blue_background.png"), WebString::fromUTF8("blue_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        registerMockedURLLoad(toTestURL("purple_background.png"), WebString::fromUTF8("purple_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        registerMockedURLLoad(toTestURL("ul-dot.png"), WebString::fromUTF8("ul-dot.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        registerMockedURLLoad(toTestURL("ol-dot.png"), WebString::fromUTF8("ol-dot.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());

        return toKURL(m_baseURL); // Top frame URL.
    }

    void loadURLInTopFrame(const WebURL& url)
    {
        FrameTestHelpers::loadFrame(m_helper.webView()->mainFrame(), url.string().utf8());
    }

    const WebString& htmlMimeType() const { return m_htmlMimeType; }
    const WebString& xhtmlMimeType() const { return m_xhtmlMimeType; }
    const WebString& cssMimeType() const { return m_cssMimeType; }
    const WebString& pngMimeType() const { return m_pngMimeType; }
    const WebString& svgMimeType() const { return m_svgMimeType; }

    static bool resourceVectorContains(const WebVector<WebPageSerializer::Resource>& resources, std::string url, const char* mimeType)
    {
        WebURL webURL = WebURL(toKURL(url));
        for (size_t i = 0; i < resources.size(); ++i) {
            const WebPageSerializer::Resource& resource = resources[i];
            if (resource.url == webURL && !resource.data.isEmpty() && !resource.mimeType.compare(WebCString(mimeType)))
                return true;
        }
        return false;
    }

    WebView* webView() const { return m_helper.webView(); }

    std::string m_baseURL;

private:
    static void configureSettings(WebSettings* settings)
    {
        settings->setImagesEnabled(true);
        settings->setLoadsImagesAutomatically(true);
        settings->setJavaScriptEnabled(true);
    }

    FrameTestHelpers::WebViewHelper m_helper;

    WebString m_htmlMimeType;
    WebString m_xhtmlMimeType;
    WebString m_cssMimeType;
    WebString m_pngMimeType;
    WebString m_svgMimeType;
};

// Test that when serializing a page, all CSS resources are reported, including url()'s
// and imports and links. Note that we don't test the resources contents, we only make sure
// they are all reported with the right mime type and that they contain some data.
TEST_F(WebPageNewSerializeTest, FAILS_CSSResources)
{
    // Register the mocked frame and load it.
    WebURL topFrameURL = setUpCSSTestPage();
    loadURLInTopFrame(topFrameURL);

    WebVector<WebPageSerializer::Resource> resources;
    WebPageSerializer::serialize(webView(), &resources);
    ASSERT_FALSE(resources.isEmpty());

    // The first resource should be the main-frame.
    const WebPageSerializer::Resource& resource = resources[0];
    EXPECT_TRUE(resource.url == WebURL(toKURL(m_baseURL)));
    EXPECT_EQ(0, resource.mimeType.compare(WebCString("text/html")));
    EXPECT_FALSE(resource.data.isEmpty());

    EXPECT_EQ(12U, resources.size()); // There should be no duplicates.
    EXPECT_TRUE(resourceVectorContains(resources, m_baseURL + "link_styles.css", "text/css"));
    EXPECT_TRUE(resourceVectorContains(resources, m_baseURL + "import_styles.css", "text/css"));
    EXPECT_TRUE(resourceVectorContains(resources, m_baseURL + "import_style_from_link.css", "text/css"));
    EXPECT_TRUE(resourceVectorContains(resources, m_baseURL + "red_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, m_baseURL + "orange_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, m_baseURL + "yellow_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, m_baseURL + "green_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, m_baseURL + "blue_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, m_baseURL + "purple_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, m_baseURL + "ul-dot.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, m_baseURL + "ol-dot.png", "image/png"));
}

TEST_F(WebPageNewSerializeTest, FAILS_TestMHTMLEncoding)
{
    // Load a page with some CSS and some images.
    WebURL topFrameURL = setUpCSSTestPage();
    loadURLInTopFrame(topFrameURL);

    WebCString mhtmlData = WebPageSerializer::serializeToMHTML(webView());
    ASSERT_FALSE(mhtmlData.isEmpty());

    // Read the MHTML data line per line and do some pseudo-parsing to make sure the right encoding is used for the different sections.
    LineReader lineReader(std::string(mhtmlData.data()));
    int sectionCheckedCount = 0;
    const char* expectedEncoding = 0;
    std::string line;
    while (lineReader.getNextLine(&line)) {
        if (!line.find("Content-Type:")) {
            ASSERT_FALSE(expectedEncoding);
            if (line.find("multipart/related;") != std::string::npos) {
                // Skip this one, it's part of the MHTML header.
                continue;
            }
            if (line.find("text/") != std::string::npos)
                expectedEncoding = "quoted-printable";
            else if (line.find("image/") != std::string::npos)
                expectedEncoding = "base64";
            else
                FAIL() << "Unexpected Content-Type: " << line;
            continue;
        }
        if (!line.find("Content-Transfer-Encoding:")) {
           ASSERT_TRUE(expectedEncoding);
           EXPECT_TRUE(line.find(expectedEncoding) != std::string::npos);
           expectedEncoding = 0;
           sectionCheckedCount++;
        }
    }
    EXPECT_EQ(12, sectionCheckedCount);
}

}
