// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/HTTPParsers.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/MathExtras.h"
#include "wtf/text/AtomicString.h"

namespace blink {

TEST(HTTPParsersTest, ParseCacheControl)
{
    CacheControlHeader header;

    header = parseCacheControlDirectives("no-cache", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_TRUE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives("no-cache no-store", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_TRUE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives("no-store must-revalidate", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_FALSE(header.containsNoCache);
    EXPECT_TRUE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives("max-age=0", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_FALSE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_EQ(0.0, header.maxAge);

    header = parseCacheControlDirectives("max-age", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_FALSE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives("max-age=0 no-cache", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_FALSE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_EQ(0.0, header.maxAge);

    header = parseCacheControlDirectives("no-cache=foo", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_FALSE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives("nonsense", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_FALSE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives("\rno-cache\n\t\v\0\b", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_TRUE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives("      no-cache       ", AtomicString());
    EXPECT_TRUE(header.parsed);
    EXPECT_TRUE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));

    header = parseCacheControlDirectives(AtomicString(), "no-cache");
    EXPECT_TRUE(header.parsed);
    EXPECT_TRUE(header.containsNoCache);
    EXPECT_FALSE(header.containsNoStore);
    EXPECT_FALSE(header.containsMustRevalidate);
    EXPECT_TRUE(std::isnan(header.maxAge));
}

TEST(HTTPParsersTest, CommaDelimitedHeaderSet)
{
    CommaDelimitedHeaderSet set1;
    CommaDelimitedHeaderSet set2;
    parseCommaDelimitedHeader("dpr, rw, whatever", set1);
    EXPECT_TRUE(set1.contains("dpr"));
    EXPECT_TRUE(set1.contains("rw"));
    EXPECT_TRUE(set1.contains("whatever"));
    parseCommaDelimitedHeader("dprw\t     , fo\to", set2);
    EXPECT_FALSE(set2.contains("dpr"));
    EXPECT_FALSE(set2.contains("rw"));
    EXPECT_FALSE(set2.contains("whatever"));
    EXPECT_TRUE(set2.contains("dprw"));
    EXPECT_FALSE(set2.contains("foo"));
    EXPECT_TRUE(set2.contains("fo\to"));
}

TEST(HTTPParsersTest, HTTPFieldContent)
{
    const UChar hiraganaA[2] = { 0x3042, 0 };

    EXPECT_TRUE(blink::isValidHTTPFieldContentRFC7230("\xd0\xa1"));
    EXPECT_TRUE(blink::isValidHTTPFieldContentRFC7230("t t"));
    EXPECT_TRUE(blink::isValidHTTPFieldContentRFC7230("t\tt"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(" "));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(""));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230("\x7f"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230("t\rt"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230("t\nt"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230("t\bt"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230("t\vt"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(" t"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230("t "));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(String("t\0t", 3)));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(String("\0", 1)));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(String("test \0, 6")));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(String("test ")));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230("test\r\n data"));
    EXPECT_FALSE(blink::isValidHTTPFieldContentRFC7230(String(hiraganaA)));
}

} // namespace blink
