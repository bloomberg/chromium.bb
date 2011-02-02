// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated. DO NOT EDIT!

#include "ppapi/proxy/ppb_opengles2_proxy.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/dev/ppb_opengles_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppb_context_3d_proxy.h"

namespace pp {
namespace proxy {

namespace {

void ActiveTexture(PP_Resource context_id, GLenum texture) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->ActiveTexture(texture);
}

void AttachShader(PP_Resource context_id, GLuint program, GLuint shader) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->AttachShader(program, shader);
}

void BindAttribLocation(
    PP_Resource context_id, GLuint program, GLuint index, const char* name) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->BindAttribLocation(program, index, name);
}

void BindBuffer(PP_Resource context_id, GLenum target, GLuint buffer) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->BindBuffer(target, buffer);
}

void BindFramebuffer(
    PP_Resource context_id, GLenum target, GLuint framebuffer) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->BindFramebuffer(target, framebuffer);
}

void BindRenderbuffer(
    PP_Resource context_id, GLenum target, GLuint renderbuffer) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->BindRenderbuffer(target, renderbuffer);
}

void BindTexture(PP_Resource context_id, GLenum target, GLuint texture) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->BindTexture(target, texture);
}

void BlendColor(
    PP_Resource context_id, GLclampf red, GLclampf green, GLclampf blue,
    GLclampf alpha) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->BlendColor(red, green, blue, alpha);
}

void BlendEquation(PP_Resource context_id, GLenum mode) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->BlendEquation(mode);
}

void BlendEquationSeparate(
    PP_Resource context_id, GLenum modeRGB, GLenum modeAlpha) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->BlendEquationSeparate(modeRGB, modeAlpha);
}

void BlendFunc(PP_Resource context_id, GLenum sfactor, GLenum dfactor) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->BlendFunc(sfactor, dfactor);
}

void BlendFuncSeparate(
    PP_Resource context_id, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
    GLenum dstAlpha) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void BufferData(
    PP_Resource context_id, GLenum target, GLsizeiptr size, const void* data,
    GLenum usage) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->BufferData(target, size, data, usage);
}

void BufferSubData(
    PP_Resource context_id, GLenum target, GLintptr offset, GLsizeiptr size,
    const void* data) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->BufferSubData(target, offset, size, data);
}

GLenum CheckFramebufferStatus(PP_Resource context_id, GLenum target) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->CheckFramebufferStatus(target);
}

void Clear(PP_Resource context_id, GLbitfield mask) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Clear(mask);
}

void ClearColor(
    PP_Resource context_id, GLclampf red, GLclampf green, GLclampf blue,
    GLclampf alpha) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->ClearColor(red, green, blue, alpha);
}

void ClearDepthf(PP_Resource context_id, GLclampf depth) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->ClearDepthf(depth);
}

void ClearStencil(PP_Resource context_id, GLint s) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->ClearStencil(s);
}

void ColorMask(
    PP_Resource context_id, GLboolean red, GLboolean green, GLboolean blue,
    GLboolean alpha) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->ColorMask(red, green, blue, alpha);
}

void CompileShader(PP_Resource context_id, GLuint shader) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->CompileShader(shader);
}

void CompressedTexImage2D(
    PP_Resource context_id, GLenum target, GLint level, GLenum internalformat,
    GLsizei width, GLsizei height, GLint border, GLsizei imageSize,
    const void* data) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->CompressedTexImage2D(
      target, level, internalformat, width, height, border, imageSize, data);
}

void CompressedTexSubImage2D(
    PP_Resource context_id, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLsizei width, GLsizei height, GLenum format,
    GLsizei imageSize, const void* data) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->CompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

void CopyTexImage2D(
    PP_Resource context_id, GLenum target, GLint level, GLenum internalformat,
    GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->CopyTexImage2D(
      target, level, internalformat, x, y, width, height, border);
}

void CopyTexSubImage2D(
    PP_Resource context_id, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->CopyTexSubImage2D(
      target, level, xoffset, yoffset, x, y, width, height);
}

GLuint CreateProgram(PP_Resource context_id) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->CreateProgram();
}

GLuint CreateShader(PP_Resource context_id, GLenum type) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->CreateShader(type);
}

void CullFace(PP_Resource context_id, GLenum mode) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->CullFace(mode);
}

void DeleteBuffers(PP_Resource context_id, GLsizei n, const GLuint* buffers) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->DeleteBuffers(n, buffers);
}

void DeleteFramebuffers(
    PP_Resource context_id, GLsizei n, const GLuint* framebuffers) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->DeleteFramebuffers(n, framebuffers);
}

void DeleteProgram(PP_Resource context_id, GLuint program) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->DeleteProgram(program);
}

void DeleteRenderbuffers(
    PP_Resource context_id, GLsizei n, const GLuint* renderbuffers) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->DeleteRenderbuffers(n, renderbuffers);
}

void DeleteShader(PP_Resource context_id, GLuint shader) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->DeleteShader(shader);
}

void DeleteTextures(
    PP_Resource context_id, GLsizei n, const GLuint* textures) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->DeleteTextures(n, textures);
}

void DepthFunc(PP_Resource context_id, GLenum func) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->DepthFunc(func);
}

void DepthMask(PP_Resource context_id, GLboolean flag) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->DepthMask(flag);
}

void DepthRangef(PP_Resource context_id, GLclampf zNear, GLclampf zFar) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->DepthRangef(zNear, zFar);
}

void DetachShader(PP_Resource context_id, GLuint program, GLuint shader) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->DetachShader(program, shader);
}

void Disable(PP_Resource context_id, GLenum cap) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Disable(cap);
}

void DisableVertexAttribArray(PP_Resource context_id, GLuint index) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->DisableVertexAttribArray(index);
}

void DrawArrays(
    PP_Resource context_id, GLenum mode, GLint first, GLsizei count) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->DrawArrays(mode, first, count);
}

void DrawElements(
    PP_Resource context_id, GLenum mode, GLsizei count, GLenum type,
    const void* indices) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->DrawElements(mode, count, type, indices);
}

void Enable(PP_Resource context_id, GLenum cap) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Enable(cap);
}

void EnableVertexAttribArray(PP_Resource context_id, GLuint index) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->EnableVertexAttribArray(index);
}

void Finish(PP_Resource context_id) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Finish();
}

void Flush(PP_Resource context_id) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Flush();
}

void FramebufferRenderbuffer(
    PP_Resource context_id, GLenum target, GLenum attachment,
    GLenum renderbuffertarget, GLuint renderbuffer) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->FramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
}

void FramebufferTexture2D(
    PP_Resource context_id, GLenum target, GLenum attachment, GLenum textarget,
    GLuint texture, GLint level) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->FramebufferTexture2D(
      target, attachment, textarget, texture, level);
}

void FrontFace(PP_Resource context_id, GLenum mode) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->FrontFace(mode);
}

void GenBuffers(PP_Resource context_id, GLsizei n, GLuint* buffers) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GenBuffers(n, buffers);
}

void GenerateMipmap(PP_Resource context_id, GLenum target) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GenerateMipmap(target);
}

void GenFramebuffers(PP_Resource context_id, GLsizei n, GLuint* framebuffers) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GenFramebuffers(n, framebuffers);
}

void GenRenderbuffers(
    PP_Resource context_id, GLsizei n, GLuint* renderbuffers) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GenRenderbuffers(n, renderbuffers);
}

void GenTextures(PP_Resource context_id, GLsizei n, GLuint* textures) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GenTextures(n, textures);
}

void GetActiveAttrib(
    PP_Resource context_id, GLuint program, GLuint index, GLsizei bufsize,
    GLsizei* length, GLint* size, GLenum* type, char* name) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetActiveAttrib(
      program, index, bufsize, length, size, type, name);
}

void GetActiveUniform(
    PP_Resource context_id, GLuint program, GLuint index, GLsizei bufsize,
    GLsizei* length, GLint* size, GLenum* type, char* name) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetActiveUniform(
      program, index, bufsize, length, size, type, name);
}

void GetAttachedShaders(
    PP_Resource context_id, GLuint program, GLsizei maxcount, GLsizei* count,
    GLuint* shaders) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetAttachedShaders(program, maxcount, count, shaders);
}

GLint GetAttribLocation(
    PP_Resource context_id, GLuint program, const char* name) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->GetAttribLocation(program, name);
}

void GetBooleanv(PP_Resource context_id, GLenum pname, GLboolean* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetBooleanv(pname, params);
}

void GetBufferParameteriv(
    PP_Resource context_id, GLenum target, GLenum pname, GLint* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetBufferParameteriv(target, pname, params);
}

GLenum GetError(PP_Resource context_id) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->GetError();
}

void GetFloatv(PP_Resource context_id, GLenum pname, GLfloat* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetFloatv(pname, params);
}

void GetFramebufferAttachmentParameteriv(
    PP_Resource context_id, GLenum target, GLenum attachment, GLenum pname,
    GLint* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetFramebufferAttachmentParameteriv(
      target, attachment, pname, params);
}

void GetIntegerv(PP_Resource context_id, GLenum pname, GLint* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetIntegerv(pname, params);
}

void GetProgramiv(
    PP_Resource context_id, GLuint program, GLenum pname, GLint* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetProgramiv(program, pname, params);
}

void GetProgramInfoLog(
    PP_Resource context_id, GLuint program, GLsizei bufsize, GLsizei* length,
    char* infolog) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetProgramInfoLog(program, bufsize, length, infolog);
}

void GetRenderbufferParameteriv(
    PP_Resource context_id, GLenum target, GLenum pname, GLint* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetRenderbufferParameteriv(target, pname, params);
}

void GetShaderiv(
    PP_Resource context_id, GLuint shader, GLenum pname, GLint* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetShaderiv(shader, pname, params);
}

void GetShaderInfoLog(
    PP_Resource context_id, GLuint shader, GLsizei bufsize, GLsizei* length,
    char* infolog) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetShaderInfoLog(shader, bufsize, length, infolog);
}

void GetShaderPrecisionFormat(
    PP_Resource context_id, GLenum shadertype, GLenum precisiontype,
    GLint* range, GLint* precision) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetShaderPrecisionFormat(
      shadertype, precisiontype, range, precision);
}

void GetShaderSource(
    PP_Resource context_id, GLuint shader, GLsizei bufsize, GLsizei* length,
    char* source) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetShaderSource(shader, bufsize, length, source);
}

const GLubyte* GetString(PP_Resource context_id, GLenum name) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->GetString(name);
}

void GetTexParameterfv(
    PP_Resource context_id, GLenum target, GLenum pname, GLfloat* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetTexParameterfv(target, pname, params);
}

void GetTexParameteriv(
    PP_Resource context_id, GLenum target, GLenum pname, GLint* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetTexParameteriv(target, pname, params);
}

void GetUniformfv(
    PP_Resource context_id, GLuint program, GLint location, GLfloat* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetUniformfv(program, location, params);
}

void GetUniformiv(
    PP_Resource context_id, GLuint program, GLint location, GLint* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetUniformiv(program, location, params);
}

GLint GetUniformLocation(
    PP_Resource context_id, GLuint program, const char* name) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->GetUniformLocation(program, name);
}

void GetVertexAttribfv(
    PP_Resource context_id, GLuint index, GLenum pname, GLfloat* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetVertexAttribfv(index, pname, params);
}

void GetVertexAttribiv(
    PP_Resource context_id, GLuint index, GLenum pname, GLint* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetVertexAttribiv(index, pname, params);
}

void GetVertexAttribPointerv(
    PP_Resource context_id, GLuint index, GLenum pname, void** pointer) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->GetVertexAttribPointerv(index, pname, pointer);
}

void Hint(PP_Resource context_id, GLenum target, GLenum mode) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Hint(target, mode);
}

GLboolean IsBuffer(PP_Resource context_id, GLuint buffer) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->IsBuffer(buffer);
}

GLboolean IsEnabled(PP_Resource context_id, GLenum cap) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->IsEnabled(cap);
}

GLboolean IsFramebuffer(PP_Resource context_id, GLuint framebuffer) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->IsFramebuffer(framebuffer);
}

GLboolean IsProgram(PP_Resource context_id, GLuint program) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->IsProgram(program);
}

GLboolean IsRenderbuffer(PP_Resource context_id, GLuint renderbuffer) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->IsRenderbuffer(renderbuffer);
}

GLboolean IsShader(PP_Resource context_id, GLuint shader) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->IsShader(shader);
}

GLboolean IsTexture(PP_Resource context_id, GLuint texture) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->IsTexture(texture);
}

void LineWidth(PP_Resource context_id, GLfloat width) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->LineWidth(width);
}

void LinkProgram(PP_Resource context_id, GLuint program) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->LinkProgram(program);
}

void PixelStorei(PP_Resource context_id, GLenum pname, GLint param) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->PixelStorei(pname, param);
}

void PolygonOffset(PP_Resource context_id, GLfloat factor, GLfloat units) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->PolygonOffset(factor, units);
}

void ReadPixels(
    PP_Resource context_id, GLint x, GLint y, GLsizei width, GLsizei height,
    GLenum format, GLenum type, void* pixels) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->ReadPixels(x, y, width, height, format, type, pixels);
}

void ReleaseShaderCompiler(PP_Resource context_id) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->ReleaseShaderCompiler();
}

void RenderbufferStorage(
    PP_Resource context_id, GLenum target, GLenum internalformat, GLsizei width,
    GLsizei height) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->RenderbufferStorage(
      target, internalformat, width, height);
}

void SampleCoverage(PP_Resource context_id, GLclampf value, GLboolean invert) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->SampleCoverage(value, invert);
}

void Scissor(
    PP_Resource context_id, GLint x, GLint y, GLsizei width, GLsizei height) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Scissor(x, y, width, height);
}

void ShaderBinary(
    PP_Resource context_id, GLsizei n, const GLuint* shaders,
    GLenum binaryformat, const void* binary, GLsizei length) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->ShaderBinary(
      n, shaders, binaryformat, binary, length);
}

void ShaderSource(
    PP_Resource context_id, GLuint shader, GLsizei count, const char** str,
    const GLint* length) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->ShaderSource(shader, count, str, length);
}

void StencilFunc(PP_Resource context_id, GLenum func, GLint ref, GLuint mask) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->StencilFunc(func, ref, mask);
}

void StencilFuncSeparate(
    PP_Resource context_id, GLenum face, GLenum func, GLint ref, GLuint mask) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->StencilFuncSeparate(face, func, ref, mask);
}

void StencilMask(PP_Resource context_id, GLuint mask) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->StencilMask(mask);
}

void StencilMaskSeparate(PP_Resource context_id, GLenum face, GLuint mask) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->StencilMaskSeparate(face, mask);
}

void StencilOp(
    PP_Resource context_id, GLenum fail, GLenum zfail, GLenum zpass) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->StencilOp(fail, zfail, zpass);
}

void StencilOpSeparate(
    PP_Resource context_id, GLenum face, GLenum fail, GLenum zfail,
    GLenum zpass) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->StencilOpSeparate(face, fail, zfail, zpass);
}

void TexImage2D(
    PP_Resource context_id, GLenum target, GLint level, GLint internalformat,
    GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->TexImage2D(
      target, level, internalformat, width, height, border, format, type,
      pixels);
}

void TexParameterf(
    PP_Resource context_id, GLenum target, GLenum pname, GLfloat param) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->TexParameterf(target, pname, param);
}

void TexParameterfv(
    PP_Resource context_id, GLenum target, GLenum pname,
    const GLfloat* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->TexParameterfv(target, pname, params);
}

void TexParameteri(
    PP_Resource context_id, GLenum target, GLenum pname, GLint param) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->TexParameteri(target, pname, param);
}

void TexParameteriv(
    PP_Resource context_id, GLenum target, GLenum pname, const GLint* params) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->TexParameteriv(target, pname, params);
}

void TexSubImage2D(
    PP_Resource context_id, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
    const void* pixels) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->TexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void Uniform1f(PP_Resource context_id, GLint location, GLfloat x) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform1f(location, x);
}

void Uniform1fv(
    PP_Resource context_id, GLint location, GLsizei count, const GLfloat* v) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform1fv(location, count, v);
}

void Uniform1i(PP_Resource context_id, GLint location, GLint x) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform1i(location, x);
}

void Uniform1iv(
    PP_Resource context_id, GLint location, GLsizei count, const GLint* v) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform1iv(location, count, v);
}

void Uniform2f(PP_Resource context_id, GLint location, GLfloat x, GLfloat y) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform2f(location, x, y);
}

void Uniform2fv(
    PP_Resource context_id, GLint location, GLsizei count, const GLfloat* v) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform2fv(location, count, v);
}

void Uniform2i(PP_Resource context_id, GLint location, GLint x, GLint y) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform2i(location, x, y);
}

void Uniform2iv(
    PP_Resource context_id, GLint location, GLsizei count, const GLint* v) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform2iv(location, count, v);
}

void Uniform3f(
    PP_Resource context_id, GLint location, GLfloat x, GLfloat y, GLfloat z) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform3f(location, x, y, z);
}

void Uniform3fv(
    PP_Resource context_id, GLint location, GLsizei count, const GLfloat* v) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform3fv(location, count, v);
}

void Uniform3i(
    PP_Resource context_id, GLint location, GLint x, GLint y, GLint z) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform3i(location, x, y, z);
}

void Uniform3iv(
    PP_Resource context_id, GLint location, GLsizei count, const GLint* v) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform3iv(location, count, v);
}

void Uniform4f(
    PP_Resource context_id, GLint location, GLfloat x, GLfloat y, GLfloat z,
    GLfloat w) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform4f(location, x, y, z, w);
}

void Uniform4fv(
    PP_Resource context_id, GLint location, GLsizei count, const GLfloat* v) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform4fv(location, count, v);
}

void Uniform4i(
    PP_Resource context_id, GLint location, GLint x, GLint y, GLint z,
    GLint w) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform4i(location, x, y, z, w);
}

void Uniform4iv(
    PP_Resource context_id, GLint location, GLsizei count, const GLint* v) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Uniform4iv(location, count, v);
}

void UniformMatrix2fv(
    PP_Resource context_id, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->UniformMatrix2fv(location, count, transpose, value);
}

void UniformMatrix3fv(
    PP_Resource context_id, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->UniformMatrix3fv(location, count, transpose, value);
}

void UniformMatrix4fv(
    PP_Resource context_id, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->UniformMatrix4fv(location, count, transpose, value);
}

void UseProgram(PP_Resource context_id, GLuint program) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->UseProgram(program);
}

void ValidateProgram(PP_Resource context_id, GLuint program) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->ValidateProgram(program);
}

void VertexAttrib1f(PP_Resource context_id, GLuint indx, GLfloat x) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->VertexAttrib1f(indx, x);
}

void VertexAttrib1fv(
    PP_Resource context_id, GLuint indx, const GLfloat* values) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->VertexAttrib1fv(indx, values);
}

void VertexAttrib2f(
    PP_Resource context_id, GLuint indx, GLfloat x, GLfloat y) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->VertexAttrib2f(indx, x, y);
}

void VertexAttrib2fv(
    PP_Resource context_id, GLuint indx, const GLfloat* values) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->VertexAttrib2fv(indx, values);
}

void VertexAttrib3f(
    PP_Resource context_id, GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->VertexAttrib3f(indx, x, y, z);
}

void VertexAttrib3fv(
    PP_Resource context_id, GLuint indx, const GLfloat* values) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->VertexAttrib3fv(indx, values);
}

void VertexAttrib4f(
    PP_Resource context_id, GLuint indx, GLfloat x, GLfloat y, GLfloat z,
    GLfloat w) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->VertexAttrib4f(indx, x, y, z, w);
}

void VertexAttrib4fv(
    PP_Resource context_id, GLuint indx, const GLfloat* values) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->VertexAttrib4fv(indx, values);
}

void VertexAttribPointer(
    PP_Resource context_id, GLuint indx, GLint size, GLenum type,
    GLboolean normalized, GLsizei stride, const void* ptr) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->VertexAttribPointer(
      indx, size, type, normalized, stride, ptr);
}

void Viewport(
    PP_Resource context_id, GLint x, GLint y, GLsizei width, GLsizei height) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->Viewport(x, y, width, height);
}

const struct PPB_OpenGLES2_Dev ppb_opengles2 = {
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

}  // namespace

PPB_OpenGLES2_Proxy::PPB_OpenGLES2_Proxy(Dispatcher* dispatcher,
                                         const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_OpenGLES2_Proxy::~PPB_OpenGLES2_Proxy() {
}

const void* PPB_OpenGLES2_Proxy::GetSourceInterface() const {
  return &ppb_opengles2;
}

InterfaceID PPB_OpenGLES2_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_NONE;
}

bool PPB_OpenGLES2_Proxy::OnMessageReceived(const IPC::Message& msg) {
  return false;
}

}  // namespace proxy
}  // namespace pp
