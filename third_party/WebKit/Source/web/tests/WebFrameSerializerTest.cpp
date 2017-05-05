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

#include "core/exported/WebViewBase.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/mhtml/MHTMLParser.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCString.h"
#include "public/platform/WebCache.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebFrameSerializerClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"

namespace blink {

namespace {
class SimpleWebFrameSerializerClient final : public WebFrameSerializerClient {
 public:
  String ToString() { return builder_.ToString(); }

 private:
  void DidSerializeDataForFrame(const WebCString& data,
                                FrameSerializationStatus) final {
    builder_.Append(data.Data(), data.length());
  }

  StringBuilder builder_;
};

class SimpleMHTMLPartsGenerationDelegate
    : public WebFrameSerializer::MHTMLPartsGenerationDelegate {
 public:
  SimpleMHTMLPartsGenerationDelegate() : remove_popup_overlay_(false) {}

  void SetRemovePopupOverlay(bool remove_popup_overlay) {
    remove_popup_overlay_ = remove_popup_overlay;
  }

 private:
  bool ShouldSkipResource(const WebURL&) final { return false; }

  WebString GetContentID(WebFrame*) final { return WebString("<cid>"); }

  WebFrameSerializerCacheControlPolicy CacheControlPolicy() final {
    return WebFrameSerializerCacheControlPolicy::kNone;
  }

  bool UseBinaryEncoding() final { return false; }
  bool RemovePopupOverlay() final { return remove_popup_overlay_; }

  bool remove_popup_overlay_;
};

// Returns the count of match for substring |pattern| in string |str|.
int MatchSubstring(const String& str, const char* pattern, size_t size) {
  int matches = 0;
  size_t start = 0;
  while (true) {
    size_t pos = str.Find(pattern, start);
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
  WebFrameSerializerTest() { helper_.Initialize(); }

  ~WebFrameSerializerTest() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void RegisterMockedImageURLLoad(const String& url) {
    // Image resources need to be mocked, but irrelevant here what image they
    // map to.
    RegisterMockedFileURLLoad(URLTestHelpers::ToKURL(url.Utf8().data()),
                              "frameserialization/awesome.png");
  }
  void RegisterMockedFileURLLoad(const KURL& url,
                                 const String& file_path,
                                 const String& mime_type = "image/png") {
    URLTestHelpers::RegisterMockedURLLoad(
        url, testing::WebTestDataPath(file_path.Utf8().data()), mime_type);
  }

  class SingleLinkRewritingDelegate
      : public WebFrameSerializer::LinkRewritingDelegate {
   public:
    SingleLinkRewritingDelegate(const WebURL& url, const WebString& local_path)
        : url_(url), local_path_(local_path) {}

    bool RewriteFrameSource(WebFrame* frame,
                            WebString* rewritten_link) override {
      return false;
    }

    bool RewriteLink(const WebURL& url, WebString* rewritten_link) override {
      if (url != url_)
        return false;

      *rewritten_link = local_path_;
      return true;
    }

   private:
    const WebURL url_;
    const WebString local_path_;
  };

  String SerializeFile(const String& url, const String& file_name) {
    KURL parsed_url(kParsedURLString, url);
    String file_path("frameserialization/" + file_name);
    RegisterMockedFileURLLoad(parsed_url, file_path, "text/html");
    FrameTestHelpers::LoadFrame(MainFrameImpl(), url.Utf8().data());
    SingleLinkRewritingDelegate delegate(parsed_url, WebString("local"));
    SimpleWebFrameSerializerClient serializer_client;
    WebFrameSerializer::Serialize(MainFrameImpl(), &serializer_client,
                                  &delegate);
    return serializer_client.ToString();
  }

  WebViewBase* WebView() { return helper_.WebView(); }

  WebLocalFrameImpl* MainFrameImpl() {
    return helper_.WebView()->MainFrameImpl();
  }

 private:
  FrameTestHelpers::WebViewHelper helper_;
};

TEST_F(WebFrameSerializerTest, URLAttributeValues) {
  RegisterMockedImageURLLoad("javascript:\"");

  const char* expected_html =
      "\n<!-- saved from url=(0020)http://www.test.com/ -->\n"
      "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; "
      "charset=UTF-8\">\n"
      "</head><body><img src=\"javascript:&quot;\">\n"
      "<a href=\"http://www.test.com/local#&quot;\">local</a>\n"
      "<a "
      "href=\"http://www.example.com/#&quot;&gt;&lt;script&gt;alert(0)&lt;/"
      "script&gt;\">external</a>\n"
      "</body></html>";
  String actual_html =
      SerializeFile("http://www.test.com", "url_attribute_values.html");
  EXPECT_EQ(expected_html, actual_html);
}

TEST_F(WebFrameSerializerTest, EncodingAndNormalization) {
  const char* expected_html =
      "<!DOCTYPE html>\n"
      "<!-- saved from url=(0020)http://www.test.com/ -->\n"
      "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; "
      "charset=EUC-KR\">\n"
      "<title>Ensure NFC normalization is not performed by frame "
      "serializer</title>\n"
      "</head><body>\n"
      "\xe4\xc5\xd1\xe2\n"
      "\n</body></html>";
  String actual_html =
      SerializeFile("http://www.test.com", "encoding_normalization.html");
  EXPECT_EQ(expected_html, actual_html);
}

TEST_F(WebFrameSerializerTest, FromUrlWithMinusMinus) {
  String actual_html =
      SerializeFile("http://www.test.com?--x--", "text_only_page.html");
  EXPECT_EQ("<!-- saved from url=(0030)http://www.test.com/?-%2Dx-%2D -->",
            actual_html.Substring(1, 60));
}

class WebFrameSerializerSanitizationTest : public WebFrameSerializerTest {
 protected:
  WebFrameSerializerSanitizationTest() {}

  ~WebFrameSerializerSanitizationTest() override {}

  String GenerateMHTMLFromHtml(const String& url, const String& file_name) {
    return GenerateMHTML(url, file_name, "text/html", false);
  }

  String GenerateMHTMLPartsFromPng(const String& url, const String& file_name) {
    return GenerateMHTML(url, file_name, "image/png", true);
  }

  String GenerateMHTML(const String& url,
                       const String& file_name,
                       const String& mime_type,
                       const bool only_body_parts) {
    KURL parsed_url(kParsedURLString, url);
    String file_path("frameserialization/" + file_name);
    RegisterMockedFileURLLoad(parsed_url, file_path, mime_type);
    FrameTestHelpers::LoadFrame(MainFrameImpl(), url.Utf8().data());
    // Boundaries are normally randomly generated but this one is predefined for
    // simplicity and as good as any other. Plus it gets used in almost all the
    // examples in the MHTML spec - RFC 2557.
    const WebString boundary("boundary-example");
    StringBuilder mhtml;
    if (!only_body_parts) {
      WebThreadSafeData header_result = WebFrameSerializer::GenerateMHTMLHeader(
          boundary, MainFrameImpl(), &mhtml_delegate_);
      mhtml.Append(header_result.Data(), header_result.size());
    }

    WebThreadSafeData body_result = WebFrameSerializer::GenerateMHTMLParts(
        boundary, MainFrameImpl(), &mhtml_delegate_);
    mhtml.Append(body_result.Data(), body_result.size());

    if (!only_body_parts) {
      RefPtr<RawData> footer_data = RawData::Create();
      MHTMLArchive::GenerateMHTMLFooterForTesting(boundary,
                                                  *footer_data->MutableData());
      mhtml.Append(footer_data->data(), footer_data->length());
    }

    String mhtml_string = mhtml.ToString();
    if (!only_body_parts) {
      // Validate the generated MHTML.
      MHTMLParser parser(SharedBuffer::Create(mhtml_string.Characters8(),
                                              size_t(mhtml_string.length())));
      EXPECT_FALSE(parser.ParseArchive().IsEmpty())
          << "Generated MHTML is not well formed";
    }
    return mhtml_string;
  }

  void SetRemovePopupOverlay(bool remove_popup_overlay) {
    mhtml_delegate_.SetRemovePopupOverlay(remove_popup_overlay);
  }

 protected:
  HistogramTester histogram_tester_;

 private:
  SimpleMHTMLPartsGenerationDelegate mhtml_delegate_;
};

TEST_F(WebFrameSerializerSanitizationTest, RemoveInlineScriptInAttributes) {
  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "script_in_attributes.html");

  // These scripting attributes should be removed.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("onload="));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("ONLOAD="));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("onclick="));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("href="));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("from="));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("to="));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("javascript:"));

  // These non-scripting attributes should remain intact.
  EXPECT_NE(WTF::kNotFound, mhtml.Find("class="));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("id="));

  // srcdoc attribute of frame element should be replaced with src attribute.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("srcdoc="));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("src="));
}

TEST_F(WebFrameSerializerSanitizationTest, RemoveOtherAttributes) {
  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "remove_attributes.html");
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("ping="));
}

TEST_F(WebFrameSerializerSanitizationTest, DisableFormElements) {
  String mhtml = GenerateMHTMLFromHtml("http://www.test.com", "form.html");

  const char kDisabledAttr[] = "disabled=3D\"\"";
  int matches =
      MatchSubstring(mhtml, kDisabledAttr, arraysize(kDisabledAttr) - 1);
  EXPECT_EQ(21, matches);
}

TEST_F(WebFrameSerializerSanitizationTest, RemoveHiddenElements) {
  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "hidden_elements.html");

  // The element with hidden attribute should be removed.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("<p id=3D\"hidden_id\""));

  // The hidden form element should be removed.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("<input type=3D\"hidden\""));

  // All other hidden elements should not be removed.
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<html"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<head"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<style"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<title"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<h1"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<h2"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<datalist"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<option"));
  // One for meta in head and another for meta in body.
  EXPECT_EQ(2, MatchSubstring(mhtml, "<meta", 5));
  // One for style in head and another for style in body.
  EXPECT_EQ(2, MatchSubstring(mhtml, "<style", 6));
  // One for link in head and another for link in body.
  EXPECT_EQ(2, MatchSubstring(mhtml, "<link", 5));

  // These visible elements should remain intact.
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<p id=3D\"visible_id\""));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<form"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<input type=3D\"text\""));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<div"));
}

// Regression test for crbug.com/678893, where in some cases serializing an
// image document could cause code to pick an element from an empty container.
TEST_F(WebFrameSerializerSanitizationTest, FromBrokenImageDocument) {
  // This test only cares that the result of the parts generation is empty so it
  // is simpler to not generate only that instead of the full MHTML.
  String mhtml =
      GenerateMHTMLPartsFromPng("http://www.test.com", "broken-image.png");
  EXPECT_TRUE(mhtml.IsEmpty());
}

TEST_F(WebFrameSerializerSanitizationTest, ImageLoadedFromSrcsetForHiDPI) {
  RegisterMockedFileURLLoad(
      KURL(kParsedURLString, "http://www.test.com/1x.png"),
      "frameserialization/1x.png");
  RegisterMockedFileURLLoad(
      KURL(kParsedURLString, "http://www.test.com/2x.png"),
      "frameserialization/2x.png");

  // Set high DPR in order to load image from srcset, instead of src.
  WebView()->SetDeviceScaleFactor(2.0f);

  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "img_srcset.html");

  // srcset attribute should be skipped.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("srcset="));

  // Width and height attributes should be set when none is present in <img>.
  EXPECT_NE(WTF::kNotFound,
            mhtml.Find("id=3D\"i1\" width=3D\"6\" height=3D\"6\">"));

  // Height attribute should not be set if width attribute is already present in
  // <img>
  EXPECT_NE(WTF::kNotFound, mhtml.Find("id=3D\"i2\" width=3D\"8\">"));
}

TEST_F(WebFrameSerializerSanitizationTest, ImageLoadedFromSrcForNormalDPI) {
  RegisterMockedFileURLLoad(
      KURL(kParsedURLString, "http://www.test.com/1x.png"),
      "frameserialization/1x.png");
  RegisterMockedFileURLLoad(
      KURL(kParsedURLString, "http://www.test.com/2x.png"),
      "frameserialization/2x.png");

  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "img_srcset.html");

  // srcset attribute should be skipped.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("srcset="));

  // New width and height attributes should not be set.
  EXPECT_NE(WTF::kNotFound, mhtml.Find("id=3D\"i1\">"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("id=3D\"i2\" width=3D\"8\">"));
}

TEST_F(WebFrameSerializerSanitizationTest, RemovePopupOverlayIfRequested) {
  WebView()->Resize(WebSize(500, 500));
  SetRemovePopupOverlay(true);
  String mhtml = GenerateMHTMLFromHtml("http://www.test.com", "popup.html");
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("class=3D\"overlay"));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("class=3D\"modal"));
  histogram_tester_.ExpectUniqueSample(
      "PageSerialization.MhtmlGeneration.PopupOverlaySkipped", true, 1);
}

TEST_F(WebFrameSerializerSanitizationTest, PopupOverlayNotFound) {
  WebView()->Resize(WebSize(500, 500));
  SetRemovePopupOverlay(true);
  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "text_only_page.html");
  histogram_tester_.ExpectUniqueSample(
      "PageSerialization.MhtmlGeneration.PopupOverlaySkipped", false, 1);
}

TEST_F(WebFrameSerializerSanitizationTest, KeepPopupOverlayIfNotRequested) {
  WebView()->Resize(WebSize(500, 500));
  SetRemovePopupOverlay(false);
  String mhtml = GenerateMHTMLFromHtml("http://www.test.com", "popup.html");
  EXPECT_NE(WTF::kNotFound, mhtml.Find("class=3D\"overlay"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("class=3D\"modal"));
  histogram_tester_.ExpectTotalCount(
      "PageSerialization.MhtmlGeneration.PopupOverlaySkipped", 0);
}

TEST_F(WebFrameSerializerSanitizationTest, RemoveElements) {
  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "remove_elements.html");

  EXPECT_EQ(WTF::kNotFound, mhtml.Find("<script"));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("<noscript"));

  // Only the meta element containing "Content-Security-Policy" is removed.
  // Other meta elements should be preserved.
  EXPECT_EQ(WTF::kNotFound,
            mhtml.Find("<meta http-equiv=3D\"Content-Security-Policy"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<meta name=3D\"description"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<meta http-equiv=3D\"refresh"));

  // If an element is removed, its children should also be skipped.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("<select"));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("<option"));
}

}  // namespace blink
