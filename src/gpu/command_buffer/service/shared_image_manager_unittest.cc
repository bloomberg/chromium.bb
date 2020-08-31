// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_manager.h"

#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/test_shared_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/tests/texture_image_factory.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace {

TEST(SharedImageManagerTest, BasicRefCounting) {
  const size_t kSizeBytes = 1024;
  SharedImageManager manager;
  auto tracker = std::make_unique<MemoryTypeTracker>(nullptr);

  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;

  auto backing = std::make_unique<TestSharedImageBacking>(
      mailbox, format, size, color_space, usage, kSizeBytes);

  auto factory_ref = manager.Register(std::move(backing), tracker.get());
  EXPECT_EQ(kSizeBytes, tracker->GetMemRepresented());

  // Taking/releasing an additional ref/representation with the same tracker
  // should have no impact.
  {
    auto gl_representation = manager.ProduceGLTexture(mailbox, tracker.get());
    EXPECT_EQ(kSizeBytes, tracker->GetMemRepresented());
  }

  // Taking/releasing an additional ref/representation with a new tracker should
  // also have no impact.
  {
    auto tracker2 = std::make_unique<MemoryTypeTracker>(nullptr);
    auto gl_representation = manager.ProduceGLTexture(mailbox, tracker2.get());
    EXPECT_EQ(kSizeBytes, tracker->GetMemRepresented());
    EXPECT_EQ(0u, tracker2->GetMemRepresented());
  }

  factory_ref.reset();
  EXPECT_EQ(0u, tracker->GetMemRepresented());
}

TEST(SharedImageManagerTest, TransferRefSameTracker) {
  const size_t kSizeBytes = 1024;
  SharedImageManager manager;
  auto tracker = std::make_unique<MemoryTypeTracker>(nullptr);

  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;

  auto backing = std::make_unique<TestSharedImageBacking>(
      mailbox, format, size, color_space, usage, kSizeBytes);

  auto factory_ref = manager.Register(std::move(backing), tracker.get());
  EXPECT_EQ(kSizeBytes, tracker->GetMemRepresented());

  // Take an additional ref/representation.
  auto gl_representation = manager.ProduceGLTexture(mailbox, tracker.get());

  // Releasing the original ref should have no impact.
  factory_ref.reset();
  EXPECT_EQ(kSizeBytes, tracker->GetMemRepresented());

  gl_representation.reset();
  EXPECT_EQ(0u, tracker->GetMemRepresented());
}

TEST(SharedImageManagerTest, TransferRefNewTracker) {
  const size_t kSizeBytes = 1024;
  SharedImageManager manager;
  auto tracker = std::make_unique<MemoryTypeTracker>(nullptr);
  auto tracker2 = std::make_unique<MemoryTypeTracker>(nullptr);

  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;

  auto backing = std::make_unique<TestSharedImageBacking>(
      mailbox, format, size, color_space, usage, kSizeBytes);

  auto factory_ref = manager.Register(std::move(backing), tracker.get());
  EXPECT_EQ(kSizeBytes, tracker->GetMemRepresented());

  // Take an additional ref/representation with a new tracker. Memory should
  // stay accounted to the original tracker.
  auto gl_representation = manager.ProduceGLTexture(mailbox, tracker2.get());
  EXPECT_EQ(kSizeBytes, tracker->GetMemRepresented());
  EXPECT_EQ(0u, tracker2->GetMemRepresented());

  // Releasing the original should transfer memory to the new tracker.
  factory_ref.reset();
  EXPECT_EQ(0u, tracker->GetMemRepresented());
  EXPECT_EQ(kSizeBytes, tracker2->GetMemRepresented());

  // We can now safely destroy the original tracker.
  tracker.reset();

  gl_representation.reset();
  EXPECT_EQ(0u, tracker2->GetMemRepresented());
}

}  // anonymous namespace
}  // namespace gpu
