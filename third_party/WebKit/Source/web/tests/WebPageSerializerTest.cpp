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
    void didSerializeDataForFrame(const WebCString& data, PageSerializationStatus) final
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

TEST_F(WebPageSerializerTest, EncodingAndNormalization)
{
    WebURL topFrameURL = toKURL("http://www.test.com");
    registerMockedURLLoad(topFrameURL.spec(), WebString::fromUTF8("encoding_normalization.html"));

    loadURLInTopFrame(topFrameURL);

    SimpleWebPageSerializerClient serializerClient;
    WebVector<WebURL> links(&topFrameURL, 1);
    WebVector<WebString> localPaths(&"local", 1);
    WebPageSerializer::serialize(webView()->mainFrame()->toWebLocalFrame(), &serializerClient, links, localPaths, "");

    const char* expectedHTML =
        "<!DOCTYPE html>\n"
        "<!-- saved from url=(0020)http://www.test.com/ -->\n"
        "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=EUC-KR\"><meta charset=\"euc-kr\">\n"
        "<title>Ensure NFC normalization is not performed by page serializer</title>\n"
        "</head><body>\n"
        "\xe4\xc5\xd1\xe2\n"
        "\n</body></html>";
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

} // namespace blink
