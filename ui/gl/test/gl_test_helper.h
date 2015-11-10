// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_TEST_GL_TEST_HELPER_H_
#define UI_GL_TEST_GL_TEST_HELPER_H_

#include "base/basictypes.h"
#include "ui/gl/gl_bindings.h"

namespace gl {

class GLTestHelper {
 public:
  // Creates a texture object.
  // Does not check for errors, always returns texture.
  static GLuint CreateTexture(GLenum target);

  // Compiles a shader.
  // Does not check for errors, always returns shader.
  static GLuint CompileShader(GLenum type, const char* src);

  // Compiles a shader and checks for compilation errors.
  // Returns shader, 0 on failure.
  static GLuint LoadShader(GLenum type, const char* src);

  // Attaches 2 shaders and links them to a program.
  // Does not check for errors, always returns program.
  static GLuint LinkProgram(GLuint vertex_shader, GLuint fragment_shader);

  // Attaches 2 shaders, links them to a program, and checks for errors.
  // Returns program, 0 on failure.
  static GLuint SetupProgram(GLuint vertex_shader, GLuint fragment_shader);

  // Creates a framebuffer, attaches a color buffer, and checks for errors.
  // Returns framebuffer, 0 on failure.
  static GLuint SetupFramebuffer(int width, int height);

  // Checks an area of pixels for a color.
  static bool CheckPixels(GLint x,
                          GLint y,
                          GLsizei width,
                          GLsizei height,
                          const uint8_t expected_color[4]);
};

}  // namespace gl

#endif  // UI_GL_TEST_GL_TEST_HELPER_H_
