// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated. DO NOT EDIT!

// This interface is used to access common and lite profile OpenGL ES 2.0
// functions.
#ifndef PPAPI_C_DEV_PPB_OPENGLES_DEV_H_
#define PPAPI_C_DEV_PPB_OPENGLES_DEV_H_

#include "ppapi/GLES2/khrplatform.h"

#define PPB_OPENGLES_DEV_INTERFACE "PPB_OpenGLES(Dev);2.0"

typedef unsigned int GLenum;
typedef void GLvoid;
typedef khronos_intptr_t GLintptr;
typedef int GLsizei;
typedef khronos_ssize_t GLsizeiptr;
typedef int GLint;
typedef unsigned char GLboolean;
typedef unsigned int GLuint;
typedef unsigned int GLbitfield;
typedef short GLshort;
typedef float GLfloat;
typedef float GLclampf;
typedef signed char GLbyte;
typedef unsigned char GLubyte;
typedef int GLfixed;
typedef unsigned short GLushort;
typedef int GLclampx;

struct PPB_OpenGLES_Dev {
  void (*ActiveTexture)(GLenum texture);
  void (*AttachShader)(GLuint program, GLuint shader);
  void (*BindAttribLocation)(GLuint program, GLuint index, const char* name);
  void (*BindBuffer)(GLenum target, GLuint buffer);
  void (*BindFramebuffer)(GLenum target, GLuint framebuffer);
  void (*BindRenderbuffer)(GLenum target, GLuint renderbuffer);
  void (*BindTexture)(GLenum target, GLuint texture);
  void (*BlendColor)(
      GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
  void (*BlendEquation)(GLenum mode);
  void (*BlendEquationSeparate)(GLenum modeRGB, GLenum modeAlpha);
  void (*BlendFunc)(GLenum sfactor, GLenum dfactor);
  void (*BlendFuncSeparate)(
      GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
  void (*BufferData)(
      GLenum target, GLsizeiptr size, const void* data, GLenum usage);
  void (*BufferSubData)(
      GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
  GLenum (*CheckFramebufferStatus)(GLenum target);
  void (*Clear)(GLbitfield mask);
  void (*ClearColor)(
      GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
  void (*ClearDepthf)(GLclampf depth);
  void (*ClearStencil)(GLint s);
  void (*ColorMask)(
      GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
  void (*CompileShader)(GLuint shader);
  void (*CompressedTexImage2D)(
      GLenum target, GLint level, GLenum internalformat, GLsizei width,
      GLsizei height, GLint border, GLsizei imageSize, const void* data);
  void (*CompressedTexSubImage2D)(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLsizei imageSize, const void* data);
  void (*CopyTexImage2D)(
      GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
      GLsizei width, GLsizei height, GLint border);
  void (*CopyTexSubImage2D)(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x,
      GLint y, GLsizei width, GLsizei height);
  GLuint (*CreateProgram)();
  GLuint (*CreateShader)(GLenum type);
  void (*CullFace)(GLenum mode);
  void (*DeleteBuffers)(GLsizei n, const GLuint* buffers);
  void (*DeleteFramebuffers)(GLsizei n, const GLuint* framebuffers);
  void (*DeleteProgram)(GLuint program);
  void (*DeleteRenderbuffers)(GLsizei n, const GLuint* renderbuffers);
  void (*DeleteShader)(GLuint shader);
  void (*DeleteTextures)(GLsizei n, const GLuint* textures);
  void (*DepthFunc)(GLenum func);
  void (*DepthMask)(GLboolean flag);
  void (*DepthRangef)(GLclampf zNear, GLclampf zFar);
  void (*DetachShader)(GLuint program, GLuint shader);
  void (*Disable)(GLenum cap);
  void (*DisableVertexAttribArray)(GLuint index);
  void (*DrawArrays)(GLenum mode, GLint first, GLsizei count);
  void (*DrawElements)(
      GLenum mode, GLsizei count, GLenum type, const void* indices);
  void (*Enable)(GLenum cap);
  void (*EnableVertexAttribArray)(GLuint index);
  void (*Finish)();
  void (*Flush)();
  void (*FramebufferRenderbuffer)(
      GLenum target, GLenum attachment, GLenum renderbuffertarget,
      GLuint renderbuffer);
  void (*FramebufferTexture2D)(
      GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
      GLint level);
  void (*FrontFace)(GLenum mode);
  void (*GenBuffers)(GLsizei n, GLuint* buffers);
  void (*GenerateMipmap)(GLenum target);
  void (*GenFramebuffers)(GLsizei n, GLuint* framebuffers);
  void (*GenRenderbuffers)(GLsizei n, GLuint* renderbuffers);
  void (*GenTextures)(GLsizei n, GLuint* textures);
  void (*GetActiveAttrib)(
      GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
      GLint* size, GLenum* type, char* name);
  void (*GetActiveUniform)(
      GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
      GLint* size, GLenum* type, char* name);
  void (*GetAttachedShaders)(
      GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders);
  GLint (*GetAttribLocation)(GLuint program, const char* name);
  void (*GetBooleanv)(GLenum pname, GLboolean* params);
  void (*GetBufferParameteriv)(GLenum target, GLenum pname, GLint* params);
  GLenum (*GetError)();
  void (*GetFloatv)(GLenum pname, GLfloat* params);
  void (*GetFramebufferAttachmentParameteriv)(
      GLenum target, GLenum attachment, GLenum pname, GLint* params);
  void (*GetIntegerv)(GLenum pname, GLint* params);
  void (*GetProgramiv)(GLuint program, GLenum pname, GLint* params);
  void (*GetProgramInfoLog)(
      GLuint program, GLsizei bufsize, GLsizei* length, char* infolog);
  void (*GetRenderbufferParameteriv)(
      GLenum target, GLenum pname, GLint* params);
  void (*GetShaderiv)(GLuint shader, GLenum pname, GLint* params);
  void (*GetShaderInfoLog)(
      GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog);
  void (*GetShaderPrecisionFormat)(
      GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);
  void (*GetShaderSource)(
      GLuint shader, GLsizei bufsize, GLsizei* length, char* source);
  const GLubyte* (*GetString)(GLenum name);
  void (*GetTexParameterfv)(GLenum target, GLenum pname, GLfloat* params);
  void (*GetTexParameteriv)(GLenum target, GLenum pname, GLint* params);
  void (*GetUniformfv)(GLuint program, GLint location, GLfloat* params);
  void (*GetUniformiv)(GLuint program, GLint location, GLint* params);
  GLint (*GetUniformLocation)(GLuint program, const char* name);
  void (*GetVertexAttribfv)(GLuint index, GLenum pname, GLfloat* params);
  void (*GetVertexAttribiv)(GLuint index, GLenum pname, GLint* params);
  void (*GetVertexAttribPointerv)(GLuint index, GLenum pname, void** pointer);
  void (*Hint)(GLenum target, GLenum mode);
  GLboolean (*IsBuffer)(GLuint buffer);
  GLboolean (*IsEnabled)(GLenum cap);
  GLboolean (*IsFramebuffer)(GLuint framebuffer);
  GLboolean (*IsProgram)(GLuint program);
  GLboolean (*IsRenderbuffer)(GLuint renderbuffer);
  GLboolean (*IsShader)(GLuint shader);
  GLboolean (*IsTexture)(GLuint texture);
  void (*LineWidth)(GLfloat width);
  void (*LinkProgram)(GLuint program);
  void (*PixelStorei)(GLenum pname, GLint param);
  void (*PolygonOffset)(GLfloat factor, GLfloat units);
  void (*ReadPixels)(
      GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
      GLenum type, void* pixels);
  void (*ReleaseShaderCompiler)();
  void (*RenderbufferStorage)(
      GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
  void (*SampleCoverage)(GLclampf value, GLboolean invert);
  void (*Scissor)(GLint x, GLint y, GLsizei width, GLsizei height);
  void (*ShaderBinary)(
      GLsizei n, const GLuint* shaders, GLenum binaryformat, const void* binary,
      GLsizei length);
  void (*ShaderSource)(
      GLuint shader, GLsizei count, const char** str, const GLint* length);
  void (*StencilFunc)(GLenum func, GLint ref, GLuint mask);
  void (*StencilFuncSeparate)(
      GLenum face, GLenum func, GLint ref, GLuint mask);
  void (*StencilMask)(GLuint mask);
  void (*StencilMaskSeparate)(GLenum face, GLuint mask);
  void (*StencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
  void (*StencilOpSeparate)(
      GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
  void (*TexImage2D)(
      GLenum target, GLint level, GLint internalformat, GLsizei width,
      GLsizei height, GLint border, GLenum format, GLenum type,
      const void* pixels);
  void (*TexParameterf)(GLenum target, GLenum pname, GLfloat param);
  void (*TexParameterfv)(GLenum target, GLenum pname, const GLfloat* params);
  void (*TexParameteri)(GLenum target, GLenum pname, GLint param);
  void (*TexParameteriv)(GLenum target, GLenum pname, const GLint* params);
  void (*TexSubImage2D)(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLenum type, const void* pixels);
  void (*Uniform1f)(GLint location, GLfloat x);
  void (*Uniform1fv)(GLint location, GLsizei count, const GLfloat* v);
  void (*Uniform1i)(GLint location, GLint x);
  void (*Uniform1iv)(GLint location, GLsizei count, const GLint* v);
  void (*Uniform2f)(GLint location, GLfloat x, GLfloat y);
  void (*Uniform2fv)(GLint location, GLsizei count, const GLfloat* v);
  void (*Uniform2i)(GLint location, GLint x, GLint y);
  void (*Uniform2iv)(GLint location, GLsizei count, const GLint* v);
  void (*Uniform3f)(GLint location, GLfloat x, GLfloat y, GLfloat z);
  void (*Uniform3fv)(GLint location, GLsizei count, const GLfloat* v);
  void (*Uniform3i)(GLint location, GLint x, GLint y, GLint z);
  void (*Uniform3iv)(GLint location, GLsizei count, const GLint* v);
  void (*Uniform4f)(
      GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
  void (*Uniform4fv)(GLint location, GLsizei count, const GLfloat* v);
  void (*Uniform4i)(GLint location, GLint x, GLint y, GLint z, GLint w);
  void (*Uniform4iv)(GLint location, GLsizei count, const GLint* v);
  void (*UniformMatrix2fv)(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value);
  void (*UniformMatrix3fv)(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value);
  void (*UniformMatrix4fv)(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value);
  void (*UseProgram)(GLuint program);
  void (*ValidateProgram)(GLuint program);
  void (*VertexAttrib1f)(GLuint indx, GLfloat x);
  void (*VertexAttrib1fv)(GLuint indx, const GLfloat* values);
  void (*VertexAttrib2f)(GLuint indx, GLfloat x, GLfloat y);
  void (*VertexAttrib2fv)(GLuint indx, const GLfloat* values);
  void (*VertexAttrib3f)(GLuint indx, GLfloat x, GLfloat y, GLfloat z);
  void (*VertexAttrib3fv)(GLuint indx, const GLfloat* values);
  void (*VertexAttrib4f)(
      GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
  void (*VertexAttrib4fv)(GLuint indx, const GLfloat* values);
  void (*VertexAttribPointer)(
      GLuint indx, GLint size, GLenum type, GLboolean normalized,
      GLsizei stride, const void* ptr);
  void (*Viewport)(GLint x, GLint y, GLsizei width, GLsizei height);
  void (*SwapBuffers)();
  GLuint (*GetMaxValueInBuffer)(
      GLuint buffer_id, GLsizei count, GLenum type, GLuint offset);
  void (*GenSharedIds)(
      GLuint namespace_id, GLuint id_offset, GLsizei n, GLuint* ids);
  void (*DeleteSharedIds)(GLuint namespace_id, GLsizei n, const GLuint* ids);
  void (*RegisterSharedIds)(GLuint namespace_id, GLsizei n, const GLuint* ids);
  GLboolean (*CommandBufferEnable)(const char* feature);
  void* (*MapBufferSubData)(
      GLuint target, GLintptr offset, GLsizeiptr size, GLenum access);
  void (*UnmapBufferSubData)(const void* mem);
  void* (*MapTexSubImage2D)(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLenum type, GLenum access);
  void (*UnmapTexSubImage2D)(const void* mem);
  void (*CopyTextureToParentTexture)(
      GLuint client_child_id, GLuint client_parent_id);
};

#endif  // PPAPI_C_DEV_PPB_OPENGLES_DEV_H_

