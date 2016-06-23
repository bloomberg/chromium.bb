// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines tests that implementations of GLImage should pass in order
// to be conformant.

#ifndef UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_
#define UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_

#include <stdint.h>

#include <memory>

#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_helper.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_image_test_support.h"
#include "ui/gl/test/gl_test_helper.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

namespace gl {
namespace {

// Compiles a fragment shader for sampling out of a texture of |size| bound to
// |target| and checks for compilation errors.
GLuint LoadFragmentShader(unsigned target, const gfx::Size& size) {
  // clang-format off
  const char kFragmentShader[] = STRINGIZE(
    uniform SamplerType a_texture;
    varying vec2 v_texCoord;
    void main() {
      gl_FragColor = TextureLookup(a_texture, v_texCoord * TextureScale);
    }
  );
  const char kShaderFloatPrecision[] = STRINGIZE(
    precision mediump float;
  );
  // clang-format on

  bool is_gles = GetGLImplementation() == kGLImplementationEGLGLES2;
  switch (target) {
    case GL_TEXTURE_2D:
      return GLHelper::LoadShader(
          GL_FRAGMENT_SHADER,
          base::StringPrintf("%s\n"
                             "#define SamplerType sampler2D\n"
                             "#define TextureLookup texture2D\n"
                             "#define TextureScale vec2(1.0, 1.0)\n"
                             "%s",
                             is_gles ? kShaderFloatPrecision : "",
                             kFragmentShader)
              .c_str());
    case GL_TEXTURE_RECTANGLE_ARB:
      return GLHelper::LoadShader(
          GL_FRAGMENT_SHADER,
          base::StringPrintf("#extension GL_ARB_texture_rectangle : require\n"
                             "%s\n"
                             "#define SamplerType sampler2DRect\n"
                             "#define TextureLookup texture2DRect\n"
                             "#define TextureScale vec2(%f, %f)\n"
                             "%s",
                             is_gles ? kShaderFloatPrecision : "",
                             static_cast<double>(size.width()),
                             static_cast<double>(size.height()),
                             kFragmentShader)
              .c_str());
    case GL_TEXTURE_EXTERNAL_OES:
      return GLHelper::LoadShader(
          GL_FRAGMENT_SHADER,
          base::StringPrintf("#extension GL_OES_EGL_image_external : require\n"
                             "%s\n"
                             "#define SamplerType samplerExternalOES\n"
                             "#define TextureLookup texture2D\n"
                             "#define TextureScale vec2(1.0, 1.0)\n"
                             "%s",
                             is_gles ? kShaderFloatPrecision : "",
                             kFragmentShader)
              .c_str());
    default:
      NOTREACHED();
      return 0;
  }
}

// Draws texture bound to |target| of texture unit 0 to the currently bound
// frame buffer.
void DrawTextureQuad(GLenum target, const gfx::Size& size) {
  // clang-format off
  const char kVertexShader[] = STRINGIZE(
    attribute vec2 a_position;
    varying vec2 v_texCoord;
    void main() {
      gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);
      v_texCoord = (a_position + vec2(1.0, 1.0)) * 0.5;
    }
  );
  // clang-format on

  GLuint vao = 0;
  if (GLHelper::ShouldTestsUseVAOs()) {
    glGenVertexArraysOES(1, &vao);
    glBindVertexArrayOES(vao);
  }

  GLuint vertex_shader = GLHelper::LoadShader(GL_VERTEX_SHADER, kVertexShader);
  GLuint fragment_shader = LoadFragmentShader(target, size);
  GLuint program = GLHelper::SetupProgram(vertex_shader, fragment_shader);
  EXPECT_NE(program, 0u);
  glUseProgram(program);

  GLint sampler_location = glGetUniformLocation(program, "a_texture");
  ASSERT_NE(sampler_location, -1);
  glUniform1i(sampler_location, 0);

  GLuint vertex_buffer = GLHelper::SetupQuadVertexBuffer();
  GLHelper::DrawQuad(vertex_buffer);

  if (vao != 0) {
    glDeleteVertexArraysOES(1, &vao);
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glDeleteProgram(program);
  glDeleteBuffersARB(1, &vertex_buffer);
}

}  // namespace

template <typename GLImageTestDelegate>
class GLImageTest : public testing::Test {
 protected:
  // Overridden from testing::Test:
  void SetUp() override {
    GLImageTestSupport::InitializeGL();
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    context_ =
        gl::init::CreateGLContext(nullptr, surface_.get(), PreferIntegratedGpu);
    context_->MakeCurrent(surface_.get());
  }
  void TearDown() override {
    context_->ReleaseCurrent(surface_.get());
    context_ = nullptr;
    surface_ = nullptr;
    GLImageTestSupport::CleanupGL();
  }

 protected:
  scoped_refptr<GLSurface> surface_;
  scoped_refptr<GLContext> context_;
  GLImageTestDelegate delegate_;
};

TYPED_TEST_CASE_P(GLImageTest);

TYPED_TEST_P(GLImageTest, CreateAndDestroy) {
  const gfx::Size small_image_size(4, 4);
  const gfx::Size large_image_size(512, 512);
  const uint8_t* image_color = this->delegate_.GetImageColor();

  // Create a small solid color green image of preferred format. This must
  // succeed in order for a GLImage to be conformant.
  scoped_refptr<GLImage> small_image =
      this->delegate_.CreateSolidColorImage(small_image_size, image_color);
  ASSERT_TRUE(small_image);

  // Create a large solid color green image of preferred format. This must
  // succeed in order for a GLImage to be conformant.
  scoped_refptr<GLImage> large_image =
      this->delegate_.CreateSolidColorImage(large_image_size, image_color);
  ASSERT_TRUE(large_image);

  // Verify that image size is correct.
  EXPECT_EQ(small_image->GetSize().ToString(), small_image_size.ToString());
  EXPECT_EQ(large_image->GetSize().ToString(), large_image_size.ToString());

  // Verify that destruction of images work correctly both when we have a
  // context and when we don't.
  small_image->Destroy(true /* have_context */);
  large_image->Destroy(false /* have_context */);
}

// The GLImageTest test case verifies the behaviour that is expected from a
// GLImage in order to be conformant.
REGISTER_TYPED_TEST_CASE_P(GLImageTest, CreateAndDestroy);

template <typename GLImageTestDelegate>
class GLImageZeroInitializeTest : public GLImageTest<GLImageTestDelegate> {};

// This test verifies that if an uninitialized image is bound to a texture, the
// result is zero-initialized.
TYPED_TEST_CASE_P(GLImageZeroInitializeTest);

TYPED_TEST_P(GLImageZeroInitializeTest, ZeroInitialize) {
#if defined(OS_MACOSX)
  // This functionality is disabled on Mavericks because it breaks PDF
  // rendering. https://crbug.com/594343.
  if (base::mac::IsOSMavericks())
    return;
#endif

  const gfx::Size image_size(256, 256);

  GLuint framebuffer =
      GLTestHelper::SetupFramebuffer(image_size.width(), image_size.height());
  ASSERT_TRUE(framebuffer);
  glBindFramebufferEXT(GL_FRAMEBUFFER, framebuffer);
  glViewport(0, 0, image_size.width(), image_size.height());

  // Create an uninitialized image of preferred format.
  scoped_refptr<GLImage> image = this->delegate_.CreateImage(image_size);

  // Create a texture that |image| will be bound to.
  GLenum target = this->delegate_.GetTextureTarget();
  GLuint texture = GLTestHelper::CreateTexture(target);
  glBindTexture(target, texture);

  // Bind |image| to |texture|.
  bool rv = image->BindTexImage(target);
  EXPECT_TRUE(rv);

  // Draw |texture| to viewport.
  DrawTextureQuad(target, image_size);

  // Release |image| from |texture|.
  image->ReleaseTexImage(target);

  // Read back pixels to check expectations.
  const uint8_t zero_color[] = {0, 0, 0, 0};
  GLTestHelper::CheckPixels(0, 0, image_size.width(), image_size.height(),
                            zero_color);

  // Clean up.
  glDeleteTextures(1, &texture);
  glDeleteFramebuffersEXT(1, &framebuffer);
  image->Destroy(true /* have_context */);
}

REGISTER_TYPED_TEST_CASE_P(GLImageZeroInitializeTest, ZeroInitialize);

template <typename GLImageTestDelegate>
class GLImageBindTest : public GLImageTest<GLImageTestDelegate> {};

TYPED_TEST_CASE_P(GLImageBindTest);

TYPED_TEST_P(GLImageBindTest, BindTexImage) {
  const gfx::Size image_size(256, 256);
  const uint8_t* image_color = this->delegate_.GetImageColor();

  GLuint framebuffer =
      GLTestHelper::SetupFramebuffer(image_size.width(), image_size.height());
  ASSERT_TRUE(framebuffer);
  glBindFramebufferEXT(GL_FRAMEBUFFER, framebuffer);
  glViewport(0, 0, image_size.width(), image_size.height());

  // Create a solid color green image of preferred format. This must succeed
  // in order for a GLImage to be conformant.
  scoped_refptr<GLImage> image =
      this->delegate_.CreateSolidColorImage(image_size, image_color);
  ASSERT_TRUE(image);

  // Initialize a blue texture of the same size as |image|.
  unsigned target = this->delegate_.GetTextureTarget();
  GLuint texture = GLTestHelper::CreateTexture(target);
  glBindTexture(target, texture);

  // Bind |image| to |texture|.
  bool rv = image->BindTexImage(target);
  EXPECT_TRUE(rv);

  glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  // Draw |texture| to viewport.
  DrawTextureQuad(target, image_size);

  // Read back pixels to check expectations.
  GLTestHelper::CheckPixels(0, 0, image_size.width(), image_size.height(),
                            image_color);

  // Clean up.
  glDeleteTextures(1, &texture);
  glDeleteFramebuffersEXT(1, &framebuffer);
  image->Destroy(true /* have_context */);
}

REGISTER_TYPED_TEST_CASE_P(GLImageBindTest, BindTexImage);

template <typename GLImageTestDelegate>
class GLImageCopyTest : public GLImageTest<GLImageTestDelegate> {};

TYPED_TEST_CASE_P(GLImageCopyTest);

TYPED_TEST_P(GLImageCopyTest, CopyTexImage) {
  const gfx::Size image_size(256, 256);
  const uint8_t* image_color = this->delegate_.GetImageColor();
  const uint8_t texture_color[] = {0, 0, 0xff, 0xff};

  GLuint framebuffer =
      GLTestHelper::SetupFramebuffer(image_size.width(), image_size.height());
  ASSERT_TRUE(framebuffer);
  glBindFramebufferEXT(GL_FRAMEBUFFER, framebuffer);
  glViewport(0, 0, image_size.width(), image_size.height());

  // Create a solid color green image of preferred format. This must succeed
  // in order for a GLImage to be conformant.
  scoped_refptr<GLImage> image =
      this->delegate_.CreateSolidColorImage(image_size, image_color);
  ASSERT_TRUE(image);

  // Create a solid color blue texture of the same size as |image|.
  unsigned target = this->delegate_.GetTextureTarget();
  GLuint texture = GLTestHelper::CreateTexture(target);
  std::unique_ptr<uint8_t[]> pixels(new uint8_t[BufferSizeForBufferFormat(
      image_size, gfx::BufferFormat::RGBA_8888)]);
  GLImageTestSupport::SetBufferDataToColor(
      image_size.width(), image_size.height(),
      static_cast<int>(RowSizeForBufferFormat(image_size.width(),
                                              gfx::BufferFormat::RGBA_8888, 0)),
      0, gfx::BufferFormat::RGBA_8888, texture_color, pixels.get());
  glBindTexture(target, texture);
  glTexImage2D(target, 0, GL_RGBA, image_size.width(), image_size.height(), 0,
               GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());

  // Copy |image| to |texture|.
  bool rv = image->CopyTexImage(target);
  EXPECT_TRUE(rv);

  // Draw |texture| to viewport.
  DrawTextureQuad(target, image_size);

  // Read back pixels to check expectations.
  GLTestHelper::CheckPixels(0, 0, image_size.width(), image_size.height(),
                            image_color);

  // Clean up.
  glDeleteTextures(1, &texture);
  glDeleteFramebuffersEXT(1, &framebuffer);
  image->Destroy(true /* have_context */);
}

// The GLImageCopyTest test case verifies that the GLImage implementation
// handles CopyTexImage correctly.
REGISTER_TYPED_TEST_CASE_P(GLImageCopyTest, CopyTexImage);

}  // namespace gl

#endif  // UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_
