// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_helper.h"

#include <string>

#include "base/logging.h"

namespace gfx {

// static
GLuint GLHelper::CompileShader(GLenum type, const char* src) {
  GLuint shader = glCreateShader(type);
  // Load the shader source.
  glShaderSource(shader, 1, &src, nullptr);
  // Compile the shader.
  glCompileShader(shader);
  return shader;
}

// static
GLuint GLHelper::LoadShader(GLenum type, const char* src) {
  GLuint shader = CompileShader(type, src);

  // Check the compile status.
  GLint value = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
  if (!value) {
    char buffer[1024];
    GLsizei length = 0;
    glGetShaderInfoLog(shader, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    DCHECK_EQ(1, value) << "Error compiling shader: " << log;
    glDeleteShader(shader);
    shader = 0;
  }
  return shader;
}

// static
GLuint GLHelper::LinkProgram(GLuint vertex_shader, GLuint fragment_shader) {
  // Create the program object.
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  // Link the program.
  glLinkProgram(program);
  return program;
}

// static
GLuint GLHelper::SetupProgram(GLuint vertex_shader, GLuint fragment_shader) {
  GLuint program = LinkProgram(vertex_shader, fragment_shader);
  // Check the link status.
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (!linked) {
    char buffer[1024];
    GLsizei length = 0;
    glGetProgramInfoLog(program, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    DCHECK_EQ(1, linked) << "Error linking program: " << log;
    glDeleteProgram(program);
    program = 0;
  }
  return program;
}

}  // namespace gfx
