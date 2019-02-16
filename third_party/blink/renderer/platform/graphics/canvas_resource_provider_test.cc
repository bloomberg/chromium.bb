// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using testing::_;
using testing::InSequence;
using testing::Return;
using testing::Test;

namespace blink {

namespace {

class FakeGLES2InterfaceWithImageSupport : public FakeGLES2Interface {
 public:
  GLuint CreateImageCHROMIUM(ClientBuffer, GLsizei, GLsizei, GLenum) final {
    return image_id_++;
  }
  void DestroyImageCHROMIUM(GLuint image_id) final {
    EXPECT_LE(image_id, image_id_);
  }

 private:
  GLuint image_id_ = 3;
};

class MockCanvasResourceDispatcherClient
    : public CanvasResourceDispatcherClient {
 public:
  MockCanvasResourceDispatcherClient() = default;

  MOCK_METHOD0(BeginFrame, void());
};

}  // anonymous namespace

class CanvasResourceProviderTest : public Test {
 public:
  void SetUp() override {
    // Install our mock GL context so that it gets served by SharedGpuContext.
    auto factory = [](FakeGLES2Interface* gl, bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = false;
      // Unretained is safe since TearDown() cleans up the SharedGpuContext.
      return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
    context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
  }

  void TearDown() override { SharedGpuContext::ResetForTesting(); }

  // Adds |buffer_format| to the context capabilities if it's not supported.
  void EnsureBufferFormatIsSupported(gfx::BufferFormat buffer_format) {
    auto* context_provider = context_provider_wrapper_->ContextProvider();
    if (context_provider->GetCapabilities().gpu_memory_buffer_formats.Has(
            buffer_format)) {
      return;
    }

    auto capabilities = context_provider->GetCapabilities();
    capabilities.gpu_memory_buffer_formats.Add(buffer_format);

    static_cast<FakeWebGraphicsContext3DProvider*>(context_provider)
        ->SetCapabilities(capabilities);
  }

 protected:
  FakeGLES2InterfaceWithImageSupport gl_;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform_;
};

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderTextureGpuMemoryBuffer) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());

  auto provider = CanvasResourceProvider::Create(
      kSize, CanvasResourceProvider::kAcceleratedCompositedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_TRUE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderTexture) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);

  auto provider = CanvasResourceProvider::Create(
      kSize, CanvasResourceProvider::kAcceleratedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderBitmapGpuMemoryBuffer) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);

  auto provider = CanvasResourceProvider::Create(
      kSize, CanvasResourceProvider::kSoftwareCompositedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderBitmap) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);

  auto provider = CanvasResourceProvider::Create(
      kSize, CanvasResourceProvider::kSoftwareResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_FALSE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderSharedBitmap) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);

  MockCanvasResourceDispatcherClient client;
  CanvasResourceDispatcher resource_dispatcher(
      &client, 1 /* client_id */, 1 /* sink_id */,
      1 /* placeholder_canvas_id */, kSize);

  auto provider = CanvasResourceProvider::Create(
      kSize, CanvasResourceProvider::kSoftwareCompositedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */, kColorParams,
      CanvasResourceProvider::kDefaultPresentationMode,
      resource_dispatcher.GetWeakPtr(), true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_TRUE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderDirectGpuMemoryBuffer) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());

  auto provider = CanvasResourceProvider::Create(
      kSize, CanvasResourceProvider::kAcceleratedDirectResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_TRUE(provider->IsSingleBuffered());
}

}  // namespace blink
