// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/trusted/plugin/nacl_http_response_headers.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

// Test that we are able to discover the cache validator headers.
TEST(NaClHttpResponseHeadersTest, TestGetValidators) {
  // Test a single (weak) ETag.
  std::string one_val_headers("Date: Wed, 15 Nov 1995 06:25:24 GMT\n"
                              "Server: Apache/2.0.52 (CentOS)\n"
                              "Content-Type: text/plain; charset=UTF-8\n"
                              "Connection: close\n"
                              "Accept-Ranges: bytes\n"
                              "ETag: w\"abcdefg\"\n"
                              "Content-Length: 2912652\n");
  std::string one_val_expected("etag:w\"abcdefg\"");
  plugin::NaClHttpResponseHeaders parser_1;
  parser_1.Parse(one_val_headers);
  EXPECT_EQ(one_val_expected, parser_1.GetCacheValidators());
  EXPECT_EQ(std::string("w\"abcdefg\""), parser_1.GetHeader("etag"));
  EXPECT_EQ(std::string(), parser_1.GetHeader("last-modified"));

  // Test a Last-Modified Header.
  std::string mod_val_headers("Date: Wed, 15 Nov 1995 06:25:24 GMT\n"
                              "Server: Apache/2.0.52 (CentOS)\n"
                              "Content-Type: text/plain; charset=UTF-8\n"
                              "Connection: close\n"
                              "Accept-Ranges: bytes\n"
                              "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\n"
                              "Content-Length: 2912652\n");
  std::string mod_val_expected("last-modified:Wed, 15 Nov 1995 04:58:08 GMT");
  plugin::NaClHttpResponseHeaders parser_1b;
  parser_1b.Parse(mod_val_headers);
  EXPECT_EQ(mod_val_expected, parser_1b.GetCacheValidators());
  EXPECT_EQ(std::string("Wed, 15 Nov 1995 04:58:08 GMT"),
            parser_1b.GetHeader("last-modified"));

  // Test both (strong) ETag and Last-Modified, with some whitespace.
  std::string two_val_headers("Date: Wed, 15 Nov 1995 06:25:24 GMT\n"
                              "Last-modified: Wed, 15 Nov 1995 04:58:08 GMT\n"
                              "Server: Apache/2.0.52 (CentOS)\n"
                              "etag  \t :\t   \"/abcdefg:A-Z0-9+/==\"\n"
                              "Content-Type: text/plain; charset=UTF-8\n"
                              "cache-control: no-cache\n"
                              "Connection: close\n"
                              "Accept-Ranges: bytes\n"
                              "Content-Length: 2912652\n");
  // Note that the value can still have white-space.
  std::string two_val_expected("etag:\"/abcdefg:A-Z0-9+/==\"&"
                               "last-modified:Wed, 15 Nov 1995 04:58:08 GMT");
  plugin::NaClHttpResponseHeaders parser_2;
  parser_2.Parse(two_val_headers);
  EXPECT_EQ(two_val_expected, parser_2.GetCacheValidators());
  EXPECT_EQ(std::string("\"/abcdefg:A-Z0-9+/==\""),
            parser_2.GetHeader("etag"));

  // Some etag generators like python HTTP server use ' instead of "
  std::string single_q_headers("Date: Wed, 15 Nov 1995 06:25:24 GMT\n"
                               "Server: BaseHTTP/0.3 Python/2.7.3\n"
                               "ETag: '/usr/local/some_file.nmf'\n");
  std::string single_q_expected("etag:'/usr/local/some_file.nmf'");
  plugin::NaClHttpResponseHeaders parser_3;
  parser_3.Parse(single_q_headers);
  EXPECT_EQ(single_q_expected, parser_3.GetCacheValidators());
  EXPECT_EQ(std::string("'/usr/local/some_file.nmf'"),
            parser_3.GetHeader("etag"));

  // Keys w/ leading whitespace are invalid.
  // See: HttpResponseHeadersTest.NormalizeHeadersLeadingWhitespace.
  std::string bad_headers("Date: Wed, 15 Nov 1995 06:25:24 GMT\n"
                          "Server: BaseHTTP/0.3 Python/2.7.3\n"
                          "   ETag: '/usr/local/some_file.nmf'\n");
  std::string bad_expected("");
  plugin::NaClHttpResponseHeaders parser_4;
  parser_4.Parse(bad_headers);
  EXPECT_EQ(bad_expected, parser_4.GetCacheValidators());
  EXPECT_EQ(bad_expected, parser_4.GetHeader("etag"));
}

// Test that we are able to determine when there is a no-store
// Cache-Control header, among all the Cache-Control headers.
TEST(NaClHttpResponseHeadersTest, TestFindNoStore) {
  // Say that there isn't one, when there isn't one.
  std::string headers_0("Date: Wed, 15 Nov 1995 06:25:24 GMT\n"
                        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\n"
                        "ETag: '/tmp/blah.nmf'\n"
                        "Cache-Control: max-age=3600\n");
  plugin::NaClHttpResponseHeaders parser_0;
  parser_0.Parse(headers_0);
  EXPECT_FALSE(parser_0.CacheControlNoStore());

  // Say that there is one, when there is one.
  std::string headers_1("Date: Wed, 15 Nov 1995 06:25:24 GMT\n"
                        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\n"
                        "ETag: \"/abcdefgA-Z0-9+/\"\n"
                        "Cache-Control: no-store\n");
  plugin::NaClHttpResponseHeaders parser_1;
  parser_1.Parse(headers_1);
  EXPECT_TRUE(parser_1.CacheControlNoStore());

  // Say that there is one, when comma separated.
  std::string headers_2("Date: Wed, 15 Nov 1995 06:25:24 GMT\n"
                        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\n"
                        "ETag: \"/abcdefgA-Z0-9+/\"\n"
                        "Cache-Control: no-store, no-cache\n");
  plugin::NaClHttpResponseHeaders parser_2;
  parser_2.Parse(headers_2);
  EXPECT_TRUE(parser_2.CacheControlNoStore());

  // Comma separated, in a different position.
  std::string headers_3("Date: Wed, 15 Nov 1995 06:25:24 GMT\n"
                        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\n"
                        "ETag: \"/abcdefgA-Z0-9+/\"\n"
                        "Cache-control: no-cache, max-age=60, no-store\n");
  plugin::NaClHttpResponseHeaders parser_3;
  parser_3.Parse(headers_3);
  EXPECT_TRUE(parser_3.CacheControlNoStore());

  // Test multiple cache-control lines, plus extra space before colon.
  std::string headers_4("Date: Wed, 15 Nov 1995 06:25:24 GMT\n"
                        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\n"
                        "ETag: \"/abcdefgA-Z0-9+/\"\n"
                        "cache-control: no-cache\n"
                        "cache-control \t : max-age=60, no-store, max-stale\n");
  plugin::NaClHttpResponseHeaders parser_4;
  parser_4.Parse(headers_4);
  EXPECT_TRUE(parser_4.CacheControlNoStore());

  // Test with extra whitespace, in the values.
  std::string headers_5("Date:   Wed, 15 Nov 1995 06:25:24 GMT  \n"
                        "Last-Modified:   Wed, 15 Nov 1995 04:58:08 GMT  \n"
                        "ETag:   \"/abcdefgA-Z0-9+/\"  \n"
                        ": empty  key \n"
                        ": empty  key2 \n"
                        "Blank-Header :   \n"
                        "Connection: close\n"
                        "cache-control:max-age=60,  no-store  \n"
                        "cache-control: no-cache\n");
  plugin::NaClHttpResponseHeaders parser_5;
  parser_5.Parse(headers_5);
  EXPECT_TRUE(parser_5.CacheControlNoStore());
}
