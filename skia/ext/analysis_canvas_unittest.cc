// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "skia/ext/analysis_canvas.h"
#include "skia/ext/refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkOffsetImageFilter.h"

namespace {

void SolidColorFill(skia::AnalysisCanvas& canvas) {
  canvas.clear(SkColorSetARGB(255, 255, 255, 255));
}

void TransparentFill(skia::AnalysisCanvas& canvas) {
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));
}

} // namespace
namespace skia {

TEST(AnalysisCanvasTest, EmptyCanvas) {
  skia::AnalysisCanvas canvas(255, 255);

  SkColor color;
  EXPECT_TRUE(canvas.GetColorIfSolid(&color));
  EXPECT_EQ(color, SkColorSetARGB(0, 0, 0, 0));
}

TEST(AnalysisCanvasTest, ClearCanvas) {
  skia::AnalysisCanvas canvas(255, 255);

  // Transparent color
  SkColor color = SkColorSetARGB(0, 12, 34, 56);
  canvas.clear(color);

  SkColor outputColor;
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  // Solid color
  color = SkColorSetARGB(255, 65, 43, 21);
  canvas.clear(color);

  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
  EXPECT_EQ(outputColor, color);

  // Translucent color
  color = SkColorSetARGB(128, 11, 22, 33);
  canvas.clear(color);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  // Test helper methods
  SolidColorFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  TransparentFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
}

TEST(AnalysisCanvasTest, ComplexActions) {
  skia::AnalysisCanvas canvas(255, 255);

  // Draw paint test.
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  SkPaint paint;
  paint.setColor(color);

  canvas.drawPaint(paint);

  SkColor outputColor;
  //TODO(vmpstr): This should return true. (crbug.com/180597)
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  // Draw points test.
  SkPoint points[4] = {
    SkPoint::Make(0, 0),
    SkPoint::Make(255, 0),
    SkPoint::Make(255, 255),
    SkPoint::Make(0, 255)
  };

  SolidColorFill(canvas);
  canvas.drawPoints(SkCanvas::kLines_PointMode, 4, points, paint);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  // Draw oval test.
  SolidColorFill(canvas);
  canvas.drawOval(SkRect::MakeWH(255, 255), paint);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  // Draw bitmap test.
  SolidColorFill(canvas);
  SkBitmap secondBitmap;
  secondBitmap.allocN32Pixels(255, 255);
  canvas.drawBitmap(secondBitmap, 0, 0);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));
}

TEST(AnalysisCanvasTest, SimpleDrawRect) {
  skia::AnalysisCanvas canvas(255, 255);

  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  SkPaint paint;
  paint.setColor(color);
  canvas.clipRect(SkRect::MakeWH(255, 255));
  canvas.drawRect(SkRect::MakeWH(255, 255), paint);

  SkColor outputColor;
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
  EXPECT_EQ(color, outputColor);

  color = SkColorSetARGB(255, 22, 33, 44);
  paint.setColor(color);
  canvas.translate(-128, -128);
  canvas.drawRect(SkRect::MakeWH(382, 382), paint);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  color = SkColorSetARGB(255, 33, 44, 55);
  paint.setColor(color);
  canvas.drawRect(SkRect::MakeWH(383, 383), paint);

  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
  EXPECT_EQ(color, outputColor);

  color = SkColorSetARGB(0, 0, 0, 0);
  paint.setColor(color);
  canvas.drawRect(SkRect::MakeWH(383, 383), paint);

  // This test relies on canvas treating a paint with 0-color as a no-op
  // thus not changing its "is_solid" status.
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
  EXPECT_EQ(outputColor, SkColorSetARGB(255, 33, 44, 55));

  color = SkColorSetARGB(128, 128, 128, 128);
  paint.setColor(color);
  canvas.drawRect(SkRect::MakeWH(383, 383), paint);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  paint.setXfermodeMode(SkXfermode::kClear_Mode);
  canvas.drawRect(SkRect::MakeWH(382, 382), paint);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  canvas.drawRect(SkRect::MakeWH(383, 383), paint);

  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  canvas.translate(128, 128);
  color = SkColorSetARGB(255, 11, 22, 33);
  paint.setColor(color);
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  canvas.drawRect(SkRect::MakeWH(255, 255), paint);

  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
  EXPECT_EQ(color, outputColor);

  canvas.rotate(50);
  canvas.drawRect(SkRect::MakeWH(255, 255), paint);

  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));
}

TEST(AnalysisCanvasTest, FilterPaint) {
  skia::AnalysisCanvas canvas(255, 255);
  SkPaint paint;

  skia::RefPtr<SkImageFilter> filter = skia::AdoptRef(SkOffsetImageFilter::Create(10, 10));
  paint.setImageFilter(filter.get());
  canvas.drawRect(SkRect::MakeWH(255, 255), paint);

  SkColor outputColor;
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));
}

TEST(AnalysisCanvasTest, ClipPath) {
  skia::AnalysisCanvas canvas(255, 255);

  // Skia will look for paths that are actually rects and treat
  // them as such. We add a divot to the following path to prevent
  // this optimization and truly test clipPath's behavior.
  SkPath path;
  path.moveTo(0, 0);
  path.lineTo(128, 50); 
  path.lineTo(255, 0);
  path.lineTo(255, 255);
  path.lineTo(0, 255);

  SkColor outputColor;
  SolidColorFill(canvas);
  canvas.clipPath(path);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  canvas.save();
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  canvas.clipPath(path);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  canvas.restore();
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  SolidColorFill(canvas);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));
}

TEST(AnalysisCanvasTest, SaveLayerRestore) {
  skia::AnalysisCanvas canvas(255, 255);

  SkColor outputColor;
  SolidColorFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  
  SkRect bounds = SkRect::MakeWH(255, 255);
  SkPaint paint;
  paint.setColor(SkColorSetARGB(255, 255, 255, 255));
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);

  // This should force non-transparency
  canvas.saveLayer(&bounds, &paint);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  TransparentFill(canvas);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  SolidColorFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  paint.setXfermodeMode(SkXfermode::kDst_Mode);

  // This should force non-solid color
  canvas.saveLayer(&bounds, &paint);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  TransparentFill(canvas);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  SolidColorFill(canvas);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));
  
  canvas.restore();
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  TransparentFill(canvas);
  EXPECT_FALSE(canvas.GetColorIfSolid(&outputColor));

  SolidColorFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  canvas.restore();
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  TransparentFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);

  SolidColorFill(canvas);
  EXPECT_TRUE(canvas.GetColorIfSolid(&outputColor));
  EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), outputColor);
}

}  // namespace skia
