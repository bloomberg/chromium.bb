
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// A class to emluate GLES2 over command buffers.

#include "gpu/command_buffer/client/gles2_implementation.h"

namespace command_buffer {
namespace gles2 {

GLenum GLES2Implementation::CheckFramebufferStatus(GLenum target) {
  return 0;
}

void GLES2Implementation::GetActiveAttrib(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
}

void GLES2Implementation::GetActiveUniform(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
}

void GLES2Implementation::GetAttachedShaders(
    GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) {
}

void GLES2Implementation::GetProgramInfoLog(
    GLuint program, GLsizei bufsize, GLsizei* length, char* infolog) {
}

void GLES2Implementation::GetShaderInfoLog(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog) {
}

void GLES2Implementation::GetShaderPrecisionFormat(
    GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
}

void GLES2Implementation::GetShaderSource(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* source) {
}

const GLubyte* GLES2Implementation::GetString(GLenum name) {
  return 0;
}

void GLES2Implementation::GetUniformfv(
    GLuint program, GLint location, GLfloat* params) {
}

void GLES2Implementation::GetUniformiv(
    GLuint program, GLint location, GLint* params) {
}

void GLES2Implementation::GetVertexAttribPointerv(
    GLuint index, GLenum pname, void** pointer) {
}

void GLES2Implementation::ReadPixels(
    GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
    void* pixels) {
}


}  // namespace gles2
}  // namespace command_buffer

