// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/win/FontFallbackWin.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(FontFallbackWinTest, scriptCodeForUnifiedHanFromLocaleTest)
{
    EXPECT_EQ(USCRIPT_HIRAGANA, scriptCodeForUnifiedHanFromLocale(
        icu::Locale("ja", "JP")));
    EXPECT_EQ(USCRIPT_HANGUL, scriptCodeForUnifiedHanFromLocale(
        icu::Locale("ko", "KR")));
    EXPECT_EQ(USCRIPT_TRADITIONAL_HAN, scriptCodeForUnifiedHanFromLocale(
        icu::Locale("zh", "TW")));
    EXPECT_EQ(USCRIPT_SIMPLIFIED_HAN, scriptCodeForUnifiedHanFromLocale(
        icu::Locale("zh", "CN")));

    EXPECT_EQ(USCRIPT_TRADITIONAL_HAN, scriptCodeForUnifiedHanFromLocale(
        icu::Locale("zh", "HK")));

    // icu::Locale::getDefault() returns other combinations if, for instnace,
    // English Windows with the display language set to Japanese.
    EXPECT_EQ(USCRIPT_HIRAGANA, scriptCodeForUnifiedHanFromLocale(
        icu::Locale("ja")));
    EXPECT_EQ(USCRIPT_HIRAGANA, scriptCodeForUnifiedHanFromLocale(
        icu::Locale("ja", "US")));
    EXPECT_EQ(USCRIPT_HANGUL, scriptCodeForUnifiedHanFromLocale(
        icu::Locale("ko")));
    EXPECT_EQ(USCRIPT_HANGUL, scriptCodeForUnifiedHanFromLocale(
        icu::Locale("ko", "US")));
    EXPECT_EQ(USCRIPT_TRADITIONAL_HAN, scriptCodeForUnifiedHanFromLocale(
        icu::Locale("zh", "hant")));
}

} // namespace blink
