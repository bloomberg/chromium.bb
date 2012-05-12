// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_INTERFACE_H_
#define UI_GL_GL_INTERFACE_H_
#pragma once

// This file implements glue to a GL interface so we can mock it for unit
// testing. It has to be Desktop GL, not GLES2 as it is used to test the service
// side code.

#include "ui/gl/gl_bindings.h"

namespace gfx {

class GL_EXPORT GLInterface {
 public:
  virtual ~GLInterface() {}

  static void SetGLInterface(GLInterface* gl_interface);

  static GLInterface* GetGLInterface();

  virtual void ActiveTexture(GLenum texture) = 0;

  virtual void AttachShader(GLuint program,
                            GLuint shader) = 0;

  virtual void BeginQuery(GLenum target, GLuint id) = 0;

  virtual void BeginQueryARB(GLenum target, GLuint id) = 0;

  virtual void BindAttribLocation(GLuint program,
                                  GLuint index,
                                  const char* name) = 0;

  virtual void BindBuffer(GLenum target, GLuint buffer) = 0;

  virtual void BindFragDataLocationIndexed(GLuint program,
                                           GLuint colorNumber,
                                           GLuint index,
                                           const char* name) = 0;

  virtual void BindFragDataLocation(GLuint program,
                                    GLuint colorNumber,
                                    const char* name) = 0;

  virtual void BindFramebufferEXT(GLenum target, GLuint framebuffer) = 0;

  virtual void BindRenderbufferEXT(GLenum target, GLuint renderbuffer) = 0;

  virtual void BindTexture(GLenum target, GLuint texture) = 0;

  virtual void BlendColor(GLclampf red,
                          GLclampf green,
                          GLclampf blue,
                          GLclampf alpha) = 0;

  virtual void BlendEquation(GLenum mode) = 0;

  virtual void BlendEquationSeparate(GLenum modeRGB,
                                     GLenum modeAlpha) = 0;

  virtual void BlendFunc(GLenum sfactor, GLenum dfactor) = 0;

  virtual void BlendFuncSeparate(GLenum srcRGB,
                                 GLenum dstRGB,
                                 GLenum srcAlpha,
                                 GLenum dstAlpha) = 0;

  virtual void BlitFramebufferANGLE(
      GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
      GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
      GLbitfield mask, GLenum filter) = 0;

  virtual void BlitFramebufferEXT(
      GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
      GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
      GLbitfield mask, GLenum filter) = 0;

  virtual void BufferData(GLenum target,
                          GLsizeiptr size,
                          const void* data,
                          GLenum usage) = 0;

  virtual void BufferSubData(GLenum target,
                             GLintptr offset,
                             GLsizeiptr size,
                             const void* data) = 0;

  virtual GLenum CheckFramebufferStatusEXT(GLenum target) = 0;

  virtual void Clear(GLbitfield mask) = 0;

  virtual void ClearColor(GLclampf red,
                          GLclampf green,
                          GLclampf blue,
                          GLclampf alpha) = 0;

  virtual void ClearDepth(GLclampd depth) = 0;

  virtual void ClearDepthf(GLclampf depth) = 0;

  virtual void ClearStencil(GLint s) = 0;

  virtual void ColorMask(GLboolean red,
                         GLboolean green,
                         GLboolean blue,
                         GLboolean alpha) = 0;

  virtual void CompileShader(GLuint shader) = 0;

  virtual void CompressedTexImage2D(GLenum target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLsizei width, GLsizei height,
                                    GLint border,
                                    GLsizei imageSize,
                                    const void* data) = 0;

  virtual void CompressedTexSubImage2D(GLenum target,
                                       GLint level,
                                       GLint xoffset, GLint yoffset,
                                       GLsizei width, GLsizei height,
                                       GLenum format,
                                       GLsizei imageSize,
                                       const void* data) = 0;

  virtual void CopyTexImage2D(GLenum target,
                              GLint level,
                              GLenum internalformat,
                              GLint x, GLint y,
                              GLsizei width, GLsizei height,
                              GLint border) = 0;

  virtual void CopyTexSubImage2D(GLenum target,
                                 GLint level,
                                 GLint xoffset, GLint yoffset,
                                 GLint x, GLint y,
                                 GLsizei width, GLsizei height) = 0;

  virtual GLuint CreateProgram() = 0;

  virtual GLuint CreateShader(GLenum type) = 0;

  virtual void CullFace(GLenum mode) = 0;

  virtual void DeleteBuffersARB(GLsizei n, const GLuint* buffers) = 0;

  virtual void DeleteFramebuffersEXT(GLsizei n, const GLuint* framebuffers) = 0;

  virtual void DeleteProgram(GLuint program) = 0;

  virtual void DeleteRenderbuffersEXT(GLsizei n,
                                      const GLuint* renderbuffers) = 0;

  virtual void DeleteQueries(GLsizei n, const GLuint* ids) = 0;

  virtual void DeleteQueriesARB(GLsizei n, const GLuint* ids) = 0;

  virtual void DeleteShader(GLuint shader) = 0;

  virtual void DeleteTextures(GLsizei n, const GLuint* textures) = 0;

  virtual void DepthFunc(GLenum func) = 0;

  virtual void DepthMask(GLboolean flag) = 0;

  virtual void DepthRange(GLclampd zNear, GLclampd zFar) = 0;

  virtual void DepthRangef(GLclampf zNear, GLclampf zFar) = 0;

  virtual void DetachShader(GLuint program, GLuint shader) = 0;

  virtual void Disable(GLenum cap) = 0;

  virtual void DisableVertexAttribArray(GLuint index) = 0;

  virtual void DrawArrays(GLenum mode, GLint first, GLsizei count) = 0;

  virtual void DrawBuffer(GLenum mode) = 0;

  virtual void DrawBuffersARB(GLsizei n, const GLenum* bufs) = 0;

  virtual void DrawElements(GLenum mode,
                            GLsizei count,
                            GLenum type,
                            const void* indices) = 0;

  virtual void EGLImageTargetTexture2DOES(
      GLenum target, GLeglImageOES image) = 0;

  virtual void EGLImageTargetRenderbufferStorageOES(
      GLenum target, GLeglImageOES image) = 0;

  virtual void Enable(GLenum cap) = 0;

  virtual void EnableVertexAttribArray(GLuint index) = 0;

  virtual void EndQuery(GLenum target) = 0;

  virtual void EndQueryARB(GLenum target) = 0;

  virtual void Finish() = 0;

  virtual void Flush() = 0;

  virtual void FramebufferRenderbufferEXT(GLenum target,
                                          GLenum attachment,
                                          GLenum renderbuffertarget,
                                          GLuint renderbuffer) = 0;

  virtual void FramebufferTexture2DEXT(GLenum target,
                                       GLenum attachment,
                                       GLenum textarget,
                                       GLuint texture,
                                       GLint level) = 0;

  virtual void FrontFace(GLenum mode) = 0;

  virtual void GenBuffersARB(GLsizei n, GLuint* buffers) = 0;

  virtual void GenerateMipmapEXT(GLenum target) = 0;

  virtual void GenFramebuffersEXT(GLsizei n, GLuint* framebuffers) = 0;

  virtual void GenQueries(GLsizei n, GLuint* ids) = 0;

  virtual void GenQueriesARB(GLsizei n, GLuint* ids) = 0;

  virtual void GenRenderbuffersEXT(GLsizei n, GLuint* renderbuffers) = 0;

  virtual void GenTextures(GLsizei n, GLuint* textures) = 0;

  virtual void GetActiveAttrib(GLuint program,
                               GLuint index,
                               GLsizei bufsize,
                               GLsizei* length,
                               GLint* size,
                               GLenum* type,
                               char* name) = 0;

  virtual void GetActiveUniform(GLuint program,
                                GLuint index,
                                GLsizei bufsize,
                                GLsizei* length,
                                GLint* size,
                                GLenum* type,
                                char* name) = 0;

  virtual void GetAttachedShaders(GLuint program,
                                  GLsizei maxcount,
                                  GLsizei* count,
                                  GLuint* shaders) = 0;

  virtual GLint GetAttribLocation(GLuint program, const char* name) = 0;

  virtual void GetBooleanv(GLenum pname, GLboolean* params) = 0;

  virtual void GetBufferParameteriv(GLenum target,
                                    GLenum pname,
                                    GLint* params) = 0;

  virtual GLenum GetError() = 0;

  virtual void GetFloatv(GLenum pname, GLfloat* params) = 0;

  virtual void GetFramebufferAttachmentParameterivEXT(GLenum target,
                                                      GLenum attachment,
                                                      GLenum pname,
                                                      GLint* params) = 0;

  virtual void GetIntegerv(GLenum pname, GLint* params) = 0;

  virtual void GetProgramiv(GLuint program, GLenum pname, GLint* params) = 0;

  // TODO(gman): Implement this
  virtual void GetProgramInfoLog(GLuint program,
                                 GLsizei bufsize,
                                 GLsizei* length,
                                 char* infolog) = 0;

  virtual void GetQueryiv(GLenum target, GLenum pname, GLint* params) = 0;

  virtual void GetQueryivARB(GLenum target, GLenum pname, GLint* params) = 0;

  virtual void GetQueryObjecti64v(GLuint id, GLenum pname, GLint64* params) = 0;

  virtual void GetQueryObjectiv(GLuint id, GLenum pname, GLint* params) = 0;

  virtual void GetQueryObjectui64v(GLuint id,
                                   GLenum pname,
                                   GLuint64* params) = 0;

  virtual void GetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params) = 0;

  virtual void GetQueryObjectuivARB(GLuint id,
                                    GLenum pname,
                                    GLuint* params) = 0;

  virtual void GetRenderbufferParameterivEXT(GLenum target,
                                             GLenum pname,
                                             GLint* params) = 0;

  virtual void GetShaderiv(GLuint shader, GLenum pname, GLint* params) = 0;

  // TODO(gman): Implement this
  virtual void GetShaderInfoLog(GLuint shader,
                                GLsizei bufsize,
                                GLsizei* length,
                                char* infolog) = 0;

  virtual void GetShaderPrecisionFormat(GLenum shadertype,
                                        GLenum precisiontype,
                                        GLint* range,
                                        GLint* precision) = 0;

  // TODO(gman): Implement this
  virtual void GetShaderSource(GLuint shader,
                               GLsizei bufsize,
                               GLsizei* length,
                               char* source) = 0;

  virtual const GLubyte* GetString(GLenum name) = 0;

  virtual void GetTexLevelParameterfv(GLenum target,
                                      GLint level,
                                      GLenum pname,
                                      GLfloat* params) = 0;

  virtual void GetTexLevelParameteriv(GLenum target,
                                      GLint level,
                                      GLenum pname,
                                      GLint* params) = 0;

  virtual void GetTexParameterfv(GLenum target,
                                 GLenum pname,
                                 GLfloat* params) = 0;

  virtual void GetTexParameteriv(GLenum target,
                                 GLenum pname,
                                 GLint* params) = 0;

  virtual void GetTranslatedShaderSourceANGLE(GLuint shader,
                                              GLsizei bufsize,
                                              GLsizei* length,
                                              char* source) = 0;

  virtual void GetUniformfv(GLuint program,
                            GLint location,
                            GLfloat* params) = 0;

  virtual void GetUniformiv(GLuint program,
                            GLint location,
                            GLint* params) = 0;

  virtual GLint GetUniformLocation(GLuint program, const char* name) = 0;

  virtual void GetVertexAttribfv(GLuint index,
                                 GLenum pname,
                                 GLfloat* params) = 0;

  virtual void GetVertexAttribiv(GLuint index,
                                 GLenum pname,
                                 GLint* params) = 0;

  virtual void GetVertexAttribPointerv(GLuint index,
                                       GLenum pname,
                                       void** pointer) = 0;

  virtual void Hint(GLenum target, GLenum mode) = 0;

  virtual GLboolean IsBuffer(GLuint buffer) = 0;

  virtual GLboolean IsEnabled(GLenum cap) = 0;

  virtual GLboolean IsFramebufferEXT(GLuint framebuffer) = 0;

  virtual GLboolean IsProgram(GLuint program) = 0;

  virtual GLboolean IsQueryARB(GLuint query) = 0;

  virtual GLboolean IsRenderbufferEXT(GLuint renderbuffer) = 0;

  virtual GLboolean IsShader(GLuint shader) = 0;

  virtual GLboolean IsTexture(GLuint texture) = 0;

  virtual void* MapBuffer(GLenum target, GLenum access) = 0;

  virtual void LineWidth(GLfloat width) = 0;

  virtual void LinkProgram(GLuint program) = 0;

  virtual void PixelStorei(GLenum pname, GLint param) = 0;

  virtual void PolygonOffset(GLfloat factor, GLfloat units) = 0;

  virtual void QueryCounter(GLuint id, GLenum target) = 0;

  virtual void ReadBuffer(GLenum src) = 0;

  virtual void ReadPixels(GLint x, GLint y,
                          GLsizei width, GLsizei height,
                          GLenum format,
                          GLenum type,
                          void* pixels) = 0;

  virtual void ReleaseShaderCompiler(void) = 0;

  virtual void RenderbufferStorageEXT(GLenum target,
                                      GLenum internalformat,
                                      GLsizei width, GLsizei height) = 0;

  virtual void RenderbufferStorageMultisampleANGLE(GLenum target,
                                                   GLsizei samples,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height) = 0;

  virtual void RenderbufferStorageMultisampleEXT(GLenum target,
                                                 GLsizei samples,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height) = 0;

  virtual void SampleCoverage(GLclampf value, GLboolean invert) = 0;

  virtual void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) = 0;

  virtual void ShaderBinary(GLsizei n,
                            const GLuint* shaders,
                            GLenum binaryformat,
                            const void* binary,
                            GLsizei length) = 0;

  virtual void ShaderSource(GLuint shader,
                            GLsizei count,
                            const char** str,
                            const GLint* length) = 0;

  virtual void StencilFunc(GLenum func, GLint ref, GLuint mask) = 0;

  virtual void StencilFuncSeparate(GLenum face,
                                   GLenum func,
                                   GLint ref,
                                   GLuint mask) = 0;

  virtual void StencilMask(GLuint mask) = 0;

  virtual void StencilMaskSeparate(GLenum face, GLuint mask) = 0;

  virtual void StencilOp(GLenum fail, GLenum zfail, GLenum zpass) = 0;

  virtual void StencilOpSeparate(GLenum face,
                                 GLenum fail,
                                 GLenum zfail,
                                 GLenum zpass) = 0;

  virtual void TexImage2D(GLenum target,
                          GLint level,
                          GLint internalformat,
                          GLsizei width, GLsizei height,
                          GLint border,
                          GLenum format,
                          GLenum type,
                          const void* pixels) = 0;

  virtual void TexParameterf(GLenum target, GLenum pname, GLfloat param) = 0;

  virtual void TexParameterfv(GLenum target,
                              GLenum pname,
                              const GLfloat* params) = 0;

  virtual void TexParameteri(GLenum target, GLenum pname, GLint param) = 0;

  virtual void TexParameteriv(GLenum target,
                              GLenum pname,
                              const GLint* params) = 0;

  virtual void TexStorage2DEXT(GLenum target,
                               GLsizei levels,
                               GLenum internalformat,
                               GLsizei width,
                               GLsizei height) = 0;

  virtual void TexSubImage2D(GLenum target,
                             GLint level,
                             GLint xoffset, GLint yoffset,
                             GLsizei width, GLsizei height,
                             GLenum format,
                             GLenum type,
                             const void* pixels) = 0;

  virtual void Uniform1f(GLint location, GLfloat x) = 0;

  virtual void Uniform1fv(GLint location, GLsizei count, const GLfloat* v) = 0;

  virtual void Uniform1i(GLint location, GLint x) = 0;

  virtual void Uniform1iv(GLint location, GLsizei count, const GLint* v) = 0;

  virtual void Uniform2f(GLint location, GLfloat x, GLfloat y) = 0;

  virtual void Uniform2fv(GLint location, GLsizei count, const GLfloat* v) = 0;

  virtual void Uniform2i(GLint location, GLint x, GLint y) = 0;

  virtual void Uniform2iv(GLint location, GLsizei count, const GLint* v) = 0;

  virtual void Uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) = 0;

  virtual void Uniform3fv(GLint location, GLsizei count, const GLfloat* v) = 0;

  virtual void Uniform3i(GLint location, GLint x, GLint y, GLint z) = 0;

  virtual void Uniform3iv(GLint location, GLsizei count, const GLint* v) = 0;

  virtual void Uniform4f(GLint location,
                         GLfloat x,
                         GLfloat y,
                         GLfloat z,
                         GLfloat w) = 0;

  virtual void Uniform4fv(GLint location, GLsizei count, const GLfloat* v) = 0;

  virtual void Uniform4i(GLint location,
                         GLint x,
                         GLint y,
                         GLint z,
                         GLint w) = 0;

  virtual void Uniform4iv(GLint location, GLsizei count, const GLint* v) = 0;

  virtual void UniformMatrix2fv(GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat* value) = 0;

  virtual void UniformMatrix3fv(GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat* value) = 0;

  virtual void UniformMatrix4fv(GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat* value) = 0;

  virtual GLboolean UnmapBuffer(GLenum target) = 0;

  virtual void UseProgram(GLuint program) = 0;

  virtual void ValidateProgram(GLuint program) = 0;

  virtual void VertexAttrib1f(GLuint indx, GLfloat x) = 0;

  virtual void VertexAttrib1fv(GLuint indx, const GLfloat* values) = 0;

  virtual void VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) = 0;

  virtual void VertexAttrib2fv(GLuint indx, const GLfloat* values) = 0;

  virtual void VertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z) = 0;

  virtual void VertexAttrib3fv(GLuint indx, const GLfloat* values) = 0;

  virtual void VertexAttrib4f(GLuint indx,
                              GLfloat x,
                              GLfloat y,
                              GLfloat z,
                              GLfloat w) = 0;

  virtual void VertexAttrib4fv(GLuint indx, const GLfloat* values) = 0;

  virtual void VertexAttribPointer(GLuint indx,
                                   GLint size,
                                   GLenum type,
                                   GLboolean normalized,
                                   GLsizei stride,
                                   const void* ptr) = 0;

  virtual void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) = 0;

  virtual void SwapBuffers() = 0;

  virtual GLuint GetMaxValueInBufferCHROMIUM(GLuint buffer_id,
                                             GLsizei count,
                                             GLenum type,
                                             GLuint offset) = 0;

  virtual void GenFencesNV(GLsizei n, GLuint *fences) = 0;

  virtual void DeleteFencesNV(GLsizei n, const GLuint *fences) = 0;

  virtual void SetFenceNV(GLuint fence, GLenum condition) = 0;

  virtual GLboolean TestFenceNV(GLuint fence) = 0;

  virtual void FinishFenceNV(GLuint fence) = 0;

  virtual GLboolean IsFenceNV(GLuint fence) = 0;

  virtual void GetFenceivNV(GLuint fence, GLenum pname, GLint *params) = 0;

  virtual void SetSurfaceCHROMIUM(GLuint id) = 0;

  virtual GLenum GetGraphicsResetStatusARB() = 0;

  virtual GLsync FenceSync(GLenum condition, GLbitfield flags) = 0;

  virtual void DeleteSync(GLsync sync) = 0;

  virtual void GetSynciv(GLsync sync,
                         GLenum pname,
                         GLsizei bufSize,
                         GLsizei* length,
                         GLint* values) = 0;

  virtual void DrawArraysInstancedANGLE(GLenum mode,
                                        GLint first,
                                        GLsizei count,
                                        GLsizei primcount) = 0;

  virtual void DrawElementsInstancedANGLE(GLenum mode,
                                          GLsizei count,
                                          GLenum type,
                                          const void* indices,
                                          GLsizei primcount) = 0;

  virtual void VertexAttribDivisorANGLE(GLuint index, GLuint divisor) = 0;

 private:
  static GLInterface* interface_;
};

}  // namespace gfx

#endif  // UI_GL_GL_INTERFACE_H_
