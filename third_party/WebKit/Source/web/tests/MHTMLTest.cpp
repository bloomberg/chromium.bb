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
#include "core/frame/LocalFrame.h"
#include "core/frame/Location.h"
#include "core/page/Page.h"
#include "platform/SerializedResource.h"
#include "platform/SharedBuffer.h"
#include "platform/mhtml/MHTMLArchive.h"
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
#include "web/tests/FrameTestHelpers.h"

using blink::URLTestHelpers::toKURL;

namespace blink {

class LineReader {
 public:
  LineReader(const std::string& text) : m_text(text), m_index(0) {}
  bool getNextLine(std::string* line) {
    line->clear();
    if (m_index >= m_text.length())
      return false;

    size_t endOfLineIndex = m_text.find("\r\n", m_index);
    if (endOfLineIndex == std::string::npos) {
      *line = m_text.substr(m_index);
      m_index = m_text.length();
      return true;
    }

    *line = m_text.substr(m_index, endOfLineIndex - m_index);
    m_index = endOfLineIndex + 2;
    return true;
  }

 private:
  std::string m_text;
  size_t m_index;
};

class MHTMLTest : public ::testing::Test {
 public:
  MHTMLTest() { m_filePath = testing::webTestDataPath("mhtml/"); }

 protected:
  void SetUp() override { m_helper.initialize(); }

  void TearDown() override {
    Platform::current()
        ->getURLLoaderMockFactory()
        ->unregisterAllURLsAndClearMemoryCache();
  }

  void registerMockedURLLoad(const std::string& url,
                             const std::string& fileName) {
    URLTestHelpers::registerMockedURLLoad(
        toKURL(url),
        testing::webTestDataPath(WebString::fromUTF8("mhtml/" + fileName)),
        WebString::fromUTF8("multipart/related"));
  }

  void loadURLInTopFrame(const WebURL& url) {
    FrameTestHelpers::loadFrame(m_helper.webView()->mainFrame(),
                                url.string().utf8().data());
  }

  Page* page() const { return m_helper.webView()->page(); }

  void addResource(const char* url,
                   const char* mime,
                   PassRefPtr<SharedBuffer> data) {
    SerializedResource resource(toKURL(url), mime, data);
    m_resources.push_back(resource);
  }

  void addResource(const char* url, const char* mime, const char* fileName) {
    addResource(url, mime, readFile(fileName));
  }

  void addTestResources() {
    addResource("http://www.test.com", "text/html", "css_test_page.html");
    addResource("http://www.test.com/link_styles.css", "text/css",
                "link_styles.css");
    addResource("http://www.test.com/import_style_from_link.css", "text/css",
                "import_style_from_link.css");
    addResource("http://www.test.com/import_styles.css", "text/css",
                "import_styles.css");
    addResource("http://www.test.com/red_background.png", "image/png",
                "red_background.png");
    addResource("http://www.test.com/orange_background.png", "image/png",
                "orange_background.png");
    addResource("http://www.test.com/yellow_background.png", "image/png",
                "yellow_background.png");
    addResource("http://www.test.com/green_background.png", "image/png",
                "green_background.png");
    addResource("http://www.test.com/blue_background.png", "image/png",
                "blue_background.png");
    addResource("http://www.test.com/purple_background.png", "image/png",
                "purple_background.png");
    addResource("http://www.test.com/ul-dot.png", "image/png", "ul-dot.png");
    addResource("http://www.test.com/ol-dot.png", "image/png", "ol-dot.png");
  }

  static PassRefPtr<RawData> generateMHTMLData(
      const Vector<SerializedResource>& resources,
      MHTMLArchive::EncodingPolicy encodingPolicy,
      const String& title,
      const String& mimeType) {
    // This boundary is as good as any other.  Plus it gets used in almost
    // all the examples in the MHTML spec - RFC 2557.
    String boundary = String::fromUTF8("boundary-example");

    RefPtr<RawData> mhtmlData = RawData::create();
    MHTMLArchive::generateMHTMLHeader(boundary, title, mimeType,
                                      *mhtmlData->mutableData());
    for (const auto& resource : resources) {
      MHTMLArchive::generateMHTMLPart(boundary, String(), encodingPolicy,
                                      resource, *mhtmlData->mutableData());
    }
    MHTMLArchive::generateMHTMLFooter(boundary, *mhtmlData->mutableData());
    return mhtmlData.release();
  }

  PassRefPtr<RawData> serialize(const char* title,
                                const char* mime,
                                MHTMLArchive::EncodingPolicy encodingPolicy) {
    return generateMHTMLData(m_resources, encodingPolicy, title, mime);
  }

 private:
  PassRefPtr<SharedBuffer> readFile(const char* fileName) {
    String filePath = m_filePath + fileName;
    return testing::readFromFile(filePath);
  }

  String m_filePath;
  Vector<SerializedResource> m_resources;
  FrameTestHelpers::WebViewHelper m_helper;
};

// Checks that the domain is set to the actual MHTML file, not the URL it was
// generated from.
TEST_F(MHTMLTest, CheckDomain) {
  const char kFileURL[] = "file:///simple_test.mht";

  // Register the mocked frame and load it.
  WebURL url = toKURL(kFileURL);
  registerMockedURLLoad(kFileURL, "simple_test.mht");
  loadURLInTopFrame(url);
  ASSERT_TRUE(page());
  LocalFrame* frame = toLocalFrame(page()->mainFrame());
  ASSERT_TRUE(frame);
  Document* document = frame->document();
  ASSERT_TRUE(document);

  EXPECT_STREQ(kFileURL, frame->domWindow()->location()->href().ascii().data());

  SecurityOrigin* origin = document->getSecurityOrigin();
  EXPECT_STRNE("localhost", origin->domain().ascii().data());
}

TEST_F(MHTMLTest, TestMHTMLEncoding) {
  addTestResources();
  RefPtr<RawData> data = serialize("Test Serialization", "text/html",
                                   MHTMLArchive::UseDefaultEncoding);

  // Read the MHTML data line per line and do some pseudo-parsing to make sure
  // the right encoding is used for the different sections.
  LineReader lineReader(std::string(data->data(), data->length()));
  int sectionCheckedCount = 0;
  const char* expectedEncoding = 0;
  std::string line;
  while (lineReader.getNextLine(&line)) {
    if (line.compare(0, 13, "Content-Type:") == 0) {
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
    if (line.compare(0, 26, "Content-Transfer-Encoding:") == 0) {
      ASSERT_TRUE(expectedEncoding);
      EXPECT_NE(line.find(expectedEncoding), std::string::npos);
      expectedEncoding = 0;
      sectionCheckedCount++;
    }
  }
  EXPECT_EQ(12, sectionCheckedCount);
}

TEST_F(MHTMLTest, MHTMLFromScheme) {
  addTestResources();
  RefPtr<RawData> rawData = serialize("Test Serialization", "text/html",
                                      MHTMLArchive::UseDefaultEncoding);
  RefPtr<SharedBuffer> data =
      SharedBuffer::create(rawData->data(), rawData->length());
  KURL httpURL = toKURL("http://www.example.com");
  KURL contentURL = toKURL("content://foo");
  KURL fileURL = toKURL("file://foo");
  KURL specialSchemeURL = toKURL("fooscheme://bar");

  // MHTMLArchives can only be initialized from local schemes, http/https
  // schemes, and content scheme(Android specific).
  EXPECT_NE(nullptr, MHTMLArchive::create(httpURL, data.get()));
#if OS(ANDROID)
  EXPECT_NE(nullptr, MHTMLArchive::create(contentURL, data.get()));
#else
  EXPECT_EQ(nullptr, MHTMLArchive::create(contentURL, data.get()));
#endif
  EXPECT_NE(nullptr, MHTMLArchive::create(fileURL, data.get()));
  EXPECT_EQ(nullptr, MHTMLArchive::create(specialSchemeURL, data.get()));
  SchemeRegistry::registerURLSchemeAsLocal("fooscheme");
  EXPECT_NE(nullptr, MHTMLArchive::create(specialSchemeURL, data.get()));
}

// Checks that full sandboxing protection has been turned on.
TEST_F(MHTMLTest, EnforceSandboxFlags) {
  const char kURL[] = "http://www.example.com";

  // Register the mocked frame and load it.
  registerMockedURLLoad(kURL, "simple_test.mht");
  loadURLInTopFrame(toKURL(kURL));
  ASSERT_TRUE(page());
  LocalFrame* frame = toLocalFrame(page()->mainFrame());
  ASSERT_TRUE(frame);
  Document* document = frame->document();
  ASSERT_TRUE(document);

  // Full sandboxing should be turned on.
  EXPECT_TRUE(document->isSandboxed(SandboxAll));

  // MHTML document should be loaded into unique origin.
  EXPECT_TRUE(document->getSecurityOrigin()->isUnique());
  // Script execution should be disabled.
  EXPECT_FALSE(document->canExecuteScripts(NotAboutToExecuteScript));
}

}  // namespace blink
