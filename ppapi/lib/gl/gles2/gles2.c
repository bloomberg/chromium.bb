// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated. DO NOT EDIT!

#include <GLES2/gl2.h>

void GL_APIENTRY glActiveTexture(GLenum texture) {
}

void GL_APIENTRY glAttachShader(GLuint program, GLuint shader) {
}

void GL_APIENTRY glBindAttribLocation(
    GLuint program, GLuint index, const char* name) {
}

void GL_APIENTRY glBindBuffer(GLenum target, GLuint buffer) {
}

void GL_APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer) {
}

void GL_APIENTRY glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
}

void GL_APIENTRY glBindTexture(GLenum target, GLuint texture) {
}

void GL_APIENTRY glBlendColor(
    GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
}

void GL_APIENTRY glBlendEquation(GLenum mode) {
}

void GL_APIENTRY glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
}

void GL_APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor) {
}

void GL_APIENTRY glBlendFuncSeparate(
    GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
}

void GL_APIENTRY glBufferData(
    GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
}

void GL_APIENTRY glBufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
}

GLenum GL_APIENTRY glCheckFramebufferStatus(GLenum target) {
  return 0;
}

void GL_APIENTRY glClear(GLbitfield mask) {
}

void GL_APIENTRY glClearColor(
    GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
}

void GL_APIENTRY glClearDepthf(GLclampf depth) {
}

void GL_APIENTRY glClearStencil(GLint s) {
}

void GL_APIENTRY glColorMask(
    GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
}

void GL_APIENTRY glCompileShader(GLuint shader) {
}

void GL_APIENTRY glCompressedTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLsizei width,
    GLsizei height, GLint border, GLsizei imageSize, const void* data) {
}

void GL_APIENTRY glCompressedTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
}

void GL_APIENTRY glCopyTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
    GLsizei width, GLsizei height, GLint border) {
}

void GL_APIENTRY glCopyTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
    GLsizei width, GLsizei height) {
}

GLuint GL_APIENTRY glCreateProgram() {
  return 0;
}

GLuint GL_APIENTRY glCreateShader(GLenum type) {
  return 0;
}

void GL_APIENTRY glCullFace(GLenum mode) {
}

void GL_APIENTRY glDeleteBuffers(GLsizei n, const GLuint* buffers) {
}

void GL_APIENTRY glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
}

void GL_APIENTRY glDeleteProgram(GLuint program) {
}

void GL_APIENTRY glDeleteRenderbuffers(
    GLsizei n, const GLuint* renderbuffers) {
}

void GL_APIENTRY glDeleteShader(GLuint shader) {
}

void GL_APIENTRY glDeleteTextures(GLsizei n, const GLuint* textures) {
}

void GL_APIENTRY glDepthFunc(GLenum func) {
}

void GL_APIENTRY glDepthMask(GLboolean flag) {
}

void GL_APIENTRY glDepthRangef(GLclampf zNear, GLclampf zFar) {
}

void GL_APIENTRY glDetachShader(GLuint program, GLuint shader) {
}

void GL_APIENTRY glDisable(GLenum cap) {
}

void GL_APIENTRY glDisableVertexAttribArray(GLuint index) {
}

void GL_APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) {
}

void GL_APIENTRY glDrawElements(
    GLenum mode, GLsizei count, GLenum type, const void* indices) {
}

void GL_APIENTRY glEnable(GLenum cap) {
}

void GL_APIENTRY glEnableVertexAttribArray(GLuint index) {
}

void GL_APIENTRY glFinish() {
}

void GL_APIENTRY glFlush() {
}

void GL_APIENTRY glFramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum renderbuffertarget,
    GLuint renderbuffer) {
}

void GL_APIENTRY glFramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
    GLint level) {
}

void GL_APIENTRY glFrontFace(GLenum mode) {
}

void GL_APIENTRY glGenBuffers(GLsizei n, GLuint* buffers) {
}

void GL_APIENTRY glGenerateMipmap(GLenum target) {
}

void GL_APIENTRY glGenFramebuffers(GLsizei n, GLuint* framebuffers) {
}

void GL_APIENTRY glGenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
}

void GL_APIENTRY glGenTextures(GLsizei n, GLuint* textures) {
}

void GL_APIENTRY glGetActiveAttrib(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
}

void GL_APIENTRY glGetActiveUniform(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
}

void GL_APIENTRY glGetAttachedShaders(
    GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) {
}

GLint GL_APIENTRY glGetAttribLocation(GLuint program, const char* name) {
  return 0;
}

void GL_APIENTRY glGetBooleanv(GLenum pname, GLboolean* params) {
}

void GL_APIENTRY glGetBufferParameteriv(
    GLenum target, GLenum pname, GLint* params) {
}

GLenum GL_APIENTRY glGetError() {
  return 0;
}

void GL_APIENTRY glGetFloatv(GLenum pname, GLfloat* params) {
}

void GL_APIENTRY glGetFramebufferAttachmentParameteriv(
    GLenum target, GLenum attachment, GLenum pname, GLint* params) {
}

void GL_APIENTRY glGetIntegerv(GLenum pname, GLint* params) {
}

void GL_APIENTRY glGetProgramiv(GLuint program, GLenum pname, GLint* params) {
}

void GL_APIENTRY glGetProgramInfoLog(
    GLuint program, GLsizei bufsize, GLsizei* length, char* infolog) {
}

void GL_APIENTRY glGetRenderbufferParameteriv(
    GLenum target, GLenum pname, GLint* params) {
}

void GL_APIENTRY glGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
}

void GL_APIENTRY glGetShaderInfoLog(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog) {
}

void GL_APIENTRY glGetShaderPrecisionFormat(
    GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
}

void GL_APIENTRY glGetShaderSource(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* source) {
}

const GLubyte* GL_APIENTRY glGetString(GLenum name) {
  return 0;
}

void GL_APIENTRY glGetTexParameterfv(
    GLenum target, GLenum pname, GLfloat* params) {
}

void GL_APIENTRY glGetTexParameteriv(
    GLenum target, GLenum pname, GLint* params) {
}

void GL_APIENTRY glGetUniformfv(
    GLuint program, GLint location, GLfloat* params) {
}

void GL_APIENTRY glGetUniformiv(
    GLuint program, GLint location, GLint* params) {
}

GLint GL_APIENTRY glGetUniformLocation(GLuint program, const char* name) {
  return 0;
}

void GL_APIENTRY glGetVertexAttribfv(
    GLuint index, GLenum pname, GLfloat* params) {
}

void GL_APIENTRY glGetVertexAttribiv(
    GLuint index, GLenum pname, GLint* params) {
}

void GL_APIENTRY glGetVertexAttribPointerv(
    GLuint index, GLenum pname, void** pointer) {
}

void GL_APIENTRY glHint(GLenum target, GLenum mode) {
}

GLboolean GL_APIENTRY glIsBuffer(GLuint buffer) {
  return 0;
}

GLboolean GL_APIENTRY glIsEnabled(GLenum cap) {
  return 0;
}

GLboolean GL_APIENTRY glIsFramebuffer(GLuint framebuffer) {
  return 0;
}

GLboolean GL_APIENTRY glIsProgram(GLuint program) {
  return 0;
}

GLboolean GL_APIENTRY glIsRenderbuffer(GLuint renderbuffer) {
  return 0;
}

GLboolean GL_APIENTRY glIsShader(GLuint shader) {
  return 0;
}

GLboolean GL_APIENTRY glIsTexture(GLuint texture) {
  return 0;
}

void GL_APIENTRY glLineWidth(GLfloat width) {
}

void GL_APIENTRY glLinkProgram(GLuint program) {
}

void GL_APIENTRY glPixelStorei(GLenum pname, GLint param) {
}

void GL_APIENTRY glPolygonOffset(GLfloat factor, GLfloat units) {
}

void GL_APIENTRY glReadPixels(
    GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
    void* pixels) {
}

void GL_APIENTRY glReleaseShaderCompiler() {
}

void GL_APIENTRY glRenderbufferStorage(
    GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
}

void GL_APIENTRY glSampleCoverage(GLclampf value, GLboolean invert) {
}

void GL_APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
}

void GL_APIENTRY glShaderBinary(
    GLsizei n, const GLuint* shaders, GLenum binaryformat, const void* binary,
    GLsizei length) {
}

void GL_APIENTRY glShaderSource(
    GLuint shader, GLsizei count, const char** str, const GLint* length) {
}

void GL_APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask) {
}

void GL_APIENTRY glStencilFuncSeparate(
    GLenum face, GLenum func, GLint ref, GLuint mask) {
}

void GL_APIENTRY glStencilMask(GLuint mask) {
}

void GL_APIENTRY glStencilMaskSeparate(GLenum face, GLuint mask) {
}

void GL_APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
}

void GL_APIENTRY glStencilOpSeparate(
    GLenum face, GLenum fail, GLenum zfail, GLenum zpass) {
}

void GL_APIENTRY glTexImage2D(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
}

void GL_APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
}

void GL_APIENTRY glTexParameterfv(
    GLenum target, GLenum pname, const GLfloat* params) {
}

void GL_APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param) {
}

void GL_APIENTRY glTexParameteriv(
    GLenum target, GLenum pname, const GLint* params) {
}

void GL_APIENTRY glTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels) {
}

void GL_APIENTRY glUniform1f(GLint location, GLfloat x) {
}

void GL_APIENTRY glUniform1fv(
    GLint location, GLsizei count, const GLfloat* v) {
}

void GL_APIENTRY glUniform1i(GLint location, GLint x) {
}

void GL_APIENTRY glUniform1iv(GLint location, GLsizei count, const GLint* v) {
}

void GL_APIENTRY glUniform2f(GLint location, GLfloat x, GLfloat y) {
}

void GL_APIENTRY glUniform2fv(
    GLint location, GLsizei count, const GLfloat* v) {
}

void GL_APIENTRY glUniform2i(GLint location, GLint x, GLint y) {
}

void GL_APIENTRY glUniform2iv(GLint location, GLsizei count, const GLint* v) {
}

void GL_APIENTRY glUniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
}

void GL_APIENTRY glUniform3fv(
    GLint location, GLsizei count, const GLfloat* v) {
}

void GL_APIENTRY glUniform3i(GLint location, GLint x, GLint y, GLint z) {
}

void GL_APIENTRY glUniform3iv(GLint location, GLsizei count, const GLint* v) {
}

void GL_APIENTRY glUniform4f(
    GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
}

void GL_APIENTRY glUniform4fv(
    GLint location, GLsizei count, const GLfloat* v) {
}

void GL_APIENTRY glUniform4i(
    GLint location, GLint x, GLint y, GLint z, GLint w) {
}

void GL_APIENTRY glUniform4iv(GLint location, GLsizei count, const GLint* v) {
}

void GL_APIENTRY glUniformMatrix2fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
}

void GL_APIENTRY glUniformMatrix3fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
}

void GL_APIENTRY glUniformMatrix4fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
}

void GL_APIENTRY glUseProgram(GLuint program) {
}

void GL_APIENTRY glValidateProgram(GLuint program) {
}

void GL_APIENTRY glVertexAttrib1f(GLuint indx, GLfloat x) {
}

void GL_APIENTRY glVertexAttrib1fv(GLuint indx, const GLfloat* values) {
}

void GL_APIENTRY glVertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
}

void GL_APIENTRY glVertexAttrib2fv(GLuint indx, const GLfloat* values) {
}

void GL_APIENTRY glVertexAttrib3f(
    GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
}

void GL_APIENTRY glVertexAttrib3fv(GLuint indx, const GLfloat* values) {
}

void GL_APIENTRY glVertexAttrib4f(
    GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
}

void GL_APIENTRY glVertexAttrib4fv(GLuint indx, const GLfloat* values) {
}

void GL_APIENTRY glVertexAttribPointer(
    GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
    const void* ptr) {
}

void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
}

