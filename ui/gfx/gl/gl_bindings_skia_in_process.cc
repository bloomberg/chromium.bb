// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "ui/gfx/gl/gl_bindings_skia_in_process.h"

#include "base/logging.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"

#include "third_party/skia/gpu/include/GrGLInterface.h"

namespace {

extern "C" {
// The following stub functions are required because the glXXX routines exported
// via gl_bindings.h use call-type GL_BINDING_CALL, which on Windows is stdcall.
// Skia has been built such that its GrGLInterface GL pointers are __cdecl.

GLvoid StubGLActiveTexture(GLenum texture) {
  glActiveTexture(texture);
}

GLvoid StubGLAttachShader(GLuint program, GLuint shader) {
  glAttachShader(program, shader);
}

GLvoid StubGLBindAttribLocation(GLuint program, GLuint index,
                                const char* name) {
  glBindAttribLocation(program, index, name);
}

GLvoid StubGLBindBuffer(GLenum target, GLuint buffer) {
  glBindBuffer(target, buffer);
}

GLvoid StubGLBindFramebuffer(GLenum target, GLuint framebuffer) {
  glBindFramebufferEXT(target, framebuffer);
}

GLvoid StubGLBindRenderbuffer(GLenum target, GLuint renderbuffer) {
  glBindRenderbufferEXT(target, renderbuffer);
}

GLvoid StubGLBindTexture(GLenum target, GLuint texture) {
  glBindTexture(target, texture);
}

GLvoid StubGLBlendColor(GLclampf red, GLclampf green, GLclampf blue,
                        GLclampf alpha) {
  glBlendColor(red, green, blue, alpha);
}

GLvoid StubGLBlendFunc(GLenum sfactor, GLenum dfactor) {
  glBlendFunc(sfactor, dfactor);
}

GLvoid StubGLBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                             GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                             GLbitfield mask, GLenum filter) {
  glBlitFramebufferEXT(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1,
                       mask, filter);
}

GLvoid StubGLBufferData(GLenum target, GLsizeiptr size, const void* data,
                        GLenum usage) {
  glBufferData(target, size, data, usage);
}

GLvoid StubGLBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size,
                           const void* data) {
  glBufferSubData(target, offset, size, data);
}

GLenum StubGLCheckFramebufferStatus(GLenum target) {
  return glCheckFramebufferStatusEXT(target);
}

GLvoid StubGLClear(GLbitfield mask) {
  glClear(mask);
}

GLvoid StubGLClearColor(GLclampf red, GLclampf green, GLclampf blue,
                        GLclampf alpha) {
  glClearColor(red, green, blue, alpha);
}

GLvoid StubGLClearStencil(GLint s) {
  glClearStencil(s);
}

GLvoid StubGLColorMask(GLboolean red, GLboolean green, GLboolean blue,
                       GLboolean alpha) {
  glColorMask(red, green, blue, alpha);
}

GLvoid StubGLCompileShader(GLuint shader) {
  glCompileShader(shader);
}

GLvoid StubGLCompressedTexImage2D(GLenum target, GLint level,
                                  GLenum internalformat, GLsizei width,
                                  GLsizei height, GLint border,
                                  GLsizei imageSize, const void* data) {
  glCompressedTexImage2D(target, level, internalformat, width, height, border,
                         imageSize, data);
}

GLuint StubGLCreateProgram(void) {
  return glCreateProgram();
}

GLuint StubGLCreateShader(GLenum type) {
  return glCreateShader(type);
}

GLvoid StubGLCullFace(GLenum mode) {
  glCullFace(mode);
}

GLvoid StubGLDeleteBuffers(GLsizei n, const GLuint* buffers) {
  glDeleteBuffersARB(n, buffers);
}

GLvoid StubGLDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  glDeleteFramebuffersEXT(n, framebuffers);
}

GLvoid StubGLDeleteProgram(GLuint program) {
  glDeleteProgram(program);
}

GLvoid StubGLDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  glDeleteRenderbuffersEXT(n, renderbuffers);
}

GLvoid StubGLDeleteShader(GLuint shader) {
  glDeleteShader(shader);
}

GLvoid StubGLDeleteTextures(GLsizei n, const GLuint* textures) {
  glDeleteTextures(n, textures);
}

GLvoid StubGLDepthMask(GLboolean flag) {
  glDepthMask(flag);
}

GLvoid StubGLDisable(GLenum cap) {
  glDisable(cap);
}

GLvoid StubGLDisableVertexAttribArray(GLuint index) {
  glDisableVertexAttribArray(index);
}

GLvoid StubGLDrawArrays(GLenum mode, GLint first, GLsizei count) {
  glDrawArrays(mode, first, count);
}

GLvoid StubGLDrawElements(GLenum mode, GLsizei count, GLenum type,
                          const void* indices) {
  glDrawElements(mode, count, type, indices);
}

GLvoid StubGLEnable(GLenum cap) {
  glEnable(cap);
}

GLvoid StubGLEnableVertexAttribArray(GLuint index) {
  glEnableVertexAttribArray(index);
}

GLvoid StubGLFramebufferRenderbuffer(GLenum target, GLenum attachment,
                                     GLenum renderbuffertarget,
                                     GLuint renderbuffer) {
  glFramebufferRenderbufferEXT(target, attachment, renderbuffertarget,
                               renderbuffer);
}

GLvoid StubGLFramebufferTexture2D(GLenum target, GLenum attachment,
                                  GLenum textarget, GLuint texture,
                                  GLint level) {
  glFramebufferTexture2DEXT(target, attachment, textarget, texture, level);
}

GLvoid StubGLFrontFace(GLenum mode) {
  glFrontFace(mode);
}

GLvoid StubGLGenBuffers(GLsizei n, GLuint* buffers) {
  glGenBuffersARB(n, buffers);
}

GLvoid StubGLGenFramebuffers(GLsizei n, GLuint* framebuffers) {
  glGenFramebuffersEXT(n, framebuffers);
}

GLvoid StubGLGenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  glGenRenderbuffersEXT(n, renderbuffers);
}

GLvoid StubGLGenTextures(GLsizei n, GLuint* textures) {
  glGenTextures(n, textures);
}

GLvoid StubGLGetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
  glGetBufferParameteriv(target, pname, params);
}

GLenum StubGLGetError() {
  return glGetError();
}

GLvoid StubGLGetIntegerv(GLenum pname, GLint* params) {
  glGetIntegerv(pname, params);
}

GLvoid StubGLGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei* length,
                               char* infolog) {
  glGetProgramInfoLog(program, bufsize, length, infolog);
}

GLvoid StubGLGetProgramiv(GLuint program, GLenum pname, GLint* params) {
  glGetProgramiv(program, pname, params);
}

GLvoid StubGLGetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei* length,
                              char* infolog) {
  glGetShaderInfoLog(shader, bufsize, length, infolog);
}

GLvoid StubGLGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
  glGetShaderiv(shader, pname, params);
}

const GLubyte* StubGLGetString(GLenum name) {
  return glGetString(name);
}

GLint StubGLGetUniformLocation(GLuint program, const char* name) {
  return glGetUniformLocation(program, name);
}

GLvoid StubGLLineWidth(GLfloat width) {
  glLineWidth(width);
}

GLvoid StubGLLinkProgram(GLuint program) {
  glLinkProgram(program);
}

void* StubGLMapBuffer(GLenum target, GLenum access) {
  return glMapBuffer(target, access);
}

GLvoid StubGLPixelStorei(GLenum pname, GLint param) {
  glPixelStorei(pname, param);
}

GLvoid StubGLReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                        GLenum format, GLenum type, void* pixels) {
  glReadPixels(x, y, width, height, format, type, pixels);
}

GLvoid StubGLRenderBufferStorage(GLenum target, GLenum internalformat,
                                 GLsizei width, GLsizei height) {
  glRenderbufferStorageEXT(target, internalformat, width, height);
}

GLvoid StubGLRenderbufferStorageMultisample(GLenum target, GLsizei samples,
                                            GLenum internalformat,
                                            GLsizei width, GLsizei height) {
  glRenderbufferStorageMultisampleEXT(target, samples, internalformat, width,
                                      height);
}

GLvoid StubGLScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  glScissor(x, y, width, height);
}

GLvoid StubGLShaderSource(GLuint shader, GLsizei count, const char** str,
                          const GLint* length) {
  glShaderSource(shader, count, str, length);
}

GLvoid StubGLStencilFunc(GLenum func, GLint ref, GLuint mask) {
  glStencilFunc(func, ref, mask);
}

GLvoid StubGLStencilFuncSeparate(GLenum face, GLenum func, GLint ref,
                                 GLuint mask) {
  glStencilFuncSeparate(face, func, ref, mask);
}

GLvoid StubGLStencilMask(GLuint mask) {
  glStencilMask(mask);
}

GLvoid StubGLStencilMaskSeparate(GLenum face, GLuint mask) {
  glStencilMaskSeparate(face, mask);
}

GLvoid StubGLStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  glStencilOp(fail, zfail, zpass);
}

GLvoid StubGLStencilOpSeparate(GLenum face, GLenum fail, GLenum zfail,
                               GLenum zpass) {
  glStencilOpSeparate(face, fail, zfail, zpass);
}

GLvoid StubGLTexImage2D(GLenum target, GLint level, GLint internalformat,
                        GLsizei width, GLsizei height, GLint border,
                        GLenum format, GLenum type, const void* pixels) {
  glTexImage2D(target, level, internalformat, width, height, border, format,
               type, pixels);
}

GLvoid StubGLTexParameteri(GLenum target, GLenum pname, GLint param) {
  glTexParameteri(target, pname, param);
}

GLvoid StubGLTexSubImage2D(GLenum target, GLint level, GLint xoffset,
                           GLint yoffset, GLsizei width, GLsizei height,
                           GLenum format, GLenum type, const void* pixels) {
  glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type,
                  pixels);
}

GLvoid StubGLUniform1f(GLint location, GLfloat v) {
  glUniform1i(location, v);
}

GLvoid StubGLUniform1i(GLint location, GLint v) {
  glUniform1i(location, v);
}

GLvoid StubGLUniform1fv(GLint location, GLsizei count, const GLfloat* v) {
  glUniform1fv(location, count, v);
}

GLvoid StubGLUniform1iv(GLint location, GLsizei count, const GLint* v) {
  glUniform1iv(location, count, v);
}

GLvoid StubGLUniform2f(GLint location, GLfloat v0, GLfloat v1) {
  glUniform2i(location, v0, v1);
}

GLvoid StubGLUniform2i(GLint location, GLint v0, GLint v1) {
  glUniform2i(location, v0, v1);
}

GLvoid StubGLUniform2fv(GLint location, GLsizei count, const GLfloat* v) {
  glUniform2fv(location, count, v);
}

GLvoid StubGLUniform2iv(GLint location, GLsizei count, const GLint* v) {
  glUniform2iv(location, count, v);
}

GLvoid StubGLUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
  glUniform3i(location, v0, v1, v2);
}

GLvoid StubGLUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
  glUniform3i(location, v0, v1, v2);
}

GLvoid StubGLUniform3fv(GLint location, GLsizei count, const GLfloat* v) {
  glUniform3fv(location, count, v);
}

GLvoid StubGLUniform3iv(GLint location, GLsizei count, const GLint* v) {
  glUniform3iv(location, count, v);
}

GLvoid StubGLUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2,
                       GLfloat v3) {
  glUniform4i(location, v0, v1, v2, v3);
}

GLvoid StubGLUniform4i(GLint location, GLint v0, GLint v1, GLint v2,
                       GLint v3) {
  glUniform4i(location, v0, v1, v2, v3);
}

GLvoid StubGLUniform4fv(GLint location, GLsizei count, const GLfloat* v) {
  glUniform4fv(location, count, v);
}

GLvoid StubGLUniform4iv(GLint location, GLsizei count, const GLint* v) {
  glUniform4iv(location, count, v);
}

GLvoid StubGLUniformMatrix2fv(GLint location, GLsizei count,
                              GLboolean transpose, const GLfloat* value) {
  glUniformMatrix2fv(location, count, transpose, value);
}

GLvoid StubGLUniformMatrix3fv(GLint location, GLsizei count,
                              GLboolean transpose, const GLfloat* value) {
  glUniformMatrix3fv(location, count, transpose, value);
}

GLvoid StubGLUniformMatrix4fv(GLint location, GLsizei count,
                              GLboolean transpose, const GLfloat* value) {
  glUniformMatrix4fv(location, count, transpose, value);
}

GLboolean StubGLUnmapBuffer(GLenum target) {
  return glUnmapBuffer(target);
}

GLvoid StubGLUseProgram(GLuint program) {
  glUseProgram(program);
}

GLvoid StubGLVertexAttrib4fv(GLuint indx, const GLfloat* values) {
  glVertexAttrib4fv(indx, values);
}

GLvoid StubGLVertexAttribPointer(GLuint indx, GLint size, GLenum type,
                                 GLboolean normalized, GLsizei stride,
                                 const void* ptr) {
  glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
}

GLvoid StubGLViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  glViewport(x, y, width, height);
}
}  // extern "C"
}  // namespace

namespace gfx {

void BindSkiaToInProcessGL() {
  static bool host_StubGL_installed = false;
  if (!host_StubGL_installed) {
    GrGLBinding binding;
    switch (gfx::GetGLImplementation()) {
      case gfx::kGLImplementationNone:
        NOTREACHED();
        break;
      case gfx::kGLImplementationDesktopGL:
        binding = kDesktop_GrGLBinding;
        break;
      case gfx::kGLImplementationOSMesaGL:
        binding = kDesktop_GrGLBinding;
        break;
      case gfx::kGLImplementationEGLGLES2:
        binding = kES2_GrGLBinding;
        break;
      case gfx::kGLImplementationMockGL:
        NOTREACHED();
        break;
    }

    static GrGLInterface host_gl_interface = {
      binding,
      StubGLActiveTexture,
      StubGLAttachShader,
      StubGLBindAttribLocation,
      StubGLBindBuffer,
      StubGLBindTexture,
      StubGLBlendColor,
      StubGLBlendFunc,
      StubGLBufferData,
      StubGLBufferSubData,
      StubGLClear,
      StubGLClearColor,
      StubGLClearStencil,
      NULL,  // glClientActiveTexture
      NULL,  // glColor4ub
      StubGLColorMask,
      NULL,  // glColorPointer
      StubGLCompileShader,
      StubGLCompressedTexImage2D,
      StubGLCreateProgram,
      StubGLCreateShader,
      StubGLCullFace,
      StubGLDeleteBuffers,
      StubGLDeleteProgram,
      StubGLDeleteShader,
      StubGLDeleteTextures,
      StubGLDepthMask,
      StubGLDisable,
      NULL,  // glDisableClientState
      StubGLDisableVertexAttribArray,
      StubGLDrawArrays,
      StubGLDrawElements,
      StubGLEnable,
      NULL,  // glEnableClientState
      StubGLEnableVertexAttribArray,
      StubGLFrontFace,
      StubGLGenBuffers,
      StubGLGenTextures,
      StubGLGetBufferParameteriv,
      StubGLGetError,
      StubGLGetIntegerv,
      StubGLGetProgramInfoLog,
      StubGLGetProgramiv,
      StubGLGetShaderInfoLog,
      StubGLGetShaderiv,
      StubGLGetString,
      StubGLGetUniformLocation,
      StubGLLineWidth,
      StubGLLinkProgram,
      NULL,  // glLoadMatrixf
      NULL,  // glMatrixMode
      StubGLPixelStorei,
      NULL,  // glPointSize
      StubGLReadPixels,
      StubGLScissor,
      NULL,  // glShadeModel
      StubGLShaderSource,
      StubGLStencilFunc,
      StubGLStencilFuncSeparate,
      StubGLStencilMask,
      StubGLStencilMaskSeparate,
      StubGLStencilOp,
      StubGLStencilOpSeparate,
      NULL,  // glTexCoordPointer
      NULL,  // glTexEnvi
      StubGLTexImage2D,
      StubGLTexParameteri,
      StubGLTexSubImage2D,
      StubGLUniform1f,
      StubGLUniform1i,
      StubGLUniform1fv,
      StubGLUniform1iv,
      StubGLUniform2f,
      StubGLUniform2i,
      StubGLUniform2fv,
      StubGLUniform2iv,
      StubGLUniform3f,
      StubGLUniform3i,
      StubGLUniform3fv,
      StubGLUniform3iv,
      StubGLUniform4f,
      StubGLUniform4i,
      StubGLUniform4fv,
      StubGLUniform4iv,
      StubGLUniformMatrix2fv,
      StubGLUniformMatrix3fv,
      StubGLUniformMatrix4fv,
      StubGLUseProgram,
      StubGLVertexAttrib4fv,
      StubGLVertexAttribPointer,
      NULL,  // glVertexPointer
      StubGLViewport,
      StubGLBindFramebuffer,
      StubGLBindRenderbuffer,
      StubGLCheckFramebufferStatus,
      StubGLDeleteFramebuffers,
      StubGLDeleteRenderbuffers,
      StubGLFramebufferRenderbuffer,
      StubGLFramebufferTexture2D,
      StubGLGenFramebuffers,
      StubGLGenRenderbuffers,
      StubGLRenderBufferStorage,
      StubGLRenderbufferStorageMultisample,
      StubGLBlitFramebuffer,
      NULL,  // glResolveMultisampleFramebuffer
      StubGLMapBuffer,
      StubGLUnmapBuffer,
      NULL,  // glBindFragDataLocationIndexed
      GrGLInterface::kStaticInitEndGuard,
    };
    GrGLSetGLInterface(&host_gl_interface);
    host_StubGL_installed = true;
  }
}

}  // namespace gfx

