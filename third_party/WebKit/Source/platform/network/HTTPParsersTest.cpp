// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "HTTPParsers.h"

#include "wtf/MathExtras.h"
#include "wtf/text/AtomicString.h"

#include <gtest/gtest.h>

namespace WebCore {

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

} // namespace WebCore

