// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "build/build_config.h"
#include "platform/SerializedResource.h"
#include "platform/SharedBuffer.h"
#include "platform/blob/BlobData.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/mhtml/MHTMLParser.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SchemeRegistry.h"
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

class MHTMLArchiveTest : public ::testing::Test {
 public:
  MHTMLArchiveTest() { file_path_ = testing::CoreTestDataPath("mhtml/"); }

 protected:
  void AddResource(const char* url,
                   const char* mime,
                   scoped_refptr<SharedBuffer> data) {
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

  static std::map<std::string, std::string> ExtractMHTMLHeaders(
      scoped_refptr<RawData> mhtml_data) {
    // Read the MHTML data per line until reaching the empty line.
    std::map<std::string, std::string> mhtml_headers;
    LineReader line_reader(
        std::string(mhtml_data->data(), mhtml_data->length()));
    std::string line;
    line_reader.GetNextLine(&line);
    while (line.length()) {
      // Peek next line to see if it starts with soft line break. If yes, append
      // to current line.
      std::string next_line;
      while (true) {
        line_reader.GetNextLine(&next_line);
        if (next_line.length() > 1 &&
            (next_line[0] == ' ' || next_line[0] == '\t')) {
          line += &(next_line.at(1));
          continue;
        }
        break;
      }

      std::string::size_type pos = line.find(':');
      if (pos == std::string::npos)
        continue;
      std::string key = line.substr(0, pos);
      std::string value = line.substr(pos + 2);
      mhtml_headers.emplace(key, value);

      line = next_line;
    }
    return mhtml_headers;
  }

  static scoped_refptr<RawData> GenerateMHTMLData(
      const Vector<SerializedResource>& resources,
      MHTMLArchive::EncodingPolicy encoding_policy,
      const KURL& url,
      const String& title,
      const String& mime_type) {
    // This boundary is as good as any other.  Plus it gets used in almost
    // all the examples in the MHTML spec - RFC 2557.
    String boundary = String::FromUTF8("boundary-example");

    scoped_refptr<RawData> mhtml_data = RawData::Create();
    MHTMLArchive::GenerateMHTMLHeader(boundary, url, title, mime_type,
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

    return mhtml_data;
  }

  scoped_refptr<RawData> Serialize(
      const KURL& url,
      const String& title,
      const String& mime,
      MHTMLArchive::EncodingPolicy encoding_policy) {
    return GenerateMHTMLData(resources_, encoding_policy, url, title, mime);
  }

 private:
  scoped_refptr<SharedBuffer> ReadFile(const char* file_name) {
    String file_path = file_path_ + file_name;
    return testing::ReadFromFile(file_path);
  }

  String file_path_;
  Vector<SerializedResource> resources_;
};

TEST_F(MHTMLArchiveTest,
       TestMHTMLHeadersWithTitleContainingAllPrintableCharacters) {
  const char kURL[] = "http://www.example.com/";
  const char kTitle[] = "abc";
  AddTestResources();
  scoped_refptr<RawData> data =
      Serialize(ToKURL(kURL), String::FromUTF8(kTitle), "text/html",
                MHTMLArchive::kUseDefaultEncoding);

  std::map<std::string, std::string> mhtml_headers = ExtractMHTMLHeaders(data);

  EXPECT_EQ("<Saved by Blink>", mhtml_headers["From"]);
  EXPECT_FALSE(mhtml_headers["Date"].empty());
  EXPECT_EQ(
      "multipart/related;type=\"text/html\";boundary=\"boundary-example\"",
      mhtml_headers["Content-Type"]);
  EXPECT_EQ("abc", mhtml_headers["Subject"]);
  EXPECT_EQ(kURL, mhtml_headers["Snapshot-Content-Location"]);
}

TEST_F(MHTMLArchiveTest,
       TestMHTMLHeadersWithTitleContainingNonPrintableCharacters) {
  const char kURL[] = "http://www.example.com/";
  const char kTitle[] = "abc \t=\xe2\x98\x9d\xf0\x9f\x8f\xbb";
  AddTestResources();
  scoped_refptr<RawData> data =
      Serialize(ToKURL(kURL), String::FromUTF8(kTitle), "text/html",
                MHTMLArchive::kUseDefaultEncoding);

  std::map<std::string, std::string> mhtml_headers = ExtractMHTMLHeaders(data);

  EXPECT_EQ("<Saved by Blink>", mhtml_headers["From"]);
  EXPECT_FALSE(mhtml_headers["Date"].empty());
  EXPECT_EQ(
      "multipart/related;type=\"text/html\";boundary=\"boundary-example\"",
      mhtml_headers["Content-Type"]);
  EXPECT_EQ("=?utf-8?Q?abc=20=09=3D=E2=98=9D=F0=9F=8F=BB?=",
            mhtml_headers["Subject"]);
  EXPECT_EQ(kURL, mhtml_headers["Snapshot-Content-Location"]);
}

TEST_F(MHTMLArchiveTest,
       TestMHTMLHeadersWithLongTitleContainingNonPrintableCharacters) {
  const char kURL[] = "http://www.example.com/";
  const char kTitle[] =
      "01234567890123456789012345678901234567890123456789"
      "01234567890123456789012345678901234567890123456789"
      " \t=\xe2\x98\x9d\xf0\x9f\x8f\xbb";
  AddTestResources();
  scoped_refptr<RawData> data =
      Serialize(ToKURL(kURL), String::FromUTF8(kTitle), "text/html",
                MHTMLArchive::kUseDefaultEncoding);

  std::map<std::string, std::string> mhtml_headers = ExtractMHTMLHeaders(data);

  EXPECT_EQ("<Saved by Blink>", mhtml_headers["From"]);
  EXPECT_FALSE(mhtml_headers["Date"].empty());
  EXPECT_EQ(
      "multipart/related;type=\"text/html\";boundary=\"boundary-example\"",
      mhtml_headers["Content-Type"]);
  EXPECT_EQ(
      "=?utf-8?Q?012345678901234567890123456789"
      "012345678901234567890123456789012?="
      "=?utf-8?Q?345678901234567890123456789"
      "0123456789=20=09=3D=E2=98=9D=F0=9F?="
      "=?utf-8?Q?=8F=BB?=",
      mhtml_headers["Subject"]);
  EXPECT_EQ(kURL, mhtml_headers["Snapshot-Content-Location"]);
}

TEST_F(MHTMLArchiveTest, TestMHTMLEncoding) {
  const char kURL[] = "http://www.example.com";
  AddTestResources();
  scoped_refptr<RawData> data =
      Serialize(ToKURL(kURL), "Test Serialization", "text/html",
                MHTMLArchive::kUseDefaultEncoding);

  // Read the MHTML data line per line and do some pseudo-parsing to make sure
  // the right encoding is used for the different sections.
  LineReader line_reader(std::string(data->data(), data->length()));
  int section_checked_count = 0;
  const char* expected_encoding = nullptr;
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
      expected_encoding = nullptr;
      section_checked_count++;
    }
  }
  EXPECT_EQ(12, section_checked_count);
}

TEST_F(MHTMLArchiveTest, MHTMLFromScheme) {
  const char kURL[] = "http://www.example.com";
  AddTestResources();
  scoped_refptr<RawData> raw_data =
      Serialize(ToKURL(kURL), "Test Serialization", "text/html",
                MHTMLArchive::kUseDefaultEncoding);

  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(raw_data->data(), raw_data->length());
  KURL http_url = ToKURL("http://www.example.com");
  KURL content_url = ToKURL("content://foo");
  KURL file_url = ToKURL("file://foo");
  KURL special_scheme_url = ToKURL("fooscheme://bar");

  // MHTMLArchives can only be initialized from local schemes, http/https
  // schemes, and content scheme(Android specific).
  EXPECT_NE(nullptr, MHTMLArchive::Create(http_url, data.get()));
#if defined(OS_ANDROID)
  EXPECT_NE(nullptr, MHTMLArchive::Create(content_url, data.get()));
#else
  EXPECT_EQ(nullptr, MHTMLArchive::Create(content_url, data.get()));
#endif
  EXPECT_NE(nullptr, MHTMLArchive::Create(file_url, data.get()));
  EXPECT_EQ(nullptr, MHTMLArchive::Create(special_scheme_url, data.get()));
  SchemeRegistry::RegisterURLSchemeAsLocal("fooscheme");
  EXPECT_NE(nullptr, MHTMLArchive::Create(special_scheme_url, data.get()));
}

}  // namespace blink
