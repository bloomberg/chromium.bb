// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/LayoutLocale.h"

#include "platform/Logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(LayoutLocaleTest, Get)
{
    LayoutLocale::clearForTesting();

    EXPECT_EQ(nullptr, LayoutLocale::get(nullAtom));

    EXPECT_EQ(emptyAtom, LayoutLocale::get(emptyAtom)->localeString());

    EXPECT_TRUE(equalIgnoringCase("en-us", LayoutLocale::get("en-us")->localeString()));
    EXPECT_TRUE(equalIgnoringCase("ja-jp", LayoutLocale::get("ja-jp")->localeString()));

    LayoutLocale::clearForTesting();
}

TEST(LayoutLocaleTest, GetCaseInsensitive)
{
    const LayoutLocale* enUS = LayoutLocale::get("en-us");
    EXPECT_EQ(enUS, LayoutLocale::get("en-US"));
}

} // namespace blink
