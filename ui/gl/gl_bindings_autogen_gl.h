// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// ui/gl/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef UI_GL_GL_BINDINGS_AUTOGEN_GL_H_
#define UI_GL_GL_BINDINGS_AUTOGEN_GL_H_

#include <string>

namespace gl {

class GLContext;

typedef void(GL_BINDING_CALL* glActiveTextureProc)(GLenum texture);
typedef void(GL_BINDING_CALL* glApplyFramebufferAttachmentCMAAINTELProc)(void);
typedef void(GL_BINDING_CALL* glAttachShaderProc)(GLuint program,
                                                  GLuint shader);
typedef void(GL_BINDING_CALL* glBeginQueryProc)(GLenum target, GLuint id);
typedef void(GL_BINDING_CALL* glBeginTransformFeedbackProc)(
    GLenum primitiveMode);
typedef void(GL_BINDING_CALL* glBindAttribLocationProc)(GLuint program,
                                                        GLuint index,
                                                        const char* name);
typedef void(GL_BINDING_CALL* glBindBufferProc)(GLenum target, GLuint buffer);
typedef void(GL_BINDING_CALL* glBindBufferBaseProc)(GLenum target,
                                                    GLuint index,
                                                    GLuint buffer);
typedef void(GL_BINDING_CALL* glBindBufferRangeProc)(GLenum target,
                                                     GLuint index,
                                                     GLuint buffer,
                                                     GLintptr offset,
                                                     GLsizeiptr size);
typedef void(GL_BINDING_CALL* glBindFragDataLocationProc)(GLuint program,
                                                          GLuint colorNumber,
                                                          const char* name);
typedef void(GL_BINDING_CALL* glBindFragDataLocationIndexedProc)(
    GLuint program,
    GLuint colorNumber,
    GLuint index,
    const char* name);
typedef void(GL_BINDING_CALL* glBindFramebufferEXTProc)(GLenum target,
                                                        GLuint framebuffer);
typedef void(GL_BINDING_CALL* glBindImageTextureEXTProc)(GLuint index,
                                                         GLuint texture,
                                                         GLint level,
                                                         GLboolean layered,
                                                         GLint layer,
                                                         GLenum access,
                                                         GLint format);
typedef void(GL_BINDING_CALL* glBindRenderbufferEXTProc)(GLenum target,
                                                         GLuint renderbuffer);
typedef void(GL_BINDING_CALL* glBindSamplerProc)(GLuint unit, GLuint sampler);
typedef void(GL_BINDING_CALL* glBindTextureProc)(GLenum target, GLuint texture);
typedef void(GL_BINDING_CALL* glBindTransformFeedbackProc)(GLenum target,
                                                           GLuint id);
typedef void(GL_BINDING_CALL* glBindUniformLocationCHROMIUMProc)(
    GLuint program,
    GLint location,
    const char* name);
typedef void(GL_BINDING_CALL* glBindVertexArrayOESProc)(GLuint array);
typedef void(GL_BINDING_CALL* glBlendBarrierKHRProc)(void);
typedef void(GL_BINDING_CALL* glBlendColorProc)(GLclampf red,
                                                GLclampf green,
                                                GLclampf blue,
                                                GLclampf alpha);
typedef void(GL_BINDING_CALL* glBlendEquationProc)(GLenum mode);
typedef void(GL_BINDING_CALL* glBlendEquationSeparateProc)(GLenum modeRGB,
                                                           GLenum modeAlpha);
typedef void(GL_BINDING_CALL* glBlendFuncProc)(GLenum sfactor, GLenum dfactor);
typedef void(GL_BINDING_CALL* glBlendFuncSeparateProc)(GLenum srcRGB,
                                                       GLenum dstRGB,
                                                       GLenum srcAlpha,
                                                       GLenum dstAlpha);
typedef void(GL_BINDING_CALL* glBlitFramebufferProc)(GLint srcX0,
                                                     GLint srcY0,
                                                     GLint srcX1,
                                                     GLint srcY1,
                                                     GLint dstX0,
                                                     GLint dstY0,
                                                     GLint dstX1,
                                                     GLint dstY1,
                                                     GLbitfield mask,
                                                     GLenum filter);
typedef void(GL_BINDING_CALL* glBufferDataProc)(GLenum target,
                                                GLsizeiptr size,
                                                const void* data,
                                                GLenum usage);
typedef void(GL_BINDING_CALL* glBufferSubDataProc)(GLenum target,
                                                   GLintptr offset,
                                                   GLsizeiptr size,
                                                   const void* data);
typedef GLenum(GL_BINDING_CALL* glCheckFramebufferStatusEXTProc)(GLenum target);
typedef void(GL_BINDING_CALL* glClearProc)(GLbitfield mask);
typedef void(GL_BINDING_CALL* glClearBufferfiProc)(GLenum buffer,
                                                   GLint drawbuffer,
                                                   const GLfloat depth,
                                                   GLint stencil);
typedef void(GL_BINDING_CALL* glClearBufferfvProc)(GLenum buffer,
                                                   GLint drawbuffer,
                                                   const GLfloat* value);
typedef void(GL_BINDING_CALL* glClearBufferivProc)(GLenum buffer,
                                                   GLint drawbuffer,
                                                   const GLint* value);
typedef void(GL_BINDING_CALL* glClearBufferuivProc)(GLenum buffer,
                                                    GLint drawbuffer,
                                                    const GLuint* value);
typedef void(GL_BINDING_CALL* glClearColorProc)(GLclampf red,
                                                GLclampf green,
                                                GLclampf blue,
                                                GLclampf alpha);
typedef void(GL_BINDING_CALL* glClearDepthProc)(GLclampd depth);
typedef void(GL_BINDING_CALL* glClearDepthfProc)(GLclampf depth);
typedef void(GL_BINDING_CALL* glClearStencilProc)(GLint s);
typedef GLenum(GL_BINDING_CALL* glClientWaitSyncProc)(GLsync sync,
                                                      GLbitfield flags,
                                                      GLuint64 timeout);
typedef void(GL_BINDING_CALL* glColorMaskProc)(GLboolean red,
                                               GLboolean green,
                                               GLboolean blue,
                                               GLboolean alpha);
typedef void(GL_BINDING_CALL* glCompileShaderProc)(GLuint shader);
typedef void(GL_BINDING_CALL* glCompressedCopyTextureCHROMIUMProc)(
    GLuint sourceId,
    GLuint destId);
typedef void(GL_BINDING_CALL* glCompressedTexImage2DProc)(GLenum target,
                                                          GLint level,
                                                          GLenum internalformat,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLint border,
                                                          GLsizei imageSize,
                                                          const void* data);
typedef void(GL_BINDING_CALL* glCompressedTexImage2DRobustANGLEProc)(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLsizei imageSize,
    GLsizei dataSize,
    const void* data);
typedef void(GL_BINDING_CALL* glCompressedTexImage3DProc)(GLenum target,
                                                          GLint level,
                                                          GLenum internalformat,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLsizei depth,
                                                          GLint border,
                                                          GLsizei imageSize,
                                                          const void* data);
typedef void(GL_BINDING_CALL* glCompressedTexImage3DRobustANGLEProc)(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLsizei imageSize,
    GLsizei dataSize,
    const void* data);
typedef void(GL_BINDING_CALL* glCompressedTexSubImage2DProc)(GLenum target,
                                                             GLint level,
                                                             GLint xoffset,
                                                             GLint yoffset,
                                                             GLsizei width,
                                                             GLsizei height,
                                                             GLenum format,
                                                             GLsizei imageSize,
                                                             const void* data);
typedef void(GL_BINDING_CALL* glCompressedTexSubImage2DRobustANGLEProc)(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLsizei imageSize,
    GLsizei dataSize,
    const void* data);
typedef void(GL_BINDING_CALL* glCompressedTexSubImage3DProc)(GLenum target,
                                                             GLint level,
                                                             GLint xoffset,
                                                             GLint yoffset,
                                                             GLint zoffset,
                                                             GLsizei width,
                                                             GLsizei height,
                                                             GLsizei depth,
                                                             GLenum format,
                                                             GLsizei imageSize,
                                                             const void* data);
typedef void(GL_BINDING_CALL* glCompressedTexSubImage3DRobustANGLEProc)(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLsizei imageSize,
    GLsizei dataSize,
    const void* data);
typedef void(GL_BINDING_CALL* glCopyBufferSubDataProc)(GLenum readTarget,
                                                       GLenum writeTarget,
                                                       GLintptr readOffset,
                                                       GLintptr writeOffset,
                                                       GLsizeiptr size);
typedef void(GL_BINDING_CALL* glCopySubTextureCHROMIUMProc)(
    GLuint sourceId,
    GLint sourceLevel,
    GLenum destTarget,
    GLuint destId,
    GLint destLevel,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpackFlipY,
    GLboolean unpackPremultiplyAlpha,
    GLboolean unpackUnmultiplyAlpha);
typedef void(GL_BINDING_CALL* glCopyTexImage2DProc)(GLenum target,
                                                    GLint level,
                                                    GLenum internalformat,
                                                    GLint x,
                                                    GLint y,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLint border);
typedef void(GL_BINDING_CALL* glCopyTexSubImage2DProc)(GLenum target,
                                                       GLint level,
                                                       GLint xoffset,
                                                       GLint yoffset,
                                                       GLint x,
                                                       GLint y,
                                                       GLsizei width,
                                                       GLsizei height);
typedef void(GL_BINDING_CALL* glCopyTexSubImage3DProc)(GLenum target,
                                                       GLint level,
                                                       GLint xoffset,
                                                       GLint yoffset,
                                                       GLint zoffset,
                                                       GLint x,
                                                       GLint y,
                                                       GLsizei width,
                                                       GLsizei height);
typedef void(GL_BINDING_CALL* glCopyTextureCHROMIUMProc)(
    GLuint sourceId,
    GLint sourceLevel,
    GLenum destTarget,
    GLuint destId,
    GLint destLevel,
    GLint internalFormat,
    GLenum destType,
    GLboolean unpackFlipY,
    GLboolean unpackPremultiplyAlpha,
    GLboolean unpackUnmultiplyAlpha);
typedef void(GL_BINDING_CALL* glCoverageModulationNVProc)(GLenum components);
typedef void(GL_BINDING_CALL* glCoverFillPathInstancedNVProc)(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues);
typedef void(GL_BINDING_CALL* glCoverFillPathNVProc)(GLuint path,
                                                     GLenum coverMode);
typedef void(GL_BINDING_CALL* glCoverStrokePathInstancedNVProc)(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues);
typedef void(GL_BINDING_CALL* glCoverStrokePathNVProc)(GLuint name,
                                                       GLenum coverMode);
typedef GLuint(GL_BINDING_CALL* glCreateProgramProc)(void);
typedef GLuint(GL_BINDING_CALL* glCreateShaderProc)(GLenum type);
typedef void(GL_BINDING_CALL* glCullFaceProc)(GLenum mode);
typedef void(GL_BINDING_CALL* glDebugMessageCallbackProc)(
    GLDEBUGPROC callback,
    const void* userParam);
typedef void(GL_BINDING_CALL* glDebugMessageControlProc)(GLenum source,
                                                         GLenum type,
                                                         GLenum severity,
                                                         GLsizei count,
                                                         const GLuint* ids,
                                                         GLboolean enabled);
typedef void(GL_BINDING_CALL* glDebugMessageInsertProc)(GLenum source,
                                                        GLenum type,
                                                        GLuint id,
                                                        GLenum severity,
                                                        GLsizei length,
                                                        const char* buf);
typedef void(GL_BINDING_CALL* glDeleteBuffersARBProc)(GLsizei n,
                                                      const GLuint* buffers);
typedef void(GL_BINDING_CALL* glDeleteFencesAPPLEProc)(GLsizei n,
                                                       const GLuint* fences);
typedef void(GL_BINDING_CALL* glDeleteFencesNVProc)(GLsizei n,
                                                    const GLuint* fences);
typedef void(GL_BINDING_CALL* glDeleteFramebuffersEXTProc)(
    GLsizei n,
    const GLuint* framebuffers);
typedef void(GL_BINDING_CALL* glDeletePathsNVProc)(GLuint path, GLsizei range);
typedef void(GL_BINDING_CALL* glDeleteProgramProc)(GLuint program);
typedef void(GL_BINDING_CALL* glDeleteQueriesProc)(GLsizei n,
                                                   const GLuint* ids);
typedef void(GL_BINDING_CALL* glDeleteRenderbuffersEXTProc)(
    GLsizei n,
    const GLuint* renderbuffers);
typedef void(GL_BINDING_CALL* glDeleteSamplersProc)(GLsizei n,
                                                    const GLuint* samplers);
typedef void(GL_BINDING_CALL* glDeleteShaderProc)(GLuint shader);
typedef void(GL_BINDING_CALL* glDeleteSyncProc)(GLsync sync);
typedef void(GL_BINDING_CALL* glDeleteTexturesProc)(GLsizei n,
                                                    const GLuint* textures);
typedef void(GL_BINDING_CALL* glDeleteTransformFeedbacksProc)(
    GLsizei n,
    const GLuint* ids);
typedef void(GL_BINDING_CALL* glDeleteVertexArraysOESProc)(
    GLsizei n,
    const GLuint* arrays);
typedef void(GL_BINDING_CALL* glDepthFuncProc)(GLenum func);
typedef void(GL_BINDING_CALL* glDepthMaskProc)(GLboolean flag);
typedef void(GL_BINDING_CALL* glDepthRangeProc)(GLclampd zNear, GLclampd zFar);
typedef void(GL_BINDING_CALL* glDepthRangefProc)(GLclampf zNear, GLclampf zFar);
typedef void(GL_BINDING_CALL* glDetachShaderProc)(GLuint program,
                                                  GLuint shader);
typedef void(GL_BINDING_CALL* glDisableProc)(GLenum cap);
typedef void(GL_BINDING_CALL* glDisableVertexAttribArrayProc)(GLuint index);
typedef void(GL_BINDING_CALL* glDiscardFramebufferEXTProc)(
    GLenum target,
    GLsizei numAttachments,
    const GLenum* attachments);
typedef void(GL_BINDING_CALL* glDrawArraysProc)(GLenum mode,
                                                GLint first,
                                                GLsizei count);
typedef void(GL_BINDING_CALL* glDrawArraysInstancedANGLEProc)(
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount);
typedef void(GL_BINDING_CALL* glDrawBufferProc)(GLenum mode);
typedef void(GL_BINDING_CALL* glDrawBuffersARBProc)(GLsizei n,
                                                    const GLenum* bufs);
typedef void(GL_BINDING_CALL* glDrawElementsProc)(GLenum mode,
                                                  GLsizei count,
                                                  GLenum type,
                                                  const void* indices);
typedef void(GL_BINDING_CALL* glDrawElementsInstancedANGLEProc)(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount);
typedef void(GL_BINDING_CALL* glDrawRangeElementsProc)(GLenum mode,
                                                       GLuint start,
                                                       GLuint end,
                                                       GLsizei count,
                                                       GLenum type,
                                                       const void* indices);
typedef void(GL_BINDING_CALL* glEGLImageTargetRenderbufferStorageOESProc)(
    GLenum target,
    GLeglImageOES image);
typedef void(GL_BINDING_CALL* glEGLImageTargetTexture2DOESProc)(
    GLenum target,
    GLeglImageOES image);
typedef void(GL_BINDING_CALL* glEnableProc)(GLenum cap);
typedef void(GL_BINDING_CALL* glEnableVertexAttribArrayProc)(GLuint index);
typedef void(GL_BINDING_CALL* glEndQueryProc)(GLenum target);
typedef void(GL_BINDING_CALL* glEndTransformFeedbackProc)(void);
typedef GLsync(GL_BINDING_CALL* glFenceSyncProc)(GLenum condition,
                                                 GLbitfield flags);
typedef void(GL_BINDING_CALL* glFinishProc)(void);
typedef void(GL_BINDING_CALL* glFinishFenceAPPLEProc)(GLuint fence);
typedef void(GL_BINDING_CALL* glFinishFenceNVProc)(GLuint fence);
typedef void(GL_BINDING_CALL* glFlushProc)(void);
typedef void(GL_BINDING_CALL* glFlushMappedBufferRangeProc)(GLenum target,
                                                            GLintptr offset,
                                                            GLsizeiptr length);
typedef void(GL_BINDING_CALL* glFramebufferRenderbufferEXTProc)(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffertarget,
    GLuint renderbuffer);
typedef void(GL_BINDING_CALL* glFramebufferTexture2DEXTProc)(GLenum target,
                                                             GLenum attachment,
                                                             GLenum textarget,
                                                             GLuint texture,
                                                             GLint level);
typedef void(GL_BINDING_CALL* glFramebufferTexture2DMultisampleEXTProc)(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level,
    GLsizei samples);
typedef void(GL_BINDING_CALL* glFramebufferTextureLayerProc)(GLenum target,
                                                             GLenum attachment,
                                                             GLuint texture,
                                                             GLint level,
                                                             GLint layer);
typedef void(GL_BINDING_CALL* glFrontFaceProc)(GLenum mode);
typedef void(GL_BINDING_CALL* glGenBuffersARBProc)(GLsizei n, GLuint* buffers);
typedef void(GL_BINDING_CALL* glGenerateMipmapEXTProc)(GLenum target);
typedef void(GL_BINDING_CALL* glGenFencesAPPLEProc)(GLsizei n, GLuint* fences);
typedef void(GL_BINDING_CALL* glGenFencesNVProc)(GLsizei n, GLuint* fences);
typedef void(GL_BINDING_CALL* glGenFramebuffersEXTProc)(GLsizei n,
                                                        GLuint* framebuffers);
typedef GLuint(GL_BINDING_CALL* glGenPathsNVProc)(GLsizei range);
typedef void(GL_BINDING_CALL* glGenQueriesProc)(GLsizei n, GLuint* ids);
typedef void(GL_BINDING_CALL* glGenRenderbuffersEXTProc)(GLsizei n,
                                                         GLuint* renderbuffers);
typedef void(GL_BINDING_CALL* glGenSamplersProc)(GLsizei n, GLuint* samplers);
typedef void(GL_BINDING_CALL* glGenTexturesProc)(GLsizei n, GLuint* textures);
typedef void(GL_BINDING_CALL* glGenTransformFeedbacksProc)(GLsizei n,
                                                           GLuint* ids);
typedef void(GL_BINDING_CALL* glGenVertexArraysOESProc)(GLsizei n,
                                                        GLuint* arrays);
typedef void(GL_BINDING_CALL* glGetActiveAttribProc)(GLuint program,
                                                     GLuint index,
                                                     GLsizei bufsize,
                                                     GLsizei* length,
                                                     GLint* size,
                                                     GLenum* type,
                                                     char* name);
typedef void(GL_BINDING_CALL* glGetActiveUniformProc)(GLuint program,
                                                      GLuint index,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      GLint* size,
                                                      GLenum* type,
                                                      char* name);
typedef void(GL_BINDING_CALL* glGetActiveUniformBlockivProc)(
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetActiveUniformBlockivRobustANGLEProc)(
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetActiveUniformBlockNameProc)(
    GLuint program,
    GLuint uniformBlockIndex,
    GLsizei bufSize,
    GLsizei* length,
    char* uniformBlockName);
typedef void(GL_BINDING_CALL* glGetActiveUniformsivProc)(
    GLuint program,
    GLsizei uniformCount,
    const GLuint* uniformIndices,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetAttachedShadersProc)(GLuint program,
                                                        GLsizei maxcount,
                                                        GLsizei* count,
                                                        GLuint* shaders);
typedef GLint(GL_BINDING_CALL* glGetAttribLocationProc)(GLuint program,
                                                        const char* name);
typedef void(GL_BINDING_CALL* glGetBooleani_vRobustANGLEProc)(GLenum target,
                                                              GLuint index,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLboolean* data);
typedef void(GL_BINDING_CALL* glGetBooleanvProc)(GLenum pname,
                                                 GLboolean* params);
typedef void(GL_BINDING_CALL* glGetBooleanvRobustANGLEProc)(GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLboolean* data);
typedef void(GL_BINDING_CALL* glGetBufferParameteri64vRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint64* params);
typedef void(GL_BINDING_CALL* glGetBufferParameterivProc)(GLenum target,
                                                          GLenum pname,
                                                          GLint* params);
typedef void(GL_BINDING_CALL* glGetBufferParameterivRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetBufferPointervRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    void** params);
typedef void(GL_BINDING_CALL* glGetDebugMessageLogProc)(GLuint count,
                                                        GLsizei bufSize,
                                                        GLenum* sources,
                                                        GLenum* types,
                                                        GLuint* ids,
                                                        GLenum* severities,
                                                        GLsizei* lengths,
                                                        char* messageLog);
typedef GLenum(GL_BINDING_CALL* glGetErrorProc)(void);
typedef void(GL_BINDING_CALL* glGetFenceivNVProc)(GLuint fence,
                                                  GLenum pname,
                                                  GLint* params);
typedef void(GL_BINDING_CALL* glGetFloatvProc)(GLenum pname, GLfloat* params);
typedef void(GL_BINDING_CALL* glGetFloatvRobustANGLEProc)(GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLfloat* data);
typedef GLint(GL_BINDING_CALL* glGetFragDataIndexProc)(GLuint program,
                                                       const char* name);
typedef GLint(GL_BINDING_CALL* glGetFragDataLocationProc)(GLuint program,
                                                          const char* name);
typedef void(GL_BINDING_CALL* glGetFramebufferAttachmentParameterivEXTProc)(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLint* params);
typedef void(
    GL_BINDING_CALL* glGetFramebufferAttachmentParameterivRobustANGLEProc)(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetFramebufferParameterivRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef GLenum(GL_BINDING_CALL* glGetGraphicsResetStatusARBProc)(void);
typedef void(GL_BINDING_CALL* glGetInteger64i_vProc)(GLenum target,
                                                     GLuint index,
                                                     GLint64* data);
typedef void(GL_BINDING_CALL* glGetInteger64i_vRobustANGLEProc)(GLenum target,
                                                                GLuint index,
                                                                GLsizei bufSize,
                                                                GLsizei* length,
                                                                GLint64* data);
typedef void(GL_BINDING_CALL* glGetInteger64vProc)(GLenum pname,
                                                   GLint64* params);
typedef void(GL_BINDING_CALL* glGetInteger64vRobustANGLEProc)(GLenum pname,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLint64* data);
typedef void(GL_BINDING_CALL* glGetIntegeri_vProc)(GLenum target,
                                                   GLuint index,
                                                   GLint* data);
typedef void(GL_BINDING_CALL* glGetIntegeri_vRobustANGLEProc)(GLenum target,
                                                              GLuint index,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLint* data);
typedef void(GL_BINDING_CALL* glGetIntegervProc)(GLenum pname, GLint* params);
typedef void(GL_BINDING_CALL* glGetIntegervRobustANGLEProc)(GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLint* data);
typedef void(GL_BINDING_CALL* glGetInternalformativProc)(GLenum target,
                                                         GLenum internalformat,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLint* params);
typedef void(GL_BINDING_CALL* glGetInternalformativRobustANGLEProc)(
    GLenum target,
    GLenum internalformat,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetMultisamplefvProc)(GLenum pname,
                                                      GLuint index,
                                                      GLfloat* val);
typedef void(GL_BINDING_CALL* glGetMultisamplefvRobustANGLEProc)(
    GLenum pname,
    GLuint index,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* val);
typedef void(GL_BINDING_CALL* glGetnUniformfvRobustANGLEProc)(GLuint program,
                                                              GLint location,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLfloat* params);
typedef void(GL_BINDING_CALL* glGetnUniformivRobustANGLEProc)(GLuint program,
                                                              GLint location,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLint* params);
typedef void(GL_BINDING_CALL* glGetnUniformuivRobustANGLEProc)(GLuint program,
                                                               GLint location,
                                                               GLsizei bufSize,
                                                               GLsizei* length,
                                                               GLuint* params);
typedef void(GL_BINDING_CALL* glGetObjectLabelProc)(GLenum identifier,
                                                    GLuint name,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    char* label);
typedef void(GL_BINDING_CALL* glGetObjectPtrLabelProc)(void* ptr,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       char* label);
typedef void(GL_BINDING_CALL* glGetPointervProc)(GLenum pname, void** params);
typedef void(GL_BINDING_CALL* glGetPointervRobustANGLERobustANGLEProc)(
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    void** params);
typedef void(GL_BINDING_CALL* glGetProgramBinaryProc)(GLuint program,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLenum* binaryFormat,
                                                      GLvoid* binary);
typedef void(GL_BINDING_CALL* glGetProgramInfoLogProc)(GLuint program,
                                                       GLsizei bufsize,
                                                       GLsizei* length,
                                                       char* infolog);
typedef void(GL_BINDING_CALL* glGetProgramInterfaceivProc)(
    GLuint program,
    GLenum programInterface,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetProgramInterfaceivRobustANGLEProc)(
    GLuint program,
    GLenum programInterface,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetProgramivProc)(GLuint program,
                                                  GLenum pname,
                                                  GLint* params);
typedef void(GL_BINDING_CALL* glGetProgramivRobustANGLEProc)(GLuint program,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLint* params);
typedef void(GL_BINDING_CALL* glGetProgramResourceivProc)(
    GLuint program,
    GLenum programInterface,
    GLuint index,
    GLsizei propCount,
    const GLenum* props,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef GLint(GL_BINDING_CALL* glGetProgramResourceLocationProc)(
    GLuint program,
    GLenum programInterface,
    const char* name);
typedef void(GL_BINDING_CALL* glGetProgramResourceNameProc)(
    GLuint program,
    GLenum programInterface,
    GLuint index,
    GLsizei bufSize,
    GLsizei* length,
    GLchar* name);
typedef void(GL_BINDING_CALL* glGetQueryivProc)(GLenum target,
                                                GLenum pname,
                                                GLint* params);
typedef void(GL_BINDING_CALL* glGetQueryivRobustANGLEProc)(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params);
typedef void(GL_BINDING_CALL* glGetQueryObjecti64vProc)(GLuint id,
                                                        GLenum pname,
                                                        GLint64* params);
typedef void(GL_BINDING_CALL* glGetQueryObjecti64vRobustANGLEProc)(
    GLuint id,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint64* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectivProc)(GLuint id,
                                                      GLenum pname,
                                                      GLint* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectivRobustANGLEProc)(
    GLuint id,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectui64vProc)(GLuint id,
                                                         GLenum pname,
                                                         GLuint64* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectui64vRobustANGLEProc)(
    GLuint id,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLuint64* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectuivProc)(GLuint id,
                                                       GLenum pname,
                                                       GLuint* params);
typedef void(GL_BINDING_CALL* glGetQueryObjectuivRobustANGLEProc)(
    GLuint id,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLuint* params);
typedef void(GL_BINDING_CALL* glGetRenderbufferParameterivEXTProc)(
    GLenum target,
    GLenum pname,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetRenderbufferParameterivRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetSamplerParameterfvProc)(GLuint sampler,
                                                           GLenum pname,
                                                           GLfloat* params);
typedef void(GL_BINDING_CALL* glGetSamplerParameterfvRobustANGLEProc)(
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetSamplerParameterIivRobustANGLEProc)(
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetSamplerParameterIuivRobustANGLEProc)(
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLuint* params);
typedef void(GL_BINDING_CALL* glGetSamplerParameterivProc)(GLuint sampler,
                                                           GLenum pname,
                                                           GLint* params);
typedef void(GL_BINDING_CALL* glGetSamplerParameterivRobustANGLEProc)(
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetShaderInfoLogProc)(GLuint shader,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      char* infolog);
typedef void(GL_BINDING_CALL* glGetShaderivProc)(GLuint shader,
                                                 GLenum pname,
                                                 GLint* params);
typedef void(GL_BINDING_CALL* glGetShaderivRobustANGLEProc)(GLuint shader,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLint* params);
typedef void(GL_BINDING_CALL* glGetShaderPrecisionFormatProc)(
    GLenum shadertype,
    GLenum precisiontype,
    GLint* range,
    GLint* precision);
typedef void(GL_BINDING_CALL* glGetShaderSourceProc)(GLuint shader,
                                                     GLsizei bufsize,
                                                     GLsizei* length,
                                                     char* source);
typedef const GLubyte*(GL_BINDING_CALL* glGetStringProc)(GLenum name);
typedef const GLubyte*(GL_BINDING_CALL* glGetStringiProc)(GLenum name,
                                                          GLuint index);
typedef void(GL_BINDING_CALL* glGetSyncivProc)(GLsync sync,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLint* values);
typedef void(GL_BINDING_CALL* glGetTexLevelParameterfvProc)(GLenum target,
                                                            GLint level,
                                                            GLenum pname,
                                                            GLfloat* params);
typedef void(GL_BINDING_CALL* glGetTexLevelParameterfvRobustANGLEProc)(
    GLenum target,
    GLint level,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetTexLevelParameterivProc)(GLenum target,
                                                            GLint level,
                                                            GLenum pname,
                                                            GLint* params);
typedef void(GL_BINDING_CALL* glGetTexLevelParameterivRobustANGLEProc)(
    GLenum target,
    GLint level,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetTexParameterfvProc)(GLenum target,
                                                       GLenum pname,
                                                       GLfloat* params);
typedef void(GL_BINDING_CALL* glGetTexParameterfvRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetTexParameterIivRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetTexParameterIuivRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLuint* params);
typedef void(GL_BINDING_CALL* glGetTexParameterivProc)(GLenum target,
                                                       GLenum pname,
                                                       GLint* params);
typedef void(GL_BINDING_CALL* glGetTexParameterivRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetTransformFeedbackVaryingProc)(
    GLuint program,
    GLuint index,
    GLsizei bufSize,
    GLsizei* length,
    GLsizei* size,
    GLenum* type,
    char* name);
typedef void(GL_BINDING_CALL* glGetTranslatedShaderSourceANGLEProc)(
    GLuint shader,
    GLsizei bufsize,
    GLsizei* length,
    char* source);
typedef GLuint(GL_BINDING_CALL* glGetUniformBlockIndexProc)(
    GLuint program,
    const char* uniformBlockName);
typedef void(GL_BINDING_CALL* glGetUniformfvProc)(GLuint program,
                                                  GLint location,
                                                  GLfloat* params);
typedef void(GL_BINDING_CALL* glGetUniformfvRobustANGLEProc)(GLuint program,
                                                             GLint location,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLfloat* params);
typedef void(GL_BINDING_CALL* glGetUniformIndicesProc)(
    GLuint program,
    GLsizei uniformCount,
    const char* const* uniformNames,
    GLuint* uniformIndices);
typedef void(GL_BINDING_CALL* glGetUniformivProc)(GLuint program,
                                                  GLint location,
                                                  GLint* params);
typedef void(GL_BINDING_CALL* glGetUniformivRobustANGLEProc)(GLuint program,
                                                             GLint location,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLint* params);
typedef GLint(GL_BINDING_CALL* glGetUniformLocationProc)(GLuint program,
                                                         const char* name);
typedef void(GL_BINDING_CALL* glGetUniformuivProc)(GLuint program,
                                                   GLint location,
                                                   GLuint* params);
typedef void(GL_BINDING_CALL* glGetUniformuivRobustANGLEProc)(GLuint program,
                                                              GLint location,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLuint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribfvProc)(GLuint index,
                                                       GLenum pname,
                                                       GLfloat* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribfvRobustANGLEProc)(
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLfloat* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribIivRobustANGLEProc)(
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribIuivRobustANGLEProc)(
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLuint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribivProc)(GLuint index,
                                                       GLenum pname,
                                                       GLint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribivRobustANGLEProc)(
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params);
typedef void(GL_BINDING_CALL* glGetVertexAttribPointervProc)(GLuint index,
                                                             GLenum pname,
                                                             void** pointer);
typedef void(GL_BINDING_CALL* glGetVertexAttribPointervRobustANGLEProc)(
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    void** pointer);
typedef void(GL_BINDING_CALL* glHintProc)(GLenum target, GLenum mode);
typedef void(GL_BINDING_CALL* glInsertEventMarkerEXTProc)(GLsizei length,
                                                          const char* marker);
typedef void(GL_BINDING_CALL* glInvalidateFramebufferProc)(
    GLenum target,
    GLsizei numAttachments,
    const GLenum* attachments);
typedef void(GL_BINDING_CALL* glInvalidateSubFramebufferProc)(
    GLenum target,
    GLsizei numAttachments,
    const GLenum* attachments,
    GLint x,
    GLint y,
    GLint width,
    GLint height);
typedef GLboolean(GL_BINDING_CALL* glIsBufferProc)(GLuint buffer);
typedef GLboolean(GL_BINDING_CALL* glIsEnabledProc)(GLenum cap);
typedef GLboolean(GL_BINDING_CALL* glIsFenceAPPLEProc)(GLuint fence);
typedef GLboolean(GL_BINDING_CALL* glIsFenceNVProc)(GLuint fence);
typedef GLboolean(GL_BINDING_CALL* glIsFramebufferEXTProc)(GLuint framebuffer);
typedef GLboolean(GL_BINDING_CALL* glIsPathNVProc)(GLuint path);
typedef GLboolean(GL_BINDING_CALL* glIsProgramProc)(GLuint program);
typedef GLboolean(GL_BINDING_CALL* glIsQueryProc)(GLuint query);
typedef GLboolean(GL_BINDING_CALL* glIsRenderbufferEXTProc)(
    GLuint renderbuffer);
typedef GLboolean(GL_BINDING_CALL* glIsSamplerProc)(GLuint sampler);
typedef GLboolean(GL_BINDING_CALL* glIsShaderProc)(GLuint shader);
typedef GLboolean(GL_BINDING_CALL* glIsSyncProc)(GLsync sync);
typedef GLboolean(GL_BINDING_CALL* glIsTextureProc)(GLuint texture);
typedef GLboolean(GL_BINDING_CALL* glIsTransformFeedbackProc)(GLuint id);
typedef GLboolean(GL_BINDING_CALL* glIsVertexArrayOESProc)(GLuint array);
typedef void(GL_BINDING_CALL* glLineWidthProc)(GLfloat width);
typedef void(GL_BINDING_CALL* glLinkProgramProc)(GLuint program);
typedef void*(GL_BINDING_CALL* glMapBufferProc)(GLenum target, GLenum access);
typedef void*(GL_BINDING_CALL* glMapBufferRangeProc)(GLenum target,
                                                     GLintptr offset,
                                                     GLsizeiptr length,
                                                     GLbitfield access);
typedef void(GL_BINDING_CALL* glMatrixLoadfEXTProc)(GLenum matrixMode,
                                                    const GLfloat* m);
typedef void(GL_BINDING_CALL* glMatrixLoadIdentityEXTProc)(GLenum matrixMode);
typedef void(GL_BINDING_CALL* glMemoryBarrierEXTProc)(GLbitfield barriers);
typedef void(GL_BINDING_CALL* glObjectLabelProc)(GLenum identifier,
                                                 GLuint name,
                                                 GLsizei length,
                                                 const char* label);
typedef void(GL_BINDING_CALL* glObjectPtrLabelProc)(void* ptr,
                                                    GLsizei length,
                                                    const char* label);
typedef void(GL_BINDING_CALL* glPathCommandsNVProc)(GLuint path,
                                                    GLsizei numCommands,
                                                    const GLubyte* commands,
                                                    GLsizei numCoords,
                                                    GLenum coordType,
                                                    const GLvoid* coords);
typedef void(GL_BINDING_CALL* glPathParameterfNVProc)(GLuint path,
                                                      GLenum pname,
                                                      GLfloat value);
typedef void(GL_BINDING_CALL* glPathParameteriNVProc)(GLuint path,
                                                      GLenum pname,
                                                      GLint value);
typedef void(GL_BINDING_CALL* glPathStencilFuncNVProc)(GLenum func,
                                                       GLint ref,
                                                       GLuint mask);
typedef void(GL_BINDING_CALL* glPauseTransformFeedbackProc)(void);
typedef void(GL_BINDING_CALL* glPixelStoreiProc)(GLenum pname, GLint param);
typedef void(GL_BINDING_CALL* glPointParameteriProc)(GLenum pname, GLint param);
typedef void(GL_BINDING_CALL* glPolygonModeProc)(GLenum face, GLenum mode);
typedef void(GL_BINDING_CALL* glPolygonOffsetProc)(GLfloat factor,
                                                   GLfloat units);
typedef void(GL_BINDING_CALL* glPopDebugGroupProc)();
typedef void(GL_BINDING_CALL* glPopGroupMarkerEXTProc)(void);
typedef void(GL_BINDING_CALL* glPrimitiveRestartIndexProc)(GLuint index);
typedef void(GL_BINDING_CALL* glProgramBinaryProc)(GLuint program,
                                                   GLenum binaryFormat,
                                                   const GLvoid* binary,
                                                   GLsizei length);
typedef void(GL_BINDING_CALL* glProgramParameteriProc)(GLuint program,
                                                       GLenum pname,
                                                       GLint value);
typedef void(GL_BINDING_CALL* glProgramPathFragmentInputGenNVProc)(
    GLuint program,
    GLint location,
    GLenum genMode,
    GLint components,
    const GLfloat* coeffs);
typedef void(GL_BINDING_CALL* glPushDebugGroupProc)(GLenum source,
                                                    GLuint id,
                                                    GLsizei length,
                                                    const char* message);
typedef void(GL_BINDING_CALL* glPushGroupMarkerEXTProc)(GLsizei length,
                                                        const char* marker);
typedef void(GL_BINDING_CALL* glQueryCounterProc)(GLuint id, GLenum target);
typedef void(GL_BINDING_CALL* glReadBufferProc)(GLenum src);
typedef void(GL_BINDING_CALL* glReadnPixelsRobustANGLEProc)(GLint x,
                                                            GLint y,
                                                            GLsizei width,
                                                            GLsizei height,
                                                            GLenum format,
                                                            GLenum type,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLsizei* columns,
                                                            GLsizei* rows,
                                                            void* data);
typedef void(GL_BINDING_CALL* glReadPixelsProc)(GLint x,
                                                GLint y,
                                                GLsizei width,
                                                GLsizei height,
                                                GLenum format,
                                                GLenum type,
                                                void* pixels);
typedef void(GL_BINDING_CALL* glReadPixelsRobustANGLEProc)(GLint x,
                                                           GLint y,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLenum format,
                                                           GLenum type,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLsizei* columns,
                                                           GLsizei* rows,
                                                           void* pixels);
typedef void(GL_BINDING_CALL* glReleaseShaderCompilerProc)(void);
typedef void(GL_BINDING_CALL* glRenderbufferStorageEXTProc)(
    GLenum target,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_BINDING_CALL* glRenderbufferStorageMultisampleProc)(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_BINDING_CALL* glRenderbufferStorageMultisampleEXTProc)(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_BINDING_CALL* glRequestExtensionANGLEProc)(const char* name);
typedef void(GL_BINDING_CALL* glResumeTransformFeedbackProc)(void);
typedef void(GL_BINDING_CALL* glSampleCoverageProc)(GLclampf value,
                                                    GLboolean invert);
typedef void(GL_BINDING_CALL* glSamplerParameterfProc)(GLuint sampler,
                                                       GLenum pname,
                                                       GLfloat param);
typedef void(GL_BINDING_CALL* glSamplerParameterfvProc)(GLuint sampler,
                                                        GLenum pname,
                                                        const GLfloat* params);
typedef void(GL_BINDING_CALL* glSamplerParameterfvRobustANGLEProc)(
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    const GLfloat* param);
typedef void(GL_BINDING_CALL* glSamplerParameteriProc)(GLuint sampler,
                                                       GLenum pname,
                                                       GLint param);
typedef void(GL_BINDING_CALL* glSamplerParameterIivRobustANGLEProc)(
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    const GLint* param);
typedef void(GL_BINDING_CALL* glSamplerParameterIuivRobustANGLEProc)(
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    const GLuint* param);
typedef void(GL_BINDING_CALL* glSamplerParameterivProc)(GLuint sampler,
                                                        GLenum pname,
                                                        const GLint* params);
typedef void(GL_BINDING_CALL* glSamplerParameterivRobustANGLEProc)(
    GLuint sampler,
    GLenum pname,
    GLsizei bufSize,
    const GLint* param);
typedef void(GL_BINDING_CALL* glScissorProc)(GLint x,
                                             GLint y,
                                             GLsizei width,
                                             GLsizei height);
typedef void(GL_BINDING_CALL* glSetFenceAPPLEProc)(GLuint fence);
typedef void(GL_BINDING_CALL* glSetFenceNVProc)(GLuint fence, GLenum condition);
typedef void(GL_BINDING_CALL* glShaderBinaryProc)(GLsizei n,
                                                  const GLuint* shaders,
                                                  GLenum binaryformat,
                                                  const void* binary,
                                                  GLsizei length);
typedef void(GL_BINDING_CALL* glShaderSourceProc)(GLuint shader,
                                                  GLsizei count,
                                                  const char* const* str,
                                                  const GLint* length);
typedef void(GL_BINDING_CALL* glStencilFillPathInstancedNVProc)(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues);
typedef void(GL_BINDING_CALL* glStencilFillPathNVProc)(GLuint path,
                                                       GLenum fillMode,
                                                       GLuint mask);
typedef void(GL_BINDING_CALL* glStencilFuncProc)(GLenum func,
                                                 GLint ref,
                                                 GLuint mask);
typedef void(GL_BINDING_CALL* glStencilFuncSeparateProc)(GLenum face,
                                                         GLenum func,
                                                         GLint ref,
                                                         GLuint mask);
typedef void(GL_BINDING_CALL* glStencilMaskProc)(GLuint mask);
typedef void(GL_BINDING_CALL* glStencilMaskSeparateProc)(GLenum face,
                                                         GLuint mask);
typedef void(GL_BINDING_CALL* glStencilOpProc)(GLenum fail,
                                               GLenum zfail,
                                               GLenum zpass);
typedef void(GL_BINDING_CALL* glStencilOpSeparateProc)(GLenum face,
                                                       GLenum fail,
                                                       GLenum zfail,
                                                       GLenum zpass);
typedef void(GL_BINDING_CALL* glStencilStrokePathInstancedNVProc)(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues);
typedef void(GL_BINDING_CALL* glStencilStrokePathNVProc)(GLuint path,
                                                         GLint reference,
                                                         GLuint mask);
typedef void(GL_BINDING_CALL* glStencilThenCoverFillPathInstancedNVProc)(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues);
typedef void(GL_BINDING_CALL* glStencilThenCoverFillPathNVProc)(
    GLuint path,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode);
typedef void(GL_BINDING_CALL* glStencilThenCoverStrokePathInstancedNVProc)(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues);
typedef void(GL_BINDING_CALL* glStencilThenCoverStrokePathNVProc)(
    GLuint path,
    GLint reference,
    GLuint mask,
    GLenum coverMode);
typedef GLboolean(GL_BINDING_CALL* glTestFenceAPPLEProc)(GLuint fence);
typedef GLboolean(GL_BINDING_CALL* glTestFenceNVProc)(GLuint fence);
typedef void(GL_BINDING_CALL* glTexBufferProc)(GLenum target,
                                               GLenum internalformat,
                                               GLuint buffer);
typedef void(GL_BINDING_CALL* glTexBufferRangeProc)(GLenum target,
                                                    GLenum internalformat,
                                                    GLuint buffer,
                                                    GLintptr offset,
                                                    GLsizeiptr size);
typedef void(GL_BINDING_CALL* glTexImage2DProc)(GLenum target,
                                                GLint level,
                                                GLint internalformat,
                                                GLsizei width,
                                                GLsizei height,
                                                GLint border,
                                                GLenum format,
                                                GLenum type,
                                                const void* pixels);
typedef void(GL_BINDING_CALL* glTexImage2DRobustANGLEProc)(GLenum target,
                                                           GLint level,
                                                           GLint internalformat,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLint border,
                                                           GLenum format,
                                                           GLenum type,
                                                           GLsizei bufSize,
                                                           const void* pixels);
typedef void(GL_BINDING_CALL* glTexImage3DProc)(GLenum target,
                                                GLint level,
                                                GLint internalformat,
                                                GLsizei width,
                                                GLsizei height,
                                                GLsizei depth,
                                                GLint border,
                                                GLenum format,
                                                GLenum type,
                                                const void* pixels);
typedef void(GL_BINDING_CALL* glTexImage3DRobustANGLEProc)(GLenum target,
                                                           GLint level,
                                                           GLint internalformat,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLsizei depth,
                                                           GLint border,
                                                           GLenum format,
                                                           GLenum type,
                                                           GLsizei bufSize,
                                                           const void* pixels);
typedef void(GL_BINDING_CALL* glTexParameterfProc)(GLenum target,
                                                   GLenum pname,
                                                   GLfloat param);
typedef void(GL_BINDING_CALL* glTexParameterfvProc)(GLenum target,
                                                    GLenum pname,
                                                    const GLfloat* params);
typedef void(GL_BINDING_CALL* glTexParameterfvRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    const GLfloat* params);
typedef void(GL_BINDING_CALL* glTexParameteriProc)(GLenum target,
                                                   GLenum pname,
                                                   GLint param);
typedef void(GL_BINDING_CALL* glTexParameterIivRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    const GLint* params);
typedef void(GL_BINDING_CALL* glTexParameterIuivRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    const GLuint* params);
typedef void(GL_BINDING_CALL* glTexParameterivProc)(GLenum target,
                                                    GLenum pname,
                                                    const GLint* params);
typedef void(GL_BINDING_CALL* glTexParameterivRobustANGLEProc)(
    GLenum target,
    GLenum pname,
    GLsizei bufSize,
    const GLint* params);
typedef void(GL_BINDING_CALL* glTexStorage2DEXTProc)(GLenum target,
                                                     GLsizei levels,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height);
typedef void(GL_BINDING_CALL* glTexStorage3DProc)(GLenum target,
                                                  GLsizei levels,
                                                  GLenum internalformat,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLsizei depth);
typedef void(GL_BINDING_CALL* glTexSubImage2DProc)(GLenum target,
                                                   GLint level,
                                                   GLint xoffset,
                                                   GLint yoffset,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLenum format,
                                                   GLenum type,
                                                   const void* pixels);
typedef void(GL_BINDING_CALL* glTexSubImage2DRobustANGLEProc)(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    GLsizei bufSize,
    const void* pixels);
typedef void(GL_BINDING_CALL* glTexSubImage3DProc)(GLenum target,
                                                   GLint level,
                                                   GLint xoffset,
                                                   GLint yoffset,
                                                   GLint zoffset,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLsizei depth,
                                                   GLenum format,
                                                   GLenum type,
                                                   const void* pixels);
typedef void(GL_BINDING_CALL* glTexSubImage3DRobustANGLEProc)(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLenum type,
    GLsizei bufSize,
    const void* pixels);
typedef void(GL_BINDING_CALL* glTransformFeedbackVaryingsProc)(
    GLuint program,
    GLsizei count,
    const char* const* varyings,
    GLenum bufferMode);
typedef void(GL_BINDING_CALL* glUniform1fProc)(GLint location, GLfloat x);
typedef void(GL_BINDING_CALL* glUniform1fvProc)(GLint location,
                                                GLsizei count,
                                                const GLfloat* v);
typedef void(GL_BINDING_CALL* glUniform1iProc)(GLint location, GLint x);
typedef void(GL_BINDING_CALL* glUniform1ivProc)(GLint location,
                                                GLsizei count,
                                                const GLint* v);
typedef void(GL_BINDING_CALL* glUniform1uiProc)(GLint location, GLuint v0);
typedef void(GL_BINDING_CALL* glUniform1uivProc)(GLint location,
                                                 GLsizei count,
                                                 const GLuint* v);
typedef void(GL_BINDING_CALL* glUniform2fProc)(GLint location,
                                               GLfloat x,
                                               GLfloat y);
typedef void(GL_BINDING_CALL* glUniform2fvProc)(GLint location,
                                                GLsizei count,
                                                const GLfloat* v);
typedef void(GL_BINDING_CALL* glUniform2iProc)(GLint location,
                                               GLint x,
                                               GLint y);
typedef void(GL_BINDING_CALL* glUniform2ivProc)(GLint location,
                                                GLsizei count,
                                                const GLint* v);
typedef void(GL_BINDING_CALL* glUniform2uiProc)(GLint location,
                                                GLuint v0,
                                                GLuint v1);
typedef void(GL_BINDING_CALL* glUniform2uivProc)(GLint location,
                                                 GLsizei count,
                                                 const GLuint* v);
typedef void(GL_BINDING_CALL* glUniform3fProc)(GLint location,
                                               GLfloat x,
                                               GLfloat y,
                                               GLfloat z);
typedef void(GL_BINDING_CALL* glUniform3fvProc)(GLint location,
                                                GLsizei count,
                                                const GLfloat* v);
typedef void(GL_BINDING_CALL* glUniform3iProc)(GLint location,
                                               GLint x,
                                               GLint y,
                                               GLint z);
typedef void(GL_BINDING_CALL* glUniform3ivProc)(GLint location,
                                                GLsizei count,
                                                const GLint* v);
typedef void(GL_BINDING_CALL* glUniform3uiProc)(GLint location,
                                                GLuint v0,
                                                GLuint v1,
                                                GLuint v2);
typedef void(GL_BINDING_CALL* glUniform3uivProc)(GLint location,
                                                 GLsizei count,
                                                 const GLuint* v);
typedef void(GL_BINDING_CALL* glUniform4fProc)(GLint location,
                                               GLfloat x,
                                               GLfloat y,
                                               GLfloat z,
                                               GLfloat w);
typedef void(GL_BINDING_CALL* glUniform4fvProc)(GLint location,
                                                GLsizei count,
                                                const GLfloat* v);
typedef void(GL_BINDING_CALL* glUniform4iProc)(GLint location,
                                               GLint x,
                                               GLint y,
                                               GLint z,
                                               GLint w);
typedef void(GL_BINDING_CALL* glUniform4ivProc)(GLint location,
                                                GLsizei count,
                                                const GLint* v);
typedef void(GL_BINDING_CALL* glUniform4uiProc)(GLint location,
                                                GLuint v0,
                                                GLuint v1,
                                                GLuint v2,
                                                GLuint v3);
typedef void(GL_BINDING_CALL* glUniform4uivProc)(GLint location,
                                                 GLsizei count,
                                                 const GLuint* v);
typedef void(GL_BINDING_CALL* glUniformBlockBindingProc)(
    GLuint program,
    GLuint uniformBlockIndex,
    GLuint uniformBlockBinding);
typedef void(GL_BINDING_CALL* glUniformMatrix2fvProc)(GLint location,
                                                      GLsizei count,
                                                      GLboolean transpose,
                                                      const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix2x3fvProc)(GLint location,
                                                        GLsizei count,
                                                        GLboolean transpose,
                                                        const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix2x4fvProc)(GLint location,
                                                        GLsizei count,
                                                        GLboolean transpose,
                                                        const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix3fvProc)(GLint location,
                                                      GLsizei count,
                                                      GLboolean transpose,
                                                      const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix3x2fvProc)(GLint location,
                                                        GLsizei count,
                                                        GLboolean transpose,
                                                        const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix3x4fvProc)(GLint location,
                                                        GLsizei count,
                                                        GLboolean transpose,
                                                        const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix4fvProc)(GLint location,
                                                      GLsizei count,
                                                      GLboolean transpose,
                                                      const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix4x2fvProc)(GLint location,
                                                        GLsizei count,
                                                        GLboolean transpose,
                                                        const GLfloat* value);
typedef void(GL_BINDING_CALL* glUniformMatrix4x3fvProc)(GLint location,
                                                        GLsizei count,
                                                        GLboolean transpose,
                                                        const GLfloat* value);
typedef GLboolean(GL_BINDING_CALL* glUnmapBufferProc)(GLenum target);
typedef void(GL_BINDING_CALL* glUseProgramProc)(GLuint program);
typedef void(GL_BINDING_CALL* glValidateProgramProc)(GLuint program);
typedef void(GL_BINDING_CALL* glVertexAttrib1fProc)(GLuint indx, GLfloat x);
typedef void(GL_BINDING_CALL* glVertexAttrib1fvProc)(GLuint indx,
                                                     const GLfloat* values);
typedef void(GL_BINDING_CALL* glVertexAttrib2fProc)(GLuint indx,
                                                    GLfloat x,
                                                    GLfloat y);
typedef void(GL_BINDING_CALL* glVertexAttrib2fvProc)(GLuint indx,
                                                     const GLfloat* values);
typedef void(GL_BINDING_CALL* glVertexAttrib3fProc)(GLuint indx,
                                                    GLfloat x,
                                                    GLfloat y,
                                                    GLfloat z);
typedef void(GL_BINDING_CALL* glVertexAttrib3fvProc)(GLuint indx,
                                                     const GLfloat* values);
typedef void(GL_BINDING_CALL* glVertexAttrib4fProc)(GLuint indx,
                                                    GLfloat x,
                                                    GLfloat y,
                                                    GLfloat z,
                                                    GLfloat w);
typedef void(GL_BINDING_CALL* glVertexAttrib4fvProc)(GLuint indx,
                                                     const GLfloat* values);
typedef void(GL_BINDING_CALL* glVertexAttribDivisorANGLEProc)(GLuint index,
                                                              GLuint divisor);
typedef void(GL_BINDING_CALL* glVertexAttribI4iProc)(GLuint indx,
                                                     GLint x,
                                                     GLint y,
                                                     GLint z,
                                                     GLint w);
typedef void(GL_BINDING_CALL* glVertexAttribI4ivProc)(GLuint indx,
                                                      const GLint* values);
typedef void(GL_BINDING_CALL* glVertexAttribI4uiProc)(GLuint indx,
                                                      GLuint x,
                                                      GLuint y,
                                                      GLuint z,
                                                      GLuint w);
typedef void(GL_BINDING_CALL* glVertexAttribI4uivProc)(GLuint indx,
                                                       const GLuint* values);
typedef void(GL_BINDING_CALL* glVertexAttribIPointerProc)(GLuint indx,
                                                          GLint size,
                                                          GLenum type,
                                                          GLsizei stride,
                                                          const void* ptr);
typedef void(GL_BINDING_CALL* glVertexAttribPointerProc)(GLuint indx,
                                                         GLint size,
                                                         GLenum type,
                                                         GLboolean normalized,
                                                         GLsizei stride,
                                                         const void* ptr);
typedef void(GL_BINDING_CALL* glViewportProc)(GLint x,
                                              GLint y,
                                              GLsizei width,
                                              GLsizei height);
typedef void(GL_BINDING_CALL* glWaitSyncProc)(GLsync sync,
                                              GLbitfield flags,
                                              GLuint64 timeout);
typedef void(GL_BINDING_CALL* glWindowRectanglesEXTProc)(GLenum mode,
                                                         GLsizei n,
                                                         const GLint* box);

struct ExtensionsGL {
  bool b_GL_ANGLE_framebuffer_blit;
  bool b_GL_ANGLE_framebuffer_multisample;
  bool b_GL_ANGLE_instanced_arrays;
  bool b_GL_ANGLE_request_extension;
  bool b_GL_ANGLE_robust_client_memory;
  bool b_GL_ANGLE_translated_shader_source;
  bool b_GL_APPLE_fence;
  bool b_GL_APPLE_vertex_array_object;
  bool b_GL_ARB_blend_func_extended;
  bool b_GL_ARB_draw_buffers;
  bool b_GL_ARB_draw_instanced;
  bool b_GL_ARB_framebuffer_object;
  bool b_GL_ARB_get_program_binary;
  bool b_GL_ARB_instanced_arrays;
  bool b_GL_ARB_internalformat_query;
  bool b_GL_ARB_map_buffer_range;
  bool b_GL_ARB_occlusion_query;
  bool b_GL_ARB_program_interface_query;
  bool b_GL_ARB_robustness;
  bool b_GL_ARB_sampler_objects;
  bool b_GL_ARB_shader_image_load_store;
  bool b_GL_ARB_sync;
  bool b_GL_ARB_texture_multisample;
  bool b_GL_ARB_texture_storage;
  bool b_GL_ARB_timer_query;
  bool b_GL_ARB_transform_feedback2;
  bool b_GL_ARB_vertex_array_object;
  bool b_GL_CHROMIUM_bind_uniform_location;
  bool b_GL_CHROMIUM_compressed_copy_texture;
  bool b_GL_CHROMIUM_copy_compressed_texture;
  bool b_GL_CHROMIUM_copy_texture;
  bool b_GL_CHROMIUM_gles_depth_binding_hack;
  bool b_GL_CHROMIUM_glgetstringi_hack;
  bool b_GL_EXT_blend_func_extended;
  bool b_GL_EXT_debug_marker;
  bool b_GL_EXT_direct_state_access;
  bool b_GL_EXT_discard_framebuffer;
  bool b_GL_EXT_disjoint_timer_query;
  bool b_GL_EXT_draw_buffers;
  bool b_GL_EXT_framebuffer_blit;
  bool b_GL_EXT_framebuffer_multisample;
  bool b_GL_EXT_framebuffer_object;
  bool b_GL_EXT_gpu_shader4;
  bool b_GL_EXT_instanced_arrays;
  bool b_GL_EXT_map_buffer_range;
  bool b_GL_EXT_multisampled_render_to_texture;
  bool b_GL_EXT_occlusion_query_boolean;
  bool b_GL_EXT_robustness;
  bool b_GL_EXT_shader_image_load_store;
  bool b_GL_EXT_texture_buffer;
  bool b_GL_EXT_texture_buffer_object;
  bool b_GL_EXT_texture_storage;
  bool b_GL_EXT_timer_query;
  bool b_GL_EXT_transform_feedback;
  bool b_GL_EXT_unpack_subimage;
  bool b_GL_EXT_window_rectangles;
  bool b_GL_IMG_multisampled_render_to_texture;
  bool b_GL_INTEL_framebuffer_CMAA;
  bool b_GL_KHR_blend_equation_advanced;
  bool b_GL_KHR_debug;
  bool b_GL_KHR_robustness;
  bool b_GL_NV_blend_equation_advanced;
  bool b_GL_NV_fence;
  bool b_GL_NV_framebuffer_mixed_samples;
  bool b_GL_NV_path_rendering;
  bool b_GL_OES_EGL_image;
  bool b_GL_OES_get_program_binary;
  bool b_GL_OES_mapbuffer;
  bool b_GL_OES_texture_buffer;
  bool b_GL_OES_vertex_array_object;
};

struct ProcsGL {
  glActiveTextureProc glActiveTextureFn;
  glApplyFramebufferAttachmentCMAAINTELProc
      glApplyFramebufferAttachmentCMAAINTELFn;
  glAttachShaderProc glAttachShaderFn;
  glBeginQueryProc glBeginQueryFn;
  glBeginTransformFeedbackProc glBeginTransformFeedbackFn;
  glBindAttribLocationProc glBindAttribLocationFn;
  glBindBufferProc glBindBufferFn;
  glBindBufferBaseProc glBindBufferBaseFn;
  glBindBufferRangeProc glBindBufferRangeFn;
  glBindFragDataLocationProc glBindFragDataLocationFn;
  glBindFragDataLocationIndexedProc glBindFragDataLocationIndexedFn;
  glBindFramebufferEXTProc glBindFramebufferEXTFn;
  glBindImageTextureEXTProc glBindImageTextureEXTFn;
  glBindRenderbufferEXTProc glBindRenderbufferEXTFn;
  glBindSamplerProc glBindSamplerFn;
  glBindTextureProc glBindTextureFn;
  glBindTransformFeedbackProc glBindTransformFeedbackFn;
  glBindUniformLocationCHROMIUMProc glBindUniformLocationCHROMIUMFn;
  glBindVertexArrayOESProc glBindVertexArrayOESFn;
  glBlendBarrierKHRProc glBlendBarrierKHRFn;
  glBlendColorProc glBlendColorFn;
  glBlendEquationProc glBlendEquationFn;
  glBlendEquationSeparateProc glBlendEquationSeparateFn;
  glBlendFuncProc glBlendFuncFn;
  glBlendFuncSeparateProc glBlendFuncSeparateFn;
  glBlitFramebufferProc glBlitFramebufferFn;
  glBufferDataProc glBufferDataFn;
  glBufferSubDataProc glBufferSubDataFn;
  glCheckFramebufferStatusEXTProc glCheckFramebufferStatusEXTFn;
  glClearProc glClearFn;
  glClearBufferfiProc glClearBufferfiFn;
  glClearBufferfvProc glClearBufferfvFn;
  glClearBufferivProc glClearBufferivFn;
  glClearBufferuivProc glClearBufferuivFn;
  glClearColorProc glClearColorFn;
  glClearDepthProc glClearDepthFn;
  glClearDepthfProc glClearDepthfFn;
  glClearStencilProc glClearStencilFn;
  glClientWaitSyncProc glClientWaitSyncFn;
  glColorMaskProc glColorMaskFn;
  glCompileShaderProc glCompileShaderFn;
  glCompressedCopyTextureCHROMIUMProc glCompressedCopyTextureCHROMIUMFn;
  glCompressedTexImage2DProc glCompressedTexImage2DFn;
  glCompressedTexImage2DRobustANGLEProc glCompressedTexImage2DRobustANGLEFn;
  glCompressedTexImage3DProc glCompressedTexImage3DFn;
  glCompressedTexImage3DRobustANGLEProc glCompressedTexImage3DRobustANGLEFn;
  glCompressedTexSubImage2DProc glCompressedTexSubImage2DFn;
  glCompressedTexSubImage2DRobustANGLEProc
      glCompressedTexSubImage2DRobustANGLEFn;
  glCompressedTexSubImage3DProc glCompressedTexSubImage3DFn;
  glCompressedTexSubImage3DRobustANGLEProc
      glCompressedTexSubImage3DRobustANGLEFn;
  glCopyBufferSubDataProc glCopyBufferSubDataFn;
  glCopySubTextureCHROMIUMProc glCopySubTextureCHROMIUMFn;
  glCopyTexImage2DProc glCopyTexImage2DFn;
  glCopyTexSubImage2DProc glCopyTexSubImage2DFn;
  glCopyTexSubImage3DProc glCopyTexSubImage3DFn;
  glCopyTextureCHROMIUMProc glCopyTextureCHROMIUMFn;
  glCoverageModulationNVProc glCoverageModulationNVFn;
  glCoverFillPathInstancedNVProc glCoverFillPathInstancedNVFn;
  glCoverFillPathNVProc glCoverFillPathNVFn;
  glCoverStrokePathInstancedNVProc glCoverStrokePathInstancedNVFn;
  glCoverStrokePathNVProc glCoverStrokePathNVFn;
  glCreateProgramProc glCreateProgramFn;
  glCreateShaderProc glCreateShaderFn;
  glCullFaceProc glCullFaceFn;
  glDebugMessageCallbackProc glDebugMessageCallbackFn;
  glDebugMessageControlProc glDebugMessageControlFn;
  glDebugMessageInsertProc glDebugMessageInsertFn;
  glDeleteBuffersARBProc glDeleteBuffersARBFn;
  glDeleteFencesAPPLEProc glDeleteFencesAPPLEFn;
  glDeleteFencesNVProc glDeleteFencesNVFn;
  glDeleteFramebuffersEXTProc glDeleteFramebuffersEXTFn;
  glDeletePathsNVProc glDeletePathsNVFn;
  glDeleteProgramProc glDeleteProgramFn;
  glDeleteQueriesProc glDeleteQueriesFn;
  glDeleteRenderbuffersEXTProc glDeleteRenderbuffersEXTFn;
  glDeleteSamplersProc glDeleteSamplersFn;
  glDeleteShaderProc glDeleteShaderFn;
  glDeleteSyncProc glDeleteSyncFn;
  glDeleteTexturesProc glDeleteTexturesFn;
  glDeleteTransformFeedbacksProc glDeleteTransformFeedbacksFn;
  glDeleteVertexArraysOESProc glDeleteVertexArraysOESFn;
  glDepthFuncProc glDepthFuncFn;
  glDepthMaskProc glDepthMaskFn;
  glDepthRangeProc glDepthRangeFn;
  glDepthRangefProc glDepthRangefFn;
  glDetachShaderProc glDetachShaderFn;
  glDisableProc glDisableFn;
  glDisableVertexAttribArrayProc glDisableVertexAttribArrayFn;
  glDiscardFramebufferEXTProc glDiscardFramebufferEXTFn;
  glDrawArraysProc glDrawArraysFn;
  glDrawArraysInstancedANGLEProc glDrawArraysInstancedANGLEFn;
  glDrawBufferProc glDrawBufferFn;
  glDrawBuffersARBProc glDrawBuffersARBFn;
  glDrawElementsProc glDrawElementsFn;
  glDrawElementsInstancedANGLEProc glDrawElementsInstancedANGLEFn;
  glDrawRangeElementsProc glDrawRangeElementsFn;
  glEGLImageTargetRenderbufferStorageOESProc
      glEGLImageTargetRenderbufferStorageOESFn;
  glEGLImageTargetTexture2DOESProc glEGLImageTargetTexture2DOESFn;
  glEnableProc glEnableFn;
  glEnableVertexAttribArrayProc glEnableVertexAttribArrayFn;
  glEndQueryProc glEndQueryFn;
  glEndTransformFeedbackProc glEndTransformFeedbackFn;
  glFenceSyncProc glFenceSyncFn;
  glFinishProc glFinishFn;
  glFinishFenceAPPLEProc glFinishFenceAPPLEFn;
  glFinishFenceNVProc glFinishFenceNVFn;
  glFlushProc glFlushFn;
  glFlushMappedBufferRangeProc glFlushMappedBufferRangeFn;
  glFramebufferRenderbufferEXTProc glFramebufferRenderbufferEXTFn;
  glFramebufferTexture2DEXTProc glFramebufferTexture2DEXTFn;
  glFramebufferTexture2DMultisampleEXTProc
      glFramebufferTexture2DMultisampleEXTFn;
  glFramebufferTextureLayerProc glFramebufferTextureLayerFn;
  glFrontFaceProc glFrontFaceFn;
  glGenBuffersARBProc glGenBuffersARBFn;
  glGenerateMipmapEXTProc glGenerateMipmapEXTFn;
  glGenFencesAPPLEProc glGenFencesAPPLEFn;
  glGenFencesNVProc glGenFencesNVFn;
  glGenFramebuffersEXTProc glGenFramebuffersEXTFn;
  glGenPathsNVProc glGenPathsNVFn;
  glGenQueriesProc glGenQueriesFn;
  glGenRenderbuffersEXTProc glGenRenderbuffersEXTFn;
  glGenSamplersProc glGenSamplersFn;
  glGenTexturesProc glGenTexturesFn;
  glGenTransformFeedbacksProc glGenTransformFeedbacksFn;
  glGenVertexArraysOESProc glGenVertexArraysOESFn;
  glGetActiveAttribProc glGetActiveAttribFn;
  glGetActiveUniformProc glGetActiveUniformFn;
  glGetActiveUniformBlockivProc glGetActiveUniformBlockivFn;
  glGetActiveUniformBlockivRobustANGLEProc
      glGetActiveUniformBlockivRobustANGLEFn;
  glGetActiveUniformBlockNameProc glGetActiveUniformBlockNameFn;
  glGetActiveUniformsivProc glGetActiveUniformsivFn;
  glGetAttachedShadersProc glGetAttachedShadersFn;
  glGetAttribLocationProc glGetAttribLocationFn;
  glGetBooleani_vRobustANGLEProc glGetBooleani_vRobustANGLEFn;
  glGetBooleanvProc glGetBooleanvFn;
  glGetBooleanvRobustANGLEProc glGetBooleanvRobustANGLEFn;
  glGetBufferParameteri64vRobustANGLEProc glGetBufferParameteri64vRobustANGLEFn;
  glGetBufferParameterivProc glGetBufferParameterivFn;
  glGetBufferParameterivRobustANGLEProc glGetBufferParameterivRobustANGLEFn;
  glGetBufferPointervRobustANGLEProc glGetBufferPointervRobustANGLEFn;
  glGetDebugMessageLogProc glGetDebugMessageLogFn;
  glGetErrorProc glGetErrorFn;
  glGetFenceivNVProc glGetFenceivNVFn;
  glGetFloatvProc glGetFloatvFn;
  glGetFloatvRobustANGLEProc glGetFloatvRobustANGLEFn;
  glGetFragDataIndexProc glGetFragDataIndexFn;
  glGetFragDataLocationProc glGetFragDataLocationFn;
  glGetFramebufferAttachmentParameterivEXTProc
      glGetFramebufferAttachmentParameterivEXTFn;
  glGetFramebufferAttachmentParameterivRobustANGLEProc
      glGetFramebufferAttachmentParameterivRobustANGLEFn;
  glGetFramebufferParameterivRobustANGLEProc
      glGetFramebufferParameterivRobustANGLEFn;
  glGetGraphicsResetStatusARBProc glGetGraphicsResetStatusARBFn;
  glGetInteger64i_vProc glGetInteger64i_vFn;
  glGetInteger64i_vRobustANGLEProc glGetInteger64i_vRobustANGLEFn;
  glGetInteger64vProc glGetInteger64vFn;
  glGetInteger64vRobustANGLEProc glGetInteger64vRobustANGLEFn;
  glGetIntegeri_vProc glGetIntegeri_vFn;
  glGetIntegeri_vRobustANGLEProc glGetIntegeri_vRobustANGLEFn;
  glGetIntegervProc glGetIntegervFn;
  glGetIntegervRobustANGLEProc glGetIntegervRobustANGLEFn;
  glGetInternalformativProc glGetInternalformativFn;
  glGetInternalformativRobustANGLEProc glGetInternalformativRobustANGLEFn;
  glGetMultisamplefvProc glGetMultisamplefvFn;
  glGetMultisamplefvRobustANGLEProc glGetMultisamplefvRobustANGLEFn;
  glGetnUniformfvRobustANGLEProc glGetnUniformfvRobustANGLEFn;
  glGetnUniformivRobustANGLEProc glGetnUniformivRobustANGLEFn;
  glGetnUniformuivRobustANGLEProc glGetnUniformuivRobustANGLEFn;
  glGetObjectLabelProc glGetObjectLabelFn;
  glGetObjectPtrLabelProc glGetObjectPtrLabelFn;
  glGetPointervProc glGetPointervFn;
  glGetPointervRobustANGLERobustANGLEProc glGetPointervRobustANGLERobustANGLEFn;
  glGetProgramBinaryProc glGetProgramBinaryFn;
  glGetProgramInfoLogProc glGetProgramInfoLogFn;
  glGetProgramInterfaceivProc glGetProgramInterfaceivFn;
  glGetProgramInterfaceivRobustANGLEProc glGetProgramInterfaceivRobustANGLEFn;
  glGetProgramivProc glGetProgramivFn;
  glGetProgramivRobustANGLEProc glGetProgramivRobustANGLEFn;
  glGetProgramResourceivProc glGetProgramResourceivFn;
  glGetProgramResourceLocationProc glGetProgramResourceLocationFn;
  glGetProgramResourceNameProc glGetProgramResourceNameFn;
  glGetQueryivProc glGetQueryivFn;
  glGetQueryivRobustANGLEProc glGetQueryivRobustANGLEFn;
  glGetQueryObjecti64vProc glGetQueryObjecti64vFn;
  glGetQueryObjecti64vRobustANGLEProc glGetQueryObjecti64vRobustANGLEFn;
  glGetQueryObjectivProc glGetQueryObjectivFn;
  glGetQueryObjectivRobustANGLEProc glGetQueryObjectivRobustANGLEFn;
  glGetQueryObjectui64vProc glGetQueryObjectui64vFn;
  glGetQueryObjectui64vRobustANGLEProc glGetQueryObjectui64vRobustANGLEFn;
  glGetQueryObjectuivProc glGetQueryObjectuivFn;
  glGetQueryObjectuivRobustANGLEProc glGetQueryObjectuivRobustANGLEFn;
  glGetRenderbufferParameterivEXTProc glGetRenderbufferParameterivEXTFn;
  glGetRenderbufferParameterivRobustANGLEProc
      glGetRenderbufferParameterivRobustANGLEFn;
  glGetSamplerParameterfvProc glGetSamplerParameterfvFn;
  glGetSamplerParameterfvRobustANGLEProc glGetSamplerParameterfvRobustANGLEFn;
  glGetSamplerParameterIivRobustANGLEProc glGetSamplerParameterIivRobustANGLEFn;
  glGetSamplerParameterIuivRobustANGLEProc
      glGetSamplerParameterIuivRobustANGLEFn;
  glGetSamplerParameterivProc glGetSamplerParameterivFn;
  glGetSamplerParameterivRobustANGLEProc glGetSamplerParameterivRobustANGLEFn;
  glGetShaderInfoLogProc glGetShaderInfoLogFn;
  glGetShaderivProc glGetShaderivFn;
  glGetShaderivRobustANGLEProc glGetShaderivRobustANGLEFn;
  glGetShaderPrecisionFormatProc glGetShaderPrecisionFormatFn;
  glGetShaderSourceProc glGetShaderSourceFn;
  glGetStringProc glGetStringFn;
  glGetStringiProc glGetStringiFn;
  glGetSyncivProc glGetSyncivFn;
  glGetTexLevelParameterfvProc glGetTexLevelParameterfvFn;
  glGetTexLevelParameterfvRobustANGLEProc glGetTexLevelParameterfvRobustANGLEFn;
  glGetTexLevelParameterivProc glGetTexLevelParameterivFn;
  glGetTexLevelParameterivRobustANGLEProc glGetTexLevelParameterivRobustANGLEFn;
  glGetTexParameterfvProc glGetTexParameterfvFn;
  glGetTexParameterfvRobustANGLEProc glGetTexParameterfvRobustANGLEFn;
  glGetTexParameterIivRobustANGLEProc glGetTexParameterIivRobustANGLEFn;
  glGetTexParameterIuivRobustANGLEProc glGetTexParameterIuivRobustANGLEFn;
  glGetTexParameterivProc glGetTexParameterivFn;
  glGetTexParameterivRobustANGLEProc glGetTexParameterivRobustANGLEFn;
  glGetTransformFeedbackVaryingProc glGetTransformFeedbackVaryingFn;
  glGetTranslatedShaderSourceANGLEProc glGetTranslatedShaderSourceANGLEFn;
  glGetUniformBlockIndexProc glGetUniformBlockIndexFn;
  glGetUniformfvProc glGetUniformfvFn;
  glGetUniformfvRobustANGLEProc glGetUniformfvRobustANGLEFn;
  glGetUniformIndicesProc glGetUniformIndicesFn;
  glGetUniformivProc glGetUniformivFn;
  glGetUniformivRobustANGLEProc glGetUniformivRobustANGLEFn;
  glGetUniformLocationProc glGetUniformLocationFn;
  glGetUniformuivProc glGetUniformuivFn;
  glGetUniformuivRobustANGLEProc glGetUniformuivRobustANGLEFn;
  glGetVertexAttribfvProc glGetVertexAttribfvFn;
  glGetVertexAttribfvRobustANGLEProc glGetVertexAttribfvRobustANGLEFn;
  glGetVertexAttribIivRobustANGLEProc glGetVertexAttribIivRobustANGLEFn;
  glGetVertexAttribIuivRobustANGLEProc glGetVertexAttribIuivRobustANGLEFn;
  glGetVertexAttribivProc glGetVertexAttribivFn;
  glGetVertexAttribivRobustANGLEProc glGetVertexAttribivRobustANGLEFn;
  glGetVertexAttribPointervProc glGetVertexAttribPointervFn;
  glGetVertexAttribPointervRobustANGLEProc
      glGetVertexAttribPointervRobustANGLEFn;
  glHintProc glHintFn;
  glInsertEventMarkerEXTProc glInsertEventMarkerEXTFn;
  glInvalidateFramebufferProc glInvalidateFramebufferFn;
  glInvalidateSubFramebufferProc glInvalidateSubFramebufferFn;
  glIsBufferProc glIsBufferFn;
  glIsEnabledProc glIsEnabledFn;
  glIsFenceAPPLEProc glIsFenceAPPLEFn;
  glIsFenceNVProc glIsFenceNVFn;
  glIsFramebufferEXTProc glIsFramebufferEXTFn;
  glIsPathNVProc glIsPathNVFn;
  glIsProgramProc glIsProgramFn;
  glIsQueryProc glIsQueryFn;
  glIsRenderbufferEXTProc glIsRenderbufferEXTFn;
  glIsSamplerProc glIsSamplerFn;
  glIsShaderProc glIsShaderFn;
  glIsSyncProc glIsSyncFn;
  glIsTextureProc glIsTextureFn;
  glIsTransformFeedbackProc glIsTransformFeedbackFn;
  glIsVertexArrayOESProc glIsVertexArrayOESFn;
  glLineWidthProc glLineWidthFn;
  glLinkProgramProc glLinkProgramFn;
  glMapBufferProc glMapBufferFn;
  glMapBufferRangeProc glMapBufferRangeFn;
  glMatrixLoadfEXTProc glMatrixLoadfEXTFn;
  glMatrixLoadIdentityEXTProc glMatrixLoadIdentityEXTFn;
  glMemoryBarrierEXTProc glMemoryBarrierEXTFn;
  glObjectLabelProc glObjectLabelFn;
  glObjectPtrLabelProc glObjectPtrLabelFn;
  glPathCommandsNVProc glPathCommandsNVFn;
  glPathParameterfNVProc glPathParameterfNVFn;
  glPathParameteriNVProc glPathParameteriNVFn;
  glPathStencilFuncNVProc glPathStencilFuncNVFn;
  glPauseTransformFeedbackProc glPauseTransformFeedbackFn;
  glPixelStoreiProc glPixelStoreiFn;
  glPointParameteriProc glPointParameteriFn;
  glPolygonModeProc glPolygonModeFn;
  glPolygonOffsetProc glPolygonOffsetFn;
  glPopDebugGroupProc glPopDebugGroupFn;
  glPopGroupMarkerEXTProc glPopGroupMarkerEXTFn;
  glPrimitiveRestartIndexProc glPrimitiveRestartIndexFn;
  glProgramBinaryProc glProgramBinaryFn;
  glProgramParameteriProc glProgramParameteriFn;
  glProgramPathFragmentInputGenNVProc glProgramPathFragmentInputGenNVFn;
  glPushDebugGroupProc glPushDebugGroupFn;
  glPushGroupMarkerEXTProc glPushGroupMarkerEXTFn;
  glQueryCounterProc glQueryCounterFn;
  glReadBufferProc glReadBufferFn;
  glReadnPixelsRobustANGLEProc glReadnPixelsRobustANGLEFn;
  glReadPixelsProc glReadPixelsFn;
  glReadPixelsRobustANGLEProc glReadPixelsRobustANGLEFn;
  glReleaseShaderCompilerProc glReleaseShaderCompilerFn;
  glRenderbufferStorageEXTProc glRenderbufferStorageEXTFn;
  glRenderbufferStorageMultisampleProc glRenderbufferStorageMultisampleFn;
  glRenderbufferStorageMultisampleEXTProc glRenderbufferStorageMultisampleEXTFn;
  glRequestExtensionANGLEProc glRequestExtensionANGLEFn;
  glResumeTransformFeedbackProc glResumeTransformFeedbackFn;
  glSampleCoverageProc glSampleCoverageFn;
  glSamplerParameterfProc glSamplerParameterfFn;
  glSamplerParameterfvProc glSamplerParameterfvFn;
  glSamplerParameterfvRobustANGLEProc glSamplerParameterfvRobustANGLEFn;
  glSamplerParameteriProc glSamplerParameteriFn;
  glSamplerParameterIivRobustANGLEProc glSamplerParameterIivRobustANGLEFn;
  glSamplerParameterIuivRobustANGLEProc glSamplerParameterIuivRobustANGLEFn;
  glSamplerParameterivProc glSamplerParameterivFn;
  glSamplerParameterivRobustANGLEProc glSamplerParameterivRobustANGLEFn;
  glScissorProc glScissorFn;
  glSetFenceAPPLEProc glSetFenceAPPLEFn;
  glSetFenceNVProc glSetFenceNVFn;
  glShaderBinaryProc glShaderBinaryFn;
  glShaderSourceProc glShaderSourceFn;
  glStencilFillPathInstancedNVProc glStencilFillPathInstancedNVFn;
  glStencilFillPathNVProc glStencilFillPathNVFn;
  glStencilFuncProc glStencilFuncFn;
  glStencilFuncSeparateProc glStencilFuncSeparateFn;
  glStencilMaskProc glStencilMaskFn;
  glStencilMaskSeparateProc glStencilMaskSeparateFn;
  glStencilOpProc glStencilOpFn;
  glStencilOpSeparateProc glStencilOpSeparateFn;
  glStencilStrokePathInstancedNVProc glStencilStrokePathInstancedNVFn;
  glStencilStrokePathNVProc glStencilStrokePathNVFn;
  glStencilThenCoverFillPathInstancedNVProc
      glStencilThenCoverFillPathInstancedNVFn;
  glStencilThenCoverFillPathNVProc glStencilThenCoverFillPathNVFn;
  glStencilThenCoverStrokePathInstancedNVProc
      glStencilThenCoverStrokePathInstancedNVFn;
  glStencilThenCoverStrokePathNVProc glStencilThenCoverStrokePathNVFn;
  glTestFenceAPPLEProc glTestFenceAPPLEFn;
  glTestFenceNVProc glTestFenceNVFn;
  glTexBufferProc glTexBufferFn;
  glTexBufferRangeProc glTexBufferRangeFn;
  glTexImage2DProc glTexImage2DFn;
  glTexImage2DRobustANGLEProc glTexImage2DRobustANGLEFn;
  glTexImage3DProc glTexImage3DFn;
  glTexImage3DRobustANGLEProc glTexImage3DRobustANGLEFn;
  glTexParameterfProc glTexParameterfFn;
  glTexParameterfvProc glTexParameterfvFn;
  glTexParameterfvRobustANGLEProc glTexParameterfvRobustANGLEFn;
  glTexParameteriProc glTexParameteriFn;
  glTexParameterIivRobustANGLEProc glTexParameterIivRobustANGLEFn;
  glTexParameterIuivRobustANGLEProc glTexParameterIuivRobustANGLEFn;
  glTexParameterivProc glTexParameterivFn;
  glTexParameterivRobustANGLEProc glTexParameterivRobustANGLEFn;
  glTexStorage2DEXTProc glTexStorage2DEXTFn;
  glTexStorage3DProc glTexStorage3DFn;
  glTexSubImage2DProc glTexSubImage2DFn;
  glTexSubImage2DRobustANGLEProc glTexSubImage2DRobustANGLEFn;
  glTexSubImage3DProc glTexSubImage3DFn;
  glTexSubImage3DRobustANGLEProc glTexSubImage3DRobustANGLEFn;
  glTransformFeedbackVaryingsProc glTransformFeedbackVaryingsFn;
  glUniform1fProc glUniform1fFn;
  glUniform1fvProc glUniform1fvFn;
  glUniform1iProc glUniform1iFn;
  glUniform1ivProc glUniform1ivFn;
  glUniform1uiProc glUniform1uiFn;
  glUniform1uivProc glUniform1uivFn;
  glUniform2fProc glUniform2fFn;
  glUniform2fvProc glUniform2fvFn;
  glUniform2iProc glUniform2iFn;
  glUniform2ivProc glUniform2ivFn;
  glUniform2uiProc glUniform2uiFn;
  glUniform2uivProc glUniform2uivFn;
  glUniform3fProc glUniform3fFn;
  glUniform3fvProc glUniform3fvFn;
  glUniform3iProc glUniform3iFn;
  glUniform3ivProc glUniform3ivFn;
  glUniform3uiProc glUniform3uiFn;
  glUniform3uivProc glUniform3uivFn;
  glUniform4fProc glUniform4fFn;
  glUniform4fvProc glUniform4fvFn;
  glUniform4iProc glUniform4iFn;
  glUniform4ivProc glUniform4ivFn;
  glUniform4uiProc glUniform4uiFn;
  glUniform4uivProc glUniform4uivFn;
  glUniformBlockBindingProc glUniformBlockBindingFn;
  glUniformMatrix2fvProc glUniformMatrix2fvFn;
  glUniformMatrix2x3fvProc glUniformMatrix2x3fvFn;
  glUniformMatrix2x4fvProc glUniformMatrix2x4fvFn;
  glUniformMatrix3fvProc glUniformMatrix3fvFn;
  glUniformMatrix3x2fvProc glUniformMatrix3x2fvFn;
  glUniformMatrix3x4fvProc glUniformMatrix3x4fvFn;
  glUniformMatrix4fvProc glUniformMatrix4fvFn;
  glUniformMatrix4x2fvProc glUniformMatrix4x2fvFn;
  glUniformMatrix4x3fvProc glUniformMatrix4x3fvFn;
  glUnmapBufferProc glUnmapBufferFn;
  glUseProgramProc glUseProgramFn;
  glValidateProgramProc glValidateProgramFn;
  glVertexAttrib1fProc glVertexAttrib1fFn;
  glVertexAttrib1fvProc glVertexAttrib1fvFn;
  glVertexAttrib2fProc glVertexAttrib2fFn;
  glVertexAttrib2fvProc glVertexAttrib2fvFn;
  glVertexAttrib3fProc glVertexAttrib3fFn;
  glVertexAttrib3fvProc glVertexAttrib3fvFn;
  glVertexAttrib4fProc glVertexAttrib4fFn;
  glVertexAttrib4fvProc glVertexAttrib4fvFn;
  glVertexAttribDivisorANGLEProc glVertexAttribDivisorANGLEFn;
  glVertexAttribI4iProc glVertexAttribI4iFn;
  glVertexAttribI4ivProc glVertexAttribI4ivFn;
  glVertexAttribI4uiProc glVertexAttribI4uiFn;
  glVertexAttribI4uivProc glVertexAttribI4uivFn;
  glVertexAttribIPointerProc glVertexAttribIPointerFn;
  glVertexAttribPointerProc glVertexAttribPointerFn;
  glViewportProc glViewportFn;
  glWaitSyncProc glWaitSyncFn;
  glWindowRectanglesEXTProc glWindowRectanglesEXTFn;
};

class GL_EXPORT GLApi {
 public:
  GLApi();
  virtual ~GLApi();

  virtual void SetDisabledExtensions(const std::string& disabled_extensions) {}

  virtual void glActiveTextureFn(GLenum texture) = 0;
  virtual void glApplyFramebufferAttachmentCMAAINTELFn(void) = 0;
  virtual void glAttachShaderFn(GLuint program, GLuint shader) = 0;
  virtual void glBeginQueryFn(GLenum target, GLuint id) = 0;
  virtual void glBeginTransformFeedbackFn(GLenum primitiveMode) = 0;
  virtual void glBindAttribLocationFn(GLuint program,
                                      GLuint index,
                                      const char* name) = 0;
  virtual void glBindBufferFn(GLenum target, GLuint buffer) = 0;
  virtual void glBindBufferBaseFn(GLenum target,
                                  GLuint index,
                                  GLuint buffer) = 0;
  virtual void glBindBufferRangeFn(GLenum target,
                                   GLuint index,
                                   GLuint buffer,
                                   GLintptr offset,
                                   GLsizeiptr size) = 0;
  virtual void glBindFragDataLocationFn(GLuint program,
                                        GLuint colorNumber,
                                        const char* name) = 0;
  virtual void glBindFragDataLocationIndexedFn(GLuint program,
                                               GLuint colorNumber,
                                               GLuint index,
                                               const char* name) = 0;
  virtual void glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) = 0;
  virtual void glBindImageTextureEXTFn(GLuint index,
                                       GLuint texture,
                                       GLint level,
                                       GLboolean layered,
                                       GLint layer,
                                       GLenum access,
                                       GLint format) = 0;
  virtual void glBindRenderbufferEXTFn(GLenum target, GLuint renderbuffer) = 0;
  virtual void glBindSamplerFn(GLuint unit, GLuint sampler) = 0;
  virtual void glBindTextureFn(GLenum target, GLuint texture) = 0;
  virtual void glBindTransformFeedbackFn(GLenum target, GLuint id) = 0;
  virtual void glBindUniformLocationCHROMIUMFn(GLuint program,
                                               GLint location,
                                               const char* name) = 0;
  virtual void glBindVertexArrayOESFn(GLuint array) = 0;
  virtual void glBlendBarrierKHRFn(void) = 0;
  virtual void glBlendColorFn(GLclampf red,
                              GLclampf green,
                              GLclampf blue,
                              GLclampf alpha) = 0;
  virtual void glBlendEquationFn(GLenum mode) = 0;
  virtual void glBlendEquationSeparateFn(GLenum modeRGB, GLenum modeAlpha) = 0;
  virtual void glBlendFuncFn(GLenum sfactor, GLenum dfactor) = 0;
  virtual void glBlendFuncSeparateFn(GLenum srcRGB,
                                     GLenum dstRGB,
                                     GLenum srcAlpha,
                                     GLenum dstAlpha) = 0;
  virtual void glBlitFramebufferFn(GLint srcX0,
                                   GLint srcY0,
                                   GLint srcX1,
                                   GLint srcY1,
                                   GLint dstX0,
                                   GLint dstY0,
                                   GLint dstX1,
                                   GLint dstY1,
                                   GLbitfield mask,
                                   GLenum filter) = 0;
  virtual void glBufferDataFn(GLenum target,
                              GLsizeiptr size,
                              const void* data,
                              GLenum usage) = 0;
  virtual void glBufferSubDataFn(GLenum target,
                                 GLintptr offset,
                                 GLsizeiptr size,
                                 const void* data) = 0;
  virtual GLenum glCheckFramebufferStatusEXTFn(GLenum target) = 0;
  virtual void glClearFn(GLbitfield mask) = 0;
  virtual void glClearBufferfiFn(GLenum buffer,
                                 GLint drawbuffer,
                                 const GLfloat depth,
                                 GLint stencil) = 0;
  virtual void glClearBufferfvFn(GLenum buffer,
                                 GLint drawbuffer,
                                 const GLfloat* value) = 0;
  virtual void glClearBufferivFn(GLenum buffer,
                                 GLint drawbuffer,
                                 const GLint* value) = 0;
  virtual void glClearBufferuivFn(GLenum buffer,
                                  GLint drawbuffer,
                                  const GLuint* value) = 0;
  virtual void glClearColorFn(GLclampf red,
                              GLclampf green,
                              GLclampf blue,
                              GLclampf alpha) = 0;
  virtual void glClearDepthFn(GLclampd depth) = 0;
  virtual void glClearDepthfFn(GLclampf depth) = 0;
  virtual void glClearStencilFn(GLint s) = 0;
  virtual GLenum glClientWaitSyncFn(GLsync sync,
                                    GLbitfield flags,
                                    GLuint64 timeout) = 0;
  virtual void glColorMaskFn(GLboolean red,
                             GLboolean green,
                             GLboolean blue,
                             GLboolean alpha) = 0;
  virtual void glCompileShaderFn(GLuint shader) = 0;
  virtual void glCompressedCopyTextureCHROMIUMFn(GLuint sourceId,
                                                 GLuint destId) = 0;
  virtual void glCompressedTexImage2DFn(GLenum target,
                                        GLint level,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height,
                                        GLint border,
                                        GLsizei imageSize,
                                        const void* data) = 0;
  virtual void glCompressedTexImage2DRobustANGLEFn(GLenum target,
                                                   GLint level,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLint border,
                                                   GLsizei imageSize,
                                                   GLsizei dataSize,
                                                   const void* data) = 0;
  virtual void glCompressedTexImage3DFn(GLenum target,
                                        GLint level,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height,
                                        GLsizei depth,
                                        GLint border,
                                        GLsizei imageSize,
                                        const void* data) = 0;
  virtual void glCompressedTexImage3DRobustANGLEFn(GLenum target,
                                                   GLint level,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLsizei depth,
                                                   GLint border,
                                                   GLsizei imageSize,
                                                   GLsizei dataSize,
                                                   const void* data) = 0;
  virtual void glCompressedTexSubImage2DFn(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLsizei imageSize,
                                           const void* data) = 0;
  virtual void glCompressedTexSubImage2DRobustANGLEFn(GLenum target,
                                                      GLint level,
                                                      GLint xoffset,
                                                      GLint yoffset,
                                                      GLsizei width,
                                                      GLsizei height,
                                                      GLenum format,
                                                      GLsizei imageSize,
                                                      GLsizei dataSize,
                                                      const void* data) = 0;
  virtual void glCompressedTexSubImage3DFn(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint zoffset,
                                           GLsizei width,
                                           GLsizei height,
                                           GLsizei depth,
                                           GLenum format,
                                           GLsizei imageSize,
                                           const void* data) = 0;
  virtual void glCompressedTexSubImage3DRobustANGLEFn(GLenum target,
                                                      GLint level,
                                                      GLint xoffset,
                                                      GLint yoffset,
                                                      GLint zoffset,
                                                      GLsizei width,
                                                      GLsizei height,
                                                      GLsizei depth,
                                                      GLenum format,
                                                      GLsizei imageSize,
                                                      GLsizei dataSize,
                                                      const void* data) = 0;
  virtual void glCopyBufferSubDataFn(GLenum readTarget,
                                     GLenum writeTarget,
                                     GLintptr readOffset,
                                     GLintptr writeOffset,
                                     GLsizeiptr size) = 0;
  virtual void glCopySubTextureCHROMIUMFn(GLuint sourceId,
                                          GLint sourceLevel,
                                          GLenum destTarget,
                                          GLuint destId,
                                          GLint destLevel,
                                          GLint xoffset,
                                          GLint yoffset,
                                          GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height,
                                          GLboolean unpackFlipY,
                                          GLboolean unpackPremultiplyAlpha,
                                          GLboolean unpackUnmultiplyAlpha) = 0;
  virtual void glCopyTexImage2DFn(GLenum target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height,
                                  GLint border) = 0;
  virtual void glCopyTexSubImage2DFn(GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height) = 0;
  virtual void glCopyTexSubImage3DFn(GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height) = 0;
  virtual void glCopyTextureCHROMIUMFn(GLuint sourceId,
                                       GLint sourceLevel,
                                       GLenum destTarget,
                                       GLuint destId,
                                       GLint destLevel,
                                       GLint internalFormat,
                                       GLenum destType,
                                       GLboolean unpackFlipY,
                                       GLboolean unpackPremultiplyAlpha,
                                       GLboolean unpackUnmultiplyAlpha) = 0;
  virtual void glCoverageModulationNVFn(GLenum components) = 0;
  virtual void glCoverFillPathInstancedNVFn(GLsizei numPaths,
                                            GLenum pathNameType,
                                            const void* paths,
                                            GLuint pathBase,
                                            GLenum coverMode,
                                            GLenum transformType,
                                            const GLfloat* transformValues) = 0;
  virtual void glCoverFillPathNVFn(GLuint path, GLenum coverMode) = 0;
  virtual void glCoverStrokePathInstancedNVFn(
      GLsizei numPaths,
      GLenum pathNameType,
      const void* paths,
      GLuint pathBase,
      GLenum coverMode,
      GLenum transformType,
      const GLfloat* transformValues) = 0;
  virtual void glCoverStrokePathNVFn(GLuint name, GLenum coverMode) = 0;
  virtual GLuint glCreateProgramFn(void) = 0;
  virtual GLuint glCreateShaderFn(GLenum type) = 0;
  virtual void glCullFaceFn(GLenum mode) = 0;
  virtual void glDebugMessageCallbackFn(GLDEBUGPROC callback,
                                        const void* userParam) = 0;
  virtual void glDebugMessageControlFn(GLenum source,
                                       GLenum type,
                                       GLenum severity,
                                       GLsizei count,
                                       const GLuint* ids,
                                       GLboolean enabled) = 0;
  virtual void glDebugMessageInsertFn(GLenum source,
                                      GLenum type,
                                      GLuint id,
                                      GLenum severity,
                                      GLsizei length,
                                      const char* buf) = 0;
  virtual void glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) = 0;
  virtual void glDeleteFencesAPPLEFn(GLsizei n, const GLuint* fences) = 0;
  virtual void glDeleteFencesNVFn(GLsizei n, const GLuint* fences) = 0;
  virtual void glDeleteFramebuffersEXTFn(GLsizei n,
                                         const GLuint* framebuffers) = 0;
  virtual void glDeletePathsNVFn(GLuint path, GLsizei range) = 0;
  virtual void glDeleteProgramFn(GLuint program) = 0;
  virtual void glDeleteQueriesFn(GLsizei n, const GLuint* ids) = 0;
  virtual void glDeleteRenderbuffersEXTFn(GLsizei n,
                                          const GLuint* renderbuffers) = 0;
  virtual void glDeleteSamplersFn(GLsizei n, const GLuint* samplers) = 0;
  virtual void glDeleteShaderFn(GLuint shader) = 0;
  virtual void glDeleteSyncFn(GLsync sync) = 0;
  virtual void glDeleteTexturesFn(GLsizei n, const GLuint* textures) = 0;
  virtual void glDeleteTransformFeedbacksFn(GLsizei n, const GLuint* ids) = 0;
  virtual void glDeleteVertexArraysOESFn(GLsizei n, const GLuint* arrays) = 0;
  virtual void glDepthFuncFn(GLenum func) = 0;
  virtual void glDepthMaskFn(GLboolean flag) = 0;
  virtual void glDepthRangeFn(GLclampd zNear, GLclampd zFar) = 0;
  virtual void glDepthRangefFn(GLclampf zNear, GLclampf zFar) = 0;
  virtual void glDetachShaderFn(GLuint program, GLuint shader) = 0;
  virtual void glDisableFn(GLenum cap) = 0;
  virtual void glDisableVertexAttribArrayFn(GLuint index) = 0;
  virtual void glDiscardFramebufferEXTFn(GLenum target,
                                         GLsizei numAttachments,
                                         const GLenum* attachments) = 0;
  virtual void glDrawArraysFn(GLenum mode, GLint first, GLsizei count) = 0;
  virtual void glDrawArraysInstancedANGLEFn(GLenum mode,
                                            GLint first,
                                            GLsizei count,
                                            GLsizei primcount) = 0;
  virtual void glDrawBufferFn(GLenum mode) = 0;
  virtual void glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) = 0;
  virtual void glDrawElementsFn(GLenum mode,
                                GLsizei count,
                                GLenum type,
                                const void* indices) = 0;
  virtual void glDrawElementsInstancedANGLEFn(GLenum mode,
                                              GLsizei count,
                                              GLenum type,
                                              const void* indices,
                                              GLsizei primcount) = 0;
  virtual void glDrawRangeElementsFn(GLenum mode,
                                     GLuint start,
                                     GLuint end,
                                     GLsizei count,
                                     GLenum type,
                                     const void* indices) = 0;
  virtual void glEGLImageTargetRenderbufferStorageOESFn(
      GLenum target,
      GLeglImageOES image) = 0;
  virtual void glEGLImageTargetTexture2DOESFn(GLenum target,
                                              GLeglImageOES image) = 0;
  virtual void glEnableFn(GLenum cap) = 0;
  virtual void glEnableVertexAttribArrayFn(GLuint index) = 0;
  virtual void glEndQueryFn(GLenum target) = 0;
  virtual void glEndTransformFeedbackFn(void) = 0;
  virtual GLsync glFenceSyncFn(GLenum condition, GLbitfield flags) = 0;
  virtual void glFinishFn(void) = 0;
  virtual void glFinishFenceAPPLEFn(GLuint fence) = 0;
  virtual void glFinishFenceNVFn(GLuint fence) = 0;
  virtual void glFlushFn(void) = 0;
  virtual void glFlushMappedBufferRangeFn(GLenum target,
                                          GLintptr offset,
                                          GLsizeiptr length) = 0;
  virtual void glFramebufferRenderbufferEXTFn(GLenum target,
                                              GLenum attachment,
                                              GLenum renderbuffertarget,
                                              GLuint renderbuffer) = 0;
  virtual void glFramebufferTexture2DEXTFn(GLenum target,
                                           GLenum attachment,
                                           GLenum textarget,
                                           GLuint texture,
                                           GLint level) = 0;
  virtual void glFramebufferTexture2DMultisampleEXTFn(GLenum target,
                                                      GLenum attachment,
                                                      GLenum textarget,
                                                      GLuint texture,
                                                      GLint level,
                                                      GLsizei samples) = 0;
  virtual void glFramebufferTextureLayerFn(GLenum target,
                                           GLenum attachment,
                                           GLuint texture,
                                           GLint level,
                                           GLint layer) = 0;
  virtual void glFrontFaceFn(GLenum mode) = 0;
  virtual void glGenBuffersARBFn(GLsizei n, GLuint* buffers) = 0;
  virtual void glGenerateMipmapEXTFn(GLenum target) = 0;
  virtual void glGenFencesAPPLEFn(GLsizei n, GLuint* fences) = 0;
  virtual void glGenFencesNVFn(GLsizei n, GLuint* fences) = 0;
  virtual void glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) = 0;
  virtual GLuint glGenPathsNVFn(GLsizei range) = 0;
  virtual void glGenQueriesFn(GLsizei n, GLuint* ids) = 0;
  virtual void glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) = 0;
  virtual void glGenSamplersFn(GLsizei n, GLuint* samplers) = 0;
  virtual void glGenTexturesFn(GLsizei n, GLuint* textures) = 0;
  virtual void glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) = 0;
  virtual void glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) = 0;
  virtual void glGetActiveAttribFn(GLuint program,
                                   GLuint index,
                                   GLsizei bufsize,
                                   GLsizei* length,
                                   GLint* size,
                                   GLenum* type,
                                   char* name) = 0;
  virtual void glGetActiveUniformFn(GLuint program,
                                    GLuint index,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    GLint* size,
                                    GLenum* type,
                                    char* name) = 0;
  virtual void glGetActiveUniformBlockivFn(GLuint program,
                                           GLuint uniformBlockIndex,
                                           GLenum pname,
                                           GLint* params) = 0;
  virtual void glGetActiveUniformBlockivRobustANGLEFn(GLuint program,
                                                      GLuint uniformBlockIndex,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) = 0;
  virtual void glGetActiveUniformBlockNameFn(GLuint program,
                                             GLuint uniformBlockIndex,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             char* uniformBlockName) = 0;
  virtual void glGetActiveUniformsivFn(GLuint program,
                                       GLsizei uniformCount,
                                       const GLuint* uniformIndices,
                                       GLenum pname,
                                       GLint* params) = 0;
  virtual void glGetAttachedShadersFn(GLuint program,
                                      GLsizei maxcount,
                                      GLsizei* count,
                                      GLuint* shaders) = 0;
  virtual GLint glGetAttribLocationFn(GLuint program, const char* name) = 0;
  virtual void glGetBooleani_vRobustANGLEFn(GLenum target,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLboolean* data) = 0;
  virtual void glGetBooleanvFn(GLenum pname, GLboolean* params) = 0;
  virtual void glGetBooleanvRobustANGLEFn(GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLboolean* data) = 0;
  virtual void glGetBufferParameteri64vRobustANGLEFn(GLenum target,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint64* params) = 0;
  virtual void glGetBufferParameterivFn(GLenum target,
                                        GLenum pname,
                                        GLint* params) = 0;
  virtual void glGetBufferParameterivRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) = 0;
  virtual void glGetBufferPointervRobustANGLEFn(GLenum target,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                void** params) = 0;
  virtual void glGetDebugMessageLogFn(GLuint count,
                                      GLsizei bufSize,
                                      GLenum* sources,
                                      GLenum* types,
                                      GLuint* ids,
                                      GLenum* severities,
                                      GLsizei* lengths,
                                      char* messageLog) = 0;
  virtual GLenum glGetErrorFn(void) = 0;
  virtual void glGetFenceivNVFn(GLuint fence, GLenum pname, GLint* params) = 0;
  virtual void glGetFloatvFn(GLenum pname, GLfloat* params) = 0;
  virtual void glGetFloatvRobustANGLEFn(GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        GLfloat* data) = 0;
  virtual GLint glGetFragDataIndexFn(GLuint program, const char* name) = 0;
  virtual GLint glGetFragDataLocationFn(GLuint program, const char* name) = 0;
  virtual void glGetFramebufferAttachmentParameterivEXTFn(GLenum target,
                                                          GLenum attachment,
                                                          GLenum pname,
                                                          GLint* params) = 0;
  virtual void glGetFramebufferAttachmentParameterivRobustANGLEFn(
      GLenum target,
      GLenum attachment,
      GLenum pname,
      GLsizei bufSize,
      GLsizei* length,
      GLint* params) = 0;
  virtual void glGetFramebufferParameterivRobustANGLEFn(GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLint* params) = 0;
  virtual GLenum glGetGraphicsResetStatusARBFn(void) = 0;
  virtual void glGetInteger64i_vFn(GLenum target,
                                   GLuint index,
                                   GLint64* data) = 0;
  virtual void glGetInteger64i_vRobustANGLEFn(GLenum target,
                                              GLuint index,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint64* data) = 0;
  virtual void glGetInteger64vFn(GLenum pname, GLint64* params) = 0;
  virtual void glGetInteger64vRobustANGLEFn(GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint64* data) = 0;
  virtual void glGetIntegeri_vFn(GLenum target, GLuint index, GLint* data) = 0;
  virtual void glGetIntegeri_vRobustANGLEFn(GLenum target,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* data) = 0;
  virtual void glGetIntegervFn(GLenum pname, GLint* params) = 0;
  virtual void glGetIntegervRobustANGLEFn(GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* data) = 0;
  virtual void glGetInternalformativFn(GLenum target,
                                       GLenum internalformat,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLint* params) = 0;
  virtual void glGetInternalformativRobustANGLEFn(GLenum target,
                                                  GLenum internalformat,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) = 0;
  virtual void glGetMultisamplefvFn(GLenum pname,
                                    GLuint index,
                                    GLfloat* val) = 0;
  virtual void glGetMultisamplefvRobustANGLEFn(GLenum pname,
                                               GLuint index,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLfloat* val) = 0;
  virtual void glGetnUniformfvRobustANGLEFn(GLuint program,
                                            GLint location,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLfloat* params) = 0;
  virtual void glGetnUniformivRobustANGLEFn(GLuint program,
                                            GLint location,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* params) = 0;
  virtual void glGetnUniformuivRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLuint* params) = 0;
  virtual void glGetObjectLabelFn(GLenum identifier,
                                  GLuint name,
                                  GLsizei bufSize,
                                  GLsizei* length,
                                  char* label) = 0;
  virtual void glGetObjectPtrLabelFn(void* ptr,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     char* label) = 0;
  virtual void glGetPointervFn(GLenum pname, void** params) = 0;
  virtual void glGetPointervRobustANGLERobustANGLEFn(GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     void** params) = 0;
  virtual void glGetProgramBinaryFn(GLuint program,
                                    GLsizei bufSize,
                                    GLsizei* length,
                                    GLenum* binaryFormat,
                                    GLvoid* binary) = 0;
  virtual void glGetProgramInfoLogFn(GLuint program,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     char* infolog) = 0;
  virtual void glGetProgramInterfaceivFn(GLuint program,
                                         GLenum programInterface,
                                         GLenum pname,
                                         GLint* params) = 0;
  virtual void glGetProgramInterfaceivRobustANGLEFn(GLuint program,
                                                    GLenum programInterface,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint* params) = 0;
  virtual void glGetProgramivFn(GLuint program,
                                GLenum pname,
                                GLint* params) = 0;
  virtual void glGetProgramivRobustANGLEFn(GLuint program,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) = 0;
  virtual void glGetProgramResourceivFn(GLuint program,
                                        GLenum programInterface,
                                        GLuint index,
                                        GLsizei propCount,
                                        const GLenum* props,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        GLint* params) = 0;
  virtual GLint glGetProgramResourceLocationFn(GLuint program,
                                               GLenum programInterface,
                                               const char* name) = 0;
  virtual void glGetProgramResourceNameFn(GLuint program,
                                          GLenum programInterface,
                                          GLuint index,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLchar* name) = 0;
  virtual void glGetQueryivFn(GLenum target, GLenum pname, GLint* params) = 0;
  virtual void glGetQueryivRobustANGLEFn(GLenum target,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLint* params) = 0;
  virtual void glGetQueryObjecti64vFn(GLuint id,
                                      GLenum pname,
                                      GLint64* params) = 0;
  virtual void glGetQueryObjecti64vRobustANGLEFn(GLuint id,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint64* params) = 0;
  virtual void glGetQueryObjectivFn(GLuint id, GLenum pname, GLint* params) = 0;
  virtual void glGetQueryObjectivRobustANGLEFn(GLuint id,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLint* params) = 0;
  virtual void glGetQueryObjectui64vFn(GLuint id,
                                       GLenum pname,
                                       GLuint64* params) = 0;
  virtual void glGetQueryObjectui64vRobustANGLEFn(GLuint id,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLuint64* params) = 0;
  virtual void glGetQueryObjectuivFn(GLuint id,
                                     GLenum pname,
                                     GLuint* params) = 0;
  virtual void glGetQueryObjectuivRobustANGLEFn(GLuint id,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLuint* params) = 0;
  virtual void glGetRenderbufferParameterivEXTFn(GLenum target,
                                                 GLenum pname,
                                                 GLint* params) = 0;
  virtual void glGetRenderbufferParameterivRobustANGLEFn(GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei* length,
                                                         GLint* params) = 0;
  virtual void glGetSamplerParameterfvFn(GLuint sampler,
                                         GLenum pname,
                                         GLfloat* params) = 0;
  virtual void glGetSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLfloat* params) = 0;
  virtual void glGetSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) = 0;
  virtual void glGetSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLuint* params) = 0;
  virtual void glGetSamplerParameterivFn(GLuint sampler,
                                         GLenum pname,
                                         GLint* params) = 0;
  virtual void glGetSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint* params) = 0;
  virtual void glGetShaderInfoLogFn(GLuint shader,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    char* infolog) = 0;
  virtual void glGetShaderivFn(GLuint shader, GLenum pname, GLint* params) = 0;
  virtual void glGetShaderivRobustANGLEFn(GLuint shader,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params) = 0;
  virtual void glGetShaderPrecisionFormatFn(GLenum shadertype,
                                            GLenum precisiontype,
                                            GLint* range,
                                            GLint* precision) = 0;
  virtual void glGetShaderSourceFn(GLuint shader,
                                   GLsizei bufsize,
                                   GLsizei* length,
                                   char* source) = 0;
  virtual const GLubyte* glGetStringFn(GLenum name) = 0;
  virtual const GLubyte* glGetStringiFn(GLenum name, GLuint index) = 0;
  virtual void glGetSyncivFn(GLsync sync,
                             GLenum pname,
                             GLsizei bufSize,
                             GLsizei* length,
                             GLint* values) = 0;
  virtual void glGetTexLevelParameterfvFn(GLenum target,
                                          GLint level,
                                          GLenum pname,
                                          GLfloat* params) = 0;
  virtual void glGetTexLevelParameterfvRobustANGLEFn(GLenum target,
                                                     GLint level,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLfloat* params) = 0;
  virtual void glGetTexLevelParameterivFn(GLenum target,
                                          GLint level,
                                          GLenum pname,
                                          GLint* params) = 0;
  virtual void glGetTexLevelParameterivRobustANGLEFn(GLenum target,
                                                     GLint level,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) = 0;
  virtual void glGetTexParameterfvFn(GLenum target,
                                     GLenum pname,
                                     GLfloat* params) = 0;
  virtual void glGetTexParameterfvRobustANGLEFn(GLenum target,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLfloat* params) = 0;
  virtual void glGetTexParameterIivRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) = 0;
  virtual void glGetTexParameterIuivRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLuint* params) = 0;
  virtual void glGetTexParameterivFn(GLenum target,
                                     GLenum pname,
                                     GLint* params) = 0;
  virtual void glGetTexParameterivRobustANGLEFn(GLenum target,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* params) = 0;
  virtual void glGetTransformFeedbackVaryingFn(GLuint program,
                                               GLuint index,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLsizei* size,
                                               GLenum* type,
                                               char* name) = 0;
  virtual void glGetTranslatedShaderSourceANGLEFn(GLuint shader,
                                                  GLsizei bufsize,
                                                  GLsizei* length,
                                                  char* source) = 0;
  virtual GLuint glGetUniformBlockIndexFn(GLuint program,
                                          const char* uniformBlockName) = 0;
  virtual void glGetUniformfvFn(GLuint program,
                                GLint location,
                                GLfloat* params) = 0;
  virtual void glGetUniformfvRobustANGLEFn(GLuint program,
                                           GLint location,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLfloat* params) = 0;
  virtual void glGetUniformIndicesFn(GLuint program,
                                     GLsizei uniformCount,
                                     const char* const* uniformNames,
                                     GLuint* uniformIndices) = 0;
  virtual void glGetUniformivFn(GLuint program,
                                GLint location,
                                GLint* params) = 0;
  virtual void glGetUniformivRobustANGLEFn(GLuint program,
                                           GLint location,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) = 0;
  virtual GLint glGetUniformLocationFn(GLuint program, const char* name) = 0;
  virtual void glGetUniformuivFn(GLuint program,
                                 GLint location,
                                 GLuint* params) = 0;
  virtual void glGetUniformuivRobustANGLEFn(GLuint program,
                                            GLint location,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLuint* params) = 0;
  virtual void glGetVertexAttribfvFn(GLuint index,
                                     GLenum pname,
                                     GLfloat* params) = 0;
  virtual void glGetVertexAttribfvRobustANGLEFn(GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLfloat* params) = 0;
  virtual void glGetVertexAttribIivRobustANGLEFn(GLuint index,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) = 0;
  virtual void glGetVertexAttribIuivRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLuint* params) = 0;
  virtual void glGetVertexAttribivFn(GLuint index,
                                     GLenum pname,
                                     GLint* params) = 0;
  virtual void glGetVertexAttribivRobustANGLEFn(GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* params) = 0;
  virtual void glGetVertexAttribPointervFn(GLuint index,
                                           GLenum pname,
                                           void** pointer) = 0;
  virtual void glGetVertexAttribPointervRobustANGLEFn(GLuint index,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      void** pointer) = 0;
  virtual void glHintFn(GLenum target, GLenum mode) = 0;
  virtual void glInsertEventMarkerEXTFn(GLsizei length, const char* marker) = 0;
  virtual void glInvalidateFramebufferFn(GLenum target,
                                         GLsizei numAttachments,
                                         const GLenum* attachments) = 0;
  virtual void glInvalidateSubFramebufferFn(GLenum target,
                                            GLsizei numAttachments,
                                            const GLenum* attachments,
                                            GLint x,
                                            GLint y,
                                            GLint width,
                                            GLint height) = 0;
  virtual GLboolean glIsBufferFn(GLuint buffer) = 0;
  virtual GLboolean glIsEnabledFn(GLenum cap) = 0;
  virtual GLboolean glIsFenceAPPLEFn(GLuint fence) = 0;
  virtual GLboolean glIsFenceNVFn(GLuint fence) = 0;
  virtual GLboolean glIsFramebufferEXTFn(GLuint framebuffer) = 0;
  virtual GLboolean glIsPathNVFn(GLuint path) = 0;
  virtual GLboolean glIsProgramFn(GLuint program) = 0;
  virtual GLboolean glIsQueryFn(GLuint query) = 0;
  virtual GLboolean glIsRenderbufferEXTFn(GLuint renderbuffer) = 0;
  virtual GLboolean glIsSamplerFn(GLuint sampler) = 0;
  virtual GLboolean glIsShaderFn(GLuint shader) = 0;
  virtual GLboolean glIsSyncFn(GLsync sync) = 0;
  virtual GLboolean glIsTextureFn(GLuint texture) = 0;
  virtual GLboolean glIsTransformFeedbackFn(GLuint id) = 0;
  virtual GLboolean glIsVertexArrayOESFn(GLuint array) = 0;
  virtual void glLineWidthFn(GLfloat width) = 0;
  virtual void glLinkProgramFn(GLuint program) = 0;
  virtual void* glMapBufferFn(GLenum target, GLenum access) = 0;
  virtual void* glMapBufferRangeFn(GLenum target,
                                   GLintptr offset,
                                   GLsizeiptr length,
                                   GLbitfield access) = 0;
  virtual void glMatrixLoadfEXTFn(GLenum matrixMode, const GLfloat* m) = 0;
  virtual void glMatrixLoadIdentityEXTFn(GLenum matrixMode) = 0;
  virtual void glMemoryBarrierEXTFn(GLbitfield barriers) = 0;
  virtual void glObjectLabelFn(GLenum identifier,
                               GLuint name,
                               GLsizei length,
                               const char* label) = 0;
  virtual void glObjectPtrLabelFn(void* ptr,
                                  GLsizei length,
                                  const char* label) = 0;
  virtual void glPathCommandsNVFn(GLuint path,
                                  GLsizei numCommands,
                                  const GLubyte* commands,
                                  GLsizei numCoords,
                                  GLenum coordType,
                                  const GLvoid* coords) = 0;
  virtual void glPathParameterfNVFn(GLuint path,
                                    GLenum pname,
                                    GLfloat value) = 0;
  virtual void glPathParameteriNVFn(GLuint path, GLenum pname, GLint value) = 0;
  virtual void glPathStencilFuncNVFn(GLenum func, GLint ref, GLuint mask) = 0;
  virtual void glPauseTransformFeedbackFn(void) = 0;
  virtual void glPixelStoreiFn(GLenum pname, GLint param) = 0;
  virtual void glPointParameteriFn(GLenum pname, GLint param) = 0;
  virtual void glPolygonModeFn(GLenum face, GLenum mode) = 0;
  virtual void glPolygonOffsetFn(GLfloat factor, GLfloat units) = 0;
  virtual void glPopDebugGroupFn() = 0;
  virtual void glPopGroupMarkerEXTFn(void) = 0;
  virtual void glPrimitiveRestartIndexFn(GLuint index) = 0;
  virtual void glProgramBinaryFn(GLuint program,
                                 GLenum binaryFormat,
                                 const GLvoid* binary,
                                 GLsizei length) = 0;
  virtual void glProgramParameteriFn(GLuint program,
                                     GLenum pname,
                                     GLint value) = 0;
  virtual void glProgramPathFragmentInputGenNVFn(GLuint program,
                                                 GLint location,
                                                 GLenum genMode,
                                                 GLint components,
                                                 const GLfloat* coeffs) = 0;
  virtual void glPushDebugGroupFn(GLenum source,
                                  GLuint id,
                                  GLsizei length,
                                  const char* message) = 0;
  virtual void glPushGroupMarkerEXTFn(GLsizei length, const char* marker) = 0;
  virtual void glQueryCounterFn(GLuint id, GLenum target) = 0;
  virtual void glReadBufferFn(GLenum src) = 0;
  virtual void glReadnPixelsRobustANGLEFn(GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height,
                                          GLenum format,
                                          GLenum type,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLsizei* columns,
                                          GLsizei* rows,
                                          void* data) = 0;
  virtual void glReadPixelsFn(GLint x,
                              GLint y,
                              GLsizei width,
                              GLsizei height,
                              GLenum format,
                              GLenum type,
                              void* pixels) = 0;
  virtual void glReadPixelsRobustANGLEFn(GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLsizei* columns,
                                         GLsizei* rows,
                                         void* pixels) = 0;
  virtual void glReleaseShaderCompilerFn(void) = 0;
  virtual void glRenderbufferStorageEXTFn(GLenum target,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height) = 0;
  virtual void glRenderbufferStorageMultisampleFn(GLenum target,
                                                  GLsizei samples,
                                                  GLenum internalformat,
                                                  GLsizei width,
                                                  GLsizei height) = 0;
  virtual void glRenderbufferStorageMultisampleEXTFn(GLenum target,
                                                     GLsizei samples,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height) = 0;
  virtual void glRequestExtensionANGLEFn(const char* name) = 0;
  virtual void glResumeTransformFeedbackFn(void) = 0;
  virtual void glSampleCoverageFn(GLclampf value, GLboolean invert) = 0;
  virtual void glSamplerParameterfFn(GLuint sampler,
                                     GLenum pname,
                                     GLfloat param) = 0;
  virtual void glSamplerParameterfvFn(GLuint sampler,
                                      GLenum pname,
                                      const GLfloat* params) = 0;
  virtual void glSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 const GLfloat* param) = 0;
  virtual void glSamplerParameteriFn(GLuint sampler,
                                     GLenum pname,
                                     GLint param) = 0;
  virtual void glSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  const GLint* param) = 0;
  virtual void glSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLuint* param) = 0;
  virtual void glSamplerParameterivFn(GLuint sampler,
                                      GLenum pname,
                                      const GLint* params) = 0;
  virtual void glSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 const GLint* param) = 0;
  virtual void glScissorFn(GLint x, GLint y, GLsizei width, GLsizei height) = 0;
  virtual void glSetFenceAPPLEFn(GLuint fence) = 0;
  virtual void glSetFenceNVFn(GLuint fence, GLenum condition) = 0;
  virtual void glShaderBinaryFn(GLsizei n,
                                const GLuint* shaders,
                                GLenum binaryformat,
                                const void* binary,
                                GLsizei length) = 0;
  virtual void glShaderSourceFn(GLuint shader,
                                GLsizei count,
                                const char* const* str,
                                const GLint* length) = 0;
  virtual void glStencilFillPathInstancedNVFn(
      GLsizei numPaths,
      GLenum pathNameType,
      const void* paths,
      GLuint pathBase,
      GLenum fillMode,
      GLuint mask,
      GLenum transformType,
      const GLfloat* transformValues) = 0;
  virtual void glStencilFillPathNVFn(GLuint path,
                                     GLenum fillMode,
                                     GLuint mask) = 0;
  virtual void glStencilFuncFn(GLenum func, GLint ref, GLuint mask) = 0;
  virtual void glStencilFuncSeparateFn(GLenum face,
                                       GLenum func,
                                       GLint ref,
                                       GLuint mask) = 0;
  virtual void glStencilMaskFn(GLuint mask) = 0;
  virtual void glStencilMaskSeparateFn(GLenum face, GLuint mask) = 0;
  virtual void glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) = 0;
  virtual void glStencilOpSeparateFn(GLenum face,
                                     GLenum fail,
                                     GLenum zfail,
                                     GLenum zpass) = 0;
  virtual void glStencilStrokePathInstancedNVFn(
      GLsizei numPaths,
      GLenum pathNameType,
      const void* paths,
      GLuint pathBase,
      GLint ref,
      GLuint mask,
      GLenum transformType,
      const GLfloat* transformValues) = 0;
  virtual void glStencilStrokePathNVFn(GLuint path,
                                       GLint reference,
                                       GLuint mask) = 0;
  virtual void glStencilThenCoverFillPathInstancedNVFn(
      GLsizei numPaths,
      GLenum pathNameType,
      const void* paths,
      GLuint pathBase,
      GLenum fillMode,
      GLuint mask,
      GLenum coverMode,
      GLenum transformType,
      const GLfloat* transformValues) = 0;
  virtual void glStencilThenCoverFillPathNVFn(GLuint path,
                                              GLenum fillMode,
                                              GLuint mask,
                                              GLenum coverMode) = 0;
  virtual void glStencilThenCoverStrokePathInstancedNVFn(
      GLsizei numPaths,
      GLenum pathNameType,
      const void* paths,
      GLuint pathBase,
      GLint ref,
      GLuint mask,
      GLenum coverMode,
      GLenum transformType,
      const GLfloat* transformValues) = 0;
  virtual void glStencilThenCoverStrokePathNVFn(GLuint path,
                                                GLint reference,
                                                GLuint mask,
                                                GLenum coverMode) = 0;
  virtual GLboolean glTestFenceAPPLEFn(GLuint fence) = 0;
  virtual GLboolean glTestFenceNVFn(GLuint fence) = 0;
  virtual void glTexBufferFn(GLenum target,
                             GLenum internalformat,
                             GLuint buffer) = 0;
  virtual void glTexBufferRangeFn(GLenum target,
                                  GLenum internalformat,
                                  GLuint buffer,
                                  GLintptr offset,
                                  GLsizeiptr size) = 0;
  virtual void glTexImage2DFn(GLenum target,
                              GLint level,
                              GLint internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLint border,
                              GLenum format,
                              GLenum type,
                              const void* pixels) = 0;
  virtual void glTexImage2DRobustANGLEFn(GLenum target,
                                         GLint level,
                                         GLint internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei bufSize,
                                         const void* pixels) = 0;
  virtual void glTexImage3DFn(GLenum target,
                              GLint level,
                              GLint internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              GLint border,
                              GLenum format,
                              GLenum type,
                              const void* pixels) = 0;
  virtual void glTexImage3DRobustANGLEFn(GLenum target,
                                         GLint level,
                                         GLint internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei bufSize,
                                         const void* pixels) = 0;
  virtual void glTexParameterfFn(GLenum target,
                                 GLenum pname,
                                 GLfloat param) = 0;
  virtual void glTexParameterfvFn(GLenum target,
                                  GLenum pname,
                                  const GLfloat* params) = 0;
  virtual void glTexParameterfvRobustANGLEFn(GLenum target,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             const GLfloat* params) = 0;
  virtual void glTexParameteriFn(GLenum target, GLenum pname, GLint param) = 0;
  virtual void glTexParameterIivRobustANGLEFn(GLenum target,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              const GLint* params) = 0;
  virtual void glTexParameterIuivRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLuint* params) = 0;
  virtual void glTexParameterivFn(GLenum target,
                                  GLenum pname,
                                  const GLint* params) = 0;
  virtual void glTexParameterivRobustANGLEFn(GLenum target,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             const GLint* params) = 0;
  virtual void glTexStorage2DEXTFn(GLenum target,
                                   GLsizei levels,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height) = 0;
  virtual void glTexStorage3DFn(GLenum target,
                                GLsizei levels,
                                GLenum internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth) = 0;
  virtual void glTexSubImage2DFn(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLenum format,
                                 GLenum type,
                                 const void* pixels) = 0;
  virtual void glTexSubImage2DRobustANGLEFn(GLenum target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLenum type,
                                            GLsizei bufSize,
                                            const void* pixels) = 0;
  virtual void glTexSubImage3DFn(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLint zoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLenum type,
                                 const void* pixels) = 0;
  virtual void glTexSubImage3DRobustANGLEFn(GLenum target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLint zoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLsizei depth,
                                            GLenum format,
                                            GLenum type,
                                            GLsizei bufSize,
                                            const void* pixels) = 0;
  virtual void glTransformFeedbackVaryingsFn(GLuint program,
                                             GLsizei count,
                                             const char* const* varyings,
                                             GLenum bufferMode) = 0;
  virtual void glUniform1fFn(GLint location, GLfloat x) = 0;
  virtual void glUniform1fvFn(GLint location,
                              GLsizei count,
                              const GLfloat* v) = 0;
  virtual void glUniform1iFn(GLint location, GLint x) = 0;
  virtual void glUniform1ivFn(GLint location,
                              GLsizei count,
                              const GLint* v) = 0;
  virtual void glUniform1uiFn(GLint location, GLuint v0) = 0;
  virtual void glUniform1uivFn(GLint location,
                               GLsizei count,
                               const GLuint* v) = 0;
  virtual void glUniform2fFn(GLint location, GLfloat x, GLfloat y) = 0;
  virtual void glUniform2fvFn(GLint location,
                              GLsizei count,
                              const GLfloat* v) = 0;
  virtual void glUniform2iFn(GLint location, GLint x, GLint y) = 0;
  virtual void glUniform2ivFn(GLint location,
                              GLsizei count,
                              const GLint* v) = 0;
  virtual void glUniform2uiFn(GLint location, GLuint v0, GLuint v1) = 0;
  virtual void glUniform2uivFn(GLint location,
                               GLsizei count,
                               const GLuint* v) = 0;
  virtual void glUniform3fFn(GLint location,
                             GLfloat x,
                             GLfloat y,
                             GLfloat z) = 0;
  virtual void glUniform3fvFn(GLint location,
                              GLsizei count,
                              const GLfloat* v) = 0;
  virtual void glUniform3iFn(GLint location, GLint x, GLint y, GLint z) = 0;
  virtual void glUniform3ivFn(GLint location,
                              GLsizei count,
                              const GLint* v) = 0;
  virtual void glUniform3uiFn(GLint location,
                              GLuint v0,
                              GLuint v1,
                              GLuint v2) = 0;
  virtual void glUniform3uivFn(GLint location,
                               GLsizei count,
                               const GLuint* v) = 0;
  virtual void glUniform4fFn(GLint location,
                             GLfloat x,
                             GLfloat y,
                             GLfloat z,
                             GLfloat w) = 0;
  virtual void glUniform4fvFn(GLint location,
                              GLsizei count,
                              const GLfloat* v) = 0;
  virtual void glUniform4iFn(GLint location,
                             GLint x,
                             GLint y,
                             GLint z,
                             GLint w) = 0;
  virtual void glUniform4ivFn(GLint location,
                              GLsizei count,
                              const GLint* v) = 0;
  virtual void glUniform4uiFn(GLint location,
                              GLuint v0,
                              GLuint v1,
                              GLuint v2,
                              GLuint v3) = 0;
  virtual void glUniform4uivFn(GLint location,
                               GLsizei count,
                               const GLuint* v) = 0;
  virtual void glUniformBlockBindingFn(GLuint program,
                                       GLuint uniformBlockIndex,
                                       GLuint uniformBlockBinding) = 0;
  virtual void glUniformMatrix2fvFn(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat* value) = 0;
  virtual void glUniformMatrix2x3fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) = 0;
  virtual void glUniformMatrix2x4fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) = 0;
  virtual void glUniformMatrix3fvFn(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat* value) = 0;
  virtual void glUniformMatrix3x2fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) = 0;
  virtual void glUniformMatrix3x4fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) = 0;
  virtual void glUniformMatrix4fvFn(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat* value) = 0;
  virtual void glUniformMatrix4x2fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) = 0;
  virtual void glUniformMatrix4x3fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) = 0;
  virtual GLboolean glUnmapBufferFn(GLenum target) = 0;
  virtual void glUseProgramFn(GLuint program) = 0;
  virtual void glValidateProgramFn(GLuint program) = 0;
  virtual void glVertexAttrib1fFn(GLuint indx, GLfloat x) = 0;
  virtual void glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) = 0;
  virtual void glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) = 0;
  virtual void glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) = 0;
  virtual void glVertexAttrib3fFn(GLuint indx,
                                  GLfloat x,
                                  GLfloat y,
                                  GLfloat z) = 0;
  virtual void glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) = 0;
  virtual void glVertexAttrib4fFn(GLuint indx,
                                  GLfloat x,
                                  GLfloat y,
                                  GLfloat z,
                                  GLfloat w) = 0;
  virtual void glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) = 0;
  virtual void glVertexAttribDivisorANGLEFn(GLuint index, GLuint divisor) = 0;
  virtual void glVertexAttribI4iFn(GLuint indx,
                                   GLint x,
                                   GLint y,
                                   GLint z,
                                   GLint w) = 0;
  virtual void glVertexAttribI4ivFn(GLuint indx, const GLint* values) = 0;
  virtual void glVertexAttribI4uiFn(GLuint indx,
                                    GLuint x,
                                    GLuint y,
                                    GLuint z,
                                    GLuint w) = 0;
  virtual void glVertexAttribI4uivFn(GLuint indx, const GLuint* values) = 0;
  virtual void glVertexAttribIPointerFn(GLuint indx,
                                        GLint size,
                                        GLenum type,
                                        GLsizei stride,
                                        const void* ptr) = 0;
  virtual void glVertexAttribPointerFn(GLuint indx,
                                       GLint size,
                                       GLenum type,
                                       GLboolean normalized,
                                       GLsizei stride,
                                       const void* ptr) = 0;
  virtual void glViewportFn(GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height) = 0;
  virtual void glWaitSyncFn(GLsync sync,
                            GLbitfield flags,
                            GLuint64 timeout) = 0;
  virtual void glWindowRectanglesEXTFn(GLenum mode,
                                       GLsizei n,
                                       const GLint* box) = 0;
};

}  // namespace gl

#define glActiveTexture ::gl::g_current_gl_context->glActiveTextureFn
#define glApplyFramebufferAttachmentCMAAINTEL \
  ::gl::g_current_gl_context->glApplyFramebufferAttachmentCMAAINTELFn
#define glAttachShader ::gl::g_current_gl_context->glAttachShaderFn
#define glBeginQuery ::gl::g_current_gl_context->glBeginQueryFn
#define glBeginTransformFeedback \
  ::gl::g_current_gl_context->glBeginTransformFeedbackFn
#define glBindAttribLocation ::gl::g_current_gl_context->glBindAttribLocationFn
#define glBindBuffer ::gl::g_current_gl_context->glBindBufferFn
#define glBindBufferBase ::gl::g_current_gl_context->glBindBufferBaseFn
#define glBindBufferRange ::gl::g_current_gl_context->glBindBufferRangeFn
#define glBindFragDataLocation \
  ::gl::g_current_gl_context->glBindFragDataLocationFn
#define glBindFragDataLocationIndexed \
  ::gl::g_current_gl_context->glBindFragDataLocationIndexedFn
#define glBindFramebufferEXT ::gl::g_current_gl_context->glBindFramebufferEXTFn
#define glBindImageTextureEXT \
  ::gl::g_current_gl_context->glBindImageTextureEXTFn
#define glBindRenderbufferEXT \
  ::gl::g_current_gl_context->glBindRenderbufferEXTFn
#define glBindSampler ::gl::g_current_gl_context->glBindSamplerFn
#define glBindTexture ::gl::g_current_gl_context->glBindTextureFn
#define glBindTransformFeedback \
  ::gl::g_current_gl_context->glBindTransformFeedbackFn
#define glBindUniformLocationCHROMIUM \
  ::gl::g_current_gl_context->glBindUniformLocationCHROMIUMFn
#define glBindVertexArrayOES ::gl::g_current_gl_context->glBindVertexArrayOESFn
#define glBlendBarrierKHR ::gl::g_current_gl_context->glBlendBarrierKHRFn
#define glBlendColor ::gl::g_current_gl_context->glBlendColorFn
#define glBlendEquation ::gl::g_current_gl_context->glBlendEquationFn
#define glBlendEquationSeparate \
  ::gl::g_current_gl_context->glBlendEquationSeparateFn
#define glBlendFunc ::gl::g_current_gl_context->glBlendFuncFn
#define glBlendFuncSeparate ::gl::g_current_gl_context->glBlendFuncSeparateFn
#define glBlitFramebuffer ::gl::g_current_gl_context->glBlitFramebufferFn
#define glBufferData ::gl::g_current_gl_context->glBufferDataFn
#define glBufferSubData ::gl::g_current_gl_context->glBufferSubDataFn
#define glCheckFramebufferStatusEXT \
  ::gl::g_current_gl_context->glCheckFramebufferStatusEXTFn
#define glClear ::gl::g_current_gl_context->glClearFn
#define glClearBufferfi ::gl::g_current_gl_context->glClearBufferfiFn
#define glClearBufferfv ::gl::g_current_gl_context->glClearBufferfvFn
#define glClearBufferiv ::gl::g_current_gl_context->glClearBufferivFn
#define glClearBufferuiv ::gl::g_current_gl_context->glClearBufferuivFn
#define glClearColor ::gl::g_current_gl_context->glClearColorFn
#define glClearDepth ::gl::g_current_gl_context->glClearDepthFn
#define glClearDepthf ::gl::g_current_gl_context->glClearDepthfFn
#define glClearStencil ::gl::g_current_gl_context->glClearStencilFn
#define glClientWaitSync ::gl::g_current_gl_context->glClientWaitSyncFn
#define glColorMask ::gl::g_current_gl_context->glColorMaskFn
#define glCompileShader ::gl::g_current_gl_context->glCompileShaderFn
#define glCompressedCopyTextureCHROMIUM \
  ::gl::g_current_gl_context->glCompressedCopyTextureCHROMIUMFn
#define glCompressedTexImage2D \
  ::gl::g_current_gl_context->glCompressedTexImage2DFn
#define glCompressedTexImage2DRobustANGLE \
  ::gl::g_current_gl_context->glCompressedTexImage2DRobustANGLEFn
#define glCompressedTexImage3D \
  ::gl::g_current_gl_context->glCompressedTexImage3DFn
#define glCompressedTexImage3DRobustANGLE \
  ::gl::g_current_gl_context->glCompressedTexImage3DRobustANGLEFn
#define glCompressedTexSubImage2D \
  ::gl::g_current_gl_context->glCompressedTexSubImage2DFn
#define glCompressedTexSubImage2DRobustANGLE \
  ::gl::g_current_gl_context->glCompressedTexSubImage2DRobustANGLEFn
#define glCompressedTexSubImage3D \
  ::gl::g_current_gl_context->glCompressedTexSubImage3DFn
#define glCompressedTexSubImage3DRobustANGLE \
  ::gl::g_current_gl_context->glCompressedTexSubImage3DRobustANGLEFn
#define glCopyBufferSubData ::gl::g_current_gl_context->glCopyBufferSubDataFn
#define glCopySubTextureCHROMIUM \
  ::gl::g_current_gl_context->glCopySubTextureCHROMIUMFn
#define glCopyTexImage2D ::gl::g_current_gl_context->glCopyTexImage2DFn
#define glCopyTexSubImage2D ::gl::g_current_gl_context->glCopyTexSubImage2DFn
#define glCopyTexSubImage3D ::gl::g_current_gl_context->glCopyTexSubImage3DFn
#define glCopyTextureCHROMIUM \
  ::gl::g_current_gl_context->glCopyTextureCHROMIUMFn
#define glCoverageModulationNV \
  ::gl::g_current_gl_context->glCoverageModulationNVFn
#define glCoverFillPathInstancedNV \
  ::gl::g_current_gl_context->glCoverFillPathInstancedNVFn
#define glCoverFillPathNV ::gl::g_current_gl_context->glCoverFillPathNVFn
#define glCoverStrokePathInstancedNV \
  ::gl::g_current_gl_context->glCoverStrokePathInstancedNVFn
#define glCoverStrokePathNV ::gl::g_current_gl_context->glCoverStrokePathNVFn
#define glCreateProgram ::gl::g_current_gl_context->glCreateProgramFn
#define glCreateShader ::gl::g_current_gl_context->glCreateShaderFn
#define glCullFace ::gl::g_current_gl_context->glCullFaceFn
#define glDebugMessageCallback \
  ::gl::g_current_gl_context->glDebugMessageCallbackFn
#define glDebugMessageControl \
  ::gl::g_current_gl_context->glDebugMessageControlFn
#define glDebugMessageInsert ::gl::g_current_gl_context->glDebugMessageInsertFn
#define glDeleteBuffersARB ::gl::g_current_gl_context->glDeleteBuffersARBFn
#define glDeleteFencesAPPLE ::gl::g_current_gl_context->glDeleteFencesAPPLEFn
#define glDeleteFencesNV ::gl::g_current_gl_context->glDeleteFencesNVFn
#define glDeleteFramebuffersEXT \
  ::gl::g_current_gl_context->glDeleteFramebuffersEXTFn
#define glDeletePathsNV ::gl::g_current_gl_context->glDeletePathsNVFn
#define glDeleteProgram ::gl::g_current_gl_context->glDeleteProgramFn
#define glDeleteQueries ::gl::g_current_gl_context->glDeleteQueriesFn
#define glDeleteRenderbuffersEXT \
  ::gl::g_current_gl_context->glDeleteRenderbuffersEXTFn
#define glDeleteSamplers ::gl::g_current_gl_context->glDeleteSamplersFn
#define glDeleteShader ::gl::g_current_gl_context->glDeleteShaderFn
#define glDeleteSync ::gl::g_current_gl_context->glDeleteSyncFn
#define glDeleteTextures ::gl::g_current_gl_context->glDeleteTexturesFn
#define glDeleteTransformFeedbacks \
  ::gl::g_current_gl_context->glDeleteTransformFeedbacksFn
#define glDeleteVertexArraysOES \
  ::gl::g_current_gl_context->glDeleteVertexArraysOESFn
#define glDepthFunc ::gl::g_current_gl_context->glDepthFuncFn
#define glDepthMask ::gl::g_current_gl_context->glDepthMaskFn
#define glDepthRange ::gl::g_current_gl_context->glDepthRangeFn
#define glDepthRangef ::gl::g_current_gl_context->glDepthRangefFn
#define glDetachShader ::gl::g_current_gl_context->glDetachShaderFn
#define glDisable ::gl::g_current_gl_context->glDisableFn
#define glDisableVertexAttribArray \
  ::gl::g_current_gl_context->glDisableVertexAttribArrayFn
#define glDiscardFramebufferEXT \
  ::gl::g_current_gl_context->glDiscardFramebufferEXTFn
#define glDrawArrays ::gl::g_current_gl_context->glDrawArraysFn
#define glDrawArraysInstancedANGLE \
  ::gl::g_current_gl_context->glDrawArraysInstancedANGLEFn
#define glDrawBuffer ::gl::g_current_gl_context->glDrawBufferFn
#define glDrawBuffersARB ::gl::g_current_gl_context->glDrawBuffersARBFn
#define glDrawElements ::gl::g_current_gl_context->glDrawElementsFn
#define glDrawElementsInstancedANGLE \
  ::gl::g_current_gl_context->glDrawElementsInstancedANGLEFn
#define glDrawRangeElements ::gl::g_current_gl_context->glDrawRangeElementsFn
#define glEGLImageTargetRenderbufferStorageOES \
  ::gl::g_current_gl_context->glEGLImageTargetRenderbufferStorageOESFn
#define glEGLImageTargetTexture2DOES \
  ::gl::g_current_gl_context->glEGLImageTargetTexture2DOESFn
#define glEnable ::gl::g_current_gl_context->glEnableFn
#define glEnableVertexAttribArray \
  ::gl::g_current_gl_context->glEnableVertexAttribArrayFn
#define glEndQuery ::gl::g_current_gl_context->glEndQueryFn
#define glEndTransformFeedback \
  ::gl::g_current_gl_context->glEndTransformFeedbackFn
#define glFenceSync ::gl::g_current_gl_context->glFenceSyncFn
#define glFinish ::gl::g_current_gl_context->glFinishFn
#define glFinishFenceAPPLE ::gl::g_current_gl_context->glFinishFenceAPPLEFn
#define glFinishFenceNV ::gl::g_current_gl_context->glFinishFenceNVFn
#define glFlush ::gl::g_current_gl_context->glFlushFn
#define glFlushMappedBufferRange \
  ::gl::g_current_gl_context->glFlushMappedBufferRangeFn
#define glFramebufferRenderbufferEXT \
  ::gl::g_current_gl_context->glFramebufferRenderbufferEXTFn
#define glFramebufferTexture2DEXT \
  ::gl::g_current_gl_context->glFramebufferTexture2DEXTFn
#define glFramebufferTexture2DMultisampleEXT \
  ::gl::g_current_gl_context->glFramebufferTexture2DMultisampleEXTFn
#define glFramebufferTextureLayer \
  ::gl::g_current_gl_context->glFramebufferTextureLayerFn
#define glFrontFace ::gl::g_current_gl_context->glFrontFaceFn
#define glGenBuffersARB ::gl::g_current_gl_context->glGenBuffersARBFn
#define glGenerateMipmapEXT ::gl::g_current_gl_context->glGenerateMipmapEXTFn
#define glGenFencesAPPLE ::gl::g_current_gl_context->glGenFencesAPPLEFn
#define glGenFencesNV ::gl::g_current_gl_context->glGenFencesNVFn
#define glGenFramebuffersEXT ::gl::g_current_gl_context->glGenFramebuffersEXTFn
#define glGenPathsNV ::gl::g_current_gl_context->glGenPathsNVFn
#define glGenQueries ::gl::g_current_gl_context->glGenQueriesFn
#define glGenRenderbuffersEXT \
  ::gl::g_current_gl_context->glGenRenderbuffersEXTFn
#define glGenSamplers ::gl::g_current_gl_context->glGenSamplersFn
#define glGenTextures ::gl::g_current_gl_context->glGenTexturesFn
#define glGenTransformFeedbacks \
  ::gl::g_current_gl_context->glGenTransformFeedbacksFn
#define glGenVertexArraysOES ::gl::g_current_gl_context->glGenVertexArraysOESFn
#define glGetActiveAttrib ::gl::g_current_gl_context->glGetActiveAttribFn
#define glGetActiveUniform ::gl::g_current_gl_context->glGetActiveUniformFn
#define glGetActiveUniformBlockiv \
  ::gl::g_current_gl_context->glGetActiveUniformBlockivFn
#define glGetActiveUniformBlockivRobustANGLE \
  ::gl::g_current_gl_context->glGetActiveUniformBlockivRobustANGLEFn
#define glGetActiveUniformBlockName \
  ::gl::g_current_gl_context->glGetActiveUniformBlockNameFn
#define glGetActiveUniformsiv \
  ::gl::g_current_gl_context->glGetActiveUniformsivFn
#define glGetAttachedShaders ::gl::g_current_gl_context->glGetAttachedShadersFn
#define glGetAttribLocation ::gl::g_current_gl_context->glGetAttribLocationFn
#define glGetBooleani_vRobustANGLE \
  ::gl::g_current_gl_context->glGetBooleani_vRobustANGLEFn
#define glGetBooleanv ::gl::g_current_gl_context->glGetBooleanvFn
#define glGetBooleanvRobustANGLE \
  ::gl::g_current_gl_context->glGetBooleanvRobustANGLEFn
#define glGetBufferParameteri64vRobustANGLE \
  ::gl::g_current_gl_context->glGetBufferParameteri64vRobustANGLEFn
#define glGetBufferParameteriv \
  ::gl::g_current_gl_context->glGetBufferParameterivFn
#define glGetBufferParameterivRobustANGLE \
  ::gl::g_current_gl_context->glGetBufferParameterivRobustANGLEFn
#define glGetBufferPointervRobustANGLE \
  ::gl::g_current_gl_context->glGetBufferPointervRobustANGLEFn
#define glGetDebugMessageLog ::gl::g_current_gl_context->glGetDebugMessageLogFn
#define glGetError ::gl::g_current_gl_context->glGetErrorFn
#define glGetFenceivNV ::gl::g_current_gl_context->glGetFenceivNVFn
#define glGetFloatv ::gl::g_current_gl_context->glGetFloatvFn
#define glGetFloatvRobustANGLE \
  ::gl::g_current_gl_context->glGetFloatvRobustANGLEFn
#define glGetFragDataIndex ::gl::g_current_gl_context->glGetFragDataIndexFn
#define glGetFragDataLocation \
  ::gl::g_current_gl_context->glGetFragDataLocationFn
#define glGetFramebufferAttachmentParameterivEXT \
  ::gl::g_current_gl_context->glGetFramebufferAttachmentParameterivEXTFn
#define glGetFramebufferAttachmentParameterivRobustANGLE \
  ::gl::g_current_gl_context->glGetFramebufferAttachmentParameterivRobustANGLEFn
#define glGetFramebufferParameterivRobustANGLE \
  ::gl::g_current_gl_context->glGetFramebufferParameterivRobustANGLEFn
#define glGetGraphicsResetStatusARB \
  ::gl::g_current_gl_context->glGetGraphicsResetStatusARBFn
#define glGetInteger64i_v ::gl::g_current_gl_context->glGetInteger64i_vFn
#define glGetInteger64i_vRobustANGLE \
  ::gl::g_current_gl_context->glGetInteger64i_vRobustANGLEFn
#define glGetInteger64v ::gl::g_current_gl_context->glGetInteger64vFn
#define glGetInteger64vRobustANGLE \
  ::gl::g_current_gl_context->glGetInteger64vRobustANGLEFn
#define glGetIntegeri_v ::gl::g_current_gl_context->glGetIntegeri_vFn
#define glGetIntegeri_vRobustANGLE \
  ::gl::g_current_gl_context->glGetIntegeri_vRobustANGLEFn
#define glGetIntegerv ::gl::g_current_gl_context->glGetIntegervFn
#define glGetIntegervRobustANGLE \
  ::gl::g_current_gl_context->glGetIntegervRobustANGLEFn
#define glGetInternalformativ \
  ::gl::g_current_gl_context->glGetInternalformativFn
#define glGetInternalformativRobustANGLE \
  ::gl::g_current_gl_context->glGetInternalformativRobustANGLEFn
#define glGetMultisamplefv ::gl::g_current_gl_context->glGetMultisamplefvFn
#define glGetMultisamplefvRobustANGLE \
  ::gl::g_current_gl_context->glGetMultisamplefvRobustANGLEFn
#define glGetnUniformfvRobustANGLE \
  ::gl::g_current_gl_context->glGetnUniformfvRobustANGLEFn
#define glGetnUniformivRobustANGLE \
  ::gl::g_current_gl_context->glGetnUniformivRobustANGLEFn
#define glGetnUniformuivRobustANGLE \
  ::gl::g_current_gl_context->glGetnUniformuivRobustANGLEFn
#define glGetObjectLabel ::gl::g_current_gl_context->glGetObjectLabelFn
#define glGetObjectPtrLabel ::gl::g_current_gl_context->glGetObjectPtrLabelFn
#define glGetPointerv ::gl::g_current_gl_context->glGetPointervFn
#define glGetPointervRobustANGLERobustANGLE \
  ::gl::g_current_gl_context->glGetPointervRobustANGLERobustANGLEFn
#define glGetProgramBinary ::gl::g_current_gl_context->glGetProgramBinaryFn
#define glGetProgramInfoLog ::gl::g_current_gl_context->glGetProgramInfoLogFn
#define glGetProgramInterfaceiv \
  ::gl::g_current_gl_context->glGetProgramInterfaceivFn
#define glGetProgramInterfaceivRobustANGLE \
  ::gl::g_current_gl_context->glGetProgramInterfaceivRobustANGLEFn
#define glGetProgramiv ::gl::g_current_gl_context->glGetProgramivFn
#define glGetProgramivRobustANGLE \
  ::gl::g_current_gl_context->glGetProgramivRobustANGLEFn
#define glGetProgramResourceiv \
  ::gl::g_current_gl_context->glGetProgramResourceivFn
#define glGetProgramResourceLocation \
  ::gl::g_current_gl_context->glGetProgramResourceLocationFn
#define glGetProgramResourceName \
  ::gl::g_current_gl_context->glGetProgramResourceNameFn
#define glGetQueryiv ::gl::g_current_gl_context->glGetQueryivFn
#define glGetQueryivRobustANGLE \
  ::gl::g_current_gl_context->glGetQueryivRobustANGLEFn
#define glGetQueryObjecti64v ::gl::g_current_gl_context->glGetQueryObjecti64vFn
#define glGetQueryObjecti64vRobustANGLE \
  ::gl::g_current_gl_context->glGetQueryObjecti64vRobustANGLEFn
#define glGetQueryObjectiv ::gl::g_current_gl_context->glGetQueryObjectivFn
#define glGetQueryObjectivRobustANGLE \
  ::gl::g_current_gl_context->glGetQueryObjectivRobustANGLEFn
#define glGetQueryObjectui64v \
  ::gl::g_current_gl_context->glGetQueryObjectui64vFn
#define glGetQueryObjectui64vRobustANGLE \
  ::gl::g_current_gl_context->glGetQueryObjectui64vRobustANGLEFn
#define glGetQueryObjectuiv ::gl::g_current_gl_context->glGetQueryObjectuivFn
#define glGetQueryObjectuivRobustANGLE \
  ::gl::g_current_gl_context->glGetQueryObjectuivRobustANGLEFn
#define glGetRenderbufferParameterivEXT \
  ::gl::g_current_gl_context->glGetRenderbufferParameterivEXTFn
#define glGetRenderbufferParameterivRobustANGLE \
  ::gl::g_current_gl_context->glGetRenderbufferParameterivRobustANGLEFn
#define glGetSamplerParameterfv \
  ::gl::g_current_gl_context->glGetSamplerParameterfvFn
#define glGetSamplerParameterfvRobustANGLE \
  ::gl::g_current_gl_context->glGetSamplerParameterfvRobustANGLEFn
#define glGetSamplerParameterIivRobustANGLE \
  ::gl::g_current_gl_context->glGetSamplerParameterIivRobustANGLEFn
#define glGetSamplerParameterIuivRobustANGLE \
  ::gl::g_current_gl_context->glGetSamplerParameterIuivRobustANGLEFn
#define glGetSamplerParameteriv \
  ::gl::g_current_gl_context->glGetSamplerParameterivFn
#define glGetSamplerParameterivRobustANGLE \
  ::gl::g_current_gl_context->glGetSamplerParameterivRobustANGLEFn
#define glGetShaderInfoLog ::gl::g_current_gl_context->glGetShaderInfoLogFn
#define glGetShaderiv ::gl::g_current_gl_context->glGetShaderivFn
#define glGetShaderivRobustANGLE \
  ::gl::g_current_gl_context->glGetShaderivRobustANGLEFn
#define glGetShaderPrecisionFormat \
  ::gl::g_current_gl_context->glGetShaderPrecisionFormatFn
#define glGetShaderSource ::gl::g_current_gl_context->glGetShaderSourceFn
#define glGetString ::gl::g_current_gl_context->glGetStringFn
#define glGetStringi ::gl::g_current_gl_context->glGetStringiFn
#define glGetSynciv ::gl::g_current_gl_context->glGetSyncivFn
#define glGetTexLevelParameterfv \
  ::gl::g_current_gl_context->glGetTexLevelParameterfvFn
#define glGetTexLevelParameterfvRobustANGLE \
  ::gl::g_current_gl_context->glGetTexLevelParameterfvRobustANGLEFn
#define glGetTexLevelParameteriv \
  ::gl::g_current_gl_context->glGetTexLevelParameterivFn
#define glGetTexLevelParameterivRobustANGLE \
  ::gl::g_current_gl_context->glGetTexLevelParameterivRobustANGLEFn
#define glGetTexParameterfv ::gl::g_current_gl_context->glGetTexParameterfvFn
#define glGetTexParameterfvRobustANGLE \
  ::gl::g_current_gl_context->glGetTexParameterfvRobustANGLEFn
#define glGetTexParameterIivRobustANGLE \
  ::gl::g_current_gl_context->glGetTexParameterIivRobustANGLEFn
#define glGetTexParameterIuivRobustANGLE \
  ::gl::g_current_gl_context->glGetTexParameterIuivRobustANGLEFn
#define glGetTexParameteriv ::gl::g_current_gl_context->glGetTexParameterivFn
#define glGetTexParameterivRobustANGLE \
  ::gl::g_current_gl_context->glGetTexParameterivRobustANGLEFn
#define glGetTransformFeedbackVarying \
  ::gl::g_current_gl_context->glGetTransformFeedbackVaryingFn
#define glGetTranslatedShaderSourceANGLE \
  ::gl::g_current_gl_context->glGetTranslatedShaderSourceANGLEFn
#define glGetUniformBlockIndex \
  ::gl::g_current_gl_context->glGetUniformBlockIndexFn
#define glGetUniformfv ::gl::g_current_gl_context->glGetUniformfvFn
#define glGetUniformfvRobustANGLE \
  ::gl::g_current_gl_context->glGetUniformfvRobustANGLEFn
#define glGetUniformIndices ::gl::g_current_gl_context->glGetUniformIndicesFn
#define glGetUniformiv ::gl::g_current_gl_context->glGetUniformivFn
#define glGetUniformivRobustANGLE \
  ::gl::g_current_gl_context->glGetUniformivRobustANGLEFn
#define glGetUniformLocation ::gl::g_current_gl_context->glGetUniformLocationFn
#define glGetUniformuiv ::gl::g_current_gl_context->glGetUniformuivFn
#define glGetUniformuivRobustANGLE \
  ::gl::g_current_gl_context->glGetUniformuivRobustANGLEFn
#define glGetVertexAttribfv ::gl::g_current_gl_context->glGetVertexAttribfvFn
#define glGetVertexAttribfvRobustANGLE \
  ::gl::g_current_gl_context->glGetVertexAttribfvRobustANGLEFn
#define glGetVertexAttribIivRobustANGLE \
  ::gl::g_current_gl_context->glGetVertexAttribIivRobustANGLEFn
#define glGetVertexAttribIuivRobustANGLE \
  ::gl::g_current_gl_context->glGetVertexAttribIuivRobustANGLEFn
#define glGetVertexAttribiv ::gl::g_current_gl_context->glGetVertexAttribivFn
#define glGetVertexAttribivRobustANGLE \
  ::gl::g_current_gl_context->glGetVertexAttribivRobustANGLEFn
#define glGetVertexAttribPointerv \
  ::gl::g_current_gl_context->glGetVertexAttribPointervFn
#define glGetVertexAttribPointervRobustANGLE \
  ::gl::g_current_gl_context->glGetVertexAttribPointervRobustANGLEFn
#define glHint ::gl::g_current_gl_context->glHintFn
#define glInsertEventMarkerEXT \
  ::gl::g_current_gl_context->glInsertEventMarkerEXTFn
#define glInvalidateFramebuffer \
  ::gl::g_current_gl_context->glInvalidateFramebufferFn
#define glInvalidateSubFramebuffer \
  ::gl::g_current_gl_context->glInvalidateSubFramebufferFn
#define glIsBuffer ::gl::g_current_gl_context->glIsBufferFn
#define glIsEnabled ::gl::g_current_gl_context->glIsEnabledFn
#define glIsFenceAPPLE ::gl::g_current_gl_context->glIsFenceAPPLEFn
#define glIsFenceNV ::gl::g_current_gl_context->glIsFenceNVFn
#define glIsFramebufferEXT ::gl::g_current_gl_context->glIsFramebufferEXTFn
#define glIsPathNV ::gl::g_current_gl_context->glIsPathNVFn
#define glIsProgram ::gl::g_current_gl_context->glIsProgramFn
#define glIsQuery ::gl::g_current_gl_context->glIsQueryFn
#define glIsRenderbufferEXT ::gl::g_current_gl_context->glIsRenderbufferEXTFn
#define glIsSampler ::gl::g_current_gl_context->glIsSamplerFn
#define glIsShader ::gl::g_current_gl_context->glIsShaderFn
#define glIsSync ::gl::g_current_gl_context->glIsSyncFn
#define glIsTexture ::gl::g_current_gl_context->glIsTextureFn
#define glIsTransformFeedback \
  ::gl::g_current_gl_context->glIsTransformFeedbackFn
#define glIsVertexArrayOES ::gl::g_current_gl_context->glIsVertexArrayOESFn
#define glLineWidth ::gl::g_current_gl_context->glLineWidthFn
#define glLinkProgram ::gl::g_current_gl_context->glLinkProgramFn
#define glMapBuffer ::gl::g_current_gl_context->glMapBufferFn
#define glMapBufferRange ::gl::g_current_gl_context->glMapBufferRangeFn
#define glMatrixLoadfEXT ::gl::g_current_gl_context->glMatrixLoadfEXTFn
#define glMatrixLoadIdentityEXT \
  ::gl::g_current_gl_context->glMatrixLoadIdentityEXTFn
#define glMemoryBarrierEXT ::gl::g_current_gl_context->glMemoryBarrierEXTFn
#define glObjectLabel ::gl::g_current_gl_context->glObjectLabelFn
#define glObjectPtrLabel ::gl::g_current_gl_context->glObjectPtrLabelFn
#define glPathCommandsNV ::gl::g_current_gl_context->glPathCommandsNVFn
#define glPathParameterfNV ::gl::g_current_gl_context->glPathParameterfNVFn
#define glPathParameteriNV ::gl::g_current_gl_context->glPathParameteriNVFn
#define glPathStencilFuncNV ::gl::g_current_gl_context->glPathStencilFuncNVFn
#define glPauseTransformFeedback \
  ::gl::g_current_gl_context->glPauseTransformFeedbackFn
#define glPixelStorei ::gl::g_current_gl_context->glPixelStoreiFn
#define glPointParameteri ::gl::g_current_gl_context->glPointParameteriFn
#define glPolygonMode ::gl::g_current_gl_context->glPolygonModeFn
#define glPolygonOffset ::gl::g_current_gl_context->glPolygonOffsetFn
#define glPopDebugGroup ::gl::g_current_gl_context->glPopDebugGroupFn
#define glPopGroupMarkerEXT ::gl::g_current_gl_context->glPopGroupMarkerEXTFn
#define glPrimitiveRestartIndex \
  ::gl::g_current_gl_context->glPrimitiveRestartIndexFn
#define glProgramBinary ::gl::g_current_gl_context->glProgramBinaryFn
#define glProgramParameteri ::gl::g_current_gl_context->glProgramParameteriFn
#define glProgramPathFragmentInputGenNV \
  ::gl::g_current_gl_context->glProgramPathFragmentInputGenNVFn
#define glPushDebugGroup ::gl::g_current_gl_context->glPushDebugGroupFn
#define glPushGroupMarkerEXT ::gl::g_current_gl_context->glPushGroupMarkerEXTFn
#define glQueryCounter ::gl::g_current_gl_context->glQueryCounterFn
#define glReadBuffer ::gl::g_current_gl_context->glReadBufferFn
#define glReadnPixelsRobustANGLE \
  ::gl::g_current_gl_context->glReadnPixelsRobustANGLEFn
#define glReadPixels ::gl::g_current_gl_context->glReadPixelsFn
#define glReadPixelsRobustANGLE \
  ::gl::g_current_gl_context->glReadPixelsRobustANGLEFn
#define glReleaseShaderCompiler \
  ::gl::g_current_gl_context->glReleaseShaderCompilerFn
#define glRenderbufferStorageEXT \
  ::gl::g_current_gl_context->glRenderbufferStorageEXTFn
#define glRenderbufferStorageMultisample \
  ::gl::g_current_gl_context->glRenderbufferStorageMultisampleFn
#define glRenderbufferStorageMultisampleEXT \
  ::gl::g_current_gl_context->glRenderbufferStorageMultisampleEXTFn
#define glRequestExtensionANGLE \
  ::gl::g_current_gl_context->glRequestExtensionANGLEFn
#define glResumeTransformFeedback \
  ::gl::g_current_gl_context->glResumeTransformFeedbackFn
#define glSampleCoverage ::gl::g_current_gl_context->glSampleCoverageFn
#define glSamplerParameterf ::gl::g_current_gl_context->glSamplerParameterfFn
#define glSamplerParameterfv ::gl::g_current_gl_context->glSamplerParameterfvFn
#define glSamplerParameterfvRobustANGLE \
  ::gl::g_current_gl_context->glSamplerParameterfvRobustANGLEFn
#define glSamplerParameteri ::gl::g_current_gl_context->glSamplerParameteriFn
#define glSamplerParameterIivRobustANGLE \
  ::gl::g_current_gl_context->glSamplerParameterIivRobustANGLEFn
#define glSamplerParameterIuivRobustANGLE \
  ::gl::g_current_gl_context->glSamplerParameterIuivRobustANGLEFn
#define glSamplerParameteriv ::gl::g_current_gl_context->glSamplerParameterivFn
#define glSamplerParameterivRobustANGLE \
  ::gl::g_current_gl_context->glSamplerParameterivRobustANGLEFn
#define glScissor ::gl::g_current_gl_context->glScissorFn
#define glSetFenceAPPLE ::gl::g_current_gl_context->glSetFenceAPPLEFn
#define glSetFenceNV ::gl::g_current_gl_context->glSetFenceNVFn
#define glShaderBinary ::gl::g_current_gl_context->glShaderBinaryFn
#define glShaderSource ::gl::g_current_gl_context->glShaderSourceFn
#define glStencilFillPathInstancedNV \
  ::gl::g_current_gl_context->glStencilFillPathInstancedNVFn
#define glStencilFillPathNV ::gl::g_current_gl_context->glStencilFillPathNVFn
#define glStencilFunc ::gl::g_current_gl_context->glStencilFuncFn
#define glStencilFuncSeparate \
  ::gl::g_current_gl_context->glStencilFuncSeparateFn
#define glStencilMask ::gl::g_current_gl_context->glStencilMaskFn
#define glStencilMaskSeparate \
  ::gl::g_current_gl_context->glStencilMaskSeparateFn
#define glStencilOp ::gl::g_current_gl_context->glStencilOpFn
#define glStencilOpSeparate ::gl::g_current_gl_context->glStencilOpSeparateFn
#define glStencilStrokePathInstancedNV \
  ::gl::g_current_gl_context->glStencilStrokePathInstancedNVFn
#define glStencilStrokePathNV \
  ::gl::g_current_gl_context->glStencilStrokePathNVFn
#define glStencilThenCoverFillPathInstancedNV \
  ::gl::g_current_gl_context->glStencilThenCoverFillPathInstancedNVFn
#define glStencilThenCoverFillPathNV \
  ::gl::g_current_gl_context->glStencilThenCoverFillPathNVFn
#define glStencilThenCoverStrokePathInstancedNV \
  ::gl::g_current_gl_context->glStencilThenCoverStrokePathInstancedNVFn
#define glStencilThenCoverStrokePathNV \
  ::gl::g_current_gl_context->glStencilThenCoverStrokePathNVFn
#define glTestFenceAPPLE ::gl::g_current_gl_context->glTestFenceAPPLEFn
#define glTestFenceNV ::gl::g_current_gl_context->glTestFenceNVFn
#define glTexBuffer ::gl::g_current_gl_context->glTexBufferFn
#define glTexBufferRange ::gl::g_current_gl_context->glTexBufferRangeFn
#define glTexImage2D ::gl::g_current_gl_context->glTexImage2DFn
#define glTexImage2DRobustANGLE \
  ::gl::g_current_gl_context->glTexImage2DRobustANGLEFn
#define glTexImage3D ::gl::g_current_gl_context->glTexImage3DFn
#define glTexImage3DRobustANGLE \
  ::gl::g_current_gl_context->glTexImage3DRobustANGLEFn
#define glTexParameterf ::gl::g_current_gl_context->glTexParameterfFn
#define glTexParameterfv ::gl::g_current_gl_context->glTexParameterfvFn
#define glTexParameterfvRobustANGLE \
  ::gl::g_current_gl_context->glTexParameterfvRobustANGLEFn
#define glTexParameteri ::gl::g_current_gl_context->glTexParameteriFn
#define glTexParameterIivRobustANGLE \
  ::gl::g_current_gl_context->glTexParameterIivRobustANGLEFn
#define glTexParameterIuivRobustANGLE \
  ::gl::g_current_gl_context->glTexParameterIuivRobustANGLEFn
#define glTexParameteriv ::gl::g_current_gl_context->glTexParameterivFn
#define glTexParameterivRobustANGLE \
  ::gl::g_current_gl_context->glTexParameterivRobustANGLEFn
#define glTexStorage2DEXT ::gl::g_current_gl_context->glTexStorage2DEXTFn
#define glTexStorage3D ::gl::g_current_gl_context->glTexStorage3DFn
#define glTexSubImage2D ::gl::g_current_gl_context->glTexSubImage2DFn
#define glTexSubImage2DRobustANGLE \
  ::gl::g_current_gl_context->glTexSubImage2DRobustANGLEFn
#define glTexSubImage3D ::gl::g_current_gl_context->glTexSubImage3DFn
#define glTexSubImage3DRobustANGLE \
  ::gl::g_current_gl_context->glTexSubImage3DRobustANGLEFn
#define glTransformFeedbackVaryings \
  ::gl::g_current_gl_context->glTransformFeedbackVaryingsFn
#define glUniform1f ::gl::g_current_gl_context->glUniform1fFn
#define glUniform1fv ::gl::g_current_gl_context->glUniform1fvFn
#define glUniform1i ::gl::g_current_gl_context->glUniform1iFn
#define glUniform1iv ::gl::g_current_gl_context->glUniform1ivFn
#define glUniform1ui ::gl::g_current_gl_context->glUniform1uiFn
#define glUniform1uiv ::gl::g_current_gl_context->glUniform1uivFn
#define glUniform2f ::gl::g_current_gl_context->glUniform2fFn
#define glUniform2fv ::gl::g_current_gl_context->glUniform2fvFn
#define glUniform2i ::gl::g_current_gl_context->glUniform2iFn
#define glUniform2iv ::gl::g_current_gl_context->glUniform2ivFn
#define glUniform2ui ::gl::g_current_gl_context->glUniform2uiFn
#define glUniform2uiv ::gl::g_current_gl_context->glUniform2uivFn
#define glUniform3f ::gl::g_current_gl_context->glUniform3fFn
#define glUniform3fv ::gl::g_current_gl_context->glUniform3fvFn
#define glUniform3i ::gl::g_current_gl_context->glUniform3iFn
#define glUniform3iv ::gl::g_current_gl_context->glUniform3ivFn
#define glUniform3ui ::gl::g_current_gl_context->glUniform3uiFn
#define glUniform3uiv ::gl::g_current_gl_context->glUniform3uivFn
#define glUniform4f ::gl::g_current_gl_context->glUniform4fFn
#define glUniform4fv ::gl::g_current_gl_context->glUniform4fvFn
#define glUniform4i ::gl::g_current_gl_context->glUniform4iFn
#define glUniform4iv ::gl::g_current_gl_context->glUniform4ivFn
#define glUniform4ui ::gl::g_current_gl_context->glUniform4uiFn
#define glUniform4uiv ::gl::g_current_gl_context->glUniform4uivFn
#define glUniformBlockBinding \
  ::gl::g_current_gl_context->glUniformBlockBindingFn
#define glUniformMatrix2fv ::gl::g_current_gl_context->glUniformMatrix2fvFn
#define glUniformMatrix2x3fv ::gl::g_current_gl_context->glUniformMatrix2x3fvFn
#define glUniformMatrix2x4fv ::gl::g_current_gl_context->glUniformMatrix2x4fvFn
#define glUniformMatrix3fv ::gl::g_current_gl_context->glUniformMatrix3fvFn
#define glUniformMatrix3x2fv ::gl::g_current_gl_context->glUniformMatrix3x2fvFn
#define glUniformMatrix3x4fv ::gl::g_current_gl_context->glUniformMatrix3x4fvFn
#define glUniformMatrix4fv ::gl::g_current_gl_context->glUniformMatrix4fvFn
#define glUniformMatrix4x2fv ::gl::g_current_gl_context->glUniformMatrix4x2fvFn
#define glUniformMatrix4x3fv ::gl::g_current_gl_context->glUniformMatrix4x3fvFn
#define glUnmapBuffer ::gl::g_current_gl_context->glUnmapBufferFn
#define glUseProgram ::gl::g_current_gl_context->glUseProgramFn
#define glValidateProgram ::gl::g_current_gl_context->glValidateProgramFn
#define glVertexAttrib1f ::gl::g_current_gl_context->glVertexAttrib1fFn
#define glVertexAttrib1fv ::gl::g_current_gl_context->glVertexAttrib1fvFn
#define glVertexAttrib2f ::gl::g_current_gl_context->glVertexAttrib2fFn
#define glVertexAttrib2fv ::gl::g_current_gl_context->glVertexAttrib2fvFn
#define glVertexAttrib3f ::gl::g_current_gl_context->glVertexAttrib3fFn
#define glVertexAttrib3fv ::gl::g_current_gl_context->glVertexAttrib3fvFn
#define glVertexAttrib4f ::gl::g_current_gl_context->glVertexAttrib4fFn
#define glVertexAttrib4fv ::gl::g_current_gl_context->glVertexAttrib4fvFn
#define glVertexAttribDivisorANGLE \
  ::gl::g_current_gl_context->glVertexAttribDivisorANGLEFn
#define glVertexAttribI4i ::gl::g_current_gl_context->glVertexAttribI4iFn
#define glVertexAttribI4iv ::gl::g_current_gl_context->glVertexAttribI4ivFn
#define glVertexAttribI4ui ::gl::g_current_gl_context->glVertexAttribI4uiFn
#define glVertexAttribI4uiv ::gl::g_current_gl_context->glVertexAttribI4uivFn
#define glVertexAttribIPointer \
  ::gl::g_current_gl_context->glVertexAttribIPointerFn
#define glVertexAttribPointer \
  ::gl::g_current_gl_context->glVertexAttribPointerFn
#define glViewport ::gl::g_current_gl_context->glViewportFn
#define glWaitSync ::gl::g_current_gl_context->glWaitSyncFn
#define glWindowRectanglesEXT \
  ::gl::g_current_gl_context->glWindowRectanglesEXTFn

#endif  //  UI_GL_GL_BINDINGS_AUTOGEN_GL_H_
