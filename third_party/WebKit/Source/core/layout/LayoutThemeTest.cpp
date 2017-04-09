// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTheme.h"

#include "core/dom/Document.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/style/ComputedStyle.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/graphics/Color.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class LayoutThemeTest : public ::testing::Test {
 protected:
  void SetUp() override;
  Document& GetDocument() const { return *document_; }
  void SetHtmlInnerHTML(const char* html_content);

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<Document> document_;
};

void LayoutThemeTest::SetUp() {
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  document_ = &dummy_page_holder_->GetDocument();
  DCHECK(document_);
}

void LayoutThemeTest::SetHtmlInnerHTML(const char* html_content) {
  GetDocument().documentElement()->setInnerHTML(String::FromUTF8(html_content));
  GetDocument().View()->UpdateAllLifecyclePhases();
}

inline Color OutlineColor(Element* element) {
  return element->GetComputedStyle()->VisitedDependentColor(
      CSSPropertyOutlineColor);
}

inline EBorderStyle OutlineStyle(Element* element) {
  return element->GetComputedStyle()->OutlineStyle();
}

TEST_F(LayoutThemeTest, ChangeFocusRingColor) {
  SetHtmlInnerHTML("<span id=span tabIndex=0>Span</span>");

  Element* span = GetDocument().GetElementById(AtomicString("span"));
  EXPECT_NE(nullptr, span);
  EXPECT_NE(nullptr, span->GetLayoutObject());

  Color custom_color = MakeRGB(123, 145, 167);

  // Checking unfocused style.
  EXPECT_EQ(kBorderStyleNone, OutlineStyle(span));
  EXPECT_NE(custom_color, OutlineColor(span));

  // Do focus.
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  span->focus();
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Checking focused style.
  EXPECT_NE(kBorderStyleNone, OutlineStyle(span));
  EXPECT_NE(custom_color, OutlineColor(span));

  // Change focus ring color.
  LayoutTheme::GetTheme().SetCustomFocusRingColor(custom_color);
  Page::PlatformColorsChanged();
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the focus ring color is updated.
  EXPECT_NE(kBorderStyleNone, OutlineStyle(span));
  EXPECT_EQ(custom_color, OutlineColor(span));
}

TEST_F(LayoutThemeTest, FormatMediaTime) {
  struct {
    float time;
    float duration;
    String expected_result;
  } tests[] = {
      {1, 1, "0:01"},        {1, 15, "0:01"},        {1, 600, "0:01"},
      {1, 3600, "00:01"},    {1, 7200, "000:01"},    {15, 15, "0:15"},
      {15, 600, "0:15"},     {15, 3600, "00:15"},    {15, 7200, "000:15"},
      {600, 600, "10:00"},   {600, 3600, "10:00"},   {600, 7200, "010:00"},
      {3600, 3600, "60:00"}, {3600, 7200, "060:00"}, {7200, 7200, "120:00"},
  };

  for (const auto& testcase : tests) {
    EXPECT_EQ(testcase.expected_result,
              LayoutTheme::GetTheme().FormatMediaControlsCurrentTime(
                  testcase.time, testcase.duration));
  }
}

}  // namespace blink
