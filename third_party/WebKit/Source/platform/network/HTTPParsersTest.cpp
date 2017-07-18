// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/HTTPParsers.h"

#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/weborigin/Suborigin.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/dtoa/utils.h"
#include "platform/wtf/text/AtomicString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(HTTPParsersTest, ParseCacheControl) {
  CacheControlHeader header;

  header = ParseCacheControlDirectives("no-cache", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));

  header = ParseCacheControlDirectives("no-cache no-store", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));

  header =
      ParseCacheControlDirectives("no-store must-revalidate", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_TRUE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));

  header = ParseCacheControlDirectives("max-age=0", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(0.0, header.max_age);

  header = ParseCacheControlDirectives("max-age", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));

  header = ParseCacheControlDirectives("max-age=0 no-cache", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_EQ(0.0, header.max_age);

  header = ParseCacheControlDirectives("no-cache=foo", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));

  header = ParseCacheControlDirectives("nonsense", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_FALSE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));

  header = ParseCacheControlDirectives("\rno-cache\n\t\v\0\b", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));

  header = ParseCacheControlDirectives("      no-cache       ", AtomicString());
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));

  header = ParseCacheControlDirectives(AtomicString(), "no-cache");
  EXPECT_TRUE(header.parsed);
  EXPECT_TRUE(header.contains_no_cache);
  EXPECT_FALSE(header.contains_no_store);
  EXPECT_FALSE(header.contains_must_revalidate);
  EXPECT_TRUE(std::isnan(header.max_age));
}

TEST(HTTPParsersTest, CommaDelimitedHeaderSet) {
  CommaDelimitedHeaderSet set1;
  CommaDelimitedHeaderSet set2;
  ParseCommaDelimitedHeader("dpr, rw, whatever", set1);
  EXPECT_TRUE(set1.Contains("dpr"));
  EXPECT_TRUE(set1.Contains("rw"));
  EXPECT_TRUE(set1.Contains("whatever"));
  ParseCommaDelimitedHeader("dprw\t     , fo\to", set2);
  EXPECT_FALSE(set2.Contains("dpr"));
  EXPECT_FALSE(set2.Contains("rw"));
  EXPECT_FALSE(set2.Contains("whatever"));
  EXPECT_TRUE(set2.Contains("dprw"));
  EXPECT_FALSE(set2.Contains("foo"));
  EXPECT_TRUE(set2.Contains("fo\to"));
}

TEST(HTTPParsersTest, HTTPToken) {
  const UChar kHiraganaA[2] = {0x3042, 0};
  const UChar kLatinCapitalAWithMacron[2] = {0x100, 0};

  EXPECT_TRUE(blink::IsValidHTTPToken("gzip"));
  EXPECT_TRUE(blink::IsValidHTTPToken("no-cache"));
  EXPECT_TRUE(blink::IsValidHTTPToken("86400"));
  EXPECT_TRUE(blink::IsValidHTTPToken("~"));
  EXPECT_FALSE(blink::IsValidHTTPToken(""));
  EXPECT_FALSE(blink::IsValidHTTPToken(" "));
  EXPECT_FALSE(blink::IsValidHTTPToken("\t"));
  EXPECT_FALSE(blink::IsValidHTTPToken("\x7f"));
  EXPECT_FALSE(blink::IsValidHTTPToken("\xff"));
  EXPECT_FALSE(blink::IsValidHTTPToken(String(kLatinCapitalAWithMacron)));
  EXPECT_FALSE(blink::IsValidHTTPToken("t a"));
  EXPECT_FALSE(blink::IsValidHTTPToken("()"));
  EXPECT_FALSE(blink::IsValidHTTPToken("(foobar)"));
  EXPECT_FALSE(blink::IsValidHTTPToken(String("\0", 1)));
  EXPECT_FALSE(blink::IsValidHTTPToken(String(kHiraganaA)));
}

TEST(HTTPParsersTest, ExtractMIMETypeFromMediaType) {
  const AtomicString text_html("text/html");

  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(AtomicString("text/html")));
  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(
                           AtomicString("text/html; charset=iso-8859-1")));

  // Quoted charset parameter
  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(
                           AtomicString("text/html; charset=\"quoted\"")));

  // Multiple parameters
  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(
                           AtomicString("text/html; charset=x; foo=bar")));

  // OWSes are trimmed.
  EXPECT_EQ(text_html,
            ExtractMIMETypeFromMediaType(AtomicString(" text/html   ")));
  EXPECT_EQ(text_html,
            ExtractMIMETypeFromMediaType(AtomicString("\ttext/html \t")));
  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(
                           AtomicString("text/html ; charset=iso-8859-1")));

  // Non-standard multiple type/subtype listing using a comma as a separator
  // is accepted.
  EXPECT_EQ(text_html,
            ExtractMIMETypeFromMediaType(AtomicString("text/html,text/plain")));
  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(
                           AtomicString("text/html , text/plain")));
  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(
                           AtomicString("text/html\t,\ttext/plain")));
  EXPECT_EQ(text_html, ExtractMIMETypeFromMediaType(AtomicString(
                           "text/html,text/plain;charset=iso-8859-1")));

  // Preserves case.
  EXPECT_EQ("tExt/hTMl",
            ExtractMIMETypeFromMediaType(AtomicString("tExt/hTMl")));

  EXPECT_EQ(g_empty_string,
            ExtractMIMETypeFromMediaType(AtomicString(", text/html")));
  EXPECT_EQ(g_empty_string,
            ExtractMIMETypeFromMediaType(AtomicString("; text/html")));

  // If no normalization is required, the same AtomicString should be returned.
  const AtomicString& passthrough = ExtractMIMETypeFromMediaType(text_html);
  EXPECT_EQ(text_html.Impl(), passthrough.Impl());
}

TEST(HTTPParsersTest, ExtractMIMETypeFromMediaTypeInvalidInput) {
  // extractMIMETypeFromMediaType() returns the string before the first
  // semicolon after trimming OWSes at the head and the tail even if the
  // string doesn't conform to the media-type ABNF defined in the RFC 7231.

  // These behaviors could be fixed later when ready.

  // Non-OWS characters meaning space are not trimmed.
  EXPECT_EQ(AtomicString("\r\ntext/html\r\n"),
            ExtractMIMETypeFromMediaType(AtomicString("\r\ntext/html\r\n")));
  // U+2003, EM SPACE (UTF-8: E2 80 83).
  EXPECT_EQ(AtomicString::FromUTF8("\xE2\x80\x83text/html"),
            ExtractMIMETypeFromMediaType(
                AtomicString::FromUTF8("\xE2\x80\x83text/html")));

  // Invalid type/subtype.
  EXPECT_EQ(AtomicString("a"), ExtractMIMETypeFromMediaType(AtomicString("a")));

  // Invalid parameters.
  EXPECT_EQ(AtomicString("text/html"),
            ExtractMIMETypeFromMediaType(AtomicString("text/html;wow")));
  EXPECT_EQ(AtomicString("text/html"),
            ExtractMIMETypeFromMediaType(AtomicString("text/html;;;;;;")));
  EXPECT_EQ(AtomicString("text/html"),
            ExtractMIMETypeFromMediaType(AtomicString("text/html; = = = ")));

  // Only OWSes at either the beginning or the end of the type/subtype
  // portion.
  EXPECT_EQ(AtomicString("text / html"),
            ExtractMIMETypeFromMediaType(AtomicString("text / html")));
  EXPECT_EQ(AtomicString("t e x t / h t m l"),
            ExtractMIMETypeFromMediaType(AtomicString("t e x t / h t m l")));

  EXPECT_EQ(AtomicString("text\r\n/\nhtml"),
            ExtractMIMETypeFromMediaType(AtomicString("text\r\n/\nhtml")));
  EXPECT_EQ(AtomicString("text\n/\nhtml"),
            ExtractMIMETypeFromMediaType(AtomicString("text\n/\nhtml")));
  EXPECT_EQ(AtomicString::FromUTF8("text\xE2\x80\x83/html"),
            ExtractMIMETypeFromMediaType(
                AtomicString::FromUTF8("text\xE2\x80\x83/html")));
}

void ExpectParseNamePass(const char* message,
                         String header,
                         String expected_name) {
  SCOPED_TRACE(message);

  Vector<String> messages;
  Suborigin suborigin;
  EXPECT_TRUE(ParseSuboriginHeader(header, &suborigin, messages));
  EXPECT_EQ(expected_name, suborigin.GetName());
}

void ExpectParseNameFail(const char* message, String header) {
  SCOPED_TRACE(message);

  Vector<String> messages;
  Suborigin suborigin;
  EXPECT_FALSE(ParseSuboriginHeader(header, &suborigin, messages));
  EXPECT_EQ(String(), suborigin.GetName());
}

void ExpectParsePolicyPass(
    const char* message,
    String header,
    const Suborigin::SuboriginPolicyOptions expected_policy[],
    size_t num_policies) {
  SCOPED_TRACE(message);

  Vector<String> messages;
  Suborigin suborigin;
  EXPECT_TRUE(ParseSuboriginHeader(header, &suborigin, messages));
  unsigned policies_mask = 0;
  for (size_t i = 0; i < num_policies; i++)
    policies_mask |= static_cast<unsigned>(expected_policy[i]);
  EXPECT_EQ(policies_mask, suborigin.OptionsMask());
}

void ExpectParsePolicyFail(const char* message, String header) {
  SCOPED_TRACE(message);

  Vector<String> messages;
  Suborigin suborigin;
  EXPECT_FALSE(ParseSuboriginHeader(header, &suborigin, messages));
  EXPECT_EQ(String(), suborigin.GetName());
}

TEST(HTTPParsersTest, SuboriginParseValidNames) {
  // Single headers
  ExpectParseNamePass("Alpha", "foo", "foo");
  ExpectParseNamePass("Whitespace alpha", "  foo  ", "foo");
  ExpectParseNamePass("Alphanumeric", "f0o", "f0o");
  ExpectParseNamePass("Numeric at end", "foo42", "foo42");

  // Mulitple headers should only give the first name
  ExpectParseNamePass("Multiple headers, no whitespace", "foo,bar", "foo");
  ExpectParseNamePass("Multiple headers, whitespace before second", "foo, bar",
                      "foo");
  ExpectParseNamePass(
      "Multiple headers, whitespace after first and before second", "foo, bar",
      "foo");
  ExpectParseNamePass("Multiple headers, empty second ignored", "foo, bar",
                      "foo");
  ExpectParseNamePass("Multiple headers, invalid second ignored", "foo, bar",
                      "foo");
}

TEST(HTTPParsersTest, SuboriginParseInvalidNames) {
  // Single header, invalid value
  ExpectParseNameFail("Empty header", "");
  ExpectParseNameFail("Numeric", "42");
  ExpectParseNameFail("Hyphen middle", "foo-bar");
  ExpectParseNameFail("Hyphen start", "-foobar");
  ExpectParseNameFail("Hyphen end", "foobar-");
  ExpectParseNameFail("Whitespace in middle", "foo bar");
  ExpectParseNameFail("Invalid character at end of name", "foobar'");
  ExpectParseNameFail("Invalid character at start of name", "'foobar");
  ExpectParseNameFail("Invalid character in middle of name", "foo'bar");
  ExpectParseNameFail("Alternate invalid character in middle of name",
                      "foob@r");
  ExpectParseNameFail("First cap", "Foo");
  ExpectParseNameFail("All cap", "FOO");

  // Multiple headers, invalid value(s)
  ExpectParseNameFail("Multple headers, empty first header", ", bar");
  ExpectParseNameFail("Multple headers, both empty headers", ",");
  ExpectParseNameFail("Multple headers, invalid character in first header",
                      "f@oo, bar");
  ExpectParseNameFail("Multple headers, invalid character in both headers",
                      "f@oo, b@r");
}

TEST(HTTPParsersTest, SuboriginParseValidPolicy) {
  const Suborigin::SuboriginPolicyOptions kUnsafePostmessageSend[] = {
      Suborigin::SuboriginPolicyOptions::kUnsafePostMessageSend};
  const Suborigin::SuboriginPolicyOptions kUnsafePostmessageReceive[] = {
      Suborigin::SuboriginPolicyOptions::kUnsafePostMessageReceive};
  const Suborigin::SuboriginPolicyOptions kUnsafePostmessageSendAndReceive[] = {
      Suborigin::SuboriginPolicyOptions::kUnsafePostMessageSend,
      Suborigin::SuboriginPolicyOptions::kUnsafePostMessageReceive};
  const Suborigin::SuboriginPolicyOptions kUnsafeCookies[] = {
      Suborigin::SuboriginPolicyOptions::kUnsafeCookies};
  const Suborigin::SuboriginPolicyOptions kUnsafeCredentials[] = {
      Suborigin::SuboriginPolicyOptions::kUnsafeCredentials};
  const Suborigin::SuboriginPolicyOptions kAllOptions[] = {
      Suborigin::SuboriginPolicyOptions::kUnsafePostMessageSend,
      Suborigin::SuboriginPolicyOptions::kUnsafePostMessageReceive,
      Suborigin::SuboriginPolicyOptions::kUnsafeCookies,
      Suborigin::SuboriginPolicyOptions::kUnsafeCredentials};

  // All simple, valid policies
  ExpectParsePolicyPass(
      "One policy, unsafe-postmessage-send", "foobar 'unsafe-postmessage-send'",
      kUnsafePostmessageSend, ARRAY_SIZE(kUnsafePostmessageSend));
  ExpectParsePolicyPass("One policy, unsafe-postmessage-receive",
                        "foobar 'unsafe-postmessage-receive'",
                        kUnsafePostmessageReceive,
                        ARRAY_SIZE(kUnsafePostmessageReceive));
  ExpectParsePolicyPass("One policy, unsafe-cookies", "foobar 'unsafe-cookies'",
                        kUnsafeCookies, ARRAY_SIZE(kUnsafeCookies));
  ExpectParsePolicyPass("One policy, unsafe-credentials",
                        "foobar 'unsafe-credentials'", kUnsafeCredentials,
                        ARRAY_SIZE(kUnsafeCredentials));

  // Formatting differences of policies and multiple policies
  ExpectParsePolicyPass("One policy, whitespace all around",
                        "foobar      'unsafe-postmessage-send'          ",
                        kUnsafePostmessageSend,
                        ARRAY_SIZE(kUnsafePostmessageSend));
  ExpectParsePolicyPass(
      "Multiple, same policies",
      "foobar 'unsafe-postmessage-send' 'unsafe-postmessage-send'",
      kUnsafePostmessageSend, ARRAY_SIZE(kUnsafePostmessageSend));
  ExpectParsePolicyPass(
      "Multiple, different policies",
      "foobar 'unsafe-postmessage-send' 'unsafe-postmessage-receive'",
      kUnsafePostmessageSendAndReceive,
      ARRAY_SIZE(kUnsafePostmessageSendAndReceive));
  ExpectParsePolicyPass("Many different policies",
                        "foobar 'unsafe-postmessage-send' "
                        "'unsafe-postmessage-receive' 'unsafe-cookies' "
                        "'unsafe-credentials'",
                        kAllOptions, ARRAY_SIZE(kAllOptions));
  ExpectParsePolicyPass("One policy, unknown option", "foobar 'unknown-option'",
                        {}, 0);
}

TEST(HTTPParsersTest, SuboriginParseInvalidPolicy) {
  ExpectParsePolicyFail("One policy, no suborigin name",
                        "'unsafe-postmessage-send'");
  ExpectParsePolicyFail("One policy, invalid characters",
                        "foobar 'un$afe-postmessage-send'");
  ExpectParsePolicyFail("One policy, caps", "foobar 'UNSAFE-POSTMESSAGE-SEND'");
  ExpectParsePolicyFail("One policy, missing first quote",
                        "foobar unsafe-postmessage-send'");
  ExpectParsePolicyFail("One policy, missing last quote",
                        "foobar 'unsafe-postmessage-send");
  ExpectParsePolicyFail("One policy, invalid character at end",
                        "foobar 'unsafe-postmessage-send';");
  ExpectParsePolicyFail(
      "Multiple policies, extra character between options",
      "foobar 'unsafe-postmessage-send' ; 'unsafe-postmessage-send'");
  ExpectParsePolicyFail("Policy that is a single quote", "foobar '");
  ExpectParsePolicyFail("Valid policy and then policy that is a single quote",
                        "foobar 'unsafe-postmessage-send' '");
}

TEST(HTTPParsersTest, ParseHTTPRefresh) {
  double delay;
  String url;
  EXPECT_FALSE(ParseHTTPRefresh("", nullptr, delay, url));
  EXPECT_FALSE(ParseHTTPRefresh(" ", nullptr, delay, url));
  EXPECT_FALSE(ParseHTTPRefresh("1.3xyz url=foo", nullptr, delay, url));
  EXPECT_FALSE(ParseHTTPRefresh("1.3.4xyz url=foo", nullptr, delay, url));
  EXPECT_FALSE(ParseHTTPRefresh("1e1 url=foo", nullptr, delay, url));

  EXPECT_TRUE(ParseHTTPRefresh("123 ", nullptr, delay, url));
  EXPECT_EQ(123.0, delay);
  EXPECT_TRUE(url.IsEmpty());

  EXPECT_TRUE(ParseHTTPRefresh("1 ; url=dest", nullptr, delay, url));
  EXPECT_EQ(1.0, delay);
  EXPECT_EQ("dest", url);
  EXPECT_TRUE(
      ParseHTTPRefresh("1 ;\nurl=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(1.0, delay);
  EXPECT_EQ("dest", url);
  EXPECT_TRUE(ParseHTTPRefresh("1 ;\nurl=dest", nullptr, delay, url));
  EXPECT_EQ(1.0, delay);
  EXPECT_EQ("url=dest", url);

  EXPECT_TRUE(ParseHTTPRefresh("1 url=dest", nullptr, delay, url));
  EXPECT_EQ(1.0, delay);
  EXPECT_EQ("dest", url);

  EXPECT_TRUE(
      ParseHTTPRefresh("10\nurl=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(10, delay);
  EXPECT_EQ("dest", url);

  EXPECT_TRUE(
      ParseHTTPRefresh("1.5; url=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(1.5, delay);
  EXPECT_EQ("dest", url);
  EXPECT_TRUE(
      ParseHTTPRefresh("1.5.9; url=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(1.5, delay);
  EXPECT_EQ("dest", url);
  EXPECT_TRUE(
      ParseHTTPRefresh("7..; url=dest", IsASCIISpace<UChar>, delay, url));
  EXPECT_EQ(7, delay);
  EXPECT_EQ("dest", url);
}

TEST(HTTPParsersTest, ParseMultipartHeadersResult) {
  struct {
    const char* data;
    const bool result;
    const size_t end;
  } tests[] = {
      {"This is junk", false, 0},
      {"Foo: bar\nBaz:\n\nAfter:\n", true, 15},
      {"Foo: bar\nBaz:\n", false, 0},
      {"Foo: bar\r\nBaz:\r\n\r\nAfter:\r\n", true, 18},
      {"Foo: bar\r\nBaz:\r\n", false, 0},
      {"Foo: bar\nBaz:\r\n\r\nAfter:\n\n", true, 17},
      {"Foo: bar\r\nBaz:\n", false, 0},
      {"\r\n", true, 2},
  };
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(tests); ++i) {
    ResourceResponse response;
    size_t end = 0;
    bool result = ParseMultipartHeadersFromBody(
        tests[i].data, strlen(tests[i].data), &response, &end);
    EXPECT_EQ(tests[i].result, result);
    EXPECT_EQ(tests[i].end, end);
  }
}

TEST(HTTPParsersTest, ParseMultipartHeaders) {
  ResourceResponse response;
  response.AddHTTPHeaderField("foo", "bar");
  response.AddHTTPHeaderField("range", "piyo");
  response.AddHTTPHeaderField("content-length", "999");

  const char kData[] = "content-type: image/png\ncontent-length: 10\n\n";
  size_t end = 0;
  bool result =
      ParseMultipartHeadersFromBody(kData, strlen(kData), &response, &end);

  EXPECT_TRUE(result);
  EXPECT_EQ(strlen(kData), end);
  EXPECT_EQ("image/png", response.HttpHeaderField("content-type"));
  EXPECT_EQ("10", response.HttpHeaderField("content-length"));
  EXPECT_EQ("bar", response.HttpHeaderField("foo"));
  EXPECT_EQ(AtomicString(), response.HttpHeaderField("range"));
}

TEST(HTTPParsersTest, ParseMultipartHeadersContentCharset) {
  ResourceResponse response;
  const char kData[] = "content-type: text/html; charset=utf-8\n\n";
  size_t end = 0;
  bool result =
      ParseMultipartHeadersFromBody(kData, strlen(kData), &response, &end);

  EXPECT_TRUE(result);
  EXPECT_EQ(strlen(kData), end);
  EXPECT_EQ("text/html; charset=utf-8",
            response.HttpHeaderField("content-type"));
  EXPECT_EQ("utf-8", response.TextEncodingName());
}

void testServerTimingHeader(const char* headerValue,
                            Vector<Vector<String>> expectedResults) {
  std::unique_ptr<ServerTimingHeaderVector> results =
      ParseServerTimingHeader(headerValue);
  EXPECT_EQ((*results).size(), expectedResults.size());
  unsigned i = 0;
  for (const auto& header : *results) {
    Vector<String> expectedResult = expectedResults[i++];
    EXPECT_EQ(header->metric, expectedResult[0]);
    EXPECT_EQ(header->value, expectedResult[1].ToDouble());
    EXPECT_EQ(header->description, expectedResult[2]);
  }
}

TEST(HTTPParsersTest, ParseServerTimingHeader) {
  testServerTimingHeader("", {});
  testServerTimingHeader("metric", {{"metric", "0", ""}});
  testServerTimingHeader("metric,", {{"metric", "0", ""}});
  testServerTimingHeader("metric ,", {{"metric", "0", ""}});
  testServerTimingHeader("metric;", {{"metric", "0", ""}});
  testServerTimingHeader("metric;,", {{"metric", "0", ""}});
  testServerTimingHeader("metric; ,", {{"metric", "0", ""}});
  testServerTimingHeader("metric ;", {{"metric", "0", ""}});
  testServerTimingHeader("metric ;,", {{"metric", "0", ""}});
  testServerTimingHeader("metric ; ,", {{"metric", "0", ""}});
  testServerTimingHeader("metric;description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric;description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric;description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric ;description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric ;description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric ;description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric; description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric; description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric; description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric ; description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric ; description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric ; description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric=", {{"metric", "0", ""}});
  testServerTimingHeader("metric=,", {{"metric", "0", ""}});
  testServerTimingHeader("metric= ,", {{"metric", "0", ""}});
  testServerTimingHeader("metric=;", {{"metric", "0", ""}});
  testServerTimingHeader("metric=;,", {{"metric", "0", ""}});
  testServerTimingHeader("metric=; ,", {{"metric", "0", ""}});
  testServerTimingHeader("metric= ;", {{"metric", "0", ""}});
  testServerTimingHeader("metric= ;,", {{"metric", "0", ""}});
  testServerTimingHeader("metric= ; ,", {{"metric", "0", ""}});
  testServerTimingHeader("metric=;description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric=;description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric=;description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric= ;description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric= ;description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric= ;description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric=; description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric=; description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric=; description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric= ; description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric= ; description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric= ; description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric =", {{"metric", "0", ""}});
  testServerTimingHeader("metric =,", {{"metric", "0", ""}});
  testServerTimingHeader("metric = ,", {{"metric", "0", ""}});
  testServerTimingHeader("metric =;", {{"metric", "0", ""}});
  testServerTimingHeader("metric =;,", {{"metric", "0", ""}});
  testServerTimingHeader("metric =; ,", {{"metric", "0", ""}});
  testServerTimingHeader("metric = ;", {{"metric", "0", ""}});
  testServerTimingHeader("metric = ;,", {{"metric", "0", ""}});
  testServerTimingHeader("metric = ; ,", {{"metric", "0", ""}});
  testServerTimingHeader("metric =;description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric =;description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric =;description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric = ;description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric = ;description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric = ;description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric =; description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric =; description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric =; description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric = ; description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric = ; description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric = ; description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric=123.4", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric=123.4,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric=123.4 ,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric=123.4;", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric=123.4;,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric=123.4; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric=123.4 ;", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric=123.4 ;,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric=123.4 ; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric=123.4;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric=123.4;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric=123.4;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric=123.4 ;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric=123.4 ;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric=123.4 ;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric=123.4; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric=123.4; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric=123.4; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric=123.4 ; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric=123.4 ; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric=123.4 ; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric =123.4", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric =123.4,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric =123.4 ,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric =123.4;", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric =123.4;,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric =123.4; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric =123.4 ;", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric =123.4 ;,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric =123.4 ; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric =123.4;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric =123.4;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric =123.4;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric =123.4 ;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric =123.4 ;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric =123.4 ;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric =123.4; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric =123.4; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric =123.4; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric =123.4 ; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric =123.4 ; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric =123.4 ; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric= 123.4", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric= 123.4,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric= 123.4 ,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric= 123.4;", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric= 123.4;,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric= 123.4; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric= 123.4 ;", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric= 123.4 ;,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric= 123.4 ; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric= 123.4;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric= 123.4;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric= 123.4;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric= 123.4 ;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric= 123.4 ;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric= 123.4 ;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric= 123.4; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric= 123.4; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric= 123.4; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric= 123.4 ; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric= 123.4 ; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric= 123.4 ; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric = 123.4", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric = 123.4,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric = 123.4 ,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric = 123.4;", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric = 123.4;,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric = 123.4; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric = 123.4 ;", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric = 123.4 ;,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric = 123.4 ; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric = 123.4;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric = 123.4;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric = 123.4;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric = 123.4 ;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric = 123.4 ;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric = 123.4 ;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric = 123.4; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric = 123.4; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric = 123.4; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric = 123.4 ; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric = 123.4 ; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric = 123.4 ; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric", {{"metric", "0", ""}});
  testServerTimingHeader(" metric,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric ,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric;", {{"metric", "0", ""}});
  testServerTimingHeader(" metric;,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric; ,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric ;", {{"metric", "0", ""}});
  testServerTimingHeader(" metric ;,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric ; ,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric;description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric;description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric;description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric ;description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric ;description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric ;description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric; description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric; description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric; description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric ; description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric ; description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric ; description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric=", {{"metric", "0", ""}});
  testServerTimingHeader(" metric=,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric= ,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric=;", {{"metric", "0", ""}});
  testServerTimingHeader(" metric=;,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric=; ,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric= ;", {{"metric", "0", ""}});
  testServerTimingHeader(" metric= ;,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric= ; ,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric=;description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric=;description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric=;description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric= ;description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric= ;description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric= ;description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric=; description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric=; description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric=; description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric= ; description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric= ; description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric= ; description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric =", {{"metric", "0", ""}});
  testServerTimingHeader(" metric =,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric = ,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric =;", {{"metric", "0", ""}});
  testServerTimingHeader(" metric =;,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric =; ,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric = ;", {{"metric", "0", ""}});
  testServerTimingHeader(" metric = ;,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric = ; ,", {{"metric", "0", ""}});
  testServerTimingHeader(" metric =;description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric =;description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric =;description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric = ;description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric = ;description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric = ;description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric =; description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric =; description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric =; description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric = ; description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric = ; description,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric = ; description ,",
                         {{"metric", "0", "description"}});
  testServerTimingHeader(" metric=123.4", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric=123.4,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric=123.4 ,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric=123.4;", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric=123.4;,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric=123.4; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric=123.4 ;", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric=123.4 ;,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric=123.4 ; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric=123.4;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric=123.4;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric=123.4;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric=123.4 ;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric=123.4 ;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric=123.4 ;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric=123.4; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric=123.4; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric=123.4; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric=123.4 ; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric=123.4 ; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric=123.4 ; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric =123.4", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric =123.4,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric =123.4 ,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric =123.4;", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric =123.4;,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric =123.4; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric =123.4 ;", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric =123.4 ;,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric =123.4 ; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric =123.4;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric =123.4;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric =123.4;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric =123.4 ;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric =123.4 ;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric =123.4 ;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric =123.4; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric =123.4; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric =123.4; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric =123.4 ; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric =123.4 ; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric =123.4 ; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric= 123.4", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric= 123.4,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric= 123.4 ,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric= 123.4;", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric= 123.4;,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric= 123.4; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric= 123.4 ;", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric= 123.4 ;,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric= 123.4 ; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric= 123.4;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric= 123.4;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric= 123.4;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric= 123.4 ;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric= 123.4 ;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric= 123.4 ;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric= 123.4; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric= 123.4; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric= 123.4; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric= 123.4 ; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric= 123.4 ; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric= 123.4 ; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric = 123.4", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric = 123.4,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric = 123.4 ,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric = 123.4;", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric = 123.4;,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric = 123.4; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric = 123.4 ;", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric = 123.4 ;,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric = 123.4 ; ,", {{"metric", "123.4", ""}});
  testServerTimingHeader(" metric = 123.4;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric = 123.4;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric = 123.4;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric = 123.4 ;description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric = 123.4 ;description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric = 123.4 ;description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric = 123.4; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric = 123.4; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric = 123.4; description ,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric = 123.4 ; description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric = 123.4 ; description,",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader(" metric = 123.4 ; description ,",
                         {{"metric", "123.4", "description"}});

  testServerTimingHeader(
      "metric1=12.3;description1,metric2=45.6;description2,metric3=78.9;"
      "description3",
      {{"metric1", "12.3", "description1"},
       {"metric2", "45.6", "description2"},
       {"metric3", "78.9", "description3"}});

  // quoted-string
  testServerTimingHeader("metric;\"\"", {{"metric", "0", ""}});
  testServerTimingHeader("metric;\"\"\"", {{"metric", "0", ""}});
  testServerTimingHeader("metric;\"\\\"\"", {{"metric", "0", "\""}});
  testServerTimingHeader("metric;\"\\\\\"\"", {{"metric", "0", "\\"}});
  testServerTimingHeader("metric;\"\\\\\\\"\"", {{"metric", "0", "\\\""}});
}

}  // namespace blink
