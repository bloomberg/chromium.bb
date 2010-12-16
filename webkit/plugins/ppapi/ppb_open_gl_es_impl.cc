// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated. DO NOT EDIT!

#include "webkit/plugins/ppapi/ppb_graphics_3d_impl.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/dev/ppb_opengles_dev.h"

namespace webkit {
namespace ppapi {

namespace {

void ActiveTexture(GLenum texture) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->ActiveTexture(texture);
}
void AttachShader(GLuint program, GLuint shader) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->AttachShader(program, shader);
}
void BindAttribLocation(GLuint program, GLuint index, const char* name) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->BindAttribLocation(program, index, name);
}
void BindBuffer(GLenum target, GLuint buffer) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->BindBuffer(target, buffer);
}
void BindFramebuffer(GLenum target, GLuint framebuffer) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->BindFramebuffer(target, framebuffer);
}
void BindRenderbuffer(GLenum target, GLuint renderbuffer) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->BindRenderbuffer(target, renderbuffer);
}
void BindTexture(GLenum target, GLuint texture) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->BindTexture(target, texture);
}
void BlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->BlendColor(red, green, blue, alpha);
}
void BlendEquation(GLenum mode) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->BlendEquation(mode);
}
void BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->BlendEquationSeparate(modeRGB, modeAlpha);
}
void BlendFunc(GLenum sfactor, GLenum dfactor) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->BlendFunc(sfactor, dfactor);
}
void BlendFuncSeparate(
    GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->BlendFuncSeparate(
      srcRGB, dstRGB, srcAlpha, dstAlpha);
}
void BufferData(
    GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->BufferData(target, size, data, usage);
}
void BufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->BufferSubData(target, offset, size, data);
}
GLenum CheckFramebufferStatus(GLenum target) {
  return PPB_Graphics3D_Impl::GetCurrent()->impl()->CheckFramebufferStatus(target);
}
void Clear(GLbitfield mask) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Clear(mask);
}
void ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->ClearColor(red, green, blue, alpha);
}
void ClearDepthf(GLclampf depth) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->ClearDepthf(depth);
}
void ClearStencil(GLint s) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->ClearStencil(s);
}
void ColorMask(
    GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->ColorMask(red, green, blue, alpha);
}
void CompileShader(GLuint shader) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->CompileShader(shader);
}
void CompressedTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLsizei width,
    GLsizei height, GLint border, GLsizei imageSize, const void* data) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->CompressedTexImage2D(
      target, level, internalformat, width, height, border, imageSize, data);
}
void CompressedTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->CompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
}
void CopyTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
    GLsizei width, GLsizei height, GLint border) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->CopyTexImage2D(
      target, level, internalformat, x, y, width, height, border);
}
void CopyTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
    GLsizei width, GLsizei height) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->CopyTexSubImage2D(
      target, level, xoffset, yoffset, x, y, width, height);
}
GLuint CreateProgram() {
  return PPB_Graphics3D_Impl::GetCurrent()->impl()->CreateProgram();
}
GLuint CreateShader(GLenum type) {
  return PPB_Graphics3D_Impl::GetCurrent()->impl()->CreateShader(type);
}
void CullFace(GLenum mode) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->CullFace(mode);
}
void DeleteBuffers(GLsizei n, const GLuint* buffers) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->DeleteBuffers(n, buffers);
}
void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->DeleteFramebuffers(n, framebuffers);
}
void DeleteProgram(GLuint program) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->DeleteProgram(program);
}
void DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->DeleteRenderbuffers(n, renderbuffers);
}
void DeleteShader(GLuint shader) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->DeleteShader(shader);
}
void DeleteTextures(GLsizei n, const GLuint* textures) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->DeleteTextures(n, textures);
}
void DepthFunc(GLenum func) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->DepthFunc(func);
}
void DepthMask(GLboolean flag) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->DepthMask(flag);
}
void DepthRangef(GLclampf zNear, GLclampf zFar) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->DepthRangef(zNear, zFar);
}
void DetachShader(GLuint program, GLuint shader) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->DetachShader(program, shader);
}
void Disable(GLenum cap) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Disable(cap);
}
void DisableVertexAttribArray(GLuint index) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->DisableVertexAttribArray(index);
}
void DrawArrays(GLenum mode, GLint first, GLsizei count) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->DrawArrays(mode, first, count);
}
void DrawElements(
    GLenum mode, GLsizei count, GLenum type, const void* indices) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->DrawElements(mode, count, type, indices);
}
void Enable(GLenum cap) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Enable(cap);
}
void EnableVertexAttribArray(GLuint index) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->EnableVertexAttribArray(index);
}
void Finish() {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Finish();
}
void Flush() {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Flush();
}
void FramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum renderbuffertarget,
    GLuint renderbuffer) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->FramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
}
void FramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
    GLint level) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->FramebufferTexture2D(
      target, attachment, textarget, texture, level);
}
void FrontFace(GLenum mode) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->FrontFace(mode);
}
void GenBuffers(GLsizei n, GLuint* buffers) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GenBuffers(n, buffers);
}
void GenerateMipmap(GLenum target) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GenerateMipmap(target);
}
void GenFramebuffers(GLsizei n, GLuint* framebuffers) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GenFramebuffers(n, framebuffers);
}
void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GenRenderbuffers(n, renderbuffers);
}
void GenTextures(GLsizei n, GLuint* textures) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GenTextures(n, textures);
}
void GetActiveAttrib(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetActiveAttrib(
      program, index, bufsize, length, size, type, name);
}
void GetActiveUniform(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetActiveUniform(
      program, index, bufsize, length, size, type, name);
}
void GetAttachedShaders(
    GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetAttachedShaders(
      program, maxcount, count, shaders);
}
GLint GetAttribLocation(GLuint program, const char* name) {
  return PPB_Graphics3D_Impl::GetCurrent()->impl()->GetAttribLocation(program, name);
}
void GetBooleanv(GLenum pname, GLboolean* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetBooleanv(pname, params);
}
void GetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetBufferParameteriv(
      target, pname, params);
}
GLenum GetError() {
  return PPB_Graphics3D_Impl::GetCurrent()->impl()->GetError();
}
void GetFloatv(GLenum pname, GLfloat* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetFloatv(pname, params);
}
void GetFramebufferAttachmentParameteriv(
    GLenum target, GLenum attachment, GLenum pname, GLint* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetFramebufferAttachmentParameteriv(
      target, attachment, pname, params);
}
void GetIntegerv(GLenum pname, GLint* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetIntegerv(pname, params);
}
void GetProgramiv(GLuint program, GLenum pname, GLint* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetProgramiv(program, pname, params);
}
void GetProgramInfoLog(
    GLuint program, GLsizei bufsize, GLsizei* length, char* infolog) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetProgramInfoLog(
      program, bufsize, length, infolog);
}
void GetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetRenderbufferParameteriv(
      target, pname, params);
}
void GetShaderiv(GLuint shader, GLenum pname, GLint* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetShaderiv(shader, pname, params);
}
void GetShaderInfoLog(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetShaderInfoLog(
      shader, bufsize, length, infolog);
}
void GetShaderPrecisionFormat(
    GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetShaderPrecisionFormat(
      shadertype, precisiontype, range, precision);
}
void GetShaderSource(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* source) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetShaderSource(
      shader, bufsize, length, source);
}
const GLubyte* GetString(GLenum name) {
  return PPB_Graphics3D_Impl::GetCurrent()->impl()->GetString(name);
}
void GetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetTexParameterfv(target, pname, params);
}
void GetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetTexParameteriv(target, pname, params);
}
void GetUniformfv(GLuint program, GLint location, GLfloat* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetUniformfv(program, location, params);
}
void GetUniformiv(GLuint program, GLint location, GLint* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetUniformiv(program, location, params);
}
GLint GetUniformLocation(GLuint program, const char* name) {
  return PPB_Graphics3D_Impl::GetCurrent()->impl()->GetUniformLocation(program, name);
}
void GetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetVertexAttribfv(index, pname, params);
}
void GetVertexAttribiv(GLuint index, GLenum pname, GLint* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetVertexAttribiv(index, pname, params);
}
void GetVertexAttribPointerv(GLuint index, GLenum pname, void** pointer) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->GetVertexAttribPointerv(
      index, pname, pointer);
}
void Hint(GLenum target, GLenum mode) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Hint(target, mode);
}
GLboolean IsBuffer(GLuint buffer) {
  return PPB_Graphics3D_Impl::GetCurrent()->impl()->IsBuffer(buffer);
}
GLboolean IsEnabled(GLenum cap) {
  return PPB_Graphics3D_Impl::GetCurrent()->impl()->IsEnabled(cap);
}
GLboolean IsFramebuffer(GLuint framebuffer) {
  return PPB_Graphics3D_Impl::GetCurrent()->impl()->IsFramebuffer(framebuffer);
}
GLboolean IsProgram(GLuint program) {
  return PPB_Graphics3D_Impl::GetCurrent()->impl()->IsProgram(program);
}
GLboolean IsRenderbuffer(GLuint renderbuffer) {
  return PPB_Graphics3D_Impl::GetCurrent()->impl()->IsRenderbuffer(renderbuffer);
}
GLboolean IsShader(GLuint shader) {
  return PPB_Graphics3D_Impl::GetCurrent()->impl()->IsShader(shader);
}
GLboolean IsTexture(GLuint texture) {
  return PPB_Graphics3D_Impl::GetCurrent()->impl()->IsTexture(texture);
}
void LineWidth(GLfloat width) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->LineWidth(width);
}
void LinkProgram(GLuint program) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->LinkProgram(program);
}
void PixelStorei(GLenum pname, GLint param) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->PixelStorei(pname, param);
}
void PolygonOffset(GLfloat factor, GLfloat units) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->PolygonOffset(factor, units);
}
void ReadPixels(
    GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
    void* pixels) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->ReadPixels(
      x, y, width, height, format, type, pixels);
}
void ReleaseShaderCompiler() {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->ReleaseShaderCompiler();
}
void RenderbufferStorage(
    GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->RenderbufferStorage(
      target, internalformat, width, height);
}
void SampleCoverage(GLclampf value, GLboolean invert) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->SampleCoverage(value, invert);
}
void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Scissor(x, y, width, height);
}
void ShaderBinary(
    GLsizei n, const GLuint* shaders, GLenum binaryformat, const void* binary,
    GLsizei length) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->ShaderBinary(
      n, shaders, binaryformat, binary, length);
}
void ShaderSource(
    GLuint shader, GLsizei count, const char** str, const GLint* length) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->ShaderSource(shader, count, str, length);
}
void StencilFunc(GLenum func, GLint ref, GLuint mask) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->StencilFunc(func, ref, mask);
}
void StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->StencilFuncSeparate(face, func, ref, mask);
}
void StencilMask(GLuint mask) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->StencilMask(mask);
}
void StencilMaskSeparate(GLenum face, GLuint mask) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->StencilMaskSeparate(face, mask);
}
void StencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->StencilOp(fail, zfail, zpass);
}
void StencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->StencilOpSeparate(
      face, fail, zfail, zpass);
}
void TexImage2D(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->TexImage2D(
      target, level, internalformat, width, height, border, format, type,
      pixels);
}
void TexParameterf(GLenum target, GLenum pname, GLfloat param) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->TexParameterf(target, pname, param);
}
void TexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->TexParameterfv(target, pname, params);
}
void TexParameteri(GLenum target, GLenum pname, GLint param) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->TexParameteri(target, pname, param);
}
void TexParameteriv(GLenum target, GLenum pname, const GLint* params) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->TexParameteriv(target, pname, params);
}
void TexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->TexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
}
void Uniform1f(GLint location, GLfloat x) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform1f(location, x);
}
void Uniform1fv(GLint location, GLsizei count, const GLfloat* v) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform1fv(location, count, v);
}
void Uniform1i(GLint location, GLint x) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform1i(location, x);
}
void Uniform1iv(GLint location, GLsizei count, const GLint* v) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform1iv(location, count, v);
}
void Uniform2f(GLint location, GLfloat x, GLfloat y) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform2f(location, x, y);
}
void Uniform2fv(GLint location, GLsizei count, const GLfloat* v) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform2fv(location, count, v);
}
void Uniform2i(GLint location, GLint x, GLint y) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform2i(location, x, y);
}
void Uniform2iv(GLint location, GLsizei count, const GLint* v) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform2iv(location, count, v);
}
void Uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform3f(location, x, y, z);
}
void Uniform3fv(GLint location, GLsizei count, const GLfloat* v) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform3fv(location, count, v);
}
void Uniform3i(GLint location, GLint x, GLint y, GLint z) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform3i(location, x, y, z);
}
void Uniform3iv(GLint location, GLsizei count, const GLint* v) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform3iv(location, count, v);
}
void Uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform4f(location, x, y, z, w);
}
void Uniform4fv(GLint location, GLsizei count, const GLfloat* v) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform4fv(location, count, v);
}
void Uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform4i(location, x, y, z, w);
}
void Uniform4iv(GLint location, GLsizei count, const GLint* v) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Uniform4iv(location, count, v);
}
void UniformMatrix2fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->UniformMatrix2fv(
      location, count, transpose, value);
}
void UniformMatrix3fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->UniformMatrix3fv(
      location, count, transpose, value);
}
void UniformMatrix4fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->UniformMatrix4fv(
      location, count, transpose, value);
}
void UseProgram(GLuint program) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->UseProgram(program);
}
void ValidateProgram(GLuint program) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->ValidateProgram(program);
}
void VertexAttrib1f(GLuint indx, GLfloat x) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->VertexAttrib1f(indx, x);
}
void VertexAttrib1fv(GLuint indx, const GLfloat* values) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->VertexAttrib1fv(indx, values);
}
void VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->VertexAttrib2f(indx, x, y);
}
void VertexAttrib2fv(GLuint indx, const GLfloat* values) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->VertexAttrib2fv(indx, values);
}
void VertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->VertexAttrib3f(indx, x, y, z);
}
void VertexAttrib3fv(GLuint indx, const GLfloat* values) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->VertexAttrib3fv(indx, values);
}
void VertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->VertexAttrib4f(indx, x, y, z, w);
}
void VertexAttrib4fv(GLuint indx, const GLfloat* values) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->VertexAttrib4fv(indx, values);
}
void VertexAttribPointer(
    GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
    const void* ptr) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->VertexAttribPointer(
      indx, size, type, normalized, stride, ptr);
}
void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->Viewport(x, y, width, height);
}
void SwapBuffers() {
  PPB_Graphics3D_Impl::GetCurrent()->impl()->SwapBuffers();
}

const struct PPB_OpenGLES_Dev ppb_opengles = {
  &ActiveTexture,
  &AttachShader,
  &BindAttribLocation,
  &BindBuffer,
  &BindFramebuffer,
  &BindRenderbuffer,
  &BindTexture,
  &BlendColor,
  &BlendEquation,
  &BlendEquationSeparate,
  &BlendFunc,
  &BlendFuncSeparate,
  &BufferData,
  &BufferSubData,
  &CheckFramebufferStatus,
  &Clear,
  &ClearColor,
  &ClearDepthf,
  &ClearStencil,
  &ColorMask,
  &CompileShader,
  &CompressedTexImage2D,
  &CompressedTexSubImage2D,
  &CopyTexImage2D,
  &CopyTexSubImage2D,
  &CreateProgram,
  &CreateShader,
  &CullFace,
  &DeleteBuffers,
  &DeleteFramebuffers,
  &DeleteProgram,
  &DeleteRenderbuffers,
  &DeleteShader,
  &DeleteTextures,
  &DepthFunc,
  &DepthMask,
  &DepthRangef,
  &DetachShader,
  &Disable,
  &DisableVertexAttribArray,
  &DrawArrays,
  &DrawElements,
  &Enable,
  &EnableVertexAttribArray,
  &Finish,
  &Flush,
  &FramebufferRenderbuffer,
  &FramebufferTexture2D,
  &FrontFace,
  &GenBuffers,
  &GenerateMipmap,
  &GenFramebuffers,
  &GenRenderbuffers,
  &GenTextures,
  &GetActiveAttrib,
  &GetActiveUniform,
  &GetAttachedShaders,
  &GetAttribLocation,
  &GetBooleanv,
  &GetBufferParameteriv,
  &GetError,
  &GetFloatv,
  &GetFramebufferAttachmentParameteriv,
  &GetIntegerv,
  &GetProgramiv,
  &GetProgramInfoLog,
  &GetRenderbufferParameteriv,
  &GetShaderiv,
  &GetShaderInfoLog,
  &GetShaderPrecisionFormat,
  &GetShaderSource,
  &GetString,
  &GetTexParameterfv,
  &GetTexParameteriv,
  &GetUniformfv,
  &GetUniformiv,
  &GetUniformLocation,
  &GetVertexAttribfv,
  &GetVertexAttribiv,
  &GetVertexAttribPointerv,
  &Hint,
  &IsBuffer,
  &IsEnabled,
  &IsFramebuffer,
  &IsProgram,
  &IsRenderbuffer,
  &IsShader,
  &IsTexture,
  &LineWidth,
  &LinkProgram,
  &PixelStorei,
  &PolygonOffset,
  &ReadPixels,
  &ReleaseShaderCompiler,
  &RenderbufferStorage,
  &SampleCoverage,
  &Scissor,
  &ShaderBinary,
  &ShaderSource,
  &StencilFunc,
  &StencilFuncSeparate,
  &StencilMask,
  &StencilMaskSeparate,
  &StencilOp,
  &StencilOpSeparate,
  &TexImage2D,
  &TexParameterf,
  &TexParameterfv,
  &TexParameteri,
  &TexParameteriv,
  &TexSubImage2D,
  &Uniform1f,
  &Uniform1fv,
  &Uniform1i,
  &Uniform1iv,
  &Uniform2f,
  &Uniform2fv,
  &Uniform2i,
  &Uniform2iv,
  &Uniform3f,
  &Uniform3fv,
  &Uniform3i,
  &Uniform3iv,
  &Uniform4f,
  &Uniform4fv,
  &Uniform4i,
  &Uniform4iv,
  &UniformMatrix2fv,
  &UniformMatrix3fv,
  &UniformMatrix4fv,
  &UseProgram,
  &ValidateProgram,
  &VertexAttrib1f,
  &VertexAttrib1fv,
  &VertexAttrib2f,
  &VertexAttrib2fv,
  &VertexAttrib3f,
  &VertexAttrib3fv,
  &VertexAttrib4f,
  &VertexAttrib4fv,
  &VertexAttribPointer,
  &Viewport,
  &SwapBuffers
};

}  // namespace

const PPB_OpenGLES_Dev* PPB_Graphics3D_Impl::GetOpenGLESInterface() {
  return &ppb_opengles;
}

}  // namespace ppapi
}  // namespace webkit

