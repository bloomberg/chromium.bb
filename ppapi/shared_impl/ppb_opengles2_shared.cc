// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

#include "ppapi/shared_impl/ppb_opengles2_shared.h"

#include "base/logging.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/shared_impl/ppb_graphics_3d_shared.h"
#include "ppapi/thunk/enter.h"

namespace ppapi {

namespace {

gpu::gles2::GLES2Implementation* GetGLES(PP_Resource context) {
  thunk::EnterResource<thunk::PPB_Graphics3D_API> enter_g3d(context, false);
  DCHECK(enter_g3d.succeeded());
  return static_cast<PPB_Graphics3D_Shared*>(enter_g3d.object())->gles2_impl();
}

void ActiveTexture(PP_Resource context_id, GLenum texture) {
  GetGLES(context_id)->ActiveTexture(texture);
}

void AttachShader(PP_Resource context_id, GLuint program, GLuint shader) {
  GetGLES(context_id)->AttachShader(program, shader);
}

void BindAttribLocation(
    PP_Resource context_id, GLuint program, GLuint index, const char* name) {
  GetGLES(context_id)->BindAttribLocation(program, index, name);
}

void BindBuffer(PP_Resource context_id, GLenum target, GLuint buffer) {
  GetGLES(context_id)->BindBuffer(target, buffer);
}

void BindFramebuffer(
    PP_Resource context_id, GLenum target, GLuint framebuffer) {
  GetGLES(context_id)->BindFramebuffer(target, framebuffer);
}

void BindRenderbuffer(
    PP_Resource context_id, GLenum target, GLuint renderbuffer) {
  GetGLES(context_id)->BindRenderbuffer(target, renderbuffer);
}

void BindTexture(PP_Resource context_id, GLenum target, GLuint texture) {
  GetGLES(context_id)->BindTexture(target, texture);
}

void BlendColor(
    PP_Resource context_id, GLclampf red, GLclampf green, GLclampf blue,
    GLclampf alpha) {
  GetGLES(context_id)->BlendColor(red, green, blue, alpha);
}

void BlendEquation(PP_Resource context_id, GLenum mode) {
  GetGLES(context_id)->BlendEquation(mode);
}

void BlendEquationSeparate(
    PP_Resource context_id, GLenum modeRGB, GLenum modeAlpha) {
  GetGLES(context_id)->BlendEquationSeparate(modeRGB, modeAlpha);
}

void BlendFunc(PP_Resource context_id, GLenum sfactor, GLenum dfactor) {
  GetGLES(context_id)->BlendFunc(sfactor, dfactor);
}

void BlendFuncSeparate(
    PP_Resource context_id, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
    GLenum dstAlpha) {
  GetGLES(context_id)->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void BufferData(
    PP_Resource context_id, GLenum target, GLsizeiptr size, const void* data,
    GLenum usage) {
  GetGLES(context_id)->BufferData(target, size, data, usage);
}

void BufferSubData(
    PP_Resource context_id, GLenum target, GLintptr offset, GLsizeiptr size,
    const void* data) {
  GetGLES(context_id)->BufferSubData(target, offset, size, data);
}

GLenum CheckFramebufferStatus(PP_Resource context_id, GLenum target) {
  return GetGLES(context_id)->CheckFramebufferStatus(target);
}

void Clear(PP_Resource context_id, GLbitfield mask) {
  GetGLES(context_id)->Clear(mask);
}

void ClearColor(
    PP_Resource context_id, GLclampf red, GLclampf green, GLclampf blue,
    GLclampf alpha) {
  GetGLES(context_id)->ClearColor(red, green, blue, alpha);
}

void ClearDepthf(PP_Resource context_id, GLclampf depth) {
  GetGLES(context_id)->ClearDepthf(depth);
}

void ClearStencil(PP_Resource context_id, GLint s) {
  GetGLES(context_id)->ClearStencil(s);
}

void ColorMask(
    PP_Resource context_id, GLboolean red, GLboolean green, GLboolean blue,
    GLboolean alpha) {
  GetGLES(context_id)->ColorMask(red, green, blue, alpha);
}

void CompileShader(PP_Resource context_id, GLuint shader) {
  GetGLES(context_id)->CompileShader(shader);
}

void CompressedTexImage2D(
    PP_Resource context_id, GLenum target, GLint level, GLenum internalformat,
    GLsizei width, GLsizei height, GLint border, GLsizei imageSize,
    const void* data) {
  GetGLES(
      context_id)->CompressedTexImage2D(
          target, level, internalformat, width, height, border, imageSize,
          data);
}

void CompressedTexSubImage2D(
    PP_Resource context_id, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLsizei width, GLsizei height, GLenum format,
    GLsizei imageSize, const void* data) {
  GetGLES(
      context_id)->CompressedTexSubImage2D(
          target, level, xoffset, yoffset, width, height, format, imageSize,
          data);
}

void CopyTexImage2D(
    PP_Resource context_id, GLenum target, GLint level, GLenum internalformat,
    GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
  GetGLES(
      context_id)->CopyTexImage2D(
          target, level, internalformat, x, y, width, height, border);
}

void CopyTexSubImage2D(
    PP_Resource context_id, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
  GetGLES(
      context_id)->CopyTexSubImage2D(
          target, level, xoffset, yoffset, x, y, width, height);
}

GLuint CreateProgram(PP_Resource context_id) {
  return GetGLES(context_id)->CreateProgram();
}

GLuint CreateShader(PP_Resource context_id, GLenum type) {
  return GetGLES(context_id)->CreateShader(type);
}

void CullFace(PP_Resource context_id, GLenum mode) {
  GetGLES(context_id)->CullFace(mode);
}

void DeleteBuffers(PP_Resource context_id, GLsizei n, const GLuint* buffers) {
  GetGLES(context_id)->DeleteBuffers(n, buffers);
}

void DeleteFramebuffers(
    PP_Resource context_id, GLsizei n, const GLuint* framebuffers) {
  GetGLES(context_id)->DeleteFramebuffers(n, framebuffers);
}

void DeleteProgram(PP_Resource context_id, GLuint program) {
  GetGLES(context_id)->DeleteProgram(program);
}

void DeleteRenderbuffers(
    PP_Resource context_id, GLsizei n, const GLuint* renderbuffers) {
  GetGLES(context_id)->DeleteRenderbuffers(n, renderbuffers);
}

void DeleteShader(PP_Resource context_id, GLuint shader) {
  GetGLES(context_id)->DeleteShader(shader);
}

void DeleteTextures(
    PP_Resource context_id, GLsizei n, const GLuint* textures) {
  GetGLES(context_id)->DeleteTextures(n, textures);
}

void DepthFunc(PP_Resource context_id, GLenum func) {
  GetGLES(context_id)->DepthFunc(func);
}

void DepthMask(PP_Resource context_id, GLboolean flag) {
  GetGLES(context_id)->DepthMask(flag);
}

void DepthRangef(PP_Resource context_id, GLclampf zNear, GLclampf zFar) {
  GetGLES(context_id)->DepthRangef(zNear, zFar);
}

void DetachShader(PP_Resource context_id, GLuint program, GLuint shader) {
  GetGLES(context_id)->DetachShader(program, shader);
}

void Disable(PP_Resource context_id, GLenum cap) {
  GetGLES(context_id)->Disable(cap);
}

void DisableVertexAttribArray(PP_Resource context_id, GLuint index) {
  GetGLES(context_id)->DisableVertexAttribArray(index);
}

void DrawArrays(
    PP_Resource context_id, GLenum mode, GLint first, GLsizei count) {
  GetGLES(context_id)->DrawArrays(mode, first, count);
}

void DrawElements(
    PP_Resource context_id, GLenum mode, GLsizei count, GLenum type,
    const void* indices) {
  GetGLES(context_id)->DrawElements(mode, count, type, indices);
}

void Enable(PP_Resource context_id, GLenum cap) {
  GetGLES(context_id)->Enable(cap);
}

void EnableVertexAttribArray(PP_Resource context_id, GLuint index) {
  GetGLES(context_id)->EnableVertexAttribArray(index);
}

void Finish(PP_Resource context_id) {
  GetGLES(context_id)->Finish();
}

void Flush(PP_Resource context_id) {
  GetGLES(context_id)->Flush();
}

void FramebufferRenderbuffer(
    PP_Resource context_id, GLenum target, GLenum attachment,
    GLenum renderbuffertarget, GLuint renderbuffer) {
  GetGLES(
      context_id)->FramebufferRenderbuffer(
          target, attachment, renderbuffertarget, renderbuffer);
}

void FramebufferTexture2D(
    PP_Resource context_id, GLenum target, GLenum attachment, GLenum textarget,
    GLuint texture, GLint level) {
  GetGLES(
      context_id)->FramebufferTexture2D(
          target, attachment, textarget, texture, level);
}

void FrontFace(PP_Resource context_id, GLenum mode) {
  GetGLES(context_id)->FrontFace(mode);
}

void GenBuffers(PP_Resource context_id, GLsizei n, GLuint* buffers) {
  GetGLES(context_id)->GenBuffers(n, buffers);
}

void GenerateMipmap(PP_Resource context_id, GLenum target) {
  GetGLES(context_id)->GenerateMipmap(target);
}

void GenFramebuffers(PP_Resource context_id, GLsizei n, GLuint* framebuffers) {
  GetGLES(context_id)->GenFramebuffers(n, framebuffers);
}

void GenRenderbuffers(
    PP_Resource context_id, GLsizei n, GLuint* renderbuffers) {
  GetGLES(context_id)->GenRenderbuffers(n, renderbuffers);
}

void GenTextures(PP_Resource context_id, GLsizei n, GLuint* textures) {
  GetGLES(context_id)->GenTextures(n, textures);
}

void GetActiveAttrib(
    PP_Resource context_id, GLuint program, GLuint index, GLsizei bufsize,
    GLsizei* length, GLint* size, GLenum* type, char* name) {
  GetGLES(
      context_id)->GetActiveAttrib(
          program, index, bufsize, length, size, type, name);
}

void GetActiveUniform(
    PP_Resource context_id, GLuint program, GLuint index, GLsizei bufsize,
    GLsizei* length, GLint* size, GLenum* type, char* name) {
  GetGLES(
      context_id)->GetActiveUniform(
          program, index, bufsize, length, size, type, name);
}

void GetAttachedShaders(
    PP_Resource context_id, GLuint program, GLsizei maxcount, GLsizei* count,
    GLuint* shaders) {
  GetGLES(context_id)->GetAttachedShaders(program, maxcount, count, shaders);
}

GLint GetAttribLocation(
    PP_Resource context_id, GLuint program, const char* name) {
  return GetGLES(context_id)->GetAttribLocation(program, name);
}

void GetBooleanv(PP_Resource context_id, GLenum pname, GLboolean* params) {
  GetGLES(context_id)->GetBooleanv(pname, params);
}

void GetBufferParameteriv(
    PP_Resource context_id, GLenum target, GLenum pname, GLint* params) {
  GetGLES(context_id)->GetBufferParameteriv(target, pname, params);
}

GLenum GetError(PP_Resource context_id) {
  return GetGLES(context_id)->GetError();
}

void GetFloatv(PP_Resource context_id, GLenum pname, GLfloat* params) {
  GetGLES(context_id)->GetFloatv(pname, params);
}

void GetFramebufferAttachmentParameteriv(
    PP_Resource context_id, GLenum target, GLenum attachment, GLenum pname,
    GLint* params) {
  GetGLES(
      context_id)->GetFramebufferAttachmentParameteriv(
          target, attachment, pname, params);
}

void GetIntegerv(PP_Resource context_id, GLenum pname, GLint* params) {
  GetGLES(context_id)->GetIntegerv(pname, params);
}

void GetProgramiv(
    PP_Resource context_id, GLuint program, GLenum pname, GLint* params) {
  GetGLES(context_id)->GetProgramiv(program, pname, params);
}

void GetProgramInfoLog(
    PP_Resource context_id, GLuint program, GLsizei bufsize, GLsizei* length,
    char* infolog) {
  GetGLES(context_id)->GetProgramInfoLog(program, bufsize, length, infolog);
}

void GetRenderbufferParameteriv(
    PP_Resource context_id, GLenum target, GLenum pname, GLint* params) {
  GetGLES(context_id)->GetRenderbufferParameteriv(target, pname, params);
}

void GetShaderiv(
    PP_Resource context_id, GLuint shader, GLenum pname, GLint* params) {
  GetGLES(context_id)->GetShaderiv(shader, pname, params);
}

void GetShaderInfoLog(
    PP_Resource context_id, GLuint shader, GLsizei bufsize, GLsizei* length,
    char* infolog) {
  GetGLES(context_id)->GetShaderInfoLog(shader, bufsize, length, infolog);
}

void GetShaderPrecisionFormat(
    PP_Resource context_id, GLenum shadertype, GLenum precisiontype,
    GLint* range, GLint* precision) {
  GetGLES(
      context_id)->GetShaderPrecisionFormat(
          shadertype, precisiontype, range, precision);
}

void GetShaderSource(
    PP_Resource context_id, GLuint shader, GLsizei bufsize, GLsizei* length,
    char* source) {
  GetGLES(context_id)->GetShaderSource(shader, bufsize, length, source);
}

const GLubyte* GetString(PP_Resource context_id, GLenum name) {
  return GetGLES(context_id)->GetString(name);
}

void GetTexParameterfv(
    PP_Resource context_id, GLenum target, GLenum pname, GLfloat* params) {
  GetGLES(context_id)->GetTexParameterfv(target, pname, params);
}

void GetTexParameteriv(
    PP_Resource context_id, GLenum target, GLenum pname, GLint* params) {
  GetGLES(context_id)->GetTexParameteriv(target, pname, params);
}

void GetUniformfv(
    PP_Resource context_id, GLuint program, GLint location, GLfloat* params) {
  GetGLES(context_id)->GetUniformfv(program, location, params);
}

void GetUniformiv(
    PP_Resource context_id, GLuint program, GLint location, GLint* params) {
  GetGLES(context_id)->GetUniformiv(program, location, params);
}

GLint GetUniformLocation(
    PP_Resource context_id, GLuint program, const char* name) {
  return GetGLES(context_id)->GetUniformLocation(program, name);
}

void GetVertexAttribfv(
    PP_Resource context_id, GLuint index, GLenum pname, GLfloat* params) {
  GetGLES(context_id)->GetVertexAttribfv(index, pname, params);
}

void GetVertexAttribiv(
    PP_Resource context_id, GLuint index, GLenum pname, GLint* params) {
  GetGLES(context_id)->GetVertexAttribiv(index, pname, params);
}

void GetVertexAttribPointerv(
    PP_Resource context_id, GLuint index, GLenum pname, void** pointer) {
  GetGLES(context_id)->GetVertexAttribPointerv(index, pname, pointer);
}

void Hint(PP_Resource context_id, GLenum target, GLenum mode) {
  GetGLES(context_id)->Hint(target, mode);
}

GLboolean IsBuffer(PP_Resource context_id, GLuint buffer) {
  return GetGLES(context_id)->IsBuffer(buffer);
}

GLboolean IsEnabled(PP_Resource context_id, GLenum cap) {
  return GetGLES(context_id)->IsEnabled(cap);
}

GLboolean IsFramebuffer(PP_Resource context_id, GLuint framebuffer) {
  return GetGLES(context_id)->IsFramebuffer(framebuffer);
}

GLboolean IsProgram(PP_Resource context_id, GLuint program) {
  return GetGLES(context_id)->IsProgram(program);
}

GLboolean IsRenderbuffer(PP_Resource context_id, GLuint renderbuffer) {
  return GetGLES(context_id)->IsRenderbuffer(renderbuffer);
}

GLboolean IsShader(PP_Resource context_id, GLuint shader) {
  return GetGLES(context_id)->IsShader(shader);
}

GLboolean IsTexture(PP_Resource context_id, GLuint texture) {
  return GetGLES(context_id)->IsTexture(texture);
}

void LineWidth(PP_Resource context_id, GLfloat width) {
  GetGLES(context_id)->LineWidth(width);
}

void LinkProgram(PP_Resource context_id, GLuint program) {
  GetGLES(context_id)->LinkProgram(program);
}

void PixelStorei(PP_Resource context_id, GLenum pname, GLint param) {
  GetGLES(context_id)->PixelStorei(pname, param);
}

void PolygonOffset(PP_Resource context_id, GLfloat factor, GLfloat units) {
  GetGLES(context_id)->PolygonOffset(factor, units);
}

void ReadPixels(
    PP_Resource context_id, GLint x, GLint y, GLsizei width, GLsizei height,
    GLenum format, GLenum type, void* pixels) {
  GetGLES(context_id)->ReadPixels(x, y, width, height, format, type, pixels);
}

void ReleaseShaderCompiler(PP_Resource context_id) {
  GetGLES(context_id)->ReleaseShaderCompiler();
}

void RenderbufferStorage(
    PP_Resource context_id, GLenum target, GLenum internalformat, GLsizei width,
    GLsizei height) {
  GetGLES(
      context_id)->RenderbufferStorage(target, internalformat, width, height);
}

void SampleCoverage(PP_Resource context_id, GLclampf value, GLboolean invert) {
  GetGLES(context_id)->SampleCoverage(value, invert);
}

void Scissor(
    PP_Resource context_id, GLint x, GLint y, GLsizei width, GLsizei height) {
  GetGLES(context_id)->Scissor(x, y, width, height);
}

void ShaderBinary(
    PP_Resource context_id, GLsizei n, const GLuint* shaders,
    GLenum binaryformat, const void* binary, GLsizei length) {
  GetGLES(context_id)->ShaderBinary(n, shaders, binaryformat, binary, length);
}

void ShaderSource(
    PP_Resource context_id, GLuint shader, GLsizei count, const char** str,
    const GLint* length) {
  GetGLES(context_id)->ShaderSource(shader, count, str, length);
}

void StencilFunc(PP_Resource context_id, GLenum func, GLint ref, GLuint mask) {
  GetGLES(context_id)->StencilFunc(func, ref, mask);
}

void StencilFuncSeparate(
    PP_Resource context_id, GLenum face, GLenum func, GLint ref, GLuint mask) {
  GetGLES(context_id)->StencilFuncSeparate(face, func, ref, mask);
}

void StencilMask(PP_Resource context_id, GLuint mask) {
  GetGLES(context_id)->StencilMask(mask);
}

void StencilMaskSeparate(PP_Resource context_id, GLenum face, GLuint mask) {
  GetGLES(context_id)->StencilMaskSeparate(face, mask);
}

void StencilOp(
    PP_Resource context_id, GLenum fail, GLenum zfail, GLenum zpass) {
  GetGLES(context_id)->StencilOp(fail, zfail, zpass);
}

void StencilOpSeparate(
    PP_Resource context_id, GLenum face, GLenum fail, GLenum zfail,
    GLenum zpass) {
  GetGLES(context_id)->StencilOpSeparate(face, fail, zfail, zpass);
}

void TexImage2D(
    PP_Resource context_id, GLenum target, GLint level, GLint internalformat,
    GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  GetGLES(
      context_id)->TexImage2D(
          target, level, internalformat, width, height, border, format, type,
          pixels);
}

void TexParameterf(
    PP_Resource context_id, GLenum target, GLenum pname, GLfloat param) {
  GetGLES(context_id)->TexParameterf(target, pname, param);
}

void TexParameterfv(
    PP_Resource context_id, GLenum target, GLenum pname,
    const GLfloat* params) {
  GetGLES(context_id)->TexParameterfv(target, pname, params);
}

void TexParameteri(
    PP_Resource context_id, GLenum target, GLenum pname, GLint param) {
  GetGLES(context_id)->TexParameteri(target, pname, param);
}

void TexParameteriv(
    PP_Resource context_id, GLenum target, GLenum pname, const GLint* params) {
  GetGLES(context_id)->TexParameteriv(target, pname, params);
}

void TexSubImage2D(
    PP_Resource context_id, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
    const void* pixels) {
  GetGLES(
      context_id)->TexSubImage2D(
          target, level, xoffset, yoffset, width, height, format, type,
          pixels);
}

void Uniform1f(PP_Resource context_id, GLint location, GLfloat x) {
  GetGLES(context_id)->Uniform1f(location, x);
}

void Uniform1fv(
    PP_Resource context_id, GLint location, GLsizei count, const GLfloat* v) {
  GetGLES(context_id)->Uniform1fv(location, count, v);
}

void Uniform1i(PP_Resource context_id, GLint location, GLint x) {
  GetGLES(context_id)->Uniform1i(location, x);
}

void Uniform1iv(
    PP_Resource context_id, GLint location, GLsizei count, const GLint* v) {
  GetGLES(context_id)->Uniform1iv(location, count, v);
}

void Uniform2f(PP_Resource context_id, GLint location, GLfloat x, GLfloat y) {
  GetGLES(context_id)->Uniform2f(location, x, y);
}

void Uniform2fv(
    PP_Resource context_id, GLint location, GLsizei count, const GLfloat* v) {
  GetGLES(context_id)->Uniform2fv(location, count, v);
}

void Uniform2i(PP_Resource context_id, GLint location, GLint x, GLint y) {
  GetGLES(context_id)->Uniform2i(location, x, y);
}

void Uniform2iv(
    PP_Resource context_id, GLint location, GLsizei count, const GLint* v) {
  GetGLES(context_id)->Uniform2iv(location, count, v);
}

void Uniform3f(
    PP_Resource context_id, GLint location, GLfloat x, GLfloat y, GLfloat z) {
  GetGLES(context_id)->Uniform3f(location, x, y, z);
}

void Uniform3fv(
    PP_Resource context_id, GLint location, GLsizei count, const GLfloat* v) {
  GetGLES(context_id)->Uniform3fv(location, count, v);
}

void Uniform3i(
    PP_Resource context_id, GLint location, GLint x, GLint y, GLint z) {
  GetGLES(context_id)->Uniform3i(location, x, y, z);
}

void Uniform3iv(
    PP_Resource context_id, GLint location, GLsizei count, const GLint* v) {
  GetGLES(context_id)->Uniform3iv(location, count, v);
}

void Uniform4f(
    PP_Resource context_id, GLint location, GLfloat x, GLfloat y, GLfloat z,
    GLfloat w) {
  GetGLES(context_id)->Uniform4f(location, x, y, z, w);
}

void Uniform4fv(
    PP_Resource context_id, GLint location, GLsizei count, const GLfloat* v) {
  GetGLES(context_id)->Uniform4fv(location, count, v);
}

void Uniform4i(
    PP_Resource context_id, GLint location, GLint x, GLint y, GLint z,
    GLint w) {
  GetGLES(context_id)->Uniform4i(location, x, y, z, w);
}

void Uniform4iv(
    PP_Resource context_id, GLint location, GLsizei count, const GLint* v) {
  GetGLES(context_id)->Uniform4iv(location, count, v);
}

void UniformMatrix2fv(
    PP_Resource context_id, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  GetGLES(context_id)->UniformMatrix2fv(location, count, transpose, value);
}

void UniformMatrix3fv(
    PP_Resource context_id, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  GetGLES(context_id)->UniformMatrix3fv(location, count, transpose, value);
}

void UniformMatrix4fv(
    PP_Resource context_id, GLint location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  GetGLES(context_id)->UniformMatrix4fv(location, count, transpose, value);
}

void UseProgram(PP_Resource context_id, GLuint program) {
  GetGLES(context_id)->UseProgram(program);
}

void ValidateProgram(PP_Resource context_id, GLuint program) {
  GetGLES(context_id)->ValidateProgram(program);
}

void VertexAttrib1f(PP_Resource context_id, GLuint indx, GLfloat x) {
  GetGLES(context_id)->VertexAttrib1f(indx, x);
}

void VertexAttrib1fv(
    PP_Resource context_id, GLuint indx, const GLfloat* values) {
  GetGLES(context_id)->VertexAttrib1fv(indx, values);
}

void VertexAttrib2f(
    PP_Resource context_id, GLuint indx, GLfloat x, GLfloat y) {
  GetGLES(context_id)->VertexAttrib2f(indx, x, y);
}

void VertexAttrib2fv(
    PP_Resource context_id, GLuint indx, const GLfloat* values) {
  GetGLES(context_id)->VertexAttrib2fv(indx, values);
}

void VertexAttrib3f(
    PP_Resource context_id, GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  GetGLES(context_id)->VertexAttrib3f(indx, x, y, z);
}

void VertexAttrib3fv(
    PP_Resource context_id, GLuint indx, const GLfloat* values) {
  GetGLES(context_id)->VertexAttrib3fv(indx, values);
}

void VertexAttrib4f(
    PP_Resource context_id, GLuint indx, GLfloat x, GLfloat y, GLfloat z,
    GLfloat w) {
  GetGLES(context_id)->VertexAttrib4f(indx, x, y, z, w);
}

void VertexAttrib4fv(
    PP_Resource context_id, GLuint indx, const GLfloat* values) {
  GetGLES(context_id)->VertexAttrib4fv(indx, values);
}

void VertexAttribPointer(
    PP_Resource context_id, GLuint indx, GLint size, GLenum type,
    GLboolean normalized, GLsizei stride, const void* ptr) {
  GetGLES(
      context_id)->VertexAttribPointer(
          indx, size, type, normalized, stride, ptr);
}

void Viewport(
    PP_Resource context_id, GLint x, GLint y, GLsizei width, GLsizei height) {
  GetGLES(context_id)->Viewport(x, y, width, height);
}

void BlitFramebufferEXT(
    PP_Resource context_id, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
    GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask,
    GLenum filter) {
  GetGLES(
      context_id)->BlitFramebufferEXT(
          srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask,
          filter);
}

void RenderbufferStorageMultisampleEXT(
    PP_Resource context_id, GLenum target, GLsizei samples,
    GLenum internalformat, GLsizei width, GLsizei height) {
  GetGLES(
      context_id)->RenderbufferStorageMultisampleEXT(
          target, samples, internalformat, width, height);
}

void GenQueriesEXT(PP_Resource context_id, GLsizei n, GLuint* queries) {
  GetGLES(context_id)->GenQueriesEXT(n, queries);
}

void DeleteQueriesEXT(
    PP_Resource context_id, GLsizei n, const GLuint* queries) {
  GetGLES(context_id)->DeleteQueriesEXT(n, queries);
}

GLboolean IsQueryEXT(PP_Resource context_id, GLuint id) {
  return GetGLES(context_id)->IsQueryEXT(id);
}

void BeginQueryEXT(PP_Resource context_id, GLenum target, GLuint id) {
  GetGLES(context_id)->BeginQueryEXT(target, id);
}

void EndQueryEXT(PP_Resource context_id, GLenum target) {
  GetGLES(context_id)->EndQueryEXT(target);
}

void GetQueryivEXT(
    PP_Resource context_id, GLenum target, GLenum pname, GLint* params) {
  GetGLES(context_id)->GetQueryivEXT(target, pname, params);
}

void GetQueryObjectuivEXT(
    PP_Resource context_id, GLuint id, GLenum pname, GLuint* params) {
  GetGLES(context_id)->GetQueryObjectuivEXT(id, pname, params);
}

GLboolean EnableFeatureCHROMIUM(PP_Resource context_id, const char* feature) {
  return GetGLES(context_id)->EnableFeatureCHROMIUM(feature);
}

void* MapBufferSubDataCHROMIUM(
    PP_Resource context_id, GLuint target, GLintptr offset, GLsizeiptr size,
    GLenum access) {
  return GetGLES(
      context_id)->MapBufferSubDataCHROMIUM(target, offset, size, access);
}

void UnmapBufferSubDataCHROMIUM(PP_Resource context_id, const void* mem) {
  GetGLES(context_id)->UnmapBufferSubDataCHROMIUM(mem);
}

void* MapTexSubImage2DCHROMIUM(
    PP_Resource context_id, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
    GLenum access) {
  return GetGLES(
      context_id)->MapTexSubImage2DCHROMIUM(
          target, level, xoffset, yoffset, width, height, format, type,
          access);
}

void UnmapTexSubImage2DCHROMIUM(PP_Resource context_id, const void* mem) {
  GetGLES(context_id)->UnmapTexSubImage2DCHROMIUM(mem);
}

void DrawArraysInstancedANGLE(
    PP_Resource context_id, GLenum mode, GLint first, GLsizei count,
    GLsizei primcount) {
  GetGLES(context_id)->DrawArraysInstancedANGLE(mode, first, count, primcount);
}

void DrawElementsInstancedANGLE(
    PP_Resource context_id, GLenum mode, GLsizei count, GLenum type,
    const void* indices, GLsizei primcount) {
  GetGLES(
      context_id)->DrawElementsInstancedANGLE(
          mode, count, type, indices, primcount);
}

void VertexAttribDivisorANGLE(
    PP_Resource context_id, GLuint index, GLuint divisor) {
  GetGLES(context_id)->VertexAttribDivisorANGLE(index, divisor);
}

}  // namespace
const PPB_OpenGLES2* PPB_OpenGLES2_Shared::GetInterface() {
  static const struct PPB_OpenGLES2 ppb_opengles2 = {
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
  return &ppb_opengles2;
}
const PPB_OpenGLES2InstancedArrays* PPB_OpenGLES2_Shared::GetInstancedArraysInterface() {  // NOLINT
  static const struct PPB_OpenGLES2InstancedArrays ppb_opengles2 = {
    &DrawArraysInstancedANGLE,
    &DrawElementsInstancedANGLE,
    &VertexAttribDivisorANGLE
  };
  return &ppb_opengles2;
}
const PPB_OpenGLES2FramebufferBlit* PPB_OpenGLES2_Shared::GetFramebufferBlitInterface() {  // NOLINT
  static const struct PPB_OpenGLES2FramebufferBlit ppb_opengles2 = {
    &BlitFramebufferEXT
  };
  return &ppb_opengles2;
}
const PPB_OpenGLES2FramebufferMultisample* PPB_OpenGLES2_Shared::GetFramebufferMultisampleInterface() {  // NOLINT
  static const struct PPB_OpenGLES2FramebufferMultisample ppb_opengles2 = {
    &RenderbufferStorageMultisampleEXT
  };
  return &ppb_opengles2;
}
const PPB_OpenGLES2ChromiumEnableFeature* PPB_OpenGLES2_Shared::GetChromiumEnableFeatureInterface() {  // NOLINT
  static const struct PPB_OpenGLES2ChromiumEnableFeature ppb_opengles2 = {
    &EnableFeatureCHROMIUM
  };
  return &ppb_opengles2;
}
const PPB_OpenGLES2ChromiumMapSub* PPB_OpenGLES2_Shared::GetChromiumMapSubInterface() {  // NOLINT
  static const struct PPB_OpenGLES2ChromiumMapSub ppb_opengles2 = {
    &MapBufferSubDataCHROMIUM,
    &UnmapBufferSubDataCHROMIUM,
    &MapTexSubImage2DCHROMIUM,
    &UnmapTexSubImage2DCHROMIUM
  };
  return &ppb_opengles2;
}
const PPB_OpenGLES2Query* PPB_OpenGLES2_Shared::GetQueryInterface() {
  static const struct PPB_OpenGLES2Query ppb_opengles2 = {
    &GenQueriesEXT,
    &DeleteQueriesEXT,
    &IsQueryEXT,
    &BeginQueryEXT,
    &EndQueryEXT,
    &GetQueryivEXT,
    &GetQueryObjectuivEXT
  };
  return &ppb_opengles2;
}
}  // namespace ppapi
