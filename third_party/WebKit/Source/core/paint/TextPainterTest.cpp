// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TextPainter.h"

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/api/LineLayoutText.h"
#include "core/paint/PaintInfo.h"
#include "core/style/ShadowData.h"
#include "core/style/ShadowList.h"
#include "platform/graphics/paint/PaintController.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {
namespace {

class TextPainterTest : public RenderingTest {
 public:
  TextPainterTest()
      : layout_text_(nullptr),
        paint_controller_(PaintController::Create()),
        context_(*paint_controller_) {}

 protected:
  LineLayoutText GetLineLayoutText() { return LineLayoutText(layout_text_); }

  PaintInfo CreatePaintInfo(bool uses_text_as_clip, bool is_printing) {
    return PaintInfo(
        context_, IntRect(),
        uses_text_as_clip ? PaintPhase::kTextClip
                          : PaintPhase::kSelfBlockBackgroundOnly,
        is_printing ? kGlobalPaintPrinting : kGlobalPaintNormalPhase, 0);
  }

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    SetBodyInnerHTML("Hello world");
    layout_text_ =
        ToLayoutText(GetDocument().body()->firstChild()->GetLayoutObject());
    ASSERT_TRUE(layout_text_);
    ASSERT_EQ("Hello world", layout_text_->GetText());
  }

  LayoutText* layout_text_;
  std::unique_ptr<PaintController> paint_controller_;
  GraphicsContext context_;
};

TEST_F(TextPainterTest, TextPaintingStyle_Simple) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyColor, CSSValueBlue);
  GetDocument().View()->UpdateAllLifecyclePhases();

  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLineLayoutText().GetDocument(), GetLineLayoutText().StyleRef(),
      CreatePaintInfo(false /* usesTextAsClip */, false /* isPrinting */));
  EXPECT_EQ(Color(0, 0, 255), text_style.fill_color);
  EXPECT_EQ(Color(0, 0, 255), text_style.stroke_color);
  EXPECT_EQ(Color(0, 0, 255), text_style.emphasis_mark_color);
  EXPECT_EQ(0, text_style.stroke_width);
  EXPECT_EQ(nullptr, text_style.shadow);
}

TEST_F(TextPainterTest, TextPaintingStyle_AllProperties) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyWebkitTextFillColor,
                                               CSSValueRed);
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyWebkitTextStrokeColor,
                                               CSSValueLime);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyWebkitTextEmphasisColor, CSSValueBlue);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyWebkitTextStrokeWidth, 4,
      CSSPrimitiveValue::UnitType::kPixels);
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyTextShadow,
                                               "1px 2px 3px yellow");
  GetDocument().View()->UpdateAllLifecyclePhases();

  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLineLayoutText().GetDocument(), GetLineLayoutText().StyleRef(),
      CreatePaintInfo(false /* usesTextAsClip */, false /* isPrinting */));
  EXPECT_EQ(Color(255, 0, 0), text_style.fill_color);
  EXPECT_EQ(Color(0, 255, 0), text_style.stroke_color);
  EXPECT_EQ(Color(0, 0, 255), text_style.emphasis_mark_color);
  EXPECT_EQ(4, text_style.stroke_width);
  ASSERT_NE(nullptr, text_style.shadow);
  EXPECT_EQ(1u, text_style.shadow->Shadows().size());
  EXPECT_EQ(1, text_style.shadow->Shadows()[0].X());
  EXPECT_EQ(2, text_style.shadow->Shadows()[0].Y());
  EXPECT_EQ(3, text_style.shadow->Shadows()[0].Blur());
  EXPECT_EQ(Color(255, 255, 0),
            text_style.shadow->Shadows()[0].GetColor().GetColor());
}

TEST_F(TextPainterTest, TextPaintingStyle_UsesTextAsClip) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyWebkitTextFillColor,
                                               CSSValueRed);
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyWebkitTextStrokeColor,
                                               CSSValueLime);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyWebkitTextEmphasisColor, CSSValueBlue);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyWebkitTextStrokeWidth, 4,
      CSSPrimitiveValue::UnitType::kPixels);
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyTextShadow,
                                               "1px 2px 3px yellow");
  GetDocument().View()->UpdateAllLifecyclePhases();

  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLineLayoutText().GetDocument(), GetLineLayoutText().StyleRef(),
      CreatePaintInfo(true /* usesTextAsClip */, false /* isPrinting */));
  EXPECT_EQ(Color::kBlack, text_style.fill_color);
  EXPECT_EQ(Color::kBlack, text_style.stroke_color);
  EXPECT_EQ(Color::kBlack, text_style.emphasis_mark_color);
  EXPECT_EQ(4, text_style.stroke_width);
  EXPECT_EQ(nullptr, text_style.shadow);
}

TEST_F(TextPainterTest,
       TextPaintingStyle_ForceBackgroundToWhite_NoAdjustmentNeeded) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyWebkitTextFillColor,
                                               CSSValueRed);
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyWebkitTextStrokeColor,
                                               CSSValueLime);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyWebkitTextEmphasisColor, CSSValueBlue);
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyWebkitPrintColorAdjust, CSSValueEconomy);
  GetDocument().GetSettings()->SetShouldPrintBackgrounds(false);
  FloatSize page_size(500, 800);
  GetFrame().SetPrinting(true, page_size, page_size, 1);
  GetDocument().View()->UpdateAllLifecyclePhases();

  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLineLayoutText().GetDocument(), GetLineLayoutText().StyleRef(),
      CreatePaintInfo(false /* usesTextAsClip */, true /* isPrinting */));
  EXPECT_EQ(Color(255, 0, 0), text_style.fill_color);
  EXPECT_EQ(Color(0, 255, 0), text_style.stroke_color);
  EXPECT_EQ(Color(0, 0, 255), text_style.emphasis_mark_color);
}

TEST_F(TextPainterTest, TextPaintingStyle_ForceBackgroundToWhite_Darkened) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyWebkitTextFillColor,
                                               "rgb(255, 220, 220)");
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyWebkitTextStrokeColor,
                                               "rgb(220, 255, 220)");
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyWebkitTextEmphasisColor, "rgb(220, 220, 255)");
  GetDocument().body()->SetInlineStyleProperty(
      CSSPropertyWebkitPrintColorAdjust, CSSValueEconomy);
  GetDocument().GetSettings()->SetShouldPrintBackgrounds(false);
  FloatSize page_size(500, 800);
  GetFrame().SetPrinting(true, page_size, page_size, 1);
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();

  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      GetLineLayoutText().GetDocument(), GetLineLayoutText().StyleRef(),
      CreatePaintInfo(false /* usesTextAsClip */, true /* isPrinting */));
  EXPECT_EQ(Color(255, 220, 220).Dark(), text_style.fill_color);
  EXPECT_EQ(Color(220, 255, 220).Dark(), text_style.stroke_color);
  EXPECT_EQ(Color(220, 220, 255).Dark(), text_style.emphasis_mark_color);
}

}  // namespace
}  // namespace blink
