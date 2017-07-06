// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/FilterOperationResolver.h"

#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/resolver/FontStyleResolver.h"
#include "core/css/threaded/MultiThreadedTestUtil.h"
#include "platform/Language.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontCustomPlatformData.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontSelector.h"
#include "platform/testing/FontTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::testing::CreateTestFont;

namespace blink {

TSAN_TEST(FontObjectThreadedTest, Language) {
  RunOnThreads([]() { EXPECT_EQ(DefaultLanguage(), "en-US"); });
}

TSAN_TEST(FontObjectThreadedTest, GetFontDefinition) {
  RunOnThreads([]() {
    MutableStylePropertySet* style =
        MutableStylePropertySet::Create(kHTMLStandardMode);
    CSSParser::ParseValue(style, CSSPropertyFont, "15px Ahem", true);

    FontDescription desc = FontStyleResolver::ComputeFont(*style);

    EXPECT_EQ(desc.SpecifiedSize(), 15);
    EXPECT_EQ(desc.ComputedSize(), 15);
    EXPECT_EQ(desc.Family().Family(), "Ahem");
  });
}

// This test passes by not crashing TSAN.
TSAN_TEST(FontObjectThreadedTest, FontSelector) {
  RunOnThreads([]() {
    Font font =
        CreateTestFont("Ahem", testing::PlatformTestDataPath("Ahem.woff"), 16);
  });
}

}  // namespace blink
