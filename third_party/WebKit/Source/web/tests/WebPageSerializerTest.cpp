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

#include "public/web/WebPageSerializer.h"

#include "platform/testing/URLTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebPageSerializerClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {
class SimpleWebPageSerializerClient final : public WebPageSerializerClient {
public:
    String toString() { return m_builder.toString(); }

private:
    void didSerializeDataForFrame(const WebCString& data, PageSerializationStatus) final
    {
        m_builder.append(data.data(), data.length());
    }

    StringBuilder m_builder;
};

} // namespace

class WebPageSerializerTest : public testing::Test {
protected:
    WebPageSerializerTest()
    {
        m_helper.initialize();
    }

    ~WebPageSerializerTest() override
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

    void registerMockedImageURL(const String& url)
    {
        // Image resources need to be mocked, but irrelevant here what image they map to.
        URLTestHelpers::registerMockedURLLoad(KURL(ParsedURLString, url), "pageserialization/awesome.png");
    }

    String serializeFile(const String& url, const String& fileName)
    {
        KURL parsedURL(ParsedURLString, url);
        URLTestHelpers::registerMockedURLLoad(parsedURL, fileName, "pageserialization/", "text/html");
        FrameTestHelpers::loadFrame(mainFrameImpl(), url.utf8().data());
        std::vector<std::pair<WebURL, WebString>> urlsToLocalPaths;
        urlsToLocalPaths.push_back(std::make_pair(parsedURL, WebString("local")));
        SimpleWebPageSerializerClient serializerClient;
        WebPageSerializer::serialize(mainFrameImpl(), &serializerClient, urlsToLocalPaths);
        return serializerClient.toString();
    }

    WebLocalFrameImpl* mainFrameImpl()
    {
        return m_helper.webViewImpl()->mainFrameImpl();
    }

private:
    FrameTestHelpers::WebViewHelper m_helper;
};

TEST_F(WebPageSerializerTest, URLAttributeValues)
{
    registerMockedImageURL("javascript:\"");

    const char* expectedHTML =
        "\n<!-- saved from url=(0020)http://www.test.com/ -->\n"
        "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n"
        "</head><body><img src=\"javascript:&quot;\">\n"
        "<a href=\"http://www.test.com/local#&quot;\">local</a>\n"
        "<a href=\"http://www.example.com/#&quot;&gt;&lt;script&gt;alert(0)&lt;/script&gt;\">external</a>\n"
        "</body></html>";
    String actualHTML = serializeFile("http://www.test.com", "url_attribute_values.html");
    EXPECT_EQ(expectedHTML, actualHTML);
}

TEST_F(WebPageSerializerTest, EncodingAndNormalization)
{
    const char* expectedHTML =
        "<!DOCTYPE html>\n"
        "<!-- saved from url=(0020)http://www.test.com/ -->\n"
        "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=EUC-KR\">\n"
        "<title>Ensure NFC normalization is not performed by page serializer</title>\n"
        "</head><body>\n"
        "\xe4\xc5\xd1\xe2\n"
        "\n</body></html>";
    String actualHTML = serializeFile("http://www.test.com", "encoding_normalization.html");
    EXPECT_EQ(expectedHTML, actualHTML);
}

TEST_F(WebPageSerializerTest, FromUrlWithMinusMinus)
{
    String actualHTML = serializeFile("http://www.test.com?--x--", "text_only_page.html");
    EXPECT_EQ("<!-- saved from url=(0030)http://www.test.com/?-%2Dx-%2D -->", actualHTML.substring(1, 60));
}

} // namespace blink
