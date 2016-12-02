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

#include "public/web/WebFrameSerializer.h"

#include "platform/testing/URLTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCString.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebCache.h"
#include "public/web/WebFrameSerializerClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {
class SimpleWebFrameSerializerClient final : public WebFrameSerializerClient {
 public:
  String toString() { return m_builder.toString(); }

 private:
  void didSerializeDataForFrame(const WebCString& data,
                                FrameSerializationStatus) final {
    m_builder.append(data.data(), data.length());
  }

  StringBuilder m_builder;
};

class SimpleMHTMLPartsGenerationDelegate
    : public WebFrameSerializer::MHTMLPartsGenerationDelegate {
 private:
  bool shouldSkipResource(const WebURL&) final { return false; }

  WebString getContentID(WebFrame*) final { return WebString("<cid>"); }

  WebFrameSerializerCacheControlPolicy cacheControlPolicy() final {
    return WebFrameSerializerCacheControlPolicy::None;
  }

  bool useBinaryEncoding() final { return false; }
};

}  // namespace

class WebFrameSerializerTest : public testing::Test {
 protected:
  WebFrameSerializerTest() { m_helper.initialize(); }

  ~WebFrameSerializerTest() override {
    Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
    WebCache::clear();
  }

  void registerMockedImageURL(const String& url) {
    // Image resources need to be mocked, but irrelevant here what image they
    // map to.
    URLTestHelpers::registerMockedURLLoad(KURL(ParsedURLString, url),
                                          "frameserialization/awesome.png");
  }

  class SingleLinkRewritingDelegate
      : public WebFrameSerializer::LinkRewritingDelegate {
   public:
    SingleLinkRewritingDelegate(const WebURL& url, const WebString& localPath)
        : m_url(url), m_localPath(localPath) {}

    bool rewriteFrameSource(WebFrame* frame,
                            WebString* rewrittenLink) override {
      return false;
    }

    bool rewriteLink(const WebURL& url, WebString* rewrittenLink) override {
      if (url != m_url)
        return false;

      *rewrittenLink = m_localPath;
      return true;
    }

   private:
    const WebURL m_url;
    const WebString m_localPath;
  };

  String serializeFile(const String& url, const String& fileName) {
    KURL parsedURL(ParsedURLString, url);
    URLTestHelpers::registerMockedURLLoad(parsedURL, fileName,
                                          "frameserialization/", "text/html");
    FrameTestHelpers::loadFrame(mainFrameImpl(), url.utf8().data());
    SingleLinkRewritingDelegate delegate(parsedURL, WebString("local"));
    SimpleWebFrameSerializerClient serializerClient;
    WebFrameSerializer::serialize(mainFrameImpl(), &serializerClient,
                                  &delegate);
    return serializerClient.toString();
  }

  WebLocalFrameImpl* mainFrameImpl() {
    return m_helper.webView()->mainFrameImpl();
  }

 private:
  FrameTestHelpers::WebViewHelper m_helper;
};

TEST_F(WebFrameSerializerTest, URLAttributeValues) {
  registerMockedImageURL("javascript:\"");

  const char* expectedHTML =
      "\n<!-- saved from url=(0020)http://www.test.com/ -->\n"
      "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; "
      "charset=UTF-8\">\n"
      "</head><body><img src=\"javascript:&quot;\">\n"
      "<a href=\"http://www.test.com/local#&quot;\">local</a>\n"
      "<a "
      "href=\"http://www.example.com/#&quot;&gt;&lt;script&gt;alert(0)&lt;/"
      "script&gt;\">external</a>\n"
      "</body></html>";
  String actualHTML =
      serializeFile("http://www.test.com", "url_attribute_values.html");
  EXPECT_EQ(expectedHTML, actualHTML);
}

TEST_F(WebFrameSerializerTest, EncodingAndNormalization) {
  const char* expectedHTML =
      "<!DOCTYPE html>\n"
      "<!-- saved from url=(0020)http://www.test.com/ -->\n"
      "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; "
      "charset=EUC-KR\">\n"
      "<title>Ensure NFC normalization is not performed by frame "
      "serializer</title>\n"
      "</head><body>\n"
      "\xe4\xc5\xd1\xe2\n"
      "\n</body></html>";
  String actualHTML =
      serializeFile("http://www.test.com", "encoding_normalization.html");
  EXPECT_EQ(expectedHTML, actualHTML);
}

TEST_F(WebFrameSerializerTest, FromUrlWithMinusMinus) {
  String actualHTML =
      serializeFile("http://www.test.com?--x--", "text_only_page.html");
  EXPECT_EQ("<!-- saved from url=(0030)http://www.test.com/?-%2Dx-%2D -->",
            actualHTML.substring(1, 60));
}

class WebFrameSerializerSanitizationTest : public WebFrameSerializerTest {
 protected:
  WebFrameSerializerSanitizationTest() {}

  ~WebFrameSerializerSanitizationTest() override {}

  String generateMHTMLParts(const String& url, const String& fileName) {
    KURL parsedURL(ParsedURLString, url);
    URLTestHelpers::registerMockedURLLoad(parsedURL, fileName,
                                          "frameserialization/", "text/html");
    FrameTestHelpers::loadFrame(mainFrameImpl(), url.utf8().data());
    WebThreadSafeData result = WebFrameSerializer::generateMHTMLParts(
        WebString("boundary"), mainFrameImpl(), &m_mhtmlDelegate);
    return String(result.data(), result.size());
  }

 private:
  SimpleMHTMLPartsGenerationDelegate m_mhtmlDelegate;
};

TEST_F(WebFrameSerializerSanitizationTest, RemoveInlineScriptInAttributes) {
  String mhtml =
      generateMHTMLParts("http://www.test.com", "script_in_attributes.html");

  // These scripting attributes should be removed.
  EXPECT_EQ(WTF::kNotFound, mhtml.find("onload="));
  EXPECT_EQ(WTF::kNotFound, mhtml.find("ONLOAD="));
  EXPECT_EQ(WTF::kNotFound, mhtml.find("onclick="));
  EXPECT_EQ(WTF::kNotFound, mhtml.find("href="));
  EXPECT_EQ(WTF::kNotFound, mhtml.find("from="));
  EXPECT_EQ(WTF::kNotFound, mhtml.find("to="));
  EXPECT_EQ(WTF::kNotFound, mhtml.find("javascript:"));

  // These non-scripting attributes should remain intact.
  EXPECT_NE(WTF::kNotFound, mhtml.find("class="));
  EXPECT_NE(WTF::kNotFound, mhtml.find("id="));

  // srcdoc attribute of frame element should be replaced with src attribute.
  EXPECT_EQ(WTF::kNotFound, mhtml.find("srcdoc="));
  EXPECT_NE(WTF::kNotFound, mhtml.find("src="));
}

}  // namespace blink
