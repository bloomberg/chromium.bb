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

void ActiveTexture(PP_Resource context, GLenum texture) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->ActiveTexture(texture);
}

void AttachShader(PP_Resource context, GLuint program, GLuint shader) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->AttachShader(program, shader);
}

void BindAttribLocation(
    PP_Resource context, GLuint program, GLuint index, const char* name) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->BindAttribLocation(program, index, name);
}

void BindBuffer(PP_Resource context, GLenum target, GLuint buffer) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->BindBuffer(target, buffer);
}

void BindFramebuffer(PP_Resource context, GLenum target, GLuint framebuffer) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->BindFramebuffer(target, framebuffer);
}

void BindRenderbuffer(
    PP_Resource context, GLenum target, GLuint renderbuffer) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->BindRenderbuffer(target, renderbuffer);
}

void BindTexture(PP_Resource context, GLenum target, GLuint texture) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->BindTexture(target, texture);
}

void BlendColor(
    PP_Resource context, GLclampf red, GLclampf green, GLclampf blue,
    GLclampf alpha) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->BlendColor(red, green, blue, alpha);
}

void BlendEquation(PP_Resource context, GLenum mode) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->BlendEquation(mode);
}

void BlendEquationSeparate(
    PP_Resource context, GLenum modeRGB, GLenum modeAlpha) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->BlendEquationSeparate(modeRGB, modeAlpha);
}

void BlendFunc(PP_Resource context, GLenum sfactor, GLenum dfactor) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->BlendFunc(sfactor, dfactor);
}

void BlendFuncSeparate(
    PP_Resource context, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
    GLenum dstAlpha) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void BufferData(
    PP_Resource context, GLenum target, GLsizeiptr size, const void* data,
    GLenum usage) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->BufferData(target, size, data, usage);
}

void BufferSubData(
    PP_Resource context, GLenum target, GLintptr offset, GLsizeiptr size,
    const void* data) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->BufferSubData(target, offset, size, data);
}

GLenum CheckFramebufferStatus(PP_Resource context, GLenum target) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  return graphics_3d->impl()->CheckFramebufferStatus(target);
}

void Clear(PP_Resource context, GLbitfield mask) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Clear(mask);
}

void ClearColor(
    PP_Resource context, GLclampf red, GLclampf green, GLclampf blue,
    GLclampf alpha) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->ClearColor(red, green, blue, alpha);
}

void ClearDepthf(PP_Resource context, GLclampf depth) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->ClearDepthf(depth);
}

void ClearStencil(PP_Resource context, GLint s) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->ClearStencil(s);
}

void ColorMask(
    PP_Resource context, GLboolean red, GLboolean green, GLboolean blue,
    GLboolean alpha) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->ColorMask(red, green, blue, alpha);
}

void CompileShader(PP_Resource context, GLuint shader) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->CompileShader(shader);
}

void CompressedTexImage2D(
    PP_Resource context, GLenum target, GLint level, GLenum internalformat,
    GLsizei width, GLsizei height, GLint border, GLsizei imageSize,
    const void* data) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->CompressedTexImage2D(
      target, level, internalformat, width, height, border, imageSize, data);
}

void CompressedTexSubImage2D(
    PP_Resource context, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLsizei width, GLsizei height, GLenum format,
    GLsizei imageSize, const void* data) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->CompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

void CopyTexImage2D(
    PP_Resource context, GLenum target, GLint level, GLenum internalformat,
    GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->CopyTexImage2D(
      target, level, internalformat, x, y, width, height, border);
}

void CopyTexSubImage2D(
    PP_Resource context, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->CopyTexSubImage2D(
      target, level, xoffset, yoffset, x, y, width, height);
}

GLuint CreateProgram(PP_Resource context) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  return graphics_3d->impl()->CreateProgram();
}

GLuint CreateShader(PP_Resource context, GLenum type) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  return graphics_3d->impl()->CreateShader(type);
}

void CullFace(PP_Resource context, GLenum mode) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->CullFace(mode);
}

void DeleteBuffers(PP_Resource context, GLsizei n, const GLuint* buffers) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->DeleteBuffers(n, buffers);
}

void DeleteFramebuffers(
    PP_Resource context, GLsizei n, const GLuint* framebuffers) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->DeleteFramebuffers(n, framebuffers);
}

void DeleteProgram(PP_Resource context, GLuint program) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->DeleteProgram(program);
}

void DeleteRenderbuffers(
    PP_Resource context, GLsizei n, const GLuint* renderbuffers) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->DeleteRenderbuffers(n, renderbuffers);
}

void DeleteShader(PP_Resource context, GLuint shader) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->DeleteShader(shader);
}

void DeleteTextures(PP_Resource context, GLsizei n, const GLuint* textures) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->DeleteTextures(n, textures);
}

void DepthFunc(PP_Resource context, GLenum func) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->DepthFunc(func);
}

void DepthMask(PP_Resource context, GLboolean flag) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->DepthMask(flag);
}

void DepthRangef(PP_Resource context, GLclampf zNear, GLclampf zFar) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->DepthRangef(zNear, zFar);
}

void DetachShader(PP_Resource context, GLuint program, GLuint shader) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->DetachShader(program, shader);
}

void Disable(PP_Resource context, GLenum cap) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Disable(cap);
}

void DisableVertexAttribArray(PP_Resource context, GLuint index) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->DisableVertexAttribArray(index);
}

void DrawArrays(PP_Resource context, GLenum mode, GLint first, GLsizei count) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->DrawArrays(mode, first, count);
}

void DrawElements(
    PP_Resource context, GLenum mode, GLsizei count, GLenum type,
    const void* indices) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->DrawElements(mode, count, type, indices);
}

void Enable(PP_Resource context, GLenum cap) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Enable(cap);
}

void EnableVertexAttribArray(PP_Resource context, GLuint index) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->EnableVertexAttribArray(index);
}

void Finish(PP_Resource context) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Finish();
}

void Flush(PP_Resource context) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Flush();
}

void FramebufferRenderbuffer(
    PP_Resource context, GLenum target, GLenum attachment,
    GLenum renderbuffertarget, GLuint renderbuffer) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->FramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
}

void FramebufferTexture2D(
    PP_Resource context, GLenum target, GLenum attachment, GLenum textarget,
    GLuint texture, GLint level) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->FramebufferTexture2D(
      target, attachment, textarget, texture, level);
}

void FrontFace(PP_Resource context, GLenum mode) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->FrontFace(mode);
}

void GenBuffers(PP_Resource context, GLsizei n, GLuint* buffers) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GenBuffers(n, buffers);
}

void GenerateMipmap(PP_Resource context, GLenum target) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GenerateMipmap(target);
}

void GenFramebuffers(PP_Resource context, GLsizei n, GLuint* framebuffers) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GenFramebuffers(n, framebuffers);
}

void GenRenderbuffers(PP_Resource context, GLsizei n, GLuint* renderbuffers) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GenRenderbuffers(n, renderbuffers);
}

void GenTextures(PP_Resource context, GLsizei n, GLuint* textures) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GenTextures(n, textures);
}

void GetActiveAttrib(
    PP_Resource context, GLuint program, GLuint index, GLsizei bufsize,
    GLsizei* length, GLint* size, GLenum* type, char* name) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetActiveAttrib(
      program, index, bufsize, length, size, type, name);
}

void GetActiveUniform(
    PP_Resource context, GLuint program, GLuint index, GLsizei bufsize,
    GLsizei* length, GLint* size, GLenum* type, char* name) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetActiveUniform(
      program, index, bufsize, length, size, type, name);
}

void GetAttachedShaders(
    PP_Resource context, GLuint program, GLsizei maxcount, GLsizei* count,
    GLuint* shaders) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetAttachedShaders(program, maxcount, count, shaders);
}

GLint GetAttribLocation(
    PP_Resource context, GLuint program, const char* name) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  return graphics_3d->impl()->GetAttribLocation(program, name);
}

void GetBooleanv(PP_Resource context, GLenum pname, GLboolean* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetBooleanv(pname, params);
}

void GetBufferParameteriv(
    PP_Resource context, GLenum target, GLenum pname, GLint* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetBufferParameteriv(target, pname, params);
}

GLenum GetError(PP_Resource context) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  return graphics_3d->impl()->GetError();
}

void GetFloatv(PP_Resource context, GLenum pname, GLfloat* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetFloatv(pname, params);
}

void GetFramebufferAttachmentParameteriv(
    PP_Resource context, GLenum target, GLenum attachment, GLenum pname,
    GLint* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetFramebufferAttachmentParameteriv(
      target, attachment, pname, params);
}

void GetIntegerv(PP_Resource context, GLenum pname, GLint* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetIntegerv(pname, params);
}

void GetProgramiv(
    PP_Resource context, GLuint program, GLenum pname, GLint* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetProgramiv(program, pname, params);
}

void GetProgramInfoLog(
    PP_Resource context, GLuint program, GLsizei bufsize, GLsizei* length,
    char* infolog) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetProgramInfoLog(program, bufsize, length, infolog);
}

void GetRenderbufferParameteriv(
    PP_Resource context, GLenum target, GLenum pname, GLint* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetRenderbufferParameteriv(target, pname, params);
}

void GetShaderiv(
    PP_Resource context, GLuint shader, GLenum pname, GLint* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetShaderiv(shader, pname, params);
}

void GetShaderInfoLog(
    PP_Resource context, GLuint shader, GLsizei bufsize, GLsizei* length,
    char* infolog) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetShaderInfoLog(shader, bufsize, length, infolog);
}

void GetShaderPrecisionFormat(
    PP_Resource context, GLenum shadertype, GLenum precisiontype, GLint* range,
    GLint* precision) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetShaderPrecisionFormat(
      shadertype, precisiontype, range, precision);
}

void GetShaderSource(
    PP_Resource context, GLuint shader, GLsizei bufsize, GLsizei* length,
    char* source) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetShaderSource(shader, bufsize, length, source);
}

const GLubyte* GetString(PP_Resource context, GLenum name) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  return graphics_3d->impl()->GetString(name);
}

void GetTexParameterfv(
    PP_Resource context, GLenum target, GLenum pname, GLfloat* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetTexParameterfv(target, pname, params);
}

void GetTexParameteriv(
    PP_Resource context, GLenum target, GLenum pname, GLint* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetTexParameteriv(target, pname, params);
}

void GetUniformfv(
    PP_Resource context, GLuint program, GLint location, GLfloat* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetUniformfv(program, location, params);
}

void GetUniformiv(
    PP_Resource context, GLuint program, GLint location, GLint* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetUniformiv(program, location, params);
}

GLint GetUniformLocation(
    PP_Resource context, GLuint program, const char* name) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  return graphics_3d->impl()->GetUniformLocation(program, name);
}

void GetVertexAttribfv(
    PP_Resource context, GLuint index, GLenum pname, GLfloat* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetVertexAttribfv(index, pname, params);
}

void GetVertexAttribiv(
    PP_Resource context, GLuint index, GLenum pname, GLint* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetVertexAttribiv(index, pname, params);
}

void GetVertexAttribPointerv(
    PP_Resource context, GLuint index, GLenum pname, void** pointer) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->GetVertexAttribPointerv(index, pname, pointer);
}

void Hint(PP_Resource context, GLenum target, GLenum mode) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Hint(target, mode);
}

GLboolean IsBuffer(PP_Resource context, GLuint buffer) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  return graphics_3d->impl()->IsBuffer(buffer);
}

GLboolean IsEnabled(PP_Resource context, GLenum cap) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  return graphics_3d->impl()->IsEnabled(cap);
}

GLboolean IsFramebuffer(PP_Resource context, GLuint framebuffer) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  return graphics_3d->impl()->IsFramebuffer(framebuffer);
}

GLboolean IsProgram(PP_Resource context, GLuint program) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  return graphics_3d->impl()->IsProgram(program);
}

GLboolean IsRenderbuffer(PP_Resource context, GLuint renderbuffer) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  return graphics_3d->impl()->IsRenderbuffer(renderbuffer);
}

GLboolean IsShader(PP_Resource context, GLuint shader) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  return graphics_3d->impl()->IsShader(shader);
}

GLboolean IsTexture(PP_Resource context, GLuint texture) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  return graphics_3d->impl()->IsTexture(texture);
}

void LineWidth(PP_Resource context, GLfloat width) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->LineWidth(width);
}

void LinkProgram(PP_Resource context, GLuint program) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->LinkProgram(program);
}

void PixelStorei(PP_Resource context, GLenum pname, GLint param) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->PixelStorei(pname, param);
}

void PolygonOffset(PP_Resource context, GLfloat factor, GLfloat units) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->PolygonOffset(factor, units);
}

void ReadPixels(
    PP_Resource context, GLint x, GLint y, GLsizei width, GLsizei height,
    GLenum format, GLenum type, void* pixels) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->ReadPixels(x, y, width, height, format, type, pixels);
}

void ReleaseShaderCompiler(PP_Resource context) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->ReleaseShaderCompiler();
}

void RenderbufferStorage(
    PP_Resource context, GLenum target, GLenum internalformat, GLsizei width,
    GLsizei height) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->RenderbufferStorage(
      target, internalformat, width, height);
}

void SampleCoverage(PP_Resource context, GLclampf value, GLboolean invert) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->SampleCoverage(value, invert);
}

void Scissor(
    PP_Resource context, GLint x, GLint y, GLsizei width, GLsizei height) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Scissor(x, y, width, height);
}

void ShaderBinary(
    PP_Resource context, GLsizei n, const GLuint* shaders, GLenum binaryformat,
    const void* binary, GLsizei length) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->ShaderBinary(n, shaders, binaryformat, binary, length);
}

void ShaderSource(
    PP_Resource context, GLuint shader, GLsizei count, const char** str,
    const GLint* length) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->ShaderSource(shader, count, str, length);
}

void StencilFunc(PP_Resource context, GLenum func, GLint ref, GLuint mask) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->StencilFunc(func, ref, mask);
}

void StencilFuncSeparate(
    PP_Resource context, GLenum face, GLenum func, GLint ref, GLuint mask) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->StencilFuncSeparate(face, func, ref, mask);
}

void StencilMask(PP_Resource context, GLuint mask) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->StencilMask(mask);
}

void StencilMaskSeparate(PP_Resource context, GLenum face, GLuint mask) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->StencilMaskSeparate(face, mask);
}

void StencilOp(PP_Resource context, GLenum fail, GLenum zfail, GLenum zpass) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->StencilOp(fail, zfail, zpass);
}

void StencilOpSeparate(
    PP_Resource context, GLenum face, GLenum fail, GLenum zfail,
    GLenum zpass) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->StencilOpSeparate(face, fail, zfail, zpass);
}

void TexImage2D(
    PP_Resource context, GLenum target, GLint level, GLint internalformat,
    GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->TexImage2D(
      target, level, internalformat, width, height, border, format, type,
      pixels);
}

void TexParameterf(
    PP_Resource context, GLenum target, GLenum pname, GLfloat param) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->TexParameterf(target, pname, param);
}

void TexParameterfv(
    PP_Resource context, GLenum target, GLenum pname, const GLfloat* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->TexParameterfv(target, pname, params);
}

void TexParameteri(
    PP_Resource context, GLenum target, GLenum pname, GLint param) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->TexParameteri(target, pname, param);
}

void TexParameteriv(
    PP_Resource context, GLenum target, GLenum pname, const GLint* params) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->TexParameteriv(target, pname, params);
}

void TexSubImage2D(
    PP_Resource context, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
    const void* pixels) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->TexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void Uniform1f(PP_Resource context, GLint location, GLfloat x) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform1f(location, x);
}

void Uniform1fv(
    PP_Resource context, GLint location, GLsizei count, const GLfloat* v) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform1fv(location, count, v);
}

void Uniform1i(PP_Resource context, GLint location, GLint x) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform1i(location, x);
}

void Uniform1iv(
    PP_Resource context, GLint location, GLsizei count, const GLint* v) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform1iv(location, count, v);
}

void Uniform2f(PP_Resource context, GLint location, GLfloat x, GLfloat y) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform2f(location, x, y);
}

void Uniform2fv(
    PP_Resource context, GLint location, GLsizei count, const GLfloat* v) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform2fv(location, count, v);
}

void Uniform2i(PP_Resource context, GLint location, GLint x, GLint y) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform2i(location, x, y);
}

void Uniform2iv(
    PP_Resource context, GLint location, GLsizei count, const GLint* v) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform2iv(location, count, v);
}

void Uniform3f(
    PP_Resource context, GLint location, GLfloat x, GLfloat y, GLfloat z) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform3f(location, x, y, z);
}

void Uniform3fv(
    PP_Resource context, GLint location, GLsizei count, const GLfloat* v) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform3fv(location, count, v);
}

void Uniform3i(
    PP_Resource context, GLint location, GLint x, GLint y, GLint z) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform3i(location, x, y, z);
}

void Uniform3iv(
    PP_Resource context, GLint location, GLsizei count, const GLint* v) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform3iv(location, count, v);
}

void Uniform4f(
    PP_Resource context, GLint location, GLfloat x, GLfloat y, GLfloat z,
    GLfloat w) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform4f(location, x, y, z, w);
}

void Uniform4fv(
    PP_Resource context, GLint location, GLsizei count, const GLfloat* v) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform4fv(location, count, v);
}

void Uniform4i(
    PP_Resource context, GLint location, GLint x, GLint y, GLint z, GLint w) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform4i(location, x, y, z, w);
}

void Uniform4iv(
    PP_Resource context, GLint location, GLsizei count, const GLint* v) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Uniform4iv(location, count, v);
}

void UniformMatrix2fv(
    PP_Resource context, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->UniformMatrix2fv(location, count, transpose, value);
}

void UniformMatrix3fv(
    PP_Resource context, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->UniformMatrix3fv(location, count, transpose, value);
}

void UniformMatrix4fv(
    PP_Resource context, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->UniformMatrix4fv(location, count, transpose, value);
}

void UseProgram(PP_Resource context, GLuint program) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->UseProgram(program);
}

void ValidateProgram(PP_Resource context, GLuint program) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->ValidateProgram(program);
}

void VertexAttrib1f(PP_Resource context, GLuint indx, GLfloat x) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->VertexAttrib1f(indx, x);
}

void VertexAttrib1fv(PP_Resource context, GLuint indx, const GLfloat* values) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->VertexAttrib1fv(indx, values);
}

void VertexAttrib2f(PP_Resource context, GLuint indx, GLfloat x, GLfloat y) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->VertexAttrib2f(indx, x, y);
}

void VertexAttrib2fv(PP_Resource context, GLuint indx, const GLfloat* values) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->VertexAttrib2fv(indx, values);
}

void VertexAttrib3f(
    PP_Resource context, GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->VertexAttrib3f(indx, x, y, z);
}

void VertexAttrib3fv(PP_Resource context, GLuint indx, const GLfloat* values) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->VertexAttrib3fv(indx, values);
}

void VertexAttrib4f(
    PP_Resource context, GLuint indx, GLfloat x, GLfloat y, GLfloat z,
    GLfloat w) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->VertexAttrib4f(indx, x, y, z, w);
}

void VertexAttrib4fv(PP_Resource context, GLuint indx, const GLfloat* values) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->VertexAttrib4fv(indx, values);
}

void VertexAttribPointer(
    PP_Resource context, GLuint indx, GLint size, GLenum type,
    GLboolean normalized, GLsizei stride, const void* ptr) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->VertexAttribPointer(
      indx, size, type, normalized, stride, ptr);
}

void Viewport(
    PP_Resource context, GLint x, GLint y, GLsizei width, GLsizei height) {
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d = 
      Resource::GetAs<PPB_Graphics3D_Impl>(context);
  graphics_3d->impl()->Viewport(x, y, width, height);
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

const PPB_OpenGLES2_Dev* PPB_Graphics3D_Impl::GetOpenGLES2Interface() {
  return &ppb_opengles2;
}

}  // namespace ppapi
}  // namespace webkit

