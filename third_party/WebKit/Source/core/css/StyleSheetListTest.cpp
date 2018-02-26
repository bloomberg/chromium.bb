// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSStyleSheetInit.h"
#include "core/css/StyleSheetList.h"
#include "core/testing/PageTestBase.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class StyleSheetListTest : public PageTestBase {
 protected:
  virtual void SetUp() {
    PageTestBase::SetUp();
    RuntimeEnabledFeatures::SetConstructableStylesheetsEnabled(true);
  }
};

TEST_F(StyleSheetListTest, ConstructorWithoutRuntimeFlagThrowsException) {
  DummyExceptionStateForTesting exception_state;
  RuntimeEnabledFeatures::SetConstructableStylesheetsEnabled(false);
  HeapVector<Member<CSSStyleSheet>> style_sheet_vector;
  EXPECT_EQ(StyleSheetList::Create(style_sheet_vector, exception_state),
            nullptr);
  ASSERT_TRUE(exception_state.HadException());
}

TEST_F(StyleSheetListTest, StyleSheetListConstructionWithEmptyList) {
  DummyExceptionStateForTesting exception_state;
  HeapVector<Member<CSSStyleSheet>> style_sheet_vector;
  StyleSheetList* sheet_list =
      StyleSheetList::Create(style_sheet_vector, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  EXPECT_EQ(sheet_list->length(), 0U);
}

TEST_F(StyleSheetListTest, StyleSheetListConstructionWithNonEmptyList) {
  DummyExceptionStateForTesting exception_state;
  HeapVector<Member<CSSStyleSheet>> style_sheet_vector;
  CSSStyleSheetInit init;
  init.setTitle("Red Sheet");
  CSSStyleSheet* red_style_sheet = CSSStyleSheet::Create(
      GetDocument(), ".red { color: red; }", init, exception_state);
  init.setTitle("Blue Sheet");
  CSSStyleSheet* blue_style_sheet = CSSStyleSheet::Create(
      GetDocument(), ".blue { color: blue; }", init, exception_state);
  style_sheet_vector.push_back(red_style_sheet);
  style_sheet_vector.push_back(blue_style_sheet);

  StyleSheetList* sheet_list =
      StyleSheetList::Create(style_sheet_vector, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  EXPECT_EQ(sheet_list->length(), 2U);
  EXPECT_EQ(sheet_list->item(0), red_style_sheet);
  EXPECT_EQ(sheet_list->item(1), blue_style_sheet);
}

}  // namespace blink
