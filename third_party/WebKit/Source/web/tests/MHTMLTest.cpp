/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Location.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/page/Page.h"
#include "platform/SerializedResource.h"
#include "platform/SharedBuffer.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/mhtml/MHTMLParser.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "public/platform/Platform.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrame.h"
#include "public/web/WebView.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::URLTestHelpers::ToKURL;

namespace blink {

class LineReader {
 public:
  LineReader(const std::string& text) : text_(text), index_(0) {}
  bool GetNextLine(std::string* line) {
    line->clear();
    if (index_ >= text_.length())
      return false;

    size_t end_of_line_index = text_.find("\r\n", index_);
    if (end_of_line_index == std::string::npos) {
      *line = text_.substr(index_);
      index_ = text_.length();
      return true;
    }

    *line = text_.substr(index_, end_of_line_index - index_);
    index_ = end_of_line_index + 2;
    return true;
  }

 private:
  std::string text_;
  size_t index_;
};

class MHTMLTest : public ::testing::Test {
 public:
  MHTMLTest() { file_path_ = testing::WebTestDataPath("mhtml/"); }

 protected:
  void SetUp() override { helper_.Initialize(); }

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void RegisterMockedURLLoad(const std::string& url,
                             const std::string& file_name) {
    URLTestHelpers::RegisterMockedURLLoad(
        ToKURL(url),
        testing::WebTestDataPath(WebString::FromUTF8("mhtml/" + file_name)),
        WebString::FromUTF8("multipart/related"));
  }

  void LoadURLInTopFrame(const WebURL& url) {
    FrameTestHelpers::LoadFrame(helper_.WebView()->MainFrameImpl(),
                                url.GetString().Utf8().data());
  }

  Page* GetPage() const { return helper_.WebView()->GetPage(); }

  void AddResource(const char* url,
                   const char* mime,
                   PassRefPtr<SharedBuffer> data) {
    SerializedResource resource(ToKURL(url), mime, std::move(data));
    resources_.push_back(resource);
  }

  void AddResource(const char* url, const char* mime, const char* file_name) {
    AddResource(url, mime, ReadFile(file_name));
  }

  void AddTestResources() {
    AddResource("http://www.test.com", "text/html", "css_test_page.html");
    AddResource("http://www.test.com/link_styles.css", "text/css",
                "link_styles.css");
    AddResource("http://www.test.com/import_style_from_link.css", "text/css",
                "import_style_from_link.css");
    AddResource("http://www.test.com/import_styles.css", "text/css",
                "import_styles.css");
    AddResource("http://www.test.com/red_background.png", "image/png",
                "red_background.png");
    AddResource("http://www.test.com/orange_background.png", "image/png",
                "orange_background.png");
    AddResource("http://www.test.com/yellow_background.png", "image/png",
                "yellow_background.png");
    AddResource("http://www.test.com/green_background.png", "image/png",
                "green_background.png");
    AddResource("http://www.test.com/blue_background.png", "image/png",
                "blue_background.png");
    AddResource("http://www.test.com/purple_background.png", "image/png",
                "purple_background.png");
    AddResource("http://www.test.com/ul-dot.png", "image/png", "ul-dot.png");
    AddResource("http://www.test.com/ol-dot.png", "image/png", "ol-dot.png");
  }

  static PassRefPtr<RawData> GenerateMHTMLData(
      const Vector<SerializedResource>& resources,
      MHTMLArchive::EncodingPolicy encoding_policy,
      const String& title,
      const String& mime_type) {
    // This boundary is as good as any other.  Plus it gets used in almost
    // all the examples in the MHTML spec - RFC 2557.
    String boundary = String::FromUTF8("boundary-example");

    RefPtr<RawData> mhtml_data = RawData::Create();
    MHTMLArchive::GenerateMHTMLHeader(boundary, title, mime_type,
                                      *mhtml_data->MutableData());
    for (const auto& resource : resources) {
      MHTMLArchive::GenerateMHTMLPart(boundary, String(), encoding_policy,
                                      resource, *mhtml_data->MutableData());
    }
    MHTMLArchive::GenerateMHTMLFooterForTesting(boundary,
                                                *mhtml_data->MutableData());

    // Validate the generated MHTML.
    MHTMLParser parser(
        SharedBuffer::Create(mhtml_data->data(), mhtml_data->length()));
    EXPECT_FALSE(parser.ParseArchive().IsEmpty())
        << "Generated MHTML is malformed";

    return mhtml_data.Release();
  }

  PassRefPtr<RawData> Serialize(const char* title,
                                const char* mime,
                                MHTMLArchive::EncodingPolicy encoding_policy) {
    return GenerateMHTMLData(resources_, encoding_policy, title, mime);
  }

 private:
  PassRefPtr<SharedBuffer> ReadFile(const char* file_name) {
    String file_path = file_path_ + file_name;
    return testing::ReadFromFile(file_path);
  }

  String file_path_;
  Vector<SerializedResource> resources_;
  FrameTestHelpers::WebViewHelper helper_;
};

// Checks that the domain is set to the actual MHTML file, not the URL it was
// generated from.
TEST_F(MHTMLTest, CheckDomain) {
  const char kFileURL[] = "file:///simple_test.mht";

  // Register the mocked frame and load it.
  WebURL url = ToKURL(kFileURL);
  RegisterMockedURLLoad(kFileURL, "simple_test.mht");
  LoadURLInTopFrame(url);
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = ToLocalFrame(GetPage()->MainFrame());
  ASSERT_TRUE(frame);
  Document* document = frame->GetDocument();
  ASSERT_TRUE(document);

  EXPECT_STREQ(kFileURL, frame->DomWindow()->location()->href().Ascii().data());

  SecurityOrigin* origin = document->GetSecurityOrigin();
  EXPECT_STRNE("localhost", origin->Domain().Ascii().data());
}

TEST_F(MHTMLTest, TestMHTMLEncoding) {
  AddTestResources();
  RefPtr<RawData> data = Serialize("Test Serialization", "text/html",
                                   MHTMLArchive::kUseDefaultEncoding);

  // Read the MHTML data line per line and do some pseudo-parsing to make sure
  // the right encoding is used for the different sections.
  LineReader line_reader(std::string(data->data(), data->length()));
  int section_checked_count = 0;
  const char* expected_encoding = 0;
  std::string line;
  while (line_reader.GetNextLine(&line)) {
    if (line.compare(0, 13, "Content-Type:") == 0) {
      ASSERT_FALSE(expected_encoding);
      if (line.find("multipart/related;") != std::string::npos) {
        // Skip this one, it's part of the MHTML header.
        continue;
      }
      if (line.find("text/") != std::string::npos)
        expected_encoding = "quoted-printable";
      else if (line.find("image/") != std::string::npos)
        expected_encoding = "base64";
      else
        FAIL() << "Unexpected Content-Type: " << line;
      continue;
    }
    if (line.compare(0, 26, "Content-Transfer-Encoding:") == 0) {
      ASSERT_TRUE(expected_encoding);
      EXPECT_NE(line.find(expected_encoding), std::string::npos);
      expected_encoding = 0;
      section_checked_count++;
    }
  }
  EXPECT_EQ(12, section_checked_count);
}

TEST_F(MHTMLTest, MHTMLFromScheme) {
  AddTestResources();
  RefPtr<RawData> raw_data = Serialize("Test Serialization", "text/html",
                                       MHTMLArchive::kUseDefaultEncoding);

  RefPtr<SharedBuffer> data =
      SharedBuffer::Create(raw_data->data(), raw_data->length());
  KURL http_url = ToKURL("http://www.example.com");
  KURL content_url = ToKURL("content://foo");
  KURL file_url = ToKURL("file://foo");
  KURL special_scheme_url = ToKURL("fooscheme://bar");

  // MHTMLArchives can only be initialized from local schemes, http/https
  // schemes, and content scheme(Android specific).
  EXPECT_NE(nullptr, MHTMLArchive::Create(http_url, data.Get()));
#if OS(ANDROID)
  EXPECT_NE(nullptr, MHTMLArchive::Create(content_url, data.Get()));
#else
  EXPECT_EQ(nullptr, MHTMLArchive::Create(content_url, data.Get()));
#endif
  EXPECT_NE(nullptr, MHTMLArchive::Create(file_url, data.Get()));
  EXPECT_EQ(nullptr, MHTMLArchive::Create(special_scheme_url, data.Get()));
  SchemeRegistry::RegisterURLSchemeAsLocal("fooscheme");
  EXPECT_NE(nullptr, MHTMLArchive::Create(special_scheme_url, data.Get()));
}

// Checks that full sandboxing protection has been turned on.
TEST_F(MHTMLTest, EnforceSandboxFlags) {
  const char kURL[] = "http://www.example.com";

  // Register the mocked frame and load it.
  RegisterMockedURLLoad(kURL, "page_with_javascript.mht");
  LoadURLInTopFrame(ToKURL(kURL));
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = ToLocalFrame(GetPage()->MainFrame());
  ASSERT_TRUE(frame);
  Document* document = frame->GetDocument();
  ASSERT_TRUE(document);

  // Full sandboxing with the exception to new top-level windows should be
  // turned on.
  EXPECT_EQ(kSandboxAll & ~(kSandboxPopups |
                            kSandboxPropagatesToAuxiliaryBrowsingContexts),
            document->GetSandboxFlags());

  // MHTML document should be loaded into unique origin.
  EXPECT_TRUE(document->GetSecurityOrigin()->IsUnique());
  // Script execution should be disabled.
  EXPECT_FALSE(document->CanExecuteScripts(kNotAboutToExecuteScript));

  // The element to be created by the script is not there.
  EXPECT_FALSE(document->getElementById("mySpan"));
}

TEST_F(MHTMLTest, ShadowDom) {
  const char kURL[] = "http://www.example.com";

  // Register the mocked frame and load it.
  RegisterMockedURLLoad(kURL, "shadow.mht");
  LoadURLInTopFrame(ToKURL(kURL));
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = ToLocalFrame(GetPage()->MainFrame());
  ASSERT_TRUE(frame);
  Document* document = frame->GetDocument();
  ASSERT_TRUE(document);

  EXPECT_TRUE(IsShadowHost(document->getElementById("h1")));
  EXPECT_TRUE(IsShadowHost(document->getElementById("h2")));
  // The nested shadow DOM tree is created.
  EXPECT_TRUE(IsShadowHost(document->getElementById("h2")
                               ->Shadow()
                               ->OldestShadowRoot()
                               .getElementById("h3")));

  EXPECT_TRUE(IsShadowHost(document->getElementById("h4")));
  // The static element in the shadow dom template is found.
  EXPECT_TRUE(document->getElementById("h4")
                  ->Shadow()
                  ->OldestShadowRoot()
                  .getElementById("s1"));
  // The element to be created by the script in the shadow dom template is
  // not found because the script is blocked.
  EXPECT_FALSE(document->getElementById("h4")
                   ->Shadow()
                   ->OldestShadowRoot()
                   .getElementById("s2"));
}

}  // namespace blink
