// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines tests that implementations of GLImage should pass in order
// to be conformant.

#ifndef UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_
#define UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/test/gl_image_test_support.h"
#include "ui/gl/test/gl_test_helper.h"

namespace gfx {

template <typename GLImageTestDelegate>
class GLImageTest : public testing::Test {
 protected:
  // Overridden from testing::Test:
  void SetUp() override {
    GLImageTestSupport::InitializeGL();
    surface_ = GLSurface::CreateOffscreenGLSurface(Size());
    context_ = GLContext::CreateGLContext(nullptr, surface_.get(),
                                          PreferIntegratedGpu);
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

// Copy image to texture. Support is optional. Texels should be updated if
// supported, and left unchanged if not.
TYPED_TEST_P(GLImageTest, CopyTexSubImage) {
  const Size image_size(256, 256);
  const uint8_t image_color[] = {0, 0xff, 0, 0xff};
  const uint8_t texture_color[] = {0, 0, 0xff, 0xff};

  GLuint framebuffer =
      GLTestHelper::SetupFramebuffer(image_size.width(), image_size.height());
  ASSERT_TRUE(framebuffer);
  glBindFramebufferEXT(GL_FRAMEBUFFER, framebuffer);
  glViewport(0, 0, image_size.width(), image_size.height());

  // Create a solid color green image of preferred format. This must succeed
  // in order for a GLImage to be conformant.
  scoped_refptr<GLImage> image = this->delegate_.CreateSolidColorImage(
      image_size, GLImageTestSupport::GetPreferredInternalFormat(),
      GLImageTestSupport::GetPreferredBufferFormat(), image_color);
  ASSERT_TRUE(image);

  // Create a solid color blue texture of the same size as |image|.
  GLuint texture = GLTestHelper::CreateTexture(GL_TEXTURE_2D);
  scoped_ptr<uint8_t[]> pixels(new uint8_t[BufferSizeForBufferFormat(
      image_size, GLImageTestSupport::GetPreferredBufferFormat())]);
  GLImageTestSupport::SetBufferDataToColor(
      image_size.width(), image_size.height(),
      static_cast<int>(RowSizeForBufferFormat(
          image_size.width(), GLImageTestSupport::GetPreferredBufferFormat(),
          0)),
      GLImageTestSupport::GetPreferredBufferFormat(), texture_color,
      pixels.get());
  // Note: This test assume that |image| can be used with GL_TEXTURE_2D but
  // that might not be the case for some GLImage implementations.
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0,
               GLImageTestSupport::GetPreferredInternalFormat(),
               image_size.width(), image_size.height(), 0,
               GLImageTestSupport::GetPreferredInternalFormat(),
               GL_UNSIGNED_BYTE, pixels.get());

  // Attempt to copy |image| to |texture|.
  // Returns true on success, false on failure.
  bool rv = image->CopyTexSubImage(GL_TEXTURE_2D, Point(), Rect(image_size));

  // clang-format off
  const char kVertexShader[] = STRINGIZE(
    attribute vec2 a_position;
    attribute vec2 a_texCoord;
    varying vec2 v_texCoord;
    void main() {
      gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);
      v_texCoord = a_texCoord;
    }
  );
  const char kFragmentShader[] = STRINGIZE(
    uniform sampler2D a_texture;
    varying vec2 v_texCoord;
    void main() {
      gl_FragColor = texture2D(a_texture, v_texCoord);
    }
  );
  const char kShaderFloatPrecision[] = STRINGIZE(
    precision mediump float;
  );
  // clang-format on

  GLuint vertex_shader =
      GLTestHelper::LoadShader(GL_VERTEX_SHADER, kVertexShader);
  bool is_gles = GetGLImplementation() == kGLImplementationEGLGLES2;
  GLuint fragment_shader = GLTestHelper::LoadShader(
      GL_FRAGMENT_SHADER,
      base::StringPrintf("%s%s", is_gles ? kShaderFloatPrecision : "",
                         kFragmentShader)
          .c_str());
  GLuint program = GLTestHelper::SetupProgram(vertex_shader, fragment_shader);
  EXPECT_NE(program, 0u);
  glUseProgram(program);

  GLint sampler_location = glGetUniformLocation(program, "a_texture");
  ASSERT_NE(sampler_location, -1);
  glUniform1i(sampler_location, 0);

  // clang-format off
  static GLfloat vertices[] = {
    -1.f, -1.f, 0.f, 0.f,
     1.f, -1.f, 1.f, 0.f,
    -1.f,  1.f, 0.f, 1.f,
     1.f,  1.f, 1.f, 1.f
  };
  // clang-format on

  GLuint vertex_buffer;
  glGenBuffersARB(1, &vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  GLint position_location = glGetAttribLocation(program, "a_position");
  ASSERT_NE(position_location, -1);
  glEnableVertexAttribArray(position_location);
  glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE,
                        sizeof(GLfloat) * 4, 0);
  GLint tex_coord_location = glGetAttribLocation(program, "a_texCoord");
  EXPECT_NE(tex_coord_location, -1);
  glEnableVertexAttribArray(tex_coord_location);
  glVertexAttribPointer(tex_coord_location, 2, GL_FLOAT, GL_FALSE,
                        sizeof(GLfloat) * 4,
                        reinterpret_cast<void*>(sizeof(GLfloat) * 2));

  // Draw |texture| to viewport and read back pixels to check expectations.
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  GLTestHelper::CheckPixels(0, 0, image_size.width(), image_size.height(),
                            rv ? image_color : texture_color);

  // Clean up.
  glDeleteProgram(program);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glDeleteBuffersARB(1, &vertex_buffer);
  glDeleteTextures(1, &texture);
  glDeleteFramebuffersEXT(1, &framebuffer);
  image->Destroy(true);
}

// The GLImageTest test case verifies behaviour that is expected from a
// GLImage in order to be conformant.
REGISTER_TYPED_TEST_CASE_P(GLImageTest, CopyTexSubImage);

}  // namespace gfx

#endif  // UI_GL_TEST_GL_IMAGE_TEST_TEMPLATE_H_
