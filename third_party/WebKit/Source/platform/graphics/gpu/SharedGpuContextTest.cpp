// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/gpu/SharedGpuContext.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/graphics/Canvas2DLayerBridge.h"
#include "platform/graphics/gpu/AcceleratedImageBufferSurface.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using ::testing::Test;

namespace blink {

namespace {

class SharedGpuContextTest : public Test {
 public:
  void SetUp() override {
    SharedGpuContext::SetContextProviderFactoryForTesting([this] {
      gl_.SetIsContextLost(false);
      return std::unique_ptr<WebGraphicsContext3DProvider>(
          new FakeWebGraphicsContext3DProvider(&gl_));
    });
  }

  void TearDown() override {
    SharedGpuContext::SetContextProviderFactoryForTesting(nullptr);
  }

  FakeGLES2Interface gl_;
};

// Test fixure that simulate a graphics context creation failure
class BadSharedGpuContextTest : public Test {
 public:
  void SetUp() override {
    SharedGpuContext::SetContextProviderFactoryForTesting(
        [] { return std::unique_ptr<WebGraphicsContext3DProvider>(nullptr); });
  }

  void TearDown() override {
    SharedGpuContext::SetContextProviderFactoryForTesting(nullptr);
  }
};

TEST_F(SharedGpuContextTest, contextLossAutoRecovery) {
  EXPECT_NE(SharedGpuContext::ContextProviderWrapper(), nullptr);
  WeakPtr<WebGraphicsContext3DProviderWrapper> context =
      SharedGpuContext::ContextProviderWrapper();
  gl_.SetIsContextLost(true);
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoring());
  EXPECT_TRUE(!!context);

  // Context recreation results in old provider being discarded.
  EXPECT_TRUE(!!SharedGpuContext::ContextProviderWrapper());
  EXPECT_FALSE(!!context);
}

TEST_F(SharedGpuContextTest, AccelerateImageBufferSurfaceAutoRecovery) {
  // Verifies that after a context loss, attempting to allocate an
  // AcceleratedImageBufferSurface will restore the context and succeed
  gl_.SetIsContextLost(true);
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoring());
  IntSize size(10, 10);
  std::unique_ptr<ImageBufferSurface> surface =
      WTF::WrapUnique(new AcceleratedImageBufferSurface(size, kNonOpaque));
  EXPECT_TRUE(surface->IsValid());
  EXPECT_TRUE(SharedGpuContext::IsValidWithoutRestoring());
}

TEST_F(SharedGpuContextTest, Canvas2DLayerBridgeAutoRecovery) {
  // Verifies that after a context loss, attempting to allocate a
  // Canvas2DLayerBridge will restore the context and succeed.
  gl_.SetIsContextLost(true);
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoring());
  IntSize size(10, 10);
  CanvasColorParams color_params;
  RefPtr<Canvas2DLayerBridge> bridge = AdoptRef(new Canvas2DLayerBridge(
      size, 0, /*msaa sample count*/
      kNonOpaque, Canvas2DLayerBridge::kEnableAcceleration, color_params));
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_TRUE(SharedGpuContext::IsValidWithoutRestoring());
  bridge->BeginDestruction();
}

TEST_F(SharedGpuContextTest, IsValidWithoutRestoring) {
  EXPECT_NE(SharedGpuContext::ContextProviderWrapper(), nullptr);
  EXPECT_TRUE(SharedGpuContext::IsValidWithoutRestoring());
}

TEST_F(BadSharedGpuContextTest, IsValidWithoutRestoring) {
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoring());
}

TEST_F(BadSharedGpuContextTest, AllowSoftwareToAcceleratedCanvasUpgrade) {
  EXPECT_FALSE(SharedGpuContext::AllowSoftwareToAcceleratedCanvasUpgrade());
}

TEST_F(BadSharedGpuContextTest, AccelerateImageBufferSurfaceCreationFails) {
  // With a bad shared context, AccelerateImageBufferSurface creation should
  // fail gracefully
  IntSize size(10, 10);
  std::unique_ptr<ImageBufferSurface> surface =
      WTF::WrapUnique(new AcceleratedImageBufferSurface(size, kNonOpaque));
  EXPECT_FALSE(surface->IsValid());
}

}  // unnamed namespace

}  // blink
