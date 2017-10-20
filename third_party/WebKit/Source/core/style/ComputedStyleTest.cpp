// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/ComputedStyle.h"

#include "build/build_config.h"
#include "core/style/ClipPathOperation.h"
#include "core/style/ShapeValue.h"
#include "core/style/StyleDifference.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ComputedStyleTest, ShapeOutsideBoxEqual) {
  ShapeValue* shape1 = ShapeValue::CreateBoxShapeValue(kContentBox);
  ShapeValue* shape2 = ShapeValue::CreateBoxShapeValue(kContentBox);
  scoped_refptr<ComputedStyle> style1 = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> style2 = ComputedStyle::Create();
  style1->SetShapeOutside(shape1);
  style2->SetShapeOutside(shape2);
  ASSERT_EQ(*style1, *style2);
}

TEST(ComputedStyleTest, ShapeOutsideCircleEqual) {
  scoped_refptr<BasicShapeCircle> circle1 = BasicShapeCircle::Create();
  scoped_refptr<BasicShapeCircle> circle2 = BasicShapeCircle::Create();
  ShapeValue* shape1 = ShapeValue::CreateShapeValue(circle1, kContentBox);
  ShapeValue* shape2 = ShapeValue::CreateShapeValue(circle2, kContentBox);
  scoped_refptr<ComputedStyle> style1 = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> style2 = ComputedStyle::Create();
  style1->SetShapeOutside(shape1);
  style2->SetShapeOutside(shape2);
  ASSERT_EQ(*style1, *style2);
}

TEST(ComputedStyleTest, ClipPathEqual) {
  scoped_refptr<BasicShapeCircle> shape = BasicShapeCircle::Create();
  scoped_refptr<ShapeClipPathOperation> path1 =
      ShapeClipPathOperation::Create(shape);
  scoped_refptr<ShapeClipPathOperation> path2 =
      ShapeClipPathOperation::Create(shape);
  scoped_refptr<ComputedStyle> style1 = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> style2 = ComputedStyle::Create();
  style1->SetClipPath(path1);
  style2->SetClipPath(path2);
  ASSERT_EQ(*style1, *style2);
}

TEST(ComputedStyleTest, FocusRingWidth) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetEffectiveZoom(3.5);
#if defined(OS_MACOSX)
  style->SetOutlineStyle(EBorderStyle::kSolid);
  ASSERT_EQ(3, style->GetOutlineStrokeWidthForFocusRing());
#else
  ASSERT_EQ(3.5, style->GetOutlineStrokeWidthForFocusRing());
  style->SetEffectiveZoom(0.5);
  ASSERT_EQ(1, style->GetOutlineStrokeWidthForFocusRing());
#endif
}

TEST(ComputedStyleTest, FocusRingOutset) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetOutlineStyle(EBorderStyle::kSolid);
  style->SetOutlineStyleIsAuto(kOutlineIsAutoOn);
  style->SetEffectiveZoom(4.75);
#if defined(OS_MACOSX)
  ASSERT_EQ(4, style->OutlineOutsetExtent());
#else
  ASSERT_EQ(3, style->OutlineOutsetExtent());
#endif
}

TEST(ComputedStyleTest, Preserve3dForceStackingContext) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetTransformStyle3D(ETransformStyle3D::kPreserve3d);
  style->SetOverflowX(EOverflow::kHidden);
  style->SetOverflowY(EOverflow::kHidden);
  style->UpdateIsStackingContext(false, false);
  EXPECT_EQ(ETransformStyle3D::kFlat, style->UsedTransformStyle3D());
  EXPECT_TRUE(style->IsStackingContext());
}

TEST(ComputedStyleTest, FirstPublicPseudoStyle) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetHasPseudoStyle(kPseudoIdFirstLine);
  EXPECT_TRUE(style->HasPseudoStyle(kPseudoIdFirstLine));
  EXPECT_TRUE(style->HasAnyPublicPseudoStyles());
}

TEST(ComputedStyleTest, LastPublicPseudoStyle) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetHasPseudoStyle(kPseudoIdScrollbar);
  EXPECT_TRUE(style->HasPseudoStyle(kPseudoIdScrollbar));
  EXPECT_TRUE(style->HasAnyPublicPseudoStyles());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesRespectsTransformAnimation) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);
  other->SetHasCurrentTransformAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.TransformChanged());
}

TEST(ComputedStyleTest, HasOutlineWithCurrentColor) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  EXPECT_FALSE(style->HasOutline());
  EXPECT_FALSE(style->HasOutlineWithCurrentColor());
  style->SetOutlineColor(StyleColor::CurrentColor());
  EXPECT_FALSE(style->HasOutlineWithCurrentColor());
  style->SetOutlineWidth(5);
  EXPECT_FALSE(style->HasOutlineWithCurrentColor());
  style->SetOutlineStyle(EBorderStyle::kSolid);
  EXPECT_TRUE(style->HasOutlineWithCurrentColor());
}

TEST(ComputedStyleTest, HasBorderColorReferencingCurrentColor) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  EXPECT_FALSE(style->HasBorderColorReferencingCurrentColor());
  style->SetBorderBottomColor(StyleColor::CurrentColor());
  EXPECT_FALSE(style->HasBorderColorReferencingCurrentColor());
  style->SetBorderBottomWidth(5);
  EXPECT_FALSE(style->HasBorderColorReferencingCurrentColor());
  style->SetBorderBottomStyle(EBorderStyle::kSolid);
  EXPECT_TRUE(style->HasBorderColorReferencingCurrentColor());
}

TEST(ComputedStyleTest, BorderWidth) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetBorderBottomWidth(5);
  EXPECT_EQ(style->BorderBottomWidth(), 0);
  EXPECT_EQ(style->BorderBottom().Width(), 5);
  style->SetBorderBottomStyle(EBorderStyle::kSolid);
  EXPECT_EQ(style->BorderBottomWidth(), 5);
  EXPECT_EQ(style->BorderBottom().Width(), 5);
}

}  // namespace blink
