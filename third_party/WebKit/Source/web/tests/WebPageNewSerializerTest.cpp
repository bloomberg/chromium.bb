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
#include "web/tests/URLTestHelpers.h"
#include <gtest/gtest.h>

using namespace blink;
using blink::Document;
using blink::FrameTestHelpers::runPendingTasks;
using blink::URLTestHelpers::toKURL;
using blink::URLTestHelpers::registerMockedURLLoad;

namespace {

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

class WebPageNewSerializeTest : public testing::Test {
public:
    WebPageNewSerializeTest()
        : m_htmlMimeType(WebString::fromUTF8("text/html"))
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

    WebURL setUpCSSTestPage()
    {
        WebURL topFrameURL = toKURL("http://www.test.com");
        registerMockedURLLoad(topFrameURL, WebString::fromUTF8("css_test_page.html"), WebString::fromUTF8("pageserializer/"), htmlMimeType());
        registerMockedURLLoad(toKURL("http://www.test.com/link_styles.css"), WebString::fromUTF8("link_styles.css"), WebString::fromUTF8("pageserializer/"), cssMimeType());
        registerMockedURLLoad(toKURL("http://www.test.com/import_style_from_link.css"), WebString::fromUTF8("import_style_from_link.css"), WebString::fromUTF8("pageserializer/"), cssMimeType());
        registerMockedURLLoad(toKURL("http://www.test.com/import_styles.css"), WebString::fromUTF8("import_styles.css"), WebString::fromUTF8("pageserializer/"), cssMimeType());
        registerMockedURLLoad(toKURL("http://www.test.com/red_background.png"), WebString::fromUTF8("red_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        registerMockedURLLoad(toKURL("http://www.test.com/orange_background.png"), WebString::fromUTF8("orange_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        registerMockedURLLoad(toKURL("http://www.test.com/yellow_background.png"), WebString::fromUTF8("yellow_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        registerMockedURLLoad(toKURL("http://www.test.com/green_background.png"), WebString::fromUTF8("green_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        registerMockedURLLoad(toKURL("http://www.test.com/blue_background.png"), WebString::fromUTF8("blue_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        registerMockedURLLoad(toKURL("http://www.test.com/purple_background.png"), WebString::fromUTF8("purple_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        registerMockedURLLoad(toKURL("http://www.test.com/ul-dot.png"), WebString::fromUTF8("ul-dot.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        registerMockedURLLoad(toKURL("http://www.test.com/ol-dot.png"), WebString::fromUTF8("ol-dot.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
        return topFrameURL;
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

    static bool resourceVectorContains(const WebVector<WebPageSerializer::Resource>& resources, const char* url, const char* mimeType)
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

// Tests that a page with resources and sub-frame is reported with all its resources.
TEST_F(WebPageNewSerializeTest, PageWithFrames)
{
    // Register the mocked frames.
    WebURL topFrameURL = toKURL("http://www.test.com");
    registerMockedURLLoad(topFrameURL, WebString::fromUTF8("top_frame.html"), WebString::fromUTF8("pageserializer/"), htmlMimeType());
    registerMockedURLLoad(toKURL("http://www.test.com/iframe.html"), WebString::fromUTF8("iframe.html"), WebString::fromUTF8("pageserializer/"), htmlMimeType());
    registerMockedURLLoad(toKURL("http://www.test.com/iframe2.html"), WebString::fromUTF8("iframe2.html"), WebString::fromUTF8("pageserializer/"), htmlMimeType());
    registerMockedURLLoad(toKURL("http://www.test.com/red_background.png"), WebString::fromUTF8("red_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
    registerMockedURLLoad(toKURL("http://www.test.com/green_background.png"), WebString::fromUTF8("green_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
    registerMockedURLLoad(toKURL("http://www.test.com/blue_background.png"), WebString::fromUTF8("blue_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());

    loadURLInTopFrame(topFrameURL);

    WebVector<WebPageSerializer::Resource> resources;
    WebPageSerializer::serialize(webView(), &resources);
    ASSERT_FALSE(resources.isEmpty());

    // The first resource should be the main-frame.
    const WebPageSerializer::Resource& resource = resources[0];
    EXPECT_TRUE(resource.url == WebURL(toKURL("http://www.test.com")));
    EXPECT_EQ(0, resource.mimeType.compare(WebCString("text/html")));
    EXPECT_FALSE(resource.data.isEmpty());

    EXPECT_EQ(6U, resources.size()); // There should be no duplicates.
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/red_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/green_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/blue_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/iframe.html", "text/html"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/iframe2.html", "text/html"));
}

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
    EXPECT_TRUE(resource.url == WebURL(toKURL("http://www.test.com")));
    EXPECT_EQ(0, resource.mimeType.compare(WebCString("text/html")));
    EXPECT_FALSE(resource.data.isEmpty());

    EXPECT_EQ(12U, resources.size()); // There should be no duplicates.
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/link_styles.css", "text/css"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/import_styles.css", "text/css"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/import_style_from_link.css", "text/css"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/red_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/orange_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/yellow_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/green_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/blue_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/purple_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/ul-dot.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/ol-dot.png", "image/png"));
}

// Tests that when serializing a page with blank frames these are reported with their resources.
TEST_F(WebPageNewSerializeTest, BlankFrames)
{
    // Register the mocked frame and load it.
    WebURL topFrameURL = toKURL("http://www.test.com");
    registerMockedURLLoad(topFrameURL, WebString::fromUTF8("blank_frames.html"), WebString::fromUTF8("pageserializer/"), htmlMimeType());
    registerMockedURLLoad(toKURL("http://www.test.com/red_background.png"), WebString::fromUTF8("red_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
    registerMockedURLLoad(toKURL("http://www.test.com/orange_background.png"), WebString::fromUTF8("orange_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
    registerMockedURLLoad(toKURL("http://www.test.com/blue_background.png"), WebString::fromUTF8("blue_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());

    loadURLInTopFrame(topFrameURL);

    WebVector<WebPageSerializer::Resource> resources;
    WebPageSerializer::serialize(webView(), &resources);
    ASSERT_FALSE(resources.isEmpty());

    // The first resource should be the main-frame.
    const WebPageSerializer::Resource& resource = resources[0];
    EXPECT_TRUE(resource.url == WebURL(toKURL("http://www.test.com")));
    EXPECT_EQ(0, resource.mimeType.compare(WebCString("text/html")));
    EXPECT_FALSE(resource.data.isEmpty());

    EXPECT_EQ(7U, resources.size()); // There should be no duplicates.
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/red_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/orange_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/blue_background.png", "image/png"));
    // The blank frames should have got a magic URL.
    EXPECT_TRUE(resourceVectorContains(resources, "wyciwyg://frame/0", "text/html"));
    EXPECT_TRUE(resourceVectorContains(resources, "wyciwyg://frame/1", "text/html"));
    EXPECT_TRUE(resourceVectorContains(resources, "wyciwyg://frame/2", "text/html"));
}

TEST_F(WebPageNewSerializeTest, SerializeXMLHasRightDeclaration)
{
    WebURL topFrameURL = toKURL("http://www.test.com/simple.xhtml");
    registerMockedURLLoad(topFrameURL, WebString::fromUTF8("simple.xhtml"), WebString::fromUTF8("pageserializer/"), xhtmlMimeType());

    loadURLInTopFrame(topFrameURL);

    WebVector<WebPageSerializer::Resource> resources;
    WebPageSerializer::serialize(webView(), &resources);
    ASSERT_FALSE(resources.isEmpty());

    // We expect only one resource, the XML.
    ASSERT_EQ(1U, resources.size());
    std::string xml = std::string(resources[0].data.data());

    // We should have one and only one instance of the XML declaration.
    size_t pos = xml.find("<?xml version=");
    ASSERT_TRUE(pos != std::string::npos);

    pos = xml.find("<?xml version=", pos + 1);
    ASSERT_TRUE(pos == std::string::npos);
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

// Test that we don't regress https://bugs.webkit.org/show_bug.cgi?id=99105
TEST_F(WebPageNewSerializeTest, SVGImageDontCrash)
{
    WebURL pageUrl = toKURL("http://www.test.com");
    WebURL imageUrl = toKURL("http://www.test.com/green_rectangle.svg");

    registerMockedURLLoad(pageUrl, WebString::fromUTF8("page_with_svg_image.html"), WebString::fromUTF8("pageserializer/"), htmlMimeType());
    registerMockedURLLoad(imageUrl, WebString::fromUTF8("green_rectangle.svg"), WebString::fromUTF8("pageserializer/"), svgMimeType());

    loadURLInTopFrame(pageUrl);

    WebCString mhtml = WebPageSerializer::serializeToMHTML(webView());
    // We expect some data to be generated.
    EXPECT_GT(mhtml.length(), 50U);
}


TEST_F(WebPageNewSerializeTest, DontIncludeErrorImage)
{
    WebURL pageUrl = toKURL("http://www.test.com");
    WebURL imageUrl = toKURL("http://www.test.com/error_image.png");

    registerMockedURLLoad(pageUrl, WebString::fromUTF8("page_with_img_error.html"), WebString::fromUTF8("pageserializer/"), htmlMimeType());
    registerMockedURLLoad(imageUrl, WebString::fromUTF8("error_image.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());

    loadURLInTopFrame(pageUrl);

    WebCString mhtmlData = WebPageSerializer::serializeToMHTML(webView());
    ASSERT_FALSE(mhtmlData.isEmpty());

    // Sniff the MHTML data to make sure image content is excluded.
    LineReader lineReader(std::string(mhtmlData.data()));
    std::string line;
    while (lineReader.getNextLine(&line)) {
        if (line.find("image/") != std::string::npos)
            FAIL() << "Error Image was not excluded " << line;
    }
}


TEST_F(WebPageNewSerializeTest, NamespaceElementsDontCrash)
{
    WebURL pageUrl = toKURL("http://www.test.com");
    registerMockedURLLoad(pageUrl, WebString::fromUTF8("namespace_element.html"), WebString::fromUTF8("pageserializer/"), htmlMimeType());

    loadURLInTopFrame(pageUrl);

    WebVector<WebURL> localLinks(static_cast<size_t>(1));
    WebVector<WebString> localPaths(static_cast<size_t>(1));
    localLinks[0] = pageUrl;
    localPaths[0] = WebString("/");

    size_t counter = 0;
    LengthCountingWebPageSerializerClient client(&counter);

    // We just want to make sure nothing crazy happens, namely that no
    // assertions are hit. As a sanity check, we also make sure that some data
    // was returned.
    WebPageSerializer::serialize(webView()->mainFrame()->toWebLocalFrame(), true, &client, localLinks, localPaths, WebString(""));

    EXPECT_GT(counter, 0U);
}

TEST_F(WebPageNewSerializeTest, SubFrameSerialization)
{
    WebURL pageUrl = toKURL("http://www.test.com");
    registerMockedURLLoad(pageUrl, WebString::fromUTF8("top_frame.html"), WebString::fromUTF8("pageserializer/"), htmlMimeType());
    registerMockedURLLoad(toKURL("http://www.test.com/iframe.html"), WebString::fromUTF8("iframe.html"), WebString::fromUTF8("pageserializer/"), htmlMimeType());
    registerMockedURLLoad(toKURL("http://www.test.com/iframe2.html"), WebString::fromUTF8("iframe2.html"), WebString::fromUTF8("pageserializer/"), htmlMimeType());
    registerMockedURLLoad(toKURL("http://www.test.com/red_background.png"), WebString::fromUTF8("red_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
    registerMockedURLLoad(toKURL("http://www.test.com/green_background.png"), WebString::fromUTF8("green_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());
    registerMockedURLLoad(toKURL("http://www.test.com/blue_background.png"), WebString::fromUTF8("blue_background.png"), WebString::fromUTF8("pageserializer/"), pngMimeType());

    loadURLInTopFrame(pageUrl);

    WebVector<WebURL> localLinks(static_cast<size_t>(2));
    WebVector<WebString> localPaths(static_cast<size_t>(2));
    localLinks[0] = pageUrl;
    localPaths[0] = WebString("/");
    localLinks[1] = toKURL("http://www.test.com/iframe.html");
    localPaths[1] = WebString("SavedFiles/iframe.html");

    WebString serializedData;
    FrameDataWebPageSerializerClient client(pageUrl, &serializedData);

    // We just want to make sure nothing crazy happens, namely that no
    // assertions are hit. As a sanity check, we also make sure that some data
    // was returned.
    WebPageSerializer::serialize(webView()->mainFrame()->toWebLocalFrame(), true, &client, localLinks, localPaths, WebString(""));

    // Subframe src
    EXPECT_TRUE(static_cast<String>(serializedData).contains("src=\"SavedFiles/iframe.html\""));
}

}

TEST_F(WebPageNewSerializeTest, TestMHTMLEncodingWithDataURL)
{
    // Load a page with some data urls.
    WebURL topFrameURL = toKURL("http://www.test.com");
    registerMockedURLLoad(topFrameURL, WebString::fromUTF8("page_with_data.html"), WebString::fromUTF8("pageserializer/"), htmlMimeType());
    loadURLInTopFrame(topFrameURL);

    WebCString mhtmlData = WebPageSerializer::serializeToMHTML(webView());
    ASSERT_FALSE(mhtmlData.isEmpty());

    // Read the MHTML data line and check that the string data:image is found
    // exactly one time.
    size_t nbDataURLs = 0;
    LineReader lineReader(std::string(mhtmlData.data()));
    std::string line;
    while (lineReader.getNextLine(&line)) {
        if (line.find("data:image") != std::string::npos)
            nbDataURLs++;
    }
    EXPECT_EQ(1u, nbDataURLs);
}


TEST_F(WebPageNewSerializeTest, TestMHTMLEncodingWithMorphingDataURL)
{
    // Load a page with some data urls.
    WebURL topFrameURL = toKURL("http://www.test.com");
    registerMockedURLLoad(topFrameURL, WebString::fromUTF8("page_with_morphing_data.html"), WebString::fromUTF8("pageserializer/"), htmlMimeType());
    loadURLInTopFrame(topFrameURL);

    WebCString mhtmlData = WebPageSerializer::serializeToMHTML(webView());
    ASSERT_FALSE(mhtmlData.isEmpty());

    // Read the MHTML data line and check that the string data:image is found
    // exactly two times.
    size_t nbDataURLs = 0;
    LineReader lineReader(std::string(mhtmlData.data()));
    std::string line;
    while (lineReader.getNextLine(&line)) {
        if (line.find("data:text") != std::string::npos)
            nbDataURLs++;
    }
    EXPECT_EQ(2u, nbDataURLs);
}
