// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas2d/CanvasRenderingContext2D.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/ImageData.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/canvas2d/CanvasGradient.h"
#include "modules/canvas2d/CanvasPattern.h"
#include "modules/canvas2d/HitRegionOptions.h"
#include "modules/canvas2d/Path2D.h"
#include "modules/webgl/WebGLRenderingContext.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"

using ::testing::Mock;

namespace blink {
namespace UsageTrackingTests {

enum BitmapOpacity { kOpaqueBitmap, kTransparentBitmap };

class FakeImageSource : public CanvasImageSource {
 public:
  FakeImageSource(IntSize, BitmapOpacity);

  PassRefPtr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                            AccelerationHint,
                                            SnapshotReason,
                                            const FloatSize&) const override;

  bool WouldTaintOrigin(
      SecurityOrigin* destination_security_origin) const override {
    return false;
  }
  FloatSize ElementSize(const FloatSize&) const override {
    return FloatSize(size_);
  }
  bool IsOpaque() const override { return is_opaque_; }
  bool IsAccelerated() const override { return false; }
  int SourceWidth() override { return size_.Width(); }
  int SourceHeight() override { return size_.Height(); }

  ~FakeImageSource() override {}

 private:
  IntSize size_;
  RefPtr<Image> image_;
  bool is_opaque_;
};

FakeImageSource::FakeImageSource(IntSize size, BitmapOpacity opacity)
    : size_(size), is_opaque_(opacity == kOpaqueBitmap) {
  sk_sp<SkSurface> surface(
      SkSurface::MakeRasterN32Premul(size_.Width(), size_.Height()));
  surface->getCanvas()->clear(opacity == kOpaqueBitmap ? SK_ColorWHITE
                                                       : SK_ColorTRANSPARENT);
  image_ = StaticBitmapImage::Create(surface->makeImageSnapshot());
}

PassRefPtr<Image> FakeImageSource::GetSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint,
    SnapshotReason,
    const FloatSize&) const {
  if (status)
    *status = kNormalSourceImageStatus;
  return image_;
}

class CanvasRenderingContextUsageTrackingTest : public ::testing::Test {
 protected:
  CanvasRenderingContextUsageTrackingTest();
  void SetUp() override;

  DummyPageHolder& Page() const { return *dummy_page_holder_; }
  Document& GetDocument() const { return *document_; }
  HTMLCanvasElement& CanvasElement() const { return *canvas_element_; }
  CanvasRenderingContext2D* Context2d() const;

  void CreateContext(OpacityMode);

  FakeImageSource opaque_bitmap_;
  FakeImageSource alpha_bitmap_;
  Persistent<ImageData> full_image_data_;
  void TearDown();

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(canvas_element_->GetFrame());
  }

 private:
  std::shared_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<Document> document_;
  Persistent<HTMLCanvasElement> canvas_element_;
  Persistent<MemoryCache> global_memory_cache_;
};

CanvasRenderingContextUsageTrackingTest::
    CanvasRenderingContextUsageTrackingTest()
    : opaque_bitmap_(IntSize(10, 10), kOpaqueBitmap),
      alpha_bitmap_(IntSize(10, 10), kTransparentBitmap) {}

void CanvasRenderingContextUsageTrackingTest::TearDown() {
  ThreadState::Current()->CollectGarbage(BlinkGC::kNoHeapPointersOnStack,
                                         BlinkGC::kGCWithSweep,
                                         BlinkGC::kForcedGC);
  ReplaceMemoryCacheForTesting(global_memory_cache_.Release());
}

CanvasRenderingContext2D* CanvasRenderingContextUsageTrackingTest::Context2d()
    const {
  // If the following check fails, perhaps you forgot to call createContext
  // in your test?
  EXPECT_NE(nullptr, CanvasElement().RenderingContext());
  EXPECT_TRUE(CanvasElement().RenderingContext()->Is2d());
  return static_cast<CanvasRenderingContext2D*>(
      CanvasElement().RenderingContext());
}

void CanvasRenderingContextUsageTrackingTest::CreateContext(
    OpacityMode opacity_mode) {
  String canvas_type("2d");
  CanvasContextCreationAttributes attributes;
  attributes.setAlpha(opacity_mode == kNonOpaque);
  canvas_element_->GetCanvasRenderingContext(canvas_type, attributes);
  Context2d();  // Calling this for the checks
}

void CanvasRenderingContextUsageTrackingTest::SetUp() {
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  dummy_page_holder_ =
      DummyPageHolder::Create(IntSize(800, 600), &page_clients);
  document_ = &dummy_page_holder_->GetDocument();
  document_->documentElement()->setInnerHTML(
      "<body><canvas id='c'></canvas></body>");
  document_->View()->UpdateAllLifecyclePhases();
  canvas_element_ = toHTMLCanvasElement(document_->GetElementById("c"));
  full_image_data_ = ImageData::Create(IntSize(10, 10));

  global_memory_cache_ = ReplaceMemoryCacheForTesting(MemoryCache::Create());

  RuntimeEnabledFeatures::setEnableCanvas2dDynamicRenderingModeSwitchingEnabled(
      true);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, FillTracking) {
  CreateContext(kNonOpaque);

  int num_reps = 100;
  for (int i = 0; i < num_reps; i++) {
    Context2d()->beginPath();
    Context2d()->moveTo(1, 1);
    Context2d()->lineTo(4, 1);
    Context2d()->lineTo(4, 4);
    Context2d()->lineTo(2, 2);
    Context2d()->lineTo(1, 4);
    Context2d()->fill();  // path is not convex

    Context2d()->fillText("Hello World!!!", 10, 10, 1);
    Context2d()->fillRect(10, 10, 100, 100);
    Context2d()->fillText("Hello World!!!", 10, 10);

    Context2d()->beginPath();
    Context2d()->moveTo(1, 1);
    Context2d()->lineTo(4, 1);
    Context2d()->lineTo(4, 4);
    Context2d()->lineTo(1, 4);
    Context2d()->fill();  // path is convex

    Context2d()->fillRect(10, 10, 100, 100);
  }

  EXPECT_EQ(num_reps * 2,
            Context2d()
                ->GetUsage()
                .num_draw_calls[BaseRenderingContext2D::kFillPath]);
  EXPECT_EQ(num_reps * 2,
            Context2d()
                ->GetUsage()
                .num_draw_calls[BaseRenderingContext2D::kFillRect]);
  EXPECT_EQ(num_reps, Context2d()->GetUsage().num_non_convex_fill_path_calls);
  EXPECT_EQ(num_reps * 2,
            Context2d()
                ->GetUsage()
                .num_draw_calls[BaseRenderingContext2D::kFillText]);

  // areas
  EXPECT_NEAR(
      static_cast<double>(num_reps * (100 * 100) * 2),
      Context2d()
          ->GetUsage()
          .bounding_box_area_draw_calls[BaseRenderingContext2D::kFillRect],
      1.0);
  EXPECT_NEAR(
      static_cast<double>(num_reps * (2 * 100 + 2 * 100) * 2),
      Context2d()
          ->GetUsage()
          .bounding_box_perimeter_draw_calls[BaseRenderingContext2D::kFillRect],
      1.0);

  EXPECT_TRUE(
      static_cast<double>(num_reps * (100 * 100) * 2) <
      Context2d()
          ->GetUsage()
          .bounding_box_area_fill_type[BaseRenderingContext2D::kColorFillType]);
  EXPECT_TRUE(
      static_cast<double>(num_reps * (100 * 100) * 2) * 1.1 >
      Context2d()
          ->GetUsage()
          .bounding_box_area_fill_type[BaseRenderingContext2D::kColorFillType]);

  EXPECT_NEAR(
      static_cast<double>(num_reps * (3 * 3) * 2),
      Context2d()
          ->GetUsage()
          .bounding_box_area_draw_calls[BaseRenderingContext2D::kFillPath],
      1.0);
  EXPECT_NEAR(
      static_cast<double>(num_reps * (2 * 3 + 2 * 3) * 2),
      Context2d()
          ->GetUsage()
          .bounding_box_perimeter_draw_calls[BaseRenderingContext2D::kFillPath],
      1.0);

  EXPECT_TRUE(Context2d()->GetUsage().bounding_box_perimeter_draw_calls
                  [BaseRenderingContext2D::kFillText] > 0.0);
  EXPECT_TRUE(Context2d()->GetUsage().bounding_box_perimeter_draw_calls
                  [BaseRenderingContext2D::kStrokeText] == 0.0);

  // create gradient
  CanvasGradient* gradient;
  gradient = Context2d()->createLinearGradient(0, 0, 100, 100);
  Context2d()->setFillStyle(
      StringOrCanvasGradientOrCanvasPattern::fromCanvasGradient(gradient));
  Context2d()->fillRect(10, 10, 100, 20);
  EXPECT_EQ(1, Context2d()->GetUsage().num_linear_gradients);
  EXPECT_NEAR(100 * 20,
              Context2d()->GetUsage().bounding_box_area_fill_type
                  [BaseRenderingContext2D::kLinearGradientFillType],
              1.0);

  NonThrowableExceptionState exception_state;
  gradient = Context2d()->createRadialGradient(0, 0, 100, 100, 200, 200,
                                               exception_state);
  Context2d()->setFillStyle(
      StringOrCanvasGradientOrCanvasPattern::fromCanvasGradient(gradient));
  Context2d()->fillRect(10, 10, 100, 20);
  EXPECT_EQ(1, Context2d()->GetUsage().num_linear_gradients);
  EXPECT_EQ(1, Context2d()->GetUsage().num_radial_gradients);
  EXPECT_NEAR(100 * 20,
              Context2d()->GetUsage().bounding_box_area_fill_type
                  [BaseRenderingContext2D::kRadialGradientFillType],
              1.0);

  // create pattern
  NonThrowableExceptionState exception_state2;
  CanvasPattern* canvas_pattern = Context2d()->createPattern(
      GetScriptState(), &opaque_bitmap_, "repeat-x", exception_state2);
  StringOrCanvasGradientOrCanvasPattern pattern =
      StringOrCanvasGradientOrCanvasPattern::fromCanvasPattern(canvas_pattern);
  Context2d()->setFillStyle(pattern);
  Context2d()->fillRect(10, 10, 200, 100);
  EXPECT_EQ(1, Context2d()->GetUsage().num_patterns);
  EXPECT_NEAR(200 * 100,
              Context2d()->GetUsage().bounding_box_area_fill_type
                  [BaseRenderingContext2D::kPatternFillType],
              1.0);

  // Other fill method
  Context2d()->fill(Path2D::Create("Hello World!!!"));
  EXPECT_EQ(num_reps * 2 + 1,
            Context2d()
                ->GetUsage()
                .num_draw_calls[BaseRenderingContext2D::kFillPath]);

  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kStrokePath]);
  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kStrokeText]);
  EXPECT_EQ(0, Context2d()->GetUsage().num_put_image_data_calls);
  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kDrawVectorImage]);
  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kDrawBitmapImage]);
  EXPECT_EQ(0, Context2d()->GetUsage().num_get_image_data_calls);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, StrokeTracking) {
  CreateContext(kNonOpaque);

  int num_reps = 100;
  for (int i = 0; i < num_reps; i++) {
    Context2d()->strokeRect(10, 10, 100, 100);

    Context2d()->beginPath();
    Context2d()->moveTo(1, 1);
    Context2d()->lineTo(4, 1);
    Context2d()->lineTo(4, 4);
    Context2d()->lineTo(2, 2);
    Context2d()->lineTo(1, 4);
    Context2d()->stroke();

    Context2d()->strokeText("Hello World!!!", 10, 10, 1);

    Context2d()->strokeRect(10, 10, 100, 100);
    Context2d()->strokeText("Hello World!!!", 10, 10);
  }

  EXPECT_EQ(num_reps, Context2d()
                          ->GetUsage()
                          .num_draw_calls[BaseRenderingContext2D::kStrokePath]);
  EXPECT_EQ(num_reps * 2,
            Context2d()
                ->GetUsage()
                .num_draw_calls[BaseRenderingContext2D::kStrokeRect]);
  EXPECT_EQ(num_reps * 2,
            Context2d()
                ->GetUsage()
                .num_draw_calls[BaseRenderingContext2D::kStrokeText]);

  EXPECT_NEAR(
      static_cast<double>(num_reps * (100 * 100) * 2),
      Context2d()
          ->GetUsage()
          .bounding_box_area_draw_calls[BaseRenderingContext2D::kStrokeRect],
      1.0);
  EXPECT_NEAR(static_cast<double>(num_reps * (2 * 100 + 2 * 100) * 2),
              Context2d()->GetUsage().bounding_box_perimeter_draw_calls
                  [BaseRenderingContext2D::kStrokeRect],
              1.0);
  EXPECT_NEAR(
      static_cast<double>(num_reps * (3 * 3)),
      Context2d()
          ->GetUsage()
          .bounding_box_area_draw_calls[BaseRenderingContext2D::kStrokePath],
      1.0);
  EXPECT_NEAR(static_cast<double>(num_reps * (2 * 3 + 2 * 3)),
              Context2d()->GetUsage().bounding_box_perimeter_draw_calls
                  [BaseRenderingContext2D::kStrokePath],
              1.0);

  // create gradient
  CanvasGradient* gradient;
  gradient = Context2d()->createLinearGradient(0, 0, 100, 100);
  Context2d()->setStrokeStyle(
      StringOrCanvasGradientOrCanvasPattern::fromCanvasGradient(gradient));
  Context2d()->strokeRect(10, 10, 100, 100);
  EXPECT_EQ(1, Context2d()->GetUsage().num_linear_gradients);
  EXPECT_NEAR(100 * 100,
              Context2d()->GetUsage().bounding_box_area_fill_type
                  [BaseRenderingContext2D::kLinearGradientFillType],
              1.0);

  // create pattern
  NonThrowableExceptionState exception_state;
  CanvasPattern* canvas_pattern = Context2d()->createPattern(
      GetScriptState(), &opaque_bitmap_, "repeat-x", exception_state);
  StringOrCanvasGradientOrCanvasPattern pattern =
      StringOrCanvasGradientOrCanvasPattern::fromCanvasPattern(canvas_pattern);
  Context2d()->setStrokeStyle(pattern);
  Context2d()->strokeRect(10, 10, 100, 100);
  EXPECT_EQ(1, Context2d()->GetUsage().num_patterns);
  EXPECT_NEAR(100 * 100,
              Context2d()->GetUsage().bounding_box_area_fill_type
                  [BaseRenderingContext2D::kPatternFillType],
              1.0);

  // Other stroke method
  Context2d()->stroke(Path2D::Create("Hello World!!!"));
  EXPECT_EQ(num_reps + 1,
            Context2d()
                ->GetUsage()
                .num_draw_calls[BaseRenderingContext2D::kStrokePath]);
  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kFillPath]);
  EXPECT_EQ(0, Context2d()->GetUsage().num_non_convex_fill_path_calls);
  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kFillText]);
  EXPECT_EQ(0, Context2d()->GetUsage().num_put_image_data_calls);
  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kDrawVectorImage]);
  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kDrawBitmapImage]);
  EXPECT_EQ(0, Context2d()->GetUsage().num_get_image_data_calls);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, ImageTracking) {
  // Testing draw, put, get and filters
  CreateContext(kNonOpaque);
  NonThrowableExceptionState exception_state;

  int num_reps = 5;
  int img_width = 100;
  int img_height = 200;
  for (int i = 0; i < num_reps; i++) {
    Context2d()->putImageData(full_image_data_.Get(), 0, 0, exception_state);
    Context2d()->drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 1, 1, 0, 0,
                           img_width, img_height, exception_state);
    Context2d()->getImageData(0, 0, 10, 100, exception_state);
  }

  EXPECT_NEAR(num_reps * img_width * img_height,
              Context2d()->GetUsage().bounding_box_area_draw_calls
                  [BaseRenderingContext2D::kDrawBitmapImage],
              0.1);
  EXPECT_NEAR(num_reps * (2 * img_width + 2 * img_height),
              Context2d()->GetUsage().bounding_box_perimeter_draw_calls
                  [BaseRenderingContext2D::kDrawBitmapImage],
              0.1);

  EXPECT_NEAR(0.0,
              Context2d()->GetUsage().bounding_box_area_draw_calls
                  [BaseRenderingContext2D::kDrawVectorImage],
              0.1);
  EXPECT_NEAR(0.0,
              Context2d()->GetUsage().bounding_box_perimeter_draw_calls
                  [BaseRenderingContext2D::kDrawVectorImage],
              0.1);

  Context2d()->setFilter("blur(5px)");
  Context2d()->drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 1, 1, 0, 0,
                         10, 10, exception_state);

  EXPECT_EQ(num_reps, Context2d()->GetUsage().num_put_image_data_calls);
  EXPECT_NE(0, Context2d()->GetUsage().area_put_image_data_calls);
  EXPECT_NEAR(num_reps * full_image_data_.Get()->width() *
                  full_image_data_.Get()->height(),
              Context2d()->GetUsage().area_put_image_data_calls, 0.1);

  EXPECT_EQ(num_reps + 1,
            Context2d()
                ->GetUsage()
                .num_draw_calls[BaseRenderingContext2D::kDrawBitmapImage]);
  EXPECT_EQ(num_reps, Context2d()->GetUsage().num_get_image_data_calls);

  EXPECT_EQ(1, Context2d()->GetUsage().num_filters);
  EXPECT_NEAR(num_reps * 10 * 100,
              Context2d()->GetUsage().area_get_image_data_calls, 0.1);

  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kFillPath]);
  EXPECT_EQ(0, Context2d()->GetUsage().num_non_convex_fill_path_calls);
  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kFillText]);
  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kStrokePath]);
  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kStrokeText]);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, ClearRectTracking) {
  CreateContext(kNonOpaque);
  Context2d()->clearRect(0, 0, 1, 1);
  Context2d()->clearRect(0, 0, 2, 2);
  EXPECT_EQ(2, Context2d()->GetUsage().num_clear_rect_calls);

  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kFillPath]);
  EXPECT_EQ(0, Context2d()->GetUsage().num_non_convex_fill_path_calls);
  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kFillText]);
  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kStrokePath]);
  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kStrokeText]);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, ComplexClipsTracking) {
  CreateContext(kNonOpaque);
  Context2d()->save();

  // simple clip
  Context2d()->beginPath();
  Context2d()->moveTo(1, 1);
  Context2d()->lineTo(4, 1);
  Context2d()->lineTo(4, 4);
  Context2d()->lineTo(1, 4);
  Context2d()->clip("nonzero");
  Context2d()->fillRect(0, 0, 1, 1);
  EXPECT_EQ(0, Context2d()->GetUsage().num_draw_with_complex_clips);

  // complex clips
  int num_reps = 5;
  for (int i = 0; i < num_reps; i++) {
    Context2d()->beginPath();
    Context2d()->moveTo(1, 1);
    Context2d()->lineTo(4, 1);
    Context2d()->lineTo(4, 4);
    Context2d()->lineTo(2, 2);
    Context2d()->lineTo(1, 4);
    Context2d()->clip("nonzero");
    Context2d()->fillRect(0, 0, 1, 1);
  }

  // remove all previous clips
  Context2d()->restore();

  // simple clip again
  Context2d()->beginPath();
  Context2d()->moveTo(1, 1);
  Context2d()->lineTo(4, 1);
  Context2d()->lineTo(4, 4);
  Context2d()->lineTo(1, 4);
  Context2d()->clip("nonzero");
  Context2d()->fillRect(0, 0, 1, 1);

  EXPECT_EQ(num_reps, Context2d()->GetUsage().num_draw_with_complex_clips);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, ShadowsTracking) {
  CreateContext(kNonOpaque);

  Context2d()->beginPath();
  Context2d()->fillRect(0, 0, 1, 1);

  // shadow with blur = 0
  Context2d()->beginPath();
  Context2d()->setShadowBlur(0.0);
  Context2d()->setShadowColor("rgba(255, 0, 0, 0.5)");
  Context2d()->setShadowOffsetX(1.0);
  Context2d()->setShadowOffsetY(1.0);
  Context2d()->fillRect(0, 0, 1, 1);

  // shadow with alpha = 0
  Context2d()->beginPath();
  Context2d()->setShadowBlur(0.5);
  Context2d()->setShadowColor("rgba(255, 0, 0, 0)");
  Context2d()->setShadowOffsetX(1.0);
  Context2d()->setShadowOffsetY(1.0);
  Context2d()->fillRect(0, 0, 1, 1);

  EXPECT_EQ(0, Context2d()->GetUsage().num_blurred_shadows);
  EXPECT_NEAR(
      0.0, Context2d()->GetUsage().bounding_box_area_times_shadow_blur_squared,
      0.1);
  EXPECT_NEAR(
      0.0,
      Context2d()->GetUsage().bounding_box_perimeter_times_shadow_blur_squared,
      0.1);

  int num_reps = 5;
  double shadow_blur = 0.5;
  for (int i = 0; i < num_reps; i++) {
    Context2d()->beginPath();
    Context2d()->setShadowBlur(shadow_blur);
    Context2d()->setShadowColor("rgba(255, 0, 0, 0.5)");
    Context2d()->setShadowOffsetX(1.0);
    Context2d()->setShadowOffsetY(1.0);
    Context2d()->fillRect(0, 0, 3, 10);
  }
  EXPECT_EQ(num_reps, Context2d()->GetUsage().num_blurred_shadows);

  double area_times_shadow_blur_squared =
      shadow_blur * shadow_blur * num_reps * 3 * 10;
  double perimeter_times_shadow_blur_squared =
      shadow_blur * shadow_blur * num_reps * (2 * 3 + 2 * 10);
  EXPECT_NEAR(
      area_times_shadow_blur_squared,
      Context2d()->GetUsage().bounding_box_area_times_shadow_blur_squared, 0.1);
  EXPECT_NEAR(
      perimeter_times_shadow_blur_squared,
      Context2d()->GetUsage().bounding_box_perimeter_times_shadow_blur_squared,
      0.1);

  double shadow_blur2 = 4.5;
  Context2d()->beginPath();
  Context2d()->setShadowBlur(shadow_blur2);
  Context2d()->moveTo(1, 1);
  Context2d()->lineTo(4, 1);
  Context2d()->lineTo(4, 4);
  Context2d()->lineTo(2, 2);
  Context2d()->lineTo(1, 4);
  Context2d()->fill();

  area_times_shadow_blur_squared += 3.0 * 3.0 * shadow_blur2 * shadow_blur2;
  perimeter_times_shadow_blur_squared +=
      (2 * 3 + 2 * 3) * shadow_blur2 * shadow_blur2;
  EXPECT_NEAR(
      area_times_shadow_blur_squared,
      Context2d()->GetUsage().bounding_box_area_times_shadow_blur_squared, 0.1);
  EXPECT_NEAR(
      perimeter_times_shadow_blur_squared,
      Context2d()->GetUsage().bounding_box_perimeter_times_shadow_blur_squared,
      0.1);

  NonThrowableExceptionState exception_state;
  int img_width = 100;
  int img_height = 200;
  for (int i = 0; i < num_reps; i++) {
    Context2d()->putImageData(full_image_data_.Get(), 0, 0, exception_state);
    Context2d()->setShadowBlur(shadow_blur2);
    Context2d()->drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 1, 1, 0, 0,
                           img_width, img_height, exception_state);
    Context2d()->getImageData(0, 0, 1, 1, exception_state);
  }

  area_times_shadow_blur_squared +=
      img_width * img_height * shadow_blur2 * shadow_blur2 * num_reps;
  perimeter_times_shadow_blur_squared +=
      (2 * img_width + 2 * img_height) * shadow_blur2 * shadow_blur2 * num_reps;
  EXPECT_NEAR(
      area_times_shadow_blur_squared,
      Context2d()->GetUsage().bounding_box_area_times_shadow_blur_squared, 0.1);
  EXPECT_NEAR(
      perimeter_times_shadow_blur_squared,
      Context2d()->GetUsage().bounding_box_perimeter_times_shadow_blur_squared,
      0.1);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, incrementFrameCount) {
  CreateContext(kNonOpaque);
  EXPECT_EQ(0, Context2d()->GetUsage().num_frames_since_reset);

  Context2d()->FinalizeFrame();
  EXPECT_EQ(1, Context2d()->GetUsage().num_frames_since_reset);

  Context2d()->FinalizeFrame();
  Context2d()->FinalizeFrame();
  EXPECT_EQ(3, Context2d()->GetUsage().num_frames_since_reset);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, resetUsageTracking) {
  CreateContext(kNonOpaque);
  EXPECT_EQ(0, Context2d()->GetUsage().num_frames_since_reset);

  Context2d()->FinalizeFrame();
  Context2d()->FinalizeFrame();
  EXPECT_EQ(2, Context2d()->GetUsage().num_frames_since_reset);

  const int kNumReps = 100;
  for (int i = 0; i < kNumReps; i++) {
    Context2d()->fillRect(10, 10, 100, 100);
  }
  EXPECT_EQ(kNumReps, Context2d()
                          ->GetUsage()
                          .num_draw_calls[BaseRenderingContext2D::kFillRect]);

  Context2d()->ResetUsageTracking();
  EXPECT_EQ(0, Context2d()->GetUsage().num_frames_since_reset);
  EXPECT_EQ(0, Context2d()
                   ->GetUsage()
                   .num_draw_calls[BaseRenderingContext2D::kFillRect]);

  for (int i = 0; i < kNumReps; i++) {
    Context2d()->fillRect(10, 10, 100, 100);
  }
  EXPECT_EQ(kNumReps, Context2d()
                          ->GetUsage()
                          .num_draw_calls[BaseRenderingContext2D::kFillRect]);
}

}  // namespace UsageTrackingTests
}  // namespace blink
