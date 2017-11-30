// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas/canvas2d/CanvasRenderingContext2D.h"

#include <memory>
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/ImageData.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/loader/EmptyClients.h"
#include "core/paint/PaintLayer.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/canvas/canvas2d/CanvasGradient.h"
#include "modules/canvas/canvas2d/CanvasPattern.h"
#include "modules/webgl/WebGLRenderingContext.h"
#include "platform/graphics/CanvasHeuristicParameters.h"
#include "platform/graphics/ColorCorrectionTestUtils.h"
#include "platform/graphics/RecordingImageBufferSurface.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/TestingPlatformSupportWithMockScheduler.h"
#include "platform/wtf/ByteSwap.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/common/page/page_visibility_state.mojom-blink.h"
#include "third_party/skia/include/core/SkColorSpaceXform.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSwizzle.h"

using ::testing::Mock;

namespace blink {

enum BitmapOpacity { kOpaqueBitmap, kTransparentBitmap };

class FakeImageSource : public CanvasImageSource {
 public:
  FakeImageSource(IntSize, BitmapOpacity);

  scoped_refptr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                               AccelerationHint,
                                               SnapshotReason,
                                               const FloatSize&) override;

  bool WouldTaintOrigin(
      SecurityOrigin* destination_security_origin) const override {
    return false;
  }
  FloatSize ElementSize(const FloatSize&) const override {
    return FloatSize(size_);
  }
  bool IsOpaque() const override { return is_opaque_; }
  bool IsAccelerated() const { return false; }

  ~FakeImageSource() override {}

 private:
  IntSize size_;
  scoped_refptr<Image> image_;
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

scoped_refptr<Image> FakeImageSource::GetSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint,
    SnapshotReason,
    const FloatSize&) {
  if (status)
    *status = kNormalSourceImageStatus;
  return image_;
}

//============================================================================

enum LinearPixelMathState { kLinearPixelMathDisabled, kLinearPixelMathEnabled };

class CanvasRenderingContext2DTest : public ::testing::Test {
 protected:
  CanvasRenderingContext2DTest();
  void SetUp() override;

  DummyPageHolder& Page() const { return *dummy_page_holder_; }
  Document& GetDocument() const { return *document_; }
  HTMLCanvasElement& CanvasElement() const { return *canvas_element_; }
  CanvasRenderingContext2D* Context2d() const {
    return static_cast<CanvasRenderingContext2D*>(
        CanvasElement().RenderingContext());
  }
  intptr_t GetGlobalGPUMemoryUsage() const {
    return HTMLCanvasElement::GetGlobalGPUMemoryUsage();
  }
  unsigned GetGlobalAcceleratedContextCount() const {
    return HTMLCanvasElement::GetGlobalAcceleratedContextCount();
  }
  intptr_t GetCurrentGPUMemoryUsage() const {
    return CanvasElement().GetGPUMemoryUsage();
  }

  void CreateContext(OpacityMode,
                     String color_space = String(),
                     LinearPixelMathState = kLinearPixelMathDisabled);
  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(canvas_element_->GetFrame());
  }

  void TearDown() override;
  void UnrefCanvas();
  std::unique_ptr<Canvas2DLayerBridge> MakeBridge(
      const IntSize&,
      Canvas2DLayerBridge::AccelerationMode);

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<Document> document_;
  Persistent<HTMLCanvasElement> canvas_element_;
  Persistent<MemoryCache> global_memory_cache_;

  class WrapGradients final : public GarbageCollectedFinalized<WrapGradients> {
   public:
    static WrapGradients* Create() { return new WrapGradients; }

    void Trace(blink::Visitor* visitor) {
      visitor->Trace(opaque_gradient_);
      visitor->Trace(alpha_gradient_);
    }

    StringOrCanvasGradientOrCanvasPattern opaque_gradient_;
    StringOrCanvasGradientOrCanvasPattern alpha_gradient_;
  };

  // TODO(Oilpan): avoid tedious part-object wrapper by supporting on-heap
  // ::testing::Tests.
  Persistent<WrapGradients> wrap_gradients_;

 protected:
  // Pre-canned objects for testing
  Persistent<ImageData> full_image_data_;
  Persistent<ImageData> partial_image_data_;
  FakeImageSource opaque_bitmap_;
  FakeImageSource alpha_bitmap_;
  FakeGLES2Interface gl_;

  // Set this to override frame settings.
  FrameSettingOverrideFunction override_settings_function_ = nullptr;

  StringOrCanvasGradientOrCanvasPattern& OpaqueGradient() {
    return wrap_gradients_->opaque_gradient_;
  }
  StringOrCanvasGradientOrCanvasPattern& AlphaGradient() {
    return wrap_gradients_->alpha_gradient_;
  }
};

CanvasRenderingContext2DTest::CanvasRenderingContext2DTest()
    : wrap_gradients_(WrapGradients::Create()),
      opaque_bitmap_(IntSize(10, 10), kOpaqueBitmap),
      alpha_bitmap_(IntSize(10, 10), kTransparentBitmap) {}

void CanvasRenderingContext2DTest::CreateContext(
    OpacityMode opacity_mode,
    String color_space,
    LinearPixelMathState LinearPixelMath_state) {
  String canvas_type("2d");
  CanvasContextCreationAttributes attributes;
  attributes.setAlpha(opacity_mode == kNonOpaque);
  if (!color_space.IsEmpty()) {
    attributes.setColorSpace(color_space);
    if (LinearPixelMath_state == kLinearPixelMathEnabled) {
      attributes.setPixelFormat("float16");
    }
  }
  canvas_element_->GetCanvasRenderingContext(canvas_type, attributes);
}

void CanvasRenderingContext2DTest::SetUp() {
  auto factory = [](FakeGLES2Interface* gl, bool* gpu_compositing_disabled)
      -> std::unique_ptr<WebGraphicsContext3DProvider> {
    *gpu_compositing_disabled = false;
    gl->SetIsContextLost(false);
    return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
  };
  SharedGpuContext::SetContextProviderFactoryForTesting(
      WTF::Bind(factory, WTF::Unretained(&gl_)));

  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  dummy_page_holder_ = DummyPageHolder::Create(
      IntSize(800, 600), &page_clients, nullptr, override_settings_function_);
  document_ = &dummy_page_holder_->GetDocument();
  document_->documentElement()->SetInnerHTMLFromString(
      "<body><canvas id='c'></canvas><canvas id='d'></canvas></body>");
  document_->View()->UpdateAllLifecyclePhases();
  canvas_element_ = ToHTMLCanvasElement(document_->getElementById("c"));

  full_image_data_ = ImageData::Create(IntSize(10, 10));
  partial_image_data_ = ImageData::Create(IntSize(2, 2));

  NonThrowableExceptionState exception_state;
  CanvasGradient* opaque_gradient =
      CanvasGradient::Create(FloatPoint(0, 0), FloatPoint(10, 0));
  opaque_gradient->addColorStop(0, String("green"), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  opaque_gradient->addColorStop(1, String("blue"), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  this->OpaqueGradient().SetCanvasGradient(opaque_gradient);

  CanvasGradient* alpha_gradient =
      CanvasGradient::Create(FloatPoint(0, 0), FloatPoint(10, 0));
  alpha_gradient->addColorStop(0, String("green"), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  alpha_gradient->addColorStop(1, String("rgba(0, 0, 255, 0.5)"),
                               exception_state);
  EXPECT_FALSE(exception_state.HadException());
  StringOrCanvasGradientOrCanvasPattern wrapped_alpha_gradient;
  this->AlphaGradient().SetCanvasGradient(alpha_gradient);

  global_memory_cache_ = ReplaceMemoryCacheForTesting(MemoryCache::Create());
}

void CanvasRenderingContext2DTest::TearDown() {
  ThreadState::Current()->CollectGarbage(BlinkGC::kNoHeapPointersOnStack,
                                         BlinkGC::kGCWithSweep,
                                         BlinkGC::kForcedGC);
  ReplaceMemoryCacheForTesting(global_memory_cache_.Release());
  SharedGpuContext::ResetForTesting();
}

std::unique_ptr<Canvas2DLayerBridge> CanvasRenderingContext2DTest::MakeBridge(
    const IntSize& size,
    Canvas2DLayerBridge::AccelerationMode acceleration_mode) {
  std::unique_ptr<Canvas2DLayerBridge> bridge = WTF::WrapUnique(
      new Canvas2DLayerBridge(size, 0, acceleration_mode, CanvasColorParams()));
  bridge->SetCanvasResourceHost(canvas_element_);
  return bridge;
}

//============================================================================

class FakeAcceleratedImageBufferSurface
    : public UnacceleratedImageBufferSurface {
 public:
  FakeAcceleratedImageBufferSurface(const IntSize& size,
                                    CanvasColorParams color_params)
      : UnacceleratedImageBufferSurface(size,
                                        kInitializeImagePixels,
                                        color_params),
        is_accelerated_(true) {}
  ~FakeAcceleratedImageBufferSurface() override {}
  bool IsAccelerated() const override { return is_accelerated_; }
  void SetIsAccelerated(bool is_accelerated) {
    if (is_accelerated != is_accelerated_)
      is_accelerated_ = is_accelerated;
  }

 private:
  bool is_accelerated_;
};

//============================================================================

class MockImageBufferSurfaceForOverwriteTesting : public Canvas2DLayerBridge {
 public:
  MockImageBufferSurfaceForOverwriteTesting(const IntSize& size,
                                            CanvasColorParams color_params)
      : Canvas2DLayerBridge(size,
                            0,
                            Canvas2DLayerBridge::kDisableAcceleration,
                            color_params) {}
  ~MockImageBufferSurfaceForOverwriteTesting() override {}
  MOCK_METHOD0(WillOverwriteCanvas, void());
};

//============================================================================

#define TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS)                                \
  std::unique_ptr<MockImageBufferSurfaceForOverwriteTesting> mock_surface =    \
      std::make_unique<MockImageBufferSurfaceForOverwriteTesting>(             \
          IntSize(10, 10), CanvasColorParams());                               \
  MockImageBufferSurfaceForOverwriteTesting* surface_ptr = mock_surface.get(); \
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(                     \
      std::move(mock_surface));                                                \
  EXPECT_CALL(*surface_ptr, WillOverwriteCanvas()).Times(EXPECTED_OVERDRAWS);  \
  Context2d()->save();

#define TEST_OVERDRAW_FINALIZE \
  Context2d()->restore();      \
  Mock::VerifyAndClearExpectations(surface_ptr);

#define TEST_OVERDRAW_1(EXPECTED_OVERDRAWS, CALL1) \
  do {                                             \
    TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS)        \
    Context2d()->CALL1;                            \
    TEST_OVERDRAW_FINALIZE                         \
  } while (0)

#define TEST_OVERDRAW_2(EXPECTED_OVERDRAWS, CALL1, CALL2) \
  do {                                                    \
    TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS)               \
    Context2d()->CALL1;                                   \
    Context2d()->CALL2;                                   \
    TEST_OVERDRAW_FINALIZE                                \
  } while (0)

#define TEST_OVERDRAW_3(EXPECTED_OVERDRAWS, CALL1, CALL2, CALL3) \
  do {                                                           \
    TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS)                      \
    Context2d()->CALL1;                                          \
    Context2d()->CALL2;                                          \
    Context2d()->CALL3;                                          \
    TEST_OVERDRAW_FINALIZE                                       \
  } while (0)

#define TEST_OVERDRAW_4(EXPECTED_OVERDRAWS, CALL1, CALL2, CALL3, CALL4) \
  do {                                                                  \
    TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS)                             \
    Context2d()->CALL1;                                                 \
    Context2d()->CALL2;                                                 \
    Context2d()->CALL3;                                                 \
    Context2d()->CALL4;                                                 \
    TEST_OVERDRAW_FINALIZE                                              \
  } while (0)

//============================================================================

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithFillRect) {
  CreateContext(kNonOpaque);

  TEST_OVERDRAW_1(1, fillRect(-1, -1, 12, 12));
  TEST_OVERDRAW_1(1, fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_1(
      0, strokeRect(0, 0, 10,
                    10));  // stroking instead of filling does not overwrite
  TEST_OVERDRAW_2(0, setGlobalAlpha(0.5f), fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_1(0, fillRect(0, 0, 9, 9));
  TEST_OVERDRAW_2(0, translate(1, 1), fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(1, translate(1, 1), fillRect(-1, -1, 10, 10));
  TEST_OVERDRAW_2(1, setFillStyle(OpaqueGradient()), fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(0, setFillStyle(AlphaGradient()), fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_3(0, setGlobalAlpha(0.5), setFillStyle(OpaqueGradient()),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_3(1, setGlobalAlpha(0.5f),
                  setGlobalCompositeOperation(String("copy")),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")),
                  fillRect(0, 0, 9, 9));
  TEST_OVERDRAW_3(0, rect(0, 0, 5, 5), clip(), fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_4(0, rect(0, 0, 5, 5), clip(),
                  setGlobalCompositeOperation(String("copy")),
                  fillRect(0, 0, 10, 10));
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithClearRect) {
  CreateContext(kNonOpaque);

  TEST_OVERDRAW_1(1, clearRect(0, 0, 10, 10));
  TEST_OVERDRAW_1(0, clearRect(0, 0, 9, 9));
  TEST_OVERDRAW_2(1, setGlobalAlpha(0.5f), clearRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(1, setFillStyle(AlphaGradient()), clearRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(0, translate(1, 1), clearRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(1, translate(1, 1), clearRect(-1, -1, 10, 10));
  TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("destination-in")),
                  clearRect(0, 0, 10, 10));  // composite op ignored
  TEST_OVERDRAW_3(0, rect(0, 0, 5, 5), clip(), clearRect(0, 0, 10, 10));
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithDrawImage) {
  CreateContext(kNonOpaque);
  NonThrowableExceptionState exception_state;

  TEST_OVERDRAW_1(1, drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10,
                               0, 0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(1, drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 1, 1, 0,
                               0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(0, setGlobalAlpha(0.5f),
                  drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10, 0,
                            0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(0, drawImage(GetScriptState(), &alpha_bitmap_, 0, 0, 10, 10,
                               0, 0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(0, setGlobalAlpha(0.5f),
                  drawImage(GetScriptState(), &alpha_bitmap_, 0, 0, 10, 10, 0,
                            0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(0, drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10,
                               1, 0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(0, drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10,
                               0, 0, 9, 9, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(1, drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10,
                               0, 0, 11, 11, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(1, translate(-1, 0),
                  drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10, 1,
                            0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(0, translate(-1, 0),
                  drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10, 0,
                            0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(
      0, setFillStyle(OpaqueGradient()),
      drawImage(GetScriptState(), &alpha_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                exception_state));  // fillStyle ignored by drawImage
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(
      1, setFillStyle(AlphaGradient()),
      drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                exception_state));  // fillStyle ignored by drawImage
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")),
                  drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10, 1,
                            0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_3(0, rect(0, 0, 5, 5), clip(),
                  drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10, 0,
                            0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithPutImageData) {
  CreateContext(kNonOpaque);
  NonThrowableExceptionState exception_state;

  // Test putImageData
  TEST_OVERDRAW_1(1,
                  putImageData(full_image_data_.Get(), 0, 0, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(1, putImageData(full_image_data_.Get(), 0, 0, 0, 0, 10, 10,
                                  exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(0, putImageData(full_image_data_.Get(), 0, 0, 1, 1, 8, 8,
                                  exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(1, setGlobalAlpha(0.5f),
                  putImageData(full_image_data_.Get(), 0, 0,
                               exception_state));  // alpha has no effect
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(
      0, putImageData(partial_image_data_.Get(), 0, 0, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(1, translate(1, 1),
                  putImageData(full_image_data_.Get(), 0, 0,
                               exception_state));  // ignores tranforms
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(0,
                  putImageData(full_image_data_.Get(), 1, 0, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_3(1, rect(0, 0, 5, 5), clip(),
                  putImageData(full_image_data_.Get(), 0, 0,
                               exception_state));  // ignores clip
  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithCompositeOperations) {
  CreateContext(kNonOpaque);

  // Test composite operators with an opaque rect that covers the entire canvas
  // Note: all the untested composite operations take the same code path as
  // source-in, which assumes that the destination may not be overwritten
  TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("clear")),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("source-over")),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("source-in")),
                  fillRect(0, 0, 10, 10));
  // Test composite operators with a transparent rect that covers the entire
  // canvas
  TEST_OVERDRAW_3(1, setGlobalAlpha(0.5f),
                  setGlobalCompositeOperation(String("clear")),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_3(1, setGlobalAlpha(0.5f),
                  setGlobalCompositeOperation(String("copy")),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_3(0, setGlobalAlpha(0.5f),
                  setGlobalCompositeOperation(String("source-over")),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_3(0, setGlobalAlpha(0.5f),
                  setGlobalCompositeOperation(String("source-in")),
                  fillRect(0, 0, 10, 10));
  // Test composite operators with an opaque rect that does not cover the entire
  // canvas
  TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("clear")),
                  fillRect(0, 0, 5, 5));
  TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")),
                  fillRect(0, 0, 5, 5));
  TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("source-over")),
                  fillRect(0, 0, 5, 5));
  TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("source-in")),
                  fillRect(0, 0, 5, 5));
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionByDefault) {
  CreateContext(kNonOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  EXPECT_FALSE(CanvasElement().ShouldBeDirectComposited());
  EXPECT_TRUE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionUnderOverdrawLimit) {
  CreateContext(kNonOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  Context2d()->setGlobalAlpha(0.5f);  // To prevent overdraw optimization
  for (int i = 0;
       i < CanvasHeuristicParameters::kExpensiveOverdrawThreshold - 1; i++) {
    Context2d()->fillRect(0, 0, 10, 10);
  }

  EXPECT_FALSE(CanvasElement().ShouldBeDirectComposited());
  EXPECT_TRUE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionOverOverdrawLimit) {
  CreateContext(kNonOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  Context2d()->setGlobalAlpha(0.5f);  // To prevent overdraw optimization
  for (int i = 0; i < CanvasHeuristicParameters::kExpensiveOverdrawThreshold;
       i++) {
    Context2d()->fillRect(0, 0, 10, 10);
  }

  EXPECT_TRUE(CanvasElement().ShouldBeDirectComposited());
  EXPECT_TRUE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionUnderImageSizeRatioLimit) {
  CreateContext(kNonOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  NonThrowableExceptionState exception_state;
  Element* source_canvas_element =
      GetDocument().createElement("canvas", exception_state);
  EXPECT_FALSE(exception_state.HadException());
  HTMLCanvasElement* source_canvas =
      static_cast<HTMLCanvasElement*>(source_canvas_element);
  IntSize source_size(10,
                      10 * CanvasHeuristicParameters::kExpensiveImageSizeRatio);
  std::unique_ptr<UnacceleratedImageBufferSurface> source_surface =
      std::make_unique<UnacceleratedImageBufferSurface>(source_size);
  source_canvas->CreateImageBufferUsingSurfaceForTesting(
      std::move(source_surface));

  const ImageBitmapOptions default_options;
  Optional<IntRect> crop_rect = IntRect(IntPoint(0, 0), source_size);
  // Go through an ImageBitmap to avoid triggering a display list fallback
  ImageBitmap* source_image_bitmap =
      ImageBitmap::Create(source_canvas, crop_rect, default_options);

  Context2d()->drawImage(GetScriptState(), source_image_bitmap, 0, 0, 1, 1, 0,
                         0, 1, 1, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_FALSE(CanvasElement().ShouldBeDirectComposited());
  EXPECT_TRUE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionOverImageSizeRatioLimit) {
  CreateContext(kNonOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  NonThrowableExceptionState exception_state;
  Element* source_canvas_element =
      GetDocument().createElement("canvas", exception_state);
  EXPECT_FALSE(exception_state.HadException());
  HTMLCanvasElement* source_canvas =
      static_cast<HTMLCanvasElement*>(source_canvas_element);
  IntSize source_size(
      10, 10 * CanvasHeuristicParameters::kExpensiveImageSizeRatio + 1);
  std::unique_ptr<UnacceleratedImageBufferSurface> source_surface =
      std::make_unique<UnacceleratedImageBufferSurface>(source_size);
  source_canvas->CreateImageBufferUsingSurfaceForTesting(
      std::move(source_surface));

  const ImageBitmapOptions default_options;
  Optional<IntRect> crop_rect = IntRect(IntPoint(0, 0), source_size);
  // Go through an ImageBitmap to avoid triggering a display list fallback
  ImageBitmap* source_image_bitmap =
      ImageBitmap::Create(source_canvas, crop_rect, default_options);

  Context2d()->drawImage(GetScriptState(), source_image_bitmap, 0, 0, 1, 1, 0,
                         0, 1, 1, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_TRUE(CanvasElement().ShouldBeDirectComposited());
  EXPECT_TRUE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest,
       NoLayerPromotionUnderExpensivePathPointCount) {
  CreateContext(kNonOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  Context2d()->beginPath();
  Context2d()->moveTo(7, 5);
  for (int i = 1; i < CanvasHeuristicParameters::kExpensivePathPointCount - 1;
       i++) {
    float angle_rad = twoPiFloat * i /
                      (CanvasHeuristicParameters::kExpensivePathPointCount - 1);
    Context2d()->lineTo(5 + 2 * cos(angle_rad), 5 + 2 * sin(angle_rad));
  }
  Context2d()->fill();

  EXPECT_FALSE(CanvasElement().ShouldBeDirectComposited());
  EXPECT_TRUE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest,
       LayerPromotionOverExpensivePathPointCount) {
  CreateContext(kNonOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  Context2d()->beginPath();
  Context2d()->moveTo(7, 5);
  for (int i = 1; i < CanvasHeuristicParameters::kExpensivePathPointCount + 1;
       i++) {
    float angle_rad = twoPiFloat * i /
                      (CanvasHeuristicParameters::kExpensivePathPointCount + 1);
    Context2d()->lineTo(5 + 2 * cos(angle_rad), 5 + 2 * sin(angle_rad));
  }
  Context2d()->fill();

  EXPECT_TRUE(CanvasElement().ShouldBeDirectComposited());
  EXPECT_TRUE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionWhenPathIsConcave) {
  CreateContext(kNonOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  Context2d()->beginPath();
  Context2d()->moveTo(1, 1);
  Context2d()->lineTo(5, 5);
  Context2d()->lineTo(9, 1);
  Context2d()->lineTo(5, 9);
  Context2d()->fill();

  if (CanvasHeuristicParameters::kConcavePathsAreExpensive) {
    EXPECT_TRUE(CanvasElement().ShouldBeDirectComposited());
  } else {
    EXPECT_FALSE(CanvasElement().ShouldBeDirectComposited());
  }
  EXPECT_TRUE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionWithRectangleClip) {
  CreateContext(kNonOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  Context2d()->beginPath();
  Context2d()->rect(1, 1, 2, 2);
  Context2d()->clip();
  Context2d()->fillRect(0, 0, 4, 4);

  EXPECT_FALSE(CanvasElement().ShouldBeDirectComposited());
  EXPECT_TRUE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionWithComplexClip) {
  CreateContext(kNonOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  Context2d()->beginPath();
  Context2d()->moveTo(1, 1);
  Context2d()->lineTo(5, 5);
  Context2d()->lineTo(9, 1);
  Context2d()->lineTo(5, 9);
  Context2d()->clip();
  Context2d()->fillRect(0, 0, 4, 4);

  if (CanvasHeuristicParameters::kComplexClipsAreExpensive) {
    EXPECT_TRUE(CanvasElement().ShouldBeDirectComposited());
  } else {
    EXPECT_FALSE(CanvasElement().ShouldBeDirectComposited());
  }
  EXPECT_TRUE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionWithBlurredShadow) {
  CreateContext(kNonOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  Context2d()->setShadowColor(String("red"));
  Context2d()->setShadowBlur(1.0f);
  Context2d()->fillRect(1, 1, 1, 1);

  if (CanvasHeuristicParameters::kBlurredShadowsAreExpensive) {
    EXPECT_TRUE(CanvasElement().ShouldBeDirectComposited());
  } else {
    EXPECT_FALSE(CanvasElement().ShouldBeDirectComposited());
  }
  EXPECT_TRUE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionWithSharpShadow) {
  CreateContext(kNonOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  Context2d()->setShadowColor(String("red"));
  Context2d()->setShadowOffsetX(1.0f);
  Context2d()->fillRect(1, 1, 1, 1);

  EXPECT_FALSE(CanvasElement().ShouldBeDirectComposited());
  EXPECT_TRUE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest, NoFallbackWithSmallState) {
  CreateContext(kNonOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  Context2d()->fillRect(0, 0, 1, 1);  // To have a non-empty dirty rect.
  for (int i = 0;
       i < CanvasHeuristicParameters::kExpensiveRecordingStackDepth - 1; ++i) {
    Context2d()->save();
    Context2d()->translate(1.0f, 0.0f);
  }
  CanvasElement().FinalizeFrame();  // To close the current frame.

  EXPECT_TRUE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest, FallbackWithLargeState) {
  CreateContext(kNonOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  Context2d()->fillRect(0, 0, 1, 1);  // To have a non-empty dirty rect.
  for (int i = 0; i < CanvasHeuristicParameters::kExpensiveRecordingStackDepth;
       ++i) {
    Context2d()->save();
    Context2d()->translate(1.0f, 0.0f);
  }
  CanvasElement().FinalizeFrame();  // To close the current frame.

  EXPECT_FALSE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest, OpaqueDisplayListFallsBackForText) {
  // Verify that drawing text to an opaque canvas, which is expected to
  // render with subpixel text anti-aliasing, results in falling out
  // of display list mode because the current diplay list implementation
  // does not support pixel geometry settings.
  // See: crbug.com/583809
  CreateContext(kOpaque);
  auto surface = std::make_unique<RecordingImageBufferSurface>(
      IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback,
      CanvasColorParams(kSRGBCanvasColorSpace, kRGBA8CanvasPixelFormat,
                        kOpaque));
  auto* surface_ptr = surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(surface));

  Context2d()->fillText("Text", 0, 5);

  EXPECT_FALSE(surface_ptr->IsRecording());
}

TEST_F(CanvasRenderingContext2DTest, ImageResourceLifetime) {
  NonThrowableExceptionState non_throwable_exception_state;
  Element* canvas_element =
      GetDocument().createElement("canvas", non_throwable_exception_state);
  EXPECT_FALSE(non_throwable_exception_state.HadException());
  HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(canvas_element);
  canvas->SetSize(IntSize(40, 40));
  ImageBitmap* image_bitmap_derived = nullptr;
  {
    const ImageBitmapOptions default_options;
    Optional<IntRect> crop_rect =
        IntRect(0, 0, canvas->width(), canvas->height());
    ImageBitmap* image_bitmap_from_canvas =
        ImageBitmap::Create(canvas, crop_rect, default_options);
    crop_rect = IntRect(0, 0, 20, 20);
    image_bitmap_derived = ImageBitmap::Create(image_bitmap_from_canvas,
                                               crop_rect, default_options);
  }
  CanvasContextCreationAttributes attributes;
  CanvasRenderingContext2D* context = static_cast<CanvasRenderingContext2D*>(
      canvas->GetCanvasRenderingContext("2d", attributes));
  DummyExceptionStateForTesting exception_state;
  CanvasImageSourceUnion image_source;
  image_source.SetImageBitmap(image_bitmap_derived);
  context->drawImage(GetScriptState(), image_source, 0, 0, exception_state);
}

TEST_F(CanvasRenderingContext2DTest, GPUMemoryUpdateForAcceleratedCanvas) {
  CreateContext(kNonOpaque);

  std::unique_ptr<FakeAcceleratedImageBufferSurface> fake_accelerate_surface =
      std::make_unique<FakeAcceleratedImageBufferSurface>(IntSize(10, 10),
                                                          CanvasColorParams());
  FakeAcceleratedImageBufferSurface* fake_accelerate_surface_ptr =
      fake_accelerate_surface.get();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(
      std::move(fake_accelerate_surface));
  // 800 = 10 * 10 * 4 * 2 where 10*10 is canvas size, 4 is num of bytes per
  // pixel per buffer, and 2 is an estimate of num of gpu buffers required
  EXPECT_EQ(800, GetCurrentGPUMemoryUsage());
  EXPECT_EQ(800, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());

  // Switching accelerated mode to non-accelerated mode
  fake_accelerate_surface_ptr->SetIsAccelerated(false);
  CanvasElement().UpdateMemoryUsage();
  EXPECT_EQ(0, GetCurrentGPUMemoryUsage());
  EXPECT_EQ(0, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(0u, GetGlobalAcceleratedContextCount());

  // Switching non-accelerated mode to accelerated mode
  fake_accelerate_surface_ptr->SetIsAccelerated(true);
  CanvasElement().UpdateMemoryUsage();
  EXPECT_EQ(800, GetCurrentGPUMemoryUsage());
  EXPECT_EQ(800, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());

  // Creating a different accelerated image buffer
  HTMLCanvasElement* anotherCanvas =
      ToHTMLCanvasElement(GetDocument().getElementById("d"));
  CanvasContextCreationAttributes attributes;
  anotherCanvas->GetCanvasRenderingContext("2d", attributes);
  auto fake_accelerate_surface2 =
      std::make_unique<FakeAcceleratedImageBufferSurface>(IntSize(10, 5),
                                                          CanvasColorParams());
  anotherCanvas->CreateImageBufferUsingSurfaceForTesting(
      std::move(fake_accelerate_surface2));
  EXPECT_EQ(800, GetCurrentGPUMemoryUsage());
  EXPECT_EQ(1200, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(2u, GetGlobalAcceleratedContextCount());

  // Tear down the first image buffer that resides in current canvas element
  CanvasElement().SetSize(IntSize(20, 20));
  Mock::VerifyAndClearExpectations(fake_accelerate_surface_ptr);
  EXPECT_EQ(400, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());

  // Tear down the second image buffer
  anotherCanvas->SetSize(IntSize(20, 20));
  EXPECT_EQ(0, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(0u, GetGlobalAcceleratedContextCount());
}

TEST_F(CanvasRenderingContext2DTest, CanvasDisposedBeforeContext) {
  CreateContext(kNonOpaque);
  Context2d()->fillRect(0, 0, 1, 1);  // results in task observer registration

  Context2d()->DetachHost();

  // This is the only method that is callable after DetachHost
  // Test passes by not crashing.
  Context2d()->DidProcessTask();

  // Test passes by not crashing during teardown
}

TEST_F(CanvasRenderingContext2DTest, ContextDisposedBeforeCanvas) {
  CreateContext(kNonOpaque);

  CanvasElement().DetachContext();
  // Passes by not crashing later during teardown
}

#if defined(MEMORY_SANITIZER)
#define MAYBE_GetImageDataDisablesAcceleration \
  DISABLED_GetImageDataDisablesAcceleration
#else
#define MAYBE_GetImageDataDisablesAcceleration GetImageDataDisablesAcceleration
#endif

TEST_F(CanvasRenderingContext2DTest, MAYBE_GetImageDataDisablesAcceleration) {
  ScopedCanvas2dFixedRenderingModeForTest canvas_2d_fixed_rendering_mode(false);

  CreateContext(kNonOpaque);
  IntSize size(300, 300);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(size, Canvas2DLayerBridge::kForceAccelerationForTesting);
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(bridge));

  EXPECT_TRUE(CanvasElement().GetImageBuffer()->IsAccelerated());
  EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());
  EXPECT_EQ(720000, GetGlobalGPUMemoryUsage());

  DummyExceptionStateForTesting exception_state;
  for (int i = 0;
       i < CanvasHeuristicParameters::kGPUReadbackMinSuccessiveFrames - 1;
       i++) {
    Context2d()->getImageData(0, 0, 1, 1, exception_state);
    CanvasElement().FinalizeFrame();

    EXPECT_FALSE(exception_state.HadException());
    EXPECT_TRUE(CanvasElement().GetImageBuffer()->IsAccelerated());
    EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());
    EXPECT_EQ(720000, GetGlobalGPUMemoryUsage());
  }

  Context2d()->getImageData(0, 0, 1, 1, exception_state);
  CanvasElement().FinalizeFrame();

  EXPECT_FALSE(exception_state.HadException());
  if (CanvasHeuristicParameters::kGPUReadbackForcesNoAcceleration) {
    EXPECT_FALSE(CanvasElement().GetImageBuffer()->IsAccelerated());
    EXPECT_EQ(0u, GetGlobalAcceleratedContextCount());
    EXPECT_EQ(0, GetGlobalGPUMemoryUsage());
  } else {
    EXPECT_TRUE(CanvasElement().GetImageBuffer()->IsAccelerated());
    EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());
    EXPECT_EQ(720000, GetGlobalGPUMemoryUsage());
  }
}

TEST_F(CanvasRenderingContext2DTest, TextureUploadHeuristics) {
  ScopedCanvas2dFixedRenderingModeForTest canvas_2d_fixed_rendering_mode(false);

  enum TestVariants {
    kLargeTextureDisablesAcceleration = 0,
    kSmallTextureDoesNotDisableAcceleration = 1,

    kTestVariantCount = 2,
  };

  for (int test_variant = 0; test_variant < kTestVariantCount; test_variant++) {
    int delta = test_variant == kLargeTextureDisablesAcceleration ? 1 : -1;
    int src_size =
        std::sqrt(static_cast<float>(
            CanvasHeuristicParameters::kDrawImageTextureUploadSoftSizeLimit)) +
        delta;
    int dst_size =
        src_size /
            std::sqrt(static_cast<float>(
                CanvasHeuristicParameters::
                    kDrawImageTextureUploadSoftSizeLimitScaleThreshold)) -
        delta;

    CreateContext(kNonOpaque);
    IntSize size(dst_size, dst_size);
    std::unique_ptr<Canvas2DLayerBridge> bridge =
        MakeBridge(size, Canvas2DLayerBridge::kEnableAcceleration);
    CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(bridge));

    EXPECT_TRUE(CanvasElement().GetImageBuffer()->IsAccelerated());
    EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());
    // 4 bytes per pixel * 2 buffers = 8
    EXPECT_EQ(8 * dst_size * dst_size, GetGlobalGPUMemoryUsage());
    sk_sp<SkSurface> sk_surface =
        SkSurface::MakeRasterN32Premul(src_size, src_size);
    scoped_refptr<StaticBitmapImage> big_bitmap =
        StaticBitmapImage::Create(sk_surface->makeImageSnapshot());
    ImageBitmap* big_image = ImageBitmap::Create(std::move(big_bitmap));
    NonThrowableExceptionState exception_state;
    V8TestingScope scope;
    Context2d()->drawImage(scope.GetScriptState(), big_image, 0, 0, src_size,
                           src_size, 0, 0, dst_size, dst_size, exception_state);
    EXPECT_FALSE(exception_state.HadException());

    if (test_variant == kLargeTextureDisablesAcceleration) {
      EXPECT_FALSE(CanvasElement().GetImageBuffer()->IsAccelerated());
      EXPECT_EQ(0u, GetGlobalAcceleratedContextCount());
      EXPECT_EQ(0, GetGlobalGPUMemoryUsage());
    } else {
      EXPECT_TRUE(CanvasElement().GetImageBuffer()->IsAccelerated());
      EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());
      EXPECT_EQ(8 * dst_size * dst_size, GetGlobalGPUMemoryUsage());
    }
  }
}

TEST_F(CanvasRenderingContext2DTest, DisableAcceleration) {
  CreateContext(kNonOpaque);

  auto fake_accelerate_surface =
      std::make_unique<FakeAcceleratedImageBufferSurface>(IntSize(10, 10),
                                                          CanvasColorParams());
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(
      std::move(fake_accelerate_surface));
  CanvasRenderingContext2D* context = Context2d();

  // 800 = 10 * 10 * 4 * 2 where 10*10 is canvas size, 4 is num of bytes per
  // pixel per buffer, and 2 is an estimate of num of gpu buffers required
  EXPECT_EQ(800, GetCurrentGPUMemoryUsage());
  EXPECT_EQ(800, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());

  context->fillRect(10, 10, 100, 100);
  EXPECT_TRUE(CanvasElement().GetImageBuffer()->IsAccelerated());

  CanvasElement().DisableAcceleration();
  EXPECT_FALSE(CanvasElement().GetImageBuffer()->IsAccelerated());

  context->fillRect(10, 10, 100, 100);

  EXPECT_EQ(0, GetCurrentGPUMemoryUsage());
  EXPECT_EQ(0, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(0u, GetGlobalAcceleratedContextCount());
}

enum class ColorSpaceConversion : uint8_t {
  NONE = 0,
  DEFAULT_COLOR_CORRECTED = 1,
  SRGB = 2,
  LINEAR_RGB = 3,
  P3 = 4,
  REC2020 = 5,

  LAST = REC2020
};

static ImageBitmapOptions PrepareBitmapOptionsAndSetRuntimeFlags(
    const ColorSpaceConversion& color_space_conversion) {
  // Set the color space conversion in ImageBitmapOptions
  ImageBitmapOptions options;
  static const Vector<String> kConversions = {
      "none", "default", "srgb", "linear-rgb", "p3", "rec2020"};
  options.setColorSpaceConversion(
      kConversions[static_cast<uint8_t>(color_space_conversion)]);

  return options;
}

TEST_F(CanvasRenderingContext2DTest, ImageBitmapColorSpaceConversion) {
  ScopedExperimentalCanvasFeaturesForTest experimental_canvas_features(true);

  Persistent<HTMLCanvasElement> canvas =
      Persistent<HTMLCanvasElement>(CanvasElement());
  CanvasContextCreationAttributes attributes;
  attributes.setAlpha(true);
  attributes.setColorSpace("srgb");
  CanvasRenderingContext2D* context = static_cast<CanvasRenderingContext2D*>(
      canvas->GetCanvasRenderingContext("2d", attributes));
  StringOrCanvasGradientOrCanvasPattern fill_style;
  fill_style.SetString("#FFC08040");  // 255,192,128,64
  context->setFillStyle(fill_style);
  context->fillRect(0, 0, 4, 4);
  NonThrowableExceptionState exception_state;
  uint8_t* src_pixel =
      context->getImageData(2, 2, 1, 1, exception_state)->data()->Data();

  // Create and test the ImageBitmap objects.
  Optional<IntRect> crop_rect = IntRect(0, 0, 4, 4);
  sk_sp<SkColorSpace> color_space = nullptr;
  SkColorType color_type = SkColorType::kRGBA_8888_SkColorType;
  SkColorSpaceXform::ColorFormat color_format32 =
      SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
  SkColorSpaceXform::ColorFormat color_format = color_format32;
  sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();

  for (uint8_t i =
           static_cast<uint8_t>(ColorSpaceConversion::DEFAULT_COLOR_CORRECTED);
       i <= static_cast<uint8_t>(ColorSpaceConversion::LAST); i++) {
    ColorSpaceConversion color_space_conversion =
        static_cast<ColorSpaceConversion>(i);

    switch (color_space_conversion) {
      case ColorSpaceConversion::NONE:
        NOTREACHED();
        break;
      case ColorSpaceConversion::DEFAULT_COLOR_CORRECTED:
      case ColorSpaceConversion::SRGB:
        color_space = SkColorSpace::MakeSRGB();
        color_format = color_format32;
        break;
      case ColorSpaceConversion::LINEAR_RGB:
        color_space = SkColorSpace::MakeSRGBLinear();
        color_type = SkColorType::kRGBA_F16_SkColorType;
        color_format = SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat;
        break;
      case ColorSpaceConversion::P3:
        color_space =
            SkColorSpace::MakeRGB(SkColorSpace::kLinear_RenderTargetGamma,
                                  SkColorSpace::kDCIP3_D65_Gamut);
        color_type = SkColorType::kRGBA_F16_SkColorType;
        color_format = SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat;
        break;
      case ColorSpaceConversion::REC2020:
        color_space =
            SkColorSpace::MakeRGB(SkColorSpace::kLinear_RenderTargetGamma,
                                  SkColorSpace::kRec2020_Gamut);
        color_type = SkColorType::kRGBA_F16_SkColorType;
        color_format = SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat;
        break;
      default:
        NOTREACHED();
    }

    // Color convert using ImageBitmap
    ImageBitmapOptions options =
        PrepareBitmapOptionsAndSetRuntimeFlags(color_space_conversion);
    ImageBitmap* image_bitmap = ImageBitmap::Create(canvas, crop_rect, options);
    sk_sp<SkImage> converted_image =
        image_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSkImage();
    SkImageInfo image_info =
        SkImageInfo::Make(1, 1, color_type, SkAlphaType::kUnpremul_SkAlphaType,
                          converted_image->refColorSpace());
    std::unique_ptr<uint8_t[]> converted_pixel(
        new uint8_t[image_info.bytesPerPixel()]());
    EXPECT_TRUE(converted_image->readPixels(image_info, converted_pixel.get(),
                                            image_info.minRowBytes(), 2, 2));

    // Transform the source pixel and check if the image bitmap color conversion
    // is done correctly.
    std::unique_ptr<SkColorSpaceXform> color_space_xform =
        SkColorSpaceXform::New(src_rgb_color_space.get(), color_space.get());
    std::unique_ptr<uint8_t[]> transformed_pixel(
        new uint8_t[image_info.bytesPerPixel()]());
    EXPECT_TRUE(color_space_xform->apply(color_format, transformed_pixel.get(),
                                         color_format32, src_pixel, 1,
                                         SkAlphaType::kUnpremul_SkAlphaType));

    ColorCorrectionTestUtils::CompareColorCorrectedPixels(
        converted_pixel.get(), transformed_pixel.get(), 1,
        (color_type == kRGBA_8888_SkColorType) ? kUint8ClampedArrayStorageFormat
                                               : kUint16ArrayStorageFormat,
        kAlphaUnmultiplied, kUnpremulRoundTripTolerance);
  }
}

// The color settings of the surface of the canvas always remaines loyal to the
// first created context 2D. Therefore, we have to test different canvas color
// space settings for CanvasRenderingContext2D::putImageData() in different
// tests.
enum class CanvasColorSpaceSettings : uint8_t {
  CANVAS_SRGB = 0,
  CANVAS_LINEARSRGB = 1,
  CANVAS_REC2020 = 2,
  CANVAS_P3 = 3,

  LAST = CANVAS_P3
};

// This test verifies the correct behavior of putImageData member function in
// color managed mode.
void TestPutImageDataOnCanvasWithColorSpaceSettings(
    HTMLCanvasElement& canvas_element,
    CanvasColorSpaceSettings canvas_colorspace_setting) {
  unsigned num_image_data_color_spaces = 3;
  CanvasColorSpace image_data_color_spaces[] = {
      kSRGBCanvasColorSpace, kRec2020CanvasColorSpace, kP3CanvasColorSpace,
  };

  unsigned num_image_data_storage_formats = 3;
  ImageDataStorageFormat image_data_storage_formats[] = {
      kUint8ClampedArrayStorageFormat, kUint16ArrayStorageFormat,
      kFloat32ArrayStorageFormat,
  };

  CanvasColorSpace canvas_color_spaces[] = {
      kSRGBCanvasColorSpace, kSRGBCanvasColorSpace, kRec2020CanvasColorSpace,
      kP3CanvasColorSpace,
  };

  String canvas_color_space_names[] = {
      kSRGBCanvasColorSpaceName, kSRGBCanvasColorSpaceName,
      kRec2020CanvasColorSpaceName, kP3CanvasColorSpaceName};

  CanvasPixelFormat canvas_pixel_formats[] = {
      kRGBA8CanvasPixelFormat, kF16CanvasPixelFormat, kF16CanvasPixelFormat,
      kF16CanvasPixelFormat,
  };

  String canvas_pixel_format_names[] = {
      kRGBA8CanvasPixelFormatName, kF16CanvasPixelFormatName,
      kF16CanvasPixelFormatName, kF16CanvasPixelFormatName};

  // Source pixels in RGBA32
  uint8_t u8_pixels[] = {255, 0,   0,   255,  // Red
                         0,   0,   0,   0,    // Transparent
                         255, 192, 128, 64,   // Decreasing values
                         93,  117, 205, 41};  // Random values
  unsigned data_length = 16;

  uint16_t* u16_pixels = new uint16_t[data_length];
  for (unsigned i = 0; i < data_length; i++)
    u16_pixels[i] = u8_pixels[i] * 257;

  float* f32_pixels = new float[data_length];
  for (unsigned i = 0; i < data_length; i++)
    f32_pixels[i] = u8_pixels[i] / 255.0;

  DOMArrayBufferView* data_array = nullptr;

  DOMUint8ClampedArray* data_u8 =
      DOMUint8ClampedArray::Create(u8_pixels, data_length);
  DCHECK(data_u8);
  EXPECT_EQ(data_length, data_u8->length());
  DOMUint16Array* data_u16 = DOMUint16Array::Create(u16_pixels, data_length);
  DCHECK(data_u16);
  EXPECT_EQ(data_length, data_u16->length());
  DOMFloat32Array* data_f32 = DOMFloat32Array::Create(f32_pixels, data_length);
  DCHECK(data_f32);
  EXPECT_EQ(data_length, data_f32->length());

  ImageData* image_data = nullptr;
  ImageDataColorSettings color_settings;
  int num_pixels = data_length / 4;

  // At most four bytes are needed for Float32 output per color component.
  std::unique_ptr<uint8_t[]> pixels_converted_manually(
      new uint8_t[data_length * 4]());

  // Loop through different possible combinations of image data color space and
  // storage formats and create the respective test image data objects.
  for (unsigned i = 0; i < num_image_data_color_spaces; i++) {
    color_settings.setColorSpace(
        ImageData::CanvasColorSpaceName(image_data_color_spaces[i]));

    for (unsigned j = 0; j < num_image_data_storage_formats; j++) {
      switch (image_data_storage_formats[j]) {
        case kUint8ClampedArrayStorageFormat:
          data_array = static_cast<DOMArrayBufferView*>(data_u8);
          color_settings.setStorageFormat(kUint8ClampedArrayStorageFormatName);
          break;
        case kUint16ArrayStorageFormat:
          data_array = static_cast<DOMArrayBufferView*>(data_u16);
          color_settings.setStorageFormat(kUint16ArrayStorageFormatName);
          break;
        case kFloat32ArrayStorageFormat:
          data_array = static_cast<DOMArrayBufferView*>(data_f32);
          color_settings.setStorageFormat(kFloat32ArrayStorageFormatName);
          break;
        default:
          NOTREACHED();
      }

      image_data =
          ImageData::CreateForTest(IntSize(2, 2), data_array, &color_settings);

      unsigned k = (unsigned)(canvas_colorspace_setting);
      // Convert the original data used to create ImageData to the
      // canvas color space and canvas pixel format.
      EXPECT_TRUE(
          ColorCorrectionTestUtils::
              ConvertPixelsToColorSpaceAndPixelFormatForTest(
                  data_array->BaseAddress(), data_length,
                  image_data_color_spaces[i], image_data_storage_formats[j],
                  canvas_color_spaces[k], canvas_pixel_formats[k],
                  pixels_converted_manually,
                  SkColorSpaceXform::ColorFormat::kRGBA_F32_ColorFormat));

      // Create a canvas and call putImageData and getImageData to make sure
      // the conversion is done correctly.
      CanvasContextCreationAttributes attributes;
      attributes.setAlpha(true);
      attributes.setColorSpace(canvas_color_space_names[k]);
      attributes.setPixelFormat(canvas_pixel_format_names[k]);
      CanvasRenderingContext2D* context =
          static_cast<CanvasRenderingContext2D*>(
              canvas_element.GetCanvasRenderingContext("2d", attributes));
      NonThrowableExceptionState exception_state;
      context->putImageData(image_data, 0, 0, exception_state);

      void* pixels_from_get_image_data =
          context->getImageData(0, 0, 2, 2, exception_state)
              ->BufferBase()
              ->Data();
      ColorCorrectionTestUtils::CompareColorCorrectedPixels(
          pixels_from_get_image_data, pixels_converted_manually.get(),
          num_pixels,
          (canvas_pixel_formats[k] == kRGBA8CanvasPixelFormat)
              ? kUint8ClampedArrayStorageFormat
              : kFloat32ArrayStorageFormat,
          kAlphaUnmultiplied, kUnpremulRoundTripTolerance);
    }
  }
  delete[] u16_pixels;
  delete[] f32_pixels;
}

// Test disabled due to crbug.com/780925
TEST_F(CanvasRenderingContext2DTest, ColorManagedPutImageDataOnSRGBCanvas) {
  TestPutImageDataOnCanvasWithColorSpaceSettings(
      CanvasElement(), CanvasColorSpaceSettings::CANVAS_SRGB);
}

TEST_F(CanvasRenderingContext2DTest,
       ColorManagedPutImageDataOnLinearSRGBCanvas) {
  TestPutImageDataOnCanvasWithColorSpaceSettings(
      CanvasElement(), CanvasColorSpaceSettings::CANVAS_LINEARSRGB);
}

TEST_F(CanvasRenderingContext2DTest, ColorManagedPutImageDataOnRec2020Canvas) {
  TestPutImageDataOnCanvasWithColorSpaceSettings(
      CanvasElement(), CanvasColorSpaceSettings::CANVAS_REC2020);
}

TEST_F(CanvasRenderingContext2DTest, ColorManagedPutImageDataOnP3Canvas) {
  TestPutImageDataOnCanvasWithColorSpaceSettings(
      CanvasElement(), CanvasColorSpaceSettings::CANVAS_P3);
}

void OverrideScriptEnabled(Settings& settings) {
  // Simulate that we allow scripts, so that HTMLCanvasElement uses
  // LayoutHTMLCanvas.
  settings.SetScriptEnabled(true);
}

class CanvasRenderingContext2DTestWithTestingPlatform
    : public CanvasRenderingContext2DTest {
 protected:
  void SetUp() override {
    platform_ = std::make_unique<ScopedTestingPlatformSupport<
        TestingPlatformSupportWithMockScheduler>>();
    override_settings_function_ = &OverrideScriptEnabled;
    (*platform_)
        ->AdvanceClockSeconds(1.);  // For non-zero DocumentParserTimings.
    CanvasRenderingContext2DTest::SetUp();
    GetDocument().View()->UpdateLayout();
  }

  void TearDown() override {
    platform_.reset();
    CanvasRenderingContext2DTest::TearDown();
  }

  void RunUntilIdle() { (*platform_)->RunUntilIdle(); }

  std::unique_ptr<
      ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>>
      platform_;
};

// https://crbug.com/708445: When the Canvas2DLayerBridge hibernates or wakes up
// from hibernation, the compositing reasons for the canvas element may change.
// In these cases, the element should request a compositing update.
TEST_F(CanvasRenderingContext2DTestWithTestingPlatform,
       ElementRequestsCompositingUpdateOnHibernateAndWakeUp) {
  CreateContext(kNonOpaque);
  IntSize size(300, 300);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(size, Canvas2DLayerBridge::kEnableAcceleration);
  // Force hibernatation to occur in an immediate task.
  bridge->DontUseIdleSchedulingForTesting();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(bridge));

  EXPECT_TRUE(CanvasElement().GetImageBuffer()->IsAccelerated());
  // Take a snapshot to trigger lazy resource provider creation
  CanvasElement().GetImageBuffer()->NewImageSnapshot(kPreferAcceleration,
                                                     kSnapshotReasonUnknown);
  EXPECT_TRUE(CanvasElement().GetLayoutBoxModelObject());
  PaintLayer* layer = CanvasElement().GetLayoutBoxModelObject()->Layer();
  EXPECT_TRUE(layer);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Hide element to trigger hibernation (if enabled).
  GetDocument().GetPage()->SetVisibilityState(
      mojom::PageVisibilityState::kHidden, false);
  RunUntilIdle();  // Run hibernation task.
  // If enabled, hibernation should cause compositing update.
  EXPECT_EQ(!!CANVAS2D_HIBERNATION_ENABLED,
            layer->NeedsCompositingInputsUpdate());

  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(layer->NeedsCompositingInputsUpdate());

  // Wake up again, which should request a compositing update synchronously.
  GetDocument().GetPage()->SetVisibilityState(
      mojom::PageVisibilityState::kVisible, false);
  EXPECT_EQ(!!CANVAS2D_HIBERNATION_ENABLED,
            layer->NeedsCompositingInputsUpdate());
  RunUntilIdle();  // Clear task queue.
}

TEST_F(CanvasRenderingContext2DTestWithTestingPlatform,
       NoHibernationIfNoResourceProvider) {
  CreateContext(kNonOpaque);
  IntSize size(300, 300);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(size, Canvas2DLayerBridge::kEnableAcceleration);
  // Force hibernatation to occur in an immediate task.
  bridge->DontUseIdleSchedulingForTesting();
  CanvasElement().CreateImageBufferUsingSurfaceForTesting(std::move(bridge));

  EXPECT_TRUE(CanvasElement().GetImageBuffer()->IsAccelerated());

  EXPECT_TRUE(CanvasElement().GetLayoutBoxModelObject());
  PaintLayer* layer = CanvasElement().GetLayoutBoxModelObject()->Layer();
  EXPECT_TRUE(layer);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Hide element to trigger hibernation (if enabled).
  GetDocument().GetPage()->SetVisibilityState(
      mojom::PageVisibilityState::kHidden, false);
  RunUntilIdle();  // Run hibernation task.
  // Never hibernate a canvas with no resource provider
  EXPECT_FALSE(layer->NeedsCompositingInputsUpdate());
}

}  // namespace blink
