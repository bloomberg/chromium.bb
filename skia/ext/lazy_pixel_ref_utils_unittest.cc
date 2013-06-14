// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cc/test/geometry_test_utils.h"
#include "skia/ext/lazy_pixel_ref.h"
#include "skia/ext/lazy_pixel_ref_utils.h"
#include "skia/ext/refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkFlattenableBuffers.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/src/core/SkOrderedReadBuffer.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"

namespace skia {

namespace {

void CreateBitmap(gfx::Size size, const char* uri, SkBitmap* bitmap);

class TestPixelRef : public SkPixelRef {
 public:
  TestPixelRef(int width, int height);
  virtual ~TestPixelRef();

  virtual SkFlattenable::Factory getFactory() OVERRIDE;
  virtual void* onLockPixels(SkColorTable** color_table) OVERRIDE;
  virtual void onUnlockPixels() OVERRIDE {}
  virtual SkPixelRef* deepCopy(SkBitmap::Config config, const SkIRect* subset)
      OVERRIDE;

 private:
  scoped_ptr<char[]> pixels_;
};

class TestLazyPixelRef : public skia::LazyPixelRef {
 public:
  TestLazyPixelRef(int width, int height);
  virtual ~TestLazyPixelRef();

  virtual SkFlattenable::Factory getFactory() OVERRIDE;
  virtual void* onLockPixels(SkColorTable** color_table) OVERRIDE;
  virtual void onUnlockPixels() OVERRIDE {}
  virtual bool PrepareToDecode(const PrepareParams& params) OVERRIDE;
  virtual bool MaybeDecoded() OVERRIDE;
  virtual SkPixelRef* deepCopy(SkBitmap::Config config, const SkIRect* subset)
      OVERRIDE;
  virtual void Decode() OVERRIDE {}

 private:
  scoped_ptr<char[]> pixels_;
};

class TestLazyShader : public SkShader {
 public:
  TestLazyShader() { CreateBitmap(gfx::Size(50, 50), "lazy", &bitmap_); }

  TestLazyShader(SkFlattenableReadBuffer& flattenable_buffer) {
    SkOrderedReadBuffer& buffer =
        static_cast<SkOrderedReadBuffer&>(flattenable_buffer);
    SkReader32* reader = buffer.getReader32();

    reader->skip(-4);
    uint32_t toSkip = reader->readU32();
    reader->skip(toSkip);

    CreateBitmap(gfx::Size(50, 50), "lazy", &bitmap_);
  }

  virtual SkShader::BitmapType asABitmap(SkBitmap* bitmap,
                                         SkMatrix* matrix,
                                         TileMode xy[2]) const OVERRIDE {
    if (bitmap)
      *bitmap = bitmap_;
    return SkShader::kDefault_BitmapType;
  }

  // Pure virtual implementaiton.
  virtual void shadeSpan(int x, int y, SkPMColor[], int count) OVERRIDE {}
  SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(TestLazyShader);

 private:
  SkBitmap bitmap_;
};

TestPixelRef::TestPixelRef(int width, int height)
    : pixels_(new char[4 * width * height]) {}

TestPixelRef::~TestPixelRef() {}

SkFlattenable::Factory TestPixelRef::getFactory() { return NULL; }

void* TestPixelRef::onLockPixels(SkColorTable** color_table) {
  return pixels_.get();
}

SkPixelRef* TestPixelRef::deepCopy(SkBitmap::Config config,
                                   const SkIRect* subset) {
  this->ref();
  return this;
}

TestLazyPixelRef::TestLazyPixelRef(int width, int height)
    : pixels_(new char[4 * width * height]) {}

TestLazyPixelRef::~TestLazyPixelRef() {}

SkFlattenable::Factory TestLazyPixelRef::getFactory() { return NULL; }

void* TestLazyPixelRef::onLockPixels(SkColorTable** color_table) {
  return pixels_.get();
}

bool TestLazyPixelRef::PrepareToDecode(const PrepareParams& params) {
  return true;
}

bool TestLazyPixelRef::MaybeDecoded() {
  return true;
}

SkPixelRef* TestLazyPixelRef::deepCopy(SkBitmap::Config config,
                                       const SkIRect* subset) {
  this->ref();
  return this;
}

void CreateBitmap(gfx::Size size, const char* uri, SkBitmap* bitmap) {
  skia::RefPtr<TestLazyPixelRef> lazy_pixel_ref =
      skia::AdoptRef(new TestLazyPixelRef(size.width(), size.height()));
  lazy_pixel_ref->setURI(uri);

  bitmap->setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
  bitmap->setPixelRef(lazy_pixel_ref.get());
}

SkCanvas* StartRecording(SkPicture* picture, gfx::Rect layer_rect) {
  SkCanvas* canvas = picture->beginRecording(
      layer_rect.width(),
      layer_rect.height(),
      SkPicture::kUsePathBoundsForClip_RecordingFlag |
          SkPicture::kOptimizeForClippedPlayback_RecordingFlag);

  canvas->save();
  canvas->translate(-layer_rect.x(), -layer_rect.y());
  canvas->clipRect(SkRect::MakeXYWH(
      layer_rect.x(), layer_rect.y(), layer_rect.width(), layer_rect.height()));

  return canvas;
}

void StopRecording(SkPicture* picture, SkCanvas* canvas) {
  canvas->restore();
  picture->endRecording();
}

}  // namespace

TEST(LazyPixelRefUtilsTest, DrawPaint) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  SkCanvas* canvas = StartRecording(picture.get(), layer_rect);

  TestLazyShader first_shader;
  SkPaint first_paint;
  first_paint.setShader(&first_shader);

  TestLazyShader second_shader;
  SkPaint second_paint;
  second_paint.setShader(&second_shader);

  TestLazyShader third_shader;
  SkPaint third_paint;
  third_paint.setShader(&third_shader);

  canvas->drawPaint(first_paint);
  canvas->clipRect(SkRect::MakeXYWH(34, 45, 56, 67));
  canvas->drawPaint(second_paint);
  // Total clip is now (34, 45, 56, 55)
  canvas->clipRect(SkRect::MakeWH(100, 100));
  canvas->drawPaint(third_paint);

  StopRecording(picture.get(), canvas);

  std::vector<skia::LazyPixelRefUtils::PositionLazyPixelRef> pixel_refs;
  skia::LazyPixelRefUtils::GatherPixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(3u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 256, 256),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(34, 45, 56, 67),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(34, 45, 56, 55),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
}

TEST(LazyPixelRefUtilsTest, DrawPoints) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  SkCanvas* canvas = StartRecording(picture.get(), layer_rect);

  TestLazyShader first_shader;
  SkPaint first_paint;
  first_paint.setShader(&first_shader);

  TestLazyShader second_shader;
  SkPaint second_paint;
  second_paint.setShader(&second_shader);

  TestLazyShader third_shader;
  SkPaint third_paint;
  third_paint.setShader(&third_shader);

  SkPoint points[3];
  points[0].set(10, 10);
  points[1].set(100, 20);
  points[2].set(50, 100);
  // (10, 10, 90, 90).
  canvas->drawPoints(SkCanvas::kPolygon_PointMode, 3, points, first_paint);

  canvas->save();

  canvas->clipRect(SkRect::MakeWH(50, 50));
  // (10, 10, 40, 40).
  canvas->drawPoints(SkCanvas::kPolygon_PointMode, 3, points, second_paint);

  canvas->restore();

  points[0].set(50, 55);
  points[1].set(50, 55);
  points[2].set(200, 200);
  // (50, 55, 150, 145).
  canvas->drawPoints(SkCanvas::kPolygon_PointMode, 3, points, third_paint);

  StopRecording(picture.get(), canvas);

  std::vector<skia::LazyPixelRefUtils::PositionLazyPixelRef> pixel_refs;
  skia::LazyPixelRefUtils::GatherPixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(3u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(10, 10, 90, 90),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(10, 10, 40, 40),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(50, 55, 150, 145),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
}

TEST(LazyPixelRefUtilsTest, DrawRect) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  SkCanvas* canvas = StartRecording(picture.get(), layer_rect);

  TestLazyShader first_shader;
  SkPaint first_paint;
  first_paint.setShader(&first_shader);

  TestLazyShader second_shader;
  SkPaint second_paint;
  second_paint.setShader(&second_shader);

  TestLazyShader third_shader;
  SkPaint third_paint;
  third_paint.setShader(&third_shader);

  // (10, 20, 30, 40).
  canvas->drawRect(SkRect::MakeXYWH(10, 20, 30, 40), first_paint);

  canvas->save();

  canvas->translate(5, 17);
  // (5, 50, 25, 35)
  canvas->drawRect(SkRect::MakeXYWH(0, 33, 25, 35), second_paint);

  canvas->restore();

  canvas->clipRect(SkRect::MakeXYWH(50, 50, 50, 50));
  canvas->translate(20, 20);
  // (50, 50, 50, 50)
  canvas->drawRect(SkRect::MakeXYWH(0, 0, 100, 100), third_paint);

  std::vector<skia::LazyPixelRefUtils::PositionLazyPixelRef> pixel_refs;
  skia::LazyPixelRefUtils::GatherPixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(3u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(10, 20, 30, 40),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(5, 50, 25, 35),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(50, 50, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
}

TEST(LazyPixelRefUtilsTest, DrawRRect) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  SkCanvas* canvas = StartRecording(picture.get(), layer_rect);

  TestLazyShader first_shader;
  SkPaint first_paint;
  first_paint.setShader(&first_shader);

  TestLazyShader second_shader;
  SkPaint second_paint;
  second_paint.setShader(&second_shader);

  TestLazyShader third_shader;
  SkPaint third_paint;
  third_paint.setShader(&third_shader);

  SkRRect rrect;
  rrect.setRect(SkRect::MakeXYWH(10, 20, 30, 40));

  // (10, 20, 30, 40).
  canvas->drawRRect(rrect, first_paint);

  canvas->save();

  canvas->translate(5, 17);
  rrect.setRect(SkRect::MakeXYWH(0, 33, 25, 35));
  // (5, 50, 25, 35)
  canvas->drawRRect(rrect, second_paint);

  canvas->restore();

  canvas->clipRect(SkRect::MakeXYWH(50, 50, 50, 50));
  canvas->translate(20, 20);
  rrect.setRect(SkRect::MakeXYWH(0, 0, 100, 100));
  // (50, 50, 50, 50)
  canvas->drawRRect(rrect, third_paint);

  std::vector<skia::LazyPixelRefUtils::PositionLazyPixelRef> pixel_refs;
  skia::LazyPixelRefUtils::GatherPixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(3u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(10, 20, 30, 40),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(5, 50, 25, 35),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(50, 50, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
}

TEST(LazyPixelRefUtilsTest, DrawOval) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  SkCanvas* canvas = StartRecording(picture.get(), layer_rect);

  TestLazyShader first_shader;
  SkPaint first_paint;
  first_paint.setShader(&first_shader);

  TestLazyShader second_shader;
  SkPaint second_paint;
  second_paint.setShader(&second_shader);

  TestLazyShader third_shader;
  SkPaint third_paint;
  third_paint.setShader(&third_shader);

  canvas->save();

  canvas->scale(2, 0.5);
  // (20, 10, 60, 20).
  canvas->drawOval(SkRect::MakeXYWH(10, 20, 30, 40), first_paint);

  canvas->restore();
  canvas->save();

  canvas->translate(1, 2);
  // (1, 35, 25, 35)
  canvas->drawRect(SkRect::MakeXYWH(0, 33, 25, 35), second_paint);

  canvas->restore();

  canvas->clipRect(SkRect::MakeXYWH(50, 50, 50, 50));
  canvas->translate(20, 20);
  // (50, 50, 50, 50)
  canvas->drawRect(SkRect::MakeXYWH(0, 0, 100, 100), third_paint);

  std::vector<skia::LazyPixelRefUtils::PositionLazyPixelRef> pixel_refs;
  skia::LazyPixelRefUtils::GatherPixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(3u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(20, 10, 60, 20),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(1, 35, 25, 35),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(50, 50, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
}

TEST(LazyPixelRefUtilsTest, DrawPath) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  SkCanvas* canvas = StartRecording(picture.get(), layer_rect);

  TestLazyShader first_shader;
  SkPaint first_paint;
  first_paint.setShader(&first_shader);

  TestLazyShader second_shader;
  SkPaint second_paint;
  second_paint.setShader(&second_shader);

  SkPath path;
  path.moveTo(12, 13);
  path.lineTo(50, 50);
  path.lineTo(22, 101);

  // (12, 13, 38, 88).
  canvas->drawPath(path, first_paint);

  canvas->save();
  canvas->clipRect(SkRect::MakeWH(50, 50));

  // (12, 13, 38, 37).
  canvas->drawPath(path, second_paint);

  canvas->restore();

  StopRecording(picture.get(), canvas);

  std::vector<skia::LazyPixelRefUtils::PositionLazyPixelRef> pixel_refs;
  skia::LazyPixelRefUtils::GatherPixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(2u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(12, 13, 38, 88),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(12, 13, 38, 37),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
}

TEST(LazyPixelRefUtilsTest, DrawBitmap) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  SkCanvas* canvas = StartRecording(picture.get(), layer_rect);

  SkBitmap first;
  CreateBitmap(gfx::Size(50, 50), "lazy", &first);
  SkBitmap second;
  CreateBitmap(gfx::Size(50, 50), "lazy", &second);
  SkBitmap third;
  CreateBitmap(gfx::Size(50, 50), "lazy", &third);
  SkBitmap fourth;
  CreateBitmap(gfx::Size(50, 1), "lazy", &fourth);
  SkBitmap fifth;
  CreateBitmap(gfx::Size(10, 10), "lazy", &fifth);

  canvas->save();

  // At (0, 0).
  canvas->drawBitmap(first, 0, 0);
  canvas->translate(25, 0);
  // At (25, 0).
  canvas->drawBitmap(second, 0, 0);
  canvas->translate(0, 50);
  // At (50, 50).
  canvas->drawBitmap(third, 25, 0);

  canvas->restore();
  canvas->save();

  canvas->rotate(90);
  // At (0, 0), rotated 90 degrees
  canvas->drawBitmap(fourth, 0, 0);

  canvas->restore();

  canvas->scale(5, 6);
  // At (0, 0), scaled by 5 and 6
  canvas->drawBitmap(fifth, 0, 0);

  StopRecording(picture.get(), canvas);

  std::vector<skia::LazyPixelRefUtils::PositionLazyPixelRef> pixel_refs;
  skia::LazyPixelRefUtils::GatherPixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(5u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(25, 0, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(50, 50, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(-1, 0, 1, 50),
                       gfx::SkRectToRectF(pixel_refs[3].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 50, 60),
                       gfx::SkRectToRectF(pixel_refs[4].pixel_ref_rect));

}

TEST(LazyPixelRefUtilsTest, DrawBitmapRect) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  SkCanvas* canvas = StartRecording(picture.get(), layer_rect);

  SkBitmap first;
  CreateBitmap(gfx::Size(50, 50), "lazy", &first);
  SkBitmap second;
  CreateBitmap(gfx::Size(50, 50), "lazy", &second);
  SkBitmap third;
  CreateBitmap(gfx::Size(50, 50), "lazy", &third);

  TestLazyShader first_shader;
  SkPaint first_paint;
  first_paint.setShader(&first_shader);

  SkPaint non_lazy_paint;

  canvas->save();

  // (0, 0, 100, 100).
  canvas->drawBitmapRect(first, SkRect::MakeWH(100, 100), &non_lazy_paint);
  canvas->translate(25, 0);
  // (75, 50, 10, 10).
  canvas->drawBitmapRect(
      second, SkRect::MakeXYWH(50, 50, 10, 10), &non_lazy_paint);
  canvas->translate(5, 50);
  // (0, 30, 100, 100). One from bitmap, one from paint.
  canvas->drawBitmapRect(
      third, SkRect::MakeXYWH(-30, -20, 100, 100), &first_paint);

  canvas->restore();

  StopRecording(picture.get(), canvas);

  std::vector<skia::LazyPixelRefUtils::PositionLazyPixelRef> pixel_refs;
  skia::LazyPixelRefUtils::GatherPixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(4u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 100, 100),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(75, 50, 10, 10),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 30, 100, 100),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 30, 100, 100),
                       gfx::SkRectToRectF(pixel_refs[3].pixel_ref_rect));
}

TEST(LazyPixelRefUtilsTest, DrawSprite) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  SkCanvas* canvas = StartRecording(picture.get(), layer_rect);

  SkBitmap first;
  CreateBitmap(gfx::Size(50, 50), "lazy", &first);
  SkBitmap second;
  CreateBitmap(gfx::Size(50, 50), "lazy", &second);
  SkBitmap third;
  CreateBitmap(gfx::Size(50, 50), "lazy", &third);
  SkBitmap fourth;
  CreateBitmap(gfx::Size(50, 50), "lazy", &fourth);
  SkBitmap fifth;
  CreateBitmap(gfx::Size(50, 50), "lazy", &fifth);

  canvas->save();

  // Sprites aren't affected by the current matrix.

  // (0, 0, 50, 50).
  canvas->drawSprite(first, 0, 0);
  canvas->translate(25, 0);
  // (10, 0, 50, 50).
  canvas->drawSprite(second, 10, 0);
  canvas->translate(0, 50);
  // (25, 0, 50, 50).
  canvas->drawSprite(third, 25, 0);

  canvas->restore();
  canvas->save();

  canvas->rotate(90);
  // (0, 0, 50, 50).
  canvas->drawSprite(fourth, 0, 0);

  canvas->restore();

  TestLazyShader first_shader;
  SkPaint first_paint;
  first_paint.setShader(&first_shader);

  canvas->scale(5, 6);
  // (100, 100, 50, 50).
  canvas->drawSprite(fifth, 100, 100, &first_paint);

  StopRecording(picture.get(), canvas);

  std::vector<skia::LazyPixelRefUtils::PositionLazyPixelRef> pixel_refs;
  skia::LazyPixelRefUtils::GatherPixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(6u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(10, 0, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(25, 0, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[3].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(100, 100, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[4].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(100, 100, 50, 50),
                       gfx::SkRectToRectF(pixel_refs[5].pixel_ref_rect));
}

TEST(LazyPixelRefUtilsTest, DrawText) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  SkCanvas* canvas = StartRecording(picture.get(), layer_rect);

  TestLazyShader first_shader;
  SkPaint first_paint;
  first_paint.setShader(&first_shader);

  SkPoint points[4];
  points[0].set(10, 50);
  points[1].set(20, 50);
  points[2].set(30, 50);
  points[3].set(40, 50);

  SkPath path;
  path.moveTo(10, 50);
  path.lineTo(20, 50);
  path.lineTo(30, 50);
  path.lineTo(40, 50);
  path.lineTo(50, 50);

  canvas->drawText("text", 4, 50, 50, first_paint);
  canvas->drawPosText("text", 4, points, first_paint);
  canvas->drawTextOnPath("text", 4, path, NULL, first_paint);

  std::vector<skia::LazyPixelRefUtils::PositionLazyPixelRef> pixel_refs;
  skia::LazyPixelRefUtils::GatherPixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(3u, pixel_refs.size());
}

TEST(LazyPixelRefUtilsTest, DrawVertices) {
  gfx::Rect layer_rect(0, 0, 256, 256);

  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  SkCanvas* canvas = StartRecording(picture.get(), layer_rect);

  TestLazyShader first_shader;
  SkPaint first_paint;
  first_paint.setShader(&first_shader);

  TestLazyShader second_shader;
  SkPaint second_paint;
  second_paint.setShader(&second_shader);

  TestLazyShader third_shader;
  SkPaint third_paint;
  third_paint.setShader(&third_shader);

  SkPoint points[3];
  SkColor colors[3];
  uint16_t indecies[3] = {0, 1, 2};
  points[0].set(10, 10);
  points[1].set(100, 20);
  points[2].set(50, 100);
  // (10, 10, 90, 90).
  canvas->drawVertices(SkCanvas::kTriangles_VertexMode,
                       3,
                       points,
                       points,
                       colors,
                       NULL,
                       indecies,
                       3,
                       first_paint);

  canvas->save();

  canvas->clipRect(SkRect::MakeWH(50, 50));
  // (10, 10, 40, 40).
  canvas->drawVertices(SkCanvas::kTriangles_VertexMode,
                       3,
                       points,
                       points,
                       colors,
                       NULL,
                       indecies,
                       3,
                       second_paint);

  canvas->restore();

  points[0].set(50, 55);
  points[1].set(50, 55);
  points[2].set(200, 200);
  // (50, 55, 150, 145).
  canvas->drawVertices(SkCanvas::kTriangles_VertexMode,
                       3,
                       points,
                       points,
                       colors,
                       NULL,
                       indecies,
                       3,
                       third_paint);

  StopRecording(picture.get(), canvas);

  std::vector<skia::LazyPixelRefUtils::PositionLazyPixelRef> pixel_refs;
  skia::LazyPixelRefUtils::GatherPixelRefs(picture.get(), &pixel_refs);

  EXPECT_EQ(3u, pixel_refs.size());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(10, 10, 90, 90),
                       gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(10, 10, 40, 40),
                       gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect));
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(50, 55, 150, 145),
                       gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect));
}

}  // namespace skia
