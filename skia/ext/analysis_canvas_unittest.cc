// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "skia/ext/analysis_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkShader.h"

namespace {

void solidColorFill(skia::AnalysisCanvas& canvas) {
  canvas.clear(SkColorSetARGB(255, 255, 255, 255));
}

void transparentFill(skia::AnalysisCanvas& canvas) {
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));
}

} // namespace
namespace skia {

class TestPixelRef : public SkPixelRef {
 public:
  // Pure virtual implementation.
  SkFlattenable::Factory getFactory() { return NULL; }
  void* onLockPixels(SkColorTable**) { return NULL; }
  void onUnlockPixels() {}
};

class TestLazyPixelRef : public LazyPixelRef {
 public:
  // Pure virtual implementation.
  SkFlattenable::Factory getFactory() { return NULL; }
  void* onLockPixels(SkColorTable**) { return NULL; }
  void onUnlockPixels() {}
  bool PrepareToDecode(const PrepareParams& params) { return true; }
  void Decode() {}
};

class TestShader : public SkShader {
 public:
  TestShader(SkBitmap* bitmap)
    : bitmap_(bitmap) {
  }

  SkShader::BitmapType asABitmap(SkBitmap* bitmap,
                                 SkMatrix*, TileMode xy[2]) const {
    if (bitmap)
      *bitmap = *bitmap_;
    return SkShader::kDefault_BitmapType;
  }

  // Pure virtual implementation.
  void shadeSpan(int x, int y, SkPMColor[], int count) {}
  SkFlattenable::Factory getFactory() { return NULL; }

 private:

  SkBitmap* bitmap_;
};

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
  EXPECT_FALSE(canvas.isCheap());
  EXPECT_EQ(outputColor, color);

  // Translucent color
  color = SkColorSetARGB(128, 11, 22, 33);
  canvas.clear(color);

  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_FALSE(canvas.isCheap());

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
  EXPECT_FALSE(canvas.isCheap());

  // Draw oval test.
  solidColorFill(canvas);
  canvas.drawOval(SkRect::MakeWH(255, 255), paint);

  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_FALSE(canvas.isCheap());

  // Draw bitmap test.
  solidColorFill(canvas);
  SkBitmap secondBitmap;
  secondBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  canvas.drawBitmap(secondBitmap, 0, 0);

  EXPECT_FALSE(canvas.getColorIfSolid(&outputColor));
  EXPECT_FALSE(canvas.isTransparent());
  EXPECT_FALSE(canvas.isCheap());
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

TEST(AnalysisCanvasTest, LazyPixelRefs) {
  // Set up two lazy and two non-lazy pixel refs and the corresponding bitmaps.
  TestLazyPixelRef firstLazyPixelRef;
  firstLazyPixelRef.setURI("lazy");
  TestLazyPixelRef secondLazyPixelRef;
  secondLazyPixelRef.setURI("lazy");

  TestPixelRef firstNonLazyPixelRef;
  TestPixelRef secondNonLazyPixelRef;
  secondNonLazyPixelRef.setURI("notsolazy");

  SkBitmap firstLazyBitmap;
  firstLazyBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  firstLazyBitmap.setPixelRef(&firstLazyPixelRef);
  SkBitmap secondLazyBitmap;
  secondLazyBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  secondLazyBitmap.setPixelRef(&secondLazyPixelRef);

  SkBitmap firstNonLazyBitmap;
  firstNonLazyBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  SkBitmap secondNonLazyBitmap;
  secondNonLazyBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  secondNonLazyBitmap.setPixelRef(&secondNonLazyPixelRef);

  // The testcase starts here.
  SkBitmap emptyBitmap;
  emptyBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  skia::AnalysisDevice device(emptyBitmap);
  skia::AnalysisCanvas canvas(&device);
  
  // This should be the first ref.
  canvas.drawBitmap(firstLazyBitmap, 0, 0);
  // The following will be ignored (non-lazy).
  canvas.drawBitmap(firstNonLazyBitmap, 0, 0);
  canvas.drawBitmap(firstNonLazyBitmap, 0, 0);
  canvas.drawBitmap(secondNonLazyBitmap, 0, 0);
  canvas.drawBitmap(secondNonLazyBitmap, 0, 0);
  // This one will be ignored (already exists).
  canvas.drawBitmap(firstLazyBitmap, 0, 0);
  // This should be the second ref.
  canvas.drawBitmap(secondLazyBitmap, 0, 0);

  std::list<skia::LazyPixelRef*> pixelRefs;
  canvas.consumeLazyPixelRefs(&pixelRefs);

  // We expect to get only lazy pixel refs and only unique results.
  EXPECT_EQ(pixelRefs.size(), 2u);
  if (!pixelRefs.empty()) {
    EXPECT_EQ(pixelRefs.front(),
              static_cast<LazyPixelRef*>(&firstLazyPixelRef));
    EXPECT_EQ(pixelRefs.back(),
              static_cast<LazyPixelRef*>(&secondLazyPixelRef));
  }
}

TEST(AnalysisCanvasTest, PixelRefsFromPaint) {
  TestLazyPixelRef lazyPixelRef;
  lazyPixelRef.setURI("lazy");

  TestPixelRef nonLazyPixelRef;
  nonLazyPixelRef.setURI("notsolazy");

  SkBitmap lazyBitmap;
  lazyBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  lazyBitmap.setPixelRef(&lazyPixelRef);

  SkBitmap nonLazyBitmap;
  nonLazyBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  nonLazyBitmap.setPixelRef(&nonLazyPixelRef);

  TestShader lazyShader(&lazyBitmap);
  TestShader nonLazyShader(&nonLazyBitmap);

  SkPaint lazyPaint;
  lazyPaint.setShader(&lazyShader);
  SkPaint nonLazyPaint;
  nonLazyPaint.setShader(&nonLazyShader);

  SkBitmap emptyBitmap;
  emptyBitmap.setConfig(SkBitmap::kNo_Config, 255, 255);
  skia::AnalysisDevice device(emptyBitmap);
  skia::AnalysisCanvas canvas(&device);

  canvas.drawRect(SkRect::MakeWH(255, 255), lazyPaint);
  canvas.drawRect(SkRect::MakeWH(255, 255), lazyPaint);
  canvas.drawRect(SkRect::MakeWH(255, 255), lazyPaint);
  canvas.drawRect(SkRect::MakeWH(255, 255), nonLazyPaint);
  canvas.drawRect(SkRect::MakeWH(255, 255), nonLazyPaint);
  canvas.drawRect(SkRect::MakeWH(255, 255), nonLazyPaint);

  std::list<skia::LazyPixelRef*> pixelRefs;
  canvas.consumeLazyPixelRefs(&pixelRefs);

  // We expect to get only lazy pixel refs and only unique results.
  EXPECT_EQ(pixelRefs.size(), 1u);
  if (!pixelRefs.empty()) {
    EXPECT_EQ(pixelRefs.front(), static_cast<LazyPixelRef*>(&lazyPixelRef));
  }
}

}  // namespace skia
