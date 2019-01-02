// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_factory.h"

#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/tests/texture_image_factory.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace {

class SharedImageFactoryTest : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    ASSERT_TRUE(surface_);
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    ASSERT_TRUE(context_);
    bool result = context_->MakeCurrent(surface_.get());
    ASSERT_TRUE(result);

    GpuPreferences preferences;
    preferences.use_passthrough_cmd_decoder = use_passthrough();
    GpuDriverBugWorkarounds workarounds;
    workarounds.max_texture_size = INT_MAX - 1;
    factory_ = std::make_unique<SharedImageFactory>(
        preferences, workarounds, GpuFeatureInfo(), &mailbox_manager_,
        &image_factory_, nullptr);
  }

  void TearDown() override {
    factory_->DestroyAllSharedImages(true);
    factory_.reset();
  }

  bool use_passthrough() {
    return GetParam() && gles2::PassthroughCommandDecoderSupported();
  }

 protected:
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  gles2::MailboxManagerImpl mailbox_manager_;
  TextureImageFactory image_factory_;
  std::unique_ptr<SharedImageFactory> factory_;
};

TEST_P(SharedImageFactoryTest, Basic) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  EXPECT_TRUE(
      factory_->CreateSharedImage(mailbox, format, size, color_space, usage));
  TextureBase* texture_base = mailbox_manager_.ConsumeTexture(mailbox);
  ASSERT_TRUE(texture_base);
  GLenum expected_target = GL_TEXTURE_2D;
  EXPECT_EQ(texture_base->target(), expected_target);
  if (!use_passthrough()) {
    auto* texture = static_cast<gles2::Texture*>(texture_base);
    EXPECT_TRUE(texture->IsImmutable());
    int width, height, depth;
    bool has_level =
        texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, &depth);
    EXPECT_TRUE(has_level);
    EXPECT_EQ(width, size.width());
    EXPECT_EQ(height, size.height());
  }

  EXPECT_TRUE(factory_->DestroySharedImage(mailbox));
  EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));
}

TEST_P(SharedImageFactoryTest, Image) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_SCANOUT;
  EXPECT_TRUE(
      factory_->CreateSharedImage(mailbox, format, size, color_space, usage));
  TextureBase* texture_base = mailbox_manager_.ConsumeTexture(mailbox);
  ASSERT_TRUE(texture_base);
  GLenum target = texture_base->target();
  scoped_refptr<gl::GLImage> image;
  if (use_passthrough()) {
    auto* texture = static_cast<gles2::TexturePassthrough*>(texture_base);
    image = texture->GetLevelImage(target, 0);
  } else {
    auto* texture = static_cast<gles2::Texture*>(texture_base);
    image = texture->GetLevelImage(target, 0);
  }
  ASSERT_TRUE(image);
  EXPECT_EQ(size, image->GetSize());
}

TEST_P(SharedImageFactoryTest, DuplicateMailbox) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  EXPECT_TRUE(
      factory_->CreateSharedImage(mailbox, format, size, color_space, usage));
  EXPECT_FALSE(
      factory_->CreateSharedImage(mailbox, format, size, color_space, usage));
}

TEST_P(SharedImageFactoryTest, DestroyInexistentMailbox) {
  auto mailbox = Mailbox::Generate();
  EXPECT_FALSE(factory_->DestroySharedImage(mailbox));
}

TEST_P(SharedImageFactoryTest, InvalidFormat) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::ResourceFormat::UYVY_422;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  EXPECT_FALSE(
      factory_->CreateSharedImage(mailbox, format, size, color_space, usage));
}

TEST_P(SharedImageFactoryTest, InvalidSize) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(0, 0);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  EXPECT_FALSE(
      factory_->CreateSharedImage(mailbox, format, size, color_space, usage));

  size = gfx::Size(INT_MAX, INT_MAX);
  EXPECT_FALSE(
      factory_->CreateSharedImage(mailbox, format, size, color_space, usage));
}

INSTANTIATE_TEST_CASE_P(Service, SharedImageFactoryTest, ::testing::Bool());

}  // anonymous namespace
}  // namespace gpu
