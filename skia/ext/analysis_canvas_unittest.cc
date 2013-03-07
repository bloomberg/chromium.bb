// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "skia/ext/analysis_canvas.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

void solidColorFill(skia::AnalysisCanvas& canvas) {
  canvas.clear(SkColorSetARGB(255, 255, 255, 255));
}

void transparentFill(skia::AnalysisCanvas& canvas) {
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));
}

} // namespace
namespace skia {

TEST(AnalysisCanvasTest, EmptyCanvas) {
  SkBitmap emptyBitmap;
  emptyBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  skia::AnalysisDevice device(emptyBitmap);
  skia::AnalysisCanvas canvas(&device);
  
  SkColor color;
  EXPECT_FALSE(canvas.getColorIfSolid(&color));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());
}

TEST(AnalysisCanvasTest, ClearCanvas) {
  SkBitmap emptyBitmap;
  emptyBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  skia::AnalysisDevice device(emptyBitmap);
  skia::AnalysisCanvas canvas(&device);

  // Transparent color
  SkColor color = SkColorSetARGB(0, 12, 34, 56);
  canvas.clear(color);

  SkColor outputColor;
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_TRUE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());

  // Solid color
  color = SkColorSetARGB(255, 65, 43, 21);
  canvas.clear(color);

  EXPECT_TRUE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());
  EXPECT_EQ(outputColor, color);

  // Translucent color
  color = SkColorSetARGB(128, 11, 22, 33);
  canvas.clear(color);

  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());

  // Test helper methods
  solidColorFill(canvas);
  EXPECT_TRUE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());

  transparentFill(canvas);
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_TRUE(canvas.isTransparent());
}

TEST(AnalysisCanvasTest, ComplexActions) {
  SkBitmap emptyBitmap;
  emptyBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  skia::AnalysisDevice device(emptyBitmap);
  skia::AnalysisCanvas canvas(&device);

  // Draw paint test.
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  SkPaint paint;
  paint.setColor(color);

  canvas.drawPaint(paint);

  SkColor outputColor;
  //TODO(vmpstr): This should return true. (crbug.com/180597)
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());

  // Draw points test.
  SkPoint points[4] = {
    SkPoint::Make(0, 0),
    SkPoint::Make(255, 0),
    SkPoint::Make(255, 255),
    SkPoint::Make(0, 255)
  };

  solidColorFill(canvas);
  canvas.drawPoints(SkCanvas::kLines_PointMode, 4, points, paint);

  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());

  // Draw oval test.
  solidColorFill(canvas);
  canvas.drawOval(SkRect::MakeWH(255, 255), paint);

  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());

  // Draw bitmap test.
  solidColorFill(canvas);
  SkBitmap secondBitmap;
  secondBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  canvas.drawBitmap(secondBitmap, 0, 0);

  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());
}

TEST(AnalysisCanvasTest, SimpleDrawRect) {
  SkBitmap emptyBitmap;
  emptyBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  skia::AnalysisDevice device(emptyBitmap);
  skia::AnalysisCanvas canvas(&device);

  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  SkPaint paint;
  paint.setColor(color);
  canvas.clipRect(SkRect::MakeWH(255, 255));
  canvas.drawRect(SkRect::MakeWH(255, 255), paint);

  SkColor outputColor;
  EXPECT_TRUE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());
  EXPECT_EQ(color, outputColor);

  color = SkColorSetARGB(255, 22, 33, 44);
  paint.setColor(color);
  canvas.translate(-128, -128);
  canvas.drawRect(SkRect::MakeWH(382, 382), paint);

  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());

  color = SkColorSetARGB(255, 33, 44, 55);
  paint.setColor(color);
  canvas.drawRect(SkRect::MakeWH(383, 383), paint);

  EXPECT_TRUE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());
  EXPECT_EQ(color, outputColor);

  color = SkColorSetARGB(0, 0, 0, 0);
  paint.setColor(color);
  canvas.drawRect(SkRect::MakeWH(383, 383), paint);

  EXPECT_TRUE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());
  EXPECT_EQ(outputColor, SkColorSetARGB(255, 33, 44, 55));

  color = SkColorSetARGB(128, 128, 128, 128);
  paint.setColor(color);
  canvas.drawRect(SkRect::MakeWH(383, 383), paint);

  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());

  paint.setXfermodeMode(SkXfermode::kClear_Mode);
  canvas.drawRect(SkRect::MakeWH(382, 382), paint);

  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());

  canvas.drawRect(SkRect::MakeWH(383, 383), paint);

  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_TRUE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());

  canvas.translate(128, 128);
  color = SkColorSetARGB(255, 11, 22, 33);
  paint.setColor(color);
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  canvas.drawRect(SkRect::MakeWH(255, 255), paint);

  EXPECT_TRUE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());
  EXPECT_EQ(color, outputColor);

  canvas.rotate(50);
  canvas.drawRect(SkRect::MakeWH(255, 255), paint);

  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_TRUE(canvas.isCheap());
}

TEST(AnalysisCanvasTest, ClipPath) {
  SkBitmap emptyBitmap;
  emptyBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  skia::AnalysisDevice device(emptyBitmap);
  skia::AnalysisCanvas canvas(&device);

  SkPath path;
  path.moveTo(0, 0);
  path.lineTo(255, 0);
  path.lineTo(255, 255);
  path.lineTo(0, 255);

  SkColor outputColor;
  solidColorFill(canvas);
  canvas.clipPath(path);
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));

  canvas.save();
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));

  canvas.clipPath(path);
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));

  canvas.restore();
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));

  solidColorFill(canvas);
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
}

TEST(AnalysisCanvasTest, SaveLayerRestore) {
  SkBitmap emptyBitmap;
  emptyBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  skia::AnalysisDevice device(emptyBitmap);
  skia::AnalysisCanvas canvas(&device);

  SkColor outputColor;
  solidColorFill(canvas);
  EXPECT_TRUE(canvas.getColorIfSolid(&outputColor));
  
  SkRect bounds = SkRect::MakeWH(255, 255);
  SkPaint paint;
  paint.setColor(SkColorSetARGB(255, 255, 255, 255));
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);

  // This should force non-transparency
  canvas.saveLayer(&bounds, &paint, SkCanvas::kMatrix_SaveFlag);
  EXPECT_TRUE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());

  transparentFill(canvas);
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());

  solidColorFill(canvas);
  EXPECT_TRUE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());

  paint.setXfermodeMode(SkXfermode::kDst_Mode);

  // This should force non-solid color
  canvas.saveLayer(&bounds, &paint, SkCanvas::kMatrix_SaveFlag);
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());

  transparentFill(canvas);
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());

  solidColorFill(canvas);
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  
  canvas.restore();
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());

  transparentFill(canvas);
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());

  solidColorFill(canvas);
  EXPECT_TRUE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());

  canvas.restore();
  EXPECT_TRUE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());

  transparentFill(canvas);
  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_TRUE(canvas.isTransparent());

  solidColorFill(canvas);
  EXPECT_TRUE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
}

}  // namespace skia
