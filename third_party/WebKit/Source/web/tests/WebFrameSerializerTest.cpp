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
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCString.h"
#include "public/platform/WebCache.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoaderMockFactory.h"
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
 public:
  SimpleMHTMLPartsGenerationDelegate() : m_removePopupOverlay(false) {}

  void setRemovePopupOverlay(bool removePopupOverlay) {
    m_removePopupOverlay = removePopupOverlay;
  }

 private:
  bool shouldSkipResource(const WebURL&) final { return false; }

  WebString getContentID(WebFrame*) final { return WebString("<cid>"); }

  WebFrameSerializerCacheControlPolicy cacheControlPolicy() final {
    return WebFrameSerializerCacheControlPolicy::None;
  }

  bool useBinaryEncoding() final { return false; }
  bool removePopupOverlay() final { return m_removePopupOverlay; }

  bool m_removePopupOverlay;
};

// Returns the count of match for substring |pattern| in string |str|.
int matchSubstring(const String& str, const char* pattern, size_t size) {
  int matches = 0;
  size_t start = 0;
  while (true) {
    size_t pos = str.find(pattern, start);
    if (pos == WTF::kNotFound)
      break;
    matches++;
    start = pos + size;
  }
  return matches;
}

}  // namespace

class WebFrameSerializerTest : public ::testing::Test {
 protected:
  WebFrameSerializerTest() { m_helper.initialize(); }

  ~WebFrameSerializerTest() override {
    Platform::current()
        ->getURLLoaderMockFactory()
        ->unregisterAllURLsAndClearMemoryCache();
  }

  void registerMockedImageURLLoad(const String& url) {
    // Image resources need to be mocked, but irrelevant here what image they
    // map to.
    registerMockedFileURLLoad(URLTestHelpers::toKURL(url.utf8().data()),
                              "frameserialization/awesome.png");
  }
  void registerMockedFileURLLoad(const KURL& url,
                                 const String& filePath,
                                 const String& mimeType = "image/png") {
    URLTestHelpers::registerMockedURLLoad(
        url, testing::webTestDataPath(filePath.utf8().data()), mimeType);
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
    String filePath("frameserialization/" + fileName);
    registerMockedFileURLLoad(parsedURL, filePath, "text/html");
    FrameTestHelpers::loadFrame(mainFrameImpl(), url.utf8().data());
    SingleLinkRewritingDelegate delegate(parsedURL, WebString("local"));
    SimpleWebFrameSerializerClient serializerClient;
    WebFrameSerializer::serialize(mainFrameImpl(), &serializerClient,
                                  &delegate);
    return serializerClient.toString();
  }

  WebViewImpl* webView() { return m_helper.webView(); }

  WebLocalFrameImpl* mainFrameImpl() {
    return m_helper.webView()->mainFrameImpl();
  }

 private:
  FrameTestHelpers::WebViewHelper m_helper;
};

TEST_F(WebFrameSerializerTest, URLAttributeValues) {
  registerMockedImageURLLoad("javascript:\"");

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

  String generateMHTMLParts(const String& url,
                            const String& fileName,
                            const String& mimeType = "text/html") {
    KURL parsedURL(ParsedURLString, url);
    String filePath("frameserialization/" + fileName);
    registerMockedFileURLLoad(parsedURL, filePath, mimeType);
    FrameTestHelpers::loadFrame(mainFrameImpl(), url.utf8().data());
    WebThreadSafeData result = WebFrameSerializer::generateMHTMLParts(
        WebString("boundary"), mainFrameImpl(), &m_mhtmlDelegate);
    return String(result.data(), result.size());
  }

  void setRemovePopupOverlay(bool removePopupOverlay) {
    m_mhtmlDelegate.setRemovePopupOverlay(removePopupOverlay);
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

TEST_F(WebFrameSerializerSanitizationTest, DisableFormElements) {
  String mhtml = generateMHTMLParts("http://www.test.com", "form.html");

  const char kDisabledAttr[] = "disabled=3D\"\"";
  int matches =
      matchSubstring(mhtml, kDisabledAttr, arraysize(kDisabledAttr) - 1);
  EXPECT_EQ(21, matches);
}

TEST_F(WebFrameSerializerSanitizationTest, RemoveHiddenElements) {
  String mhtml =
      generateMHTMLParts("http://www.test.com", "hidden_elements.html");

  // These hidden elements that do not affect layout should be removed.
  EXPECT_EQ(WTF::kNotFound, mhtml.find("<h1"));
  EXPECT_EQ(WTF::kNotFound, mhtml.find("<p id=3D\"hidden_id\""));
  EXPECT_EQ(WTF::kNotFound, mhtml.find("<input type=3D\"hidden\""));

  // These default-hidden elements should not be removed.
  EXPECT_NE(WTF::kNotFound, mhtml.find("<html"));
  EXPECT_NE(WTF::kNotFound, mhtml.find("<head"));
  EXPECT_NE(WTF::kNotFound, mhtml.find("<style"));
  EXPECT_NE(WTF::kNotFound, mhtml.find("<title"));
  EXPECT_NE(WTF::kNotFound, mhtml.find("<datalist"));
  EXPECT_NE(WTF::kNotFound, mhtml.find("<option"));
  // One for meta in head and another for meta in body.
  EXPECT_EQ(2, matchSubstring(mhtml, "<meta", 5));
  // One for style in head and another for style in body.
  EXPECT_EQ(2, matchSubstring(mhtml, "<style", 6));
  // One for link in head and another for link in body.
  EXPECT_EQ(2, matchSubstring(mhtml, "<link", 5));

  // These hidden elements that affect layout should remain intact.
  EXPECT_NE(WTF::kNotFound, mhtml.find("<h2"));

  // These visible elements should remain intact.
  EXPECT_NE(WTF::kNotFound, mhtml.find("<h2"));
  EXPECT_NE(WTF::kNotFound, mhtml.find("<p id=3D\"visible_id\""));
  EXPECT_NE(WTF::kNotFound, mhtml.find("<form"));
  EXPECT_NE(WTF::kNotFound, mhtml.find("<input type=3D\"text\""));
  EXPECT_NE(WTF::kNotFound, mhtml.find("<div"));
}

// Regression test for crbug.com/678893, where in some cases serializing an
// image document could cause code to pick an element from an empty container.
TEST_F(WebFrameSerializerSanitizationTest, FromBrokenImageDocument) {
  String mhtml = generateMHTMLParts("http://www.test.com", "broken-image.png",
                                    "image/png");
  EXPECT_TRUE(mhtml.isEmpty());
}

TEST_F(WebFrameSerializerSanitizationTest, ImageLoadedFromSrcsetForHiDPI) {
  registerMockedFileURLLoad(KURL(ParsedURLString, "http://www.test.com/1x.png"),
                            "frameserialization/1x.png");
  registerMockedFileURLLoad(KURL(ParsedURLString, "http://www.test.com/2x.png"),
                            "frameserialization/2x.png");

  // Set high DPR in order to load image from srcset, instead of src.
  webView()->setDeviceScaleFactor(2.0f);

  String mhtml = generateMHTMLParts("http://www.test.com", "img_srcset.html");

  // srcset attribute should be skipped.
  EXPECT_EQ(WTF::kNotFound, mhtml.find("srcset="));

  // Width and height attributes should be set when none is present in <img>.
  EXPECT_NE(WTF::kNotFound,
            mhtml.find("id=3D\"i1\" width=3D\"6\" height=3D\"6\">"));

  // Height attribute should not be set if width attribute is already present in
  // <img>
  EXPECT_NE(WTF::kNotFound, mhtml.find("id=3D\"i2\" width=3D\"8\">"));
}

TEST_F(WebFrameSerializerSanitizationTest, ImageLoadedFromSrcForNormalDPI) {
  registerMockedFileURLLoad(KURL(ParsedURLString, "http://www.test.com/1x.png"),
                            "frameserialization/1x.png");
  registerMockedFileURLLoad(KURL(ParsedURLString, "http://www.test.com/2x.png"),
                            "frameserialization/2x.png");

  String mhtml = generateMHTMLParts("http://www.test.com", "img_srcset.html");

  // srcset attribute should be skipped.
  EXPECT_EQ(WTF::kNotFound, mhtml.find("srcset="));

  // New width and height attributes should not be set.
  EXPECT_NE(WTF::kNotFound, mhtml.find("id=3D\"i1\">"));
  EXPECT_NE(WTF::kNotFound, mhtml.find("id=3D\"i2\" width=3D\"8\">"));
}

TEST_F(WebFrameSerializerSanitizationTest, RemovePopupOverlayIfRequested) {
  webView()->resize(WebSize(500, 500));
  setRemovePopupOverlay(true);
  String mhtml = generateMHTMLParts("http://www.test.com", "popup.html");
  EXPECT_EQ(WTF::kNotFound, mhtml.find("class=3D\"overlay"));
  EXPECT_EQ(WTF::kNotFound, mhtml.find("class=3D\"modal"));
}

TEST_F(WebFrameSerializerSanitizationTest, KeepPopupOverlayIfNotRequested) {
  webView()->resize(WebSize(500, 500));
  setRemovePopupOverlay(false);
  String mhtml = generateMHTMLParts("http://www.test.com", "popup.html");
  EXPECT_NE(WTF::kNotFound, mhtml.find("class=3D\"overlay"));
  EXPECT_NE(WTF::kNotFound, mhtml.find("class=3D\"modal"));
}

TEST_F(WebFrameSerializerSanitizationTest, RemoveElements) {
  String mhtml =
      generateMHTMLParts("http://www.test.com", "remove_elements.html");
  LOG(ERROR) << mhtml;

  EXPECT_EQ(WTF::kNotFound, mhtml.find("<script"));
  EXPECT_EQ(WTF::kNotFound, mhtml.find("<noscript"));

  // Only the meta element containing "Content-Security-Policy" is removed.
  // Other meta elements should be preserved.
  EXPECT_EQ(WTF::kNotFound,
            mhtml.find("<meta http-equiv=3D\"Content-Security-Policy"));
  EXPECT_NE(WTF::kNotFound, mhtml.find("<meta name=3D\"description"));
  EXPECT_NE(WTF::kNotFound, mhtml.find("<meta http-equiv=3D\"refresh"));

  // If an element is removed, its children should also be skipped.
  EXPECT_EQ(WTF::kNotFound, mhtml.find("<select"));
  EXPECT_EQ(WTF::kNotFound, mhtml.find("<option"));
}

}  // namespace blink
