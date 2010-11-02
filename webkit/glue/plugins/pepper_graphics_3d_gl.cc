// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated. DO NOT EDIT!

#include "webkit/glue/plugins/pepper_graphics_3d.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "third_party/ppapi/c/dev/ppb_opengles_dev.h"

namespace pepper {

namespace {

void ActiveTexture(GLenum texture) {
  Graphics3D::GetCurrent()->impl()->ActiveTexture(texture);
}
void AttachShader(GLuint program, GLuint shader) {
  Graphics3D::GetCurrent()->impl()->AttachShader(program, shader);
}
void BindAttribLocation(GLuint program, GLuint index, const char* name) {
  Graphics3D::GetCurrent()->impl()->BindAttribLocation(program, index, name);
}
void BindBuffer(GLenum target, GLuint buffer) {
  Graphics3D::GetCurrent()->impl()->BindBuffer(target, buffer);
}
void BindFramebuffer(GLenum target, GLuint framebuffer) {
  Graphics3D::GetCurrent()->impl()->BindFramebuffer(target, framebuffer);
}
void BindRenderbuffer(GLenum target, GLuint renderbuffer) {
  Graphics3D::GetCurrent()->impl()->BindRenderbuffer(target, renderbuffer);
}
void BindTexture(GLenum target, GLuint texture) {
  Graphics3D::GetCurrent()->impl()->BindTexture(target, texture);
}
void BlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  Graphics3D::GetCurrent()->impl()->BlendColor(red, green, blue, alpha);
}
void BlendEquation(GLenum mode) {
  Graphics3D::GetCurrent()->impl()->BlendEquation(mode);
}
void BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
  Graphics3D::GetCurrent()->impl()->BlendEquationSeparate(modeRGB, modeAlpha);
}
void BlendFunc(GLenum sfactor, GLenum dfactor) {
  Graphics3D::GetCurrent()->impl()->BlendFunc(sfactor, dfactor);
}
void BlendFuncSeparate(
    GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
  Graphics3D::GetCurrent()->impl()->BlendFuncSeparate(
      srcRGB, dstRGB, srcAlpha, dstAlpha);
}
void BufferData(
    GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
  Graphics3D::GetCurrent()->impl()->BufferData(target, size, data, usage);
}
void BufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
  Graphics3D::GetCurrent()->impl()->BufferSubData(target, offset, size, data);
}
GLenum CheckFramebufferStatus(GLenum target) {
  return Graphics3D::GetCurrent()->impl()->CheckFramebufferStatus(target);
}
void Clear(GLbitfield mask) {
  Graphics3D::GetCurrent()->impl()->Clear(mask);
}
void ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  Graphics3D::GetCurrent()->impl()->ClearColor(red, green, blue, alpha);
}
void ClearDepthf(GLclampf depth) {
  Graphics3D::GetCurrent()->impl()->ClearDepthf(depth);
}
void ClearStencil(GLint s) {
  Graphics3D::GetCurrent()->impl()->ClearStencil(s);
}
void ColorMask(
    GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
  Graphics3D::GetCurrent()->impl()->ColorMask(red, green, blue, alpha);
}
void CompileShader(GLuint shader) {
  Graphics3D::GetCurrent()->impl()->CompileShader(shader);
}
void CompressedTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLsizei width,
    GLsizei height, GLint border, GLsizei imageSize, const void* data) {
  Graphics3D::GetCurrent()->impl()->CompressedTexImage2D(
      target, level, internalformat, width, height, border, imageSize, data);
}
void CompressedTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
  Graphics3D::GetCurrent()->impl()->CompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
}
void CopyTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
    GLsizei width, GLsizei height, GLint border) {
  Graphics3D::GetCurrent()->impl()->CopyTexImage2D(
      target, level, internalformat, x, y, width, height, border);
}
void CopyTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
    GLsizei width, GLsizei height) {
  Graphics3D::GetCurrent()->impl()->CopyTexSubImage2D(
      target, level, xoffset, yoffset, x, y, width, height);
}
GLuint CreateProgram() {
  return Graphics3D::GetCurrent()->impl()->CreateProgram();
}
GLuint CreateShader(GLenum type) {
  return Graphics3D::GetCurrent()->impl()->CreateShader(type);
}
void CullFace(GLenum mode) {
  Graphics3D::GetCurrent()->impl()->CullFace(mode);
}
void DeleteBuffers(GLsizei n, const GLuint* buffers) {
  Graphics3D::GetCurrent()->impl()->DeleteBuffers(n, buffers);
}
void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  Graphics3D::GetCurrent()->impl()->DeleteFramebuffers(n, framebuffers);
}
void DeleteProgram(GLuint program) {
  Graphics3D::GetCurrent()->impl()->DeleteProgram(program);
}
void DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  Graphics3D::GetCurrent()->impl()->DeleteRenderbuffers(n, renderbuffers);
}
void DeleteShader(GLuint shader) {
  Graphics3D::GetCurrent()->impl()->DeleteShader(shader);
}
void DeleteTextures(GLsizei n, const GLuint* textures) {
  Graphics3D::GetCurrent()->impl()->DeleteTextures(n, textures);
}
void DepthFunc(GLenum func) {
  Graphics3D::GetCurrent()->impl()->DepthFunc(func);
}
void DepthMask(GLboolean flag) {
  Graphics3D::GetCurrent()->impl()->DepthMask(flag);
}
void DepthRangef(GLclampf zNear, GLclampf zFar) {
  Graphics3D::GetCurrent()->impl()->DepthRangef(zNear, zFar);
}
void DetachShader(GLuint program, GLuint shader) {
  Graphics3D::GetCurrent()->impl()->DetachShader(program, shader);
}
void Disable(GLenum cap) {
  Graphics3D::GetCurrent()->impl()->Disable(cap);
}
void DisableVertexAttribArray(GLuint index) {
  Graphics3D::GetCurrent()->impl()->DisableVertexAttribArray(index);
}
void DrawArrays(GLenum mode, GLint first, GLsizei count) {
  Graphics3D::GetCurrent()->impl()->DrawArrays(mode, first, count);
}
void DrawElements(
    GLenum mode, GLsizei count, GLenum type, const void* indices) {
  Graphics3D::GetCurrent()->impl()->DrawElements(mode, count, type, indices);
}
void Enable(GLenum cap) {
  Graphics3D::GetCurrent()->impl()->Enable(cap);
}
void EnableVertexAttribArray(GLuint index) {
  Graphics3D::GetCurrent()->impl()->EnableVertexAttribArray(index);
}
void Finish() {
  Graphics3D::GetCurrent()->impl()->Finish();
}
void Flush() {
  Graphics3D::GetCurrent()->impl()->Flush();
}
void FramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum renderbuffertarget,
    GLuint renderbuffer) {
  Graphics3D::GetCurrent()->impl()->FramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
}
void FramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
    GLint level) {
  Graphics3D::GetCurrent()->impl()->FramebufferTexture2D(
      target, attachment, textarget, texture, level);
}
void FrontFace(GLenum mode) {
  Graphics3D::GetCurrent()->impl()->FrontFace(mode);
}
void GenBuffers(GLsizei n, GLuint* buffers) {
  Graphics3D::GetCurrent()->impl()->GenBuffers(n, buffers);
}
void GenerateMipmap(GLenum target) {
  Graphics3D::GetCurrent()->impl()->GenerateMipmap(target);
}
void GenFramebuffers(GLsizei n, GLuint* framebuffers) {
  Graphics3D::GetCurrent()->impl()->GenFramebuffers(n, framebuffers);
}
void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  Graphics3D::GetCurrent()->impl()->GenRenderbuffers(n, renderbuffers);
}
void GenTextures(GLsizei n, GLuint* textures) {
  Graphics3D::GetCurrent()->impl()->GenTextures(n, textures);
}
void GetActiveAttrib(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  Graphics3D::GetCurrent()->impl()->GetActiveAttrib(
      program, index, bufsize, length, size, type, name);
}
void GetActiveUniform(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  Graphics3D::GetCurrent()->impl()->GetActiveUniform(
      program, index, bufsize, length, size, type, name);
}
void GetAttachedShaders(
    GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) {
  Graphics3D::GetCurrent()->impl()->GetAttachedShaders(
      program, maxcount, count, shaders);
}
GLint GetAttribLocation(GLuint program, const char* name) {
  return Graphics3D::GetCurrent()->impl()->GetAttribLocation(program, name);
}
void GetBooleanv(GLenum pname, GLboolean* params) {
  Graphics3D::GetCurrent()->impl()->GetBooleanv(pname, params);
}
void GetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
  Graphics3D::GetCurrent()->impl()->GetBufferParameteriv(
      target, pname, params);
}
GLenum GetError() {
  return Graphics3D::GetCurrent()->impl()->GetError();
}
void GetFloatv(GLenum pname, GLfloat* params) {
  Graphics3D::GetCurrent()->impl()->GetFloatv(pname, params);
}
void GetFramebufferAttachmentParameteriv(
    GLenum target, GLenum attachment, GLenum pname, GLint* params) {
  Graphics3D::GetCurrent()->impl()->GetFramebufferAttachmentParameteriv(
      target, attachment, pname, params);
}
void GetIntegerv(GLenum pname, GLint* params) {
  Graphics3D::GetCurrent()->impl()->GetIntegerv(pname, params);
}
void GetProgramiv(GLuint program, GLenum pname, GLint* params) {
  Graphics3D::GetCurrent()->impl()->GetProgramiv(program, pname, params);
}
void GetProgramInfoLog(
    GLuint program, GLsizei bufsize, GLsizei* length, char* infolog) {
  Graphics3D::GetCurrent()->impl()->GetProgramInfoLog(
      program, bufsize, length, infolog);
}
void GetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params) {
  Graphics3D::GetCurrent()->impl()->GetRenderbufferParameteriv(
      target, pname, params);
}
void GetShaderiv(GLuint shader, GLenum pname, GLint* params) {
  Graphics3D::GetCurrent()->impl()->GetShaderiv(shader, pname, params);
}
void GetShaderInfoLog(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog) {
  Graphics3D::GetCurrent()->impl()->GetShaderInfoLog(
      shader, bufsize, length, infolog);
}
void GetShaderPrecisionFormat(
    GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
  Graphics3D::GetCurrent()->impl()->GetShaderPrecisionFormat(
      shadertype, precisiontype, range, precision);
}
void GetShaderSource(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* source) {
  Graphics3D::GetCurrent()->impl()->GetShaderSource(
      shader, bufsize, length, source);
}
const GLubyte* GetString(GLenum name) {
  return Graphics3D::GetCurrent()->impl()->GetString(name);
}
void GetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) {
  Graphics3D::GetCurrent()->impl()->GetTexParameterfv(target, pname, params);
}
void GetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
  Graphics3D::GetCurrent()->impl()->GetTexParameteriv(target, pname, params);
}
void GetUniformfv(GLuint program, GLint location, GLfloat* params) {
  Graphics3D::GetCurrent()->impl()->GetUniformfv(program, location, params);
}
void GetUniformiv(GLuint program, GLint location, GLint* params) {
  Graphics3D::GetCurrent()->impl()->GetUniformiv(program, location, params);
}
GLint GetUniformLocation(GLuint program, const char* name) {
  return Graphics3D::GetCurrent()->impl()->GetUniformLocation(program, name);
}
void GetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params) {
  Graphics3D::GetCurrent()->impl()->GetVertexAttribfv(index, pname, params);
}
void GetVertexAttribiv(GLuint index, GLenum pname, GLint* params) {
  Graphics3D::GetCurrent()->impl()->GetVertexAttribiv(index, pname, params);
}
void GetVertexAttribPointerv(GLuint index, GLenum pname, void** pointer) {
  Graphics3D::GetCurrent()->impl()->GetVertexAttribPointerv(
      index, pname, pointer);
}
void Hint(GLenum target, GLenum mode) {
  Graphics3D::GetCurrent()->impl()->Hint(target, mode);
}
GLboolean IsBuffer(GLuint buffer) {
  return Graphics3D::GetCurrent()->impl()->IsBuffer(buffer);
}
GLboolean IsEnabled(GLenum cap) {
  return Graphics3D::GetCurrent()->impl()->IsEnabled(cap);
}
GLboolean IsFramebuffer(GLuint framebuffer) {
  return Graphics3D::GetCurrent()->impl()->IsFramebuffer(framebuffer);
}
GLboolean IsProgram(GLuint program) {
  return Graphics3D::GetCurrent()->impl()->IsProgram(program);
}
GLboolean IsRenderbuffer(GLuint renderbuffer) {
  return Graphics3D::GetCurrent()->impl()->IsRenderbuffer(renderbuffer);
}
GLboolean IsShader(GLuint shader) {
  return Graphics3D::GetCurrent()->impl()->IsShader(shader);
}
GLboolean IsTexture(GLuint texture) {
  return Graphics3D::GetCurrent()->impl()->IsTexture(texture);
}
void LineWidth(GLfloat width) {
  Graphics3D::GetCurrent()->impl()->LineWidth(width);
}
void LinkProgram(GLuint program) {
  Graphics3D::GetCurrent()->impl()->LinkProgram(program);
}
void PixelStorei(GLenum pname, GLint param) {
  Graphics3D::GetCurrent()->impl()->PixelStorei(pname, param);
}
void PolygonOffset(GLfloat factor, GLfloat units) {
  Graphics3D::GetCurrent()->impl()->PolygonOffset(factor, units);
}
void ReadPixels(
    GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
    void* pixels) {
  Graphics3D::GetCurrent()->impl()->ReadPixels(
      x, y, width, height, format, type, pixels);
}
void ReleaseShaderCompiler() {
  Graphics3D::GetCurrent()->impl()->ReleaseShaderCompiler();
}
void RenderbufferStorage(
    GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  Graphics3D::GetCurrent()->impl()->RenderbufferStorage(
      target, internalformat, width, height);
}
void SampleCoverage(GLclampf value, GLboolean invert) {
  Graphics3D::GetCurrent()->impl()->SampleCoverage(value, invert);
}
void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  Graphics3D::GetCurrent()->impl()->Scissor(x, y, width, height);
}
void ShaderBinary(
    GLsizei n, const GLuint* shaders, GLenum binaryformat, const void* binary,
    GLsizei length) {
  Graphics3D::GetCurrent()->impl()->ShaderBinary(
      n, shaders, binaryformat, binary, length);
}
void ShaderSource(
    GLuint shader, GLsizei count, const char** str, const GLint* length) {
  Graphics3D::GetCurrent()->impl()->ShaderSource(shader, count, str, length);
}
void StencilFunc(GLenum func, GLint ref, GLuint mask) {
  Graphics3D::GetCurrent()->impl()->StencilFunc(func, ref, mask);
}
void StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
  Graphics3D::GetCurrent()->impl()->StencilFuncSeparate(face, func, ref, mask);
}
void StencilMask(GLuint mask) {
  Graphics3D::GetCurrent()->impl()->StencilMask(mask);
}
void StencilMaskSeparate(GLenum face, GLuint mask) {
  Graphics3D::GetCurrent()->impl()->StencilMaskSeparate(face, mask);
}
void StencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  Graphics3D::GetCurrent()->impl()->StencilOp(fail, zfail, zpass);
}
void StencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass) {
  Graphics3D::GetCurrent()->impl()->StencilOpSeparate(
      face, fail, zfail, zpass);
}
void TexImage2D(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  Graphics3D::GetCurrent()->impl()->TexImage2D(
      target, level, internalformat, width, height, border, format, type,
      pixels);
}
void TexParameterf(GLenum target, GLenum pname, GLfloat param) {
  Graphics3D::GetCurrent()->impl()->TexParameterf(target, pname, param);
}
void TexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
  Graphics3D::GetCurrent()->impl()->TexParameterfv(target, pname, params);
}
void TexParameteri(GLenum target, GLenum pname, GLint param) {
  Graphics3D::GetCurrent()->impl()->TexParameteri(target, pname, param);
}
void TexParameteriv(GLenum target, GLenum pname, const GLint* params) {
  Graphics3D::GetCurrent()->impl()->TexParameteriv(target, pname, params);
}
void TexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels) {
  Graphics3D::GetCurrent()->impl()->TexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
}
void Uniform1f(GLint location, GLfloat x) {
  Graphics3D::GetCurrent()->impl()->Uniform1f(location, x);
}
void Uniform1fv(GLint location, GLsizei count, const GLfloat* v) {
  Graphics3D::GetCurrent()->impl()->Uniform1fv(location, count, v);
}
void Uniform1i(GLint location, GLint x) {
  Graphics3D::GetCurrent()->impl()->Uniform1i(location, x);
}
void Uniform1iv(GLint location, GLsizei count, const GLint* v) {
  Graphics3D::GetCurrent()->impl()->Uniform1iv(location, count, v);
}
void Uniform2f(GLint location, GLfloat x, GLfloat y) {
  Graphics3D::GetCurrent()->impl()->Uniform2f(location, x, y);
}
void Uniform2fv(GLint location, GLsizei count, const GLfloat* v) {
  Graphics3D::GetCurrent()->impl()->Uniform2fv(location, count, v);
}
void Uniform2i(GLint location, GLint x, GLint y) {
  Graphics3D::GetCurrent()->impl()->Uniform2i(location, x, y);
}
void Uniform2iv(GLint location, GLsizei count, const GLint* v) {
  Graphics3D::GetCurrent()->impl()->Uniform2iv(location, count, v);
}
void Uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
  Graphics3D::GetCurrent()->impl()->Uniform3f(location, x, y, z);
}
void Uniform3fv(GLint location, GLsizei count, const GLfloat* v) {
  Graphics3D::GetCurrent()->impl()->Uniform3fv(location, count, v);
}
void Uniform3i(GLint location, GLint x, GLint y, GLint z) {
  Graphics3D::GetCurrent()->impl()->Uniform3i(location, x, y, z);
}
void Uniform3iv(GLint location, GLsizei count, const GLint* v) {
  Graphics3D::GetCurrent()->impl()->Uniform3iv(location, count, v);
}
void Uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  Graphics3D::GetCurrent()->impl()->Uniform4f(location, x, y, z, w);
}
void Uniform4fv(GLint location, GLsizei count, const GLfloat* v) {
  Graphics3D::GetCurrent()->impl()->Uniform4fv(location, count, v);
}
void Uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
  Graphics3D::GetCurrent()->impl()->Uniform4i(location, x, y, z, w);
}
void Uniform4iv(GLint location, GLsizei count, const GLint* v) {
  Graphics3D::GetCurrent()->impl()->Uniform4iv(location, count, v);
}
void UniformMatrix2fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  Graphics3D::GetCurrent()->impl()->UniformMatrix2fv(
      location, count, transpose, value);
}
void UniformMatrix3fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  Graphics3D::GetCurrent()->impl()->UniformMatrix3fv(
      location, count, transpose, value);
}
void UniformMatrix4fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  Graphics3D::GetCurrent()->impl()->UniformMatrix4fv(
      location, count, transpose, value);
}
void UseProgram(GLuint program) {
  Graphics3D::GetCurrent()->impl()->UseProgram(program);
}
void ValidateProgram(GLuint program) {
  Graphics3D::GetCurrent()->impl()->ValidateProgram(program);
}
void VertexAttrib1f(GLuint indx, GLfloat x) {
  Graphics3D::GetCurrent()->impl()->VertexAttrib1f(indx, x);
}
void VertexAttrib1fv(GLuint indx, const GLfloat* values) {
  Graphics3D::GetCurrent()->impl()->VertexAttrib1fv(indx, values);
}
void VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
  Graphics3D::GetCurrent()->impl()->VertexAttrib2f(indx, x, y);
}
void VertexAttrib2fv(GLuint indx, const GLfloat* values) {
  Graphics3D::GetCurrent()->impl()->VertexAttrib2fv(indx, values);
}
void VertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  Graphics3D::GetCurrent()->impl()->VertexAttrib3f(indx, x, y, z);
}
void VertexAttrib3fv(GLuint indx, const GLfloat* values) {
  Graphics3D::GetCurrent()->impl()->VertexAttrib3fv(indx, values);
}
void VertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  Graphics3D::GetCurrent()->impl()->VertexAttrib4f(indx, x, y, z, w);
}
void VertexAttrib4fv(GLuint indx, const GLfloat* values) {
  Graphics3D::GetCurrent()->impl()->VertexAttrib4fv(indx, values);
}
void VertexAttribPointer(
    GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
    const void* ptr) {
  Graphics3D::GetCurrent()->impl()->VertexAttribPointer(
      indx, size, type, normalized, stride, ptr);
}
void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  Graphics3D::GetCurrent()->impl()->Viewport(x, y, width, height);
}
void SwapBuffers() {
  Graphics3D::GetCurrent()->impl()->SwapBuffers();
}
GLuint GetMaxValueInBuffer(
    GLuint buffer_id, GLsizei count, GLenum type, GLuint offset) {
  return Graphics3D::GetCurrent()->impl()->GetMaxValueInBuffer(
      buffer_id, count, type, offset);
}
void GenSharedIds(
    GLuint namespace_id, GLuint id_offset, GLsizei n, GLuint* ids) {
  Graphics3D::GetCurrent()->impl()->GenSharedIds(
      namespace_id, id_offset, n, ids);
}
void DeleteSharedIds(GLuint namespace_id, GLsizei n, const GLuint* ids) {
  Graphics3D::GetCurrent()->impl()->DeleteSharedIds(namespace_id, n, ids);
}
void RegisterSharedIds(GLuint namespace_id, GLsizei n, const GLuint* ids) {
  Graphics3D::GetCurrent()->impl()->RegisterSharedIds(namespace_id, n, ids);
}
GLboolean CommandBufferEnable(const char* feature) {
  return Graphics3D::GetCurrent()->impl()->CommandBufferEnable(feature);
}
void* MapBufferSubData(
    GLuint target, GLintptr offset, GLsizeiptr size, GLenum access) {
  return Graphics3D::GetCurrent()->impl()->MapBufferSubData(
      target, offset, size, access);
}
void UnmapBufferSubData(const void* mem) {
  Graphics3D::GetCurrent()->impl()->UnmapBufferSubData(mem);
}
void* MapTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, GLenum access) {
  return Graphics3D::GetCurrent()->impl()->MapTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, access);
}
void UnmapTexSubImage2D(const void* mem) {
  Graphics3D::GetCurrent()->impl()->UnmapTexSubImage2D(mem);
}

const PPB_OpenGLES_Dev ppb_opengles = {
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
  &SwapBuffers,
  &GetMaxValueInBuffer,
  &GenSharedIds,
  &DeleteSharedIds,
  &RegisterSharedIds,
  &CommandBufferEnable,
  &MapBufferSubData,
  &UnmapBufferSubData,
  &MapTexSubImage2D,
  &UnmapTexSubImage2D
};

}  // namespace

const PPB_OpenGLES_Dev* Graphics3D::GetOpenGLESInterface() {
  return &ppb_opengles;
}

}  // namespace pepper

