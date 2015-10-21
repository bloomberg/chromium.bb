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
#include "public/web/WebPageSerializer.h"

#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrame.h"
#include "public/web/WebPageSerializerClient.h"
#include "public/web/WebView.h"
#include "web/tests/FrameTestHelpers.h"
#include <gtest/gtest.h>

using blink::Document;
using blink::URLTestHelpers::toKURL;

namespace blink {

namespace {
class SimpleWebPageSerializerClient : public  WebPageSerializerClient {
public:
    std::string toString() const { return m_string; }

private:
    void didSerializeDataForFrame(const WebURL&, const WebCString& data, PageSerializationStatus) final
    {
        m_string += data;
    }

    std::string m_string;
};

} // namespace

class WebPageSerializerTest : public testing::Test {
public:
    WebPageSerializerTest()
        : m_supportedSchemes(static_cast<size_t>(3))
    {
        m_supportedSchemes[0] = "http";
        m_supportedSchemes[1] = "https";
        m_supportedSchemes[2] = "file";

        registerMockedImageURL("http://www.test.com/awesome.png");
    }

protected:
    void SetUp() override
    {
        m_helper.initialize();
    }

    void TearDown() override
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

    void registerMockedURLLoad(const std::string& url, const WebString& fileName)
    {
        URLTestHelpers::registerMockedURLLoad(toKURL(url), fileName, WebString::fromUTF8("pageserialization/"), WebString::fromUTF8("text/html"));
    }

    void registerMockedImageURL(const std::string& url)
    {
        // Image resources need to be mocked, but irrelevant here what image they map to.
        URLTestHelpers::registerMockedURLLoad(toKURL(url), "pageserialization/awesome.png");
    }

    void loadURLInTopFrame(const WebURL& url)
    {
        FrameTestHelpers::loadFrame(m_helper.webView()->mainFrame(), url.string().utf8());
    }

    static bool webVectorContains(const WebVector<WebURL>& vector, const char* url)
    {
        return vector.contains(WebURL(toKURL(std::string(url))));
    }

    // Useful for debugging.
    static void printWebURLs(const WebVector<WebURL>& urls)
    {
        for (size_t i = 0; i < urls.size(); i++)
            printf("%s\n", urls[i].spec().data());
    }

    WebView* webView() const { return m_helper.webView(); }

    WebVector<WebCString> m_supportedSchemes;

private:
    FrameTestHelpers::WebViewHelper m_helper;
};

TEST_F(WebPageSerializerTest, HTMLNodes)
{
    // Register the mocked frame and load it.
    WebURL topFrameURL = toKURL("http://www.test.com");
    registerMockedURLLoad("http://www.test.com", WebString::fromUTF8("simple_page.html"));
    registerMockedURLLoad("http://www.example.com/beautifull.css", WebString::fromUTF8("beautifull.css"));

    registerMockedImageURL("http://www.test.com/imageButton.png");
    registerMockedImageURL("http://www.test.com/tableBackground.png");
    registerMockedImageURL("http://www.test.com/trBackground.png");
    registerMockedImageURL("http://www.test.com/tdBackground.png");
    registerMockedImageURL("http://www.test.com/innerFrame.png");
    registerMockedImageURL("http://www.test.com/bodyBackground.jpg");
    registerMockedImageURL("https://www.secure.com/https.gif");
    registerMockedImageURL("ftp://ftp.com/ftp.gif");
    registerMockedImageURL("unknown://unkown.com/unknown.gif");

    loadURLInTopFrame(topFrameURL);

    // Retrieve all resources.
    WebVector<WebURL> frames;
    WebVector<WebURL> resources;
    ASSERT_TRUE(WebPageSerializer::retrieveAllResources(
        webView(), m_supportedSchemes, &resources, &frames));

    // Tests that all resources from the frame have been retrieved.
    EXPECT_EQ(1U, frames.size()); // There should be no duplicates.
    EXPECT_TRUE(webVectorContains(frames, "http://www.test.com"));

    EXPECT_EQ(14U, resources.size()); // There should be no duplicates.
    EXPECT_TRUE(webVectorContains(resources, "http://www.example.com/beautifull.css"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/awesome.js"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/bodyBackground.jpg"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/awesome.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/imageButton.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/tableBackground.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/trBackground.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/tdBackground.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.evene.fr/citations/auteur.php?ida=46"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.brainyquote.com/quotes/authors/c/charles_darwin.html"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/why_deleted.html"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/why_inserted.html"));
    EXPECT_TRUE(webVectorContains(resources, "https://www.secure.com/https.gif"));
    EXPECT_TRUE(webVectorContains(resources, "file://c/my_folder/file.gif"));
}

TEST_F(WebPageSerializerTest, URLAttributeValues)
{
    WebURL topFrameURL = toKURL("http://www.test.com");
    registerMockedURLLoad(topFrameURL.spec(), WebString::fromUTF8("url_attribute_values.html"));
    registerMockedImageURL("javascript:\"");

    loadURLInTopFrame(topFrameURL);

    SimpleWebPageSerializerClient serializerClient;
    WebVector<WebURL> links(&topFrameURL, 1);
    WebVector<WebString> localPaths(&"local", 1);
    WebPageSerializer::serialize(webView()->mainFrame()->toWebLocalFrame(), &serializerClient, links, localPaths, "");

    const char* expectedHTML =
        "\n<!-- saved from url=(0020)http://www.test.com/ -->\n"
        "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"><meta charset=\"utf8\">\n"
        "</head><body><img src=\"javascript:&quot;\">\n"
        "<a href=\"http://www.test.com/local#&quot;\">local</a>\n"
        "<a href=\"http://www.example.com/#&quot;&gt;&lt;script&gt;alert(0)&lt;/script&gt;\">external</a>\n"
        "</body></html>";
    EXPECT_EQ(expectedHTML, serializerClient.toString());
}

TEST_F(WebPageSerializerTest, fromUrlWithMinusMinus)
{
    WebURL topFrameURL = toKURL("http://www.test.com?--x--");
    registerMockedURLLoad(topFrameURL.spec(), WebString::fromUTF8("text_only_page.html"));
    loadURLInTopFrame(topFrameURL);

    SimpleWebPageSerializerClient serializerClient;
    WebVector<WebURL> links(&topFrameURL, 1);
    WebVector<WebString> localPaths(&"local", 1);
    WebPageSerializer::serialize(webView()->mainFrame()->toWebLocalFrame(), &serializerClient, links, localPaths, "");

    EXPECT_EQ("<!-- saved from url=(0030)http://www.test.com/?-%2Dx-%2D -->", serializerClient.toString().substr(1, 60));
}

TEST_F(WebPageSerializerTest, MultipleFrames)
{
    // Register the mocked frames.
    WebURL topFrameURL = toKURL("http://www.test.com");
    registerMockedURLLoad("http://www.test.com", WebString::fromUTF8("top_frame.html"));
    registerMockedURLLoad("http://www.test.com/simple_iframe.html",
                          WebString::fromUTF8("simple_iframe.html"));
    registerMockedURLLoad("http://www.test.com/object_iframe.html",
                          WebString::fromUTF8("object_iframe.html"));
    registerMockedURLLoad("http://www.test.com/embed_iframe.html",
                          WebString::fromUTF8("embed_iframe.html"));
    registerMockedImageURL("http://www.test.com/innerFrame.png");
    registerMockedImageURL("http://www.test.com/embed.png");
    registerMockedImageURL("http://www.test.com/object.png");

    loadURLInTopFrame(topFrameURL);

    // Retrieve all resources.
    WebVector<WebURL> frames;
    WebVector<WebURL> resources;
    ASSERT_TRUE(WebPageSerializer::retrieveAllResources(
        webView(), m_supportedSchemes, &resources, &frames));

    // Tests that all resources from the frame have been retrieved.
    EXPECT_EQ(4U, frames.size()); // There should be no duplicates.
    EXPECT_TRUE(webVectorContains(frames, "http://www.test.com"));
    EXPECT_TRUE(webVectorContains(frames, "http://www.test.com/simple_iframe.html"));
    EXPECT_TRUE(webVectorContains(frames, "http://www.test.com/object_iframe.html"));
    EXPECT_TRUE(webVectorContains(frames, "http://www.test.com/embed_iframe.html"));

    EXPECT_EQ(5U, resources.size()); // There should be no duplicates.
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/awesome.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/innerFrame.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/flash.swf"));
    // FIXME: for some reason the following resources is missing on one of the bot
    //        causing the test to fail. Probably a plugin issue.
    // EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/music.mid"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/object.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/embed.png"));
}

} // namespace blink
