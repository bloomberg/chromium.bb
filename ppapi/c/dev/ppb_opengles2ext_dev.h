// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

// OpenGL ES interface.
#ifndef C_DEV_PPB_OPENGLES2EXT_DEV_H_
#define C_DEV_PPB_OPENGLES2EXT_DEV_H_

#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_opengles2.h"

#define PPB_OPENGLES2_INSTANCEDARRAYS_DEV_INTERFACE_1_0 "PPB_OpenGLES2InstancedArrays(Dev);1.0"  // NOLINT
#define PPB_OPENGLES2_INSTANCEDARRAYS_DEV_INTERFACE PPB_OPENGLES2_INSTANCEDARRAYS_DEV_INTERFACE_1_0  // NOLINT

struct PPB_OpenGLES2InstancedArrays_Dev {
  void (*DrawArraysInstancedANGLE)(
      PP_Resource context, GLenum mode, GLint first, GLsizei count,
      GLsizei primcount);
  void (*DrawElementsInstancedANGLE)(
      PP_Resource context, GLenum mode, GLsizei count, GLenum type,
      const void* indices, GLsizei primcount);
  void (*VertexAttribDivisorANGLE)(
      PP_Resource context, GLuint index, GLuint divisor);
};

#define PPB_OPENGLES2_FRAMEBUFFERBLIT_DEV_INTERFACE_1_0 "PPB_OpenGLES2FramebufferBlit(Dev);1.0"  // NOLINT
#define PPB_OPENGLES2_FRAMEBUFFERBLIT_DEV_INTERFACE PPB_OPENGLES2_FRAMEBUFFERBLIT_DEV_INTERFACE_1_0  // NOLINT

struct PPB_OpenGLES2FramebufferBlit_Dev {
  void (*BlitFramebufferEXT)(
      PP_Resource context, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
      GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask,
      GLenum filter);
};

#define PPB_OPENGLES2_FRAMEBUFFERMULTISAMPLE_DEV_INTERFACE_1_0 "PPB_OpenGLES2FramebufferMultisample(Dev);1.0"  // NOLINT
#define PPB_OPENGLES2_FRAMEBUFFERMULTISAMPLE_DEV_INTERFACE PPB_OPENGLES2_FRAMEBUFFERMULTISAMPLE_DEV_INTERFACE_1_0  // NOLINT

struct PPB_OpenGLES2FramebufferMultisample_Dev {
  void (*RenderbufferStorageMultisampleEXT)(
      PP_Resource context, GLenum target, GLsizei samples,
      GLenum internalformat, GLsizei width, GLsizei height);
};

#define PPB_OPENGLES2_CHROMIUMENABLEFEATURE_DEV_INTERFACE_1_0 "PPB_OpenGLES2ChromiumEnableFeature(Dev);1.0"  // NOLINT
#define PPB_OPENGLES2_CHROMIUMENABLEFEATURE_DEV_INTERFACE PPB_OPENGLES2_CHROMIUMENABLEFEATURE_DEV_INTERFACE_1_0  // NOLINT

struct PPB_OpenGLES2ChromiumEnableFeature_Dev {
  GLboolean (*EnableFeatureCHROMIUM)(PP_Resource context, const char* feature);
};

#define PPB_OPENGLES2_CHROMIUMMAPSUB_DEV_INTERFACE_1_0 "PPB_OpenGLES2ChromiumMapSub(Dev);1.0"  // NOLINT
#define PPB_OPENGLES2_CHROMIUMMAPSUB_DEV_INTERFACE PPB_OPENGLES2_CHROMIUMMAPSUB_DEV_INTERFACE_1_0  // NOLINT

struct PPB_OpenGLES2ChromiumMapSub_Dev {
  void* (*MapBufferSubDataCHROMIUM)(
      PP_Resource context, GLuint target, GLintptr offset, GLsizeiptr size,
      GLenum access);
  void (*UnmapBufferSubDataCHROMIUM)(PP_Resource context, const void* mem);
  void* (*MapTexSubImage2DCHROMIUM)(
      PP_Resource context, GLenum target, GLint level, GLint xoffset,
      GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
      GLenum access);
  void (*UnmapTexSubImage2DCHROMIUM)(PP_Resource context, const void* mem);
};

#define PPB_OPENGLES2_QUERY_DEV_INTERFACE_1_0 "PPB_OpenGLES2Query(Dev);1.0"
#define PPB_OPENGLES2_QUERY_DEV_INTERFACE PPB_OPENGLES2_QUERY_DEV_INTERFACE_1_0

struct PPB_OpenGLES2Query_Dev {
  void (*GenQueriesEXT)(PP_Resource context, GLsizei n, GLuint* queries);
  void (*DeleteQueriesEXT)(
      PP_Resource context, GLsizei n, const GLuint* queries);
  GLboolean (*IsQueryEXT)(PP_Resource context, GLuint id);
  void (*BeginQueryEXT)(PP_Resource context, GLenum target, GLuint id);
  void (*EndQueryEXT)(PP_Resource context, GLenum target);
  void (*GetQueryivEXT)(
      PP_Resource context, GLenum target, GLenum pname, GLint* params);
  void (*GetQueryObjectuivEXT)(
      PP_Resource context, GLuint id, GLenum pname, GLuint* params);
};

#endif  // C_DEV_PPB_OPENGLES2EXT_DEV_H_

