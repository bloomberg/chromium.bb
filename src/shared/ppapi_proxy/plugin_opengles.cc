// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated. DO NOT EDIT!

#include "native_client/src/shared/ppapi_proxy/plugin_context_3d.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/dev/ppb_opengles_dev.h"

using ppapi_proxy::PluginContext3D;
using ppapi_proxy::PluginResource;

namespace {

void ActiveTexture(PP_Resource context, GLenum texture) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->ActiveTexture(texture);
}
void AttachShader(PP_Resource context, GLuint program, GLuint shader) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->AttachShader(program, shader);
}
void BindAttribLocation(
    PP_Resource context, GLuint program, GLuint index, const char* name) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->BindAttribLocation(program, index, name);
}
void BindBuffer(PP_Resource context, GLenum target, GLuint buffer) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->BindBuffer(target, buffer);
}
void BindFramebuffer(PP_Resource context, GLenum target, GLuint framebuffer) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->BindFramebuffer(target, framebuffer);
}
void BindRenderbuffer(
    PP_Resource context, GLenum target, GLuint renderbuffer) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->BindRenderbuffer(target, renderbuffer);
}
void BindTexture(PP_Resource context, GLenum target, GLuint texture) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->BindTexture(target, texture);
}
void BlendColor(
    PP_Resource context, GLclampf red, GLclampf green, GLclampf blue,
    GLclampf alpha) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->BlendColor(red, green, blue, alpha);
}
void BlendEquation(PP_Resource context, GLenum mode) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->BlendEquation(mode);
}
void BlendEquationSeparate(
    PP_Resource context, GLenum modeRGB, GLenum modeAlpha) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->BlendEquationSeparate(modeRGB, modeAlpha);
}
void BlendFunc(PP_Resource context, GLenum sfactor, GLenum dfactor) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->BlendFunc(sfactor, dfactor);
}
void BlendFuncSeparate(
    PP_Resource context, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
    GLenum dstAlpha) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}
void BufferData(
    PP_Resource context, GLenum target, GLsizeiptr size, const void* data,
    GLenum usage) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->BufferData(target, size, data, usage);
}
void BufferSubData(
    PP_Resource context, GLenum target, GLintptr offset, GLsizeiptr size,
    const void* data) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->BufferSubData(target, offset, size, data);
}
GLenum CheckFramebufferStatus(PP_Resource context, GLenum target) {
  return PluginResource::GetAs<PluginContext3D>(
      context)->impl()->CheckFramebufferStatus(target);
}
void Clear(PP_Resource context, GLbitfield mask) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->Clear(mask);
}
void ClearColor(
    PP_Resource context, GLclampf red, GLclampf green, GLclampf blue,
    GLclampf alpha) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->ClearColor(red, green, blue, alpha);
}
void ClearDepthf(PP_Resource context, GLclampf depth) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->ClearDepthf(depth);
}
void ClearStencil(PP_Resource context, GLint s) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->ClearStencil(s);
}
void ColorMask(
    PP_Resource context, GLboolean red, GLboolean green, GLboolean blue,
    GLboolean alpha) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->ColorMask(red, green, blue, alpha);
}
void CompileShader(PP_Resource context, GLuint shader) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->CompileShader(shader);
}
void CompressedTexImage2D(
    PP_Resource context, GLenum target, GLint level, GLenum internalformat,
    GLsizei width, GLsizei height, GLint border, GLsizei imageSize,
    const void* data) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->CompressedTexImage2D(
          target, level, internalformat, width, height, border, imageSize,
          data);
}
void CompressedTexSubImage2D(
    PP_Resource context, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLsizei width, GLsizei height, GLenum format,
    GLsizei imageSize, const void* data) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->CompressedTexSubImage2D(
          target, level, xoffset, yoffset, width, height, format, imageSize,
          data);
}
void CopyTexImage2D(
    PP_Resource context, GLenum target, GLint level, GLenum internalformat,
    GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->CopyTexImage2D(
          target, level, internalformat, x, y, width, height, border);
}
void CopyTexSubImage2D(
    PP_Resource context, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->CopyTexSubImage2D(
          target, level, xoffset, yoffset, x, y, width, height);
}
GLuint CreateProgram(PP_Resource context) {
  return PluginResource::GetAs<PluginContext3D>(
      context)->impl()->CreateProgram();
}
GLuint CreateShader(PP_Resource context, GLenum type) {
  return PluginResource::GetAs<PluginContext3D>(
      context)->impl()->CreateShader(type);
}
void CullFace(PP_Resource context, GLenum mode) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->CullFace(mode);
}
void DeleteBuffers(PP_Resource context, GLsizei n, const GLuint* buffers) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->DeleteBuffers(n, buffers);
}
void DeleteFramebuffers(
    PP_Resource context, GLsizei n, const GLuint* framebuffers) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->DeleteFramebuffers(n, framebuffers);
}
void DeleteProgram(PP_Resource context, GLuint program) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->DeleteProgram(program);
}
void DeleteRenderbuffers(
    PP_Resource context, GLsizei n, const GLuint* renderbuffers) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->DeleteRenderbuffers(n, renderbuffers);
}
void DeleteShader(PP_Resource context, GLuint shader) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->DeleteShader(shader);
}
void DeleteTextures(PP_Resource context, GLsizei n, const GLuint* textures) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->DeleteTextures(n, textures);
}
void DepthFunc(PP_Resource context, GLenum func) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->DepthFunc(func);
}
void DepthMask(PP_Resource context, GLboolean flag) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->DepthMask(flag);
}
void DepthRangef(PP_Resource context, GLclampf zNear, GLclampf zFar) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->DepthRangef(zNear, zFar);
}
void DetachShader(PP_Resource context, GLuint program, GLuint shader) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->DetachShader(program, shader);
}
void Disable(PP_Resource context, GLenum cap) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->Disable(cap);
}
void DisableVertexAttribArray(PP_Resource context, GLuint index) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->DisableVertexAttribArray(index);
}
void DrawArrays(PP_Resource context, GLenum mode, GLint first, GLsizei count) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->DrawArrays(mode, first, count);
}
void DrawElements(
    PP_Resource context, GLenum mode, GLsizei count, GLenum type,
    const void* indices) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->DrawElements(mode, count, type, indices);
}
void Enable(PP_Resource context, GLenum cap) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->Enable(cap);
}
void EnableVertexAttribArray(PP_Resource context, GLuint index) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->EnableVertexAttribArray(index);
}
void Finish(PP_Resource context) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->Finish();
}
void Flush(PP_Resource context) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->Flush();
}
void FramebufferRenderbuffer(
    PP_Resource context, GLenum target, GLenum attachment,
    GLenum renderbuffertarget, GLuint renderbuffer) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->FramebufferRenderbuffer(
          target, attachment, renderbuffertarget, renderbuffer);
}
void FramebufferTexture2D(
    PP_Resource context, GLenum target, GLenum attachment, GLenum textarget,
    GLuint texture, GLint level) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->FramebufferTexture2D(
          target, attachment, textarget, texture, level);
}
void FrontFace(PP_Resource context, GLenum mode) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->FrontFace(mode);
}
void GenBuffers(PP_Resource context, GLsizei n, GLuint* buffers) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GenBuffers(n, buffers);
}
void GenerateMipmap(PP_Resource context, GLenum target) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GenerateMipmap(target);
}
void GenFramebuffers(PP_Resource context, GLsizei n, GLuint* framebuffers) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GenFramebuffers(n, framebuffers);
}
void GenRenderbuffers(PP_Resource context, GLsizei n, GLuint* renderbuffers) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GenRenderbuffers(n, renderbuffers);
}
void GenTextures(PP_Resource context, GLsizei n, GLuint* textures) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GenTextures(n, textures);
}
void GetActiveAttrib(
    PP_Resource context, GLuint program, GLuint index, GLsizei bufsize,
    GLsizei* length, GLint* size, GLenum* type, char* name) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetActiveAttrib(
          program, index, bufsize, length, size, type, name);
}
void GetActiveUniform(
    PP_Resource context, GLuint program, GLuint index, GLsizei bufsize,
    GLsizei* length, GLint* size, GLenum* type, char* name) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetActiveUniform(
          program, index, bufsize, length, size, type, name);
}
void GetAttachedShaders(
    PP_Resource context, GLuint program, GLsizei maxcount, GLsizei* count,
    GLuint* shaders) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetAttachedShaders(program, maxcount, count, shaders);
}
GLint GetAttribLocation(
    PP_Resource context, GLuint program, const char* name) {
  return PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetAttribLocation(program, name);
}
void GetBooleanv(PP_Resource context, GLenum pname, GLboolean* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetBooleanv(pname, params);
}
void GetBufferParameteriv(
    PP_Resource context, GLenum target, GLenum pname, GLint* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetBufferParameteriv(target, pname, params);
}
GLenum GetError(PP_Resource context) {
  return PluginResource::GetAs<PluginContext3D>(context)->impl()->GetError();
}
void GetFloatv(PP_Resource context, GLenum pname, GLfloat* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetFloatv(pname, params);
}
void GetFramebufferAttachmentParameteriv(
    PP_Resource context, GLenum target, GLenum attachment, GLenum pname,
    GLint* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetFramebufferAttachmentParameteriv(
          target, attachment, pname, params);
}
void GetIntegerv(PP_Resource context, GLenum pname, GLint* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetIntegerv(pname, params);
}
void GetProgramiv(
    PP_Resource context, GLuint program, GLenum pname, GLint* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetProgramiv(program, pname, params);
}
void GetProgramInfoLog(
    PP_Resource context, GLuint program, GLsizei bufsize, GLsizei* length,
    char* infolog) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetProgramInfoLog(program, bufsize, length, infolog);
}
void GetRenderbufferParameteriv(
    PP_Resource context, GLenum target, GLenum pname, GLint* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetRenderbufferParameteriv(target, pname, params);
}
void GetShaderiv(
    PP_Resource context, GLuint shader, GLenum pname, GLint* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetShaderiv(shader, pname, params);
}
void GetShaderInfoLog(
    PP_Resource context, GLuint shader, GLsizei bufsize, GLsizei* length,
    char* infolog) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetShaderInfoLog(shader, bufsize, length, infolog);
}
void GetShaderPrecisionFormat(
    PP_Resource context, GLenum shadertype, GLenum precisiontype, GLint* range,
    GLint* precision) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetShaderPrecisionFormat(
          shadertype, precisiontype, range, precision);
}
void GetShaderSource(
    PP_Resource context, GLuint shader, GLsizei bufsize, GLsizei* length,
    char* source) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetShaderSource(shader, bufsize, length, source);
}
const GLubyte* GetString(PP_Resource context, GLenum name) {
  return PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetString(name);
}
void GetTexParameterfv(
    PP_Resource context, GLenum target, GLenum pname, GLfloat* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetTexParameterfv(target, pname, params);
}
void GetTexParameteriv(
    PP_Resource context, GLenum target, GLenum pname, GLint* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetTexParameteriv(target, pname, params);
}
void GetUniformfv(
    PP_Resource context, GLuint program, GLint location, GLfloat* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetUniformfv(program, location, params);
}
void GetUniformiv(
    PP_Resource context, GLuint program, GLint location, GLint* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetUniformiv(program, location, params);
}
GLint GetUniformLocation(
    PP_Resource context, GLuint program, const char* name) {
  return PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetUniformLocation(program, name);
}
void GetVertexAttribfv(
    PP_Resource context, GLuint index, GLenum pname, GLfloat* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetVertexAttribfv(index, pname, params);
}
void GetVertexAttribiv(
    PP_Resource context, GLuint index, GLenum pname, GLint* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetVertexAttribiv(index, pname, params);
}
void GetVertexAttribPointerv(
    PP_Resource context, GLuint index, GLenum pname, void** pointer) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->GetVertexAttribPointerv(index, pname, pointer);
}
void Hint(PP_Resource context, GLenum target, GLenum mode) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->Hint(target, mode);
}
GLboolean IsBuffer(PP_Resource context, GLuint buffer) {
  return PluginResource::GetAs<PluginContext3D>(
      context)->impl()->IsBuffer(buffer);
}
GLboolean IsEnabled(PP_Resource context, GLenum cap) {
  return PluginResource::GetAs<PluginContext3D>(
      context)->impl()->IsEnabled(cap);
}
GLboolean IsFramebuffer(PP_Resource context, GLuint framebuffer) {
  return PluginResource::GetAs<PluginContext3D>(
      context)->impl()->IsFramebuffer(framebuffer);
}
GLboolean IsProgram(PP_Resource context, GLuint program) {
  return PluginResource::GetAs<PluginContext3D>(
      context)->impl()->IsProgram(program);
}
GLboolean IsRenderbuffer(PP_Resource context, GLuint renderbuffer) {
  return PluginResource::GetAs<PluginContext3D>(
      context)->impl()->IsRenderbuffer(renderbuffer);
}
GLboolean IsShader(PP_Resource context, GLuint shader) {
  return PluginResource::GetAs<PluginContext3D>(
      context)->impl()->IsShader(shader);
}
GLboolean IsTexture(PP_Resource context, GLuint texture) {
  return PluginResource::GetAs<PluginContext3D>(
      context)->impl()->IsTexture(texture);
}
void LineWidth(PP_Resource context, GLfloat width) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->LineWidth(width);
}
void LinkProgram(PP_Resource context, GLuint program) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->LinkProgram(program);
}
void PixelStorei(PP_Resource context, GLenum pname, GLint param) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->PixelStorei(pname, param);
}
void PolygonOffset(PP_Resource context, GLfloat factor, GLfloat units) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->PolygonOffset(factor, units);
}
void ReadPixels(
    PP_Resource context, GLint x, GLint y, GLsizei width, GLsizei height,
    GLenum format, GLenum type, void* pixels) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->ReadPixels(x, y, width, height, format, type, pixels);
}
void ReleaseShaderCompiler(PP_Resource context) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->ReleaseShaderCompiler();
}
void RenderbufferStorage(
    PP_Resource context, GLenum target, GLenum internalformat, GLsizei width,
    GLsizei height) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->RenderbufferStorage(
          target, internalformat, width, height);
}
void SampleCoverage(PP_Resource context, GLclampf value, GLboolean invert) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->SampleCoverage(value, invert);
}
void Scissor(
    PP_Resource context, GLint x, GLint y, GLsizei width, GLsizei height) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Scissor(x, y, width, height);
}
void ShaderBinary(
    PP_Resource context, GLsizei n, const GLuint* shaders, GLenum binaryformat,
    const void* binary, GLsizei length) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->ShaderBinary(n, shaders, binaryformat, binary, length);
}
void ShaderSource(
    PP_Resource context, GLuint shader, GLsizei count, const char** str,
    const GLint* length) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->ShaderSource(shader, count, str, length);
}
void StencilFunc(PP_Resource context, GLenum func, GLint ref, GLuint mask) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->StencilFunc(func, ref, mask);
}
void StencilFuncSeparate(
    PP_Resource context, GLenum face, GLenum func, GLint ref, GLuint mask) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->StencilFuncSeparate(face, func, ref, mask);
}
void StencilMask(PP_Resource context, GLuint mask) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->StencilMask(mask);
}
void StencilMaskSeparate(PP_Resource context, GLenum face, GLuint mask) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->StencilMaskSeparate(face, mask);
}
void StencilOp(PP_Resource context, GLenum fail, GLenum zfail, GLenum zpass) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->StencilOp(fail, zfail, zpass);
}
void StencilOpSeparate(
    PP_Resource context, GLenum face, GLenum fail, GLenum zfail,
    GLenum zpass) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->StencilOpSeparate(face, fail, zfail, zpass);
}
void TexImage2D(
    PP_Resource context, GLenum target, GLint level, GLint internalformat,
    GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->TexImage2D(
          target, level, internalformat, width, height, border, format, type,
          pixels);
}
void TexParameterf(
    PP_Resource context, GLenum target, GLenum pname, GLfloat param) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->TexParameterf(target, pname, param);
}
void TexParameterfv(
    PP_Resource context, GLenum target, GLenum pname, const GLfloat* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->TexParameterfv(target, pname, params);
}
void TexParameteri(
    PP_Resource context, GLenum target, GLenum pname, GLint param) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->TexParameteri(target, pname, param);
}
void TexParameteriv(
    PP_Resource context, GLenum target, GLenum pname, const GLint* params) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->TexParameteriv(target, pname, params);
}
void TexSubImage2D(
    PP_Resource context, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
    const void* pixels) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->TexSubImage2D(
          target, level, xoffset, yoffset, width, height, format, type,
          pixels);
}
void Uniform1f(PP_Resource context, GLint location, GLfloat x) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform1f(location, x);
}
void Uniform1fv(
    PP_Resource context, GLint location, GLsizei count, const GLfloat* v) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform1fv(location, count, v);
}
void Uniform1i(PP_Resource context, GLint location, GLint x) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform1i(location, x);
}
void Uniform1iv(
    PP_Resource context, GLint location, GLsizei count, const GLint* v) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform1iv(location, count, v);
}
void Uniform2f(PP_Resource context, GLint location, GLfloat x, GLfloat y) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform2f(location, x, y);
}
void Uniform2fv(
    PP_Resource context, GLint location, GLsizei count, const GLfloat* v) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform2fv(location, count, v);
}
void Uniform2i(PP_Resource context, GLint location, GLint x, GLint y) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform2i(location, x, y);
}
void Uniform2iv(
    PP_Resource context, GLint location, GLsizei count, const GLint* v) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform2iv(location, count, v);
}
void Uniform3f(
    PP_Resource context, GLint location, GLfloat x, GLfloat y, GLfloat z) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform3f(location, x, y, z);
}
void Uniform3fv(
    PP_Resource context, GLint location, GLsizei count, const GLfloat* v) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform3fv(location, count, v);
}
void Uniform3i(
    PP_Resource context, GLint location, GLint x, GLint y, GLint z) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform3i(location, x, y, z);
}
void Uniform3iv(
    PP_Resource context, GLint location, GLsizei count, const GLint* v) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform3iv(location, count, v);
}
void Uniform4f(
    PP_Resource context, GLint location, GLfloat x, GLfloat y, GLfloat z,
    GLfloat w) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform4f(location, x, y, z, w);
}
void Uniform4fv(
    PP_Resource context, GLint location, GLsizei count, const GLfloat* v) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform4fv(location, count, v);
}
void Uniform4i(
    PP_Resource context, GLint location, GLint x, GLint y, GLint z, GLint w) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform4i(location, x, y, z, w);
}
void Uniform4iv(
    PP_Resource context, GLint location, GLsizei count, const GLint* v) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Uniform4iv(location, count, v);
}
void UniformMatrix2fv(
    PP_Resource context, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->UniformMatrix2fv(location, count, transpose, value);
}
void UniformMatrix3fv(
    PP_Resource context, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->UniformMatrix3fv(location, count, transpose, value);
}
void UniformMatrix4fv(
    PP_Resource context, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->UniformMatrix4fv(location, count, transpose, value);
}
void UseProgram(PP_Resource context, GLuint program) {
  PluginResource::GetAs<PluginContext3D>(context)->impl()->UseProgram(program);
}
void ValidateProgram(PP_Resource context, GLuint program) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->ValidateProgram(program);
}
void VertexAttrib1f(PP_Resource context, GLuint indx, GLfloat x) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->VertexAttrib1f(indx, x);
}
void VertexAttrib1fv(PP_Resource context, GLuint indx, const GLfloat* values) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->VertexAttrib1fv(indx, values);
}
void VertexAttrib2f(PP_Resource context, GLuint indx, GLfloat x, GLfloat y) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->VertexAttrib2f(indx, x, y);
}
void VertexAttrib2fv(PP_Resource context, GLuint indx, const GLfloat* values) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->VertexAttrib2fv(indx, values);
}
void VertexAttrib3f(
    PP_Resource context, GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->VertexAttrib3f(indx, x, y, z);
}
void VertexAttrib3fv(PP_Resource context, GLuint indx, const GLfloat* values) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->VertexAttrib3fv(indx, values);
}
void VertexAttrib4f(
    PP_Resource context, GLuint indx, GLfloat x, GLfloat y, GLfloat z,
    GLfloat w) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->VertexAttrib4f(indx, x, y, z, w);
}
void VertexAttrib4fv(PP_Resource context, GLuint indx, const GLfloat* values) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->VertexAttrib4fv(indx, values);
}
void VertexAttribPointer(
    PP_Resource context, GLuint indx, GLint size, GLenum type,
    GLboolean normalized, GLsizei stride, const void* ptr) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->VertexAttribPointer(
          indx, size, type, normalized, stride, ptr);
}
void Viewport(
    PP_Resource context, GLint x, GLint y, GLsizei width, GLsizei height) {
  PluginResource::GetAs<PluginContext3D>(
      context)->impl()->Viewport(x, y, width, height);
}

} // namespace

const PPB_OpenGLES2_Dev* PluginContext3D::GetOpenGLESInterface() {
  const static struct PPB_OpenGLES2_Dev ppb_opengles = {
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
