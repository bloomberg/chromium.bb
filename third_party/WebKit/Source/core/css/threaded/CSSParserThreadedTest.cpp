// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserContext.h"

#include "core/css/StylePropertySet.h"
#include "core/css/threaded/MultiThreadedTestUtil.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CSSParserThreadedTest : public MultiThreadedTest {
 public:
  static void TestSingle(CSSPropertyID prop, const String& text) {
    const CSSValue* value = CSSParser::ParseSingleValue(prop, text);
    ASSERT_TRUE(value);
    EXPECT_EQ(text, value->CssText());
  }

  static MutableStylePropertySet* TestValue(CSSPropertyID prop,
                                            const String& text) {
    MutableStylePropertySet* style =
        MutableStylePropertySet::Create(kHTMLStandardMode);
    CSSParser::ParseValue(style, prop, text, true);
    return style;
  }
};

TSAN_TEST_F(CSSParserThreadedTest, SinglePropertyFilter) {
  RunOnThreads([]() {
    TestSingle(CSSPropertyFilter, "sepia(50%)");
    TestSingle(CSSPropertyFilter, "blur(10px)");
    TestSingle(CSSPropertyFilter, "brightness(50%) invert(100%)");
  });
}

TSAN_TEST_F(CSSParserThreadedTest, SinglePropertyFont) {
  RunOnThreads([]() {
    TestSingle(CSSPropertyFontFamily, "serif");
    TestSingle(CSSPropertyFontFamily, "monospace");
    TestSingle(CSSPropertyFontFamily, "times");
    TestSingle(CSSPropertyFontFamily, "arial");

    TestSingle(CSSPropertyFontWeight, "normal");
    TestSingle(CSSPropertyFontWeight, "bold");

    TestSingle(CSSPropertyFontSize, "10px");
    TestSingle(CSSPropertyFontSize, "20em");
  });
}

TSAN_TEST_F(CSSParserThreadedTest, ValuePropertyFont) {
  RunOnThreads([]() {
    MutableStylePropertySet* v = TestValue(CSSPropertyFont, "15px arial");
    EXPECT_EQ(v->GetPropertyValue(CSSPropertyFontFamily), "arial");
    EXPECT_EQ(v->GetPropertyValue(CSSPropertyFontSize), "15px");
  });
}

}  // namespace blink
