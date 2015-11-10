// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/test/gl_test_helper.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gl {

// static
GLuint GLTestHelper::CreateTexture(GLenum target) {
  // Create the texture object.
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(target, texture);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  return texture;
}

// static
GLuint GLTestHelper::CompileShader(GLenum type, const char* src) {
  GLuint shader = glCreateShader(type);
  // Load the shader source.
  glShaderSource(shader, 1, &src, nullptr);
  // Compile the shader.
  glCompileShader(shader);
  return shader;
}

// static
GLuint GLTestHelper::LoadShader(GLenum type, const char* src) {
  GLuint shader = CompileShader(type, src);

  // Check the compile status.
  GLint value = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
  if (!value) {
    char buffer[1024];
    GLsizei length = 0;
    glGetShaderInfoLog(shader, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    EXPECT_EQ(1, value) << "Error compiling shader: " << log;
    glDeleteShader(shader);
    shader = 0;
  }
  return shader;
}

// static
GLuint GLTestHelper::LinkProgram(GLuint vertex_shader, GLuint fragment_shader) {
  // Create the program object.
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  // Link the program.
  glLinkProgram(program);
  return program;
}

// static
GLuint GLTestHelper::SetupProgram(GLuint vertex_shader,
                                  GLuint fragment_shader) {
  GLuint program = LinkProgram(vertex_shader, fragment_shader);
  // Check the link status.
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (!linked) {
    char buffer[1024];
    GLsizei length = 0;
    glGetProgramInfoLog(program, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    EXPECT_EQ(1, linked) << "Error linking program: " << log;
    glDeleteProgram(program);
    program = 0;
  }
  return program;
}

// static
GLuint GLTestHelper::SetupFramebuffer(int width, int height) {
  GLuint color_buffer_texture = CreateTexture(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, color_buffer_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  GLuint framebuffer = 0;
  glGenFramebuffersEXT(1, &framebuffer);
  glBindFramebufferEXT(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            color_buffer_texture, 0);
  if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              glCheckFramebufferStatusEXT(GL_FRAMEBUFFER))
        << "Error setting up framebuffer";
    glDeleteFramebuffersEXT(1, &framebuffer);
    framebuffer = 0;
  }
  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  glDeleteTextures(1, &color_buffer_texture);
  return framebuffer;
}

// static
bool GLTestHelper::CheckPixels(int x,
                               int y,
                               int width,
                               int height,
                               const uint8_t expected_color[4]) {
  int size = width * height * 4;
  scoped_ptr<uint8_t[]> pixels(new uint8_t[size]);
  const uint8_t kCheckClearValue = 123u;
  memset(pixels.get(), kCheckClearValue, size);
  glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
  int bad_count = 0;
  for (int yy = 0; yy < height; ++yy) {
    for (int xx = 0; xx < width; ++xx) {
      int offset = yy * width * 4 + xx * 4;
      for (int jj = 0; jj < 4; ++jj) {
        uint8_t actual = pixels[offset + jj];
        uint8_t expected = expected_color[jj];
        EXPECT_EQ(expected, actual) << " at " << (xx + x) << ", " << (yy + y)
                                    << " channel " << jj;
        bad_count += actual != expected;
        // Exit early just so we don't spam the log but we print enough to
        // hopefully make it easy to diagnose the issue.
        if (bad_count > 16)
          return false;
      }
    }
  }

  return !bad_count;
}

}  // namespace gl
