// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_gl_api_implementation.h"

#include <algorithm>
#include <vector>

#include "base/string_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_state_restorer.h"
#include "ui/gl/gl_surface.h"

namespace gfx {

RealGLApi* g_real_gl;

void InitializeGLBindingsGL() {
  g_driver_gl.InitializeBindings();
  if (!g_real_gl) {
    g_real_gl = new RealGLApi();
  }
  g_real_gl->Initialize(&g_driver_gl);
  SetGLToRealGLApi();
}

GLApi* GetCurrentGLApi() {
  return g_current_gl_context;
}

void SetGLApi(GLApi* api) {
  g_current_gl_context = api;
}

void SetGLToRealGLApi() {
  SetGLApi(g_real_gl);
}

void InitializeGLExtensionBindingsGL(GLContext* context) {
  g_driver_gl.InitializeExtensionBindings(context);
}

void InitializeDebugGLBindingsGL() {
  g_driver_gl.InitializeDebugBindings();
}

void ClearGLBindingsGL() {
  if (g_real_gl) {
    delete g_real_gl;
    g_real_gl = NULL;
  }
  g_current_gl_context = NULL;
  g_driver_gl.ClearBindings();
}

GLApi::GLApi() {
}

GLApi::~GLApi() {
}

RealGLApi::RealGLApi() {
}

RealGLApi::~RealGLApi() {
}

void RealGLApi::Initialize(DriverGL* driver) {
  driver_ = driver;
}

VirtualGLApi::VirtualGLApi()
    : driver_(NULL),
      real_context_(NULL),
      current_context_(NULL) {
}

VirtualGLApi::~VirtualGLApi() {
}

void VirtualGLApi::Initialize(DriverGL* driver, GLContext* real_context) {
  driver_ = driver;
  real_context_ = real_context;

  DCHECK(real_context->IsCurrent(NULL));
  std::string ext_string(
      reinterpret_cast<const char*>(driver_->fn.glGetStringFn(GL_EXTENSIONS)));
  std::vector<std::string> ext;
  Tokenize(ext_string, " ", &ext);

  std::vector<std::string>::iterator it;
  // We can't support GL_EXT_occlusion_query_boolean which is
  // based on GL_ARB_occlusion_query without a lot of work virtualizing
  // queries.
  it = std::find(ext.begin(), ext.end(), "GL_EXT_occlusion_query_boolean");
  if (it != ext.end())
    ext.erase(it);

  extensions_ = JoinString(ext, " ");
}

bool VirtualGLApi::MakeCurrent(GLContext* virtual_context, GLSurface* surface) {
  bool switched_contexts = g_current_gl_context != this;
  GLSurface* current_surface = GLSurface::GetCurrent();
  if (switched_contexts || surface != current_surface) {
    if (!switched_contexts && current_surface &&
        virtual_context->IsCurrent(surface)) {
      // MakeCurrent 'lite' path that avoids potentially expensive MakeCurrent()
      // calls if the GLSurface uses the same underlying surface or renders to
      // an FBO.
      if (!surface->OnMakeCurrent(real_context_)) {
        LOG(ERROR) << "Could not make GLSurface current.";
        return false;
      }
    } else if (!real_context_->MakeCurrent(surface)) {
      return false;
    }
  }

  DCHECK(GLSurface::GetCurrent());
  DCHECK(real_context_->IsCurrent(GLSurface::GetCurrent()));
  DCHECK(virtual_context->IsCurrent(surface));

  if (switched_contexts || virtual_context != current_context_) {
    // There should be no errors from the previous context leaking into the
    // new context.
    DCHECK_EQ(glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

    current_context_ = virtual_context;
    // Set all state that is different from the real state
    // NOTE: !!! This is a temporary implementation that just restores all
    // state to let us test that it works.
    // TODO: ASAP, change this to something that only restores the state
    // needed for individual GL calls.
    GLApi* temp = GetCurrentGLApi();
    SetGLToRealGLApi();
    virtual_context->GetGLStateRestorer()->RestoreState();
    SetGLApi(temp);
  }
  SetGLApi(this);
  return true;
}

void VirtualGLApi::OnDestroyVirtualContext(GLContext* virtual_context) {
  if (current_context_ == virtual_context)
    current_context_ = NULL;
}

void VirtualGLApi::glActiveTextureFn(GLenum texture) {
  driver_->fn.glActiveTextureFn(texture);
}

void VirtualGLApi::glAttachShaderFn(GLuint program, GLuint shader) {
  driver_->fn.glAttachShaderFn(program, shader);
}

void VirtualGLApi::glBeginQueryFn(GLenum target, GLuint id) {
  driver_->fn.glBeginQueryFn(target, id);
}

void VirtualGLApi::glBeginQueryARBFn(GLenum target, GLuint id) {
  driver_->fn.glBeginQueryARBFn(target, id);
}

void VirtualGLApi::glBindAttribLocationFn(
    GLuint program, GLuint index, const char* name) {
  driver_->fn.glBindAttribLocationFn(program, index, name);
}

void VirtualGLApi::glBindBufferFn(GLenum target, GLuint buffer) {
  driver_->fn.glBindBufferFn(target, buffer);
}

void VirtualGLApi::glBindFragDataLocationFn(
    GLuint program, GLuint colorNumber, const char* name) {
  driver_->fn.glBindFragDataLocationFn(program, colorNumber, name);
}

void VirtualGLApi::glBindFragDataLocationIndexedFn(
    GLuint program, GLuint colorNumber, GLuint index, const char* name) {
  driver_->fn.glBindFragDataLocationIndexedFn(
      program, colorNumber, index, name);
}

void VirtualGLApi::glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) {
  driver_->fn.glBindFramebufferEXTFn(target, framebuffer);
}

void VirtualGLApi::glBindRenderbufferEXTFn(GLenum target, GLuint renderbuffer) {
  driver_->fn.glBindRenderbufferEXTFn(target, renderbuffer);
}

void VirtualGLApi::glBindTextureFn(GLenum target, GLuint texture) {
  driver_->fn.glBindTextureFn(target, texture);
}

void VirtualGLApi::glBlendColorFn(
    GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  driver_->fn.glBlendColorFn(red, green, blue, alpha);
}

void VirtualGLApi::glBlendEquationFn( GLenum mode ) {
  driver_->fn.glBlendEquationFn( mode );
}

void VirtualGLApi::glBlendEquationSeparateFn(GLenum modeRGB, GLenum modeAlpha) {
  driver_->fn.glBlendEquationSeparateFn(modeRGB, modeAlpha);
}

void VirtualGLApi::glBlendFuncFn(GLenum sfactor, GLenum dfactor) {
  driver_->fn.glBlendFuncFn(sfactor, dfactor);
}

void VirtualGLApi::glBlendFuncSeparateFn(
    GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
  driver_->fn.glBlendFuncSeparateFn(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void VirtualGLApi::glBlitFramebufferEXTFn(
    GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
    GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
    GLbitfield mask, GLenum filter) {
  driver_->fn.glBlitFramebufferEXTFn(
      srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void VirtualGLApi::glBlitFramebufferANGLEFn(
    GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
    GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
    GLbitfield mask, GLenum filter) {
  driver_->fn.glBlitFramebufferANGLEFn(
      srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void VirtualGLApi::glBufferDataFn(
    GLenum target, GLsizei size, const void* data, GLenum usage) {
  driver_->fn.glBufferDataFn(target, size, data, usage);
}

void VirtualGLApi::glBufferSubDataFn(
    GLenum target, GLint offset, GLsizei size, const void* data) {
  driver_->fn.glBufferSubDataFn(target, offset, size, data);
}

GLenum VirtualGLApi::glCheckFramebufferStatusEXTFn(GLenum target) {
  return driver_->fn.glCheckFramebufferStatusEXTFn(target);
}

void VirtualGLApi::glClearFn(GLbitfield mask) {
  driver_->fn.glClearFn(mask);
}

void VirtualGLApi::glClearColorFn(
    GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  driver_->fn.glClearColorFn(red, green, blue, alpha);
}

void VirtualGLApi::glClearDepthFn(GLclampd depth) {
  driver_->fn.glClearDepthFn(depth);
}

void VirtualGLApi::glClearDepthfFn(GLclampf depth) {
  driver_->fn.glClearDepthfFn(depth);
}

void VirtualGLApi::glClearStencilFn(GLint s) {
  driver_->fn.glClearStencilFn(s);
}

void VirtualGLApi::glColorMaskFn(
    GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
  driver_->fn.glColorMaskFn(red, green, blue, alpha);
}

void VirtualGLApi::glCompileShaderFn(GLuint shader) {
  driver_->fn.glCompileShaderFn(shader);
}

void VirtualGLApi::glCompressedTexImage2DFn(
    GLenum target, GLint level, GLenum internalformat,
    GLsizei width, GLsizei height, GLint border,
    GLsizei imageSize, const void* data) {
  driver_->fn.glCompressedTexImage2DFn(
      target, level, internalformat, width, height, border, imageSize, data);
}

void VirtualGLApi::glCompressedTexSubImage2DFn(
    GLenum target, GLint level, GLint xoffset, GLint yoffset,
    GLsizei width, GLsizei height, GLenum format,
    GLsizei imageSize, const void* data) {
  driver_->fn.glCompressedTexSubImage2DFn(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

void VirtualGLApi::glCopyTexImage2DFn(
    GLenum target, GLint level, GLenum internalformat,
    GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
  driver_->fn.glCopyTexImage2DFn(
      target, level, internalformat, x, y, width, height, border);
}

void VirtualGLApi::glCopyTexSubImage2DFn(
    GLenum target, GLint level, GLint xoffset, GLint yoffset,
    GLint x, GLint y, GLsizei width, GLsizei height) {
  driver_->fn.glCopyTexSubImage2DFn(
      target, level, xoffset, yoffset, x, y, width, height);
}

GLuint VirtualGLApi::glCreateProgramFn(void) {
  return driver_->fn.glCreateProgramFn();
}

GLuint VirtualGLApi::glCreateShaderFn(GLenum type) {
  return driver_->fn.glCreateShaderFn(type);
}

void VirtualGLApi::glCullFaceFn(GLenum mode) {
  driver_->fn.glCullFaceFn(mode);
}

void VirtualGLApi::glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) {
  driver_->fn.glDeleteBuffersARBFn(n, buffers);
}

void VirtualGLApi::glDeleteFramebuffersEXTFn(
    GLsizei n, const GLuint* framebuffers) {
  driver_->fn.glDeleteFramebuffersEXTFn(n, framebuffers);
}

void VirtualGLApi::glDeleteProgramFn(GLuint program) {
  driver_->fn.glDeleteProgramFn(program);
}

void VirtualGLApi::glDeleteQueriesFn(GLsizei n, const GLuint* ids) {
  driver_->fn.glDeleteQueriesFn(n, ids);
}

void VirtualGLApi::glDeleteQueriesARBFn(GLsizei n, const GLuint* ids) {
  driver_->fn.glDeleteQueriesARBFn(n, ids);
}

void VirtualGLApi::glDeleteRenderbuffersEXTFn(
    GLsizei n, const GLuint* renderbuffers) {
  driver_->fn.glDeleteRenderbuffersEXTFn(n, renderbuffers);
}

void VirtualGLApi::glDeleteShaderFn(GLuint shader) {
  driver_->fn.glDeleteShaderFn(shader);
}

void VirtualGLApi::glDeleteTexturesFn(GLsizei n, const GLuint* textures) {
  driver_->fn.glDeleteTexturesFn(n, textures);
}

void VirtualGLApi::glDepthFuncFn(GLenum func) {
  driver_->fn.glDepthFuncFn(func);
}

void VirtualGLApi::glDepthMaskFn(GLboolean flag) {
  driver_->fn.glDepthMaskFn(flag);
}

void VirtualGLApi::glDepthRangeFn(GLclampd zNear, GLclampd zFar) {
  driver_->fn.glDepthRangeFn(zNear, zFar);
}

void VirtualGLApi::glDepthRangefFn(GLclampf zNear, GLclampf zFar) {
  driver_->fn.glDepthRangefFn(zNear, zFar);
}

void VirtualGLApi::glDetachShaderFn(GLuint program, GLuint shader) {
  driver_->fn.glDetachShaderFn(program, shader);
}

void VirtualGLApi::glDisableFn(GLenum cap) {
  driver_->fn.glDisableFn(cap);
}

void VirtualGLApi::glDisableVertexAttribArrayFn(GLuint index) {
  driver_->fn.glDisableVertexAttribArrayFn(index);
}

void VirtualGLApi::glDiscardFramebufferEXTFn(GLenum target,
                                           GLsizei numAttachments,
                                           const GLenum* attachments) {
  driver_->fn.glDiscardFramebufferEXTFn(target, numAttachments, attachments);
}

void VirtualGLApi::glDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  driver_->fn.glDrawArraysFn(mode, first, count);
}

void VirtualGLApi::glDrawBufferFn(GLenum mode) {
  driver_->fn.glDrawBufferFn(mode);
}

void VirtualGLApi::glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) {
  driver_->fn.glDrawBuffersARBFn(n, bufs);
}

void VirtualGLApi::glDrawElementsFn(
    GLenum mode, GLsizei count, GLenum type, const void* indices) {
  driver_->fn.glDrawElementsFn(mode, count, type, indices);
}

void VirtualGLApi::glEGLImageTargetTexture2DOESFn(
    GLenum target, GLeglImageOES image) {
  driver_->fn.glEGLImageTargetTexture2DOESFn(target, image);
}

void VirtualGLApi::glEGLImageTargetRenderbufferStorageOESFn(
    GLenum target, GLeglImageOES image) {
  driver_->fn.glEGLImageTargetRenderbufferStorageOESFn(target, image);
}

void VirtualGLApi::glEnableFn(GLenum cap) {
  driver_->fn.glEnableFn(cap);
}

void VirtualGLApi::glEnableVertexAttribArrayFn(GLuint index) {
  driver_->fn.glEnableVertexAttribArrayFn(index);
}

void VirtualGLApi::glEndQueryFn(GLenum target) {
  driver_->fn.glEndQueryFn(target);
}

void VirtualGLApi::glEndQueryARBFn(GLenum target) {
  driver_->fn.glEndQueryARBFn(target);
}

void VirtualGLApi::glFinishFn(void) {
  driver_->fn.glFinishFn();
}

void VirtualGLApi::glFlushFn(void) {
  driver_->fn.glFlushFn();
}

void VirtualGLApi::glFramebufferRenderbufferEXTFn(
    GLenum target, GLenum attachment,
    GLenum renderbuffertarget, GLuint renderbuffer) {
  driver_->fn.glFramebufferRenderbufferEXTFn(
      target, attachment, renderbuffertarget, renderbuffer);
}

void VirtualGLApi::glFramebufferTexture2DEXTFn(
    GLenum target, GLenum attachment,
    GLenum textarget, GLuint texture, GLint level) {
  driver_->fn.glFramebufferTexture2DEXTFn(
      target, attachment, textarget, texture, level);
}

void VirtualGLApi::glFrontFaceFn(GLenum mode) {
  driver_->fn.glFrontFaceFn(mode);
}

void VirtualGLApi::glGenBuffersARBFn(GLsizei n, GLuint* buffers) {
  driver_->fn.glGenBuffersARBFn(n, buffers);
}

void VirtualGLApi::glGenQueriesFn(GLsizei n, GLuint* ids) {
  driver_->fn.glGenQueriesFn(n, ids);
}

void VirtualGLApi::glGenQueriesARBFn(GLsizei n, GLuint* ids) {
  driver_->fn.glGenQueriesARBFn(n, ids);
}

void VirtualGLApi::glGenerateMipmapEXTFn(GLenum target) {
  driver_->fn.glGenerateMipmapEXTFn(target);
}

void VirtualGLApi::glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) {
  driver_->fn.glGenFramebuffersEXTFn(n, framebuffers);
}

void VirtualGLApi::glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) {
  driver_->fn.glGenRenderbuffersEXTFn(n, renderbuffers);
}

void VirtualGLApi::glGenTexturesFn(GLsizei n, GLuint* textures) {
  driver_->fn.glGenTexturesFn(n, textures);
}

void VirtualGLApi::glGetActiveAttribFn(
    GLuint program, GLuint index, GLsizei bufsize,
    GLsizei* length, GLint* size, GLenum* type, char* name) {
  driver_->fn.glGetActiveAttribFn(
      program, index, bufsize, length, size, type, name);
}

void VirtualGLApi::glGetActiveUniformFn(
    GLuint program, GLuint index, GLsizei bufsize,
    GLsizei* length, GLint* size, GLenum* type, char* name) {
  driver_->fn.glGetActiveUniformFn(
      program, index, bufsize, length, size, type, name);
}

void VirtualGLApi::glGetAttachedShadersFn(
    GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) {
  driver_->fn.glGetAttachedShadersFn(program, maxcount, count, shaders);
}

GLint VirtualGLApi::glGetAttribLocationFn(GLuint program, const char* name) {
  return driver_->fn.glGetAttribLocationFn(program, name);
}

void VirtualGLApi::glGetBooleanvFn(GLenum pname, GLboolean* params) {
  driver_->fn.glGetBooleanvFn(pname, params);
}

void VirtualGLApi::glGetBufferParameterivFn(
    GLenum target, GLenum pname, GLint* params) {
  driver_->fn.glGetBufferParameterivFn(target, pname, params);
}

GLenum VirtualGLApi::glGetErrorFn(void) {
  return driver_->fn.glGetErrorFn();
}

void VirtualGLApi::glGetFloatvFn(GLenum pname, GLfloat* params) {
  driver_->fn.glGetFloatvFn(pname, params);
}

void VirtualGLApi::glGetFramebufferAttachmentParameterivEXTFn(
    GLenum target, GLenum attachment, GLenum pname, GLint* params) {
  driver_->fn.glGetFramebufferAttachmentParameterivEXTFn(
      target, attachment, pname, params);
}

GLenum VirtualGLApi::glGetGraphicsResetStatusARBFn(void) {
  return driver_->fn.glGetGraphicsResetStatusARBFn();
}

void VirtualGLApi::glGetIntegervFn(GLenum pname, GLint* params) {
  driver_->fn.glGetIntegervFn(pname, params);
}

void VirtualGLApi::glGetProgramBinaryFn(
    GLuint program, GLsizei bufSize,
    GLsizei* length, GLenum* binaryFormat, GLvoid* binary) {
  driver_->fn.glGetProgramBinaryFn(
      program, bufSize, length, binaryFormat, binary);
}

void VirtualGLApi::glGetProgramivFn(
    GLuint program, GLenum pname, GLint* params) {
  driver_->fn.glGetProgramivFn(program, pname, params);
}

void VirtualGLApi::glGetProgramInfoLogFn(
    GLuint program, GLsizei bufsize, GLsizei* length, char* infolog) {
  driver_->fn.glGetProgramInfoLogFn(program, bufsize, length, infolog);
}

void VirtualGLApi::glGetQueryivFn(GLenum target, GLenum pname, GLint* params) {
  driver_->fn.glGetQueryivFn(target, pname, params);
}

void VirtualGLApi::glGetQueryivARBFn(
    GLenum target, GLenum pname, GLint* params) {
  driver_->fn.glGetQueryivARBFn(target, pname, params);
}

void VirtualGLApi::glGetQueryObjecti64vFn(
    GLuint id, GLenum pname, GLint64* params) {
  driver_->fn.glGetQueryObjecti64vFn(id, pname, params);
}

void VirtualGLApi::glGetQueryObjectivFn(
    GLuint id, GLenum pname, GLint* params) {
  driver_->fn.glGetQueryObjectivFn(id, pname, params);
}

void VirtualGLApi::glGetQueryObjectui64vFn(
    GLuint id, GLenum pname, GLuint64* params) {
  driver_->fn.glGetQueryObjectui64vFn(id, pname, params);
}

void VirtualGLApi::glGetQueryObjectuivFn(
    GLuint id, GLenum pname, GLuint* params) {
  driver_->fn.glGetQueryObjectuivFn(id, pname, params);
}

void VirtualGLApi::glGetQueryObjectuivARBFn(
    GLuint id, GLenum pname, GLuint* params) {
  driver_->fn.glGetQueryObjectuivARBFn(id, pname, params);
}

void VirtualGLApi::glGetRenderbufferParameterivEXTFn(
    GLenum target, GLenum pname, GLint* params) {
  driver_->fn.glGetRenderbufferParameterivEXTFn(target, pname, params);
}

void VirtualGLApi::glGetShaderivFn(GLuint shader, GLenum pname, GLint* params) {
  driver_->fn.glGetShaderivFn(shader, pname, params);
}

void VirtualGLApi::glGetShaderInfoLogFn(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog) {
  driver_->fn.glGetShaderInfoLogFn(shader, bufsize, length, infolog);
}

void VirtualGLApi::glGetShaderPrecisionFormatFn(
    GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
  driver_->fn.glGetShaderPrecisionFormatFn(
      shadertype, precisiontype, range, precision);
}

void VirtualGLApi::glGetShaderSourceFn(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* source) {
  driver_->fn.glGetShaderSourceFn(shader, bufsize, length, source);
}

const GLubyte* VirtualGLApi::glGetStringFn(GLenum name) {
  switch (name) {
    case GL_EXTENSIONS:
      return reinterpret_cast<const GLubyte*>(extensions_.c_str());
    default:
      return driver_->fn.glGetStringFn(name);
  }
}

void VirtualGLApi::glGetTexLevelParameterfvFn(
    GLenum target, GLint level, GLenum pname, GLfloat* params) {
  driver_->fn.glGetTexLevelParameterfvFn(target, level, pname, params);
}

void VirtualGLApi::glGetTexLevelParameterivFn(
    GLenum target, GLint level, GLenum pname, GLint* params) {
  driver_->fn.glGetTexLevelParameterivFn(target, level, pname, params);
}

void VirtualGLApi::glGetTexParameterfvFn(
    GLenum target, GLenum pname, GLfloat* params) {
  driver_->fn.glGetTexParameterfvFn(target, pname, params);
}

void VirtualGLApi::glGetTexParameterivFn(
    GLenum target, GLenum pname, GLint* params) {
  driver_->fn.glGetTexParameterivFn(target, pname, params);
}

void VirtualGLApi::glGetTranslatedShaderSourceANGLEFn(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* source) {
  driver_->fn.glGetTranslatedShaderSourceANGLEFn(
      shader, bufsize, length, source);
}

void VirtualGLApi::glGetUniformfvFn(
    GLuint program, GLint location, GLfloat* params) {
  driver_->fn.glGetUniformfvFn(program, location, params);
}

void VirtualGLApi::glGetUniformivFn(
    GLuint program, GLint location, GLint* params) {
  driver_->fn.glGetUniformivFn(program, location, params);
}

GLint VirtualGLApi::glGetUniformLocationFn(GLuint program, const char* name) {
  return driver_->fn.glGetUniformLocationFn(program, name);
}

void VirtualGLApi::glGetVertexAttribfvFn(
    GLuint index, GLenum pname, GLfloat* params) {
  driver_->fn.glGetVertexAttribfvFn(index, pname, params);
}

void VirtualGLApi::glGetVertexAttribivFn(
    GLuint index, GLenum pname, GLint* params) {
  driver_->fn.glGetVertexAttribivFn(index, pname, params);
}

void VirtualGLApi::glGetVertexAttribPointervFn(
    GLuint index, GLenum pname, void** pointer) {
  driver_->fn.glGetVertexAttribPointervFn(index, pname, pointer);
}

void VirtualGLApi::glHintFn(GLenum target, GLenum mode) {
  driver_->fn.glHintFn(target, mode);
}

GLboolean VirtualGLApi::glIsBufferFn(GLuint buffer) {
  return driver_->fn.glIsBufferFn(buffer);
}

GLboolean VirtualGLApi::glIsEnabledFn(GLenum cap) {
  return driver_->fn.glIsEnabledFn(cap);
}

GLboolean VirtualGLApi::glIsFramebufferEXTFn(GLuint framebuffer) {
  return driver_->fn.glIsFramebufferEXTFn(framebuffer);
}

GLboolean VirtualGLApi::glIsProgramFn(GLuint program) {
  return driver_->fn.glIsProgramFn(program);
}

GLboolean VirtualGLApi::glIsQueryARBFn(GLuint query) {
  return driver_->fn.glIsQueryARBFn(query);
}

GLboolean VirtualGLApi::glIsRenderbufferEXTFn(GLuint renderbuffer) {
  return driver_->fn.glIsRenderbufferEXTFn(renderbuffer);
}

GLboolean VirtualGLApi::glIsShaderFn(GLuint shader) {
  return driver_->fn.glIsShaderFn(shader);
}

GLboolean VirtualGLApi::glIsTextureFn(GLuint texture) {
  return driver_->fn.glIsTextureFn(texture);
}

void VirtualGLApi::glLineWidthFn(GLfloat width) {
  driver_->fn.glLineWidthFn(width);
}

void VirtualGLApi::glLinkProgramFn(GLuint program) {
  driver_->fn.glLinkProgramFn(program);
}

void* VirtualGLApi::glMapBufferFn(GLenum target, GLenum access) {
  return driver_->fn.glMapBufferFn(target, access);
}

void VirtualGLApi::glPixelStoreiFn(GLenum pname, GLint param) {
  driver_->fn.glPixelStoreiFn(pname, param);
}

void VirtualGLApi::glPointParameteriFn(GLenum pname, GLint param) {
  driver_->fn.glPointParameteriFn(pname, param);
}

void VirtualGLApi::glPolygonOffsetFn(GLfloat factor, GLfloat units) {
  driver_->fn.glPolygonOffsetFn(factor, units);
}

void VirtualGLApi::glProgramBinaryFn(
    GLuint program, GLenum binaryFormat, const GLvoid* binary, GLsizei length) {
  driver_->fn.glProgramBinaryFn(program, binaryFormat, binary, length);
}

void VirtualGLApi::glProgramParameteriFn(
    GLuint program, GLenum pname, GLint value) {
  driver_->fn.glProgramParameteriFn(program, pname, value);
}

void VirtualGLApi::glQueryCounterFn(GLuint id, GLenum target) {
  driver_->fn.glQueryCounterFn(id, target);
}

void VirtualGLApi::glReadBufferFn(GLenum src) {
  driver_->fn.glReadBufferFn(src);
}

void VirtualGLApi::glReadPixelsFn(
    GLint x, GLint y, GLsizei width, GLsizei height,
    GLenum format, GLenum type, void* pixels) {
  driver_->fn.glReadPixelsFn(x, y, width, height, format, type, pixels);
}

void VirtualGLApi::glReleaseShaderCompilerFn(void) {
  driver_->fn.glReleaseShaderCompilerFn();
}

void VirtualGLApi::glRenderbufferStorageMultisampleEXTFn(
    GLenum target, GLsizei samples, GLenum internalformat,
    GLsizei width, GLsizei height) {
  driver_->fn.glRenderbufferStorageMultisampleEXTFn(
      target, samples, internalformat, width, height);
}

void VirtualGLApi::glRenderbufferStorageMultisampleANGLEFn(
    GLenum target, GLsizei samples, GLenum internalformat,
    GLsizei width, GLsizei height) {
  driver_->fn.glRenderbufferStorageMultisampleANGLEFn(
      target, samples, internalformat, width, height);
}

void VirtualGLApi::glRenderbufferStorageEXTFn(
    GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  driver_->fn.glRenderbufferStorageEXTFn(target, internalformat, width, height);
}

void VirtualGLApi::glSampleCoverageFn(GLclampf value, GLboolean invert) {
  driver_->fn.glSampleCoverageFn(value, invert);
}

void VirtualGLApi::glScissorFn(
    GLint x, GLint y, GLsizei width, GLsizei height) {
  driver_->fn.glScissorFn(x, y, width, height);
}

void VirtualGLApi::glShaderBinaryFn(
    GLsizei n, const GLuint* shaders, GLenum binaryformat,
    const void* binary, GLsizei length) {
  driver_->fn.glShaderBinaryFn(n, shaders, binaryformat, binary, length);
}

void VirtualGLApi::glShaderSourceFn(
    GLuint shader, GLsizei count, const char** str, const GLint* length) {
  driver_->fn.glShaderSourceFn(shader, count, str, length);
}

void VirtualGLApi::glStencilFuncFn(GLenum func, GLint ref, GLuint mask) {
  driver_->fn.glStencilFuncFn(func, ref, mask);
}

void VirtualGLApi::glStencilFuncSeparateFn(
    GLenum face, GLenum func, GLint ref, GLuint mask) {
  driver_->fn.glStencilFuncSeparateFn(face, func, ref, mask);
}

void VirtualGLApi::glStencilMaskFn(GLuint mask) {
  driver_->fn.glStencilMaskFn(mask);
}

void VirtualGLApi::glStencilMaskSeparateFn(GLenum face, GLuint mask) {
  driver_->fn.glStencilMaskSeparateFn(face, mask);
}

void VirtualGLApi::glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) {
  driver_->fn.glStencilOpFn(fail, zfail, zpass);
}

void VirtualGLApi::glStencilOpSeparateFn(
    GLenum face, GLenum fail, GLenum zfail, GLenum zpass) {
  driver_->fn.glStencilOpSeparateFn(face, fail, zfail, zpass);
}

void VirtualGLApi::glTexImage2DFn(
    GLenum target, GLint level, GLint internalformat,
    GLsizei width, GLsizei height,
    GLint border, GLenum format, GLenum type, const void* pixels) {
  driver_->fn.glTexImage2DFn(
      target, level, internalformat, width, height,
      border, format, type, pixels);
}

void VirtualGLApi::glTexParameterfFn(
    GLenum target, GLenum pname, GLfloat param) {
  driver_->fn.glTexParameterfFn(target, pname, param);
}

void VirtualGLApi::glTexParameterfvFn(
    GLenum target, GLenum pname, const GLfloat* params) {
  driver_->fn.glTexParameterfvFn(target, pname, params);
}

void VirtualGLApi::glTexParameteriFn(GLenum target, GLenum pname, GLint param) {
  driver_->fn.glTexParameteriFn(target, pname, param);
}

void VirtualGLApi::glTexParameterivFn(
    GLenum target, GLenum pname, const GLint* params) {
  driver_->fn.glTexParameterivFn(target, pname, params);
}

void VirtualGLApi::glTexStorage2DEXTFn(
    GLenum target, GLsizei levels, GLenum internalformat,
    GLsizei width, GLsizei height) {
  driver_->fn.glTexStorage2DEXTFn(
      target, levels, internalformat, width, height);
}

void VirtualGLApi::glTexSubImage2DFn(
    GLenum target, GLint level, GLint xoffset, GLint yoffset,
    GLsizei width, GLsizei height, GLenum format, GLenum type,
    const void* pixels) {
  driver_->fn.glTexSubImage2DFn(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void VirtualGLApi::glUniform1fFn(GLint location, GLfloat x) {
  driver_->fn.glUniform1fFn(location, x);
}

void VirtualGLApi::glUniform1fvFn(
    GLint location, GLsizei count, const GLfloat* v) {
  driver_->fn.glUniform1fvFn(location, count, v);
}

void VirtualGLApi::glUniform1iFn(GLint location, GLint x) {
  driver_->fn.glUniform1iFn(location, x);
}

void VirtualGLApi::glUniform1ivFn(
    GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform1ivFn(location, count, v);
}

void VirtualGLApi::glUniform2fFn(GLint location, GLfloat x, GLfloat y) {
  driver_->fn.glUniform2fFn(location, x, y);
}

void VirtualGLApi::glUniform2fvFn(
    GLint location, GLsizei count, const GLfloat* v) {
  driver_->fn.glUniform2fvFn(location, count, v);
}

void VirtualGLApi::glUniform2iFn(GLint location, GLint x, GLint y) {
  driver_->fn.glUniform2iFn(location, x, y);
}

void VirtualGLApi::glUniform2ivFn(
    GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform2ivFn(location, count, v);
}

void VirtualGLApi::glUniform3fFn(
    GLint location, GLfloat x, GLfloat y, GLfloat z) {
  driver_->fn.glUniform3fFn(location, x, y, z);
}

void VirtualGLApi::glUniform3fvFn(
    GLint location, GLsizei count, const GLfloat* v) {
  driver_->fn.glUniform3fvFn(location, count, v);
}

void VirtualGLApi::glUniform3iFn(GLint location, GLint x, GLint y, GLint z) {
  driver_->fn.glUniform3iFn(location, x, y, z);
}

void VirtualGLApi::glUniform3ivFn(
    GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform3ivFn(location, count, v);
}

void VirtualGLApi::glUniform4fFn(
    GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  driver_->fn.glUniform4fFn(location, x, y, z, w);
}

void VirtualGLApi::glUniform4fvFn(
    GLint location, GLsizei count, const GLfloat* v) {
  driver_->fn.glUniform4fvFn(location, count, v);
}

void VirtualGLApi::glUniform4iFn(
    GLint location, GLint x, GLint y, GLint z, GLint w) {
  driver_->fn.glUniform4iFn(location, x, y, z, w);
}

void VirtualGLApi::glUniform4ivFn(
    GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform4ivFn(location, count, v);
}

void VirtualGLApi::glUniformMatrix2fvFn(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  driver_->fn.glUniformMatrix2fvFn(location, count, transpose, value);
}

void VirtualGLApi::glUniformMatrix3fvFn(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  driver_->fn.glUniformMatrix3fvFn(location, count, transpose, value);
}

void VirtualGLApi::glUniformMatrix4fvFn(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  driver_->fn.glUniformMatrix4fvFn(location, count, transpose, value);
}

GLboolean VirtualGLApi::glUnmapBufferFn(GLenum target) {
  return driver_->fn.glUnmapBufferFn(target);
}

void VirtualGLApi::glUseProgramFn(GLuint program) {
  driver_->fn.glUseProgramFn(program);
}

void VirtualGLApi::glValidateProgramFn(GLuint program) {
  driver_->fn.glValidateProgramFn(program);
}

void VirtualGLApi::glVertexAttrib1fFn(GLuint indx, GLfloat x) {
  driver_->fn.glVertexAttrib1fFn(indx, x);
}

void VirtualGLApi::glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib1fvFn(indx, values);
}

void VirtualGLApi::glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) {
  driver_->fn.glVertexAttrib2fFn(indx, x, y);
}

void VirtualGLApi::glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib2fvFn(indx, values);
}

void VirtualGLApi::glVertexAttrib3fFn(
    GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  driver_->fn.glVertexAttrib3fFn(indx, x, y, z);
}

void VirtualGLApi::glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib3fvFn(indx, values);
}

void VirtualGLApi::glVertexAttrib4fFn(
    GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  driver_->fn.glVertexAttrib4fFn(indx, x, y, z, w);
}

void VirtualGLApi::glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib4fvFn(indx, values);
}

void VirtualGLApi::glVertexAttribPointerFn(
    GLuint indx, GLint size, GLenum type, GLboolean normalized,
    GLsizei stride, const void* ptr) {
  driver_->fn.glVertexAttribPointerFn(
      indx, size, type, normalized, stride, ptr);
}

void VirtualGLApi::glViewportFn(
    GLint x, GLint y, GLsizei width, GLsizei height) {
  driver_->fn.glViewportFn(x, y, width, height);
}

void VirtualGLApi::glGenFencesNVFn(GLsizei n, GLuint* fences) {
  driver_->fn.glGenFencesNVFn(n, fences);
}

void VirtualGLApi::glDeleteFencesNVFn(GLsizei n, const GLuint* fences) {
  driver_->fn.glDeleteFencesNVFn(n, fences);
}

void VirtualGLApi::glSetFenceNVFn(GLuint fence, GLenum condition) {
  driver_->fn.glSetFenceNVFn(fence, condition);
}

GLboolean VirtualGLApi::glTestFenceNVFn(GLuint fence) {
  return driver_->fn.glTestFenceNVFn(fence);
}

void VirtualGLApi::glFinishFenceNVFn(GLuint fence) {
  driver_->fn.glFinishFenceNVFn(fence);
}

GLboolean VirtualGLApi::glIsFenceNVFn(GLuint fence) {
  return driver_->fn.glIsFenceNVFn(fence);
}

void VirtualGLApi::glGetFenceivNVFn(GLuint fence, GLenum pname, GLint* params) {
  driver_->fn.glGetFenceivNVFn(fence, pname, params);
}

GLsync VirtualGLApi::glFenceSyncFn(GLenum condition, GLbitfield flags) {
  return driver_->fn.glFenceSyncFn(condition, flags);
}

void VirtualGLApi::glDeleteSyncFn(GLsync sync) {
  driver_->fn.glDeleteSyncFn(sync);
}

void VirtualGLApi::glGetSyncivFn(
    GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length,GLint* values) {
  driver_->fn.glGetSyncivFn(sync, pname, bufSize, length,values);
}

void VirtualGLApi::glDrawArraysInstancedANGLEFn(
    GLenum mode, GLint first, GLsizei count, GLsizei primcount) {
  driver_->fn.glDrawArraysInstancedANGLEFn(mode, first, count, primcount);
}

void VirtualGLApi::glDrawElementsInstancedANGLEFn(
    GLenum mode, GLsizei count, GLenum type, const void* indices,
    GLsizei primcount) {
  driver_->fn.glDrawElementsInstancedANGLEFn(
      mode, count, type, indices, primcount);
}

void VirtualGLApi::glVertexAttribDivisorANGLEFn(GLuint index, GLuint divisor) {
  driver_->fn.glVertexAttribDivisorANGLEFn(index, divisor);
}

void VirtualGLApi::glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) {
  driver_->fn.glGenVertexArraysOESFn(n, arrays);
}

void VirtualGLApi::glDeleteVertexArraysOESFn(GLsizei n, const GLuint* arrays) {
  driver_->fn.glDeleteVertexArraysOESFn(n, arrays);
}

void VirtualGLApi::glBindVertexArrayOESFn(GLuint array) {
  driver_->fn.glBindVertexArrayOESFn(array);
}

GLboolean VirtualGLApi::glIsVertexArrayOESFn(GLuint array) {
  return driver_->fn.glIsVertexArrayOESFn(array);
}

}  // namespace gfx
