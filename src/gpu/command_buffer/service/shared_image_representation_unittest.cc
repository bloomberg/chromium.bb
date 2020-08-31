// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/test_shared_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"

namespace gpu {

class SharedImageRepresentationTest : public ::testing::Test {
 public:
  void SetUp() override {
    tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    mailbox_ = Mailbox::GenerateForSharedImage();
    auto format = viz::ResourceFormat::RGBA_8888;
    gfx::Size size(256, 256);
    auto color_space = gfx::ColorSpace::CreateSRGB();
    uint32_t usage = SHARED_IMAGE_USAGE_GLES2;

    auto backing = std::make_unique<TestSharedImageBacking>(
        mailbox_, format, size, color_space, usage, 0 /* estimated_size */);
    factory_ref_ = manager_.Register(std::move(backing), tracker_.get());
  }

 protected:
  gpu::Mailbox mailbox_;
  SharedImageManager manager_;
  std::unique_ptr<MemoryTypeTracker> tracker_;
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref_;
};

TEST_F(SharedImageRepresentationTest, GLTextureClearing) {
  auto representation = manager_.ProduceGLTexture(mailbox_, tracker_.get());
  EXPECT_FALSE(representation->IsCleared());

  // We should not be able to begin access when |allow_uncleared| is false.
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_FALSE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // Begin/End access should not modify clear status on its own.
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // Clearing underlying GL texture should clear the SharedImage.
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(scoped_access);
    representation->GetTexture()->SetLevelCleared(GL_TEXTURE_2D, 0,
                                                  true /* cleared */);
  }
  EXPECT_TRUE(representation->IsCleared());

  // We can now begin access with |allow_uncleared| == false.
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_TRUE(scoped_access);
  }

  // Reset the representation to uncleared. This should unclear the texture on
  // BeginAccess.
  representation->SetClearedRect(gfx::Rect());
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(scoped_access);
    EXPECT_FALSE(
        representation->GetTexture()->IsLevelCleared(GL_TEXTURE_2D, 0));
  }
  EXPECT_FALSE(representation->IsCleared());
}

TEST_F(SharedImageRepresentationTest, GLTexturePassthroughClearing) {
  auto representation =
      manager_.ProduceGLTexturePassthrough(mailbox_, tracker_.get());
  EXPECT_FALSE(representation->IsCleared());

  // We should not be able to begin access when |allow_uncleared| is false.
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_FALSE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // Begin/End access will not clear the representation on its own.
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // Clear the SharedImage.
  representation->SetCleared();
  EXPECT_TRUE(representation->IsCleared());

  // We can now begin accdess with |allow_uncleared| == false.
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_TRUE(scoped_access);
  }
}

TEST_F(SharedImageRepresentationTest, SkiaClearing) {
  auto representation = manager_.ProduceSkia(mailbox_, tracker_.get(), nullptr);
  EXPECT_FALSE(representation->IsCleared());

  // We should not be able to begin read access.
  {
    auto scoped_access =
        representation->BeginScopedReadAccess(nullptr, nullptr);
    EXPECT_FALSE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // We should not be able to begin write access when |allow_uncleared| is
  // false.
  {
    auto scoped_access = representation->BeginScopedWriteAccess(
        nullptr, nullptr, SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_FALSE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // We can begin write access when |allow_uncleared| is true.
  {
    auto scoped_access = representation->BeginScopedWriteAccess(
        nullptr, nullptr,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // Clear the SharedImage.
  representation->SetCleared();
  EXPECT_TRUE(representation->IsCleared());

  // We can now begin read access.
  {
    auto scoped_access =
        representation->BeginScopedReadAccess(nullptr, nullptr);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_TRUE(representation->IsCleared());

  // We can also begin write access with |allow_uncleared| == false.
  {
    auto scoped_access = representation->BeginScopedWriteAccess(
        nullptr, nullptr, SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_TRUE(representation->IsCleared());
}

TEST_F(SharedImageRepresentationTest, DawnClearing) {
  auto representation =
      manager_.ProduceDawn(mailbox_, tracker_.get(), nullptr /* device */);
  EXPECT_FALSE(representation->IsCleared());

  // We should not be able to begin access with |allow_uncleared| == false.
  {
    auto scoped_access = representation->BeginScopedAccess(
        WGPUTextureUsage_None,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_FALSE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // We can begin access when |allow_uncleared| is true.
  {
    auto scoped_access = representation->BeginScopedAccess(
        WGPUTextureUsage_None,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // Clear the SharedImage.
  representation->SetCleared();
  EXPECT_TRUE(representation->IsCleared());

  // We can also begin access with |allow_uncleared| == false.
  {
    auto scoped_access = representation->BeginScopedAccess(
        WGPUTextureUsage_None,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_TRUE(representation->IsCleared());
}

TEST_F(SharedImageRepresentationTest, OverlayClearing) {
  auto representation = manager_.ProduceOverlay(mailbox_, tracker_.get());
  EXPECT_FALSE(representation->IsCleared());

  // We should not be able to begin read ccess.
  {
    auto scoped_access =
        representation->BeginScopedReadAccess(false /* needs_gl_image */);
    EXPECT_FALSE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // Clear the SharedImage.
  representation->SetCleared();
  EXPECT_TRUE(representation->IsCleared());

  // We can now begin read access.
  {
    auto scoped_access =
        representation->BeginScopedReadAccess(false /* needs_gl_image */);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_TRUE(representation->IsCleared());
}

}  // namespace gpu
