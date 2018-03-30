// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "services/network/cross_origin_read_blocking.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using MimeType = network::CrossOriginReadBlocking::MimeType;
using SniffingResult = network::CrossOriginReadBlocking::SniffingResult;

namespace network {

TEST(CrossOriginReadBlockingTest, IsBlockableScheme) {
  GURL data_url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAA==");
  GURL ftp_url("ftp://google.com");
  GURL mailto_url("mailto:google@google.com");
  GURL about_url("about:chrome");
  GURL http_url("http://google.com");
  GURL https_url("https://google.com");

  EXPECT_FALSE(CrossOriginReadBlocking::IsBlockableScheme(data_url));
  EXPECT_FALSE(CrossOriginReadBlocking::IsBlockableScheme(ftp_url));
  EXPECT_FALSE(CrossOriginReadBlocking::IsBlockableScheme(mailto_url));
  EXPECT_FALSE(CrossOriginReadBlocking::IsBlockableScheme(about_url));
  EXPECT_TRUE(CrossOriginReadBlocking::IsBlockableScheme(http_url));
  EXPECT_TRUE(CrossOriginReadBlocking::IsBlockableScheme(https_url));
}

TEST(CrossOriginReadBlockingTest, IsValidCorsHeaderSet) {
  url::Origin frame_origin = url::Origin::Create(GURL("http://www.google.com"));

  EXPECT_TRUE(CrossOriginReadBlocking::IsValidCorsHeaderSet(frame_origin, "*"));
  EXPECT_FALSE(
      CrossOriginReadBlocking::IsValidCorsHeaderSet(frame_origin, "\"*\""));
  EXPECT_FALSE(CrossOriginReadBlocking::IsValidCorsHeaderSet(
      frame_origin, "http://mail.google.com"));
  EXPECT_TRUE(CrossOriginReadBlocking::IsValidCorsHeaderSet(
      frame_origin, "http://www.google.com"));
  EXPECT_FALSE(CrossOriginReadBlocking::IsValidCorsHeaderSet(
      frame_origin, "https://www.google.com"));
  EXPECT_FALSE(CrossOriginReadBlocking::IsValidCorsHeaderSet(
      frame_origin, "http://yahoo.com"));
  EXPECT_FALSE(CrossOriginReadBlocking::IsValidCorsHeaderSet(frame_origin,
                                                             "www.google.com"));
}

TEST(CrossOriginReadBlockingTest, SniffForHTML) {
  StringPiece html_data("  \t\r\n    <HtMladfokadfkado");
  StringPiece comment_html_data(" <!-- this is comment --> <html><body>");
  StringPiece two_comments_html_data(
      "<!-- this is comment -->\n<!-- this is comment --><html><body>");
  StringPiece commented_out_html_tag_data("<!-- <html> <?xml> \n<html>--><b");
  StringPiece mixed_comments_html_data(
      "<!-- this is comment <!-- --> <script></script>");
  StringPiece non_html_data("        var name=window.location;\nadfadf");
  StringPiece comment_js_data(
      " <!-- this is comment\n document.write(1);\n// -->window.open()");
  StringPiece empty_data("");

  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForHTML(html_data));
  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForHTML(comment_html_data));
  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForHTML(two_comments_html_data));
  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForHTML(commented_out_html_tag_data));
  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForHTML(mixed_comments_html_data));
  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForHTML(non_html_data));
  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForHTML(comment_js_data));

  // Prefixes of |commented_out_html_tag_data| should be indeterminate.
  StringPiece almost_html = commented_out_html_tag_data;
  while (!almost_html.empty()) {
    almost_html.remove_suffix(1);
    EXPECT_EQ(SniffingResult::kMaybe,
              CrossOriginReadBlocking::SniffForHTML(almost_html))
        << almost_html;
  }
}

TEST(CrossOriginReadBlockingTest, SniffForXML) {
  StringPiece xml_data("   \t \r \n     <?xml version=\"1.0\"?>\n <catalog");
  StringPiece non_xml_data("        var name=window.location;\nadfadf");
  StringPiece empty_data("");

  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForXML(xml_data));
  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForXML(non_xml_data));

  // Empty string should be indeterminate.
  EXPECT_EQ(SniffingResult::kMaybe,
            CrossOriginReadBlocking::SniffForXML(empty_data));
}

TEST(CrossOriginReadBlockingTest, SniffForJSON) {
  StringPiece json_data("\t\t\r\n   { \"name\" : \"chrome\", ");
  StringPiece json_corrupt_after_first_key(
      "\t\t\r\n   { \"name\" :^^^^!!@#\1\", ");
  StringPiece json_data2("{ \"key   \\\"  \"          \t\t\r\n:");
  StringPiece non_json_data0("\t\t\r\n   { name : \"chrome\", ");
  StringPiece non_json_data1("\t\t\r\n   foo({ \"name\" : \"chrome\", ");
  StringPiece empty_data("");

  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForJSON(json_data));
  EXPECT_EQ(SniffingResult::kYes, CrossOriginReadBlocking::SniffForJSON(
                                      json_corrupt_after_first_key));

  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForJSON(json_data2));

  // All prefixes prefixes of |json_data2| ought to be indeterminate.
  StringPiece almost_json = json_data2;
  while (!almost_json.empty()) {
    almost_json.remove_suffix(1);
    EXPECT_EQ(SniffingResult::kMaybe,
              CrossOriginReadBlocking::SniffForJSON(almost_json))
        << almost_json;
  }

  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForJSON(non_json_data0));
  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForJSON(non_json_data1));

  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForJSON(R"({"" : 1})"))
      << "Empty strings are accepted";
  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForJSON(R"({'' : 1})"))
      << "Single quotes are not accepted";
  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForJSON("{\"\\\"\" : 1}"))
      << "Escaped quotes are recognized";
  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForJSON(R"({"\\\u000a" : 1})"))
      << "Escaped control characters are recognized";
  EXPECT_EQ(SniffingResult::kMaybe,
            CrossOriginReadBlocking::SniffForJSON(R"({"\\\u00)"))
      << "Incomplete escape results in maybe";
  EXPECT_EQ(SniffingResult::kMaybe,
            CrossOriginReadBlocking::SniffForJSON("{\"\\"))
      << "Incomplete escape results in maybe";
  EXPECT_EQ(SniffingResult::kMaybe,
            CrossOriginReadBlocking::SniffForJSON("{\"\\\""))
      << "Incomplete escape results in maybe";
  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForJSON("{\"\n\" : true}"))
      << "Unescaped control characters are rejected";
  EXPECT_EQ(SniffingResult::kNo, CrossOriginReadBlocking::SniffForJSON("{}"))
      << "Empty dictionary is not recognized (since it's valid JS too)";
  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForJSON("[true, false, 1, 2]"))
      << "Lists dictionary are not recognized (since they're valid JS too)";
  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForJSON(R"({":"})"))
      << "A colon character inside a string does not trigger a match";
}

TEST(CrossOriginReadBlockingTest, GetCanonicalMimeType) {
  std::vector<std::pair<const char*, MimeType>> tests = {
      // Basic tests for things in the original implementation:
      {"text/html", MimeType::kHtml},
      {"text/xml", MimeType::kXml},
      {"application/rss+xml", MimeType::kXml},
      {"application/xml", MimeType::kXml},
      {"application/json", MimeType::kJson},
      {"text/json", MimeType::kJson},
      {"text/x-json", MimeType::kJson},
      {"text/plain", MimeType::kPlain},

      // Other mime types:
      {"application/foobar", MimeType::kOthers},

      // Regression tests for https://crbug.com/799155 (prefix/suffix matching):
      {"application/json+protobuf", MimeType::kJson},
      {"text/json+protobuf", MimeType::kJson},
      {"application/activity+json", MimeType::kJson},
      {"text/foobar+xml", MimeType::kXml},
      // No match without a '+' character:
      {"application/jsonfoobar", MimeType::kOthers},
      {"application/foobarjson", MimeType::kOthers},
      {"application/xmlfoobar", MimeType::kOthers},
      {"application/foobarxml", MimeType::kOthers},

      // Case-insensitive comparison:
      {"APPLICATION/JSON", MimeType::kJson},
      {"APPLICATION/JSON+PROTOBUF", MimeType::kJson},
      {"APPLICATION/ACTIVITY+JSON", MimeType::kJson},

      // Images are allowed cross-site, and SVG is an image, so we should
      // classify SVG as "other" instead of "xml" (even though it technically is
      // an xml document).
      {"image/svg+xml", MimeType::kOthers},

      // Javascript should not be blocked.
      {"application/javascript", MimeType::kOthers},
      {"application/jsonp", MimeType::kOthers},
  };

  for (const auto& test : tests) {
    const char* input = test.first;  // e.g. "text/html"
    MimeType expected = test.second;
    MimeType actual = CrossOriginReadBlocking::GetCanonicalMimeType(input);
    EXPECT_EQ(expected, actual)
        << "when testing with the following input: " << input;
  }
}

}  // namespace network
