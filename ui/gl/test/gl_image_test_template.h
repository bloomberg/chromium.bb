// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines tests that implementations of GLImage should pass in order
// to be conformant.

#ifndef UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_
#define UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_

#include <stdint.h>

#include "base/memory/scoped_ptr.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_helper.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/test/gl_image_test_support.h"
#include "ui/gl/test/gl_test_helper.h"

namespace gl {
namespace {

std::string PrependFragmentSamplerType(unsigned target,
                                       std::string shader_string) {
  switch (target) {
    case GL_TEXTURE_2D:
      DCHECK_NE(shader_string.find("SamplerType"), std::string::npos);
      DCHECK_NE(shader_string.find("TextureLookup"), std::string::npos);
      return "#define SamplerType sampler2D\n"
             "#define TextureLookup texture2D\n" +
             shader_string;
    case GL_TEXTURE_RECTANGLE_ARB:
      DCHECK_NE(shader_string.find("SamplerType"), std::string::npos);
      DCHECK_NE(shader_string.find("TextureLookup"), std::string::npos);
      return "#extension GL_ARB_texture_rectangle : require\n"
             "#define SamplerType sampler2DRect\n"
             "#define TextureLookup texture2DRect\n" +
             shader_string;
    default:
      NOTREACHED();
      break;
  }
  return shader_string;
}

}  // namespace

template <typename GLImageTestDelegate>
class GLImageTest : public testing::Test {
 protected:
  // Overridden from testing::Test:
  void SetUp() override {
    GLImageTestSupport::InitializeGL();
    surface_ = gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size());
    context_ = gfx::GLContext::CreateGLContext(nullptr, surface_.get(),
                                               gfx::PreferIntegratedGpu);
    context_->MakeCurrent(surface_.get());
  }
  void TearDown() override {
    context_->ReleaseCurrent(surface_.get());
    context_ = nullptr;
    surface_ = nullptr;
    GLImageTestSupport::CleanupGL();
  }

 protected:
  scoped_refptr<gfx::GLSurface> surface_;
  scoped_refptr<gfx::GLContext> context_;
  GLImageTestDelegate delegate_;
};

TYPED_TEST_CASE_P(GLImageTest);

TYPED_TEST_P(GLImageTest, CreateAndDestroy) {
  const gfx::Size small_image_size(4, 4);
  const gfx::Size large_image_size(512, 512);
  const uint8_t image_color[] = {0, 0xff, 0, 0xff};

  // Create a small solid color green image of preferred format. This must
  // succeed in order for a GLImage to be conformant.
  scoped_refptr<gl::GLImage> small_image =
      this->delegate_.CreateSolidColorImage(small_image_size, image_color);
  ASSERT_TRUE(small_image);

  // Create a large solid color green image of preferred format. This must
  // succeed in order for a GLImage to be conformant.
  scoped_refptr<gl::GLImage> large_image =
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
class GLImageCopyTest : public GLImageTest<GLImageTestDelegate> {};

TYPED_TEST_CASE_P(GLImageCopyTest);

TYPED_TEST_P(GLImageCopyTest, CopyTexImage) {
  const gfx::Size image_size(256, 256);
  // These values are picked so that RGB -> YUV on the CPU converted
  // back to RGB on the GPU produces the original RGB values without
  // any error.
  const uint8_t image_color[] = {0x10, 0x20, 0, 0xff};
  const uint8_t texture_color[] = {0, 0, 0xff, 0xff};

  GLuint framebuffer =
      GLTestHelper::SetupFramebuffer(image_size.width(), image_size.height());
  ASSERT_TRUE(framebuffer);
  glBindFramebufferEXT(GL_FRAMEBUFFER, framebuffer);
  glViewport(0, 0, image_size.width(), image_size.height());

  // Create a solid color green image of preferred format. This must succeed
  // in order for a GLImage to be conformant.
  scoped_refptr<gl::GLImage> image =
      this->delegate_.CreateSolidColorImage(image_size, image_color);
  ASSERT_TRUE(image);

  // Create a solid color blue texture of the same size as |image|.
  unsigned target = this->delegate_.GetTextureTarget();
  GLuint texture = GLTestHelper::CreateTexture(target);
  scoped_ptr<uint8_t[]> pixels(new uint8_t[BufferSizeForBufferFormat(
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

  // clang-format off
  const char kVertexShader[] = STRINGIZE(
    attribute vec2 a_position;
    varying vec2 v_texCoord;
    void main() {
      gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);
      v_texCoord = (a_position + vec2(1.0, 1.0)) * 0.5;
    }
  );
  const char kFragmentShader[] = STRINGIZE(
    uniform SamplerType a_texture;
    varying vec2 v_texCoord;
    void main() {
      gl_FragColor = TextureLookup(a_texture, v_texCoord);
    }
  );
  const char kShaderFloatPrecision[] = STRINGIZE(
    precision mediump float;
  );
  // clang-format on

  GLuint vertex_shader =
      gfx::GLHelper::LoadShader(GL_VERTEX_SHADER, kVertexShader);
  bool is_gles = gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2;
  GLuint fragment_shader = gfx::GLHelper::LoadShader(
      GL_FRAGMENT_SHADER,
      base::StringPrintf(
          "%s\n%s", is_gles ? kShaderFloatPrecision : "",
          PrependFragmentSamplerType(target, kFragmentShader).c_str())
          .c_str());
  GLuint program = gfx::GLHelper::SetupProgram(vertex_shader, fragment_shader);
  EXPECT_NE(program, 0u);
  glUseProgram(program);

  GLint sampler_location = glGetUniformLocation(program, "a_texture");
  ASSERT_NE(sampler_location, -1);
  glUniform1i(sampler_location, 0);

  GLuint vertex_buffer = gfx::GLHelper::SetupQuadVertexBuffer();
  // Draw |texture| to viewport and read back pixels to check expectations.
  gfx::GLHelper::DrawQuad(vertex_buffer);

  GLTestHelper::CheckPixels(0, 0, image_size.width(), image_size.height(),
                            image_color);

  // Clean up.
  glDeleteProgram(program);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glDeleteBuffersARB(1, &vertex_buffer);
  glDeleteTextures(1, &texture);
  glDeleteFramebuffersEXT(1, &framebuffer);
  image->Destroy(true);
}

// The GLImageCopyTest test case verifies that the GLImage implementation
// handles CopyTexImage correctly.
REGISTER_TYPED_TEST_CASE_P(GLImageCopyTest, CopyTexImage);

}  // namespace gl

#endif  // UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_
