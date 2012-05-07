// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_graphics_3d.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/ppb_opengles2.h"

using ppapi_proxy::PluginGraphics3D;
using ppapi_proxy::PluginResource;

namespace {

void ActiveTexture(PP_Resource context, GLenum texture) {
  PluginGraphics3D::implFromResource(context)->ActiveTexture(texture);
}
void AttachShader(PP_Resource context, GLuint program, GLuint shader) {
  PluginGraphics3D::implFromResource(context)->AttachShader(program, shader);
}
void BindAttribLocation(
    PP_Resource context, GLuint program, GLuint index, const char* name) {
  PluginGraphics3D::implFromResource(
      context)->BindAttribLocation(program, index, name);
}
void BindBuffer(PP_Resource context, GLenum target, GLuint buffer) {
  PluginGraphics3D::implFromResource(context)->BindBuffer(target, buffer);
}
void BindFramebuffer(PP_Resource context, GLenum target, GLuint framebuffer) {
  PluginGraphics3D::implFromResource(
      context)->BindFramebuffer(target, framebuffer);
}
void BindRenderbuffer(
    PP_Resource context, GLenum target, GLuint renderbuffer) {
  PluginGraphics3D::implFromResource(
      context)->BindRenderbuffer(target, renderbuffer);
}
void BindTexture(PP_Resource context, GLenum target, GLuint texture) {
  PluginGraphics3D::implFromResource(context)->BindTexture(target, texture);
}
void BlendColor(
    PP_Resource context, GLclampf red, GLclampf green, GLclampf blue,
    GLclampf alpha) {
  PluginGraphics3D::implFromResource(
      context)->BlendColor(red, green, blue, alpha);
}
void BlendEquation(PP_Resource context, GLenum mode) {
  PluginGraphics3D::implFromResource(context)->BlendEquation(mode);
}
void BlendEquationSeparate(
    PP_Resource context, GLenum modeRGB, GLenum modeAlpha) {
  PluginGraphics3D::implFromResource(
      context)->BlendEquationSeparate(modeRGB, modeAlpha);
}
void BlendFunc(PP_Resource context, GLenum sfactor, GLenum dfactor) {
  PluginGraphics3D::implFromResource(context)->BlendFunc(sfactor, dfactor);
}
void BlendFuncSeparate(
    PP_Resource context, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
    GLenum dstAlpha) {
  PluginGraphics3D::implFromResource(
      context)->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}
void BufferData(
    PP_Resource context, GLenum target, GLsizeiptr size, const void* data,
    GLenum usage) {
  PluginGraphics3D::implFromResource(
      context)->BufferData(target, size, data, usage);
}
void BufferSubData(
    PP_Resource context, GLenum target, GLintptr offset, GLsizeiptr size,
    const void* data) {
  PluginGraphics3D::implFromResource(
      context)->BufferSubData(target, offset, size, data);
}
GLenum CheckFramebufferStatus(PP_Resource context, GLenum target) {
  return PluginGraphics3D::implFromResource(
      context)->CheckFramebufferStatus(target);
}
void Clear(PP_Resource context, GLbitfield mask) {
  PluginGraphics3D::implFromResource(context)->Clear(mask);
}
void ClearColor(
    PP_Resource context, GLclampf red, GLclampf green, GLclampf blue,
    GLclampf alpha) {
  PluginGraphics3D::implFromResource(
      context)->ClearColor(red, green, blue, alpha);
}
void ClearDepthf(PP_Resource context, GLclampf depth) {
  PluginGraphics3D::implFromResource(context)->ClearDepthf(depth);
}
void ClearStencil(PP_Resource context, GLint s) {
  PluginGraphics3D::implFromResource(context)->ClearStencil(s);
}
void ColorMask(
    PP_Resource context, GLboolean red, GLboolean green, GLboolean blue,
    GLboolean alpha) {
  PluginGraphics3D::implFromResource(
      context)->ColorMask(red, green, blue, alpha);
}
void CompileShader(PP_Resource context, GLuint shader) {
  PluginGraphics3D::implFromResource(context)->CompileShader(shader);
}
void CompressedTexImage2D(
    PP_Resource context, GLenum target, GLint level, GLenum internalformat,
    GLsizei width, GLsizei height, GLint border, GLsizei imageSize,
    const void* data) {
  PluginGraphics3D::implFromResource(
      context)->CompressedTexImage2D(
          target, level, internalformat, width, height, border, imageSize,
          data);
}
void CompressedTexSubImage2D(
    PP_Resource context, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLsizei width, GLsizei height, GLenum format,
    GLsizei imageSize, const void* data) {
  PluginGraphics3D::implFromResource(
      context)->CompressedTexSubImage2D(
          target, level, xoffset, yoffset, width, height, format, imageSize,
          data);
}
void CopyTexImage2D(
    PP_Resource context, GLenum target, GLint level, GLenum internalformat,
    GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
  PluginGraphics3D::implFromResource(
      context)->CopyTexImage2D(
          target, level, internalformat, x, y, width, height, border);
}
void CopyTexSubImage2D(
    PP_Resource context, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
  PluginGraphics3D::implFromResource(
      context)->CopyTexSubImage2D(
          target, level, xoffset, yoffset, x, y, width, height);
}
GLuint CreateProgram(PP_Resource context) {
  return PluginGraphics3D::implFromResource(context)->CreateProgram();
}
GLuint CreateShader(PP_Resource context, GLenum type) {
  return PluginGraphics3D::implFromResource(context)->CreateShader(type);
}
void CullFace(PP_Resource context, GLenum mode) {
  PluginGraphics3D::implFromResource(context)->CullFace(mode);
}
void DeleteBuffers(PP_Resource context, GLsizei n, const GLuint* buffers) {
  PluginGraphics3D::implFromResource(context)->DeleteBuffers(n, buffers);
}
void DeleteFramebuffers(
    PP_Resource context, GLsizei n, const GLuint* framebuffers) {
  PluginGraphics3D::implFromResource(
      context)->DeleteFramebuffers(n, framebuffers);
}
void DeleteProgram(PP_Resource context, GLuint program) {
  PluginGraphics3D::implFromResource(context)->DeleteProgram(program);
}
void DeleteRenderbuffers(
    PP_Resource context, GLsizei n, const GLuint* renderbuffers) {
  PluginGraphics3D::implFromResource(
      context)->DeleteRenderbuffers(n, renderbuffers);
}
void DeleteShader(PP_Resource context, GLuint shader) {
  PluginGraphics3D::implFromResource(context)->DeleteShader(shader);
}
void DeleteTextures(PP_Resource context, GLsizei n, const GLuint* textures) {
  PluginGraphics3D::implFromResource(context)->DeleteTextures(n, textures);
}
void DepthFunc(PP_Resource context, GLenum func) {
  PluginGraphics3D::implFromResource(context)->DepthFunc(func);
}
void DepthMask(PP_Resource context, GLboolean flag) {
  PluginGraphics3D::implFromResource(context)->DepthMask(flag);
}
void DepthRangef(PP_Resource context, GLclampf zNear, GLclampf zFar) {
  PluginGraphics3D::implFromResource(context)->DepthRangef(zNear, zFar);
}
void DetachShader(PP_Resource context, GLuint program, GLuint shader) {
  PluginGraphics3D::implFromResource(context)->DetachShader(program, shader);
}
void Disable(PP_Resource context, GLenum cap) {
  PluginGraphics3D::implFromResource(context)->Disable(cap);
}
void DisableVertexAttribArray(PP_Resource context, GLuint index) {
  PluginGraphics3D::implFromResource(context)->DisableVertexAttribArray(index);
}
void DrawArrays(PP_Resource context, GLenum mode, GLint first, GLsizei count) {
  PluginGraphics3D::implFromResource(context)->DrawArrays(mode, first, count);
}
void DrawElements(
    PP_Resource context, GLenum mode, GLsizei count, GLenum type,
    const void* indices) {
  PluginGraphics3D::implFromResource(
      context)->DrawElements(mode, count, type, indices);
}
void Enable(PP_Resource context, GLenum cap) {
  PluginGraphics3D::implFromResource(context)->Enable(cap);
}
void EnableVertexAttribArray(PP_Resource context, GLuint index) {
  PluginGraphics3D::implFromResource(context)->EnableVertexAttribArray(index);
}
void Finish(PP_Resource context) {
  PluginGraphics3D::implFromResource(context)->Finish();
}
void Flush(PP_Resource context) {
  PluginGraphics3D::implFromResource(context)->Flush();
}
void FramebufferRenderbuffer(
    PP_Resource context, GLenum target, GLenum attachment,
    GLenum renderbuffertarget, GLuint renderbuffer) {
  PluginGraphics3D::implFromResource(
      context)->FramebufferRenderbuffer(
          target, attachment, renderbuffertarget, renderbuffer);
}
void FramebufferTexture2D(
    PP_Resource context, GLenum target, GLenum attachment, GLenum textarget,
    GLuint texture, GLint level) {
  PluginGraphics3D::implFromResource(
      context)->FramebufferTexture2D(
          target, attachment, textarget, texture, level);
}
void FrontFace(PP_Resource context, GLenum mode) {
  PluginGraphics3D::implFromResource(context)->FrontFace(mode);
}
void GenBuffers(PP_Resource context, GLsizei n, GLuint* buffers) {
  PluginGraphics3D::implFromResource(context)->GenBuffers(n, buffers);
}
void GenerateMipmap(PP_Resource context, GLenum target) {
  PluginGraphics3D::implFromResource(context)->GenerateMipmap(target);
}
void GenFramebuffers(PP_Resource context, GLsizei n, GLuint* framebuffers) {
  PluginGraphics3D::implFromResource(
      context)->GenFramebuffers(n, framebuffers);
}
void GenRenderbuffers(PP_Resource context, GLsizei n, GLuint* renderbuffers) {
  PluginGraphics3D::implFromResource(
      context)->GenRenderbuffers(n, renderbuffers);
}
void GenTextures(PP_Resource context, GLsizei n, GLuint* textures) {
  PluginGraphics3D::implFromResource(context)->GenTextures(n, textures);
}
void GetActiveAttrib(
    PP_Resource context, GLuint program, GLuint index, GLsizei bufsize,
    GLsizei* length, GLint* size, GLenum* type, char* name) {
  PluginGraphics3D::implFromResource(
      context)->GetActiveAttrib(
          program, index, bufsize, length, size, type, name);
}
void GetActiveUniform(
    PP_Resource context, GLuint program, GLuint index, GLsizei bufsize,
    GLsizei* length, GLint* size, GLenum* type, char* name) {
  PluginGraphics3D::implFromResource(
      context)->GetActiveUniform(
          program, index, bufsize, length, size, type, name);
}
void GetAttachedShaders(
    PP_Resource context, GLuint program, GLsizei maxcount, GLsizei* count,
    GLuint* shaders) {
  PluginGraphics3D::implFromResource(
      context)->GetAttachedShaders(program, maxcount, count, shaders);
}
GLint GetAttribLocation(
    PP_Resource context, GLuint program, const char* name) {
  return PluginGraphics3D::implFromResource(
      context)->GetAttribLocation(program, name);
}
void GetBooleanv(PP_Resource context, GLenum pname, GLboolean* params) {
  PluginGraphics3D::implFromResource(context)->GetBooleanv(pname, params);
}
void GetBufferParameteriv(
    PP_Resource context, GLenum target, GLenum pname, GLint* params) {
  PluginGraphics3D::implFromResource(
      context)->GetBufferParameteriv(target, pname, params);
}
GLenum GetError(PP_Resource context) {
  return PluginGraphics3D::implFromResource(context)->GetError();
}
void GetFloatv(PP_Resource context, GLenum pname, GLfloat* params) {
  PluginGraphics3D::implFromResource(context)->GetFloatv(pname, params);
}
void GetFramebufferAttachmentParameteriv(
    PP_Resource context, GLenum target, GLenum attachment, GLenum pname,
    GLint* params) {
  PluginGraphics3D::implFromResource(
      context)->GetFramebufferAttachmentParameteriv(
          target, attachment, pname, params);
}
void GetIntegerv(PP_Resource context, GLenum pname, GLint* params) {
  PluginGraphics3D::implFromResource(context)->GetIntegerv(pname, params);
}
void GetProgramiv(
    PP_Resource context, GLuint program, GLenum pname, GLint* params) {
  PluginGraphics3D::implFromResource(
      context)->GetProgramiv(program, pname, params);
}
void GetProgramInfoLog(
    PP_Resource context, GLuint program, GLsizei bufsize, GLsizei* length,
    char* infolog) {
  PluginGraphics3D::implFromResource(
      context)->GetProgramInfoLog(program, bufsize, length, infolog);
}
void GetRenderbufferParameteriv(
    PP_Resource context, GLenum target, GLenum pname, GLint* params) {
  PluginGraphics3D::implFromResource(
      context)->GetRenderbufferParameteriv(target, pname, params);
}
void GetShaderiv(
    PP_Resource context, GLuint shader, GLenum pname, GLint* params) {
  PluginGraphics3D::implFromResource(
      context)->GetShaderiv(shader, pname, params);
}
void GetShaderInfoLog(
    PP_Resource context, GLuint shader, GLsizei bufsize, GLsizei* length,
    char* infolog) {
  PluginGraphics3D::implFromResource(
      context)->GetShaderInfoLog(shader, bufsize, length, infolog);
}
void GetShaderPrecisionFormat(
    PP_Resource context, GLenum shadertype, GLenum precisiontype, GLint* range,
    GLint* precision) {
  PluginGraphics3D::implFromResource(
      context)->GetShaderPrecisionFormat(
          shadertype, precisiontype, range, precision);
}
void GetShaderSource(
    PP_Resource context, GLuint shader, GLsizei bufsize, GLsizei* length,
    char* source) {
  PluginGraphics3D::implFromResource(
      context)->GetShaderSource(shader, bufsize, length, source);
}
const GLubyte* GetString(PP_Resource context, GLenum name) {
  return PluginGraphics3D::implFromResource(context)->GetString(name);
}
void GetTexParameterfv(
    PP_Resource context, GLenum target, GLenum pname, GLfloat* params) {
  PluginGraphics3D::implFromResource(
      context)->GetTexParameterfv(target, pname, params);
}
void GetTexParameteriv(
    PP_Resource context, GLenum target, GLenum pname, GLint* params) {
  PluginGraphics3D::implFromResource(
      context)->GetTexParameteriv(target, pname, params);
}
void GetUniformfv(
    PP_Resource context, GLuint program, GLint location, GLfloat* params) {
  PluginGraphics3D::implFromResource(
      context)->GetUniformfv(program, location, params);
}
void GetUniformiv(
    PP_Resource context, GLuint program, GLint location, GLint* params) {
  PluginGraphics3D::implFromResource(
      context)->GetUniformiv(program, location, params);
}
GLint GetUniformLocation(
    PP_Resource context, GLuint program, const char* name) {
  return PluginGraphics3D::implFromResource(
      context)->GetUniformLocation(program, name);
}
void GetVertexAttribfv(
    PP_Resource context, GLuint index, GLenum pname, GLfloat* params) {
  PluginGraphics3D::implFromResource(
      context)->GetVertexAttribfv(index, pname, params);
}
void GetVertexAttribiv(
    PP_Resource context, GLuint index, GLenum pname, GLint* params) {
  PluginGraphics3D::implFromResource(
      context)->GetVertexAttribiv(index, pname, params);
}
void GetVertexAttribPointerv(
    PP_Resource context, GLuint index, GLenum pname, void** pointer) {
  PluginGraphics3D::implFromResource(
      context)->GetVertexAttribPointerv(index, pname, pointer);
}
void Hint(PP_Resource context, GLenum target, GLenum mode) {
  PluginGraphics3D::implFromResource(context)->Hint(target, mode);
}
GLboolean IsBuffer(PP_Resource context, GLuint buffer) {
  return PluginGraphics3D::implFromResource(context)->IsBuffer(buffer);
}
GLboolean IsEnabled(PP_Resource context, GLenum cap) {
  return PluginGraphics3D::implFromResource(context)->IsEnabled(cap);
}
GLboolean IsFramebuffer(PP_Resource context, GLuint framebuffer) {
  return PluginGraphics3D::implFromResource(
      context)->IsFramebuffer(framebuffer);
}
GLboolean IsProgram(PP_Resource context, GLuint program) {
  return PluginGraphics3D::implFromResource(context)->IsProgram(program);
}
GLboolean IsRenderbuffer(PP_Resource context, GLuint renderbuffer) {
  return PluginGraphics3D::implFromResource(
      context)->IsRenderbuffer(renderbuffer);
}
GLboolean IsShader(PP_Resource context, GLuint shader) {
  return PluginGraphics3D::implFromResource(context)->IsShader(shader);
}
GLboolean IsTexture(PP_Resource context, GLuint texture) {
  return PluginGraphics3D::implFromResource(context)->IsTexture(texture);
}
void LineWidth(PP_Resource context, GLfloat width) {
  PluginGraphics3D::implFromResource(context)->LineWidth(width);
}
void LinkProgram(PP_Resource context, GLuint program) {
  PluginGraphics3D::implFromResource(context)->LinkProgram(program);
}
void PixelStorei(PP_Resource context, GLenum pname, GLint param) {
  PluginGraphics3D::implFromResource(context)->PixelStorei(pname, param);
}
void PolygonOffset(PP_Resource context, GLfloat factor, GLfloat units) {
  PluginGraphics3D::implFromResource(context)->PolygonOffset(factor, units);
}
void ReadPixels(
    PP_Resource context, GLint x, GLint y, GLsizei width, GLsizei height,
    GLenum format, GLenum type, void* pixels) {
  PluginGraphics3D::implFromResource(
      context)->ReadPixels(x, y, width, height, format, type, pixels);
}
void ReleaseShaderCompiler(PP_Resource context) {
  PluginGraphics3D::implFromResource(context)->ReleaseShaderCompiler();
}
void RenderbufferStorage(
    PP_Resource context, GLenum target, GLenum internalformat, GLsizei width,
    GLsizei height) {
  PluginGraphics3D::implFromResource(
      context)->RenderbufferStorage(target, internalformat, width, height);
}
void SampleCoverage(PP_Resource context, GLclampf value, GLboolean invert) {
  PluginGraphics3D::implFromResource(context)->SampleCoverage(value, invert);
}
void Scissor(
    PP_Resource context, GLint x, GLint y, GLsizei width, GLsizei height) {
  PluginGraphics3D::implFromResource(context)->Scissor(x, y, width, height);
}
void ShaderBinary(
    PP_Resource context, GLsizei n, const GLuint* shaders, GLenum binaryformat,
    const void* binary, GLsizei length) {
  PluginGraphics3D::implFromResource(
      context)->ShaderBinary(n, shaders, binaryformat, binary, length);
}
void ShaderSource(
    PP_Resource context, GLuint shader, GLsizei count, const char** str,
    const GLint* length) {
  PluginGraphics3D::implFromResource(
      context)->ShaderSource(shader, count, str, length);
}
void StencilFunc(PP_Resource context, GLenum func, GLint ref, GLuint mask) {
  PluginGraphics3D::implFromResource(context)->StencilFunc(func, ref, mask);
}
void StencilFuncSeparate(
    PP_Resource context, GLenum face, GLenum func, GLint ref, GLuint mask) {
  PluginGraphics3D::implFromResource(
      context)->StencilFuncSeparate(face, func, ref, mask);
}
void StencilMask(PP_Resource context, GLuint mask) {
  PluginGraphics3D::implFromResource(context)->StencilMask(mask);
}
void StencilMaskSeparate(PP_Resource context, GLenum face, GLuint mask) {
  PluginGraphics3D::implFromResource(context)->StencilMaskSeparate(face, mask);
}
void StencilOp(PP_Resource context, GLenum fail, GLenum zfail, GLenum zpass) {
  PluginGraphics3D::implFromResource(context)->StencilOp(fail, zfail, zpass);
}
void StencilOpSeparate(
    PP_Resource context, GLenum face, GLenum fail, GLenum zfail,
    GLenum zpass) {
  PluginGraphics3D::implFromResource(
      context)->StencilOpSeparate(face, fail, zfail, zpass);
}
void TexImage2D(
    PP_Resource context, GLenum target, GLint level, GLint internalformat,
    GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  PluginGraphics3D::implFromResource(
      context)->TexImage2D(
          target, level, internalformat, width, height, border, format, type,
          pixels);
}
void TexParameterf(
    PP_Resource context, GLenum target, GLenum pname, GLfloat param) {
  PluginGraphics3D::implFromResource(
      context)->TexParameterf(target, pname, param);
}
void TexParameterfv(
    PP_Resource context, GLenum target, GLenum pname, const GLfloat* params) {
  PluginGraphics3D::implFromResource(
      context)->TexParameterfv(target, pname, params);
}
void TexParameteri(
    PP_Resource context, GLenum target, GLenum pname, GLint param) {
  PluginGraphics3D::implFromResource(
      context)->TexParameteri(target, pname, param);
}
void TexParameteriv(
    PP_Resource context, GLenum target, GLenum pname, const GLint* params) {
  PluginGraphics3D::implFromResource(
      context)->TexParameteriv(target, pname, params);
}
void TexSubImage2D(
    PP_Resource context, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
    const void* pixels) {
  PluginGraphics3D::implFromResource(
      context)->TexSubImage2D(
          target, level, xoffset, yoffset, width, height, format, type,
          pixels);
}
void Uniform1f(PP_Resource context, GLint location, GLfloat x) {
  PluginGraphics3D::implFromResource(context)->Uniform1f(location, x);
}
void Uniform1fv(
    PP_Resource context, GLint location, GLsizei count, const GLfloat* v) {
  PluginGraphics3D::implFromResource(context)->Uniform1fv(location, count, v);
}
void Uniform1i(PP_Resource context, GLint location, GLint x) {
  PluginGraphics3D::implFromResource(context)->Uniform1i(location, x);
}
void Uniform1iv(
    PP_Resource context, GLint location, GLsizei count, const GLint* v) {
  PluginGraphics3D::implFromResource(context)->Uniform1iv(location, count, v);
}
void Uniform2f(PP_Resource context, GLint location, GLfloat x, GLfloat y) {
  PluginGraphics3D::implFromResource(context)->Uniform2f(location, x, y);
}
void Uniform2fv(
    PP_Resource context, GLint location, GLsizei count, const GLfloat* v) {
  PluginGraphics3D::implFromResource(context)->Uniform2fv(location, count, v);
}
void Uniform2i(PP_Resource context, GLint location, GLint x, GLint y) {
  PluginGraphics3D::implFromResource(context)->Uniform2i(location, x, y);
}
void Uniform2iv(
    PP_Resource context, GLint location, GLsizei count, const GLint* v) {
  PluginGraphics3D::implFromResource(context)->Uniform2iv(location, count, v);
}
void Uniform3f(
    PP_Resource context, GLint location, GLfloat x, GLfloat y, GLfloat z) {
  PluginGraphics3D::implFromResource(context)->Uniform3f(location, x, y, z);
}
void Uniform3fv(
    PP_Resource context, GLint location, GLsizei count, const GLfloat* v) {
  PluginGraphics3D::implFromResource(context)->Uniform3fv(location, count, v);
}
void Uniform3i(
    PP_Resource context, GLint location, GLint x, GLint y, GLint z) {
  PluginGraphics3D::implFromResource(context)->Uniform3i(location, x, y, z);
}
void Uniform3iv(
    PP_Resource context, GLint location, GLsizei count, const GLint* v) {
  PluginGraphics3D::implFromResource(context)->Uniform3iv(location, count, v);
}
void Uniform4f(
    PP_Resource context, GLint location, GLfloat x, GLfloat y, GLfloat z,
    GLfloat w) {
  PluginGraphics3D::implFromResource(context)->Uniform4f(location, x, y, z, w);
}
void Uniform4fv(
    PP_Resource context, GLint location, GLsizei count, const GLfloat* v) {
  PluginGraphics3D::implFromResource(context)->Uniform4fv(location, count, v);
}
void Uniform4i(
    PP_Resource context, GLint location, GLint x, GLint y, GLint z, GLint w) {
  PluginGraphics3D::implFromResource(context)->Uniform4i(location, x, y, z, w);
}
void Uniform4iv(
    PP_Resource context, GLint location, GLsizei count, const GLint* v) {
  PluginGraphics3D::implFromResource(context)->Uniform4iv(location, count, v);
}
void UniformMatrix2fv(
    PP_Resource context, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  PluginGraphics3D::implFromResource(
      context)->UniformMatrix2fv(location, count, transpose, value);
}
void UniformMatrix3fv(
    PP_Resource context, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  PluginGraphics3D::implFromResource(
      context)->UniformMatrix3fv(location, count, transpose, value);
}
void UniformMatrix4fv(
    PP_Resource context, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  PluginGraphics3D::implFromResource(
      context)->UniformMatrix4fv(location, count, transpose, value);
}
void UseProgram(PP_Resource context, GLuint program) {
  PluginGraphics3D::implFromResource(context)->UseProgram(program);
}
void ValidateProgram(PP_Resource context, GLuint program) {
  PluginGraphics3D::implFromResource(context)->ValidateProgram(program);
}
void VertexAttrib1f(PP_Resource context, GLuint indx, GLfloat x) {
  PluginGraphics3D::implFromResource(context)->VertexAttrib1f(indx, x);
}
void VertexAttrib1fv(PP_Resource context, GLuint indx, const GLfloat* values) {
  PluginGraphics3D::implFromResource(context)->VertexAttrib1fv(indx, values);
}
void VertexAttrib2f(PP_Resource context, GLuint indx, GLfloat x, GLfloat y) {
  PluginGraphics3D::implFromResource(context)->VertexAttrib2f(indx, x, y);
}
void VertexAttrib2fv(PP_Resource context, GLuint indx, const GLfloat* values) {
  PluginGraphics3D::implFromResource(context)->VertexAttrib2fv(indx, values);
}
void VertexAttrib3f(
    PP_Resource context, GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  PluginGraphics3D::implFromResource(context)->VertexAttrib3f(indx, x, y, z);
}
void VertexAttrib3fv(PP_Resource context, GLuint indx, const GLfloat* values) {
  PluginGraphics3D::implFromResource(context)->VertexAttrib3fv(indx, values);
}
void VertexAttrib4f(
    PP_Resource context, GLuint indx, GLfloat x, GLfloat y, GLfloat z,
    GLfloat w) {
  PluginGraphics3D::implFromResource(
      context)->VertexAttrib4f(indx, x, y, z, w);
}
void VertexAttrib4fv(PP_Resource context, GLuint indx, const GLfloat* values) {
  PluginGraphics3D::implFromResource(context)->VertexAttrib4fv(indx, values);
}
void VertexAttribPointer(
    PP_Resource context, GLuint indx, GLint size, GLenum type,
    GLboolean normalized, GLsizei stride, const void* ptr) {
  PluginGraphics3D::implFromResource(
      context)->VertexAttribPointer(indx, size, type, normalized, stride, ptr);
}
void Viewport(
    PP_Resource context, GLint x, GLint y, GLsizei width, GLsizei height) {
  PluginGraphics3D::implFromResource(context)->Viewport(x, y, width, height);
}
void BlitFramebufferEXT(
    PP_Resource context, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
    GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask,
    GLenum filter) {
  PluginGraphics3D::implFromResource(
      context)->BlitFramebufferEXT(
          srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask,
          filter);
}
void RenderbufferStorageMultisampleEXT(
    PP_Resource context, GLenum target, GLsizei samples, GLenum internalformat,
    GLsizei width, GLsizei height) {
  PluginGraphics3D::implFromResource(
      context)->RenderbufferStorageMultisampleEXT(
          target, samples, internalformat, width, height);
}
void GenQueriesEXT(PP_Resource context, GLsizei n, GLuint* queries) {
  PluginGraphics3D::implFromResource(context)->GenQueriesEXT(n, queries);
}
void DeleteQueriesEXT(PP_Resource context, GLsizei n, const GLuint* queries) {
  PluginGraphics3D::implFromResource(context)->DeleteQueriesEXT(n, queries);
}
GLboolean IsQueryEXT(PP_Resource context, GLuint id) {
  return PluginGraphics3D::implFromResource(context)->IsQueryEXT(id);
}
void BeginQueryEXT(PP_Resource context, GLenum target, GLuint id) {
  PluginGraphics3D::implFromResource(context)->BeginQueryEXT(target, id);
}
void EndQueryEXT(PP_Resource context, GLenum target) {
  PluginGraphics3D::implFromResource(context)->EndQueryEXT(target);
}
void GetQueryivEXT(
    PP_Resource context, GLenum target, GLenum pname, GLint* params) {
  PluginGraphics3D::implFromResource(
      context)->GetQueryivEXT(target, pname, params);
}
void GetQueryObjectuivEXT(
    PP_Resource context, GLuint id, GLenum pname, GLuint* params) {
  PluginGraphics3D::implFromResource(
      context)->GetQueryObjectuivEXT(id, pname, params);
}
GLboolean EnableFeatureCHROMIUM(PP_Resource context, const char* feature) {
  return PluginGraphics3D::implFromResource(
      context)->EnableFeatureCHROMIUM(feature);
}
void* MapBufferSubDataCHROMIUM(
    PP_Resource context, GLuint target, GLintptr offset, GLsizeiptr size,
    GLenum access) {
  return PluginGraphics3D::implFromResource(
      context)->MapBufferSubDataCHROMIUM(target, offset, size, access);
}
void UnmapBufferSubDataCHROMIUM(PP_Resource context, const void* mem) {
  PluginGraphics3D::implFromResource(context)->UnmapBufferSubDataCHROMIUM(mem);
}
void* MapTexSubImage2DCHROMIUM(
    PP_Resource context, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
    GLenum access) {
  return PluginGraphics3D::implFromResource(
      context)->MapTexSubImage2DCHROMIUM(
          target, level, xoffset, yoffset, width, height, format, type,
          access);
}
void UnmapTexSubImage2DCHROMIUM(PP_Resource context, const void* mem) {
  PluginGraphics3D::implFromResource(context)->UnmapTexSubImage2DCHROMIUM(mem);
}
void DrawArraysInstancedANGLE(
    PP_Resource context, GLenum mode, GLint first, GLsizei count,
    GLsizei primcount) {
  PluginGraphics3D::implFromResource(
      context)->DrawArraysInstancedANGLE(mode, first, count, primcount);
}
void DrawElementsInstancedANGLE(
    PP_Resource context, GLenum mode, GLsizei count, GLenum type,
    const void* indices, GLsizei primcount) {
  PluginGraphics3D::implFromResource(
      context)->DrawElementsInstancedANGLE(
          mode, count, type, indices, primcount);
}
void VertexAttribDivisorANGLE(
    PP_Resource context, GLuint index, GLuint divisor) {
  PluginGraphics3D::implFromResource(
      context)->VertexAttribDivisorANGLE(index, divisor);
}

} // namespace

const PPB_OpenGLES2* PluginGraphics3D::GetOpenGLESInterface() {
  const static struct PPB_OpenGLES2 ppb_opengles = {
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
    &Viewport
  };
  return &ppb_opengles;
}
const PPB_OpenGLES2InstancedArrays* PluginGraphics3D::GetOpenGLESInstancedArraysInterface() {  // NOLINT
  const static struct PPB_OpenGLES2InstancedArrays ppb_opengles = {
    &DrawArraysInstancedANGLE,
    &DrawElementsInstancedANGLE,
    &VertexAttribDivisorANGLE
  };
  return &ppb_opengles;
}
const PPB_OpenGLES2FramebufferBlit* PluginGraphics3D::GetOpenGLESFramebufferBlitInterface() {  // NOLINT
  const static struct PPB_OpenGLES2FramebufferBlit ppb_opengles = {
    &BlitFramebufferEXT
  };
  return &ppb_opengles;
}
const PPB_OpenGLES2FramebufferMultisample* PluginGraphics3D::GetOpenGLESFramebufferMultisampleInterface() {  // NOLINT
  const static struct PPB_OpenGLES2FramebufferMultisample ppb_opengles = {
    &RenderbufferStorageMultisampleEXT
  };
  return &ppb_opengles;
}
const PPB_OpenGLES2ChromiumEnableFeature* PluginGraphics3D::GetOpenGLESChromiumEnableFeatureInterface() {  // NOLINT
  const static struct PPB_OpenGLES2ChromiumEnableFeature ppb_opengles = {
    &EnableFeatureCHROMIUM
  };
  return &ppb_opengles;
}
const PPB_OpenGLES2ChromiumMapSub* PluginGraphics3D::GetOpenGLESChromiumMapSubInterface() {  // NOLINT
  const static struct PPB_OpenGLES2ChromiumMapSub ppb_opengles = {
    &MapBufferSubDataCHROMIUM,
    &UnmapBufferSubDataCHROMIUM,
    &MapTexSubImage2DCHROMIUM,
    &UnmapTexSubImage2DCHROMIUM
  };
  return &ppb_opengles;
}
const PPB_OpenGLES2Query* PluginGraphics3D::GetOpenGLESQueryInterface() {
  const static struct PPB_OpenGLES2Query ppb_opengles = {
    &GenQueriesEXT,
    &DeleteQueriesEXT,
    &IsQueryEXT,
    &BeginQueryEXT,
    &EndQueryEXT,
    &GetQueryivEXT,
    &GetQueryObjectuivEXT
  };
  return &ppb_opengles;
}
