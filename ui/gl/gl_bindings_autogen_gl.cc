// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// ui/gl/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/protected_memory.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gl {

void DriverGL::InitializeStaticBindings() {
  // Ensure struct has been zero-initialized.
  char* this_bytes = reinterpret_cast<char*>(this);
  DCHECK(this_bytes[0] == 0);
  DCHECK(memcmp(this_bytes, this_bytes + 1, sizeof(*this) - 1) == 0);

  fn.glActiveTextureFn = reinterpret_cast<glActiveTextureProc>(
      GetGLProcAddress("glActiveTexture"));
  fn.glAttachShaderFn =
      reinterpret_cast<glAttachShaderProc>(GetGLProcAddress("glAttachShader"));
  fn.glBindAttribLocationFn = reinterpret_cast<glBindAttribLocationProc>(
      GetGLProcAddress("glBindAttribLocation"));
  fn.glBindBufferFn =
      reinterpret_cast<glBindBufferProc>(GetGLProcAddress("glBindBuffer"));
  fn.glBindTextureFn =
      reinterpret_cast<glBindTextureProc>(GetGLProcAddress("glBindTexture"));
  fn.glBlendColorFn =
      reinterpret_cast<glBlendColorProc>(GetGLProcAddress("glBlendColor"));
  fn.glBlendEquationFn = reinterpret_cast<glBlendEquationProc>(
      GetGLProcAddress("glBlendEquation"));
  fn.glBlendEquationSeparateFn = reinterpret_cast<glBlendEquationSeparateProc>(
      GetGLProcAddress("glBlendEquationSeparate"));
  fn.glBlendFuncFn =
      reinterpret_cast<glBlendFuncProc>(GetGLProcAddress("glBlendFunc"));
  fn.glBlendFuncSeparateFn = reinterpret_cast<glBlendFuncSeparateProc>(
      GetGLProcAddress("glBlendFuncSeparate"));
  fn.glBufferDataFn =
      reinterpret_cast<glBufferDataProc>(GetGLProcAddress("glBufferData"));
  fn.glBufferSubDataFn = reinterpret_cast<glBufferSubDataProc>(
      GetGLProcAddress("glBufferSubData"));
  fn.glClearFn = reinterpret_cast<glClearProc>(GetGLProcAddress("glClear"));
  fn.glClearColorFn =
      reinterpret_cast<glClearColorProc>(GetGLProcAddress("glClearColor"));
  fn.glClearDepthFn =
      reinterpret_cast<glClearDepthProc>(GetGLProcAddress("glClearDepth"));
  fn.glClearStencilFn =
      reinterpret_cast<glClearStencilProc>(GetGLProcAddress("glClearStencil"));
  fn.glColorMaskFn =
      reinterpret_cast<glColorMaskProc>(GetGLProcAddress("glColorMask"));
  fn.glCompileShaderFn = reinterpret_cast<glCompileShaderProc>(
      GetGLProcAddress("glCompileShader"));
  fn.glCompressedTexImage2DFn = reinterpret_cast<glCompressedTexImage2DProc>(
      GetGLProcAddress("glCompressedTexImage2D"));
  fn.glCompressedTexSubImage2DFn =
      reinterpret_cast<glCompressedTexSubImage2DProc>(
          GetGLProcAddress("glCompressedTexSubImage2D"));
  fn.glCopyTexImage2DFn = reinterpret_cast<glCopyTexImage2DProc>(
      GetGLProcAddress("glCopyTexImage2D"));
  fn.glCopyTexSubImage2DFn = reinterpret_cast<glCopyTexSubImage2DProc>(
      GetGLProcAddress("glCopyTexSubImage2D"));
  fn.glCreateProgramFn = reinterpret_cast<glCreateProgramProc>(
      GetGLProcAddress("glCreateProgram"));
  fn.glCreateShaderFn =
      reinterpret_cast<glCreateShaderProc>(GetGLProcAddress("glCreateShader"));
  fn.glCullFaceFn =
      reinterpret_cast<glCullFaceProc>(GetGLProcAddress("glCullFace"));
  fn.glDeleteBuffersARBFn = reinterpret_cast<glDeleteBuffersARBProc>(
      GetGLProcAddress("glDeleteBuffers"));
  fn.glDeleteProgramFn = reinterpret_cast<glDeleteProgramProc>(
      GetGLProcAddress("glDeleteProgram"));
  fn.glDeleteShaderFn =
      reinterpret_cast<glDeleteShaderProc>(GetGLProcAddress("glDeleteShader"));
  fn.glDeleteTexturesFn = reinterpret_cast<glDeleteTexturesProc>(
      GetGLProcAddress("glDeleteTextures"));
  fn.glDepthFuncFn =
      reinterpret_cast<glDepthFuncProc>(GetGLProcAddress("glDepthFunc"));
  fn.glDepthMaskFn =
      reinterpret_cast<glDepthMaskProc>(GetGLProcAddress("glDepthMask"));
  fn.glDepthRangeFn =
      reinterpret_cast<glDepthRangeProc>(GetGLProcAddress("glDepthRange"));
  fn.glDetachShaderFn =
      reinterpret_cast<glDetachShaderProc>(GetGLProcAddress("glDetachShader"));
  fn.glDisableFn =
      reinterpret_cast<glDisableProc>(GetGLProcAddress("glDisable"));
  fn.glDisableVertexAttribArrayFn =
      reinterpret_cast<glDisableVertexAttribArrayProc>(
          GetGLProcAddress("glDisableVertexAttribArray"));
  fn.glDrawArraysFn =
      reinterpret_cast<glDrawArraysProc>(GetGLProcAddress("glDrawArrays"));
  fn.glDrawElementsFn =
      reinterpret_cast<glDrawElementsProc>(GetGLProcAddress("glDrawElements"));
  fn.glEnableFn = reinterpret_cast<glEnableProc>(GetGLProcAddress("glEnable"));
  fn.glEnableVertexAttribArrayFn =
      reinterpret_cast<glEnableVertexAttribArrayProc>(
          GetGLProcAddress("glEnableVertexAttribArray"));
  fn.glFinishFn = reinterpret_cast<glFinishProc>(GetGLProcAddress("glFinish"));
  fn.glFlushFn = reinterpret_cast<glFlushProc>(GetGLProcAddress("glFlush"));
  fn.glFrontFaceFn =
      reinterpret_cast<glFrontFaceProc>(GetGLProcAddress("glFrontFace"));
  fn.glGenBuffersARBFn =
      reinterpret_cast<glGenBuffersARBProc>(GetGLProcAddress("glGenBuffers"));
  fn.glGenTexturesFn =
      reinterpret_cast<glGenTexturesProc>(GetGLProcAddress("glGenTextures"));
  fn.glGetActiveAttribFn = reinterpret_cast<glGetActiveAttribProc>(
      GetGLProcAddress("glGetActiveAttrib"));
  fn.glGetActiveUniformFn = reinterpret_cast<glGetActiveUniformProc>(
      GetGLProcAddress("glGetActiveUniform"));
  fn.glGetAttachedShadersFn = reinterpret_cast<glGetAttachedShadersProc>(
      GetGLProcAddress("glGetAttachedShaders"));
  fn.glGetAttribLocationFn = reinterpret_cast<glGetAttribLocationProc>(
      GetGLProcAddress("glGetAttribLocation"));
  fn.glGetBooleanvFn =
      reinterpret_cast<glGetBooleanvProc>(GetGLProcAddress("glGetBooleanv"));
  fn.glGetBufferParameterivFn = reinterpret_cast<glGetBufferParameterivProc>(
      GetGLProcAddress("glGetBufferParameteriv"));
  fn.glGetErrorFn =
      reinterpret_cast<glGetErrorProc>(GetGLProcAddress("glGetError"));
  fn.glGetFloatvFn =
      reinterpret_cast<glGetFloatvProc>(GetGLProcAddress("glGetFloatv"));
  fn.glGetIntegervFn =
      reinterpret_cast<glGetIntegervProc>(GetGLProcAddress("glGetIntegerv"));
  fn.glGetProgramInfoLogFn = reinterpret_cast<glGetProgramInfoLogProc>(
      GetGLProcAddress("glGetProgramInfoLog"));
  fn.glGetProgramivFn =
      reinterpret_cast<glGetProgramivProc>(GetGLProcAddress("glGetProgramiv"));
  fn.glGetShaderInfoLogFn = reinterpret_cast<glGetShaderInfoLogProc>(
      GetGLProcAddress("glGetShaderInfoLog"));
  fn.glGetShaderivFn =
      reinterpret_cast<glGetShaderivProc>(GetGLProcAddress("glGetShaderiv"));
  fn.glGetShaderSourceFn = reinterpret_cast<glGetShaderSourceProc>(
      GetGLProcAddress("glGetShaderSource"));
  fn.glGetStringFn =
      reinterpret_cast<glGetStringProc>(GetGLProcAddress("glGetString"));
  fn.glGetStringiFn =
      reinterpret_cast<glGetStringiProc>(GetGLProcAddress("glGetStringi"));
  fn.glGetTexParameterfvFn = reinterpret_cast<glGetTexParameterfvProc>(
      GetGLProcAddress("glGetTexParameterfv"));
  fn.glGetTexParameterivFn = reinterpret_cast<glGetTexParameterivProc>(
      GetGLProcAddress("glGetTexParameteriv"));
  fn.glGetUniformfvFn =
      reinterpret_cast<glGetUniformfvProc>(GetGLProcAddress("glGetUniformfv"));
  fn.glGetUniformivFn =
      reinterpret_cast<glGetUniformivProc>(GetGLProcAddress("glGetUniformiv"));
  fn.glGetUniformLocationFn = reinterpret_cast<glGetUniformLocationProc>(
      GetGLProcAddress("glGetUniformLocation"));
  fn.glGetVertexAttribfvFn = reinterpret_cast<glGetVertexAttribfvProc>(
      GetGLProcAddress("glGetVertexAttribfv"));
  fn.glGetVertexAttribivFn = reinterpret_cast<glGetVertexAttribivProc>(
      GetGLProcAddress("glGetVertexAttribiv"));
  fn.glGetVertexAttribPointervFn =
      reinterpret_cast<glGetVertexAttribPointervProc>(
          GetGLProcAddress("glGetVertexAttribPointerv"));
  fn.glHintFn = reinterpret_cast<glHintProc>(GetGLProcAddress("glHint"));
  fn.glIsBufferFn =
      reinterpret_cast<glIsBufferProc>(GetGLProcAddress("glIsBuffer"));
  fn.glIsEnabledFn =
      reinterpret_cast<glIsEnabledProc>(GetGLProcAddress("glIsEnabled"));
  fn.glIsProgramFn =
      reinterpret_cast<glIsProgramProc>(GetGLProcAddress("glIsProgram"));
  fn.glIsShaderFn =
      reinterpret_cast<glIsShaderProc>(GetGLProcAddress("glIsShader"));
  fn.glIsTextureFn =
      reinterpret_cast<glIsTextureProc>(GetGLProcAddress("glIsTexture"));
  fn.glLineWidthFn =
      reinterpret_cast<glLineWidthProc>(GetGLProcAddress("glLineWidth"));
  fn.glLinkProgramFn =
      reinterpret_cast<glLinkProgramProc>(GetGLProcAddress("glLinkProgram"));
  fn.glPixelStoreiFn =
      reinterpret_cast<glPixelStoreiProc>(GetGLProcAddress("glPixelStorei"));
  fn.glPolygonOffsetFn = reinterpret_cast<glPolygonOffsetProc>(
      GetGLProcAddress("glPolygonOffset"));
  fn.glReadPixelsFn =
      reinterpret_cast<glReadPixelsProc>(GetGLProcAddress("glReadPixels"));
  fn.glSampleCoverageFn = reinterpret_cast<glSampleCoverageProc>(
      GetGLProcAddress("glSampleCoverage"));
  fn.glScissorFn =
      reinterpret_cast<glScissorProc>(GetGLProcAddress("glScissor"));
  fn.glShaderSourceFn =
      reinterpret_cast<glShaderSourceProc>(GetGLProcAddress("glShaderSource"));
  fn.glStencilFuncFn =
      reinterpret_cast<glStencilFuncProc>(GetGLProcAddress("glStencilFunc"));
  fn.glStencilFuncSeparateFn = reinterpret_cast<glStencilFuncSeparateProc>(
      GetGLProcAddress("glStencilFuncSeparate"));
  fn.glStencilMaskFn =
      reinterpret_cast<glStencilMaskProc>(GetGLProcAddress("glStencilMask"));
  fn.glStencilMaskSeparateFn = reinterpret_cast<glStencilMaskSeparateProc>(
      GetGLProcAddress("glStencilMaskSeparate"));
  fn.glStencilOpFn =
      reinterpret_cast<glStencilOpProc>(GetGLProcAddress("glStencilOp"));
  fn.glStencilOpSeparateFn = reinterpret_cast<glStencilOpSeparateProc>(
      GetGLProcAddress("glStencilOpSeparate"));
  fn.glTexImage2DFn =
      reinterpret_cast<glTexImage2DProc>(GetGLProcAddress("glTexImage2D"));
  fn.glTexParameterfFn = reinterpret_cast<glTexParameterfProc>(
      GetGLProcAddress("glTexParameterf"));
  fn.glTexParameterfvFn = reinterpret_cast<glTexParameterfvProc>(
      GetGLProcAddress("glTexParameterfv"));
  fn.glTexParameteriFn = reinterpret_cast<glTexParameteriProc>(
      GetGLProcAddress("glTexParameteri"));
  fn.glTexParameterivFn = reinterpret_cast<glTexParameterivProc>(
      GetGLProcAddress("glTexParameteriv"));
  fn.glTexSubImage2DFn = reinterpret_cast<glTexSubImage2DProc>(
      GetGLProcAddress("glTexSubImage2D"));
  fn.glUniform1fFn =
      reinterpret_cast<glUniform1fProc>(GetGLProcAddress("glUniform1f"));
  fn.glUniform1fvFn =
      reinterpret_cast<glUniform1fvProc>(GetGLProcAddress("glUniform1fv"));
  fn.glUniform1iFn =
      reinterpret_cast<glUniform1iProc>(GetGLProcAddress("glUniform1i"));
  fn.glUniform1ivFn =
      reinterpret_cast<glUniform1ivProc>(GetGLProcAddress("glUniform1iv"));
  fn.glUniform2fFn =
      reinterpret_cast<glUniform2fProc>(GetGLProcAddress("glUniform2f"));
  fn.glUniform2fvFn =
      reinterpret_cast<glUniform2fvProc>(GetGLProcAddress("glUniform2fv"));
  fn.glUniform2iFn =
      reinterpret_cast<glUniform2iProc>(GetGLProcAddress("glUniform2i"));
  fn.glUniform2ivFn =
      reinterpret_cast<glUniform2ivProc>(GetGLProcAddress("glUniform2iv"));
  fn.glUniform3fFn =
      reinterpret_cast<glUniform3fProc>(GetGLProcAddress("glUniform3f"));
  fn.glUniform3fvFn =
      reinterpret_cast<glUniform3fvProc>(GetGLProcAddress("glUniform3fv"));
  fn.glUniform3iFn =
      reinterpret_cast<glUniform3iProc>(GetGLProcAddress("glUniform3i"));
  fn.glUniform3ivFn =
      reinterpret_cast<glUniform3ivProc>(GetGLProcAddress("glUniform3iv"));
  fn.glUniform4fFn =
      reinterpret_cast<glUniform4fProc>(GetGLProcAddress("glUniform4f"));
  fn.glUniform4fvFn =
      reinterpret_cast<glUniform4fvProc>(GetGLProcAddress("glUniform4fv"));
  fn.glUniform4iFn =
      reinterpret_cast<glUniform4iProc>(GetGLProcAddress("glUniform4i"));
  fn.glUniform4ivFn =
      reinterpret_cast<glUniform4ivProc>(GetGLProcAddress("glUniform4iv"));
  fn.glUniformMatrix2fvFn = reinterpret_cast<glUniformMatrix2fvProc>(
      GetGLProcAddress("glUniformMatrix2fv"));
  fn.glUniformMatrix3fvFn = reinterpret_cast<glUniformMatrix3fvProc>(
      GetGLProcAddress("glUniformMatrix3fv"));
  fn.glUniformMatrix4fvFn = reinterpret_cast<glUniformMatrix4fvProc>(
      GetGLProcAddress("glUniformMatrix4fv"));
  fn.glUseProgramFn =
      reinterpret_cast<glUseProgramProc>(GetGLProcAddress("glUseProgram"));
  fn.glValidateProgramFn = reinterpret_cast<glValidateProgramProc>(
      GetGLProcAddress("glValidateProgram"));
  fn.glVertexAttrib1fFn = reinterpret_cast<glVertexAttrib1fProc>(
      GetGLProcAddress("glVertexAttrib1f"));
  fn.glVertexAttrib1fvFn = reinterpret_cast<glVertexAttrib1fvProc>(
      GetGLProcAddress("glVertexAttrib1fv"));
  fn.glVertexAttrib2fFn = reinterpret_cast<glVertexAttrib2fProc>(
      GetGLProcAddress("glVertexAttrib2f"));
  fn.glVertexAttrib2fvFn = reinterpret_cast<glVertexAttrib2fvProc>(
      GetGLProcAddress("glVertexAttrib2fv"));
  fn.glVertexAttrib3fFn = reinterpret_cast<glVertexAttrib3fProc>(
      GetGLProcAddress("glVertexAttrib3f"));
  fn.glVertexAttrib3fvFn = reinterpret_cast<glVertexAttrib3fvProc>(
      GetGLProcAddress("glVertexAttrib3fv"));
  fn.glVertexAttrib4fFn = reinterpret_cast<glVertexAttrib4fProc>(
      GetGLProcAddress("glVertexAttrib4f"));
  fn.glVertexAttrib4fvFn = reinterpret_cast<glVertexAttrib4fvProc>(
      GetGLProcAddress("glVertexAttrib4fv"));
  fn.glVertexAttribPointerFn = reinterpret_cast<glVertexAttribPointerProc>(
      GetGLProcAddress("glVertexAttribPointer"));
  fn.glViewportFn =
      reinterpret_cast<glViewportProc>(GetGLProcAddress("glViewport"));
}

void DriverGL::InitializeDynamicBindings(const GLVersionInfo* ver,
                                         const ExtensionSet& extensions) {
  ext.b_GL_ANGLE_framebuffer_blit =
      HasExtension(extensions, "GL_ANGLE_framebuffer_blit");
  ext.b_GL_ANGLE_framebuffer_multisample =
      HasExtension(extensions, "GL_ANGLE_framebuffer_multisample");
  ext.b_GL_ANGLE_instanced_arrays =
      HasExtension(extensions, "GL_ANGLE_instanced_arrays");
  ext.b_GL_ANGLE_request_extension =
      HasExtension(extensions, "GL_ANGLE_request_extension");
  ext.b_GL_ANGLE_robust_client_memory =
      HasExtension(extensions, "GL_ANGLE_robust_client_memory");
  ext.b_GL_ANGLE_translated_shader_source =
      HasExtension(extensions, "GL_ANGLE_translated_shader_source");
  ext.b_GL_APPLE_fence = HasExtension(extensions, "GL_APPLE_fence");
  ext.b_GL_APPLE_vertex_array_object =
      HasExtension(extensions, "GL_APPLE_vertex_array_object");
  ext.b_GL_ARB_blend_func_extended =
      HasExtension(extensions, "GL_ARB_blend_func_extended");
  ext.b_GL_ARB_draw_buffers = HasExtension(extensions, "GL_ARB_draw_buffers");
  ext.b_GL_ARB_draw_instanced =
      HasExtension(extensions, "GL_ARB_draw_instanced");
  ext.b_GL_ARB_framebuffer_object =
      HasExtension(extensions, "GL_ARB_framebuffer_object");
  ext.b_GL_ARB_get_program_binary =
      HasExtension(extensions, "GL_ARB_get_program_binary");
  ext.b_GL_ARB_instanced_arrays =
      HasExtension(extensions, "GL_ARB_instanced_arrays");
  ext.b_GL_ARB_internalformat_query =
      HasExtension(extensions, "GL_ARB_internalformat_query");
  ext.b_GL_ARB_map_buffer_range =
      HasExtension(extensions, "GL_ARB_map_buffer_range");
  ext.b_GL_ARB_occlusion_query =
      HasExtension(extensions, "GL_ARB_occlusion_query");
  ext.b_GL_ARB_program_interface_query =
      HasExtension(extensions, "GL_ARB_program_interface_query");
  ext.b_GL_ARB_robustness = HasExtension(extensions, "GL_ARB_robustness");
  ext.b_GL_ARB_sampler_objects =
      HasExtension(extensions, "GL_ARB_sampler_objects");
  ext.b_GL_ARB_shader_image_load_store =
      HasExtension(extensions, "GL_ARB_shader_image_load_store");
  ext.b_GL_ARB_sync = HasExtension(extensions, "GL_ARB_sync");
  ext.b_GL_ARB_texture_multisample =
      HasExtension(extensions, "GL_ARB_texture_multisample");
  ext.b_GL_ARB_texture_storage =
      HasExtension(extensions, "GL_ARB_texture_storage");
  ext.b_GL_ARB_timer_query = HasExtension(extensions, "GL_ARB_timer_query");
  ext.b_GL_ARB_transform_feedback2 =
      HasExtension(extensions, "GL_ARB_transform_feedback2");
  ext.b_GL_ARB_vertex_array_object =
      HasExtension(extensions, "GL_ARB_vertex_array_object");
  ext.b_GL_CHROMIUM_bind_uniform_location =
      HasExtension(extensions, "GL_CHROMIUM_bind_uniform_location");
  ext.b_GL_CHROMIUM_compressed_copy_texture =
      HasExtension(extensions, "GL_CHROMIUM_compressed_copy_texture");
  ext.b_GL_CHROMIUM_copy_compressed_texture =
      HasExtension(extensions, "GL_CHROMIUM_copy_compressed_texture");
  ext.b_GL_CHROMIUM_copy_texture =
      HasExtension(extensions, "GL_CHROMIUM_copy_texture");
  ext.b_GL_CHROMIUM_gles_depth_binding_hack =
      HasExtension(extensions, "GL_CHROMIUM_gles_depth_binding_hack");
  ext.b_GL_CHROMIUM_glgetstringi_hack =
      HasExtension(extensions, "GL_CHROMIUM_glgetstringi_hack");
  ext.b_GL_EXT_blend_func_extended =
      HasExtension(extensions, "GL_EXT_blend_func_extended");
  ext.b_GL_EXT_debug_marker = HasExtension(extensions, "GL_EXT_debug_marker");
  ext.b_GL_EXT_direct_state_access =
      HasExtension(extensions, "GL_EXT_direct_state_access");
  ext.b_GL_EXT_discard_framebuffer =
      HasExtension(extensions, "GL_EXT_discard_framebuffer");
  ext.b_GL_EXT_disjoint_timer_query =
      HasExtension(extensions, "GL_EXT_disjoint_timer_query");
  ext.b_GL_EXT_draw_buffers = HasExtension(extensions, "GL_EXT_draw_buffers");
  ext.b_GL_EXT_framebuffer_blit =
      HasExtension(extensions, "GL_EXT_framebuffer_blit");
  ext.b_GL_EXT_framebuffer_multisample =
      HasExtension(extensions, "GL_EXT_framebuffer_multisample");
  ext.b_GL_EXT_framebuffer_object =
      HasExtension(extensions, "GL_EXT_framebuffer_object");
  ext.b_GL_EXT_gpu_shader4 = HasExtension(extensions, "GL_EXT_gpu_shader4");
  ext.b_GL_EXT_instanced_arrays =
      HasExtension(extensions, "GL_EXT_instanced_arrays");
  ext.b_GL_EXT_map_buffer_range =
      HasExtension(extensions, "GL_EXT_map_buffer_range");
  ext.b_GL_EXT_multisampled_render_to_texture =
      HasExtension(extensions, "GL_EXT_multisampled_render_to_texture");
  ext.b_GL_EXT_occlusion_query_boolean =
      HasExtension(extensions, "GL_EXT_occlusion_query_boolean");
  ext.b_GL_EXT_robustness = HasExtension(extensions, "GL_EXT_robustness");
  ext.b_GL_EXT_shader_image_load_store =
      HasExtension(extensions, "GL_EXT_shader_image_load_store");
  ext.b_GL_EXT_texture_buffer =
      HasExtension(extensions, "GL_EXT_texture_buffer");
  ext.b_GL_EXT_texture_buffer_object =
      HasExtension(extensions, "GL_EXT_texture_buffer_object");
  ext.b_GL_EXT_texture_storage =
      HasExtension(extensions, "GL_EXT_texture_storage");
  ext.b_GL_EXT_timer_query = HasExtension(extensions, "GL_EXT_timer_query");
  ext.b_GL_EXT_transform_feedback =
      HasExtension(extensions, "GL_EXT_transform_feedback");
  ext.b_GL_EXT_unpack_subimage =
      HasExtension(extensions, "GL_EXT_unpack_subimage");
  ext.b_GL_EXT_window_rectangles =
      HasExtension(extensions, "GL_EXT_window_rectangles");
  ext.b_GL_IMG_multisampled_render_to_texture =
      HasExtension(extensions, "GL_IMG_multisampled_render_to_texture");
  ext.b_GL_INTEL_framebuffer_CMAA =
      HasExtension(extensions, "GL_INTEL_framebuffer_CMAA");
  ext.b_GL_KHR_blend_equation_advanced =
      HasExtension(extensions, "GL_KHR_blend_equation_advanced");
  ext.b_GL_KHR_debug = HasExtension(extensions, "GL_KHR_debug");
  ext.b_GL_KHR_robustness = HasExtension(extensions, "GL_KHR_robustness");
  ext.b_GL_NV_blend_equation_advanced =
      HasExtension(extensions, "GL_NV_blend_equation_advanced");
  ext.b_GL_NV_fence = HasExtension(extensions, "GL_NV_fence");
  ext.b_GL_NV_framebuffer_mixed_samples =
      HasExtension(extensions, "GL_NV_framebuffer_mixed_samples");
  ext.b_GL_NV_path_rendering = HasExtension(extensions, "GL_NV_path_rendering");
  ext.b_GL_OES_EGL_image = HasExtension(extensions, "GL_OES_EGL_image");
  ext.b_GL_OES_get_program_binary =
      HasExtension(extensions, "GL_OES_get_program_binary");
  ext.b_GL_OES_mapbuffer = HasExtension(extensions, "GL_OES_mapbuffer");
  ext.b_GL_OES_texture_buffer =
      HasExtension(extensions, "GL_OES_texture_buffer");
  ext.b_GL_OES_vertex_array_object =
      HasExtension(extensions, "GL_OES_vertex_array_object");

  if (ext.b_GL_INTEL_framebuffer_CMAA) {
    fn.glApplyFramebufferAttachmentCMAAINTELFn =
        reinterpret_cast<glApplyFramebufferAttachmentCMAAINTELProc>(
            GetGLProcAddress("glApplyFramebufferAttachmentCMAAINTEL"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBeginQueryFn =
        reinterpret_cast<glBeginQueryProc>(GetGLProcAddress("glBeginQuery"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glBeginQueryFn =
        reinterpret_cast<glBeginQueryProc>(GetGLProcAddress("glBeginQueryARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glBeginQueryFn =
        reinterpret_cast<glBeginQueryProc>(GetGLProcAddress("glBeginQueryEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBeginTransformFeedbackFn =
        reinterpret_cast<glBeginTransformFeedbackProc>(
            GetGLProcAddress("glBeginTransformFeedback"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glBeginTransformFeedbackFn =
        reinterpret_cast<glBeginTransformFeedbackProc>(
            GetGLProcAddress("glBeginTransformFeedbackEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBindBufferBaseFn = reinterpret_cast<glBindBufferBaseProc>(
        GetGLProcAddress("glBindBufferBase"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glBindBufferBaseFn = reinterpret_cast<glBindBufferBaseProc>(
        GetGLProcAddress("glBindBufferBaseEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBindBufferRangeFn = reinterpret_cast<glBindBufferRangeProc>(
        GetGLProcAddress("glBindBufferRange"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glBindBufferRangeFn = reinterpret_cast<glBindBufferRangeProc>(
        GetGLProcAddress("glBindBufferRangeEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ext.b_GL_ARB_blend_func_extended) {
    fn.glBindFragDataLocationFn = reinterpret_cast<glBindFragDataLocationProc>(
        GetGLProcAddress("glBindFragDataLocation"));
  } else if (ext.b_GL_EXT_gpu_shader4 || ext.b_GL_EXT_blend_func_extended) {
    fn.glBindFragDataLocationFn = reinterpret_cast<glBindFragDataLocationProc>(
        GetGLProcAddress("glBindFragDataLocationEXT"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_blend_func_extended) {
    fn.glBindFragDataLocationIndexedFn =
        reinterpret_cast<glBindFragDataLocationIndexedProc>(
            GetGLProcAddress("glBindFragDataLocationIndexed"));
  } else if (ext.b_GL_EXT_blend_func_extended) {
    fn.glBindFragDataLocationIndexedFn =
        reinterpret_cast<glBindFragDataLocationIndexedProc>(
            GetGLProcAddress("glBindFragDataLocationIndexedEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glBindFramebufferEXTFn = reinterpret_cast<glBindFramebufferEXTProc>(
        GetGLProcAddress("glBindFramebuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glBindFramebufferEXTFn = reinterpret_cast<glBindFramebufferEXTProc>(
        GetGLProcAddress("glBindFramebufferEXT"));
  }

  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_shader_image_load_store) {
    fn.glBindImageTextureEXTFn = reinterpret_cast<glBindImageTextureEXTProc>(
        GetGLProcAddress("glBindImageTexture"));
  } else if (ext.b_GL_EXT_shader_image_load_store) {
    fn.glBindImageTextureEXTFn = reinterpret_cast<glBindImageTextureEXTProc>(
        GetGLProcAddress("glBindImageTextureEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glBindRenderbufferEXTFn = reinterpret_cast<glBindRenderbufferEXTProc>(
        GetGLProcAddress("glBindRenderbuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glBindRenderbufferEXTFn = reinterpret_cast<glBindRenderbufferEXTProc>(
        GetGLProcAddress("glBindRenderbufferEXT"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glBindSamplerFn =
        reinterpret_cast<glBindSamplerProc>(GetGLProcAddress("glBindSampler"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glBindTransformFeedbackFn =
        reinterpret_cast<glBindTransformFeedbackProc>(
            GetGLProcAddress("glBindTransformFeedback"));
  }

  if (ext.b_GL_CHROMIUM_bind_uniform_location) {
    fn.glBindUniformLocationCHROMIUMFn =
        reinterpret_cast<glBindUniformLocationCHROMIUMProc>(
            GetGLProcAddress("glBindUniformLocationCHROMIUM"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_vertex_array_object) {
    fn.glBindVertexArrayOESFn = reinterpret_cast<glBindVertexArrayOESProc>(
        GetGLProcAddress("glBindVertexArray"));
  } else if (ext.b_GL_OES_vertex_array_object) {
    fn.glBindVertexArrayOESFn = reinterpret_cast<glBindVertexArrayOESProc>(
        GetGLProcAddress("glBindVertexArrayOES"));
  } else if (ext.b_GL_APPLE_vertex_array_object) {
    fn.glBindVertexArrayOESFn = reinterpret_cast<glBindVertexArrayOESProc>(
        GetGLProcAddress("glBindVertexArrayAPPLE"));
  }

  if (ext.b_GL_NV_blend_equation_advanced) {
    fn.glBlendBarrierKHRFn = reinterpret_cast<glBlendBarrierKHRProc>(
        GetGLProcAddress("glBlendBarrierNV"));
  } else if (ext.b_GL_KHR_blend_equation_advanced) {
    fn.glBlendBarrierKHRFn = reinterpret_cast<glBlendBarrierKHRProc>(
        GetGLProcAddress("glBlendBarrierKHR"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_framebuffer_object) {
    fn.glBlitFramebufferFn = reinterpret_cast<glBlitFramebufferProc>(
        GetGLProcAddress("glBlitFramebuffer"));
  } else if (ext.b_GL_ANGLE_framebuffer_blit) {
    fn.glBlitFramebufferFn = reinterpret_cast<glBlitFramebufferProc>(
        GetGLProcAddress("glBlitFramebufferANGLE"));
  } else if (ext.b_GL_EXT_framebuffer_blit) {
    fn.glBlitFramebufferFn = reinterpret_cast<glBlitFramebufferProc>(
        GetGLProcAddress("glBlitFramebufferEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glCheckFramebufferStatusEXTFn =
        reinterpret_cast<glCheckFramebufferStatusEXTProc>(
            GetGLProcAddress("glCheckFramebufferStatus"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glCheckFramebufferStatusEXTFn =
        reinterpret_cast<glCheckFramebufferStatusEXTProc>(
            GetGLProcAddress("glCheckFramebufferStatusEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glClearBufferfiFn = reinterpret_cast<glClearBufferfiProc>(
        GetGLProcAddress("glClearBufferfi"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glClearBufferfvFn = reinterpret_cast<glClearBufferfvProc>(
        GetGLProcAddress("glClearBufferfv"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glClearBufferivFn = reinterpret_cast<glClearBufferivProc>(
        GetGLProcAddress("glClearBufferiv"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glClearBufferuivFn = reinterpret_cast<glClearBufferuivProc>(
        GetGLProcAddress("glClearBufferuiv"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glClearDepthfFn =
        reinterpret_cast<glClearDepthfProc>(GetGLProcAddress("glClearDepthf"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glClientWaitSyncFn = reinterpret_cast<glClientWaitSyncProc>(
        GetGLProcAddress("glClientWaitSync"));
  }

  if (ext.b_GL_CHROMIUM_copy_compressed_texture ||
      ext.b_GL_CHROMIUM_compressed_copy_texture) {
    fn.glCompressedCopyTextureCHROMIUMFn =
        reinterpret_cast<glCompressedCopyTextureCHROMIUMProc>(
            GetGLProcAddress("glCompressedCopyTextureCHROMIUM"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glCompressedTexImage2DRobustANGLEFn =
        reinterpret_cast<glCompressedTexImage2DRobustANGLEProc>(
            GetGLProcAddress("glCompressedTexImage2DRobustANGLE"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glCompressedTexImage3DFn = reinterpret_cast<glCompressedTexImage3DProc>(
        GetGLProcAddress("glCompressedTexImage3D"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glCompressedTexImage3DRobustANGLEFn =
        reinterpret_cast<glCompressedTexImage3DRobustANGLEProc>(
            GetGLProcAddress("glCompressedTexImage3DRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glCompressedTexSubImage2DRobustANGLEFn =
        reinterpret_cast<glCompressedTexSubImage2DRobustANGLEProc>(
            GetGLProcAddress("glCompressedTexSubImage2DRobustANGLE"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glCompressedTexSubImage3DFn =
        reinterpret_cast<glCompressedTexSubImage3DProc>(
            GetGLProcAddress("glCompressedTexSubImage3D"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glCompressedTexSubImage3DRobustANGLEFn =
        reinterpret_cast<glCompressedTexSubImage3DRobustANGLEProc>(
            GetGLProcAddress("glCompressedTexSubImage3DRobustANGLE"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glCopyBufferSubDataFn = reinterpret_cast<glCopyBufferSubDataProc>(
        GetGLProcAddress("glCopyBufferSubData"));
  }

  if (ext.b_GL_CHROMIUM_copy_texture) {
    fn.glCopySubTextureCHROMIUMFn =
        reinterpret_cast<glCopySubTextureCHROMIUMProc>(
            GetGLProcAddress("glCopySubTextureCHROMIUM"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glCopyTexSubImage3DFn = reinterpret_cast<glCopyTexSubImage3DProc>(
        GetGLProcAddress("glCopyTexSubImage3D"));
  }

  if (ext.b_GL_CHROMIUM_copy_texture) {
    fn.glCopyTextureCHROMIUMFn = reinterpret_cast<glCopyTextureCHROMIUMProc>(
        GetGLProcAddress("glCopyTextureCHROMIUM"));
  }

  if (ext.b_GL_NV_framebuffer_mixed_samples) {
    fn.glCoverageModulationNVFn = reinterpret_cast<glCoverageModulationNVProc>(
        GetGLProcAddress("glCoverageModulationNV"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glCoverFillPathInstancedNVFn =
        reinterpret_cast<glCoverFillPathInstancedNVProc>(
            GetGLProcAddress("glCoverFillPathInstancedNV"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glCoverFillPathNVFn = reinterpret_cast<glCoverFillPathNVProc>(
        GetGLProcAddress("glCoverFillPathNV"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glCoverStrokePathInstancedNVFn =
        reinterpret_cast<glCoverStrokePathInstancedNVProc>(
            GetGLProcAddress("glCoverStrokePathInstancedNV"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glCoverStrokePathNVFn = reinterpret_cast<glCoverStrokePathNVProc>(
        GetGLProcAddress("glCoverStrokePathNV"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glDebugMessageCallbackFn = reinterpret_cast<glDebugMessageCallbackProc>(
        GetGLProcAddress("glDebugMessageCallback"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glDebugMessageCallbackFn = reinterpret_cast<glDebugMessageCallbackProc>(
        GetGLProcAddress("glDebugMessageCallbackKHR"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glDebugMessageControlFn = reinterpret_cast<glDebugMessageControlProc>(
        GetGLProcAddress("glDebugMessageControl"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glDebugMessageControlFn = reinterpret_cast<glDebugMessageControlProc>(
        GetGLProcAddress("glDebugMessageControlKHR"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glDebugMessageInsertFn = reinterpret_cast<glDebugMessageInsertProc>(
        GetGLProcAddress("glDebugMessageInsert"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glDebugMessageInsertFn = reinterpret_cast<glDebugMessageInsertProc>(
        GetGLProcAddress("glDebugMessageInsertKHR"));
  }

  if (ext.b_GL_APPLE_fence) {
    fn.glDeleteFencesAPPLEFn = reinterpret_cast<glDeleteFencesAPPLEProc>(
        GetGLProcAddress("glDeleteFencesAPPLE"));
  }

  if (ext.b_GL_NV_fence) {
    fn.glDeleteFencesNVFn = reinterpret_cast<glDeleteFencesNVProc>(
        GetGLProcAddress("glDeleteFencesNV"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glDeleteFramebuffersEXTFn =
        reinterpret_cast<glDeleteFramebuffersEXTProc>(
            GetGLProcAddress("glDeleteFramebuffers"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glDeleteFramebuffersEXTFn =
        reinterpret_cast<glDeleteFramebuffersEXTProc>(
            GetGLProcAddress("glDeleteFramebuffersEXT"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glDeletePathsNVFn = reinterpret_cast<glDeletePathsNVProc>(
        GetGLProcAddress("glDeletePathsNV"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glDeleteQueriesFn = reinterpret_cast<glDeleteQueriesProc>(
        GetGLProcAddress("glDeleteQueries"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glDeleteQueriesFn = reinterpret_cast<glDeleteQueriesProc>(
        GetGLProcAddress("glDeleteQueriesARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glDeleteQueriesFn = reinterpret_cast<glDeleteQueriesProc>(
        GetGLProcAddress("glDeleteQueriesEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glDeleteRenderbuffersEXTFn =
        reinterpret_cast<glDeleteRenderbuffersEXTProc>(
            GetGLProcAddress("glDeleteRenderbuffers"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glDeleteRenderbuffersEXTFn =
        reinterpret_cast<glDeleteRenderbuffersEXTProc>(
            GetGLProcAddress("glDeleteRenderbuffersEXT"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glDeleteSamplersFn = reinterpret_cast<glDeleteSamplersProc>(
        GetGLProcAddress("glDeleteSamplers"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glDeleteSyncFn =
        reinterpret_cast<glDeleteSyncProc>(GetGLProcAddress("glDeleteSync"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glDeleteTransformFeedbacksFn =
        reinterpret_cast<glDeleteTransformFeedbacksProc>(
            GetGLProcAddress("glDeleteTransformFeedbacks"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_vertex_array_object) {
    fn.glDeleteVertexArraysOESFn =
        reinterpret_cast<glDeleteVertexArraysOESProc>(
            GetGLProcAddress("glDeleteVertexArrays"));
  } else if (ext.b_GL_OES_vertex_array_object) {
    fn.glDeleteVertexArraysOESFn =
        reinterpret_cast<glDeleteVertexArraysOESProc>(
            GetGLProcAddress("glDeleteVertexArraysOES"));
  } else if (ext.b_GL_APPLE_vertex_array_object) {
    fn.glDeleteVertexArraysOESFn =
        reinterpret_cast<glDeleteVertexArraysOESProc>(
            GetGLProcAddress("glDeleteVertexArraysAPPLE"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glDepthRangefFn =
        reinterpret_cast<glDepthRangefProc>(GetGLProcAddress("glDepthRangef"));
  }

  if (ext.b_GL_EXT_discard_framebuffer) {
    fn.glDiscardFramebufferEXTFn =
        reinterpret_cast<glDiscardFramebufferEXTProc>(
            GetGLProcAddress("glDiscardFramebufferEXT"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glDrawArraysInstancedANGLEFn =
        reinterpret_cast<glDrawArraysInstancedANGLEProc>(
            GetGLProcAddress("glDrawArraysInstanced"));
  } else if (ext.b_GL_ARB_draw_instanced) {
    fn.glDrawArraysInstancedANGLEFn =
        reinterpret_cast<glDrawArraysInstancedANGLEProc>(
            GetGLProcAddress("glDrawArraysInstancedARB"));
  } else if (ext.b_GL_ANGLE_instanced_arrays) {
    fn.glDrawArraysInstancedANGLEFn =
        reinterpret_cast<glDrawArraysInstancedANGLEProc>(
            GetGLProcAddress("glDrawArraysInstancedANGLE"));
  }

  if (!ver->is_es) {
    fn.glDrawBufferFn =
        reinterpret_cast<glDrawBufferProc>(GetGLProcAddress("glDrawBuffer"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glDrawBuffersARBFn = reinterpret_cast<glDrawBuffersARBProc>(
        GetGLProcAddress("glDrawBuffers"));
  } else if (ext.b_GL_ARB_draw_buffers) {
    fn.glDrawBuffersARBFn = reinterpret_cast<glDrawBuffersARBProc>(
        GetGLProcAddress("glDrawBuffersARB"));
  } else if (ext.b_GL_EXT_draw_buffers) {
    fn.glDrawBuffersARBFn = reinterpret_cast<glDrawBuffersARBProc>(
        GetGLProcAddress("glDrawBuffersEXT"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glDrawElementsInstancedANGLEFn =
        reinterpret_cast<glDrawElementsInstancedANGLEProc>(
            GetGLProcAddress("glDrawElementsInstanced"));
  } else if (ext.b_GL_ARB_draw_instanced) {
    fn.glDrawElementsInstancedANGLEFn =
        reinterpret_cast<glDrawElementsInstancedANGLEProc>(
            GetGLProcAddress("glDrawElementsInstancedARB"));
  } else if (ext.b_GL_ANGLE_instanced_arrays) {
    fn.glDrawElementsInstancedANGLEFn =
        reinterpret_cast<glDrawElementsInstancedANGLEProc>(
            GetGLProcAddress("glDrawElementsInstancedANGLE"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glDrawRangeElementsFn = reinterpret_cast<glDrawRangeElementsProc>(
        GetGLProcAddress("glDrawRangeElements"));
  }

  if (ext.b_GL_OES_EGL_image) {
    fn.glEGLImageTargetRenderbufferStorageOESFn =
        reinterpret_cast<glEGLImageTargetRenderbufferStorageOESProc>(
            GetGLProcAddress("glEGLImageTargetRenderbufferStorageOES"));
  }

  if (ext.b_GL_OES_EGL_image) {
    fn.glEGLImageTargetTexture2DOESFn =
        reinterpret_cast<glEGLImageTargetTexture2DOESProc>(
            GetGLProcAddress("glEGLImageTargetTexture2DOES"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glEndQueryFn =
        reinterpret_cast<glEndQueryProc>(GetGLProcAddress("glEndQuery"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glEndQueryFn =
        reinterpret_cast<glEndQueryProc>(GetGLProcAddress("glEndQueryARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glEndQueryFn =
        reinterpret_cast<glEndQueryProc>(GetGLProcAddress("glEndQueryEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glEndTransformFeedbackFn = reinterpret_cast<glEndTransformFeedbackProc>(
        GetGLProcAddress("glEndTransformFeedback"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glEndTransformFeedbackFn = reinterpret_cast<glEndTransformFeedbackProc>(
        GetGLProcAddress("glEndTransformFeedbackEXT"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glFenceSyncFn =
        reinterpret_cast<glFenceSyncProc>(GetGLProcAddress("glFenceSync"));
  }

  if (ext.b_GL_APPLE_fence) {
    fn.glFinishFenceAPPLEFn = reinterpret_cast<glFinishFenceAPPLEProc>(
        GetGLProcAddress("glFinishFenceAPPLE"));
  }

  if (ext.b_GL_NV_fence) {
    fn.glFinishFenceNVFn = reinterpret_cast<glFinishFenceNVProc>(
        GetGLProcAddress("glFinishFenceNV"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_map_buffer_range) {
    fn.glFlushMappedBufferRangeFn =
        reinterpret_cast<glFlushMappedBufferRangeProc>(
            GetGLProcAddress("glFlushMappedBufferRange"));
  } else if (ext.b_GL_EXT_map_buffer_range) {
    fn.glFlushMappedBufferRangeFn =
        reinterpret_cast<glFlushMappedBufferRangeProc>(
            GetGLProcAddress("glFlushMappedBufferRangeEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glFramebufferRenderbufferEXTFn =
        reinterpret_cast<glFramebufferRenderbufferEXTProc>(
            GetGLProcAddress("glFramebufferRenderbuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glFramebufferRenderbufferEXTFn =
        reinterpret_cast<glFramebufferRenderbufferEXTProc>(
            GetGLProcAddress("glFramebufferRenderbufferEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glFramebufferTexture2DEXTFn =
        reinterpret_cast<glFramebufferTexture2DEXTProc>(
            GetGLProcAddress("glFramebufferTexture2D"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glFramebufferTexture2DEXTFn =
        reinterpret_cast<glFramebufferTexture2DEXTProc>(
            GetGLProcAddress("glFramebufferTexture2DEXT"));
  }

  if (ext.b_GL_EXT_multisampled_render_to_texture) {
    fn.glFramebufferTexture2DMultisampleEXTFn =
        reinterpret_cast<glFramebufferTexture2DMultisampleEXTProc>(
            GetGLProcAddress("glFramebufferTexture2DMultisampleEXT"));
  } else if (ext.b_GL_IMG_multisampled_render_to_texture) {
    fn.glFramebufferTexture2DMultisampleEXTFn =
        reinterpret_cast<glFramebufferTexture2DMultisampleEXTProc>(
            GetGLProcAddress("glFramebufferTexture2DMultisampleIMG"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glFramebufferTextureLayerFn =
        reinterpret_cast<glFramebufferTextureLayerProc>(
            GetGLProcAddress("glFramebufferTextureLayer"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGenerateMipmapEXTFn = reinterpret_cast<glGenerateMipmapEXTProc>(
        GetGLProcAddress("glGenerateMipmap"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGenerateMipmapEXTFn = reinterpret_cast<glGenerateMipmapEXTProc>(
        GetGLProcAddress("glGenerateMipmapEXT"));
  }

  if (ext.b_GL_APPLE_fence) {
    fn.glGenFencesAPPLEFn = reinterpret_cast<glGenFencesAPPLEProc>(
        GetGLProcAddress("glGenFencesAPPLE"));
  }

  if (ext.b_GL_NV_fence) {
    fn.glGenFencesNVFn =
        reinterpret_cast<glGenFencesNVProc>(GetGLProcAddress("glGenFencesNV"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGenFramebuffersEXTFn = reinterpret_cast<glGenFramebuffersEXTProc>(
        GetGLProcAddress("glGenFramebuffers"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGenFramebuffersEXTFn = reinterpret_cast<glGenFramebuffersEXTProc>(
        GetGLProcAddress("glGenFramebuffersEXT"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glGenPathsNVFn =
        reinterpret_cast<glGenPathsNVProc>(GetGLProcAddress("glGenPathsNV"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGenQueriesFn =
        reinterpret_cast<glGenQueriesProc>(GetGLProcAddress("glGenQueries"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glGenQueriesFn =
        reinterpret_cast<glGenQueriesProc>(GetGLProcAddress("glGenQueriesARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glGenQueriesFn =
        reinterpret_cast<glGenQueriesProc>(GetGLProcAddress("glGenQueriesEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGenRenderbuffersEXTFn = reinterpret_cast<glGenRenderbuffersEXTProc>(
        GetGLProcAddress("glGenRenderbuffers"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGenRenderbuffersEXTFn = reinterpret_cast<glGenRenderbuffersEXTProc>(
        GetGLProcAddress("glGenRenderbuffersEXT"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glGenSamplersFn =
        reinterpret_cast<glGenSamplersProc>(GetGLProcAddress("glGenSamplers"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glGenTransformFeedbacksFn =
        reinterpret_cast<glGenTransformFeedbacksProc>(
            GetGLProcAddress("glGenTransformFeedbacks"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_vertex_array_object) {
    fn.glGenVertexArraysOESFn = reinterpret_cast<glGenVertexArraysOESProc>(
        GetGLProcAddress("glGenVertexArrays"));
  } else if (ext.b_GL_OES_vertex_array_object) {
    fn.glGenVertexArraysOESFn = reinterpret_cast<glGenVertexArraysOESProc>(
        GetGLProcAddress("glGenVertexArraysOES"));
  } else if (ext.b_GL_APPLE_vertex_array_object) {
    fn.glGenVertexArraysOESFn = reinterpret_cast<glGenVertexArraysOESProc>(
        GetGLProcAddress("glGenVertexArraysAPPLE"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetActiveUniformBlockivFn =
        reinterpret_cast<glGetActiveUniformBlockivProc>(
            GetGLProcAddress("glGetActiveUniformBlockiv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetActiveUniformBlockivRobustANGLEFn =
        reinterpret_cast<glGetActiveUniformBlockivRobustANGLEProc>(
            GetGLProcAddress("glGetActiveUniformBlockivRobustANGLE"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetActiveUniformBlockNameFn =
        reinterpret_cast<glGetActiveUniformBlockNameProc>(
            GetGLProcAddress("glGetActiveUniformBlockName"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetActiveUniformsivFn = reinterpret_cast<glGetActiveUniformsivProc>(
        GetGLProcAddress("glGetActiveUniformsiv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBooleani_vRobustANGLEFn =
        reinterpret_cast<glGetBooleani_vRobustANGLEProc>(
            GetGLProcAddress("glGetBooleani_vRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBooleanvRobustANGLEFn =
        reinterpret_cast<glGetBooleanvRobustANGLEProc>(
            GetGLProcAddress("glGetBooleanvRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBufferParameteri64vRobustANGLEFn =
        reinterpret_cast<glGetBufferParameteri64vRobustANGLEProc>(
            GetGLProcAddress("glGetBufferParameteri64vRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBufferParameterivRobustANGLEFn =
        reinterpret_cast<glGetBufferParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetBufferParameterivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBufferPointervRobustANGLEFn =
        reinterpret_cast<glGetBufferPointervRobustANGLEProc>(
            GetGLProcAddress("glGetBufferPointervRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetDebugMessageLogFn = reinterpret_cast<glGetDebugMessageLogProc>(
        GetGLProcAddress("glGetDebugMessageLog"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glGetDebugMessageLogFn = reinterpret_cast<glGetDebugMessageLogProc>(
        GetGLProcAddress("glGetDebugMessageLogKHR"));
  }

  if (ext.b_GL_NV_fence) {
    fn.glGetFenceivNVFn = reinterpret_cast<glGetFenceivNVProc>(
        GetGLProcAddress("glGetFenceivNV"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetFloatvRobustANGLEFn = reinterpret_cast<glGetFloatvRobustANGLEProc>(
        GetGLProcAddress("glGetFloatvRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_blend_func_extended) {
    fn.glGetFragDataIndexFn = reinterpret_cast<glGetFragDataIndexProc>(
        GetGLProcAddress("glGetFragDataIndex"));
  } else if (ext.b_GL_EXT_blend_func_extended) {
    fn.glGetFragDataIndexFn = reinterpret_cast<glGetFragDataIndexProc>(
        GetGLProcAddress("glGetFragDataIndexEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetFragDataLocationFn = reinterpret_cast<glGetFragDataLocationProc>(
        GetGLProcAddress("glGetFragDataLocation"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGetFramebufferAttachmentParameterivEXTFn =
        reinterpret_cast<glGetFramebufferAttachmentParameterivEXTProc>(
            GetGLProcAddress("glGetFramebufferAttachmentParameteriv"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGetFramebufferAttachmentParameterivEXTFn =
        reinterpret_cast<glGetFramebufferAttachmentParameterivEXTProc>(
            GetGLProcAddress("glGetFramebufferAttachmentParameterivEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetFramebufferAttachmentParameterivRobustANGLEFn =
        reinterpret_cast<glGetFramebufferAttachmentParameterivRobustANGLEProc>(
            GetGLProcAddress(
                "glGetFramebufferAttachmentParameterivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetFramebufferParameterivRobustANGLEFn =
        reinterpret_cast<glGetFramebufferParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetFramebufferParameterivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 5u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetGraphicsResetStatusARBFn =
        reinterpret_cast<glGetGraphicsResetStatusARBProc>(
            GetGLProcAddress("glGetGraphicsResetStatus"));
  } else if (ext.b_GL_ARB_robustness) {
    fn.glGetGraphicsResetStatusARBFn =
        reinterpret_cast<glGetGraphicsResetStatusARBProc>(
            GetGLProcAddress("glGetGraphicsResetStatusARB"));
  } else if (ext.b_GL_KHR_robustness) {
    fn.glGetGraphicsResetStatusARBFn =
        reinterpret_cast<glGetGraphicsResetStatusARBProc>(
            GetGLProcAddress("glGetGraphicsResetStatusKHR"));
  } else if (ext.b_GL_EXT_robustness) {
    fn.glGetGraphicsResetStatusARBFn =
        reinterpret_cast<glGetGraphicsResetStatusARBProc>(
            GetGLProcAddress("glGetGraphicsResetStatusEXT"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetInteger64i_vFn = reinterpret_cast<glGetInteger64i_vProc>(
        GetGLProcAddress("glGetInteger64i_v"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetInteger64i_vRobustANGLEFn =
        reinterpret_cast<glGetInteger64i_vRobustANGLEProc>(
            GetGLProcAddress("glGetInteger64i_vRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetInteger64vFn = reinterpret_cast<glGetInteger64vProc>(
        GetGLProcAddress("glGetInteger64v"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetInteger64vRobustANGLEFn =
        reinterpret_cast<glGetInteger64vRobustANGLEProc>(
            GetGLProcAddress("glGetInteger64vRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetIntegeri_vFn = reinterpret_cast<glGetIntegeri_vProc>(
        GetGLProcAddress("glGetIntegeri_v"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetIntegeri_vRobustANGLEFn =
        reinterpret_cast<glGetIntegeri_vRobustANGLEProc>(
            GetGLProcAddress("glGetIntegeri_vRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetIntegervRobustANGLEFn =
        reinterpret_cast<glGetIntegervRobustANGLEProc>(
            GetGLProcAddress("glGetIntegervRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_internalformat_query) {
    fn.glGetInternalformativFn = reinterpret_cast<glGetInternalformativProc>(
        GetGLProcAddress("glGetInternalformativ"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetInternalformativRobustANGLEFn =
        reinterpret_cast<glGetInternalformativRobustANGLEProc>(
            GetGLProcAddress("glGetInternalformativRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_texture_multisample) {
    fn.glGetMultisamplefvFn = reinterpret_cast<glGetMultisamplefvProc>(
        GetGLProcAddress("glGetMultisamplefv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetMultisamplefvRobustANGLEFn =
        reinterpret_cast<glGetMultisamplefvRobustANGLEProc>(
            GetGLProcAddress("glGetMultisamplefvRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetnUniformfvRobustANGLEFn =
        reinterpret_cast<glGetnUniformfvRobustANGLEProc>(
            GetGLProcAddress("glGetnUniformfvRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetnUniformivRobustANGLEFn =
        reinterpret_cast<glGetnUniformivRobustANGLEProc>(
            GetGLProcAddress("glGetnUniformivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetnUniformuivRobustANGLEFn =
        reinterpret_cast<glGetnUniformuivRobustANGLEProc>(
            GetGLProcAddress("glGetnUniformuivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetObjectLabelFn = reinterpret_cast<glGetObjectLabelProc>(
        GetGLProcAddress("glGetObjectLabel"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glGetObjectLabelFn = reinterpret_cast<glGetObjectLabelProc>(
        GetGLProcAddress("glGetObjectLabelKHR"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetObjectPtrLabelFn = reinterpret_cast<glGetObjectPtrLabelProc>(
        GetGLProcAddress("glGetObjectPtrLabel"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glGetObjectPtrLabelFn = reinterpret_cast<glGetObjectPtrLabelProc>(
        GetGLProcAddress("glGetObjectPtrLabelKHR"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetPointervFn =
        reinterpret_cast<glGetPointervProc>(GetGLProcAddress("glGetPointerv"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glGetPointervFn = reinterpret_cast<glGetPointervProc>(
        GetGLProcAddress("glGetPointervKHR"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetPointervRobustANGLERobustANGLEFn =
        reinterpret_cast<glGetPointervRobustANGLERobustANGLEProc>(
            GetGLProcAddress("glGetPointervRobustANGLERobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_get_program_binary) {
    fn.glGetProgramBinaryFn = reinterpret_cast<glGetProgramBinaryProc>(
        GetGLProcAddress("glGetProgramBinary"));
  } else if (ext.b_GL_OES_get_program_binary) {
    fn.glGetProgramBinaryFn = reinterpret_cast<glGetProgramBinaryProc>(
        GetGLProcAddress("glGetProgramBinaryOES"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_program_interface_query) {
    fn.glGetProgramInterfaceivFn =
        reinterpret_cast<glGetProgramInterfaceivProc>(
            GetGLProcAddress("glGetProgramInterfaceiv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetProgramInterfaceivRobustANGLEFn =
        reinterpret_cast<glGetProgramInterfaceivRobustANGLEProc>(
            GetGLProcAddress("glGetProgramInterfaceivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetProgramivRobustANGLEFn =
        reinterpret_cast<glGetProgramivRobustANGLEProc>(
            GetGLProcAddress("glGetProgramivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_program_interface_query) {
    fn.glGetProgramResourceivFn = reinterpret_cast<glGetProgramResourceivProc>(
        GetGLProcAddress("glGetProgramResourceiv"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGetProgramResourceLocationFn =
        reinterpret_cast<glGetProgramResourceLocationProc>(
            GetGLProcAddress("glGetProgramResourceLocation"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_program_interface_query) {
    fn.glGetProgramResourceNameFn =
        reinterpret_cast<glGetProgramResourceNameProc>(
            GetGLProcAddress("glGetProgramResourceName"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetQueryivFn =
        reinterpret_cast<glGetQueryivProc>(GetGLProcAddress("glGetQueryiv"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glGetQueryivFn =
        reinterpret_cast<glGetQueryivProc>(GetGLProcAddress("glGetQueryivARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glGetQueryivFn =
        reinterpret_cast<glGetQueryivProc>(GetGLProcAddress("glGetQueryivEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryivRobustANGLEFn =
        reinterpret_cast<glGetQueryivRobustANGLEProc>(
            GetGLProcAddress("glGetQueryivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_timer_query) {
    fn.glGetQueryObjecti64vFn = reinterpret_cast<glGetQueryObjecti64vProc>(
        GetGLProcAddress("glGetQueryObjecti64v"));
  } else if (ext.b_GL_EXT_timer_query || ext.b_GL_EXT_disjoint_timer_query) {
    fn.glGetQueryObjecti64vFn = reinterpret_cast<glGetQueryObjecti64vProc>(
        GetGLProcAddress("glGetQueryObjecti64vEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryObjecti64vRobustANGLEFn =
        reinterpret_cast<glGetQueryObjecti64vRobustANGLEProc>(
            GetGLProcAddress("glGetQueryObjecti64vRobustANGLE"));
  }

  if (!ver->is_es) {
    fn.glGetQueryObjectivFn = reinterpret_cast<glGetQueryObjectivProc>(
        GetGLProcAddress("glGetQueryObjectiv"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glGetQueryObjectivFn = reinterpret_cast<glGetQueryObjectivProc>(
        GetGLProcAddress("glGetQueryObjectivARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query) {
    fn.glGetQueryObjectivFn = reinterpret_cast<glGetQueryObjectivProc>(
        GetGLProcAddress("glGetQueryObjectivEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryObjectivRobustANGLEFn =
        reinterpret_cast<glGetQueryObjectivRobustANGLEProc>(
            GetGLProcAddress("glGetQueryObjectivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_timer_query) {
    fn.glGetQueryObjectui64vFn = reinterpret_cast<glGetQueryObjectui64vProc>(
        GetGLProcAddress("glGetQueryObjectui64v"));
  } else if (ext.b_GL_EXT_timer_query || ext.b_GL_EXT_disjoint_timer_query) {
    fn.glGetQueryObjectui64vFn = reinterpret_cast<glGetQueryObjectui64vProc>(
        GetGLProcAddress("glGetQueryObjectui64vEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryObjectui64vRobustANGLEFn =
        reinterpret_cast<glGetQueryObjectui64vRobustANGLEProc>(
            GetGLProcAddress("glGetQueryObjectui64vRobustANGLE"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetQueryObjectuivFn = reinterpret_cast<glGetQueryObjectuivProc>(
        GetGLProcAddress("glGetQueryObjectuiv"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glGetQueryObjectuivFn = reinterpret_cast<glGetQueryObjectuivProc>(
        GetGLProcAddress("glGetQueryObjectuivARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glGetQueryObjectuivFn = reinterpret_cast<glGetQueryObjectuivProc>(
        GetGLProcAddress("glGetQueryObjectuivEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryObjectuivRobustANGLEFn =
        reinterpret_cast<glGetQueryObjectuivRobustANGLEProc>(
            GetGLProcAddress("glGetQueryObjectuivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGetRenderbufferParameterivEXTFn =
        reinterpret_cast<glGetRenderbufferParameterivEXTProc>(
            GetGLProcAddress("glGetRenderbufferParameteriv"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGetRenderbufferParameterivEXTFn =
        reinterpret_cast<glGetRenderbufferParameterivEXTProc>(
            GetGLProcAddress("glGetRenderbufferParameterivEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetRenderbufferParameterivRobustANGLEFn =
        reinterpret_cast<glGetRenderbufferParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetRenderbufferParameterivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glGetSamplerParameterfvFn =
        reinterpret_cast<glGetSamplerParameterfvProc>(
            GetGLProcAddress("glGetSamplerParameterfv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetSamplerParameterfvRobustANGLEFn =
        reinterpret_cast<glGetSamplerParameterfvRobustANGLEProc>(
            GetGLProcAddress("glGetSamplerParameterfvRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetSamplerParameterIivRobustANGLEFn =
        reinterpret_cast<glGetSamplerParameterIivRobustANGLEProc>(
            GetGLProcAddress("glGetSamplerParameterIivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetSamplerParameterIuivRobustANGLEFn =
        reinterpret_cast<glGetSamplerParameterIuivRobustANGLEProc>(
            GetGLProcAddress("glGetSamplerParameterIuivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glGetSamplerParameterivFn =
        reinterpret_cast<glGetSamplerParameterivProc>(
            GetGLProcAddress("glGetSamplerParameteriv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetSamplerParameterivRobustANGLEFn =
        reinterpret_cast<glGetSamplerParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetSamplerParameterivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetShaderivRobustANGLEFn =
        reinterpret_cast<glGetShaderivRobustANGLEProc>(
            GetGLProcAddress("glGetShaderivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glGetShaderPrecisionFormatFn =
        reinterpret_cast<glGetShaderPrecisionFormatProc>(
            GetGLProcAddress("glGetShaderPrecisionFormat"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glGetSyncivFn =
        reinterpret_cast<glGetSyncivProc>(GetGLProcAddress("glGetSynciv"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGetTexLevelParameterfvFn =
        reinterpret_cast<glGetTexLevelParameterfvProc>(
            GetGLProcAddress("glGetTexLevelParameterfv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexLevelParameterfvRobustANGLEFn =
        reinterpret_cast<glGetTexLevelParameterfvRobustANGLEProc>(
            GetGLProcAddress("glGetTexLevelParameterfvRobustANGLE"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGetTexLevelParameterivFn =
        reinterpret_cast<glGetTexLevelParameterivProc>(
            GetGLProcAddress("glGetTexLevelParameteriv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexLevelParameterivRobustANGLEFn =
        reinterpret_cast<glGetTexLevelParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetTexLevelParameterivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexParameterfvRobustANGLEFn =
        reinterpret_cast<glGetTexParameterfvRobustANGLEProc>(
            GetGLProcAddress("glGetTexParameterfvRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexParameterIivRobustANGLEFn =
        reinterpret_cast<glGetTexParameterIivRobustANGLEProc>(
            GetGLProcAddress("glGetTexParameterIivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexParameterIuivRobustANGLEFn =
        reinterpret_cast<glGetTexParameterIuivRobustANGLEProc>(
            GetGLProcAddress("glGetTexParameterIuivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexParameterivRobustANGLEFn =
        reinterpret_cast<glGetTexParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetTexParameterivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetTransformFeedbackVaryingFn =
        reinterpret_cast<glGetTransformFeedbackVaryingProc>(
            GetGLProcAddress("glGetTransformFeedbackVarying"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glGetTransformFeedbackVaryingFn =
        reinterpret_cast<glGetTransformFeedbackVaryingProc>(
            GetGLProcAddress("glGetTransformFeedbackVaryingEXT"));
  }

  if (ext.b_GL_ANGLE_translated_shader_source) {
    fn.glGetTranslatedShaderSourceANGLEFn =
        reinterpret_cast<glGetTranslatedShaderSourceANGLEProc>(
            GetGLProcAddress("glGetTranslatedShaderSourceANGLE"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetUniformBlockIndexFn = reinterpret_cast<glGetUniformBlockIndexProc>(
        GetGLProcAddress("glGetUniformBlockIndex"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetUniformfvRobustANGLEFn =
        reinterpret_cast<glGetUniformfvRobustANGLEProc>(
            GetGLProcAddress("glGetUniformfvRobustANGLE"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetUniformIndicesFn = reinterpret_cast<glGetUniformIndicesProc>(
        GetGLProcAddress("glGetUniformIndices"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetUniformivRobustANGLEFn =
        reinterpret_cast<glGetUniformivRobustANGLEProc>(
            GetGLProcAddress("glGetUniformivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetUniformuivFn = reinterpret_cast<glGetUniformuivProc>(
        GetGLProcAddress("glGetUniformuiv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetUniformuivRobustANGLEFn =
        reinterpret_cast<glGetUniformuivRobustANGLEProc>(
            GetGLProcAddress("glGetUniformuivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribfvRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribfvRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribfvRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribIivRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribIivRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribIivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribIuivRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribIuivRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribIuivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribivRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribivRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribPointervRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribPointervRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribPointervRobustANGLE"));
  }

  if (ext.b_GL_EXT_debug_marker) {
    fn.glInsertEventMarkerEXTFn = reinterpret_cast<glInsertEventMarkerEXTProc>(
        GetGLProcAddress("glInsertEventMarkerEXT"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glInvalidateFramebufferFn =
        reinterpret_cast<glInvalidateFramebufferProc>(
            GetGLProcAddress("glInvalidateFramebuffer"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glInvalidateSubFramebufferFn =
        reinterpret_cast<glInvalidateSubFramebufferProc>(
            GetGLProcAddress("glInvalidateSubFramebuffer"));
  }

  if (ext.b_GL_APPLE_fence) {
    fn.glIsFenceAPPLEFn = reinterpret_cast<glIsFenceAPPLEProc>(
        GetGLProcAddress("glIsFenceAPPLE"));
  }

  if (ext.b_GL_NV_fence) {
    fn.glIsFenceNVFn =
        reinterpret_cast<glIsFenceNVProc>(GetGLProcAddress("glIsFenceNV"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glIsFramebufferEXTFn = reinterpret_cast<glIsFramebufferEXTProc>(
        GetGLProcAddress("glIsFramebuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glIsFramebufferEXTFn = reinterpret_cast<glIsFramebufferEXTProc>(
        GetGLProcAddress("glIsFramebufferEXT"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glIsPathNVFn =
        reinterpret_cast<glIsPathNVProc>(GetGLProcAddress("glIsPathNV"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glIsQueryFn =
        reinterpret_cast<glIsQueryProc>(GetGLProcAddress("glIsQuery"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glIsQueryFn =
        reinterpret_cast<glIsQueryProc>(GetGLProcAddress("glIsQueryARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glIsQueryFn =
        reinterpret_cast<glIsQueryProc>(GetGLProcAddress("glIsQueryEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glIsRenderbufferEXTFn = reinterpret_cast<glIsRenderbufferEXTProc>(
        GetGLProcAddress("glIsRenderbuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glIsRenderbufferEXTFn = reinterpret_cast<glIsRenderbufferEXTProc>(
        GetGLProcAddress("glIsRenderbufferEXT"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glIsSamplerFn =
        reinterpret_cast<glIsSamplerProc>(GetGLProcAddress("glIsSampler"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glIsSyncFn =
        reinterpret_cast<glIsSyncProc>(GetGLProcAddress("glIsSync"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glIsTransformFeedbackFn = reinterpret_cast<glIsTransformFeedbackProc>(
        GetGLProcAddress("glIsTransformFeedback"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_vertex_array_object) {
    fn.glIsVertexArrayOESFn = reinterpret_cast<glIsVertexArrayOESProc>(
        GetGLProcAddress("glIsVertexArray"));
  } else if (ext.b_GL_OES_vertex_array_object) {
    fn.glIsVertexArrayOESFn = reinterpret_cast<glIsVertexArrayOESProc>(
        GetGLProcAddress("glIsVertexArrayOES"));
  } else if (ext.b_GL_APPLE_vertex_array_object) {
    fn.glIsVertexArrayOESFn = reinterpret_cast<glIsVertexArrayOESProc>(
        GetGLProcAddress("glIsVertexArrayAPPLE"));
  }

  if (!ver->is_es) {
    fn.glMapBufferFn =
        reinterpret_cast<glMapBufferProc>(GetGLProcAddress("glMapBuffer"));
  } else if (ext.b_GL_OES_mapbuffer) {
    fn.glMapBufferFn =
        reinterpret_cast<glMapBufferProc>(GetGLProcAddress("glMapBufferOES"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_map_buffer_range) {
    fn.glMapBufferRangeFn = reinterpret_cast<glMapBufferRangeProc>(
        GetGLProcAddress("glMapBufferRange"));
  } else if (ext.b_GL_EXT_map_buffer_range) {
    fn.glMapBufferRangeFn = reinterpret_cast<glMapBufferRangeProc>(
        GetGLProcAddress("glMapBufferRangeEXT"));
  }

  if (ext.b_GL_EXT_direct_state_access || ext.b_GL_NV_path_rendering) {
    fn.glMatrixLoadfEXTFn = reinterpret_cast<glMatrixLoadfEXTProc>(
        GetGLProcAddress("glMatrixLoadfEXT"));
  }

  if (ext.b_GL_EXT_direct_state_access || ext.b_GL_NV_path_rendering) {
    fn.glMatrixLoadIdentityEXTFn =
        reinterpret_cast<glMatrixLoadIdentityEXTProc>(
            GetGLProcAddress("glMatrixLoadIdentityEXT"));
  }

  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_shader_image_load_store) {
    fn.glMemoryBarrierEXTFn = reinterpret_cast<glMemoryBarrierEXTProc>(
        GetGLProcAddress("glMemoryBarrier"));
  } else if (ext.b_GL_EXT_shader_image_load_store) {
    fn.glMemoryBarrierEXTFn = reinterpret_cast<glMemoryBarrierEXTProc>(
        GetGLProcAddress("glMemoryBarrierEXT"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glObjectLabelFn =
        reinterpret_cast<glObjectLabelProc>(GetGLProcAddress("glObjectLabel"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glObjectLabelFn = reinterpret_cast<glObjectLabelProc>(
        GetGLProcAddress("glObjectLabelKHR"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glObjectPtrLabelFn = reinterpret_cast<glObjectPtrLabelProc>(
        GetGLProcAddress("glObjectPtrLabel"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glObjectPtrLabelFn = reinterpret_cast<glObjectPtrLabelProc>(
        GetGLProcAddress("glObjectPtrLabelKHR"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glPathCommandsNVFn = reinterpret_cast<glPathCommandsNVProc>(
        GetGLProcAddress("glPathCommandsNV"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glPathParameterfNVFn = reinterpret_cast<glPathParameterfNVProc>(
        GetGLProcAddress("glPathParameterfNV"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glPathParameteriNVFn = reinterpret_cast<glPathParameteriNVProc>(
        GetGLProcAddress("glPathParameteriNV"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glPathStencilFuncNVFn = reinterpret_cast<glPathStencilFuncNVProc>(
        GetGLProcAddress("glPathStencilFuncNV"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glPauseTransformFeedbackFn =
        reinterpret_cast<glPauseTransformFeedbackProc>(
            GetGLProcAddress("glPauseTransformFeedback"));
  }

  if (!ver->is_es) {
    fn.glPointParameteriFn = reinterpret_cast<glPointParameteriProc>(
        GetGLProcAddress("glPointParameteri"));
  }

  if (!ver->is_es) {
    fn.glPolygonModeFn =
        reinterpret_cast<glPolygonModeProc>(GetGLProcAddress("glPolygonMode"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glPopDebugGroupFn = reinterpret_cast<glPopDebugGroupProc>(
        GetGLProcAddress("glPopDebugGroup"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glPopDebugGroupFn = reinterpret_cast<glPopDebugGroupProc>(
        GetGLProcAddress("glPopDebugGroupKHR"));
  }

  if (ext.b_GL_EXT_debug_marker) {
    fn.glPopGroupMarkerEXTFn = reinterpret_cast<glPopGroupMarkerEXTProc>(
        GetGLProcAddress("glPopGroupMarkerEXT"));
  }

  if (ver->IsAtLeastGL(3u, 1u)) {
    fn.glPrimitiveRestartIndexFn =
        reinterpret_cast<glPrimitiveRestartIndexProc>(
            GetGLProcAddress("glPrimitiveRestartIndex"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_get_program_binary) {
    fn.glProgramBinaryFn = reinterpret_cast<glProgramBinaryProc>(
        GetGLProcAddress("glProgramBinary"));
  } else if (ext.b_GL_OES_get_program_binary) {
    fn.glProgramBinaryFn = reinterpret_cast<glProgramBinaryProc>(
        GetGLProcAddress("glProgramBinaryOES"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_get_program_binary) {
    fn.glProgramParameteriFn = reinterpret_cast<glProgramParameteriProc>(
        GetGLProcAddress("glProgramParameteri"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glProgramPathFragmentInputGenNVFn =
        reinterpret_cast<glProgramPathFragmentInputGenNVProc>(
            GetGLProcAddress("glProgramPathFragmentInputGenNV"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glPushDebugGroupFn = reinterpret_cast<glPushDebugGroupProc>(
        GetGLProcAddress("glPushDebugGroup"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glPushDebugGroupFn = reinterpret_cast<glPushDebugGroupProc>(
        GetGLProcAddress("glPushDebugGroupKHR"));
  }

  if (ext.b_GL_EXT_debug_marker) {
    fn.glPushGroupMarkerEXTFn = reinterpret_cast<glPushGroupMarkerEXTProc>(
        GetGLProcAddress("glPushGroupMarkerEXT"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_timer_query) {
    fn.glQueryCounterFn = reinterpret_cast<glQueryCounterProc>(
        GetGLProcAddress("glQueryCounter"));
  } else if (ext.b_GL_EXT_disjoint_timer_query) {
    fn.glQueryCounterFn = reinterpret_cast<glQueryCounterProc>(
        GetGLProcAddress("glQueryCounterEXT"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glReadBufferFn =
        reinterpret_cast<glReadBufferProc>(GetGLProcAddress("glReadBuffer"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glReadnPixelsRobustANGLEFn =
        reinterpret_cast<glReadnPixelsRobustANGLEProc>(
            GetGLProcAddress("glReadnPixelsRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glReadPixelsRobustANGLEFn =
        reinterpret_cast<glReadPixelsRobustANGLEProc>(
            GetGLProcAddress("glReadPixelsRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glReleaseShaderCompilerFn =
        reinterpret_cast<glReleaseShaderCompilerProc>(
            GetGLProcAddress("glReleaseShaderCompiler"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glRenderbufferStorageEXTFn =
        reinterpret_cast<glRenderbufferStorageEXTProc>(
            GetGLProcAddress("glRenderbufferStorage"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glRenderbufferStorageEXTFn =
        reinterpret_cast<glRenderbufferStorageEXTProc>(
            GetGLProcAddress("glRenderbufferStorageEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_framebuffer_object) {
    fn.glRenderbufferStorageMultisampleFn =
        reinterpret_cast<glRenderbufferStorageMultisampleProc>(
            GetGLProcAddress("glRenderbufferStorageMultisample"));
  } else if (ext.b_GL_ANGLE_framebuffer_multisample) {
    fn.glRenderbufferStorageMultisampleFn =
        reinterpret_cast<glRenderbufferStorageMultisampleProc>(
            GetGLProcAddress("glRenderbufferStorageMultisampleANGLE"));
  } else if (ext.b_GL_EXT_framebuffer_multisample) {
    fn.glRenderbufferStorageMultisampleFn =
        reinterpret_cast<glRenderbufferStorageMultisampleProc>(
            GetGLProcAddress("glRenderbufferStorageMultisampleEXT"));
  }

  if (ext.b_GL_EXT_multisampled_render_to_texture) {
    fn.glRenderbufferStorageMultisampleEXTFn =
        reinterpret_cast<glRenderbufferStorageMultisampleEXTProc>(
            GetGLProcAddress("glRenderbufferStorageMultisampleEXT"));
  } else if (ext.b_GL_IMG_multisampled_render_to_texture) {
    fn.glRenderbufferStorageMultisampleEXTFn =
        reinterpret_cast<glRenderbufferStorageMultisampleEXTProc>(
            GetGLProcAddress("glRenderbufferStorageMultisampleIMG"));
  }

  if (ext.b_GL_ANGLE_request_extension) {
    fn.glRequestExtensionANGLEFn =
        reinterpret_cast<glRequestExtensionANGLEProc>(
            GetGLProcAddress("glRequestExtensionANGLE"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glResumeTransformFeedbackFn =
        reinterpret_cast<glResumeTransformFeedbackProc>(
            GetGLProcAddress("glResumeTransformFeedback"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glSamplerParameterfFn = reinterpret_cast<glSamplerParameterfProc>(
        GetGLProcAddress("glSamplerParameterf"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glSamplerParameterfvFn = reinterpret_cast<glSamplerParameterfvProc>(
        GetGLProcAddress("glSamplerParameterfv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glSamplerParameterfvRobustANGLEFn =
        reinterpret_cast<glSamplerParameterfvRobustANGLEProc>(
            GetGLProcAddress("glSamplerParameterfvRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glSamplerParameteriFn = reinterpret_cast<glSamplerParameteriProc>(
        GetGLProcAddress("glSamplerParameteri"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glSamplerParameterIivRobustANGLEFn =
        reinterpret_cast<glSamplerParameterIivRobustANGLEProc>(
            GetGLProcAddress("glSamplerParameterIivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glSamplerParameterIuivRobustANGLEFn =
        reinterpret_cast<glSamplerParameterIuivRobustANGLEProc>(
            GetGLProcAddress("glSamplerParameterIuivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glSamplerParameterivFn = reinterpret_cast<glSamplerParameterivProc>(
        GetGLProcAddress("glSamplerParameteriv"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glSamplerParameterivRobustANGLEFn =
        reinterpret_cast<glSamplerParameterivRobustANGLEProc>(
            GetGLProcAddress("glSamplerParameterivRobustANGLE"));
  }

  if (ext.b_GL_APPLE_fence) {
    fn.glSetFenceAPPLEFn = reinterpret_cast<glSetFenceAPPLEProc>(
        GetGLProcAddress("glSetFenceAPPLE"));
  }

  if (ext.b_GL_NV_fence) {
    fn.glSetFenceNVFn =
        reinterpret_cast<glSetFenceNVProc>(GetGLProcAddress("glSetFenceNV"));
  }

  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glShaderBinaryFn = reinterpret_cast<glShaderBinaryProc>(
        GetGLProcAddress("glShaderBinary"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilFillPathInstancedNVFn =
        reinterpret_cast<glStencilFillPathInstancedNVProc>(
            GetGLProcAddress("glStencilFillPathInstancedNV"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilFillPathNVFn = reinterpret_cast<glStencilFillPathNVProc>(
        GetGLProcAddress("glStencilFillPathNV"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilStrokePathInstancedNVFn =
        reinterpret_cast<glStencilStrokePathInstancedNVProc>(
            GetGLProcAddress("glStencilStrokePathInstancedNV"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilStrokePathNVFn = reinterpret_cast<glStencilStrokePathNVProc>(
        GetGLProcAddress("glStencilStrokePathNV"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilThenCoverFillPathInstancedNVFn =
        reinterpret_cast<glStencilThenCoverFillPathInstancedNVProc>(
            GetGLProcAddress("glStencilThenCoverFillPathInstancedNV"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilThenCoverFillPathNVFn =
        reinterpret_cast<glStencilThenCoverFillPathNVProc>(
            GetGLProcAddress("glStencilThenCoverFillPathNV"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilThenCoverStrokePathInstancedNVFn =
        reinterpret_cast<glStencilThenCoverStrokePathInstancedNVProc>(
            GetGLProcAddress("glStencilThenCoverStrokePathInstancedNV"));
  }

  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilThenCoverStrokePathNVFn =
        reinterpret_cast<glStencilThenCoverStrokePathNVProc>(
            GetGLProcAddress("glStencilThenCoverStrokePathNV"));
  }

  if (ext.b_GL_APPLE_fence) {
    fn.glTestFenceAPPLEFn = reinterpret_cast<glTestFenceAPPLEProc>(
        GetGLProcAddress("glTestFenceAPPLE"));
  }

  if (ext.b_GL_NV_fence) {
    fn.glTestFenceNVFn =
        reinterpret_cast<glTestFenceNVProc>(GetGLProcAddress("glTestFenceNV"));
  }

  if (ver->IsAtLeastGLES(3u, 2u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glTexBufferFn =
        reinterpret_cast<glTexBufferProc>(GetGLProcAddress("glTexBuffer"));
  } else if (ext.b_GL_OES_texture_buffer) {
    fn.glTexBufferFn =
        reinterpret_cast<glTexBufferProc>(GetGLProcAddress("glTexBufferOES"));
  } else if (ext.b_GL_EXT_texture_buffer_object ||
             ext.b_GL_EXT_texture_buffer) {
    fn.glTexBufferFn =
        reinterpret_cast<glTexBufferProc>(GetGLProcAddress("glTexBufferEXT"));
  }

  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glTexBufferRangeFn = reinterpret_cast<glTexBufferRangeProc>(
        GetGLProcAddress("glTexBufferRange"));
  } else if (ext.b_GL_OES_texture_buffer) {
    fn.glTexBufferRangeFn = reinterpret_cast<glTexBufferRangeProc>(
        GetGLProcAddress("glTexBufferRangeOES"));
  } else if (ext.b_GL_EXT_texture_buffer) {
    fn.glTexBufferRangeFn = reinterpret_cast<glTexBufferRangeProc>(
        GetGLProcAddress("glTexBufferRangeEXT"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexImage2DRobustANGLEFn =
        reinterpret_cast<glTexImage2DRobustANGLEProc>(
            GetGLProcAddress("glTexImage2DRobustANGLE"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glTexImage3DFn =
        reinterpret_cast<glTexImage3DProc>(GetGLProcAddress("glTexImage3D"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexImage3DRobustANGLEFn =
        reinterpret_cast<glTexImage3DRobustANGLEProc>(
            GetGLProcAddress("glTexImage3DRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexParameterfvRobustANGLEFn =
        reinterpret_cast<glTexParameterfvRobustANGLEProc>(
            GetGLProcAddress("glTexParameterfvRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexParameterIivRobustANGLEFn =
        reinterpret_cast<glTexParameterIivRobustANGLEProc>(
            GetGLProcAddress("glTexParameterIivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexParameterIuivRobustANGLEFn =
        reinterpret_cast<glTexParameterIuivRobustANGLEProc>(
            GetGLProcAddress("glTexParameterIuivRobustANGLE"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexParameterivRobustANGLEFn =
        reinterpret_cast<glTexParameterivRobustANGLEProc>(
            GetGLProcAddress("glTexParameterivRobustANGLE"));
  }

  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_texture_storage) {
    fn.glTexStorage2DEXTFn = reinterpret_cast<glTexStorage2DEXTProc>(
        GetGLProcAddress("glTexStorage2D"));
  } else if (ext.b_GL_EXT_texture_storage) {
    fn.glTexStorage2DEXTFn = reinterpret_cast<glTexStorage2DEXTProc>(
        GetGLProcAddress("glTexStorage2DEXT"));
  }

  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_texture_storage) {
    fn.glTexStorage3DFn = reinterpret_cast<glTexStorage3DProc>(
        GetGLProcAddress("glTexStorage3D"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexSubImage2DRobustANGLEFn =
        reinterpret_cast<glTexSubImage2DRobustANGLEProc>(
            GetGLProcAddress("glTexSubImage2DRobustANGLE"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glTexSubImage3DFn = reinterpret_cast<glTexSubImage3DProc>(
        GetGLProcAddress("glTexSubImage3D"));
  }

  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexSubImage3DRobustANGLEFn =
        reinterpret_cast<glTexSubImage3DRobustANGLEProc>(
            GetGLProcAddress("glTexSubImage3DRobustANGLE"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glTransformFeedbackVaryingsFn =
        reinterpret_cast<glTransformFeedbackVaryingsProc>(
            GetGLProcAddress("glTransformFeedbackVaryings"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glTransformFeedbackVaryingsFn =
        reinterpret_cast<glTransformFeedbackVaryingsProc>(
            GetGLProcAddress("glTransformFeedbackVaryingsEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform1uiFn =
        reinterpret_cast<glUniform1uiProc>(GetGLProcAddress("glUniform1ui"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform1uivFn =
        reinterpret_cast<glUniform1uivProc>(GetGLProcAddress("glUniform1uiv"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform2uiFn =
        reinterpret_cast<glUniform2uiProc>(GetGLProcAddress("glUniform2ui"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform2uivFn =
        reinterpret_cast<glUniform2uivProc>(GetGLProcAddress("glUniform2uiv"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform3uiFn =
        reinterpret_cast<glUniform3uiProc>(GetGLProcAddress("glUniform3ui"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform3uivFn =
        reinterpret_cast<glUniform3uivProc>(GetGLProcAddress("glUniform3uiv"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform4uiFn =
        reinterpret_cast<glUniform4uiProc>(GetGLProcAddress("glUniform4ui"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform4uivFn =
        reinterpret_cast<glUniform4uivProc>(GetGLProcAddress("glUniform4uiv"));
  }

  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glUniformBlockBindingFn = reinterpret_cast<glUniformBlockBindingProc>(
        GetGLProcAddress("glUniformBlockBinding"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix2x3fvFn = reinterpret_cast<glUniformMatrix2x3fvProc>(
        GetGLProcAddress("glUniformMatrix2x3fv"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix2x4fvFn = reinterpret_cast<glUniformMatrix2x4fvProc>(
        GetGLProcAddress("glUniformMatrix2x4fv"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix3x2fvFn = reinterpret_cast<glUniformMatrix3x2fvProc>(
        GetGLProcAddress("glUniformMatrix3x2fv"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix3x4fvFn = reinterpret_cast<glUniformMatrix3x4fvProc>(
        GetGLProcAddress("glUniformMatrix3x4fv"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix4x2fvFn = reinterpret_cast<glUniformMatrix4x2fvProc>(
        GetGLProcAddress("glUniformMatrix4x2fv"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix4x3fvFn = reinterpret_cast<glUniformMatrix4x3fvProc>(
        GetGLProcAddress("glUniformMatrix4x3fv"));
  }

  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUnmapBufferFn =
        reinterpret_cast<glUnmapBufferProc>(GetGLProcAddress("glUnmapBuffer"));
  } else if (ext.b_GL_OES_mapbuffer) {
    fn.glUnmapBufferFn = reinterpret_cast<glUnmapBufferProc>(
        GetGLProcAddress("glUnmapBufferOES"));
  }

  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribDivisorANGLEFn =
        reinterpret_cast<glVertexAttribDivisorANGLEProc>(
            GetGLProcAddress("glVertexAttribDivisor"));
  } else if (ext.b_GL_ARB_instanced_arrays) {
    fn.glVertexAttribDivisorANGLEFn =
        reinterpret_cast<glVertexAttribDivisorANGLEProc>(
            GetGLProcAddress("glVertexAttribDivisorARB"));
  } else if (ext.b_GL_ANGLE_instanced_arrays) {
    fn.glVertexAttribDivisorANGLEFn =
        reinterpret_cast<glVertexAttribDivisorANGLEProc>(
            GetGLProcAddress("glVertexAttribDivisorANGLE"));
  } else if (ext.b_GL_EXT_instanced_arrays) {
    fn.glVertexAttribDivisorANGLEFn =
        reinterpret_cast<glVertexAttribDivisorANGLEProc>(
            GetGLProcAddress("glVertexAttribDivisorEXT"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribI4iFn = reinterpret_cast<glVertexAttribI4iProc>(
        GetGLProcAddress("glVertexAttribI4i"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribI4ivFn = reinterpret_cast<glVertexAttribI4ivProc>(
        GetGLProcAddress("glVertexAttribI4iv"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribI4uiFn = reinterpret_cast<glVertexAttribI4uiProc>(
        GetGLProcAddress("glVertexAttribI4ui"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribI4uivFn = reinterpret_cast<glVertexAttribI4uivProc>(
        GetGLProcAddress("glVertexAttribI4uiv"));
  }

  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribIPointerFn = reinterpret_cast<glVertexAttribIPointerProc>(
        GetGLProcAddress("glVertexAttribIPointer"));
  }

  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glWaitSyncFn =
        reinterpret_cast<glWaitSyncProc>(GetGLProcAddress("glWaitSync"));
  }

  if (ext.b_GL_EXT_window_rectangles) {
    fn.glWindowRectanglesEXTFn = reinterpret_cast<glWindowRectanglesEXTProc>(
        GetGLProcAddress("glWindowRectanglesEXT"));
  }
}

void DriverGL::ClearBindings() {
  memset(this, 0, sizeof(*this));
}

DISABLE_CFI_ICALL
void GLApiBase::glActiveTextureFn(GLenum texture) {
  driver_->fn.glActiveTextureFn(texture);
}

DISABLE_CFI_ICALL
void GLApiBase::glApplyFramebufferAttachmentCMAAINTELFn(void) {
  driver_->fn.glApplyFramebufferAttachmentCMAAINTELFn();
}

DISABLE_CFI_ICALL
void GLApiBase::glAttachShaderFn(GLuint program, GLuint shader) {
  driver_->fn.glAttachShaderFn(program, shader);
}

DISABLE_CFI_ICALL
void GLApiBase::glBeginQueryFn(GLenum target, GLuint id) {
  driver_->fn.glBeginQueryFn(target, id);
}

DISABLE_CFI_ICALL
void GLApiBase::glBeginTransformFeedbackFn(GLenum primitiveMode) {
  driver_->fn.glBeginTransformFeedbackFn(primitiveMode);
}

DISABLE_CFI_ICALL
void GLApiBase::glBindAttribLocationFn(GLuint program,
                                       GLuint index,
                                       const char* name) {
  driver_->fn.glBindAttribLocationFn(program, index, name);
}

DISABLE_CFI_ICALL
void GLApiBase::glBindBufferFn(GLenum target, GLuint buffer) {
  driver_->fn.glBindBufferFn(target, buffer);
}

DISABLE_CFI_ICALL
void GLApiBase::glBindBufferBaseFn(GLenum target, GLuint index, GLuint buffer) {
  driver_->fn.glBindBufferBaseFn(target, index, buffer);
}

DISABLE_CFI_ICALL
void GLApiBase::glBindBufferRangeFn(GLenum target,
                                    GLuint index,
                                    GLuint buffer,
                                    GLintptr offset,
                                    GLsizeiptr size) {
  driver_->fn.glBindBufferRangeFn(target, index, buffer, offset, size);
}

DISABLE_CFI_ICALL
void GLApiBase::glBindFragDataLocationFn(GLuint program,
                                         GLuint colorNumber,
                                         const char* name) {
  driver_->fn.glBindFragDataLocationFn(program, colorNumber, name);
}

DISABLE_CFI_ICALL
void GLApiBase::glBindFragDataLocationIndexedFn(GLuint program,
                                                GLuint colorNumber,
                                                GLuint index,
                                                const char* name) {
  driver_->fn.glBindFragDataLocationIndexedFn(program, colorNumber, index,
                                              name);
}

DISABLE_CFI_ICALL
void GLApiBase::glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) {
  driver_->fn.glBindFramebufferEXTFn(target, framebuffer);
}

DISABLE_CFI_ICALL
void GLApiBase::glBindImageTextureEXTFn(GLuint index,
                                        GLuint texture,
                                        GLint level,
                                        GLboolean layered,
                                        GLint layer,
                                        GLenum access,
                                        GLint format) {
  driver_->fn.glBindImageTextureEXTFn(index, texture, level, layered, layer,
                                      access, format);
}

DISABLE_CFI_ICALL
void GLApiBase::glBindRenderbufferEXTFn(GLenum target, GLuint renderbuffer) {
  driver_->fn.glBindRenderbufferEXTFn(target, renderbuffer);
}

DISABLE_CFI_ICALL
void GLApiBase::glBindSamplerFn(GLuint unit, GLuint sampler) {
  driver_->fn.glBindSamplerFn(unit, sampler);
}

DISABLE_CFI_ICALL
void GLApiBase::glBindTextureFn(GLenum target, GLuint texture) {
  driver_->fn.glBindTextureFn(target, texture);
}

DISABLE_CFI_ICALL
void GLApiBase::glBindTransformFeedbackFn(GLenum target, GLuint id) {
  driver_->fn.glBindTransformFeedbackFn(target, id);
}

DISABLE_CFI_ICALL
void GLApiBase::glBindUniformLocationCHROMIUMFn(GLuint program,
                                                GLint location,
                                                const char* name) {
  driver_->fn.glBindUniformLocationCHROMIUMFn(program, location, name);
}

DISABLE_CFI_ICALL
void GLApiBase::glBindVertexArrayOESFn(GLuint array) {
  driver_->fn.glBindVertexArrayOESFn(array);
}

DISABLE_CFI_ICALL
void GLApiBase::glBlendBarrierKHRFn(void) {
  driver_->fn.glBlendBarrierKHRFn();
}

DISABLE_CFI_ICALL
void GLApiBase::glBlendColorFn(GLclampf red,
                               GLclampf green,
                               GLclampf blue,
                               GLclampf alpha) {
  driver_->fn.glBlendColorFn(red, green, blue, alpha);
}

DISABLE_CFI_ICALL
void GLApiBase::glBlendEquationFn(GLenum mode) {
  driver_->fn.glBlendEquationFn(mode);
}

DISABLE_CFI_ICALL
void GLApiBase::glBlendEquationSeparateFn(GLenum modeRGB, GLenum modeAlpha) {
  driver_->fn.glBlendEquationSeparateFn(modeRGB, modeAlpha);
}

DISABLE_CFI_ICALL
void GLApiBase::glBlendFuncFn(GLenum sfactor, GLenum dfactor) {
  driver_->fn.glBlendFuncFn(sfactor, dfactor);
}

DISABLE_CFI_ICALL
void GLApiBase::glBlendFuncSeparateFn(GLenum srcRGB,
                                      GLenum dstRGB,
                                      GLenum srcAlpha,
                                      GLenum dstAlpha) {
  driver_->fn.glBlendFuncSeparateFn(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

DISABLE_CFI_ICALL
void GLApiBase::glBlitFramebufferFn(GLint srcX0,
                                    GLint srcY0,
                                    GLint srcX1,
                                    GLint srcY1,
                                    GLint dstX0,
                                    GLint dstY0,
                                    GLint dstX1,
                                    GLint dstY1,
                                    GLbitfield mask,
                                    GLenum filter) {
  driver_->fn.glBlitFramebufferFn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                  dstX1, dstY1, mask, filter);
}

DISABLE_CFI_ICALL
void GLApiBase::glBufferDataFn(GLenum target,
                               GLsizeiptr size,
                               const void* data,
                               GLenum usage) {
  driver_->fn.glBufferDataFn(target, size, data, usage);
}

DISABLE_CFI_ICALL
void GLApiBase::glBufferSubDataFn(GLenum target,
                                  GLintptr offset,
                                  GLsizeiptr size,
                                  const void* data) {
  driver_->fn.glBufferSubDataFn(target, offset, size, data);
}

DISABLE_CFI_ICALL
GLenum GLApiBase::glCheckFramebufferStatusEXTFn(GLenum target) {
  return driver_->fn.glCheckFramebufferStatusEXTFn(target);
}

DISABLE_CFI_ICALL
void GLApiBase::glClearFn(GLbitfield mask) {
  driver_->fn.glClearFn(mask);
}

DISABLE_CFI_ICALL
void GLApiBase::glClearBufferfiFn(GLenum buffer,
                                  GLint drawbuffer,
                                  const GLfloat depth,
                                  GLint stencil) {
  driver_->fn.glClearBufferfiFn(buffer, drawbuffer, depth, stencil);
}

DISABLE_CFI_ICALL
void GLApiBase::glClearBufferfvFn(GLenum buffer,
                                  GLint drawbuffer,
                                  const GLfloat* value) {
  driver_->fn.glClearBufferfvFn(buffer, drawbuffer, value);
}

DISABLE_CFI_ICALL
void GLApiBase::glClearBufferivFn(GLenum buffer,
                                  GLint drawbuffer,
                                  const GLint* value) {
  driver_->fn.glClearBufferivFn(buffer, drawbuffer, value);
}

DISABLE_CFI_ICALL
void GLApiBase::glClearBufferuivFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLuint* value) {
  driver_->fn.glClearBufferuivFn(buffer, drawbuffer, value);
}

DISABLE_CFI_ICALL
void GLApiBase::glClearColorFn(GLclampf red,
                               GLclampf green,
                               GLclampf blue,
                               GLclampf alpha) {
  driver_->fn.glClearColorFn(red, green, blue, alpha);
}

DISABLE_CFI_ICALL
void GLApiBase::glClearDepthFn(GLclampd depth) {
  driver_->fn.glClearDepthFn(depth);
}

DISABLE_CFI_ICALL
void GLApiBase::glClearDepthfFn(GLclampf depth) {
  driver_->fn.glClearDepthfFn(depth);
}

DISABLE_CFI_ICALL
void GLApiBase::glClearStencilFn(GLint s) {
  driver_->fn.glClearStencilFn(s);
}

DISABLE_CFI_ICALL
GLenum GLApiBase::glClientWaitSyncFn(GLsync sync,
                                     GLbitfield flags,
                                     GLuint64 timeout) {
  return driver_->fn.glClientWaitSyncFn(sync, flags, timeout);
}

DISABLE_CFI_ICALL
void GLApiBase::glColorMaskFn(GLboolean red,
                              GLboolean green,
                              GLboolean blue,
                              GLboolean alpha) {
  driver_->fn.glColorMaskFn(red, green, blue, alpha);
}

DISABLE_CFI_ICALL
void GLApiBase::glCompileShaderFn(GLuint shader) {
  driver_->fn.glCompileShaderFn(shader);
}

DISABLE_CFI_ICALL
void GLApiBase::glCompressedCopyTextureCHROMIUMFn(GLuint sourceId,
                                                  GLuint destId) {
  driver_->fn.glCompressedCopyTextureCHROMIUMFn(sourceId, destId);
}

DISABLE_CFI_ICALL
void GLApiBase::glCompressedTexImage2DFn(GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border,
                                         GLsizei imageSize,
                                         const void* data) {
  driver_->fn.glCompressedTexImage2DFn(target, level, internalformat, width,
                                       height, border, imageSize, data);
}

DISABLE_CFI_ICALL
void GLApiBase::glCompressedTexImage2DRobustANGLEFn(GLenum target,
                                                    GLint level,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLint border,
                                                    GLsizei imageSize,
                                                    GLsizei dataSize,
                                                    const void* data) {
  driver_->fn.glCompressedTexImage2DRobustANGLEFn(target, level, internalformat,
                                                  width, height, border,
                                                  imageSize, dataSize, data);
}

DISABLE_CFI_ICALL
void GLApiBase::glCompressedTexImage3DFn(GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLsizei imageSize,
                                         const void* data) {
  driver_->fn.glCompressedTexImage3DFn(target, level, internalformat, width,
                                       height, depth, border, imageSize, data);
}

DISABLE_CFI_ICALL
void GLApiBase::glCompressedTexImage3DRobustANGLEFn(GLenum target,
                                                    GLint level,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLsizei depth,
                                                    GLint border,
                                                    GLsizei imageSize,
                                                    GLsizei dataSize,
                                                    const void* data) {
  driver_->fn.glCompressedTexImage3DRobustANGLEFn(target, level, internalformat,
                                                  width, height, depth, border,
                                                  imageSize, dataSize, data);
}

DISABLE_CFI_ICALL
void GLApiBase::glCompressedTexSubImage2DFn(GLenum target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLsizei imageSize,
                                            const void* data) {
  driver_->fn.glCompressedTexSubImage2DFn(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

DISABLE_CFI_ICALL
void GLApiBase::glCompressedTexSubImage2DRobustANGLEFn(GLenum target,
                                                       GLint level,
                                                       GLint xoffset,
                                                       GLint yoffset,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLenum format,
                                                       GLsizei imageSize,
                                                       GLsizei dataSize,
                                                       const void* data) {
  driver_->fn.glCompressedTexSubImage2DRobustANGLEFn(
      target, level, xoffset, yoffset, width, height, format, imageSize,
      dataSize, data);
}

DISABLE_CFI_ICALL
void GLApiBase::glCompressedTexSubImage3DFn(GLenum target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLint zoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLsizei depth,
                                            GLenum format,
                                            GLsizei imageSize,
                                            const void* data) {
  driver_->fn.glCompressedTexSubImage3DFn(target, level, xoffset, yoffset,
                                          zoffset, width, height, depth, format,
                                          imageSize, data);
}

DISABLE_CFI_ICALL
void GLApiBase::glCompressedTexSubImage3DRobustANGLEFn(GLenum target,
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
                                                       const void* data) {
  driver_->fn.glCompressedTexSubImage3DRobustANGLEFn(
      target, level, xoffset, yoffset, zoffset, width, height, depth, format,
      imageSize, dataSize, data);
}

DISABLE_CFI_ICALL
void GLApiBase::glCopyBufferSubDataFn(GLenum readTarget,
                                      GLenum writeTarget,
                                      GLintptr readOffset,
                                      GLintptr writeOffset,
                                      GLsizeiptr size) {
  driver_->fn.glCopyBufferSubDataFn(readTarget, writeTarget, readOffset,
                                    writeOffset, size);
}

DISABLE_CFI_ICALL
void GLApiBase::glCopySubTextureCHROMIUMFn(GLuint sourceId,
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
                                           GLboolean unpackUnmultiplyAlpha) {
  driver_->fn.glCopySubTextureCHROMIUMFn(
      sourceId, sourceLevel, destTarget, destId, destLevel, xoffset, yoffset, x,
      y, width, height, unpackFlipY, unpackPremultiplyAlpha,
      unpackUnmultiplyAlpha);
}

DISABLE_CFI_ICALL
void GLApiBase::glCopyTexImage2DFn(GLenum target,
                                   GLint level,
                                   GLenum internalformat,
                                   GLint x,
                                   GLint y,
                                   GLsizei width,
                                   GLsizei height,
                                   GLint border) {
  driver_->fn.glCopyTexImage2DFn(target, level, internalformat, x, y, width,
                                 height, border);
}

DISABLE_CFI_ICALL
void GLApiBase::glCopyTexSubImage2DFn(GLenum target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLint x,
                                      GLint y,
                                      GLsizei width,
                                      GLsizei height) {
  driver_->fn.glCopyTexSubImage2DFn(target, level, xoffset, yoffset, x, y,
                                    width, height);
}

DISABLE_CFI_ICALL
void GLApiBase::glCopyTexSubImage3DFn(GLenum target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLint zoffset,
                                      GLint x,
                                      GLint y,
                                      GLsizei width,
                                      GLsizei height) {
  driver_->fn.glCopyTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, x,
                                    y, width, height);
}

DISABLE_CFI_ICALL
void GLApiBase::glCopyTextureCHROMIUMFn(GLuint sourceId,
                                        GLint sourceLevel,
                                        GLenum destTarget,
                                        GLuint destId,
                                        GLint destLevel,
                                        GLint internalFormat,
                                        GLenum destType,
                                        GLboolean unpackFlipY,
                                        GLboolean unpackPremultiplyAlpha,
                                        GLboolean unpackUnmultiplyAlpha) {
  driver_->fn.glCopyTextureCHROMIUMFn(
      sourceId, sourceLevel, destTarget, destId, destLevel, internalFormat,
      destType, unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

DISABLE_CFI_ICALL
void GLApiBase::glCoverageModulationNVFn(GLenum components) {
  driver_->fn.glCoverageModulationNVFn(components);
}

DISABLE_CFI_ICALL
void GLApiBase::glCoverFillPathInstancedNVFn(GLsizei numPaths,
                                             GLenum pathNameType,
                                             const void* paths,
                                             GLuint pathBase,
                                             GLenum coverMode,
                                             GLenum transformType,
                                             const GLfloat* transformValues) {
  driver_->fn.glCoverFillPathInstancedNVFn(numPaths, pathNameType, paths,
                                           pathBase, coverMode, transformType,
                                           transformValues);
}

DISABLE_CFI_ICALL
void GLApiBase::glCoverFillPathNVFn(GLuint path, GLenum coverMode) {
  driver_->fn.glCoverFillPathNVFn(path, coverMode);
}

DISABLE_CFI_ICALL
void GLApiBase::glCoverStrokePathInstancedNVFn(GLsizei numPaths,
                                               GLenum pathNameType,
                                               const void* paths,
                                               GLuint pathBase,
                                               GLenum coverMode,
                                               GLenum transformType,
                                               const GLfloat* transformValues) {
  driver_->fn.glCoverStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                             pathBase, coverMode, transformType,
                                             transformValues);
}

DISABLE_CFI_ICALL
void GLApiBase::glCoverStrokePathNVFn(GLuint name, GLenum coverMode) {
  driver_->fn.glCoverStrokePathNVFn(name, coverMode);
}

DISABLE_CFI_ICALL
GLuint GLApiBase::glCreateProgramFn(void) {
  return driver_->fn.glCreateProgramFn();
}

DISABLE_CFI_ICALL
GLuint GLApiBase::glCreateShaderFn(GLenum type) {
  return driver_->fn.glCreateShaderFn(type);
}

DISABLE_CFI_ICALL
void GLApiBase::glCullFaceFn(GLenum mode) {
  driver_->fn.glCullFaceFn(mode);
}

DISABLE_CFI_ICALL
void GLApiBase::glDebugMessageCallbackFn(GLDEBUGPROC callback,
                                         const void* userParam) {
  driver_->fn.glDebugMessageCallbackFn(callback, userParam);
}

DISABLE_CFI_ICALL
void GLApiBase::glDebugMessageControlFn(GLenum source,
                                        GLenum type,
                                        GLenum severity,
                                        GLsizei count,
                                        const GLuint* ids,
                                        GLboolean enabled) {
  driver_->fn.glDebugMessageControlFn(source, type, severity, count, ids,
                                      enabled);
}

DISABLE_CFI_ICALL
void GLApiBase::glDebugMessageInsertFn(GLenum source,
                                       GLenum type,
                                       GLuint id,
                                       GLenum severity,
                                       GLsizei length,
                                       const char* buf) {
  driver_->fn.glDebugMessageInsertFn(source, type, id, severity, length, buf);
}

DISABLE_CFI_ICALL
void GLApiBase::glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) {
  driver_->fn.glDeleteBuffersARBFn(n, buffers);
}

DISABLE_CFI_ICALL
void GLApiBase::glDeleteFencesAPPLEFn(GLsizei n, const GLuint* fences) {
  driver_->fn.glDeleteFencesAPPLEFn(n, fences);
}

DISABLE_CFI_ICALL
void GLApiBase::glDeleteFencesNVFn(GLsizei n, const GLuint* fences) {
  driver_->fn.glDeleteFencesNVFn(n, fences);
}

DISABLE_CFI_ICALL
void GLApiBase::glDeleteFramebuffersEXTFn(GLsizei n,
                                          const GLuint* framebuffers) {
  driver_->fn.glDeleteFramebuffersEXTFn(n, framebuffers);
}

DISABLE_CFI_ICALL
void GLApiBase::glDeletePathsNVFn(GLuint path, GLsizei range) {
  driver_->fn.glDeletePathsNVFn(path, range);
}

DISABLE_CFI_ICALL
void GLApiBase::glDeleteProgramFn(GLuint program) {
  driver_->fn.glDeleteProgramFn(program);
}

DISABLE_CFI_ICALL
void GLApiBase::glDeleteQueriesFn(GLsizei n, const GLuint* ids) {
  driver_->fn.glDeleteQueriesFn(n, ids);
}

DISABLE_CFI_ICALL
void GLApiBase::glDeleteRenderbuffersEXTFn(GLsizei n,
                                           const GLuint* renderbuffers) {
  driver_->fn.glDeleteRenderbuffersEXTFn(n, renderbuffers);
}

DISABLE_CFI_ICALL
void GLApiBase::glDeleteSamplersFn(GLsizei n, const GLuint* samplers) {
  driver_->fn.glDeleteSamplersFn(n, samplers);
}

DISABLE_CFI_ICALL
void GLApiBase::glDeleteShaderFn(GLuint shader) {
  driver_->fn.glDeleteShaderFn(shader);
}

DISABLE_CFI_ICALL
void GLApiBase::glDeleteSyncFn(GLsync sync) {
  driver_->fn.glDeleteSyncFn(sync);
}

DISABLE_CFI_ICALL
void GLApiBase::glDeleteTexturesFn(GLsizei n, const GLuint* textures) {
  driver_->fn.glDeleteTexturesFn(n, textures);
}

DISABLE_CFI_ICALL
void GLApiBase::glDeleteTransformFeedbacksFn(GLsizei n, const GLuint* ids) {
  driver_->fn.glDeleteTransformFeedbacksFn(n, ids);
}

DISABLE_CFI_ICALL
void GLApiBase::glDeleteVertexArraysOESFn(GLsizei n, const GLuint* arrays) {
  driver_->fn.glDeleteVertexArraysOESFn(n, arrays);
}

DISABLE_CFI_ICALL
void GLApiBase::glDepthFuncFn(GLenum func) {
  driver_->fn.glDepthFuncFn(func);
}

DISABLE_CFI_ICALL
void GLApiBase::glDepthMaskFn(GLboolean flag) {
  driver_->fn.glDepthMaskFn(flag);
}

DISABLE_CFI_ICALL
void GLApiBase::glDepthRangeFn(GLclampd zNear, GLclampd zFar) {
  driver_->fn.glDepthRangeFn(zNear, zFar);
}

DISABLE_CFI_ICALL
void GLApiBase::glDepthRangefFn(GLclampf zNear, GLclampf zFar) {
  driver_->fn.glDepthRangefFn(zNear, zFar);
}

DISABLE_CFI_ICALL
void GLApiBase::glDetachShaderFn(GLuint program, GLuint shader) {
  driver_->fn.glDetachShaderFn(program, shader);
}

DISABLE_CFI_ICALL
void GLApiBase::glDisableFn(GLenum cap) {
  driver_->fn.glDisableFn(cap);
}

DISABLE_CFI_ICALL
void GLApiBase::glDisableVertexAttribArrayFn(GLuint index) {
  driver_->fn.glDisableVertexAttribArrayFn(index);
}

DISABLE_CFI_ICALL
void GLApiBase::glDiscardFramebufferEXTFn(GLenum target,
                                          GLsizei numAttachments,
                                          const GLenum* attachments) {
  driver_->fn.glDiscardFramebufferEXTFn(target, numAttachments, attachments);
}

DISABLE_CFI_ICALL
void GLApiBase::glDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  driver_->fn.glDrawArraysFn(mode, first, count);
}

DISABLE_CFI_ICALL
void GLApiBase::glDrawArraysInstancedANGLEFn(GLenum mode,
                                             GLint first,
                                             GLsizei count,
                                             GLsizei primcount) {
  driver_->fn.glDrawArraysInstancedANGLEFn(mode, first, count, primcount);
}

DISABLE_CFI_ICALL
void GLApiBase::glDrawBufferFn(GLenum mode) {
  driver_->fn.glDrawBufferFn(mode);
}

DISABLE_CFI_ICALL
void GLApiBase::glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) {
  driver_->fn.glDrawBuffersARBFn(n, bufs);
}

DISABLE_CFI_ICALL
void GLApiBase::glDrawElementsFn(GLenum mode,
                                 GLsizei count,
                                 GLenum type,
                                 const void* indices) {
  driver_->fn.glDrawElementsFn(mode, count, type, indices);
}

DISABLE_CFI_ICALL
void GLApiBase::glDrawElementsInstancedANGLEFn(GLenum mode,
                                               GLsizei count,
                                               GLenum type,
                                               const void* indices,
                                               GLsizei primcount) {
  driver_->fn.glDrawElementsInstancedANGLEFn(mode, count, type, indices,
                                             primcount);
}

DISABLE_CFI_ICALL
void GLApiBase::glDrawRangeElementsFn(GLenum mode,
                                      GLuint start,
                                      GLuint end,
                                      GLsizei count,
                                      GLenum type,
                                      const void* indices) {
  driver_->fn.glDrawRangeElementsFn(mode, start, end, count, type, indices);
}

DISABLE_CFI_ICALL
void GLApiBase::glEGLImageTargetRenderbufferStorageOESFn(GLenum target,
                                                         GLeglImageOES image) {
  driver_->fn.glEGLImageTargetRenderbufferStorageOESFn(target, image);
}

DISABLE_CFI_ICALL
void GLApiBase::glEGLImageTargetTexture2DOESFn(GLenum target,
                                               GLeglImageOES image) {
  driver_->fn.glEGLImageTargetTexture2DOESFn(target, image);
}

DISABLE_CFI_ICALL
void GLApiBase::glEnableFn(GLenum cap) {
  driver_->fn.glEnableFn(cap);
}

DISABLE_CFI_ICALL
void GLApiBase::glEnableVertexAttribArrayFn(GLuint index) {
  driver_->fn.glEnableVertexAttribArrayFn(index);
}

DISABLE_CFI_ICALL
void GLApiBase::glEndQueryFn(GLenum target) {
  driver_->fn.glEndQueryFn(target);
}

DISABLE_CFI_ICALL
void GLApiBase::glEndTransformFeedbackFn(void) {
  driver_->fn.glEndTransformFeedbackFn();
}

DISABLE_CFI_ICALL
GLsync GLApiBase::glFenceSyncFn(GLenum condition, GLbitfield flags) {
  return driver_->fn.glFenceSyncFn(condition, flags);
}

DISABLE_CFI_ICALL
void GLApiBase::glFinishFn(void) {
  driver_->fn.glFinishFn();
}

DISABLE_CFI_ICALL
void GLApiBase::glFinishFenceAPPLEFn(GLuint fence) {
  driver_->fn.glFinishFenceAPPLEFn(fence);
}

DISABLE_CFI_ICALL
void GLApiBase::glFinishFenceNVFn(GLuint fence) {
  driver_->fn.glFinishFenceNVFn(fence);
}

DISABLE_CFI_ICALL
void GLApiBase::glFlushFn(void) {
  driver_->fn.glFlushFn();
}

DISABLE_CFI_ICALL
void GLApiBase::glFlushMappedBufferRangeFn(GLenum target,
                                           GLintptr offset,
                                           GLsizeiptr length) {
  driver_->fn.glFlushMappedBufferRangeFn(target, offset, length);
}

DISABLE_CFI_ICALL
void GLApiBase::glFramebufferRenderbufferEXTFn(GLenum target,
                                               GLenum attachment,
                                               GLenum renderbuffertarget,
                                               GLuint renderbuffer) {
  driver_->fn.glFramebufferRenderbufferEXTFn(target, attachment,
                                             renderbuffertarget, renderbuffer);
}

DISABLE_CFI_ICALL
void GLApiBase::glFramebufferTexture2DEXTFn(GLenum target,
                                            GLenum attachment,
                                            GLenum textarget,
                                            GLuint texture,
                                            GLint level) {
  driver_->fn.glFramebufferTexture2DEXTFn(target, attachment, textarget,
                                          texture, level);
}

DISABLE_CFI_ICALL
void GLApiBase::glFramebufferTexture2DMultisampleEXTFn(GLenum target,
                                                       GLenum attachment,
                                                       GLenum textarget,
                                                       GLuint texture,
                                                       GLint level,
                                                       GLsizei samples) {
  driver_->fn.glFramebufferTexture2DMultisampleEXTFn(
      target, attachment, textarget, texture, level, samples);
}

DISABLE_CFI_ICALL
void GLApiBase::glFramebufferTextureLayerFn(GLenum target,
                                            GLenum attachment,
                                            GLuint texture,
                                            GLint level,
                                            GLint layer) {
  driver_->fn.glFramebufferTextureLayerFn(target, attachment, texture, level,
                                          layer);
}

DISABLE_CFI_ICALL
void GLApiBase::glFrontFaceFn(GLenum mode) {
  driver_->fn.glFrontFaceFn(mode);
}

DISABLE_CFI_ICALL
void GLApiBase::glGenBuffersARBFn(GLsizei n, GLuint* buffers) {
  driver_->fn.glGenBuffersARBFn(n, buffers);
}

DISABLE_CFI_ICALL
void GLApiBase::glGenerateMipmapEXTFn(GLenum target) {
  driver_->fn.glGenerateMipmapEXTFn(target);
}

DISABLE_CFI_ICALL
void GLApiBase::glGenFencesAPPLEFn(GLsizei n, GLuint* fences) {
  driver_->fn.glGenFencesAPPLEFn(n, fences);
}

DISABLE_CFI_ICALL
void GLApiBase::glGenFencesNVFn(GLsizei n, GLuint* fences) {
  driver_->fn.glGenFencesNVFn(n, fences);
}

DISABLE_CFI_ICALL
void GLApiBase::glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) {
  driver_->fn.glGenFramebuffersEXTFn(n, framebuffers);
}

DISABLE_CFI_ICALL
GLuint GLApiBase::glGenPathsNVFn(GLsizei range) {
  return driver_->fn.glGenPathsNVFn(range);
}

DISABLE_CFI_ICALL
void GLApiBase::glGenQueriesFn(GLsizei n, GLuint* ids) {
  driver_->fn.glGenQueriesFn(n, ids);
}

DISABLE_CFI_ICALL
void GLApiBase::glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) {
  driver_->fn.glGenRenderbuffersEXTFn(n, renderbuffers);
}

DISABLE_CFI_ICALL
void GLApiBase::glGenSamplersFn(GLsizei n, GLuint* samplers) {
  driver_->fn.glGenSamplersFn(n, samplers);
}

DISABLE_CFI_ICALL
void GLApiBase::glGenTexturesFn(GLsizei n, GLuint* textures) {
  driver_->fn.glGenTexturesFn(n, textures);
}

DISABLE_CFI_ICALL
void GLApiBase::glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) {
  driver_->fn.glGenTransformFeedbacksFn(n, ids);
}

DISABLE_CFI_ICALL
void GLApiBase::glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) {
  driver_->fn.glGenVertexArraysOESFn(n, arrays);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetActiveAttribFn(GLuint program,
                                    GLuint index,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    GLint* size,
                                    GLenum* type,
                                    char* name) {
  driver_->fn.glGetActiveAttribFn(program, index, bufsize, length, size, type,
                                  name);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetActiveUniformFn(GLuint program,
                                     GLuint index,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     GLint* size,
                                     GLenum* type,
                                     char* name) {
  driver_->fn.glGetActiveUniformFn(program, index, bufsize, length, size, type,
                                   name);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetActiveUniformBlockivFn(GLuint program,
                                            GLuint uniformBlockIndex,
                                            GLenum pname,
                                            GLint* params) {
  driver_->fn.glGetActiveUniformBlockivFn(program, uniformBlockIndex, pname,
                                          params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetActiveUniformBlockivRobustANGLEFn(GLuint program,
                                                       GLuint uniformBlockIndex,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  driver_->fn.glGetActiveUniformBlockivRobustANGLEFn(
      program, uniformBlockIndex, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetActiveUniformBlockNameFn(GLuint program,
                                              GLuint uniformBlockIndex,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              char* uniformBlockName) {
  driver_->fn.glGetActiveUniformBlockNameFn(program, uniformBlockIndex, bufSize,
                                            length, uniformBlockName);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetActiveUniformsivFn(GLuint program,
                                        GLsizei uniformCount,
                                        const GLuint* uniformIndices,
                                        GLenum pname,
                                        GLint* params) {
  driver_->fn.glGetActiveUniformsivFn(program, uniformCount, uniformIndices,
                                      pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetAttachedShadersFn(GLuint program,
                                       GLsizei maxcount,
                                       GLsizei* count,
                                       GLuint* shaders) {
  driver_->fn.glGetAttachedShadersFn(program, maxcount, count, shaders);
}

DISABLE_CFI_ICALL
GLint GLApiBase::glGetAttribLocationFn(GLuint program, const char* name) {
  return driver_->fn.glGetAttribLocationFn(program, name);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetBooleani_vRobustANGLEFn(GLenum target,
                                             GLuint index,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLboolean* data) {
  driver_->fn.glGetBooleani_vRobustANGLEFn(target, index, bufSize, length,
                                           data);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetBooleanvFn(GLenum pname, GLboolean* params) {
  driver_->fn.glGetBooleanvFn(pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetBooleanvRobustANGLEFn(GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLboolean* data) {
  driver_->fn.glGetBooleanvRobustANGLEFn(pname, bufSize, length, data);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetBufferParameteri64vRobustANGLEFn(GLenum target,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint64* params) {
  driver_->fn.glGetBufferParameteri64vRobustANGLEFn(target, pname, bufSize,
                                                    length, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetBufferParameterivFn(GLenum target,
                                         GLenum pname,
                                         GLint* params) {
  driver_->fn.glGetBufferParameterivFn(target, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetBufferParameterivRobustANGLEFn(GLenum target,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint* params) {
  driver_->fn.glGetBufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                  length, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetBufferPointervRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 void** params) {
  driver_->fn.glGetBufferPointervRobustANGLEFn(target, pname, bufSize, length,
                                               params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetDebugMessageLogFn(GLuint count,
                                       GLsizei bufSize,
                                       GLenum* sources,
                                       GLenum* types,
                                       GLuint* ids,
                                       GLenum* severities,
                                       GLsizei* lengths,
                                       char* messageLog) {
  driver_->fn.glGetDebugMessageLogFn(count, bufSize, sources, types, ids,
                                     severities, lengths, messageLog);
}

DISABLE_CFI_ICALL
GLenum GLApiBase::glGetErrorFn(void) {
  return driver_->fn.glGetErrorFn();
}

DISABLE_CFI_ICALL
void GLApiBase::glGetFenceivNVFn(GLuint fence, GLenum pname, GLint* params) {
  driver_->fn.glGetFenceivNVFn(fence, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetFloatvFn(GLenum pname, GLfloat* params) {
  driver_->fn.glGetFloatvFn(pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetFloatvRobustANGLEFn(GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLfloat* data) {
  driver_->fn.glGetFloatvRobustANGLEFn(pname, bufSize, length, data);
}

DISABLE_CFI_ICALL
GLint GLApiBase::glGetFragDataIndexFn(GLuint program, const char* name) {
  return driver_->fn.glGetFragDataIndexFn(program, name);
}

DISABLE_CFI_ICALL
GLint GLApiBase::glGetFragDataLocationFn(GLuint program, const char* name) {
  return driver_->fn.glGetFragDataLocationFn(program, name);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetFramebufferAttachmentParameterivEXTFn(GLenum target,
                                                           GLenum attachment,
                                                           GLenum pname,
                                                           GLint* params) {
  driver_->fn.glGetFramebufferAttachmentParameterivEXTFn(target, attachment,
                                                         pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetFramebufferAttachmentParameterivRobustANGLEFn(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  driver_->fn.glGetFramebufferAttachmentParameterivRobustANGLEFn(
      target, attachment, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetFramebufferParameterivRobustANGLEFn(GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei* length,
                                                         GLint* params) {
  driver_->fn.glGetFramebufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                       length, params);
}

DISABLE_CFI_ICALL
GLenum GLApiBase::glGetGraphicsResetStatusARBFn(void) {
  return driver_->fn.glGetGraphicsResetStatusARBFn();
}

DISABLE_CFI_ICALL
void GLApiBase::glGetInteger64i_vFn(GLenum target,
                                    GLuint index,
                                    GLint64* data) {
  driver_->fn.glGetInteger64i_vFn(target, index, data);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetInteger64i_vRobustANGLEFn(GLenum target,
                                               GLuint index,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLint64* data) {
  driver_->fn.glGetInteger64i_vRobustANGLEFn(target, index, bufSize, length,
                                             data);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetInteger64vFn(GLenum pname, GLint64* params) {
  driver_->fn.glGetInteger64vFn(pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetInteger64vRobustANGLEFn(GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint64* data) {
  driver_->fn.glGetInteger64vRobustANGLEFn(pname, bufSize, length, data);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetIntegeri_vFn(GLenum target, GLuint index, GLint* data) {
  driver_->fn.glGetIntegeri_vFn(target, index, data);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetIntegeri_vRobustANGLEFn(GLenum target,
                                             GLuint index,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* data) {
  driver_->fn.glGetIntegeri_vRobustANGLEFn(target, index, bufSize, length,
                                           data);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetIntegervFn(GLenum pname, GLint* params) {
  driver_->fn.glGetIntegervFn(pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetIntegervRobustANGLEFn(GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* data) {
  driver_->fn.glGetIntegervRobustANGLEFn(pname, bufSize, length, data);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetInternalformativFn(GLenum target,
                                        GLenum internalformat,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLint* params) {
  driver_->fn.glGetInternalformativFn(target, internalformat, pname, bufSize,
                                      params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetInternalformativRobustANGLEFn(GLenum target,
                                                   GLenum internalformat,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) {
  driver_->fn.glGetInternalformativRobustANGLEFn(target, internalformat, pname,
                                                 bufSize, length, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetMultisamplefvFn(GLenum pname, GLuint index, GLfloat* val) {
  driver_->fn.glGetMultisamplefvFn(pname, index, val);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetMultisamplefvRobustANGLEFn(GLenum pname,
                                                GLuint index,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLfloat* val) {
  driver_->fn.glGetMultisamplefvRobustANGLEFn(pname, index, bufSize, length,
                                              val);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetnUniformfvRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLfloat* params) {
  driver_->fn.glGetnUniformfvRobustANGLEFn(program, location, bufSize, length,
                                           params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetnUniformivRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  driver_->fn.glGetnUniformivRobustANGLEFn(program, location, bufSize, length,
                                           params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetnUniformuivRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLuint* params) {
  driver_->fn.glGetnUniformuivRobustANGLEFn(program, location, bufSize, length,
                                            params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetObjectLabelFn(GLenum identifier,
                                   GLuint name,
                                   GLsizei bufSize,
                                   GLsizei* length,
                                   char* label) {
  driver_->fn.glGetObjectLabelFn(identifier, name, bufSize, length, label);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetObjectPtrLabelFn(void* ptr,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      char* label) {
  driver_->fn.glGetObjectPtrLabelFn(ptr, bufSize, length, label);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetPointervFn(GLenum pname, void** params) {
  driver_->fn.glGetPointervFn(pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetPointervRobustANGLERobustANGLEFn(GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      void** params) {
  driver_->fn.glGetPointervRobustANGLERobustANGLEFn(pname, bufSize, length,
                                                    params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetProgramBinaryFn(GLuint program,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     GLenum* binaryFormat,
                                     GLvoid* binary) {
  driver_->fn.glGetProgramBinaryFn(program, bufSize, length, binaryFormat,
                                   binary);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetProgramInfoLogFn(GLuint program,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      char* infolog) {
  driver_->fn.glGetProgramInfoLogFn(program, bufsize, length, infolog);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetProgramInterfaceivFn(GLuint program,
                                          GLenum programInterface,
                                          GLenum pname,
                                          GLint* params) {
  driver_->fn.glGetProgramInterfaceivFn(program, programInterface, pname,
                                        params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetProgramInterfaceivRobustANGLEFn(GLuint program,
                                                     GLenum programInterface,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  driver_->fn.glGetProgramInterfaceivRobustANGLEFn(
      program, programInterface, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetProgramivFn(GLuint program, GLenum pname, GLint* params) {
  driver_->fn.glGetProgramivFn(program, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetProgramivRobustANGLEFn(GLuint program,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* params) {
  driver_->fn.glGetProgramivRobustANGLEFn(program, pname, bufSize, length,
                                          params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetProgramResourceivFn(GLuint program,
                                         GLenum programInterface,
                                         GLuint index,
                                         GLsizei propCount,
                                         const GLenum* props,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLint* params) {
  driver_->fn.glGetProgramResourceivFn(program, programInterface, index,
                                       propCount, props, bufSize, length,
                                       params);
}

DISABLE_CFI_ICALL
GLint GLApiBase::glGetProgramResourceLocationFn(GLuint program,
                                                GLenum programInterface,
                                                const char* name) {
  return driver_->fn.glGetProgramResourceLocationFn(program, programInterface,
                                                    name);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetProgramResourceNameFn(GLuint program,
                                           GLenum programInterface,
                                           GLuint index,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLchar* name) {
  driver_->fn.glGetProgramResourceNameFn(program, programInterface, index,
                                         bufSize, length, name);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetQueryivFn(GLenum target, GLenum pname, GLint* params) {
  driver_->fn.glGetQueryivFn(target, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetQueryivRobustANGLEFn(GLenum target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params) {
  driver_->fn.glGetQueryivRobustANGLEFn(target, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetQueryObjecti64vFn(GLuint id,
                                       GLenum pname,
                                       GLint64* params) {
  driver_->fn.glGetQueryObjecti64vFn(id, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetQueryObjecti64vRobustANGLEFn(GLuint id,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint64* params) {
  driver_->fn.glGetQueryObjecti64vRobustANGLEFn(id, pname, bufSize, length,
                                                params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetQueryObjectivFn(GLuint id, GLenum pname, GLint* params) {
  driver_->fn.glGetQueryObjectivFn(id, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetQueryObjectivRobustANGLEFn(GLuint id,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* params) {
  driver_->fn.glGetQueryObjectivRobustANGLEFn(id, pname, bufSize, length,
                                              params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetQueryObjectui64vFn(GLuint id,
                                        GLenum pname,
                                        GLuint64* params) {
  driver_->fn.glGetQueryObjectui64vFn(id, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetQueryObjectui64vRobustANGLEFn(GLuint id,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLuint64* params) {
  driver_->fn.glGetQueryObjectui64vRobustANGLEFn(id, pname, bufSize, length,
                                                 params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetQueryObjectuivFn(GLuint id, GLenum pname, GLuint* params) {
  driver_->fn.glGetQueryObjectuivFn(id, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetQueryObjectuivRobustANGLEFn(GLuint id,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLuint* params) {
  driver_->fn.glGetQueryObjectuivRobustANGLEFn(id, pname, bufSize, length,
                                               params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetRenderbufferParameterivEXTFn(GLenum target,
                                                  GLenum pname,
                                                  GLint* params) {
  driver_->fn.glGetRenderbufferParameterivEXTFn(target, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetRenderbufferParameterivRobustANGLEFn(GLenum target,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params) {
  driver_->fn.glGetRenderbufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                        length, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetSamplerParameterfvFn(GLuint sampler,
                                          GLenum pname,
                                          GLfloat* params) {
  driver_->fn.glGetSamplerParameterfvFn(sampler, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLfloat* params) {
  driver_->fn.glGetSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize,
                                                   length, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  driver_->fn.glGetSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize,
                                                    length, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLuint* params) {
  driver_->fn.glGetSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize,
                                                     length, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetSamplerParameterivFn(GLuint sampler,
                                          GLenum pname,
                                          GLint* params) {
  driver_->fn.glGetSamplerParameterivFn(sampler, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  driver_->fn.glGetSamplerParameterivRobustANGLEFn(sampler, pname, bufSize,
                                                   length, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetShaderInfoLogFn(GLuint shader,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     char* infolog) {
  driver_->fn.glGetShaderInfoLogFn(shader, bufsize, length, infolog);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetShaderivFn(GLuint shader, GLenum pname, GLint* params) {
  driver_->fn.glGetShaderivFn(shader, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetShaderivRobustANGLEFn(GLuint shader,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) {
  driver_->fn.glGetShaderivRobustANGLEFn(shader, pname, bufSize, length,
                                         params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetShaderPrecisionFormatFn(GLenum shadertype,
                                             GLenum precisiontype,
                                             GLint* range,
                                             GLint* precision) {
  driver_->fn.glGetShaderPrecisionFormatFn(shadertype, precisiontype, range,
                                           precision);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetShaderSourceFn(GLuint shader,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    char* source) {
  driver_->fn.glGetShaderSourceFn(shader, bufsize, length, source);
}

DISABLE_CFI_ICALL
const GLubyte* GLApiBase::glGetStringFn(GLenum name) {
  return driver_->fn.glGetStringFn(name);
}

DISABLE_CFI_ICALL
const GLubyte* GLApiBase::glGetStringiFn(GLenum name, GLuint index) {
  return driver_->fn.glGetStringiFn(name, index);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetSyncivFn(GLsync sync,
                              GLenum pname,
                              GLsizei bufSize,
                              GLsizei* length,
                              GLint* values) {
  driver_->fn.glGetSyncivFn(sync, pname, bufSize, length, values);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetTexLevelParameterfvFn(GLenum target,
                                           GLint level,
                                           GLenum pname,
                                           GLfloat* params) {
  driver_->fn.glGetTexLevelParameterfvFn(target, level, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetTexLevelParameterfvRobustANGLEFn(GLenum target,
                                                      GLint level,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  driver_->fn.glGetTexLevelParameterfvRobustANGLEFn(target, level, pname,
                                                    bufSize, length, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetTexLevelParameterivFn(GLenum target,
                                           GLint level,
                                           GLenum pname,
                                           GLint* params) {
  driver_->fn.glGetTexLevelParameterivFn(target, level, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetTexLevelParameterivRobustANGLEFn(GLenum target,
                                                      GLint level,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  driver_->fn.glGetTexLevelParameterivRobustANGLEFn(target, level, pname,
                                                    bufSize, length, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetTexParameterfvFn(GLenum target,
                                      GLenum pname,
                                      GLfloat* params) {
  driver_->fn.glGetTexParameterfvFn(target, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetTexParameterfvRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* params) {
  driver_->fn.glGetTexParameterfvRobustANGLEFn(target, pname, bufSize, length,
                                               params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetTexParameterIivRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  driver_->fn.glGetTexParameterIivRobustANGLEFn(target, pname, bufSize, length,
                                                params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetTexParameterIuivRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLuint* params) {
  driver_->fn.glGetTexParameterIuivRobustANGLEFn(target, pname, bufSize, length,
                                                 params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetTexParameterivFn(GLenum target,
                                      GLenum pname,
                                      GLint* params) {
  driver_->fn.glGetTexParameterivFn(target, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetTexParameterivRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  driver_->fn.glGetTexParameterivRobustANGLEFn(target, pname, bufSize, length,
                                               params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetTransformFeedbackVaryingFn(GLuint program,
                                                GLuint index,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLsizei* size,
                                                GLenum* type,
                                                char* name) {
  driver_->fn.glGetTransformFeedbackVaryingFn(program, index, bufSize, length,
                                              size, type, name);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetTranslatedShaderSourceANGLEFn(GLuint shader,
                                                   GLsizei bufsize,
                                                   GLsizei* length,
                                                   char* source) {
  driver_->fn.glGetTranslatedShaderSourceANGLEFn(shader, bufsize, length,
                                                 source);
}

DISABLE_CFI_ICALL
GLuint GLApiBase::glGetUniformBlockIndexFn(GLuint program,
                                           const char* uniformBlockName) {
  return driver_->fn.glGetUniformBlockIndexFn(program, uniformBlockName);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetUniformfvFn(GLuint program,
                                 GLint location,
                                 GLfloat* params) {
  driver_->fn.glGetUniformfvFn(program, location, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetUniformfvRobustANGLEFn(GLuint program,
                                            GLint location,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLfloat* params) {
  driver_->fn.glGetUniformfvRobustANGLEFn(program, location, bufSize, length,
                                          params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetUniformIndicesFn(GLuint program,
                                      GLsizei uniformCount,
                                      const char* const* uniformNames,
                                      GLuint* uniformIndices) {
  driver_->fn.glGetUniformIndicesFn(program, uniformCount, uniformNames,
                                    uniformIndices);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetUniformivFn(GLuint program,
                                 GLint location,
                                 GLint* params) {
  driver_->fn.glGetUniformivFn(program, location, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetUniformivRobustANGLEFn(GLuint program,
                                            GLint location,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* params) {
  driver_->fn.glGetUniformivRobustANGLEFn(program, location, bufSize, length,
                                          params);
}

DISABLE_CFI_ICALL
GLint GLApiBase::glGetUniformLocationFn(GLuint program, const char* name) {
  return driver_->fn.glGetUniformLocationFn(program, name);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetUniformuivFn(GLuint program,
                                  GLint location,
                                  GLuint* params) {
  driver_->fn.glGetUniformuivFn(program, location, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetUniformuivRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLuint* params) {
  driver_->fn.glGetUniformuivRobustANGLEFn(program, location, bufSize, length,
                                           params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetVertexAttribfvFn(GLuint index,
                                      GLenum pname,
                                      GLfloat* params) {
  driver_->fn.glGetVertexAttribfvFn(index, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetVertexAttribfvRobustANGLEFn(GLuint index,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* params) {
  driver_->fn.glGetVertexAttribfvRobustANGLEFn(index, pname, bufSize, length,
                                               params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetVertexAttribIivRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  driver_->fn.glGetVertexAttribIivRobustANGLEFn(index, pname, bufSize, length,
                                                params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetVertexAttribIuivRobustANGLEFn(GLuint index,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLuint* params) {
  driver_->fn.glGetVertexAttribIuivRobustANGLEFn(index, pname, bufSize, length,
                                                 params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetVertexAttribivFn(GLuint index,
                                      GLenum pname,
                                      GLint* params) {
  driver_->fn.glGetVertexAttribivFn(index, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetVertexAttribivRobustANGLEFn(GLuint index,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  driver_->fn.glGetVertexAttribivRobustANGLEFn(index, pname, bufSize, length,
                                               params);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetVertexAttribPointervFn(GLuint index,
                                            GLenum pname,
                                            void** pointer) {
  driver_->fn.glGetVertexAttribPointervFn(index, pname, pointer);
}

DISABLE_CFI_ICALL
void GLApiBase::glGetVertexAttribPointervRobustANGLEFn(GLuint index,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       void** pointer) {
  driver_->fn.glGetVertexAttribPointervRobustANGLEFn(index, pname, bufSize,
                                                     length, pointer);
}

DISABLE_CFI_ICALL
void GLApiBase::glHintFn(GLenum target, GLenum mode) {
  driver_->fn.glHintFn(target, mode);
}

DISABLE_CFI_ICALL
void GLApiBase::glInsertEventMarkerEXTFn(GLsizei length, const char* marker) {
  driver_->fn.glInsertEventMarkerEXTFn(length, marker);
}

DISABLE_CFI_ICALL
void GLApiBase::glInvalidateFramebufferFn(GLenum target,
                                          GLsizei numAttachments,
                                          const GLenum* attachments) {
  driver_->fn.glInvalidateFramebufferFn(target, numAttachments, attachments);
}

DISABLE_CFI_ICALL
void GLApiBase::glInvalidateSubFramebufferFn(GLenum target,
                                             GLsizei numAttachments,
                                             const GLenum* attachments,
                                             GLint x,
                                             GLint y,
                                             GLint width,
                                             GLint height) {
  driver_->fn.glInvalidateSubFramebufferFn(target, numAttachments, attachments,
                                           x, y, width, height);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsBufferFn(GLuint buffer) {
  return driver_->fn.glIsBufferFn(buffer);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsEnabledFn(GLenum cap) {
  return driver_->fn.glIsEnabledFn(cap);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsFenceAPPLEFn(GLuint fence) {
  return driver_->fn.glIsFenceAPPLEFn(fence);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsFenceNVFn(GLuint fence) {
  return driver_->fn.glIsFenceNVFn(fence);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsFramebufferEXTFn(GLuint framebuffer) {
  return driver_->fn.glIsFramebufferEXTFn(framebuffer);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsPathNVFn(GLuint path) {
  return driver_->fn.glIsPathNVFn(path);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsProgramFn(GLuint program) {
  return driver_->fn.glIsProgramFn(program);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsQueryFn(GLuint query) {
  return driver_->fn.glIsQueryFn(query);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsRenderbufferEXTFn(GLuint renderbuffer) {
  return driver_->fn.glIsRenderbufferEXTFn(renderbuffer);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsSamplerFn(GLuint sampler) {
  return driver_->fn.glIsSamplerFn(sampler);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsShaderFn(GLuint shader) {
  return driver_->fn.glIsShaderFn(shader);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsSyncFn(GLsync sync) {
  return driver_->fn.glIsSyncFn(sync);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsTextureFn(GLuint texture) {
  return driver_->fn.glIsTextureFn(texture);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsTransformFeedbackFn(GLuint id) {
  return driver_->fn.glIsTransformFeedbackFn(id);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glIsVertexArrayOESFn(GLuint array) {
  return driver_->fn.glIsVertexArrayOESFn(array);
}

DISABLE_CFI_ICALL
void GLApiBase::glLineWidthFn(GLfloat width) {
  driver_->fn.glLineWidthFn(width);
}

DISABLE_CFI_ICALL
void GLApiBase::glLinkProgramFn(GLuint program) {
  driver_->fn.glLinkProgramFn(program);
}

DISABLE_CFI_ICALL
void* GLApiBase::glMapBufferFn(GLenum target, GLenum access) {
  return driver_->fn.glMapBufferFn(target, access);
}

DISABLE_CFI_ICALL
void* GLApiBase::glMapBufferRangeFn(GLenum target,
                                    GLintptr offset,
                                    GLsizeiptr length,
                                    GLbitfield access) {
  return driver_->fn.glMapBufferRangeFn(target, offset, length, access);
}

DISABLE_CFI_ICALL
void GLApiBase::glMatrixLoadfEXTFn(GLenum matrixMode, const GLfloat* m) {
  driver_->fn.glMatrixLoadfEXTFn(matrixMode, m);
}

DISABLE_CFI_ICALL
void GLApiBase::glMatrixLoadIdentityEXTFn(GLenum matrixMode) {
  driver_->fn.glMatrixLoadIdentityEXTFn(matrixMode);
}

DISABLE_CFI_ICALL
void GLApiBase::glMemoryBarrierEXTFn(GLbitfield barriers) {
  driver_->fn.glMemoryBarrierEXTFn(barriers);
}

DISABLE_CFI_ICALL
void GLApiBase::glObjectLabelFn(GLenum identifier,
                                GLuint name,
                                GLsizei length,
                                const char* label) {
  driver_->fn.glObjectLabelFn(identifier, name, length, label);
}

DISABLE_CFI_ICALL
void GLApiBase::glObjectPtrLabelFn(void* ptr,
                                   GLsizei length,
                                   const char* label) {
  driver_->fn.glObjectPtrLabelFn(ptr, length, label);
}

DISABLE_CFI_ICALL
void GLApiBase::glPathCommandsNVFn(GLuint path,
                                   GLsizei numCommands,
                                   const GLubyte* commands,
                                   GLsizei numCoords,
                                   GLenum coordType,
                                   const GLvoid* coords) {
  driver_->fn.glPathCommandsNVFn(path, numCommands, commands, numCoords,
                                 coordType, coords);
}

DISABLE_CFI_ICALL
void GLApiBase::glPathParameterfNVFn(GLuint path, GLenum pname, GLfloat value) {
  driver_->fn.glPathParameterfNVFn(path, pname, value);
}

DISABLE_CFI_ICALL
void GLApiBase::glPathParameteriNVFn(GLuint path, GLenum pname, GLint value) {
  driver_->fn.glPathParameteriNVFn(path, pname, value);
}

DISABLE_CFI_ICALL
void GLApiBase::glPathStencilFuncNVFn(GLenum func, GLint ref, GLuint mask) {
  driver_->fn.glPathStencilFuncNVFn(func, ref, mask);
}

DISABLE_CFI_ICALL
void GLApiBase::glPauseTransformFeedbackFn(void) {
  driver_->fn.glPauseTransformFeedbackFn();
}

DISABLE_CFI_ICALL
void GLApiBase::glPixelStoreiFn(GLenum pname, GLint param) {
  driver_->fn.glPixelStoreiFn(pname, param);
}

DISABLE_CFI_ICALL
void GLApiBase::glPointParameteriFn(GLenum pname, GLint param) {
  driver_->fn.glPointParameteriFn(pname, param);
}

DISABLE_CFI_ICALL
void GLApiBase::glPolygonModeFn(GLenum face, GLenum mode) {
  driver_->fn.glPolygonModeFn(face, mode);
}

DISABLE_CFI_ICALL
void GLApiBase::glPolygonOffsetFn(GLfloat factor, GLfloat units) {
  driver_->fn.glPolygonOffsetFn(factor, units);
}

DISABLE_CFI_ICALL
void GLApiBase::glPopDebugGroupFn() {
  driver_->fn.glPopDebugGroupFn();
}

DISABLE_CFI_ICALL
void GLApiBase::glPopGroupMarkerEXTFn(void) {
  driver_->fn.glPopGroupMarkerEXTFn();
}

DISABLE_CFI_ICALL
void GLApiBase::glPrimitiveRestartIndexFn(GLuint index) {
  driver_->fn.glPrimitiveRestartIndexFn(index);
}

DISABLE_CFI_ICALL
void GLApiBase::glProgramBinaryFn(GLuint program,
                                  GLenum binaryFormat,
                                  const GLvoid* binary,
                                  GLsizei length) {
  driver_->fn.glProgramBinaryFn(program, binaryFormat, binary, length);
}

DISABLE_CFI_ICALL
void GLApiBase::glProgramParameteriFn(GLuint program,
                                      GLenum pname,
                                      GLint value) {
  driver_->fn.glProgramParameteriFn(program, pname, value);
}

DISABLE_CFI_ICALL
void GLApiBase::glProgramPathFragmentInputGenNVFn(GLuint program,
                                                  GLint location,
                                                  GLenum genMode,
                                                  GLint components,
                                                  const GLfloat* coeffs) {
  driver_->fn.glProgramPathFragmentInputGenNVFn(program, location, genMode,
                                                components, coeffs);
}

DISABLE_CFI_ICALL
void GLApiBase::glPushDebugGroupFn(GLenum source,
                                   GLuint id,
                                   GLsizei length,
                                   const char* message) {
  driver_->fn.glPushDebugGroupFn(source, id, length, message);
}

DISABLE_CFI_ICALL
void GLApiBase::glPushGroupMarkerEXTFn(GLsizei length, const char* marker) {
  driver_->fn.glPushGroupMarkerEXTFn(length, marker);
}

DISABLE_CFI_ICALL
void GLApiBase::glQueryCounterFn(GLuint id, GLenum target) {
  driver_->fn.glQueryCounterFn(id, target);
}

DISABLE_CFI_ICALL
void GLApiBase::glReadBufferFn(GLenum src) {
  driver_->fn.glReadBufferFn(src);
}

DISABLE_CFI_ICALL
void GLApiBase::glReadnPixelsRobustANGLEFn(GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLsizei* columns,
                                           GLsizei* rows,
                                           void* data) {
  driver_->fn.glReadnPixelsRobustANGLEFn(x, y, width, height, format, type,
                                         bufSize, length, columns, rows, data);
}

DISABLE_CFI_ICALL
void GLApiBase::glReadPixelsFn(GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type,
                               void* pixels) {
  driver_->fn.glReadPixelsFn(x, y, width, height, format, type, pixels);
}

DISABLE_CFI_ICALL
void GLApiBase::glReadPixelsRobustANGLEFn(GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height,
                                          GLenum format,
                                          GLenum type,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLsizei* columns,
                                          GLsizei* rows,
                                          void* pixels) {
  driver_->fn.glReadPixelsRobustANGLEFn(x, y, width, height, format, type,
                                        bufSize, length, columns, rows, pixels);
}

DISABLE_CFI_ICALL
void GLApiBase::glReleaseShaderCompilerFn(void) {
  driver_->fn.glReleaseShaderCompilerFn();
}

DISABLE_CFI_ICALL
void GLApiBase::glRenderbufferStorageEXTFn(GLenum target,
                                           GLenum internalformat,
                                           GLsizei width,
                                           GLsizei height) {
  driver_->fn.glRenderbufferStorageEXTFn(target, internalformat, width, height);
}

DISABLE_CFI_ICALL
void GLApiBase::glRenderbufferStorageMultisampleFn(GLenum target,
                                                   GLsizei samples,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height) {
  driver_->fn.glRenderbufferStorageMultisampleFn(target, samples,
                                                 internalformat, width, height);
}

DISABLE_CFI_ICALL
void GLApiBase::glRenderbufferStorageMultisampleEXTFn(GLenum target,
                                                      GLsizei samples,
                                                      GLenum internalformat,
                                                      GLsizei width,
                                                      GLsizei height) {
  driver_->fn.glRenderbufferStorageMultisampleEXTFn(
      target, samples, internalformat, width, height);
}

DISABLE_CFI_ICALL
void GLApiBase::glRequestExtensionANGLEFn(const char* name) {
  driver_->fn.glRequestExtensionANGLEFn(name);
}

DISABLE_CFI_ICALL
void GLApiBase::glResumeTransformFeedbackFn(void) {
  driver_->fn.glResumeTransformFeedbackFn();
}

DISABLE_CFI_ICALL
void GLApiBase::glSampleCoverageFn(GLclampf value, GLboolean invert) {
  driver_->fn.glSampleCoverageFn(value, invert);
}

DISABLE_CFI_ICALL
void GLApiBase::glSamplerParameterfFn(GLuint sampler,
                                      GLenum pname,
                                      GLfloat param) {
  driver_->fn.glSamplerParameterfFn(sampler, pname, param);
}

DISABLE_CFI_ICALL
void GLApiBase::glSamplerParameterfvFn(GLuint sampler,
                                       GLenum pname,
                                       const GLfloat* params) {
  driver_->fn.glSamplerParameterfvFn(sampler, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  const GLfloat* param) {
  driver_->fn.glSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize, param);
}

DISABLE_CFI_ICALL
void GLApiBase::glSamplerParameteriFn(GLuint sampler,
                                      GLenum pname,
                                      GLint param) {
  driver_->fn.glSamplerParameteriFn(sampler, pname, param);
}

DISABLE_CFI_ICALL
void GLApiBase::glSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLint* param) {
  driver_->fn.glSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize,
                                                 param);
}

DISABLE_CFI_ICALL
void GLApiBase::glSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    const GLuint* param) {
  driver_->fn.glSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize,
                                                  param);
}

DISABLE_CFI_ICALL
void GLApiBase::glSamplerParameterivFn(GLuint sampler,
                                       GLenum pname,
                                       const GLint* params) {
  driver_->fn.glSamplerParameterivFn(sampler, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  const GLint* param) {
  driver_->fn.glSamplerParameterivRobustANGLEFn(sampler, pname, bufSize, param);
}

DISABLE_CFI_ICALL
void GLApiBase::glScissorFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  driver_->fn.glScissorFn(x, y, width, height);
}

DISABLE_CFI_ICALL
void GLApiBase::glSetFenceAPPLEFn(GLuint fence) {
  driver_->fn.glSetFenceAPPLEFn(fence);
}

DISABLE_CFI_ICALL
void GLApiBase::glSetFenceNVFn(GLuint fence, GLenum condition) {
  driver_->fn.glSetFenceNVFn(fence, condition);
}

DISABLE_CFI_ICALL
void GLApiBase::glShaderBinaryFn(GLsizei n,
                                 const GLuint* shaders,
                                 GLenum binaryformat,
                                 const void* binary,
                                 GLsizei length) {
  driver_->fn.glShaderBinaryFn(n, shaders, binaryformat, binary, length);
}

DISABLE_CFI_ICALL
void GLApiBase::glShaderSourceFn(GLuint shader,
                                 GLsizei count,
                                 const char* const* str,
                                 const GLint* length) {
  driver_->fn.glShaderSourceFn(shader, count, str, length);
}

DISABLE_CFI_ICALL
void GLApiBase::glStencilFillPathInstancedNVFn(GLsizei numPaths,
                                               GLenum pathNameType,
                                               const void* paths,
                                               GLuint pathBase,
                                               GLenum fillMode,
                                               GLuint mask,
                                               GLenum transformType,
                                               const GLfloat* transformValues) {
  driver_->fn.glStencilFillPathInstancedNVFn(numPaths, pathNameType, paths,
                                             pathBase, fillMode, mask,
                                             transformType, transformValues);
}

DISABLE_CFI_ICALL
void GLApiBase::glStencilFillPathNVFn(GLuint path,
                                      GLenum fillMode,
                                      GLuint mask) {
  driver_->fn.glStencilFillPathNVFn(path, fillMode, mask);
}

DISABLE_CFI_ICALL
void GLApiBase::glStencilFuncFn(GLenum func, GLint ref, GLuint mask) {
  driver_->fn.glStencilFuncFn(func, ref, mask);
}

DISABLE_CFI_ICALL
void GLApiBase::glStencilFuncSeparateFn(GLenum face,
                                        GLenum func,
                                        GLint ref,
                                        GLuint mask) {
  driver_->fn.glStencilFuncSeparateFn(face, func, ref, mask);
}

DISABLE_CFI_ICALL
void GLApiBase::glStencilMaskFn(GLuint mask) {
  driver_->fn.glStencilMaskFn(mask);
}

DISABLE_CFI_ICALL
void GLApiBase::glStencilMaskSeparateFn(GLenum face, GLuint mask) {
  driver_->fn.glStencilMaskSeparateFn(face, mask);
}

DISABLE_CFI_ICALL
void GLApiBase::glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) {
  driver_->fn.glStencilOpFn(fail, zfail, zpass);
}

DISABLE_CFI_ICALL
void GLApiBase::glStencilOpSeparateFn(GLenum face,
                                      GLenum fail,
                                      GLenum zfail,
                                      GLenum zpass) {
  driver_->fn.glStencilOpSeparateFn(face, fail, zfail, zpass);
}

DISABLE_CFI_ICALL
void GLApiBase::glStencilStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  driver_->fn.glStencilStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                               pathBase, ref, mask,
                                               transformType, transformValues);
}

DISABLE_CFI_ICALL
void GLApiBase::glStencilStrokePathNVFn(GLuint path,
                                        GLint reference,
                                        GLuint mask) {
  driver_->fn.glStencilStrokePathNVFn(path, reference, mask);
}

DISABLE_CFI_ICALL
void GLApiBase::glStencilThenCoverFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  driver_->fn.glStencilThenCoverFillPathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, fillMode, mask, coverMode,
      transformType, transformValues);
}

DISABLE_CFI_ICALL
void GLApiBase::glStencilThenCoverFillPathNVFn(GLuint path,
                                               GLenum fillMode,
                                               GLuint mask,
                                               GLenum coverMode) {
  driver_->fn.glStencilThenCoverFillPathNVFn(path, fillMode, mask, coverMode);
}

DISABLE_CFI_ICALL
void GLApiBase::glStencilThenCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  driver_->fn.glStencilThenCoverStrokePathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, ref, mask, coverMode,
      transformType, transformValues);
}

DISABLE_CFI_ICALL
void GLApiBase::glStencilThenCoverStrokePathNVFn(GLuint path,
                                                 GLint reference,
                                                 GLuint mask,
                                                 GLenum coverMode) {
  driver_->fn.glStencilThenCoverStrokePathNVFn(path, reference, mask,
                                               coverMode);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glTestFenceAPPLEFn(GLuint fence) {
  return driver_->fn.glTestFenceAPPLEFn(fence);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glTestFenceNVFn(GLuint fence) {
  return driver_->fn.glTestFenceNVFn(fence);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexBufferFn(GLenum target,
                              GLenum internalformat,
                              GLuint buffer) {
  driver_->fn.glTexBufferFn(target, internalformat, buffer);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexBufferRangeFn(GLenum target,
                                   GLenum internalformat,
                                   GLuint buffer,
                                   GLintptr offset,
                                   GLsizeiptr size) {
  driver_->fn.glTexBufferRangeFn(target, internalformat, buffer, offset, size);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexImage2DFn(GLenum target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               const void* pixels) {
  driver_->fn.glTexImage2DFn(target, level, internalformat, width, height,
                             border, format, type, pixels);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexImage2DRobustANGLEFn(GLenum target,
                                          GLint level,
                                          GLint internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLint border,
                                          GLenum format,
                                          GLenum type,
                                          GLsizei bufSize,
                                          const void* pixels) {
  driver_->fn.glTexImage2DRobustANGLEFn(target, level, internalformat, width,
                                        height, border, format, type, bufSize,
                                        pixels);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexImage3DFn(GLenum target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLsizei depth,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               const void* pixels) {
  driver_->fn.glTexImage3DFn(target, level, internalformat, width, height,
                             depth, border, format, type, pixels);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexImage3DRobustANGLEFn(GLenum target,
                                          GLint level,
                                          GLint internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLsizei depth,
                                          GLint border,
                                          GLenum format,
                                          GLenum type,
                                          GLsizei bufSize,
                                          const void* pixels) {
  driver_->fn.glTexImage3DRobustANGLEFn(target, level, internalformat, width,
                                        height, depth, border, format, type,
                                        bufSize, pixels);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexParameterfFn(GLenum target, GLenum pname, GLfloat param) {
  driver_->fn.glTexParameterfFn(target, pname, param);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexParameterfvFn(GLenum target,
                                   GLenum pname,
                                   const GLfloat* params) {
  driver_->fn.glTexParameterfvFn(target, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexParameterfvRobustANGLEFn(GLenum target,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              const GLfloat* params) {
  driver_->fn.glTexParameterfvRobustANGLEFn(target, pname, bufSize, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexParameteriFn(GLenum target, GLenum pname, GLint param) {
  driver_->fn.glTexParameteriFn(target, pname, param);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexParameterIivRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLint* params) {
  driver_->fn.glTexParameterIivRobustANGLEFn(target, pname, bufSize, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexParameterIuivRobustANGLEFn(GLenum target,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                const GLuint* params) {
  driver_->fn.glTexParameterIuivRobustANGLEFn(target, pname, bufSize, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexParameterivFn(GLenum target,
                                   GLenum pname,
                                   const GLint* params) {
  driver_->fn.glTexParameterivFn(target, pname, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexParameterivRobustANGLEFn(GLenum target,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              const GLint* params) {
  driver_->fn.glTexParameterivRobustANGLEFn(target, pname, bufSize, params);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexStorage2DEXTFn(GLenum target,
                                    GLsizei levels,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height) {
  driver_->fn.glTexStorage2DEXTFn(target, levels, internalformat, width,
                                  height);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexStorage3DFn(GLenum target,
                                 GLsizei levels,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth) {
  driver_->fn.glTexStorage3DFn(target, levels, internalformat, width, height,
                               depth);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexSubImage2DFn(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLenum format,
                                  GLenum type,
                                  const void* pixels) {
  driver_->fn.glTexSubImage2DFn(target, level, xoffset, yoffset, width, height,
                                format, type, pixels);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexSubImage2DRobustANGLEFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLenum format,
                                             GLenum type,
                                             GLsizei bufSize,
                                             const void* pixels) {
  driver_->fn.glTexSubImage2DRobustANGLEFn(target, level, xoffset, yoffset,
                                           width, height, format, type, bufSize,
                                           pixels);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexSubImage3DFn(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLint zoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLenum format,
                                  GLenum type,
                                  const void* pixels) {
  driver_->fn.glTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, width,
                                height, depth, format, type, pixels);
}

DISABLE_CFI_ICALL
void GLApiBase::glTexSubImage3DRobustANGLEFn(GLenum target,
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
                                             const void* pixels) {
  driver_->fn.glTexSubImage3DRobustANGLEFn(target, level, xoffset, yoffset,
                                           zoffset, width, height, depth,
                                           format, type, bufSize, pixels);
}

DISABLE_CFI_ICALL
void GLApiBase::glTransformFeedbackVaryingsFn(GLuint program,
                                              GLsizei count,
                                              const char* const* varyings,
                                              GLenum bufferMode) {
  driver_->fn.glTransformFeedbackVaryingsFn(program, count, varyings,
                                            bufferMode);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform1fFn(GLint location, GLfloat x) {
  driver_->fn.glUniform1fFn(location, x);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform1fvFn(GLint location,
                               GLsizei count,
                               const GLfloat* v) {
  driver_->fn.glUniform1fvFn(location, count, v);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform1iFn(GLint location, GLint x) {
  driver_->fn.glUniform1iFn(location, x);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform1ivFn(GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform1ivFn(location, count, v);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform1uiFn(GLint location, GLuint v0) {
  driver_->fn.glUniform1uiFn(location, v0);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform1uivFn(GLint location,
                                GLsizei count,
                                const GLuint* v) {
  driver_->fn.glUniform1uivFn(location, count, v);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform2fFn(GLint location, GLfloat x, GLfloat y) {
  driver_->fn.glUniform2fFn(location, x, y);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform2fvFn(GLint location,
                               GLsizei count,
                               const GLfloat* v) {
  driver_->fn.glUniform2fvFn(location, count, v);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform2iFn(GLint location, GLint x, GLint y) {
  driver_->fn.glUniform2iFn(location, x, y);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform2ivFn(GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform2ivFn(location, count, v);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform2uiFn(GLint location, GLuint v0, GLuint v1) {
  driver_->fn.glUniform2uiFn(location, v0, v1);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform2uivFn(GLint location,
                                GLsizei count,
                                const GLuint* v) {
  driver_->fn.glUniform2uivFn(location, count, v);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform3fFn(GLint location, GLfloat x, GLfloat y, GLfloat z) {
  driver_->fn.glUniform3fFn(location, x, y, z);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform3fvFn(GLint location,
                               GLsizei count,
                               const GLfloat* v) {
  driver_->fn.glUniform3fvFn(location, count, v);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform3iFn(GLint location, GLint x, GLint y, GLint z) {
  driver_->fn.glUniform3iFn(location, x, y, z);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform3ivFn(GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform3ivFn(location, count, v);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform3uiFn(GLint location,
                               GLuint v0,
                               GLuint v1,
                               GLuint v2) {
  driver_->fn.glUniform3uiFn(location, v0, v1, v2);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform3uivFn(GLint location,
                                GLsizei count,
                                const GLuint* v) {
  driver_->fn.glUniform3uivFn(location, count, v);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform4fFn(GLint location,
                              GLfloat x,
                              GLfloat y,
                              GLfloat z,
                              GLfloat w) {
  driver_->fn.glUniform4fFn(location, x, y, z, w);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform4fvFn(GLint location,
                               GLsizei count,
                               const GLfloat* v) {
  driver_->fn.glUniform4fvFn(location, count, v);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform4iFn(GLint location,
                              GLint x,
                              GLint y,
                              GLint z,
                              GLint w) {
  driver_->fn.glUniform4iFn(location, x, y, z, w);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform4ivFn(GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform4ivFn(location, count, v);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform4uiFn(GLint location,
                               GLuint v0,
                               GLuint v1,
                               GLuint v2,
                               GLuint v3) {
  driver_->fn.glUniform4uiFn(location, v0, v1, v2, v3);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniform4uivFn(GLint location,
                                GLsizei count,
                                const GLuint* v) {
  driver_->fn.glUniform4uivFn(location, count, v);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniformBlockBindingFn(GLuint program,
                                        GLuint uniformBlockIndex,
                                        GLuint uniformBlockBinding) {
  driver_->fn.glUniformBlockBindingFn(program, uniformBlockIndex,
                                      uniformBlockBinding);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniformMatrix2fvFn(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat* value) {
  driver_->fn.glUniformMatrix2fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniformMatrix2x3fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix2x3fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniformMatrix2x4fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix2x4fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniformMatrix3fvFn(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat* value) {
  driver_->fn.glUniformMatrix3fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniformMatrix3x2fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix3x2fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniformMatrix3x4fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix3x4fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniformMatrix4fvFn(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat* value) {
  driver_->fn.glUniformMatrix4fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniformMatrix4x2fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix4x2fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void GLApiBase::glUniformMatrix4x3fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix4x3fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
GLboolean GLApiBase::glUnmapBufferFn(GLenum target) {
  return driver_->fn.glUnmapBufferFn(target);
}

DISABLE_CFI_ICALL
void GLApiBase::glUseProgramFn(GLuint program) {
  driver_->fn.glUseProgramFn(program);
}

DISABLE_CFI_ICALL
void GLApiBase::glValidateProgramFn(GLuint program) {
  driver_->fn.glValidateProgramFn(program);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttrib1fFn(GLuint indx, GLfloat x) {
  driver_->fn.glVertexAttrib1fFn(indx, x);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib1fvFn(indx, values);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) {
  driver_->fn.glVertexAttrib2fFn(indx, x, y);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib2fvFn(indx, values);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttrib3fFn(GLuint indx,
                                   GLfloat x,
                                   GLfloat y,
                                   GLfloat z) {
  driver_->fn.glVertexAttrib3fFn(indx, x, y, z);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib3fvFn(indx, values);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttrib4fFn(GLuint indx,
                                   GLfloat x,
                                   GLfloat y,
                                   GLfloat z,
                                   GLfloat w) {
  driver_->fn.glVertexAttrib4fFn(indx, x, y, z, w);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib4fvFn(indx, values);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttribDivisorANGLEFn(GLuint index, GLuint divisor) {
  driver_->fn.glVertexAttribDivisorANGLEFn(index, divisor);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttribI4iFn(GLuint indx,
                                    GLint x,
                                    GLint y,
                                    GLint z,
                                    GLint w) {
  driver_->fn.glVertexAttribI4iFn(indx, x, y, z, w);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttribI4ivFn(GLuint indx, const GLint* values) {
  driver_->fn.glVertexAttribI4ivFn(indx, values);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttribI4uiFn(GLuint indx,
                                     GLuint x,
                                     GLuint y,
                                     GLuint z,
                                     GLuint w) {
  driver_->fn.glVertexAttribI4uiFn(indx, x, y, z, w);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttribI4uivFn(GLuint indx, const GLuint* values) {
  driver_->fn.glVertexAttribI4uivFn(indx, values);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttribIPointerFn(GLuint indx,
                                         GLint size,
                                         GLenum type,
                                         GLsizei stride,
                                         const void* ptr) {
  driver_->fn.glVertexAttribIPointerFn(indx, size, type, stride, ptr);
}

DISABLE_CFI_ICALL
void GLApiBase::glVertexAttribPointerFn(GLuint indx,
                                        GLint size,
                                        GLenum type,
                                        GLboolean normalized,
                                        GLsizei stride,
                                        const void* ptr) {
  driver_->fn.glVertexAttribPointerFn(indx, size, type, normalized, stride,
                                      ptr);
}

DISABLE_CFI_ICALL
void GLApiBase::glViewportFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  driver_->fn.glViewportFn(x, y, width, height);
}

DISABLE_CFI_ICALL
void GLApiBase::glWaitSyncFn(GLsync sync, GLbitfield flags, GLuint64 timeout) {
  driver_->fn.glWaitSyncFn(sync, flags, timeout);
}

DISABLE_CFI_ICALL
void GLApiBase::glWindowRectanglesEXTFn(GLenum mode,
                                        GLsizei n,
                                        const GLint* box) {
  driver_->fn.glWindowRectanglesEXTFn(mode, n, box);
}

DISABLE_CFI_ICALL
void TraceGLApi::glActiveTextureFn(GLenum texture) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glActiveTexture")
  gl_api_->glActiveTextureFn(texture);
}

DISABLE_CFI_ICALL
void TraceGLApi::glApplyFramebufferAttachmentCMAAINTELFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glApplyFramebufferAttachmentCMAAINTEL")
  gl_api_->glApplyFramebufferAttachmentCMAAINTELFn();
}

DISABLE_CFI_ICALL
void TraceGLApi::glAttachShaderFn(GLuint program, GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glAttachShader")
  gl_api_->glAttachShaderFn(program, shader);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBeginQueryFn(GLenum target, GLuint id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBeginQuery")
  gl_api_->glBeginQueryFn(target, id);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBeginTransformFeedbackFn(GLenum primitiveMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBeginTransformFeedback")
  gl_api_->glBeginTransformFeedbackFn(primitiveMode);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBindAttribLocationFn(GLuint program,
                                        GLuint index,
                                        const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindAttribLocation")
  gl_api_->glBindAttribLocationFn(program, index, name);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBindBufferFn(GLenum target, GLuint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindBuffer")
  gl_api_->glBindBufferFn(target, buffer);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBindBufferBaseFn(GLenum target,
                                    GLuint index,
                                    GLuint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindBufferBase")
  gl_api_->glBindBufferBaseFn(target, index, buffer);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBindBufferRangeFn(GLenum target,
                                     GLuint index,
                                     GLuint buffer,
                                     GLintptr offset,
                                     GLsizeiptr size) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindBufferRange")
  gl_api_->glBindBufferRangeFn(target, index, buffer, offset, size);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBindFragDataLocationFn(GLuint program,
                                          GLuint colorNumber,
                                          const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindFragDataLocation")
  gl_api_->glBindFragDataLocationFn(program, colorNumber, name);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBindFragDataLocationIndexedFn(GLuint program,
                                                 GLuint colorNumber,
                                                 GLuint index,
                                                 const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glBindFragDataLocationIndexed")
  gl_api_->glBindFragDataLocationIndexedFn(program, colorNumber, index, name);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindFramebufferEXT")
  gl_api_->glBindFramebufferEXTFn(target, framebuffer);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBindImageTextureEXTFn(GLuint index,
                                         GLuint texture,
                                         GLint level,
                                         GLboolean layered,
                                         GLint layer,
                                         GLenum access,
                                         GLint format) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindImageTextureEXT")
  gl_api_->glBindImageTextureEXTFn(index, texture, level, layered, layer,
                                   access, format);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBindRenderbufferEXTFn(GLenum target, GLuint renderbuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindRenderbufferEXT")
  gl_api_->glBindRenderbufferEXTFn(target, renderbuffer);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBindSamplerFn(GLuint unit, GLuint sampler) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindSampler")
  gl_api_->glBindSamplerFn(unit, sampler);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBindTextureFn(GLenum target, GLuint texture) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindTexture")
  gl_api_->glBindTextureFn(target, texture);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBindTransformFeedbackFn(GLenum target, GLuint id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindTransformFeedback")
  gl_api_->glBindTransformFeedbackFn(target, id);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBindUniformLocationCHROMIUMFn(GLuint program,
                                                 GLint location,
                                                 const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glBindUniformLocationCHROMIUM")
  gl_api_->glBindUniformLocationCHROMIUMFn(program, location, name);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBindVertexArrayOESFn(GLuint array) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindVertexArrayOES")
  gl_api_->glBindVertexArrayOESFn(array);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBlendBarrierKHRFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendBarrierKHR")
  gl_api_->glBlendBarrierKHRFn();
}

DISABLE_CFI_ICALL
void TraceGLApi::glBlendColorFn(GLclampf red,
                                GLclampf green,
                                GLclampf blue,
                                GLclampf alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendColor")
  gl_api_->glBlendColorFn(red, green, blue, alpha);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBlendEquationFn(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendEquation")
  gl_api_->glBlendEquationFn(mode);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBlendEquationSeparateFn(GLenum modeRGB, GLenum modeAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendEquationSeparate")
  gl_api_->glBlendEquationSeparateFn(modeRGB, modeAlpha);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBlendFuncFn(GLenum sfactor, GLenum dfactor) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendFunc")
  gl_api_->glBlendFuncFn(sfactor, dfactor);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBlendFuncSeparateFn(GLenum srcRGB,
                                       GLenum dstRGB,
                                       GLenum srcAlpha,
                                       GLenum dstAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendFuncSeparate")
  gl_api_->glBlendFuncSeparateFn(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBlitFramebufferFn(GLint srcX0,
                                     GLint srcY0,
                                     GLint srcX1,
                                     GLint srcY1,
                                     GLint dstX0,
                                     GLint dstY0,
                                     GLint dstX1,
                                     GLint dstY1,
                                     GLbitfield mask,
                                     GLenum filter) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlitFramebuffer")
  gl_api_->glBlitFramebufferFn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1,
                               dstY1, mask, filter);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBufferDataFn(GLenum target,
                                GLsizeiptr size,
                                const void* data,
                                GLenum usage) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBufferData")
  gl_api_->glBufferDataFn(target, size, data, usage);
}

DISABLE_CFI_ICALL
void TraceGLApi::glBufferSubDataFn(GLenum target,
                                   GLintptr offset,
                                   GLsizeiptr size,
                                   const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBufferSubData")
  gl_api_->glBufferSubDataFn(target, offset, size, data);
}

DISABLE_CFI_ICALL
GLenum TraceGLApi::glCheckFramebufferStatusEXTFn(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glCheckFramebufferStatusEXT")
  return gl_api_->glCheckFramebufferStatusEXTFn(target);
}

DISABLE_CFI_ICALL
void TraceGLApi::glClearFn(GLbitfield mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClear")
  gl_api_->glClearFn(mask);
}

DISABLE_CFI_ICALL
void TraceGLApi::glClearBufferfiFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLfloat depth,
                                   GLint stencil) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearBufferfi")
  gl_api_->glClearBufferfiFn(buffer, drawbuffer, depth, stencil);
}

DISABLE_CFI_ICALL
void TraceGLApi::glClearBufferfvFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearBufferfv")
  gl_api_->glClearBufferfvFn(buffer, drawbuffer, value);
}

DISABLE_CFI_ICALL
void TraceGLApi::glClearBufferivFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearBufferiv")
  gl_api_->glClearBufferivFn(buffer, drawbuffer, value);
}

DISABLE_CFI_ICALL
void TraceGLApi::glClearBufferuivFn(GLenum buffer,
                                    GLint drawbuffer,
                                    const GLuint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearBufferuiv")
  gl_api_->glClearBufferuivFn(buffer, drawbuffer, value);
}

DISABLE_CFI_ICALL
void TraceGLApi::glClearColorFn(GLclampf red,
                                GLclampf green,
                                GLclampf blue,
                                GLclampf alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearColor")
  gl_api_->glClearColorFn(red, green, blue, alpha);
}

DISABLE_CFI_ICALL
void TraceGLApi::glClearDepthFn(GLclampd depth) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearDepth")
  gl_api_->glClearDepthFn(depth);
}

DISABLE_CFI_ICALL
void TraceGLApi::glClearDepthfFn(GLclampf depth) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearDepthf")
  gl_api_->glClearDepthfFn(depth);
}

DISABLE_CFI_ICALL
void TraceGLApi::glClearStencilFn(GLint s) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearStencil")
  gl_api_->glClearStencilFn(s);
}

DISABLE_CFI_ICALL
GLenum TraceGLApi::glClientWaitSyncFn(GLsync sync,
                                      GLbitfield flags,
                                      GLuint64 timeout) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClientWaitSync")
  return gl_api_->glClientWaitSyncFn(sync, flags, timeout);
}

DISABLE_CFI_ICALL
void TraceGLApi::glColorMaskFn(GLboolean red,
                               GLboolean green,
                               GLboolean blue,
                               GLboolean alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glColorMask")
  gl_api_->glColorMaskFn(red, green, blue, alpha);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCompileShaderFn(GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompileShader")
  gl_api_->glCompileShaderFn(shader);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCompressedCopyTextureCHROMIUMFn(GLuint sourceId,
                                                   GLuint destId) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glCompressedCopyTextureCHROMIUM")
  gl_api_->glCompressedCopyTextureCHROMIUMFn(sourceId, destId);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCompressedTexImage2DFn(GLenum target,
                                          GLint level,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLint border,
                                          GLsizei imageSize,
                                          const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompressedTexImage2D")
  gl_api_->glCompressedTexImage2DFn(target, level, internalformat, width,
                                    height, border, imageSize, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCompressedTexImage2DRobustANGLEFn(GLenum target,
                                                     GLint level,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLint border,
                                                     GLsizei imageSize,
                                                     GLsizei dataSize,
                                                     const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glCompressedTexImage2DRobustANGLE")
  gl_api_->glCompressedTexImage2DRobustANGLEFn(target, level, internalformat,
                                               width, height, border, imageSize,
                                               dataSize, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCompressedTexImage3DFn(GLenum target,
                                          GLint level,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLsizei depth,
                                          GLint border,
                                          GLsizei imageSize,
                                          const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompressedTexImage3D")
  gl_api_->glCompressedTexImage3DFn(target, level, internalformat, width,
                                    height, depth, border, imageSize, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCompressedTexImage3DRobustANGLEFn(GLenum target,
                                                     GLint level,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLsizei depth,
                                                     GLint border,
                                                     GLsizei imageSize,
                                                     GLsizei dataSize,
                                                     const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glCompressedTexImage3DRobustANGLE")
  gl_api_->glCompressedTexImage3DRobustANGLEFn(target, level, internalformat,
                                               width, height, depth, border,
                                               imageSize, dataSize, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCompressedTexSubImage2DFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLenum format,
                                             GLsizei imageSize,
                                             const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompressedTexSubImage2D")
  gl_api_->glCompressedTexSubImage2DFn(target, level, xoffset, yoffset, width,
                                       height, format, imageSize, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCompressedTexSubImage2DRobustANGLEFn(GLenum target,
                                                        GLint level,
                                                        GLint xoffset,
                                                        GLint yoffset,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLenum format,
                                                        GLsizei imageSize,
                                                        GLsizei dataSize,
                                                        const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glCompressedTexSubImage2DRobustANGLE")
  gl_api_->glCompressedTexSubImage2DRobustANGLEFn(
      target, level, xoffset, yoffset, width, height, format, imageSize,
      dataSize, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCompressedTexSubImage3DFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLint zoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLsizei depth,
                                             GLenum format,
                                             GLsizei imageSize,
                                             const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompressedTexSubImage3D")
  gl_api_->glCompressedTexSubImage3DFn(target, level, xoffset, yoffset, zoffset,
                                       width, height, depth, format, imageSize,
                                       data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCompressedTexSubImage3DRobustANGLEFn(GLenum target,
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
                                                        const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glCompressedTexSubImage3DRobustANGLE")
  gl_api_->glCompressedTexSubImage3DRobustANGLEFn(
      target, level, xoffset, yoffset, zoffset, width, height, depth, format,
      imageSize, dataSize, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCopyBufferSubDataFn(GLenum readTarget,
                                       GLenum writeTarget,
                                       GLintptr readOffset,
                                       GLintptr writeOffset,
                                       GLsizeiptr size) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyBufferSubData")
  gl_api_->glCopyBufferSubDataFn(readTarget, writeTarget, readOffset,
                                 writeOffset, size);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCopySubTextureCHROMIUMFn(GLuint sourceId,
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
                                            GLboolean unpackUnmultiplyAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopySubTextureCHROMIUM")
  gl_api_->glCopySubTextureCHROMIUMFn(
      sourceId, sourceLevel, destTarget, destId, destLevel, xoffset, yoffset, x,
      y, width, height, unpackFlipY, unpackPremultiplyAlpha,
      unpackUnmultiplyAlpha);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCopyTexImage2DFn(GLenum target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLint border) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyTexImage2D")
  gl_api_->glCopyTexImage2DFn(target, level, internalformat, x, y, width,
                              height, border);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCopyTexSubImage2DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyTexSubImage2D")
  gl_api_->glCopyTexSubImage2DFn(target, level, xoffset, yoffset, x, y, width,
                                 height);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCopyTexSubImage3DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyTexSubImage3D")
  gl_api_->glCopyTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, x, y,
                                 width, height);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCopyTextureCHROMIUMFn(GLuint sourceId,
                                         GLint sourceLevel,
                                         GLenum destTarget,
                                         GLuint destId,
                                         GLint destLevel,
                                         GLint internalFormat,
                                         GLenum destType,
                                         GLboolean unpackFlipY,
                                         GLboolean unpackPremultiplyAlpha,
                                         GLboolean unpackUnmultiplyAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyTextureCHROMIUM")
  gl_api_->glCopyTextureCHROMIUMFn(
      sourceId, sourceLevel, destTarget, destId, destLevel, internalFormat,
      destType, unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCoverageModulationNVFn(GLenum components) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCoverageModulationNV")
  gl_api_->glCoverageModulationNVFn(components);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCoverFillPathInstancedNVFn(GLsizei numPaths,
                                              GLenum pathNameType,
                                              const void* paths,
                                              GLuint pathBase,
                                              GLenum coverMode,
                                              GLenum transformType,
                                              const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCoverFillPathInstancedNV")
  gl_api_->glCoverFillPathInstancedNVFn(numPaths, pathNameType, paths, pathBase,
                                        coverMode, transformType,
                                        transformValues);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCoverFillPathNVFn(GLuint path, GLenum coverMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCoverFillPathNV")
  gl_api_->glCoverFillPathNVFn(path, coverMode);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glCoverStrokePathInstancedNV")
  gl_api_->glCoverStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                          pathBase, coverMode, transformType,
                                          transformValues);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCoverStrokePathNVFn(GLuint name, GLenum coverMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCoverStrokePathNV")
  gl_api_->glCoverStrokePathNVFn(name, coverMode);
}

DISABLE_CFI_ICALL
GLuint TraceGLApi::glCreateProgramFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCreateProgram")
  return gl_api_->glCreateProgramFn();
}

DISABLE_CFI_ICALL
GLuint TraceGLApi::glCreateShaderFn(GLenum type) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCreateShader")
  return gl_api_->glCreateShaderFn(type);
}

DISABLE_CFI_ICALL
void TraceGLApi::glCullFaceFn(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCullFace")
  gl_api_->glCullFaceFn(mode);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDebugMessageCallbackFn(GLDEBUGPROC callback,
                                          const void* userParam) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDebugMessageCallback")
  gl_api_->glDebugMessageCallbackFn(callback, userParam);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDebugMessageControlFn(GLenum source,
                                         GLenum type,
                                         GLenum severity,
                                         GLsizei count,
                                         const GLuint* ids,
                                         GLboolean enabled) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDebugMessageControl")
  gl_api_->glDebugMessageControlFn(source, type, severity, count, ids, enabled);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDebugMessageInsertFn(GLenum source,
                                        GLenum type,
                                        GLuint id,
                                        GLenum severity,
                                        GLsizei length,
                                        const char* buf) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDebugMessageInsert")
  gl_api_->glDebugMessageInsertFn(source, type, id, severity, length, buf);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteBuffersARB")
  gl_api_->glDeleteBuffersARBFn(n, buffers);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDeleteFencesAPPLEFn(GLsizei n, const GLuint* fences) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteFencesAPPLE")
  gl_api_->glDeleteFencesAPPLEFn(n, fences);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDeleteFencesNVFn(GLsizei n, const GLuint* fences) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteFencesNV")
  gl_api_->glDeleteFencesNVFn(n, fences);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDeleteFramebuffersEXTFn(GLsizei n,
                                           const GLuint* framebuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteFramebuffersEXT")
  gl_api_->glDeleteFramebuffersEXTFn(n, framebuffers);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDeletePathsNVFn(GLuint path, GLsizei range) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeletePathsNV")
  gl_api_->glDeletePathsNVFn(path, range);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDeleteProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteProgram")
  gl_api_->glDeleteProgramFn(program);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDeleteQueriesFn(GLsizei n, const GLuint* ids) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteQueries")
  gl_api_->glDeleteQueriesFn(n, ids);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDeleteRenderbuffersEXTFn(GLsizei n,
                                            const GLuint* renderbuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteRenderbuffersEXT")
  gl_api_->glDeleteRenderbuffersEXTFn(n, renderbuffers);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDeleteSamplersFn(GLsizei n, const GLuint* samplers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteSamplers")
  gl_api_->glDeleteSamplersFn(n, samplers);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDeleteShaderFn(GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteShader")
  gl_api_->glDeleteShaderFn(shader);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDeleteSyncFn(GLsync sync) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteSync")
  gl_api_->glDeleteSyncFn(sync);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDeleteTexturesFn(GLsizei n, const GLuint* textures) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteTextures")
  gl_api_->glDeleteTexturesFn(n, textures);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDeleteTransformFeedbacksFn(GLsizei n, const GLuint* ids) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteTransformFeedbacks")
  gl_api_->glDeleteTransformFeedbacksFn(n, ids);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDeleteVertexArraysOESFn(GLsizei n, const GLuint* arrays) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteVertexArraysOES")
  gl_api_->glDeleteVertexArraysOESFn(n, arrays);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDepthFuncFn(GLenum func) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDepthFunc")
  gl_api_->glDepthFuncFn(func);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDepthMaskFn(GLboolean flag) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDepthMask")
  gl_api_->glDepthMaskFn(flag);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDepthRangeFn(GLclampd zNear, GLclampd zFar) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDepthRange")
  gl_api_->glDepthRangeFn(zNear, zFar);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDepthRangefFn(GLclampf zNear, GLclampf zFar) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDepthRangef")
  gl_api_->glDepthRangefFn(zNear, zFar);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDetachShaderFn(GLuint program, GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDetachShader")
  gl_api_->glDetachShaderFn(program, shader);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDisableFn(GLenum cap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDisable")
  gl_api_->glDisableFn(cap);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDisableVertexAttribArrayFn(GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDisableVertexAttribArray")
  gl_api_->glDisableVertexAttribArrayFn(index);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDiscardFramebufferEXTFn(GLenum target,
                                           GLsizei numAttachments,
                                           const GLenum* attachments) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDiscardFramebufferEXT")
  gl_api_->glDiscardFramebufferEXTFn(target, numAttachments, attachments);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawArrays")
  gl_api_->glDrawArraysFn(mode, first, count);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDrawArraysInstancedANGLEFn(GLenum mode,
                                              GLint first,
                                              GLsizei count,
                                              GLsizei primcount) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawArraysInstancedANGLE")
  gl_api_->glDrawArraysInstancedANGLEFn(mode, first, count, primcount);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDrawBufferFn(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawBuffer")
  gl_api_->glDrawBufferFn(mode);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawBuffersARB")
  gl_api_->glDrawBuffersARBFn(n, bufs);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDrawElementsFn(GLenum mode,
                                  GLsizei count,
                                  GLenum type,
                                  const void* indices) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawElements")
  gl_api_->glDrawElementsFn(mode, count, type, indices);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDrawElementsInstancedANGLEFn(GLenum mode,
                                                GLsizei count,
                                                GLenum type,
                                                const void* indices,
                                                GLsizei primcount) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glDrawElementsInstancedANGLE")
  gl_api_->glDrawElementsInstancedANGLEFn(mode, count, type, indices,
                                          primcount);
}

DISABLE_CFI_ICALL
void TraceGLApi::glDrawRangeElementsFn(GLenum mode,
                                       GLuint start,
                                       GLuint end,
                                       GLsizei count,
                                       GLenum type,
                                       const void* indices) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawRangeElements")
  gl_api_->glDrawRangeElementsFn(mode, start, end, count, type, indices);
}

DISABLE_CFI_ICALL
void TraceGLApi::glEGLImageTargetRenderbufferStorageOESFn(GLenum target,
                                                          GLeglImageOES image) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glEGLImageTargetRenderbufferStorageOES")
  gl_api_->glEGLImageTargetRenderbufferStorageOESFn(target, image);
}

DISABLE_CFI_ICALL
void TraceGLApi::glEGLImageTargetTexture2DOESFn(GLenum target,
                                                GLeglImageOES image) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glEGLImageTargetTexture2DOES")
  gl_api_->glEGLImageTargetTexture2DOESFn(target, image);
}

DISABLE_CFI_ICALL
void TraceGLApi::glEnableFn(GLenum cap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glEnable")
  gl_api_->glEnableFn(cap);
}

DISABLE_CFI_ICALL
void TraceGLApi::glEnableVertexAttribArrayFn(GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glEnableVertexAttribArray")
  gl_api_->glEnableVertexAttribArrayFn(index);
}

DISABLE_CFI_ICALL
void TraceGLApi::glEndQueryFn(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glEndQuery")
  gl_api_->glEndQueryFn(target);
}

DISABLE_CFI_ICALL
void TraceGLApi::glEndTransformFeedbackFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glEndTransformFeedback")
  gl_api_->glEndTransformFeedbackFn();
}

DISABLE_CFI_ICALL
GLsync TraceGLApi::glFenceSyncFn(GLenum condition, GLbitfield flags) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFenceSync")
  return gl_api_->glFenceSyncFn(condition, flags);
}

DISABLE_CFI_ICALL
void TraceGLApi::glFinishFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFinish")
  gl_api_->glFinishFn();
}

DISABLE_CFI_ICALL
void TraceGLApi::glFinishFenceAPPLEFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFinishFenceAPPLE")
  gl_api_->glFinishFenceAPPLEFn(fence);
}

DISABLE_CFI_ICALL
void TraceGLApi::glFinishFenceNVFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFinishFenceNV")
  gl_api_->glFinishFenceNVFn(fence);
}

DISABLE_CFI_ICALL
void TraceGLApi::glFlushFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFlush")
  gl_api_->glFlushFn();
}

DISABLE_CFI_ICALL
void TraceGLApi::glFlushMappedBufferRangeFn(GLenum target,
                                            GLintptr offset,
                                            GLsizeiptr length) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFlushMappedBufferRange")
  gl_api_->glFlushMappedBufferRangeFn(target, offset, length);
}

DISABLE_CFI_ICALL
void TraceGLApi::glFramebufferRenderbufferEXTFn(GLenum target,
                                                GLenum attachment,
                                                GLenum renderbuffertarget,
                                                GLuint renderbuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glFramebufferRenderbufferEXT")
  gl_api_->glFramebufferRenderbufferEXTFn(target, attachment,
                                          renderbuffertarget, renderbuffer);
}

DISABLE_CFI_ICALL
void TraceGLApi::glFramebufferTexture2DEXTFn(GLenum target,
                                             GLenum attachment,
                                             GLenum textarget,
                                             GLuint texture,
                                             GLint level) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFramebufferTexture2DEXT")
  gl_api_->glFramebufferTexture2DEXTFn(target, attachment, textarget, texture,
                                       level);
}

DISABLE_CFI_ICALL
void TraceGLApi::glFramebufferTexture2DMultisampleEXTFn(GLenum target,
                                                        GLenum attachment,
                                                        GLenum textarget,
                                                        GLuint texture,
                                                        GLint level,
                                                        GLsizei samples) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glFramebufferTexture2DMultisampleEXT")
  gl_api_->glFramebufferTexture2DMultisampleEXTFn(target, attachment, textarget,
                                                  texture, level, samples);
}

DISABLE_CFI_ICALL
void TraceGLApi::glFramebufferTextureLayerFn(GLenum target,
                                             GLenum attachment,
                                             GLuint texture,
                                             GLint level,
                                             GLint layer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFramebufferTextureLayer")
  gl_api_->glFramebufferTextureLayerFn(target, attachment, texture, level,
                                       layer);
}

DISABLE_CFI_ICALL
void TraceGLApi::glFrontFaceFn(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFrontFace")
  gl_api_->glFrontFaceFn(mode);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGenBuffersARBFn(GLsizei n, GLuint* buffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenBuffersARB")
  gl_api_->glGenBuffersARBFn(n, buffers);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGenerateMipmapEXTFn(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenerateMipmapEXT")
  gl_api_->glGenerateMipmapEXTFn(target);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGenFencesAPPLEFn(GLsizei n, GLuint* fences) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenFencesAPPLE")
  gl_api_->glGenFencesAPPLEFn(n, fences);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGenFencesNVFn(GLsizei n, GLuint* fences) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenFencesNV")
  gl_api_->glGenFencesNVFn(n, fences);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenFramebuffersEXT")
  gl_api_->glGenFramebuffersEXTFn(n, framebuffers);
}

DISABLE_CFI_ICALL
GLuint TraceGLApi::glGenPathsNVFn(GLsizei range) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenPathsNV")
  return gl_api_->glGenPathsNVFn(range);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGenQueriesFn(GLsizei n, GLuint* ids) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenQueries")
  gl_api_->glGenQueriesFn(n, ids);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenRenderbuffersEXT")
  gl_api_->glGenRenderbuffersEXTFn(n, renderbuffers);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGenSamplersFn(GLsizei n, GLuint* samplers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenSamplers")
  gl_api_->glGenSamplersFn(n, samplers);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGenTexturesFn(GLsizei n, GLuint* textures) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenTextures")
  gl_api_->glGenTexturesFn(n, textures);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenTransformFeedbacks")
  gl_api_->glGenTransformFeedbacksFn(n, ids);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenVertexArraysOES")
  gl_api_->glGenVertexArraysOESFn(n, arrays);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetActiveAttribFn(GLuint program,
                                     GLuint index,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     GLint* size,
                                     GLenum* type,
                                     char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetActiveAttrib")
  gl_api_->glGetActiveAttribFn(program, index, bufsize, length, size, type,
                               name);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetActiveUniformFn(GLuint program,
                                      GLuint index,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      GLint* size,
                                      GLenum* type,
                                      char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetActiveUniform")
  gl_api_->glGetActiveUniformFn(program, index, bufsize, length, size, type,
                                name);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetActiveUniformBlockivFn(GLuint program,
                                             GLuint uniformBlockIndex,
                                             GLenum pname,
                                             GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetActiveUniformBlockiv")
  gl_api_->glGetActiveUniformBlockivFn(program, uniformBlockIndex, pname,
                                       params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetActiveUniformBlockivRobustANGLEFn(
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetActiveUniformBlockivRobustANGLE")
  gl_api_->glGetActiveUniformBlockivRobustANGLEFn(
      program, uniformBlockIndex, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetActiveUniformBlockNameFn(GLuint program,
                                               GLuint uniformBlockIndex,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               char* uniformBlockName) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetActiveUniformBlockName")
  gl_api_->glGetActiveUniformBlockNameFn(program, uniformBlockIndex, bufSize,
                                         length, uniformBlockName);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetActiveUniformsivFn(GLuint program,
                                         GLsizei uniformCount,
                                         const GLuint* uniformIndices,
                                         GLenum pname,
                                         GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetActiveUniformsiv")
  gl_api_->glGetActiveUniformsivFn(program, uniformCount, uniformIndices, pname,
                                   params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetAttachedShadersFn(GLuint program,
                                        GLsizei maxcount,
                                        GLsizei* count,
                                        GLuint* shaders) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetAttachedShaders")
  gl_api_->glGetAttachedShadersFn(program, maxcount, count, shaders);
}

DISABLE_CFI_ICALL
GLint TraceGLApi::glGetAttribLocationFn(GLuint program, const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetAttribLocation")
  return gl_api_->glGetAttribLocationFn(program, name);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetBooleani_vRobustANGLEFn(GLenum target,
                                              GLuint index,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLboolean* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetBooleani_vRobustANGLE")
  gl_api_->glGetBooleani_vRobustANGLEFn(target, index, bufSize, length, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetBooleanvFn(GLenum pname, GLboolean* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetBooleanv")
  gl_api_->glGetBooleanvFn(pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetBooleanvRobustANGLEFn(GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLboolean* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetBooleanvRobustANGLE")
  gl_api_->glGetBooleanvRobustANGLEFn(pname, bufSize, length, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetBufferParameteri64vRobustANGLEFn(GLenum target,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetBufferParameteri64vRobustANGLE")
  gl_api_->glGetBufferParameteri64vRobustANGLEFn(target, pname, bufSize, length,
                                                 params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetBufferParameterivFn(GLenum target,
                                          GLenum pname,
                                          GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetBufferParameteriv")
  gl_api_->glGetBufferParameterivFn(target, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetBufferParameterivRobustANGLEFn(GLenum target,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetBufferParameterivRobustANGLE")
  gl_api_->glGetBufferParameterivRobustANGLEFn(target, pname, bufSize, length,
                                               params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetBufferPointervRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  void** params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetBufferPointervRobustANGLE")
  gl_api_->glGetBufferPointervRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetDebugMessageLogFn(GLuint count,
                                        GLsizei bufSize,
                                        GLenum* sources,
                                        GLenum* types,
                                        GLuint* ids,
                                        GLenum* severities,
                                        GLsizei* lengths,
                                        char* messageLog) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetDebugMessageLog")
  gl_api_->glGetDebugMessageLogFn(count, bufSize, sources, types, ids,
                                  severities, lengths, messageLog);
}

DISABLE_CFI_ICALL
GLenum TraceGLApi::glGetErrorFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetError")
  return gl_api_->glGetErrorFn();
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetFenceivNVFn(GLuint fence, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFenceivNV")
  gl_api_->glGetFenceivNVFn(fence, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetFloatvFn(GLenum pname, GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFloatv")
  gl_api_->glGetFloatvFn(pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetFloatvRobustANGLEFn(GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLfloat* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFloatvRobustANGLE")
  gl_api_->glGetFloatvRobustANGLEFn(pname, bufSize, length, data);
}

DISABLE_CFI_ICALL
GLint TraceGLApi::glGetFragDataIndexFn(GLuint program, const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFragDataIndex")
  return gl_api_->glGetFragDataIndexFn(program, name);
}

DISABLE_CFI_ICALL
GLint TraceGLApi::glGetFragDataLocationFn(GLuint program, const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFragDataLocation")
  return gl_api_->glGetFragDataLocationFn(program, name);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetFramebufferAttachmentParameterivEXTFn(GLenum target,
                                                            GLenum attachment,
                                                            GLenum pname,
                                                            GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetFramebufferAttachmentParameterivEXT")
  gl_api_->glGetFramebufferAttachmentParameterivEXTFn(target, attachment, pname,
                                                      params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetFramebufferAttachmentParameterivRobustANGLEFn(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetFramebufferAttachmentParameterivRobustANGLE")
  gl_api_->glGetFramebufferAttachmentParameterivRobustANGLEFn(
      target, attachment, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetFramebufferParameterivRobustANGLEFn(GLenum target,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetFramebufferParameterivRobustANGLE")
  gl_api_->glGetFramebufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                    length, params);
}

DISABLE_CFI_ICALL
GLenum TraceGLApi::glGetGraphicsResetStatusARBFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetGraphicsResetStatusARB")
  return gl_api_->glGetGraphicsResetStatusARBFn();
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetInteger64i_vFn(GLenum target,
                                     GLuint index,
                                     GLint64* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetInteger64i_v")
  gl_api_->glGetInteger64i_vFn(target, index, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetInteger64i_vRobustANGLEFn(GLenum target,
                                                GLuint index,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint64* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetInteger64i_vRobustANGLE")
  gl_api_->glGetInteger64i_vRobustANGLEFn(target, index, bufSize, length, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetInteger64vFn(GLenum pname, GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetInteger64v")
  gl_api_->glGetInteger64vFn(pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetInteger64vRobustANGLEFn(GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint64* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetInteger64vRobustANGLE")
  gl_api_->glGetInteger64vRobustANGLEFn(pname, bufSize, length, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetIntegeri_vFn(GLenum target, GLuint index, GLint* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetIntegeri_v")
  gl_api_->glGetIntegeri_vFn(target, index, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetIntegeri_vRobustANGLEFn(GLenum target,
                                              GLuint index,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetIntegeri_vRobustANGLE")
  gl_api_->glGetIntegeri_vRobustANGLEFn(target, index, bufSize, length, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetIntegervFn(GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetIntegerv")
  gl_api_->glGetIntegervFn(pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetIntegervRobustANGLEFn(GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetIntegervRobustANGLE")
  gl_api_->glGetIntegervRobustANGLEFn(pname, bufSize, length, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetInternalformativFn(GLenum target,
                                         GLenum internalformat,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetInternalformativ")
  gl_api_->glGetInternalformativFn(target, internalformat, pname, bufSize,
                                   params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetInternalformativRobustANGLEFn(GLenum target,
                                                    GLenum internalformat,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetInternalformativRobustANGLE")
  gl_api_->glGetInternalformativRobustANGLEFn(target, internalformat, pname,
                                              bufSize, length, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetMultisamplefvFn(GLenum pname,
                                      GLuint index,
                                      GLfloat* val) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetMultisamplefv")
  gl_api_->glGetMultisamplefvFn(pname, index, val);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetMultisamplefvRobustANGLEFn(GLenum pname,
                                                 GLuint index,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* val) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetMultisamplefvRobustANGLE")
  gl_api_->glGetMultisamplefvRobustANGLEFn(pname, index, bufSize, length, val);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetnUniformfvRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetnUniformfvRobustANGLE")
  gl_api_->glGetnUniformfvRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetnUniformivRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetnUniformivRobustANGLE")
  gl_api_->glGetnUniformivRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetnUniformuivRobustANGLEFn(GLuint program,
                                               GLint location,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetnUniformuivRobustANGLE")
  gl_api_->glGetnUniformuivRobustANGLEFn(program, location, bufSize, length,
                                         params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetObjectLabelFn(GLenum identifier,
                                    GLuint name,
                                    GLsizei bufSize,
                                    GLsizei* length,
                                    char* label) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetObjectLabel")
  gl_api_->glGetObjectLabelFn(identifier, name, bufSize, length, label);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetObjectPtrLabelFn(void* ptr,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       char* label) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetObjectPtrLabel")
  gl_api_->glGetObjectPtrLabelFn(ptr, bufSize, length, label);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetPointervFn(GLenum pname, void** params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetPointerv")
  gl_api_->glGetPointervFn(pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetPointervRobustANGLERobustANGLEFn(GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       void** params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetPointervRobustANGLERobustANGLE")
  gl_api_->glGetPointervRobustANGLERobustANGLEFn(pname, bufSize, length,
                                                 params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetProgramBinaryFn(GLuint program,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLenum* binaryFormat,
                                      GLvoid* binary) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramBinary")
  gl_api_->glGetProgramBinaryFn(program, bufSize, length, binaryFormat, binary);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetProgramInfoLogFn(GLuint program,
                                       GLsizei bufsize,
                                       GLsizei* length,
                                       char* infolog) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramInfoLog")
  gl_api_->glGetProgramInfoLogFn(program, bufsize, length, infolog);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetProgramInterfaceivFn(GLuint program,
                                           GLenum programInterface,
                                           GLenum pname,
                                           GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramInterfaceiv")
  gl_api_->glGetProgramInterfaceivFn(program, programInterface, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetProgramInterfaceivRobustANGLEFn(GLuint program,
                                                      GLenum programInterface,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetProgramInterfaceivRobustANGLE")
  gl_api_->glGetProgramInterfaceivRobustANGLEFn(program, programInterface,
                                                pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetProgramivFn(GLuint program, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramiv")
  gl_api_->glGetProgramivFn(program, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetProgramivRobustANGLEFn(GLuint program,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramivRobustANGLE")
  gl_api_->glGetProgramivRobustANGLEFn(program, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetProgramResourceivFn(GLuint program,
                                          GLenum programInterface,
                                          GLuint index,
                                          GLsizei propCount,
                                          const GLenum* props,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramResourceiv")
  gl_api_->glGetProgramResourceivFn(program, programInterface, index, propCount,
                                    props, bufSize, length, params);
}

DISABLE_CFI_ICALL
GLint TraceGLApi::glGetProgramResourceLocationFn(GLuint program,
                                                 GLenum programInterface,
                                                 const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetProgramResourceLocation")
  return gl_api_->glGetProgramResourceLocationFn(program, programInterface,
                                                 name);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetProgramResourceNameFn(GLuint program,
                                            GLenum programInterface,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLchar* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramResourceName")
  gl_api_->glGetProgramResourceNameFn(program, programInterface, index, bufSize,
                                      length, name);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetQueryivFn(GLenum target, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryiv")
  gl_api_->glGetQueryivFn(target, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetQueryivRobustANGLEFn(GLenum target,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryivRobustANGLE")
  gl_api_->glGetQueryivRobustANGLEFn(target, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetQueryObjecti64vFn(GLuint id,
                                        GLenum pname,
                                        GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryObjecti64v")
  gl_api_->glGetQueryObjecti64vFn(id, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetQueryObjecti64vRobustANGLEFn(GLuint id,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetQueryObjecti64vRobustANGLE")
  gl_api_->glGetQueryObjecti64vRobustANGLEFn(id, pname, bufSize, length,
                                             params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetQueryObjectivFn(GLuint id, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryObjectiv")
  gl_api_->glGetQueryObjectivFn(id, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetQueryObjectivRobustANGLEFn(GLuint id,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetQueryObjectivRobustANGLE")
  gl_api_->glGetQueryObjectivRobustANGLEFn(id, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetQueryObjectui64vFn(GLuint id,
                                         GLenum pname,
                                         GLuint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryObjectui64v")
  gl_api_->glGetQueryObjectui64vFn(id, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetQueryObjectui64vRobustANGLEFn(GLuint id,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetQueryObjectui64vRobustANGLE")
  gl_api_->glGetQueryObjectui64vRobustANGLEFn(id, pname, bufSize, length,
                                              params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetQueryObjectuivFn(GLuint id,
                                       GLenum pname,
                                       GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryObjectuiv")
  gl_api_->glGetQueryObjectuivFn(id, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetQueryObjectuivRobustANGLEFn(GLuint id,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetQueryObjectuivRobustANGLE")
  gl_api_->glGetQueryObjectuivRobustANGLEFn(id, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetRenderbufferParameterivEXTFn(GLenum target,
                                                   GLenum pname,
                                                   GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetRenderbufferParameterivEXT")
  gl_api_->glGetRenderbufferParameterivEXTFn(target, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetRenderbufferParameterivRobustANGLEFn(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetRenderbufferParameterivRobustANGLE")
  gl_api_->glGetRenderbufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                     length, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetSamplerParameterfvFn(GLuint sampler,
                                           GLenum pname,
                                           GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetSamplerParameterfv")
  gl_api_->glGetSamplerParameterfvFn(sampler, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetSamplerParameterfvRobustANGLE")
  gl_api_->glGetSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize, length,
                                                params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetSamplerParameterIivRobustANGLE")
  gl_api_->glGetSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize,
                                                 length, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetSamplerParameterIuivRobustANGLE")
  gl_api_->glGetSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize,
                                                  length, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetSamplerParameterivFn(GLuint sampler,
                                           GLenum pname,
                                           GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetSamplerParameteriv")
  gl_api_->glGetSamplerParameterivFn(sampler, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetSamplerParameterivRobustANGLE")
  gl_api_->glGetSamplerParameterivRobustANGLEFn(sampler, pname, bufSize, length,
                                                params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetShaderInfoLogFn(GLuint shader,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      char* infolog) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderInfoLog")
  gl_api_->glGetShaderInfoLogFn(shader, bufsize, length, infolog);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetShaderivFn(GLuint shader, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderiv")
  gl_api_->glGetShaderivFn(shader, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetShaderivRobustANGLEFn(GLuint shader,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderivRobustANGLE")
  gl_api_->glGetShaderivRobustANGLEFn(shader, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetShaderPrecisionFormatFn(GLenum shadertype,
                                              GLenum precisiontype,
                                              GLint* range,
                                              GLint* precision) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderPrecisionFormat")
  gl_api_->glGetShaderPrecisionFormatFn(shadertype, precisiontype, range,
                                        precision);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetShaderSourceFn(GLuint shader,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     char* source) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderSource")
  gl_api_->glGetShaderSourceFn(shader, bufsize, length, source);
}

DISABLE_CFI_ICALL
const GLubyte* TraceGLApi::glGetStringFn(GLenum name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetString")
  return gl_api_->glGetStringFn(name);
}

DISABLE_CFI_ICALL
const GLubyte* TraceGLApi::glGetStringiFn(GLenum name, GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetStringi")
  return gl_api_->glGetStringiFn(name, index);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetSyncivFn(GLsync sync,
                               GLenum pname,
                               GLsizei bufSize,
                               GLsizei* length,
                               GLint* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetSynciv")
  gl_api_->glGetSyncivFn(sync, pname, bufSize, length, values);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetTexLevelParameterfvFn(GLenum target,
                                            GLint level,
                                            GLenum pname,
                                            GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetTexLevelParameterfv")
  gl_api_->glGetTexLevelParameterfvFn(target, level, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetTexLevelParameterfvRobustANGLEFn(GLenum target,
                                                       GLint level,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetTexLevelParameterfvRobustANGLE")
  gl_api_->glGetTexLevelParameterfvRobustANGLEFn(target, level, pname, bufSize,
                                                 length, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetTexLevelParameterivFn(GLenum target,
                                            GLint level,
                                            GLenum pname,
                                            GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetTexLevelParameteriv")
  gl_api_->glGetTexLevelParameterivFn(target, level, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetTexLevelParameterivRobustANGLEFn(GLenum target,
                                                       GLint level,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetTexLevelParameterivRobustANGLE")
  gl_api_->glGetTexLevelParameterivRobustANGLEFn(target, level, pname, bufSize,
                                                 length, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetTexParameterfvFn(GLenum target,
                                       GLenum pname,
                                       GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetTexParameterfv")
  gl_api_->glGetTexParameterfvFn(target, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetTexParameterfvRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTexParameterfvRobustANGLE")
  gl_api_->glGetTexParameterfvRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetTexParameterIivRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTexParameterIivRobustANGLE")
  gl_api_->glGetTexParameterIivRobustANGLEFn(target, pname, bufSize, length,
                                             params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetTexParameterIuivRobustANGLEFn(GLenum target,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTexParameterIuivRobustANGLE")
  gl_api_->glGetTexParameterIuivRobustANGLEFn(target, pname, bufSize, length,
                                              params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetTexParameterivFn(GLenum target,
                                       GLenum pname,
                                       GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetTexParameteriv")
  gl_api_->glGetTexParameterivFn(target, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetTexParameterivRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTexParameterivRobustANGLE")
  gl_api_->glGetTexParameterivRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetTransformFeedbackVaryingFn(GLuint program,
                                                 GLuint index,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLsizei* size,
                                                 GLenum* type,
                                                 char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTransformFeedbackVarying")
  gl_api_->glGetTransformFeedbackVaryingFn(program, index, bufSize, length,
                                           size, type, name);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetTranslatedShaderSourceANGLEFn(GLuint shader,
                                                    GLsizei bufsize,
                                                    GLsizei* length,
                                                    char* source) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTranslatedShaderSourceANGLE")
  gl_api_->glGetTranslatedShaderSourceANGLEFn(shader, bufsize, length, source);
}

DISABLE_CFI_ICALL
GLuint TraceGLApi::glGetUniformBlockIndexFn(GLuint program,
                                            const char* uniformBlockName) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformBlockIndex")
  return gl_api_->glGetUniformBlockIndexFn(program, uniformBlockName);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetUniformfvFn(GLuint program,
                                  GLint location,
                                  GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformfv")
  gl_api_->glGetUniformfvFn(program, location, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetUniformfvRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformfvRobustANGLE")
  gl_api_->glGetUniformfvRobustANGLEFn(program, location, bufSize, length,
                                       params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetUniformIndicesFn(GLuint program,
                                       GLsizei uniformCount,
                                       const char* const* uniformNames,
                                       GLuint* uniformIndices) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformIndices")
  gl_api_->glGetUniformIndicesFn(program, uniformCount, uniformNames,
                                 uniformIndices);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetUniformivFn(GLuint program,
                                  GLint location,
                                  GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformiv")
  gl_api_->glGetUniformivFn(program, location, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetUniformivRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformivRobustANGLE")
  gl_api_->glGetUniformivRobustANGLEFn(program, location, bufSize, length,
                                       params);
}

DISABLE_CFI_ICALL
GLint TraceGLApi::glGetUniformLocationFn(GLuint program, const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformLocation")
  return gl_api_->glGetUniformLocationFn(program, name);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetUniformuivFn(GLuint program,
                                   GLint location,
                                   GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformuiv")
  gl_api_->glGetUniformuivFn(program, location, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetUniformuivRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformuivRobustANGLE")
  gl_api_->glGetUniformuivRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetVertexAttribfvFn(GLuint index,
                                       GLenum pname,
                                       GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetVertexAttribfv")
  gl_api_->glGetVertexAttribfvFn(index, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetVertexAttribfvRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetVertexAttribfvRobustANGLE")
  gl_api_->glGetVertexAttribfvRobustANGLEFn(index, pname, bufSize, length,
                                            params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetVertexAttribIivRobustANGLEFn(GLuint index,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetVertexAttribIivRobustANGLE")
  gl_api_->glGetVertexAttribIivRobustANGLEFn(index, pname, bufSize, length,
                                             params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetVertexAttribIuivRobustANGLEFn(GLuint index,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetVertexAttribIuivRobustANGLE")
  gl_api_->glGetVertexAttribIuivRobustANGLEFn(index, pname, bufSize, length,
                                              params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetVertexAttribivFn(GLuint index,
                                       GLenum pname,
                                       GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetVertexAttribiv")
  gl_api_->glGetVertexAttribivFn(index, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetVertexAttribivRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetVertexAttribivRobustANGLE")
  gl_api_->glGetVertexAttribivRobustANGLEFn(index, pname, bufSize, length,
                                            params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetVertexAttribPointervFn(GLuint index,
                                             GLenum pname,
                                             void** pointer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetVertexAttribPointerv")
  gl_api_->glGetVertexAttribPointervFn(index, pname, pointer);
}

DISABLE_CFI_ICALL
void TraceGLApi::glGetVertexAttribPointervRobustANGLEFn(GLuint index,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        void** pointer) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetVertexAttribPointervRobustANGLE")
  gl_api_->glGetVertexAttribPointervRobustANGLEFn(index, pname, bufSize, length,
                                                  pointer);
}

DISABLE_CFI_ICALL
void TraceGLApi::glHintFn(GLenum target, GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glHint")
  gl_api_->glHintFn(target, mode);
}

DISABLE_CFI_ICALL
void TraceGLApi::glInsertEventMarkerEXTFn(GLsizei length, const char* marker) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glInsertEventMarkerEXT")
  gl_api_->glInsertEventMarkerEXTFn(length, marker);
}

DISABLE_CFI_ICALL
void TraceGLApi::glInvalidateFramebufferFn(GLenum target,
                                           GLsizei numAttachments,
                                           const GLenum* attachments) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glInvalidateFramebuffer")
  gl_api_->glInvalidateFramebufferFn(target, numAttachments, attachments);
}

DISABLE_CFI_ICALL
void TraceGLApi::glInvalidateSubFramebufferFn(GLenum target,
                                              GLsizei numAttachments,
                                              const GLenum* attachments,
                                              GLint x,
                                              GLint y,
                                              GLint width,
                                              GLint height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glInvalidateSubFramebuffer")
  gl_api_->glInvalidateSubFramebufferFn(target, numAttachments, attachments, x,
                                        y, width, height);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsBufferFn(GLuint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsBuffer")
  return gl_api_->glIsBufferFn(buffer);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsEnabledFn(GLenum cap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsEnabled")
  return gl_api_->glIsEnabledFn(cap);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsFenceAPPLEFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsFenceAPPLE")
  return gl_api_->glIsFenceAPPLEFn(fence);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsFenceNVFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsFenceNV")
  return gl_api_->glIsFenceNVFn(fence);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsFramebufferEXTFn(GLuint framebuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsFramebufferEXT")
  return gl_api_->glIsFramebufferEXTFn(framebuffer);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsPathNVFn(GLuint path) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsPathNV")
  return gl_api_->glIsPathNVFn(path);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsProgram")
  return gl_api_->glIsProgramFn(program);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsQueryFn(GLuint query) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsQuery")
  return gl_api_->glIsQueryFn(query);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsRenderbufferEXTFn(GLuint renderbuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsRenderbufferEXT")
  return gl_api_->glIsRenderbufferEXTFn(renderbuffer);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsSamplerFn(GLuint sampler) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsSampler")
  return gl_api_->glIsSamplerFn(sampler);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsShaderFn(GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsShader")
  return gl_api_->glIsShaderFn(shader);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsSyncFn(GLsync sync) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsSync")
  return gl_api_->glIsSyncFn(sync);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsTextureFn(GLuint texture) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsTexture")
  return gl_api_->glIsTextureFn(texture);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsTransformFeedbackFn(GLuint id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsTransformFeedback")
  return gl_api_->glIsTransformFeedbackFn(id);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glIsVertexArrayOESFn(GLuint array) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsVertexArrayOES")
  return gl_api_->glIsVertexArrayOESFn(array);
}

DISABLE_CFI_ICALL
void TraceGLApi::glLineWidthFn(GLfloat width) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glLineWidth")
  gl_api_->glLineWidthFn(width);
}

DISABLE_CFI_ICALL
void TraceGLApi::glLinkProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glLinkProgram")
  gl_api_->glLinkProgramFn(program);
}

DISABLE_CFI_ICALL
void* TraceGLApi::glMapBufferFn(GLenum target, GLenum access) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMapBuffer")
  return gl_api_->glMapBufferFn(target, access);
}

DISABLE_CFI_ICALL
void* TraceGLApi::glMapBufferRangeFn(GLenum target,
                                     GLintptr offset,
                                     GLsizeiptr length,
                                     GLbitfield access) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMapBufferRange")
  return gl_api_->glMapBufferRangeFn(target, offset, length, access);
}

DISABLE_CFI_ICALL
void TraceGLApi::glMatrixLoadfEXTFn(GLenum matrixMode, const GLfloat* m) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMatrixLoadfEXT")
  gl_api_->glMatrixLoadfEXTFn(matrixMode, m);
}

DISABLE_CFI_ICALL
void TraceGLApi::glMatrixLoadIdentityEXTFn(GLenum matrixMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMatrixLoadIdentityEXT")
  gl_api_->glMatrixLoadIdentityEXTFn(matrixMode);
}

DISABLE_CFI_ICALL
void TraceGLApi::glMemoryBarrierEXTFn(GLbitfield barriers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMemoryBarrierEXT")
  gl_api_->glMemoryBarrierEXTFn(barriers);
}

DISABLE_CFI_ICALL
void TraceGLApi::glObjectLabelFn(GLenum identifier,
                                 GLuint name,
                                 GLsizei length,
                                 const char* label) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glObjectLabel")
  gl_api_->glObjectLabelFn(identifier, name, length, label);
}

DISABLE_CFI_ICALL
void TraceGLApi::glObjectPtrLabelFn(void* ptr,
                                    GLsizei length,
                                    const char* label) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glObjectPtrLabel")
  gl_api_->glObjectPtrLabelFn(ptr, length, label);
}

DISABLE_CFI_ICALL
void TraceGLApi::glPathCommandsNVFn(GLuint path,
                                    GLsizei numCommands,
                                    const GLubyte* commands,
                                    GLsizei numCoords,
                                    GLenum coordType,
                                    const GLvoid* coords) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPathCommandsNV")
  gl_api_->glPathCommandsNVFn(path, numCommands, commands, numCoords, coordType,
                              coords);
}

DISABLE_CFI_ICALL
void TraceGLApi::glPathParameterfNVFn(GLuint path,
                                      GLenum pname,
                                      GLfloat value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPathParameterfNV")
  gl_api_->glPathParameterfNVFn(path, pname, value);
}

DISABLE_CFI_ICALL
void TraceGLApi::glPathParameteriNVFn(GLuint path, GLenum pname, GLint value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPathParameteriNV")
  gl_api_->glPathParameteriNVFn(path, pname, value);
}

DISABLE_CFI_ICALL
void TraceGLApi::glPathStencilFuncNVFn(GLenum func, GLint ref, GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPathStencilFuncNV")
  gl_api_->glPathStencilFuncNVFn(func, ref, mask);
}

DISABLE_CFI_ICALL
void TraceGLApi::glPauseTransformFeedbackFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPauseTransformFeedback")
  gl_api_->glPauseTransformFeedbackFn();
}

DISABLE_CFI_ICALL
void TraceGLApi::glPixelStoreiFn(GLenum pname, GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPixelStorei")
  gl_api_->glPixelStoreiFn(pname, param);
}

DISABLE_CFI_ICALL
void TraceGLApi::glPointParameteriFn(GLenum pname, GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPointParameteri")
  gl_api_->glPointParameteriFn(pname, param);
}

DISABLE_CFI_ICALL
void TraceGLApi::glPolygonModeFn(GLenum face, GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPolygonMode")
  gl_api_->glPolygonModeFn(face, mode);
}

DISABLE_CFI_ICALL
void TraceGLApi::glPolygonOffsetFn(GLfloat factor, GLfloat units) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPolygonOffset")
  gl_api_->glPolygonOffsetFn(factor, units);
}

DISABLE_CFI_ICALL
void TraceGLApi::glPopDebugGroupFn() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPopDebugGroup")
  gl_api_->glPopDebugGroupFn();
}

DISABLE_CFI_ICALL
void TraceGLApi::glPopGroupMarkerEXTFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPopGroupMarkerEXT")
  gl_api_->glPopGroupMarkerEXTFn();
}

DISABLE_CFI_ICALL
void TraceGLApi::glPrimitiveRestartIndexFn(GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPrimitiveRestartIndex")
  gl_api_->glPrimitiveRestartIndexFn(index);
}

DISABLE_CFI_ICALL
void TraceGLApi::glProgramBinaryFn(GLuint program,
                                   GLenum binaryFormat,
                                   const GLvoid* binary,
                                   GLsizei length) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramBinary")
  gl_api_->glProgramBinaryFn(program, binaryFormat, binary, length);
}

DISABLE_CFI_ICALL
void TraceGLApi::glProgramParameteriFn(GLuint program,
                                       GLenum pname,
                                       GLint value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramParameteri")
  gl_api_->glProgramParameteriFn(program, pname, value);
}

DISABLE_CFI_ICALL
void TraceGLApi::glProgramPathFragmentInputGenNVFn(GLuint program,
                                                   GLint location,
                                                   GLenum genMode,
                                                   GLint components,
                                                   const GLfloat* coeffs) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glProgramPathFragmentInputGenNV")
  gl_api_->glProgramPathFragmentInputGenNVFn(program, location, genMode,
                                             components, coeffs);
}

DISABLE_CFI_ICALL
void TraceGLApi::glPushDebugGroupFn(GLenum source,
                                    GLuint id,
                                    GLsizei length,
                                    const char* message) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPushDebugGroup")
  gl_api_->glPushDebugGroupFn(source, id, length, message);
}

DISABLE_CFI_ICALL
void TraceGLApi::glPushGroupMarkerEXTFn(GLsizei length, const char* marker) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPushGroupMarkerEXT")
  gl_api_->glPushGroupMarkerEXTFn(length, marker);
}

DISABLE_CFI_ICALL
void TraceGLApi::glQueryCounterFn(GLuint id, GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glQueryCounter")
  gl_api_->glQueryCounterFn(id, target);
}

DISABLE_CFI_ICALL
void TraceGLApi::glReadBufferFn(GLenum src) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReadBuffer")
  gl_api_->glReadBufferFn(src);
}

DISABLE_CFI_ICALL
void TraceGLApi::glReadnPixelsRobustANGLEFn(GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLenum type,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLsizei* columns,
                                            GLsizei* rows,
                                            void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReadnPixelsRobustANGLE")
  gl_api_->glReadnPixelsRobustANGLEFn(x, y, width, height, format, type,
                                      bufSize, length, columns, rows, data);
}

DISABLE_CFI_ICALL
void TraceGLApi::glReadPixelsFn(GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                GLenum format,
                                GLenum type,
                                void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReadPixels")
  gl_api_->glReadPixelsFn(x, y, width, height, format, type, pixels);
}

DISABLE_CFI_ICALL
void TraceGLApi::glReadPixelsRobustANGLEFn(GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLsizei* columns,
                                           GLsizei* rows,
                                           void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReadPixelsRobustANGLE")
  gl_api_->glReadPixelsRobustANGLEFn(x, y, width, height, format, type, bufSize,
                                     length, columns, rows, pixels);
}

DISABLE_CFI_ICALL
void TraceGLApi::glReleaseShaderCompilerFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReleaseShaderCompiler")
  gl_api_->glReleaseShaderCompilerFn();
}

DISABLE_CFI_ICALL
void TraceGLApi::glRenderbufferStorageEXTFn(GLenum target,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glRenderbufferStorageEXT")
  gl_api_->glRenderbufferStorageEXTFn(target, internalformat, width, height);
}

DISABLE_CFI_ICALL
void TraceGLApi::glRenderbufferStorageMultisampleFn(GLenum target,
                                                    GLsizei samples,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glRenderbufferStorageMultisample")
  gl_api_->glRenderbufferStorageMultisampleFn(target, samples, internalformat,
                                              width, height);
}

DISABLE_CFI_ICALL
void TraceGLApi::glRenderbufferStorageMultisampleEXTFn(GLenum target,
                                                       GLsizei samples,
                                                       GLenum internalformat,
                                                       GLsizei width,
                                                       GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glRenderbufferStorageMultisampleEXT")
  gl_api_->glRenderbufferStorageMultisampleEXTFn(target, samples,
                                                 internalformat, width, height);
}

DISABLE_CFI_ICALL
void TraceGLApi::glRequestExtensionANGLEFn(const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glRequestExtensionANGLE")
  gl_api_->glRequestExtensionANGLEFn(name);
}

DISABLE_CFI_ICALL
void TraceGLApi::glResumeTransformFeedbackFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glResumeTransformFeedback")
  gl_api_->glResumeTransformFeedbackFn();
}

DISABLE_CFI_ICALL
void TraceGLApi::glSampleCoverageFn(GLclampf value, GLboolean invert) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSampleCoverage")
  gl_api_->glSampleCoverageFn(value, invert);
}

DISABLE_CFI_ICALL
void TraceGLApi::glSamplerParameterfFn(GLuint sampler,
                                       GLenum pname,
                                       GLfloat param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSamplerParameterf")
  gl_api_->glSamplerParameterfFn(sampler, pname, param);
}

DISABLE_CFI_ICALL
void TraceGLApi::glSamplerParameterfvFn(GLuint sampler,
                                        GLenum pname,
                                        const GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSamplerParameterfv")
  gl_api_->glSamplerParameterfvFn(sampler, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLfloat* param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glSamplerParameterfvRobustANGLE")
  gl_api_->glSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize, param);
}

DISABLE_CFI_ICALL
void TraceGLApi::glSamplerParameteriFn(GLuint sampler,
                                       GLenum pname,
                                       GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSamplerParameteri")
  gl_api_->glSamplerParameteriFn(sampler, pname, param);
}

DISABLE_CFI_ICALL
void TraceGLApi::glSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    const GLint* param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glSamplerParameterIivRobustANGLE")
  gl_api_->glSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize, param);
}

DISABLE_CFI_ICALL
void TraceGLApi::glSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     const GLuint* param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glSamplerParameterIuivRobustANGLE")
  gl_api_->glSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize, param);
}

DISABLE_CFI_ICALL
void TraceGLApi::glSamplerParameterivFn(GLuint sampler,
                                        GLenum pname,
                                        const GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSamplerParameteriv")
  gl_api_->glSamplerParameterivFn(sampler, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLint* param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glSamplerParameterivRobustANGLE")
  gl_api_->glSamplerParameterivRobustANGLEFn(sampler, pname, bufSize, param);
}

DISABLE_CFI_ICALL
void TraceGLApi::glScissorFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glScissor")
  gl_api_->glScissorFn(x, y, width, height);
}

DISABLE_CFI_ICALL
void TraceGLApi::glSetFenceAPPLEFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSetFenceAPPLE")
  gl_api_->glSetFenceAPPLEFn(fence);
}

DISABLE_CFI_ICALL
void TraceGLApi::glSetFenceNVFn(GLuint fence, GLenum condition) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSetFenceNV")
  gl_api_->glSetFenceNVFn(fence, condition);
}

DISABLE_CFI_ICALL
void TraceGLApi::glShaderBinaryFn(GLsizei n,
                                  const GLuint* shaders,
                                  GLenum binaryformat,
                                  const void* binary,
                                  GLsizei length) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glShaderBinary")
  gl_api_->glShaderBinaryFn(n, shaders, binaryformat, binary, length);
}

DISABLE_CFI_ICALL
void TraceGLApi::glShaderSourceFn(GLuint shader,
                                  GLsizei count,
                                  const char* const* str,
                                  const GLint* length) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glShaderSource")
  gl_api_->glShaderSourceFn(shader, count, str, length);
}

DISABLE_CFI_ICALL
void TraceGLApi::glStencilFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glStencilFillPathInstancedNV")
  gl_api_->glStencilFillPathInstancedNVFn(numPaths, pathNameType, paths,
                                          pathBase, fillMode, mask,
                                          transformType, transformValues);
}

DISABLE_CFI_ICALL
void TraceGLApi::glStencilFillPathNVFn(GLuint path,
                                       GLenum fillMode,
                                       GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilFillPathNV")
  gl_api_->glStencilFillPathNVFn(path, fillMode, mask);
}

DISABLE_CFI_ICALL
void TraceGLApi::glStencilFuncFn(GLenum func, GLint ref, GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilFunc")
  gl_api_->glStencilFuncFn(func, ref, mask);
}

DISABLE_CFI_ICALL
void TraceGLApi::glStencilFuncSeparateFn(GLenum face,
                                         GLenum func,
                                         GLint ref,
                                         GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilFuncSeparate")
  gl_api_->glStencilFuncSeparateFn(face, func, ref, mask);
}

DISABLE_CFI_ICALL
void TraceGLApi::glStencilMaskFn(GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilMask")
  gl_api_->glStencilMaskFn(mask);
}

DISABLE_CFI_ICALL
void TraceGLApi::glStencilMaskSeparateFn(GLenum face, GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilMaskSeparate")
  gl_api_->glStencilMaskSeparateFn(face, mask);
}

DISABLE_CFI_ICALL
void TraceGLApi::glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilOp")
  gl_api_->glStencilOpFn(fail, zfail, zpass);
}

DISABLE_CFI_ICALL
void TraceGLApi::glStencilOpSeparateFn(GLenum face,
                                       GLenum fail,
                                       GLenum zfail,
                                       GLenum zpass) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilOpSeparate")
  gl_api_->glStencilOpSeparateFn(face, fail, zfail, zpass);
}

DISABLE_CFI_ICALL
void TraceGLApi::glStencilStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glStencilStrokePathInstancedNV")
  gl_api_->glStencilStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                            pathBase, ref, mask, transformType,
                                            transformValues);
}

DISABLE_CFI_ICALL
void TraceGLApi::glStencilStrokePathNVFn(GLuint path,
                                         GLint reference,
                                         GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilStrokePathNV")
  gl_api_->glStencilStrokePathNVFn(path, reference, mask);
}

DISABLE_CFI_ICALL
void TraceGLApi::glStencilThenCoverFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glStencilThenCoverFillPathInstancedNV")
  gl_api_->glStencilThenCoverFillPathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, fillMode, mask, coverMode,
      transformType, transformValues);
}

DISABLE_CFI_ICALL
void TraceGLApi::glStencilThenCoverFillPathNVFn(GLuint path,
                                                GLenum fillMode,
                                                GLuint mask,
                                                GLenum coverMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glStencilThenCoverFillPathNV")
  gl_api_->glStencilThenCoverFillPathNVFn(path, fillMode, mask, coverMode);
}

DISABLE_CFI_ICALL
void TraceGLApi::glStencilThenCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glStencilThenCoverStrokePathInstancedNV")
  gl_api_->glStencilThenCoverStrokePathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, ref, mask, coverMode,
      transformType, transformValues);
}

DISABLE_CFI_ICALL
void TraceGLApi::glStencilThenCoverStrokePathNVFn(GLuint path,
                                                  GLint reference,
                                                  GLuint mask,
                                                  GLenum coverMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glStencilThenCoverStrokePathNV")
  gl_api_->glStencilThenCoverStrokePathNVFn(path, reference, mask, coverMode);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glTestFenceAPPLEFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTestFenceAPPLE")
  return gl_api_->glTestFenceAPPLEFn(fence);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glTestFenceNVFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTestFenceNV")
  return gl_api_->glTestFenceNVFn(fence);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexBufferFn(GLenum target,
                               GLenum internalformat,
                               GLuint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexBuffer")
  gl_api_->glTexBufferFn(target, internalformat, buffer);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexBufferRangeFn(GLenum target,
                                    GLenum internalformat,
                                    GLuint buffer,
                                    GLintptr offset,
                                    GLsizeiptr size) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexBufferRange")
  gl_api_->glTexBufferRangeFn(target, internalformat, buffer, offset, size);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexImage2DFn(GLenum target,
                                GLint level,
                                GLint internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLint border,
                                GLenum format,
                                GLenum type,
                                const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexImage2D")
  gl_api_->glTexImage2DFn(target, level, internalformat, width, height, border,
                          format, type, pixels);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexImage2DRobustANGLEFn(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLint border,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexImage2DRobustANGLE")
  gl_api_->glTexImage2DRobustANGLEFn(target, level, internalformat, width,
                                     height, border, format, type, bufSize,
                                     pixels);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexImage3DFn(GLenum target,
                                GLint level,
                                GLint internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth,
                                GLint border,
                                GLenum format,
                                GLenum type,
                                const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexImage3D")
  gl_api_->glTexImage3DFn(target, level, internalformat, width, height, depth,
                          border, format, type, pixels);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexImage3DRobustANGLEFn(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLsizei depth,
                                           GLint border,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexImage3DRobustANGLE")
  gl_api_->glTexImage3DRobustANGLEFn(target, level, internalformat, width,
                                     height, depth, border, format, type,
                                     bufSize, pixels);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexParameterfFn(GLenum target, GLenum pname, GLfloat param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexParameterf")
  gl_api_->glTexParameterfFn(target, pname, param);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexParameterfvFn(GLenum target,
                                    GLenum pname,
                                    const GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexParameterfv")
  gl_api_->glTexParameterfvFn(target, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexParameterfvRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTexParameterfvRobustANGLE")
  gl_api_->glTexParameterfvRobustANGLEFn(target, pname, bufSize, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexParameteriFn(GLenum target, GLenum pname, GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexParameteri")
  gl_api_->glTexParameteriFn(target, pname, param);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexParameterIivRobustANGLEFn(GLenum target,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                const GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTexParameterIivRobustANGLE")
  gl_api_->glTexParameterIivRobustANGLEFn(target, pname, bufSize, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexParameterIuivRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 const GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTexParameterIuivRobustANGLE")
  gl_api_->glTexParameterIuivRobustANGLEFn(target, pname, bufSize, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexParameterivFn(GLenum target,
                                    GLenum pname,
                                    const GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexParameteriv")
  gl_api_->glTexParameterivFn(target, pname, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexParameterivRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTexParameterivRobustANGLE")
  gl_api_->glTexParameterivRobustANGLEFn(target, pname, bufSize, params);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexStorage2DEXTFn(GLenum target,
                                     GLsizei levels,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexStorage2DEXT")
  gl_api_->glTexStorage2DEXTFn(target, levels, internalformat, width, height);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexStorage3DFn(GLenum target,
                                  GLsizei levels,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexStorage3D")
  gl_api_->glTexStorage3DFn(target, levels, internalformat, width, height,
                            depth);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexSubImage2DFn(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum format,
                                   GLenum type,
                                   const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexSubImage2D")
  gl_api_->glTexSubImage2DFn(target, level, xoffset, yoffset, width, height,
                             format, type, pixels);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexSubImage2DRobustANGLEFn(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLsizei width,
                                              GLsizei height,
                                              GLenum format,
                                              GLenum type,
                                              GLsizei bufSize,
                                              const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexSubImage2DRobustANGLE")
  gl_api_->glTexSubImage2DRobustANGLEFn(target, level, xoffset, yoffset, width,
                                        height, format, type, bufSize, pixels);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexSubImage3DFn(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLint zoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLenum format,
                                   GLenum type,
                                   const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexSubImage3D")
  gl_api_->glTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, width,
                             height, depth, format, type, pixels);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTexSubImage3DRobustANGLEFn(GLenum target,
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
                                              const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexSubImage3DRobustANGLE")
  gl_api_->glTexSubImage3DRobustANGLEFn(target, level, xoffset, yoffset,
                                        zoffset, width, height, depth, format,
                                        type, bufSize, pixels);
}

DISABLE_CFI_ICALL
void TraceGLApi::glTransformFeedbackVaryingsFn(GLuint program,
                                               GLsizei count,
                                               const char* const* varyings,
                                               GLenum bufferMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTransformFeedbackVaryings")
  gl_api_->glTransformFeedbackVaryingsFn(program, count, varyings, bufferMode);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform1fFn(GLint location, GLfloat x) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1f")
  gl_api_->glUniform1fFn(location, x);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform1fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1fv")
  gl_api_->glUniform1fvFn(location, count, v);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform1iFn(GLint location, GLint x) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1i")
  gl_api_->glUniform1iFn(location, x);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform1ivFn(GLint location, GLsizei count, const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1iv")
  gl_api_->glUniform1ivFn(location, count, v);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform1uiFn(GLint location, GLuint v0) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1ui")
  gl_api_->glUniform1uiFn(location, v0);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform1uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1uiv")
  gl_api_->glUniform1uivFn(location, count, v);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform2fFn(GLint location, GLfloat x, GLfloat y) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2f")
  gl_api_->glUniform2fFn(location, x, y);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform2fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2fv")
  gl_api_->glUniform2fvFn(location, count, v);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform2iFn(GLint location, GLint x, GLint y) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2i")
  gl_api_->glUniform2iFn(location, x, y);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform2ivFn(GLint location, GLsizei count, const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2iv")
  gl_api_->glUniform2ivFn(location, count, v);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform2uiFn(GLint location, GLuint v0, GLuint v1) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2ui")
  gl_api_->glUniform2uiFn(location, v0, v1);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform2uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2uiv")
  gl_api_->glUniform2uivFn(location, count, v);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform3fFn(GLint location,
                               GLfloat x,
                               GLfloat y,
                               GLfloat z) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3f")
  gl_api_->glUniform3fFn(location, x, y, z);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform3fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3fv")
  gl_api_->glUniform3fvFn(location, count, v);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform3iFn(GLint location, GLint x, GLint y, GLint z) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3i")
  gl_api_->glUniform3iFn(location, x, y, z);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform3ivFn(GLint location, GLsizei count, const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3iv")
  gl_api_->glUniform3ivFn(location, count, v);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform3uiFn(GLint location,
                                GLuint v0,
                                GLuint v1,
                                GLuint v2) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3ui")
  gl_api_->glUniform3uiFn(location, v0, v1, v2);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform3uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3uiv")
  gl_api_->glUniform3uivFn(location, count, v);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform4fFn(GLint location,
                               GLfloat x,
                               GLfloat y,
                               GLfloat z,
                               GLfloat w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4f")
  gl_api_->glUniform4fFn(location, x, y, z, w);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform4fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4fv")
  gl_api_->glUniform4fvFn(location, count, v);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform4iFn(GLint location,
                               GLint x,
                               GLint y,
                               GLint z,
                               GLint w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4i")
  gl_api_->glUniform4iFn(location, x, y, z, w);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform4ivFn(GLint location, GLsizei count, const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4iv")
  gl_api_->glUniform4ivFn(location, count, v);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform4uiFn(GLint location,
                                GLuint v0,
                                GLuint v1,
                                GLuint v2,
                                GLuint v3) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4ui")
  gl_api_->glUniform4uiFn(location, v0, v1, v2, v3);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniform4uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4uiv")
  gl_api_->glUniform4uivFn(location, count, v);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniformBlockBindingFn(GLuint program,
                                         GLuint uniformBlockIndex,
                                         GLuint uniformBlockBinding) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformBlockBinding")
  gl_api_->glUniformBlockBindingFn(program, uniformBlockIndex,
                                   uniformBlockBinding);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniformMatrix2fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix2fv")
  gl_api_->glUniformMatrix2fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniformMatrix2x3fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix2x3fv")
  gl_api_->glUniformMatrix2x3fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniformMatrix2x4fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix2x4fv")
  gl_api_->glUniformMatrix2x4fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniformMatrix3fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix3fv")
  gl_api_->glUniformMatrix3fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniformMatrix3x2fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix3x2fv")
  gl_api_->glUniformMatrix3x2fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniformMatrix3x4fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix3x4fv")
  gl_api_->glUniformMatrix3x4fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniformMatrix4fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix4fv")
  gl_api_->glUniformMatrix4fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniformMatrix4x2fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix4x2fv")
  gl_api_->glUniformMatrix4x2fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUniformMatrix4x3fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix4x3fv")
  gl_api_->glUniformMatrix4x3fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
GLboolean TraceGLApi::glUnmapBufferFn(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUnmapBuffer")
  return gl_api_->glUnmapBufferFn(target);
}

DISABLE_CFI_ICALL
void TraceGLApi::glUseProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUseProgram")
  gl_api_->glUseProgramFn(program);
}

DISABLE_CFI_ICALL
void TraceGLApi::glValidateProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glValidateProgram")
  gl_api_->glValidateProgramFn(program);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttrib1fFn(GLuint indx, GLfloat x) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib1f")
  gl_api_->glVertexAttrib1fFn(indx, x);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib1fv")
  gl_api_->glVertexAttrib1fvFn(indx, values);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib2f")
  gl_api_->glVertexAttrib2fFn(indx, x, y);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib2fv")
  gl_api_->glVertexAttrib2fvFn(indx, values);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttrib3fFn(GLuint indx,
                                    GLfloat x,
                                    GLfloat y,
                                    GLfloat z) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib3f")
  gl_api_->glVertexAttrib3fFn(indx, x, y, z);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib3fv")
  gl_api_->glVertexAttrib3fvFn(indx, values);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttrib4fFn(GLuint indx,
                                    GLfloat x,
                                    GLfloat y,
                                    GLfloat z,
                                    GLfloat w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib4f")
  gl_api_->glVertexAttrib4fFn(indx, x, y, z, w);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib4fv")
  gl_api_->glVertexAttrib4fvFn(indx, values);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttribDivisorANGLEFn(GLuint index, GLuint divisor) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribDivisorANGLE")
  gl_api_->glVertexAttribDivisorANGLEFn(index, divisor);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttribI4iFn(GLuint indx,
                                     GLint x,
                                     GLint y,
                                     GLint z,
                                     GLint w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribI4i")
  gl_api_->glVertexAttribI4iFn(indx, x, y, z, w);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttribI4ivFn(GLuint indx, const GLint* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribI4iv")
  gl_api_->glVertexAttribI4ivFn(indx, values);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttribI4uiFn(GLuint indx,
                                      GLuint x,
                                      GLuint y,
                                      GLuint z,
                                      GLuint w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribI4ui")
  gl_api_->glVertexAttribI4uiFn(indx, x, y, z, w);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttribI4uivFn(GLuint indx, const GLuint* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribI4uiv")
  gl_api_->glVertexAttribI4uivFn(indx, values);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttribIPointerFn(GLuint indx,
                                          GLint size,
                                          GLenum type,
                                          GLsizei stride,
                                          const void* ptr) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribIPointer")
  gl_api_->glVertexAttribIPointerFn(indx, size, type, stride, ptr);
}

DISABLE_CFI_ICALL
void TraceGLApi::glVertexAttribPointerFn(GLuint indx,
                                         GLint size,
                                         GLenum type,
                                         GLboolean normalized,
                                         GLsizei stride,
                                         const void* ptr) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribPointer")
  gl_api_->glVertexAttribPointerFn(indx, size, type, normalized, stride, ptr);
}

DISABLE_CFI_ICALL
void TraceGLApi::glViewportFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glViewport")
  gl_api_->glViewportFn(x, y, width, height);
}

DISABLE_CFI_ICALL
void TraceGLApi::glWaitSyncFn(GLsync sync, GLbitfield flags, GLuint64 timeout) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glWaitSync")
  gl_api_->glWaitSyncFn(sync, flags, timeout);
}

DISABLE_CFI_ICALL
void TraceGLApi::glWindowRectanglesEXTFn(GLenum mode,
                                         GLsizei n,
                                         const GLint* box) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glWindowRectanglesEXT")
  gl_api_->glWindowRectanglesEXTFn(mode, n, box);
}

DISABLE_CFI_ICALL
void DebugGLApi::glActiveTextureFn(GLenum texture) {
  GL_SERVICE_LOG("glActiveTexture"
                 << "(" << GLEnums::GetStringEnum(texture) << ")");
  gl_api_->glActiveTextureFn(texture);
}

DISABLE_CFI_ICALL
void DebugGLApi::glApplyFramebufferAttachmentCMAAINTELFn(void) {
  GL_SERVICE_LOG("glApplyFramebufferAttachmentCMAAINTEL"
                 << "("
                 << ")");
  gl_api_->glApplyFramebufferAttachmentCMAAINTELFn();
}

DISABLE_CFI_ICALL
void DebugGLApi::glAttachShaderFn(GLuint program, GLuint shader) {
  GL_SERVICE_LOG("glAttachShader"
                 << "(" << program << ", " << shader << ")");
  gl_api_->glAttachShaderFn(program, shader);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBeginQueryFn(GLenum target, GLuint id) {
  GL_SERVICE_LOG("glBeginQuery"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << id << ")");
  gl_api_->glBeginQueryFn(target, id);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBeginTransformFeedbackFn(GLenum primitiveMode) {
  GL_SERVICE_LOG("glBeginTransformFeedback"
                 << "(" << GLEnums::GetStringEnum(primitiveMode) << ")");
  gl_api_->glBeginTransformFeedbackFn(primitiveMode);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBindAttribLocationFn(GLuint program,
                                        GLuint index,
                                        const char* name) {
  GL_SERVICE_LOG("glBindAttribLocation"
                 << "(" << program << ", " << index << ", " << name << ")");
  gl_api_->glBindAttribLocationFn(program, index, name);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBindBufferFn(GLenum target, GLuint buffer) {
  GL_SERVICE_LOG("glBindBuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << buffer
                 << ")");
  gl_api_->glBindBufferFn(target, buffer);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBindBufferBaseFn(GLenum target,
                                    GLuint index,
                                    GLuint buffer) {
  GL_SERVICE_LOG("glBindBufferBase"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << buffer << ")");
  gl_api_->glBindBufferBaseFn(target, index, buffer);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBindBufferRangeFn(GLenum target,
                                     GLuint index,
                                     GLuint buffer,
                                     GLintptr offset,
                                     GLsizeiptr size) {
  GL_SERVICE_LOG("glBindBufferRange"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << buffer << ", " << offset << ", " << size << ")");
  gl_api_->glBindBufferRangeFn(target, index, buffer, offset, size);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBindFragDataLocationFn(GLuint program,
                                          GLuint colorNumber,
                                          const char* name) {
  GL_SERVICE_LOG("glBindFragDataLocation"
                 << "(" << program << ", " << colorNumber << ", " << name
                 << ")");
  gl_api_->glBindFragDataLocationFn(program, colorNumber, name);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBindFragDataLocationIndexedFn(GLuint program,
                                                 GLuint colorNumber,
                                                 GLuint index,
                                                 const char* name) {
  GL_SERVICE_LOG("glBindFragDataLocationIndexed"
                 << "(" << program << ", " << colorNumber << ", " << index
                 << ", " << name << ")");
  gl_api_->glBindFragDataLocationIndexedFn(program, colorNumber, index, name);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) {
  GL_SERVICE_LOG("glBindFramebufferEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << framebuffer
                 << ")");
  gl_api_->glBindFramebufferEXTFn(target, framebuffer);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBindImageTextureEXTFn(GLuint index,
                                         GLuint texture,
                                         GLint level,
                                         GLboolean layered,
                                         GLint layer,
                                         GLenum access,
                                         GLint format) {
  GL_SERVICE_LOG("glBindImageTextureEXT"
                 << "(" << index << ", " << texture << ", " << level << ", "
                 << GLEnums::GetStringBool(layered) << ", " << layer << ", "
                 << GLEnums::GetStringEnum(access) << ", " << format << ")");
  gl_api_->glBindImageTextureEXTFn(index, texture, level, layered, layer,
                                   access, format);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBindRenderbufferEXTFn(GLenum target, GLuint renderbuffer) {
  GL_SERVICE_LOG("glBindRenderbufferEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << renderbuffer << ")");
  gl_api_->glBindRenderbufferEXTFn(target, renderbuffer);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBindSamplerFn(GLuint unit, GLuint sampler) {
  GL_SERVICE_LOG("glBindSampler"
                 << "(" << unit << ", " << sampler << ")");
  gl_api_->glBindSamplerFn(unit, sampler);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBindTextureFn(GLenum target, GLuint texture) {
  GL_SERVICE_LOG("glBindTexture"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << texture
                 << ")");
  gl_api_->glBindTextureFn(target, texture);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBindTransformFeedbackFn(GLenum target, GLuint id) {
  GL_SERVICE_LOG("glBindTransformFeedback"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << id << ")");
  gl_api_->glBindTransformFeedbackFn(target, id);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBindUniformLocationCHROMIUMFn(GLuint program,
                                                 GLint location,
                                                 const char* name) {
  GL_SERVICE_LOG("glBindUniformLocationCHROMIUM"
                 << "(" << program << ", " << location << ", " << name << ")");
  gl_api_->glBindUniformLocationCHROMIUMFn(program, location, name);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBindVertexArrayOESFn(GLuint array) {
  GL_SERVICE_LOG("glBindVertexArrayOES"
                 << "(" << array << ")");
  gl_api_->glBindVertexArrayOESFn(array);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBlendBarrierKHRFn(void) {
  GL_SERVICE_LOG("glBlendBarrierKHR"
                 << "("
                 << ")");
  gl_api_->glBlendBarrierKHRFn();
}

DISABLE_CFI_ICALL
void DebugGLApi::glBlendColorFn(GLclampf red,
                                GLclampf green,
                                GLclampf blue,
                                GLclampf alpha) {
  GL_SERVICE_LOG("glBlendColor"
                 << "(" << red << ", " << green << ", " << blue << ", " << alpha
                 << ")");
  gl_api_->glBlendColorFn(red, green, blue, alpha);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBlendEquationFn(GLenum mode) {
  GL_SERVICE_LOG("glBlendEquation"
                 << "(" << GLEnums::GetStringEnum(mode) << ")");
  gl_api_->glBlendEquationFn(mode);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBlendEquationSeparateFn(GLenum modeRGB, GLenum modeAlpha) {
  GL_SERVICE_LOG("glBlendEquationSeparate"
                 << "(" << GLEnums::GetStringEnum(modeRGB) << ", "
                 << GLEnums::GetStringEnum(modeAlpha) << ")");
  gl_api_->glBlendEquationSeparateFn(modeRGB, modeAlpha);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBlendFuncFn(GLenum sfactor, GLenum dfactor) {
  GL_SERVICE_LOG("glBlendFunc"
                 << "(" << GLEnums::GetStringEnum(sfactor) << ", "
                 << GLEnums::GetStringEnum(dfactor) << ")");
  gl_api_->glBlendFuncFn(sfactor, dfactor);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBlendFuncSeparateFn(GLenum srcRGB,
                                       GLenum dstRGB,
                                       GLenum srcAlpha,
                                       GLenum dstAlpha) {
  GL_SERVICE_LOG("glBlendFuncSeparate"
                 << "(" << GLEnums::GetStringEnum(srcRGB) << ", "
                 << GLEnums::GetStringEnum(dstRGB) << ", "
                 << GLEnums::GetStringEnum(srcAlpha) << ", "
                 << GLEnums::GetStringEnum(dstAlpha) << ")");
  gl_api_->glBlendFuncSeparateFn(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBlitFramebufferFn(GLint srcX0,
                                     GLint srcY0,
                                     GLint srcX1,
                                     GLint srcY1,
                                     GLint dstX0,
                                     GLint dstY0,
                                     GLint dstX1,
                                     GLint dstY1,
                                     GLbitfield mask,
                                     GLenum filter) {
  GL_SERVICE_LOG("glBlitFramebuffer"
                 << "(" << srcX0 << ", " << srcY0 << ", " << srcX1 << ", "
                 << srcY1 << ", " << dstX0 << ", " << dstY0 << ", " << dstX1
                 << ", " << dstY1 << ", " << mask << ", "
                 << GLEnums::GetStringEnum(filter) << ")");
  gl_api_->glBlitFramebufferFn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1,
                               dstY1, mask, filter);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBufferDataFn(GLenum target,
                                GLsizeiptr size,
                                const void* data,
                                GLenum usage) {
  GL_SERVICE_LOG("glBufferData"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << size
                 << ", " << static_cast<const void*>(data) << ", "
                 << GLEnums::GetStringEnum(usage) << ")");
  gl_api_->glBufferDataFn(target, size, data, usage);
}

DISABLE_CFI_ICALL
void DebugGLApi::glBufferSubDataFn(GLenum target,
                                   GLintptr offset,
                                   GLsizeiptr size,
                                   const void* data) {
  GL_SERVICE_LOG("glBufferSubData"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << offset
                 << ", " << size << ", " << static_cast<const void*>(data)
                 << ")");
  gl_api_->glBufferSubDataFn(target, offset, size, data);
}

DISABLE_CFI_ICALL
GLenum DebugGLApi::glCheckFramebufferStatusEXTFn(GLenum target) {
  GL_SERVICE_LOG("glCheckFramebufferStatusEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ")");
  GLenum result = gl_api_->glCheckFramebufferStatusEXTFn(target);

  GL_SERVICE_LOG("GL_RESULT: " << GLEnums::GetStringEnum(result));

  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glClearFn(GLbitfield mask) {
  GL_SERVICE_LOG("glClear"
                 << "(" << mask << ")");
  gl_api_->glClearFn(mask);
}

DISABLE_CFI_ICALL
void DebugGLApi::glClearBufferfiFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLfloat depth,
                                   GLint stencil) {
  GL_SERVICE_LOG("glClearBufferfi"
                 << "(" << GLEnums::GetStringEnum(buffer) << ", " << drawbuffer
                 << ", " << depth << ", " << stencil << ")");
  gl_api_->glClearBufferfiFn(buffer, drawbuffer, depth, stencil);
}

DISABLE_CFI_ICALL
void DebugGLApi::glClearBufferfvFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLfloat* value) {
  GL_SERVICE_LOG("glClearBufferfv"
                 << "(" << GLEnums::GetStringEnum(buffer) << ", " << drawbuffer
                 << ", " << static_cast<const void*>(value) << ")");
  gl_api_->glClearBufferfvFn(buffer, drawbuffer, value);
}

DISABLE_CFI_ICALL
void DebugGLApi::glClearBufferivFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLint* value) {
  GL_SERVICE_LOG("glClearBufferiv"
                 << "(" << GLEnums::GetStringEnum(buffer) << ", " << drawbuffer
                 << ", " << static_cast<const void*>(value) << ")");
  gl_api_->glClearBufferivFn(buffer, drawbuffer, value);
}

DISABLE_CFI_ICALL
void DebugGLApi::glClearBufferuivFn(GLenum buffer,
                                    GLint drawbuffer,
                                    const GLuint* value) {
  GL_SERVICE_LOG("glClearBufferuiv"
                 << "(" << GLEnums::GetStringEnum(buffer) << ", " << drawbuffer
                 << ", " << static_cast<const void*>(value) << ")");
  gl_api_->glClearBufferuivFn(buffer, drawbuffer, value);
}

DISABLE_CFI_ICALL
void DebugGLApi::glClearColorFn(GLclampf red,
                                GLclampf green,
                                GLclampf blue,
                                GLclampf alpha) {
  GL_SERVICE_LOG("glClearColor"
                 << "(" << red << ", " << green << ", " << blue << ", " << alpha
                 << ")");
  gl_api_->glClearColorFn(red, green, blue, alpha);
}

DISABLE_CFI_ICALL
void DebugGLApi::glClearDepthFn(GLclampd depth) {
  GL_SERVICE_LOG("glClearDepth"
                 << "(" << depth << ")");
  gl_api_->glClearDepthFn(depth);
}

DISABLE_CFI_ICALL
void DebugGLApi::glClearDepthfFn(GLclampf depth) {
  GL_SERVICE_LOG("glClearDepthf"
                 << "(" << depth << ")");
  gl_api_->glClearDepthfFn(depth);
}

DISABLE_CFI_ICALL
void DebugGLApi::glClearStencilFn(GLint s) {
  GL_SERVICE_LOG("glClearStencil"
                 << "(" << s << ")");
  gl_api_->glClearStencilFn(s);
}

DISABLE_CFI_ICALL
GLenum DebugGLApi::glClientWaitSyncFn(GLsync sync,
                                      GLbitfield flags,
                                      GLuint64 timeout) {
  GL_SERVICE_LOG("glClientWaitSync"
                 << "(" << sync << ", " << flags << ", " << timeout << ")");
  GLenum result = gl_api_->glClientWaitSyncFn(sync, flags, timeout);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glColorMaskFn(GLboolean red,
                               GLboolean green,
                               GLboolean blue,
                               GLboolean alpha) {
  GL_SERVICE_LOG("glColorMask"
                 << "(" << GLEnums::GetStringBool(red) << ", "
                 << GLEnums::GetStringBool(green) << ", "
                 << GLEnums::GetStringBool(blue) << ", "
                 << GLEnums::GetStringBool(alpha) << ")");
  gl_api_->glColorMaskFn(red, green, blue, alpha);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCompileShaderFn(GLuint shader) {
  GL_SERVICE_LOG("glCompileShader"
                 << "(" << shader << ")");
  gl_api_->glCompileShaderFn(shader);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCompressedCopyTextureCHROMIUMFn(GLuint sourceId,
                                                   GLuint destId) {
  GL_SERVICE_LOG("glCompressedCopyTextureCHROMIUM"
                 << "(" << sourceId << ", " << destId << ")");
  gl_api_->glCompressedCopyTextureCHROMIUMFn(sourceId, destId);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCompressedTexImage2DFn(GLenum target,
                                          GLint level,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLint border,
                                          GLsizei imageSize,
                                          const void* data) {
  GL_SERVICE_LOG("glCompressedTexImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ", " << border << ", "
                 << imageSize << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glCompressedTexImage2DFn(target, level, internalformat, width,
                                    height, border, imageSize, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCompressedTexImage2DRobustANGLEFn(GLenum target,
                                                     GLint level,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLint border,
                                                     GLsizei imageSize,
                                                     GLsizei dataSize,
                                                     const void* data) {
  GL_SERVICE_LOG("glCompressedTexImage2DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ", " << border << ", "
                 << imageSize << ", " << dataSize << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glCompressedTexImage2DRobustANGLEFn(target, level, internalformat,
                                               width, height, border, imageSize,
                                               dataSize, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCompressedTexImage3DFn(GLenum target,
                                          GLint level,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLsizei depth,
                                          GLint border,
                                          GLsizei imageSize,
                                          const void* data) {
  GL_SERVICE_LOG("glCompressedTexImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ", " << depth << ", " << border
                 << ", " << imageSize << ", " << static_cast<const void*>(data)
                 << ")");
  gl_api_->glCompressedTexImage3DFn(target, level, internalformat, width,
                                    height, depth, border, imageSize, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCompressedTexImage3DRobustANGLEFn(GLenum target,
                                                     GLint level,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLsizei depth,
                                                     GLint border,
                                                     GLsizei imageSize,
                                                     GLsizei dataSize,
                                                     const void* data) {
  GL_SERVICE_LOG("glCompressedTexImage3DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ", " << depth << ", " << border
                 << ", " << imageSize << ", " << dataSize << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glCompressedTexImage3DRobustANGLEFn(target, level, internalformat,
                                               width, height, depth, border,
                                               imageSize, dataSize, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCompressedTexSubImage2DFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLenum format,
                                             GLsizei imageSize,
                                             const void* data) {
  GL_SERVICE_LOG("glCompressedTexSubImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << width << ", "
                 << height << ", " << GLEnums::GetStringEnum(format) << ", "
                 << imageSize << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glCompressedTexSubImage2DFn(target, level, xoffset, yoffset, width,
                                       height, format, imageSize, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCompressedTexSubImage2DRobustANGLEFn(GLenum target,
                                                        GLint level,
                                                        GLint xoffset,
                                                        GLint yoffset,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLenum format,
                                                        GLsizei imageSize,
                                                        GLsizei dataSize,
                                                        const void* data) {
  GL_SERVICE_LOG("glCompressedTexSubImage2DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << width << ", "
                 << height << ", " << GLEnums::GetStringEnum(format) << ", "
                 << imageSize << ", " << dataSize << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glCompressedTexSubImage2DRobustANGLEFn(
      target, level, xoffset, yoffset, width, height, format, imageSize,
      dataSize, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCompressedTexSubImage3DFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLint zoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLsizei depth,
                                             GLenum format,
                                             GLsizei imageSize,
                                             const void* data) {
  GL_SERVICE_LOG("glCompressedTexSubImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << zoffset
                 << ", " << width << ", " << height << ", " << depth << ", "
                 << GLEnums::GetStringEnum(format) << ", " << imageSize << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glCompressedTexSubImage3DFn(target, level, xoffset, yoffset, zoffset,
                                       width, height, depth, format, imageSize,
                                       data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCompressedTexSubImage3DRobustANGLEFn(GLenum target,
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
                                                        const void* data) {
  GL_SERVICE_LOG("glCompressedTexSubImage3DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << zoffset
                 << ", " << width << ", " << height << ", " << depth << ", "
                 << GLEnums::GetStringEnum(format) << ", " << imageSize << ", "
                 << dataSize << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glCompressedTexSubImage3DRobustANGLEFn(
      target, level, xoffset, yoffset, zoffset, width, height, depth, format,
      imageSize, dataSize, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCopyBufferSubDataFn(GLenum readTarget,
                                       GLenum writeTarget,
                                       GLintptr readOffset,
                                       GLintptr writeOffset,
                                       GLsizeiptr size) {
  GL_SERVICE_LOG("glCopyBufferSubData"
                 << "(" << GLEnums::GetStringEnum(readTarget) << ", "
                 << GLEnums::GetStringEnum(writeTarget) << ", " << readOffset
                 << ", " << writeOffset << ", " << size << ")");
  gl_api_->glCopyBufferSubDataFn(readTarget, writeTarget, readOffset,
                                 writeOffset, size);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCopySubTextureCHROMIUMFn(GLuint sourceId,
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
                                            GLboolean unpackUnmultiplyAlpha) {
  GL_SERVICE_LOG("glCopySubTextureCHROMIUM"
                 << "(" << sourceId << ", " << sourceLevel << ", "
                 << GLEnums::GetStringEnum(destTarget) << ", " << destId << ", "
                 << destLevel << ", " << xoffset << ", " << yoffset << ", " << x
                 << ", " << y << ", " << width << ", " << height << ", "
                 << GLEnums::GetStringBool(unpackFlipY) << ", "
                 << GLEnums::GetStringBool(unpackPremultiplyAlpha) << ", "
                 << GLEnums::GetStringBool(unpackUnmultiplyAlpha) << ")");
  gl_api_->glCopySubTextureCHROMIUMFn(
      sourceId, sourceLevel, destTarget, destId, destLevel, xoffset, yoffset, x,
      y, width, height, unpackFlipY, unpackPremultiplyAlpha,
      unpackUnmultiplyAlpha);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCopyTexImage2DFn(GLenum target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLint border) {
  GL_SERVICE_LOG("glCopyTexImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", " << x
                 << ", " << y << ", " << width << ", " << height << ", "
                 << border << ")");
  gl_api_->glCopyTexImage2DFn(target, level, internalformat, x, y, width,
                              height, border);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCopyTexSubImage2DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height) {
  GL_SERVICE_LOG("glCopyTexSubImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << x << ", " << y
                 << ", " << width << ", " << height << ")");
  gl_api_->glCopyTexSubImage2DFn(target, level, xoffset, yoffset, x, y, width,
                                 height);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCopyTexSubImage3DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height) {
  GL_SERVICE_LOG("glCopyTexSubImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << zoffset
                 << ", " << x << ", " << y << ", " << width << ", " << height
                 << ")");
  gl_api_->glCopyTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, x, y,
                                 width, height);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCopyTextureCHROMIUMFn(GLuint sourceId,
                                         GLint sourceLevel,
                                         GLenum destTarget,
                                         GLuint destId,
                                         GLint destLevel,
                                         GLint internalFormat,
                                         GLenum destType,
                                         GLboolean unpackFlipY,
                                         GLboolean unpackPremultiplyAlpha,
                                         GLboolean unpackUnmultiplyAlpha) {
  GL_SERVICE_LOG("glCopyTextureCHROMIUM"
                 << "(" << sourceId << ", " << sourceLevel << ", "
                 << GLEnums::GetStringEnum(destTarget) << ", " << destId << ", "
                 << destLevel << ", " << internalFormat << ", "
                 << GLEnums::GetStringEnum(destType) << ", "
                 << GLEnums::GetStringBool(unpackFlipY) << ", "
                 << GLEnums::GetStringBool(unpackPremultiplyAlpha) << ", "
                 << GLEnums::GetStringBool(unpackUnmultiplyAlpha) << ")");
  gl_api_->glCopyTextureCHROMIUMFn(
      sourceId, sourceLevel, destTarget, destId, destLevel, internalFormat,
      destType, unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCoverageModulationNVFn(GLenum components) {
  GL_SERVICE_LOG("glCoverageModulationNV"
                 << "(" << GLEnums::GetStringEnum(components) << ")");
  gl_api_->glCoverageModulationNVFn(components);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCoverFillPathInstancedNVFn(GLsizei numPaths,
                                              GLenum pathNameType,
                                              const void* paths,
                                              GLuint pathBase,
                                              GLenum coverMode,
                                              GLenum transformType,
                                              const GLfloat* transformValues) {
  GL_SERVICE_LOG("glCoverFillPathInstancedNV"
                 << "(" << numPaths << ", "
                 << GLEnums::GetStringEnum(pathNameType) << ", "
                 << static_cast<const void*>(paths) << ", " << pathBase << ", "
                 << GLEnums::GetStringEnum(coverMode) << ", "
                 << GLEnums::GetStringEnum(transformType) << ", "
                 << static_cast<const void*>(transformValues) << ")");
  gl_api_->glCoverFillPathInstancedNVFn(numPaths, pathNameType, paths, pathBase,
                                        coverMode, transformType,
                                        transformValues);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCoverFillPathNVFn(GLuint path, GLenum coverMode) {
  GL_SERVICE_LOG("glCoverFillPathNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(coverMode)
                 << ")");
  gl_api_->glCoverFillPathNVFn(path, coverMode);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  GL_SERVICE_LOG("glCoverStrokePathInstancedNV"
                 << "(" << numPaths << ", "
                 << GLEnums::GetStringEnum(pathNameType) << ", "
                 << static_cast<const void*>(paths) << ", " << pathBase << ", "
                 << GLEnums::GetStringEnum(coverMode) << ", "
                 << GLEnums::GetStringEnum(transformType) << ", "
                 << static_cast<const void*>(transformValues) << ")");
  gl_api_->glCoverStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                          pathBase, coverMode, transformType,
                                          transformValues);
}

DISABLE_CFI_ICALL
void DebugGLApi::glCoverStrokePathNVFn(GLuint name, GLenum coverMode) {
  GL_SERVICE_LOG("glCoverStrokePathNV"
                 << "(" << name << ", " << GLEnums::GetStringEnum(coverMode)
                 << ")");
  gl_api_->glCoverStrokePathNVFn(name, coverMode);
}

DISABLE_CFI_ICALL
GLuint DebugGLApi::glCreateProgramFn(void) {
  GL_SERVICE_LOG("glCreateProgram"
                 << "("
                 << ")");
  GLuint result = gl_api_->glCreateProgramFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLuint DebugGLApi::glCreateShaderFn(GLenum type) {
  GL_SERVICE_LOG("glCreateShader"
                 << "(" << GLEnums::GetStringEnum(type) << ")");
  GLuint result = gl_api_->glCreateShaderFn(type);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glCullFaceFn(GLenum mode) {
  GL_SERVICE_LOG("glCullFace"
                 << "(" << GLEnums::GetStringEnum(mode) << ")");
  gl_api_->glCullFaceFn(mode);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDebugMessageCallbackFn(GLDEBUGPROC callback,
                                          const void* userParam) {
  GL_SERVICE_LOG("glDebugMessageCallback"
                 << "(" << callback << ", "
                 << static_cast<const void*>(userParam) << ")");
  gl_api_->glDebugMessageCallbackFn(callback, userParam);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDebugMessageControlFn(GLenum source,
                                         GLenum type,
                                         GLenum severity,
                                         GLsizei count,
                                         const GLuint* ids,
                                         GLboolean enabled) {
  GL_SERVICE_LOG("glDebugMessageControl"
                 << "(" << GLEnums::GetStringEnum(source) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << GLEnums::GetStringEnum(severity) << ", " << count << ", "
                 << static_cast<const void*>(ids) << ", "
                 << GLEnums::GetStringBool(enabled) << ")");
  gl_api_->glDebugMessageControlFn(source, type, severity, count, ids, enabled);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDebugMessageInsertFn(GLenum source,
                                        GLenum type,
                                        GLuint id,
                                        GLenum severity,
                                        GLsizei length,
                                        const char* buf) {
  GL_SERVICE_LOG("glDebugMessageInsert"
                 << "(" << GLEnums::GetStringEnum(source) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << id << ", "
                 << GLEnums::GetStringEnum(severity) << ", " << length << ", "
                 << buf << ")");
  gl_api_->glDebugMessageInsertFn(source, type, id, severity, length, buf);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) {
  GL_SERVICE_LOG("glDeleteBuffersARB"
                 << "(" << n << ", " << static_cast<const void*>(buffers)
                 << ")");
  gl_api_->glDeleteBuffersARBFn(n, buffers);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDeleteFencesAPPLEFn(GLsizei n, const GLuint* fences) {
  GL_SERVICE_LOG("glDeleteFencesAPPLE"
                 << "(" << n << ", " << static_cast<const void*>(fences)
                 << ")");
  gl_api_->glDeleteFencesAPPLEFn(n, fences);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDeleteFencesNVFn(GLsizei n, const GLuint* fences) {
  GL_SERVICE_LOG("glDeleteFencesNV"
                 << "(" << n << ", " << static_cast<const void*>(fences)
                 << ")");
  gl_api_->glDeleteFencesNVFn(n, fences);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDeleteFramebuffersEXTFn(GLsizei n,
                                           const GLuint* framebuffers) {
  GL_SERVICE_LOG("glDeleteFramebuffersEXT"
                 << "(" << n << ", " << static_cast<const void*>(framebuffers)
                 << ")");
  gl_api_->glDeleteFramebuffersEXTFn(n, framebuffers);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDeletePathsNVFn(GLuint path, GLsizei range) {
  GL_SERVICE_LOG("glDeletePathsNV"
                 << "(" << path << ", " << range << ")");
  gl_api_->glDeletePathsNVFn(path, range);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDeleteProgramFn(GLuint program) {
  GL_SERVICE_LOG("glDeleteProgram"
                 << "(" << program << ")");
  gl_api_->glDeleteProgramFn(program);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDeleteQueriesFn(GLsizei n, const GLuint* ids) {
  GL_SERVICE_LOG("glDeleteQueries"
                 << "(" << n << ", " << static_cast<const void*>(ids) << ")");
  gl_api_->glDeleteQueriesFn(n, ids);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDeleteRenderbuffersEXTFn(GLsizei n,
                                            const GLuint* renderbuffers) {
  GL_SERVICE_LOG("glDeleteRenderbuffersEXT"
                 << "(" << n << ", " << static_cast<const void*>(renderbuffers)
                 << ")");
  gl_api_->glDeleteRenderbuffersEXTFn(n, renderbuffers);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDeleteSamplersFn(GLsizei n, const GLuint* samplers) {
  GL_SERVICE_LOG("glDeleteSamplers"
                 << "(" << n << ", " << static_cast<const void*>(samplers)
                 << ")");
  gl_api_->glDeleteSamplersFn(n, samplers);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDeleteShaderFn(GLuint shader) {
  GL_SERVICE_LOG("glDeleteShader"
                 << "(" << shader << ")");
  gl_api_->glDeleteShaderFn(shader);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDeleteSyncFn(GLsync sync) {
  GL_SERVICE_LOG("glDeleteSync"
                 << "(" << sync << ")");
  gl_api_->glDeleteSyncFn(sync);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDeleteTexturesFn(GLsizei n, const GLuint* textures) {
  GL_SERVICE_LOG("glDeleteTextures"
                 << "(" << n << ", " << static_cast<const void*>(textures)
                 << ")");
  gl_api_->glDeleteTexturesFn(n, textures);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDeleteTransformFeedbacksFn(GLsizei n, const GLuint* ids) {
  GL_SERVICE_LOG("glDeleteTransformFeedbacks"
                 << "(" << n << ", " << static_cast<const void*>(ids) << ")");
  gl_api_->glDeleteTransformFeedbacksFn(n, ids);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDeleteVertexArraysOESFn(GLsizei n, const GLuint* arrays) {
  GL_SERVICE_LOG("glDeleteVertexArraysOES"
                 << "(" << n << ", " << static_cast<const void*>(arrays)
                 << ")");
  gl_api_->glDeleteVertexArraysOESFn(n, arrays);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDepthFuncFn(GLenum func) {
  GL_SERVICE_LOG("glDepthFunc"
                 << "(" << GLEnums::GetStringEnum(func) << ")");
  gl_api_->glDepthFuncFn(func);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDepthMaskFn(GLboolean flag) {
  GL_SERVICE_LOG("glDepthMask"
                 << "(" << GLEnums::GetStringBool(flag) << ")");
  gl_api_->glDepthMaskFn(flag);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDepthRangeFn(GLclampd zNear, GLclampd zFar) {
  GL_SERVICE_LOG("glDepthRange"
                 << "(" << zNear << ", " << zFar << ")");
  gl_api_->glDepthRangeFn(zNear, zFar);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDepthRangefFn(GLclampf zNear, GLclampf zFar) {
  GL_SERVICE_LOG("glDepthRangef"
                 << "(" << zNear << ", " << zFar << ")");
  gl_api_->glDepthRangefFn(zNear, zFar);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDetachShaderFn(GLuint program, GLuint shader) {
  GL_SERVICE_LOG("glDetachShader"
                 << "(" << program << ", " << shader << ")");
  gl_api_->glDetachShaderFn(program, shader);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDisableFn(GLenum cap) {
  GL_SERVICE_LOG("glDisable"
                 << "(" << GLEnums::GetStringEnum(cap) << ")");
  gl_api_->glDisableFn(cap);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDisableVertexAttribArrayFn(GLuint index) {
  GL_SERVICE_LOG("glDisableVertexAttribArray"
                 << "(" << index << ")");
  gl_api_->glDisableVertexAttribArrayFn(index);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDiscardFramebufferEXTFn(GLenum target,
                                           GLsizei numAttachments,
                                           const GLenum* attachments) {
  GL_SERVICE_LOG("glDiscardFramebufferEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << numAttachments << ", "
                 << static_cast<const void*>(attachments) << ")");
  gl_api_->glDiscardFramebufferEXTFn(target, numAttachments, attachments);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  GL_SERVICE_LOG("glDrawArrays"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << first << ", "
                 << count << ")");
  gl_api_->glDrawArraysFn(mode, first, count);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDrawArraysInstancedANGLEFn(GLenum mode,
                                              GLint first,
                                              GLsizei count,
                                              GLsizei primcount) {
  GL_SERVICE_LOG("glDrawArraysInstancedANGLE"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << first << ", "
                 << count << ", " << primcount << ")");
  gl_api_->glDrawArraysInstancedANGLEFn(mode, first, count, primcount);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDrawBufferFn(GLenum mode) {
  GL_SERVICE_LOG("glDrawBuffer"
                 << "(" << GLEnums::GetStringEnum(mode) << ")");
  gl_api_->glDrawBufferFn(mode);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) {
  GL_SERVICE_LOG("glDrawBuffersARB"
                 << "(" << n << ", " << static_cast<const void*>(bufs) << ")");
  gl_api_->glDrawBuffersARBFn(n, bufs);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDrawElementsFn(GLenum mode,
                                  GLsizei count,
                                  GLenum type,
                                  const void* indices) {
  GL_SERVICE_LOG("glDrawElements"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << count << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(indices) << ")");
  gl_api_->glDrawElementsFn(mode, count, type, indices);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDrawElementsInstancedANGLEFn(GLenum mode,
                                                GLsizei count,
                                                GLenum type,
                                                const void* indices,
                                                GLsizei primcount) {
  GL_SERVICE_LOG("glDrawElementsInstancedANGLE"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << count << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(indices) << ", " << primcount
                 << ")");
  gl_api_->glDrawElementsInstancedANGLEFn(mode, count, type, indices,
                                          primcount);
}

DISABLE_CFI_ICALL
void DebugGLApi::glDrawRangeElementsFn(GLenum mode,
                                       GLuint start,
                                       GLuint end,
                                       GLsizei count,
                                       GLenum type,
                                       const void* indices) {
  GL_SERVICE_LOG("glDrawRangeElements"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << start << ", "
                 << end << ", " << count << ", " << GLEnums::GetStringEnum(type)
                 << ", " << static_cast<const void*>(indices) << ")");
  gl_api_->glDrawRangeElementsFn(mode, start, end, count, type, indices);
}

DISABLE_CFI_ICALL
void DebugGLApi::glEGLImageTargetRenderbufferStorageOESFn(GLenum target,
                                                          GLeglImageOES image) {
  GL_SERVICE_LOG("glEGLImageTargetRenderbufferStorageOES"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << image
                 << ")");
  gl_api_->glEGLImageTargetRenderbufferStorageOESFn(target, image);
}

DISABLE_CFI_ICALL
void DebugGLApi::glEGLImageTargetTexture2DOESFn(GLenum target,
                                                GLeglImageOES image) {
  GL_SERVICE_LOG("glEGLImageTargetTexture2DOES"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << image
                 << ")");
  gl_api_->glEGLImageTargetTexture2DOESFn(target, image);
}

DISABLE_CFI_ICALL
void DebugGLApi::glEnableFn(GLenum cap) {
  GL_SERVICE_LOG("glEnable"
                 << "(" << GLEnums::GetStringEnum(cap) << ")");
  gl_api_->glEnableFn(cap);
}

DISABLE_CFI_ICALL
void DebugGLApi::glEnableVertexAttribArrayFn(GLuint index) {
  GL_SERVICE_LOG("glEnableVertexAttribArray"
                 << "(" << index << ")");
  gl_api_->glEnableVertexAttribArrayFn(index);
}

DISABLE_CFI_ICALL
void DebugGLApi::glEndQueryFn(GLenum target) {
  GL_SERVICE_LOG("glEndQuery"
                 << "(" << GLEnums::GetStringEnum(target) << ")");
  gl_api_->glEndQueryFn(target);
}

DISABLE_CFI_ICALL
void DebugGLApi::glEndTransformFeedbackFn(void) {
  GL_SERVICE_LOG("glEndTransformFeedback"
                 << "("
                 << ")");
  gl_api_->glEndTransformFeedbackFn();
}

DISABLE_CFI_ICALL
GLsync DebugGLApi::glFenceSyncFn(GLenum condition, GLbitfield flags) {
  GL_SERVICE_LOG("glFenceSync"
                 << "(" << GLEnums::GetStringEnum(condition) << ", " << flags
                 << ")");
  GLsync result = gl_api_->glFenceSyncFn(condition, flags);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glFinishFn(void) {
  GL_SERVICE_LOG("glFinish"
                 << "("
                 << ")");
  gl_api_->glFinishFn();
}

DISABLE_CFI_ICALL
void DebugGLApi::glFinishFenceAPPLEFn(GLuint fence) {
  GL_SERVICE_LOG("glFinishFenceAPPLE"
                 << "(" << fence << ")");
  gl_api_->glFinishFenceAPPLEFn(fence);
}

DISABLE_CFI_ICALL
void DebugGLApi::glFinishFenceNVFn(GLuint fence) {
  GL_SERVICE_LOG("glFinishFenceNV"
                 << "(" << fence << ")");
  gl_api_->glFinishFenceNVFn(fence);
}

DISABLE_CFI_ICALL
void DebugGLApi::glFlushFn(void) {
  GL_SERVICE_LOG("glFlush"
                 << "("
                 << ")");
  gl_api_->glFlushFn();
}

DISABLE_CFI_ICALL
void DebugGLApi::glFlushMappedBufferRangeFn(GLenum target,
                                            GLintptr offset,
                                            GLsizeiptr length) {
  GL_SERVICE_LOG("glFlushMappedBufferRange"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << offset
                 << ", " << length << ")");
  gl_api_->glFlushMappedBufferRangeFn(target, offset, length);
}

DISABLE_CFI_ICALL
void DebugGLApi::glFramebufferRenderbufferEXTFn(GLenum target,
                                                GLenum attachment,
                                                GLenum renderbuffertarget,
                                                GLuint renderbuffer) {
  GL_SERVICE_LOG("glFramebufferRenderbufferEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(renderbuffertarget) << ", "
                 << renderbuffer << ")");
  gl_api_->glFramebufferRenderbufferEXTFn(target, attachment,
                                          renderbuffertarget, renderbuffer);
}

DISABLE_CFI_ICALL
void DebugGLApi::glFramebufferTexture2DEXTFn(GLenum target,
                                             GLenum attachment,
                                             GLenum textarget,
                                             GLuint texture,
                                             GLint level) {
  GL_SERVICE_LOG("glFramebufferTexture2DEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(textarget) << ", " << texture << ", "
                 << level << ")");
  gl_api_->glFramebufferTexture2DEXTFn(target, attachment, textarget, texture,
                                       level);
}

DISABLE_CFI_ICALL
void DebugGLApi::glFramebufferTexture2DMultisampleEXTFn(GLenum target,
                                                        GLenum attachment,
                                                        GLenum textarget,
                                                        GLuint texture,
                                                        GLint level,
                                                        GLsizei samples) {
  GL_SERVICE_LOG("glFramebufferTexture2DMultisampleEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(textarget) << ", " << texture << ", "
                 << level << ", " << samples << ")");
  gl_api_->glFramebufferTexture2DMultisampleEXTFn(target, attachment, textarget,
                                                  texture, level, samples);
}

DISABLE_CFI_ICALL
void DebugGLApi::glFramebufferTextureLayerFn(GLenum target,
                                             GLenum attachment,
                                             GLuint texture,
                                             GLint level,
                                             GLint layer) {
  GL_SERVICE_LOG("glFramebufferTextureLayer"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", " << texture
                 << ", " << level << ", " << layer << ")");
  gl_api_->glFramebufferTextureLayerFn(target, attachment, texture, level,
                                       layer);
}

DISABLE_CFI_ICALL
void DebugGLApi::glFrontFaceFn(GLenum mode) {
  GL_SERVICE_LOG("glFrontFace"
                 << "(" << GLEnums::GetStringEnum(mode) << ")");
  gl_api_->glFrontFaceFn(mode);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGenBuffersARBFn(GLsizei n, GLuint* buffers) {
  GL_SERVICE_LOG("glGenBuffersARB"
                 << "(" << n << ", " << static_cast<const void*>(buffers)
                 << ")");
  gl_api_->glGenBuffersARBFn(n, buffers);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGenerateMipmapEXTFn(GLenum target) {
  GL_SERVICE_LOG("glGenerateMipmapEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ")");
  gl_api_->glGenerateMipmapEXTFn(target);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGenFencesAPPLEFn(GLsizei n, GLuint* fences) {
  GL_SERVICE_LOG("glGenFencesAPPLE"
                 << "(" << n << ", " << static_cast<const void*>(fences)
                 << ")");
  gl_api_->glGenFencesAPPLEFn(n, fences);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGenFencesNVFn(GLsizei n, GLuint* fences) {
  GL_SERVICE_LOG("glGenFencesNV"
                 << "(" << n << ", " << static_cast<const void*>(fences)
                 << ")");
  gl_api_->glGenFencesNVFn(n, fences);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) {
  GL_SERVICE_LOG("glGenFramebuffersEXT"
                 << "(" << n << ", " << static_cast<const void*>(framebuffers)
                 << ")");
  gl_api_->glGenFramebuffersEXTFn(n, framebuffers);
}

DISABLE_CFI_ICALL
GLuint DebugGLApi::glGenPathsNVFn(GLsizei range) {
  GL_SERVICE_LOG("glGenPathsNV"
                 << "(" << range << ")");
  GLuint result = gl_api_->glGenPathsNVFn(range);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glGenQueriesFn(GLsizei n, GLuint* ids) {
  GL_SERVICE_LOG("glGenQueries"
                 << "(" << n << ", " << static_cast<const void*>(ids) << ")");
  gl_api_->glGenQueriesFn(n, ids);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) {
  GL_SERVICE_LOG("glGenRenderbuffersEXT"
                 << "(" << n << ", " << static_cast<const void*>(renderbuffers)
                 << ")");
  gl_api_->glGenRenderbuffersEXTFn(n, renderbuffers);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGenSamplersFn(GLsizei n, GLuint* samplers) {
  GL_SERVICE_LOG("glGenSamplers"
                 << "(" << n << ", " << static_cast<const void*>(samplers)
                 << ")");
  gl_api_->glGenSamplersFn(n, samplers);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGenTexturesFn(GLsizei n, GLuint* textures) {
  GL_SERVICE_LOG("glGenTextures"
                 << "(" << n << ", " << static_cast<const void*>(textures)
                 << ")");
  gl_api_->glGenTexturesFn(n, textures);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) {
  GL_SERVICE_LOG("glGenTransformFeedbacks"
                 << "(" << n << ", " << static_cast<const void*>(ids) << ")");
  gl_api_->glGenTransformFeedbacksFn(n, ids);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) {
  GL_SERVICE_LOG("glGenVertexArraysOES"
                 << "(" << n << ", " << static_cast<const void*>(arrays)
                 << ")");
  gl_api_->glGenVertexArraysOESFn(n, arrays);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetActiveAttribFn(GLuint program,
                                     GLuint index,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     GLint* size,
                                     GLenum* type,
                                     char* name) {
  GL_SERVICE_LOG("glGetActiveAttrib"
                 << "(" << program << ", " << index << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(size) << ", "
                 << static_cast<const void*>(type) << ", "
                 << static_cast<const void*>(name) << ")");
  gl_api_->glGetActiveAttribFn(program, index, bufsize, length, size, type,
                               name);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetActiveUniformFn(GLuint program,
                                      GLuint index,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      GLint* size,
                                      GLenum* type,
                                      char* name) {
  GL_SERVICE_LOG("glGetActiveUniform"
                 << "(" << program << ", " << index << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(size) << ", "
                 << static_cast<const void*>(type) << ", "
                 << static_cast<const void*>(name) << ")");
  gl_api_->glGetActiveUniformFn(program, index, bufsize, length, size, type,
                                name);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetActiveUniformBlockivFn(GLuint program,
                                             GLuint uniformBlockIndex,
                                             GLenum pname,
                                             GLint* params) {
  GL_SERVICE_LOG("glGetActiveUniformBlockiv"
                 << "(" << program << ", " << uniformBlockIndex << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetActiveUniformBlockivFn(program, uniformBlockIndex, pname,
                                       params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetActiveUniformBlockivRobustANGLEFn(
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  GL_SERVICE_LOG("glGetActiveUniformBlockivRobustANGLE"
                 << "(" << program << ", " << uniformBlockIndex << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetActiveUniformBlockivRobustANGLEFn(
      program, uniformBlockIndex, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetActiveUniformBlockNameFn(GLuint program,
                                               GLuint uniformBlockIndex,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               char* uniformBlockName) {
  GL_SERVICE_LOG("glGetActiveUniformBlockName"
                 << "(" << program << ", " << uniformBlockIndex << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(uniformBlockName) << ")");
  gl_api_->glGetActiveUniformBlockNameFn(program, uniformBlockIndex, bufSize,
                                         length, uniformBlockName);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetActiveUniformsivFn(GLuint program,
                                         GLsizei uniformCount,
                                         const GLuint* uniformIndices,
                                         GLenum pname,
                                         GLint* params) {
  GL_SERVICE_LOG("glGetActiveUniformsiv"
                 << "(" << program << ", " << uniformCount << ", "
                 << static_cast<const void*>(uniformIndices) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetActiveUniformsivFn(program, uniformCount, uniformIndices, pname,
                                   params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetAttachedShadersFn(GLuint program,
                                        GLsizei maxcount,
                                        GLsizei* count,
                                        GLuint* shaders) {
  GL_SERVICE_LOG("glGetAttachedShaders"
                 << "(" << program << ", " << maxcount << ", "
                 << static_cast<const void*>(count) << ", "
                 << static_cast<const void*>(shaders) << ")");
  gl_api_->glGetAttachedShadersFn(program, maxcount, count, shaders);
}

DISABLE_CFI_ICALL
GLint DebugGLApi::glGetAttribLocationFn(GLuint program, const char* name) {
  GL_SERVICE_LOG("glGetAttribLocation"
                 << "(" << program << ", " << name << ")");
  GLint result = gl_api_->glGetAttribLocationFn(program, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetBooleani_vRobustANGLEFn(GLenum target,
                                              GLuint index,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLboolean* data) {
  GL_SERVICE_LOG("glGetBooleani_vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glGetBooleani_vRobustANGLEFn(target, index, bufSize, length, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetBooleanvFn(GLenum pname, GLboolean* params) {
  GL_SERVICE_LOG("glGetBooleanv"
                 << "(" << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetBooleanvFn(pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetBooleanvRobustANGLEFn(GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLboolean* data) {
  GL_SERVICE_LOG("glGetBooleanvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glGetBooleanvRobustANGLEFn(pname, bufSize, length, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetBufferParameteri64vRobustANGLEFn(GLenum target,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint64* params) {
  GL_SERVICE_LOG("glGetBufferParameteri64vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetBufferParameteri64vRobustANGLEFn(target, pname, bufSize, length,
                                                 params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetBufferParameterivFn(GLenum target,
                                          GLenum pname,
                                          GLint* params) {
  GL_SERVICE_LOG("glGetBufferParameteriv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetBufferParameterivFn(target, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetBufferParameterivRobustANGLEFn(GLenum target,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  GL_SERVICE_LOG("glGetBufferParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetBufferParameterivRobustANGLEFn(target, pname, bufSize, length,
                                               params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetBufferPointervRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  void** params) {
  GL_SERVICE_LOG("glGetBufferPointervRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", " << params << ")");
  gl_api_->glGetBufferPointervRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetDebugMessageLogFn(GLuint count,
                                        GLsizei bufSize,
                                        GLenum* sources,
                                        GLenum* types,
                                        GLuint* ids,
                                        GLenum* severities,
                                        GLsizei* lengths,
                                        char* messageLog) {
  GL_SERVICE_LOG("glGetDebugMessageLog"
                 << "(" << count << ", " << bufSize << ", "
                 << static_cast<const void*>(sources) << ", "
                 << static_cast<const void*>(types) << ", "
                 << static_cast<const void*>(ids) << ", "
                 << static_cast<const void*>(severities) << ", "
                 << static_cast<const void*>(lengths) << ", "
                 << static_cast<const void*>(messageLog) << ")");
  gl_api_->glGetDebugMessageLogFn(count, bufSize, sources, types, ids,
                                  severities, lengths, messageLog);
}

DISABLE_CFI_ICALL
GLenum DebugGLApi::glGetErrorFn(void) {
  GL_SERVICE_LOG("glGetError"
                 << "("
                 << ")");
  GLenum result = gl_api_->glGetErrorFn();

  GL_SERVICE_LOG("GL_RESULT: " << GLEnums::GetStringError(result));

  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetFenceivNVFn(GLuint fence, GLenum pname, GLint* params) {
  GL_SERVICE_LOG("glGetFenceivNV"
                 << "(" << fence << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetFenceivNVFn(fence, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetFloatvFn(GLenum pname, GLfloat* params) {
  GL_SERVICE_LOG("glGetFloatv"
                 << "(" << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetFloatvFn(pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetFloatvRobustANGLEFn(GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLfloat* data) {
  GL_SERVICE_LOG("glGetFloatvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glGetFloatvRobustANGLEFn(pname, bufSize, length, data);
}

DISABLE_CFI_ICALL
GLint DebugGLApi::glGetFragDataIndexFn(GLuint program, const char* name) {
  GL_SERVICE_LOG("glGetFragDataIndex"
                 << "(" << program << ", " << name << ")");
  GLint result = gl_api_->glGetFragDataIndexFn(program, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLint DebugGLApi::glGetFragDataLocationFn(GLuint program, const char* name) {
  GL_SERVICE_LOG("glGetFragDataLocation"
                 << "(" << program << ", " << name << ")");
  GLint result = gl_api_->glGetFragDataLocationFn(program, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetFramebufferAttachmentParameterivEXTFn(GLenum target,
                                                            GLenum attachment,
                                                            GLenum pname,
                                                            GLint* params) {
  GL_SERVICE_LOG("glGetFramebufferAttachmentParameterivEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetFramebufferAttachmentParameterivEXTFn(target, attachment, pname,
                                                      params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetFramebufferAttachmentParameterivRobustANGLEFn(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  GL_SERVICE_LOG("glGetFramebufferAttachmentParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetFramebufferAttachmentParameterivRobustANGLEFn(
      target, attachment, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetFramebufferParameterivRobustANGLEFn(GLenum target,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params) {
  GL_SERVICE_LOG("glGetFramebufferParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetFramebufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                    length, params);
}

DISABLE_CFI_ICALL
GLenum DebugGLApi::glGetGraphicsResetStatusARBFn(void) {
  GL_SERVICE_LOG("glGetGraphicsResetStatusARB"
                 << "("
                 << ")");
  GLenum result = gl_api_->glGetGraphicsResetStatusARBFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetInteger64i_vFn(GLenum target,
                                     GLuint index,
                                     GLint64* data) {
  GL_SERVICE_LOG("glGetInteger64i_v"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glGetInteger64i_vFn(target, index, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetInteger64i_vRobustANGLEFn(GLenum target,
                                                GLuint index,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint64* data) {
  GL_SERVICE_LOG("glGetInteger64i_vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glGetInteger64i_vRobustANGLEFn(target, index, bufSize, length, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetInteger64vFn(GLenum pname, GLint64* params) {
  GL_SERVICE_LOG("glGetInteger64v"
                 << "(" << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetInteger64vFn(pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetInteger64vRobustANGLEFn(GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint64* data) {
  GL_SERVICE_LOG("glGetInteger64vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glGetInteger64vRobustANGLEFn(pname, bufSize, length, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetIntegeri_vFn(GLenum target, GLuint index, GLint* data) {
  GL_SERVICE_LOG("glGetIntegeri_v"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glGetIntegeri_vFn(target, index, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetIntegeri_vRobustANGLEFn(GLenum target,
                                              GLuint index,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* data) {
  GL_SERVICE_LOG("glGetIntegeri_vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(data) << ")");
  gl_api_->glGetIntegeri_vRobustANGLEFn(target, index, bufSize, length, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetIntegervFn(GLenum pname, GLint* params) {
  GL_SERVICE_LOG("glGetIntegerv"
                 << "(" << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetIntegervFn(pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetIntegervRobustANGLEFn(GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* data) {
  GL_SERVICE_LOG("glGetIntegervRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glGetIntegervRobustANGLEFn(pname, bufSize, length, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetInternalformativFn(GLenum target,
                                         GLenum internalformat,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLint* params) {
  GL_SERVICE_LOG("glGetInternalformativ"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(internalformat) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetInternalformativFn(target, internalformat, pname, bufSize,
                                   params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetInternalformativRobustANGLEFn(GLenum target,
                                                    GLenum internalformat,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint* params) {
  GL_SERVICE_LOG("glGetInternalformativRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(internalformat) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetInternalformativRobustANGLEFn(target, internalformat, pname,
                                              bufSize, length, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetMultisamplefvFn(GLenum pname,
                                      GLuint index,
                                      GLfloat* val) {
  GL_SERVICE_LOG("glGetMultisamplefv"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << index
                 << ", " << static_cast<const void*>(val) << ")");
  gl_api_->glGetMultisamplefvFn(pname, index, val);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetMultisamplefvRobustANGLEFn(GLenum pname,
                                                 GLuint index,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* val) {
  GL_SERVICE_LOG("glGetMultisamplefvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(val) << ")");
  gl_api_->glGetMultisamplefvRobustANGLEFn(pname, index, bufSize, length, val);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetnUniformfvRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLfloat* params) {
  GL_SERVICE_LOG("glGetnUniformfvRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetnUniformfvRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetnUniformivRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* params) {
  GL_SERVICE_LOG("glGetnUniformivRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetnUniformivRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetnUniformuivRobustANGLEFn(GLuint program,
                                               GLint location,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLuint* params) {
  GL_SERVICE_LOG("glGetnUniformuivRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetnUniformuivRobustANGLEFn(program, location, bufSize, length,
                                         params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetObjectLabelFn(GLenum identifier,
                                    GLuint name,
                                    GLsizei bufSize,
                                    GLsizei* length,
                                    char* label) {
  GL_SERVICE_LOG("glGetObjectLabel"
                 << "(" << GLEnums::GetStringEnum(identifier) << ", " << name
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(label) << ")");
  gl_api_->glGetObjectLabelFn(identifier, name, bufSize, length, label);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetObjectPtrLabelFn(void* ptr,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       char* label) {
  GL_SERVICE_LOG("glGetObjectPtrLabel"
                 << "(" << static_cast<const void*>(ptr) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(label) << ")");
  gl_api_->glGetObjectPtrLabelFn(ptr, bufSize, length, label);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetPointervFn(GLenum pname, void** params) {
  GL_SERVICE_LOG("glGetPointerv"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << params
                 << ")");
  gl_api_->glGetPointervFn(pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetPointervRobustANGLERobustANGLEFn(GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       void** params) {
  GL_SERVICE_LOG("glGetPointervRobustANGLERobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", " << params
                 << ")");
  gl_api_->glGetPointervRobustANGLERobustANGLEFn(pname, bufSize, length,
                                                 params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetProgramBinaryFn(GLuint program,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLenum* binaryFormat,
                                      GLvoid* binary) {
  GL_SERVICE_LOG("glGetProgramBinary"
                 << "(" << program << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(binaryFormat) << ", "
                 << static_cast<const void*>(binary) << ")");
  gl_api_->glGetProgramBinaryFn(program, bufSize, length, binaryFormat, binary);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetProgramInfoLogFn(GLuint program,
                                       GLsizei bufsize,
                                       GLsizei* length,
                                       char* infolog) {
  GL_SERVICE_LOG("glGetProgramInfoLog"
                 << "(" << program << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(infolog) << ")");
  gl_api_->glGetProgramInfoLogFn(program, bufsize, length, infolog);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetProgramInterfaceivFn(GLuint program,
                                           GLenum programInterface,
                                           GLenum pname,
                                           GLint* params) {
  GL_SERVICE_LOG("glGetProgramInterfaceiv"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetProgramInterfaceivFn(program, programInterface, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetProgramInterfaceivRobustANGLEFn(GLuint program,
                                                      GLenum programInterface,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  GL_SERVICE_LOG("glGetProgramInterfaceivRobustANGLE"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetProgramInterfaceivRobustANGLEFn(program, programInterface,
                                                pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetProgramivFn(GLuint program, GLenum pname, GLint* params) {
  GL_SERVICE_LOG("glGetProgramiv"
                 << "(" << program << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetProgramivFn(program, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetProgramivRobustANGLEFn(GLuint program,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  GL_SERVICE_LOG("glGetProgramivRobustANGLE"
                 << "(" << program << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetProgramivRobustANGLEFn(program, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetProgramResourceivFn(GLuint program,
                                          GLenum programInterface,
                                          GLuint index,
                                          GLsizei propCount,
                                          const GLenum* props,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params) {
  GL_SERVICE_LOG("glGetProgramResourceiv"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", " << index
                 << ", " << propCount << ", " << static_cast<const void*>(props)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetProgramResourceivFn(program, programInterface, index, propCount,
                                    props, bufSize, length, params);
}

DISABLE_CFI_ICALL
GLint DebugGLApi::glGetProgramResourceLocationFn(GLuint program,
                                                 GLenum programInterface,
                                                 const char* name) {
  GL_SERVICE_LOG("glGetProgramResourceLocation"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", " << name
                 << ")");
  GLint result =
      gl_api_->glGetProgramResourceLocationFn(program, programInterface, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetProgramResourceNameFn(GLuint program,
                                            GLenum programInterface,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLchar* name) {
  GL_SERVICE_LOG("glGetProgramResourceName"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(name) << ")");
  gl_api_->glGetProgramResourceNameFn(program, programInterface, index, bufSize,
                                      length, name);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetQueryivFn(GLenum target, GLenum pname, GLint* params) {
  GL_SERVICE_LOG("glGetQueryiv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryivFn(target, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetQueryivRobustANGLEFn(GLenum target,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) {
  GL_SERVICE_LOG("glGetQueryivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryivRobustANGLEFn(target, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetQueryObjecti64vFn(GLuint id,
                                        GLenum pname,
                                        GLint64* params) {
  GL_SERVICE_LOG("glGetQueryObjecti64v"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjecti64vFn(id, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetQueryObjecti64vRobustANGLEFn(GLuint id,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint64* params) {
  GL_SERVICE_LOG("glGetQueryObjecti64vRobustANGLE"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjecti64vRobustANGLEFn(id, pname, bufSize, length,
                                             params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetQueryObjectivFn(GLuint id, GLenum pname, GLint* params) {
  GL_SERVICE_LOG("glGetQueryObjectiv"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjectivFn(id, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetQueryObjectivRobustANGLEFn(GLuint id,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  GL_SERVICE_LOG("glGetQueryObjectivRobustANGLE"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjectivRobustANGLEFn(id, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetQueryObjectui64vFn(GLuint id,
                                         GLenum pname,
                                         GLuint64* params) {
  GL_SERVICE_LOG("glGetQueryObjectui64v"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjectui64vFn(id, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetQueryObjectui64vRobustANGLEFn(GLuint id,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint64* params) {
  GL_SERVICE_LOG("glGetQueryObjectui64vRobustANGLE"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjectui64vRobustANGLEFn(id, pname, bufSize, length,
                                              params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetQueryObjectuivFn(GLuint id,
                                       GLenum pname,
                                       GLuint* params) {
  GL_SERVICE_LOG("glGetQueryObjectuiv"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjectuivFn(id, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetQueryObjectuivRobustANGLEFn(GLuint id,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLuint* params) {
  GL_SERVICE_LOG("glGetQueryObjectuivRobustANGLE"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetQueryObjectuivRobustANGLEFn(id, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetRenderbufferParameterivEXTFn(GLenum target,
                                                   GLenum pname,
                                                   GLint* params) {
  GL_SERVICE_LOG("glGetRenderbufferParameterivEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetRenderbufferParameterivEXTFn(target, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetRenderbufferParameterivRobustANGLEFn(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params) {
  GL_SERVICE_LOG("glGetRenderbufferParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetRenderbufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                     length, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetSamplerParameterfvFn(GLuint sampler,
                                           GLenum pname,
                                           GLfloat* params) {
  GL_SERVICE_LOG("glGetSamplerParameterfv"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetSamplerParameterfvFn(sampler, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  GL_SERVICE_LOG("glGetSamplerParameterfvRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize, length,
                                                params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  GL_SERVICE_LOG("glGetSamplerParameterIivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize,
                                                 length, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLuint* params) {
  GL_SERVICE_LOG("glGetSamplerParameterIuivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize,
                                                  length, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetSamplerParameterivFn(GLuint sampler,
                                           GLenum pname,
                                           GLint* params) {
  GL_SERVICE_LOG("glGetSamplerParameteriv"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetSamplerParameterivFn(sampler, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  GL_SERVICE_LOG("glGetSamplerParameterivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetSamplerParameterivRobustANGLEFn(sampler, pname, bufSize, length,
                                                params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetShaderInfoLogFn(GLuint shader,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      char* infolog) {
  GL_SERVICE_LOG("glGetShaderInfoLog"
                 << "(" << shader << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(infolog) << ")");
  gl_api_->glGetShaderInfoLogFn(shader, bufsize, length, infolog);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetShaderivFn(GLuint shader, GLenum pname, GLint* params) {
  GL_SERVICE_LOG("glGetShaderiv"
                 << "(" << shader << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetShaderivFn(shader, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetShaderivRobustANGLEFn(GLuint shader,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* params) {
  GL_SERVICE_LOG("glGetShaderivRobustANGLE"
                 << "(" << shader << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetShaderivRobustANGLEFn(shader, pname, bufSize, length, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetShaderPrecisionFormatFn(GLenum shadertype,
                                              GLenum precisiontype,
                                              GLint* range,
                                              GLint* precision) {
  GL_SERVICE_LOG("glGetShaderPrecisionFormat"
                 << "(" << GLEnums::GetStringEnum(shadertype) << ", "
                 << GLEnums::GetStringEnum(precisiontype) << ", "
                 << static_cast<const void*>(range) << ", "
                 << static_cast<const void*>(precision) << ")");
  gl_api_->glGetShaderPrecisionFormatFn(shadertype, precisiontype, range,
                                        precision);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetShaderSourceFn(GLuint shader,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     char* source) {
  GL_SERVICE_LOG("glGetShaderSource"
                 << "(" << shader << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(source) << ")");
  gl_api_->glGetShaderSourceFn(shader, bufsize, length, source);
}

DISABLE_CFI_ICALL
const GLubyte* DebugGLApi::glGetStringFn(GLenum name) {
  GL_SERVICE_LOG("glGetString"
                 << "(" << GLEnums::GetStringEnum(name) << ")");
  const GLubyte* result = gl_api_->glGetStringFn(name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
const GLubyte* DebugGLApi::glGetStringiFn(GLenum name, GLuint index) {
  GL_SERVICE_LOG("glGetStringi"
                 << "(" << GLEnums::GetStringEnum(name) << ", " << index
                 << ")");
  const GLubyte* result = gl_api_->glGetStringiFn(name, index);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetSyncivFn(GLsync sync,
                               GLenum pname,
                               GLsizei bufSize,
                               GLsizei* length,
                               GLint* values) {
  GL_SERVICE_LOG("glGetSynciv"
                 << "(" << sync << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(values) << ")");
  gl_api_->glGetSyncivFn(sync, pname, bufSize, length, values);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetTexLevelParameterfvFn(GLenum target,
                                            GLint level,
                                            GLenum pname,
                                            GLfloat* params) {
  GL_SERVICE_LOG("glGetTexLevelParameterfv"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexLevelParameterfvFn(target, level, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetTexLevelParameterfvRobustANGLEFn(GLenum target,
                                                       GLint level,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLfloat* params) {
  GL_SERVICE_LOG("glGetTexLevelParameterfvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexLevelParameterfvRobustANGLEFn(target, level, pname, bufSize,
                                                 length, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetTexLevelParameterivFn(GLenum target,
                                            GLint level,
                                            GLenum pname,
                                            GLint* params) {
  GL_SERVICE_LOG("glGetTexLevelParameteriv"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexLevelParameterivFn(target, level, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetTexLevelParameterivRobustANGLEFn(GLenum target,
                                                       GLint level,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  GL_SERVICE_LOG("glGetTexLevelParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexLevelParameterivRobustANGLEFn(target, level, pname, bufSize,
                                                 length, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetTexParameterfvFn(GLenum target,
                                       GLenum pname,
                                       GLfloat* params) {
  GL_SERVICE_LOG("glGetTexParameterfv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexParameterfvFn(target, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetTexParameterfvRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLfloat* params) {
  GL_SERVICE_LOG("glGetTexParameterfvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexParameterfvRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetTexParameterIivRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) {
  GL_SERVICE_LOG("glGetTexParameterIivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexParameterIivRobustANGLEFn(target, pname, bufSize, length,
                                             params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetTexParameterIuivRobustANGLEFn(GLenum target,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint* params) {
  GL_SERVICE_LOG("glGetTexParameterIuivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexParameterIuivRobustANGLEFn(target, pname, bufSize, length,
                                              params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetTexParameterivFn(GLenum target,
                                       GLenum pname,
                                       GLint* params) {
  GL_SERVICE_LOG("glGetTexParameteriv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexParameterivFn(target, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetTexParameterivRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  GL_SERVICE_LOG("glGetTexParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetTexParameterivRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetTransformFeedbackVaryingFn(GLuint program,
                                                 GLuint index,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLsizei* size,
                                                 GLenum* type,
                                                 char* name) {
  GL_SERVICE_LOG("glGetTransformFeedbackVarying"
                 << "(" << program << ", " << index << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(size) << ", "
                 << static_cast<const void*>(type) << ", "
                 << static_cast<const void*>(name) << ")");
  gl_api_->glGetTransformFeedbackVaryingFn(program, index, bufSize, length,
                                           size, type, name);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetTranslatedShaderSourceANGLEFn(GLuint shader,
                                                    GLsizei bufsize,
                                                    GLsizei* length,
                                                    char* source) {
  GL_SERVICE_LOG("glGetTranslatedShaderSourceANGLE"
                 << "(" << shader << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(source) << ")");
  gl_api_->glGetTranslatedShaderSourceANGLEFn(shader, bufsize, length, source);
}

DISABLE_CFI_ICALL
GLuint DebugGLApi::glGetUniformBlockIndexFn(GLuint program,
                                            const char* uniformBlockName) {
  GL_SERVICE_LOG("glGetUniformBlockIndex"
                 << "(" << program << ", " << uniformBlockName << ")");
  GLuint result = gl_api_->glGetUniformBlockIndexFn(program, uniformBlockName);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetUniformfvFn(GLuint program,
                                  GLint location,
                                  GLfloat* params) {
  GL_SERVICE_LOG("glGetUniformfv"
                 << "(" << program << ", " << location << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetUniformfvFn(program, location, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetUniformfvRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLfloat* params) {
  GL_SERVICE_LOG("glGetUniformfvRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetUniformfvRobustANGLEFn(program, location, bufSize, length,
                                       params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetUniformIndicesFn(GLuint program,
                                       GLsizei uniformCount,
                                       const char* const* uniformNames,
                                       GLuint* uniformIndices) {
  GL_SERVICE_LOG("glGetUniformIndices"
                 << "(" << program << ", " << uniformCount << ", "
                 << static_cast<const void*>(uniformNames) << ", "
                 << static_cast<const void*>(uniformIndices) << ")");
  gl_api_->glGetUniformIndicesFn(program, uniformCount, uniformNames,
                                 uniformIndices);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetUniformivFn(GLuint program,
                                  GLint location,
                                  GLint* params) {
  GL_SERVICE_LOG("glGetUniformiv"
                 << "(" << program << ", " << location << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetUniformivFn(program, location, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetUniformivRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  GL_SERVICE_LOG("glGetUniformivRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetUniformivRobustANGLEFn(program, location, bufSize, length,
                                       params);
}

DISABLE_CFI_ICALL
GLint DebugGLApi::glGetUniformLocationFn(GLuint program, const char* name) {
  GL_SERVICE_LOG("glGetUniformLocation"
                 << "(" << program << ", " << name << ")");
  GLint result = gl_api_->glGetUniformLocationFn(program, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetUniformuivFn(GLuint program,
                                   GLint location,
                                   GLuint* params) {
  GL_SERVICE_LOG("glGetUniformuiv"
                 << "(" << program << ", " << location << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetUniformuivFn(program, location, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetUniformuivRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLuint* params) {
  GL_SERVICE_LOG("glGetUniformuivRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glGetUniformuivRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetVertexAttribfvFn(GLuint index,
                                       GLenum pname,
                                       GLfloat* params) {
  GL_SERVICE_LOG("glGetVertexAttribfv"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetVertexAttribfvFn(index, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetVertexAttribfvRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLfloat* params) {
  GL_SERVICE_LOG("glGetVertexAttribfvRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetVertexAttribfvRobustANGLEFn(index, pname, bufSize, length,
                                            params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetVertexAttribIivRobustANGLEFn(GLuint index,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) {
  GL_SERVICE_LOG("glGetVertexAttribIivRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetVertexAttribIivRobustANGLEFn(index, pname, bufSize, length,
                                             params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetVertexAttribIuivRobustANGLEFn(GLuint index,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint* params) {
  GL_SERVICE_LOG("glGetVertexAttribIuivRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetVertexAttribIuivRobustANGLEFn(index, pname, bufSize, length,
                                              params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetVertexAttribivFn(GLuint index,
                                       GLenum pname,
                                       GLint* params) {
  GL_SERVICE_LOG("glGetVertexAttribiv"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetVertexAttribivFn(index, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetVertexAttribivRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  GL_SERVICE_LOG("glGetVertexAttribivRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glGetVertexAttribivRobustANGLEFn(index, pname, bufSize, length,
                                            params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetVertexAttribPointervFn(GLuint index,
                                             GLenum pname,
                                             void** pointer) {
  GL_SERVICE_LOG("glGetVertexAttribPointerv"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << pointer << ")");
  gl_api_->glGetVertexAttribPointervFn(index, pname, pointer);
}

DISABLE_CFI_ICALL
void DebugGLApi::glGetVertexAttribPointervRobustANGLEFn(GLuint index,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        void** pointer) {
  GL_SERVICE_LOG("glGetVertexAttribPointervRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << pointer << ")");
  gl_api_->glGetVertexAttribPointervRobustANGLEFn(index, pname, bufSize, length,
                                                  pointer);
}

DISABLE_CFI_ICALL
void DebugGLApi::glHintFn(GLenum target, GLenum mode) {
  GL_SERVICE_LOG("glHint"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(mode) << ")");
  gl_api_->glHintFn(target, mode);
}

DISABLE_CFI_ICALL
void DebugGLApi::glInsertEventMarkerEXTFn(GLsizei length, const char* marker) {
  GL_SERVICE_LOG("glInsertEventMarkerEXT"
                 << "(" << length << ", " << marker << ")");
  gl_api_->glInsertEventMarkerEXTFn(length, marker);
}

DISABLE_CFI_ICALL
void DebugGLApi::glInvalidateFramebufferFn(GLenum target,
                                           GLsizei numAttachments,
                                           const GLenum* attachments) {
  GL_SERVICE_LOG("glInvalidateFramebuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << numAttachments << ", "
                 << static_cast<const void*>(attachments) << ")");
  gl_api_->glInvalidateFramebufferFn(target, numAttachments, attachments);
}

DISABLE_CFI_ICALL
void DebugGLApi::glInvalidateSubFramebufferFn(GLenum target,
                                              GLsizei numAttachments,
                                              const GLenum* attachments,
                                              GLint x,
                                              GLint y,
                                              GLint width,
                                              GLint height) {
  GL_SERVICE_LOG("glInvalidateSubFramebuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << numAttachments << ", "
                 << static_cast<const void*>(attachments) << ", " << x << ", "
                 << y << ", " << width << ", " << height << ")");
  gl_api_->glInvalidateSubFramebufferFn(target, numAttachments, attachments, x,
                                        y, width, height);
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsBufferFn(GLuint buffer) {
  GL_SERVICE_LOG("glIsBuffer"
                 << "(" << buffer << ")");
  GLboolean result = gl_api_->glIsBufferFn(buffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsEnabledFn(GLenum cap) {
  GL_SERVICE_LOG("glIsEnabled"
                 << "(" << GLEnums::GetStringEnum(cap) << ")");
  GLboolean result = gl_api_->glIsEnabledFn(cap);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsFenceAPPLEFn(GLuint fence) {
  GL_SERVICE_LOG("glIsFenceAPPLE"
                 << "(" << fence << ")");
  GLboolean result = gl_api_->glIsFenceAPPLEFn(fence);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsFenceNVFn(GLuint fence) {
  GL_SERVICE_LOG("glIsFenceNV"
                 << "(" << fence << ")");
  GLboolean result = gl_api_->glIsFenceNVFn(fence);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsFramebufferEXTFn(GLuint framebuffer) {
  GL_SERVICE_LOG("glIsFramebufferEXT"
                 << "(" << framebuffer << ")");
  GLboolean result = gl_api_->glIsFramebufferEXTFn(framebuffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsPathNVFn(GLuint path) {
  GL_SERVICE_LOG("glIsPathNV"
                 << "(" << path << ")");
  GLboolean result = gl_api_->glIsPathNVFn(path);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsProgramFn(GLuint program) {
  GL_SERVICE_LOG("glIsProgram"
                 << "(" << program << ")");
  GLboolean result = gl_api_->glIsProgramFn(program);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsQueryFn(GLuint query) {
  GL_SERVICE_LOG("glIsQuery"
                 << "(" << query << ")");
  GLboolean result = gl_api_->glIsQueryFn(query);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsRenderbufferEXTFn(GLuint renderbuffer) {
  GL_SERVICE_LOG("glIsRenderbufferEXT"
                 << "(" << renderbuffer << ")");
  GLboolean result = gl_api_->glIsRenderbufferEXTFn(renderbuffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsSamplerFn(GLuint sampler) {
  GL_SERVICE_LOG("glIsSampler"
                 << "(" << sampler << ")");
  GLboolean result = gl_api_->glIsSamplerFn(sampler);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsShaderFn(GLuint shader) {
  GL_SERVICE_LOG("glIsShader"
                 << "(" << shader << ")");
  GLboolean result = gl_api_->glIsShaderFn(shader);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsSyncFn(GLsync sync) {
  GL_SERVICE_LOG("glIsSync"
                 << "(" << sync << ")");
  GLboolean result = gl_api_->glIsSyncFn(sync);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsTextureFn(GLuint texture) {
  GL_SERVICE_LOG("glIsTexture"
                 << "(" << texture << ")");
  GLboolean result = gl_api_->glIsTextureFn(texture);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsTransformFeedbackFn(GLuint id) {
  GL_SERVICE_LOG("glIsTransformFeedback"
                 << "(" << id << ")");
  GLboolean result = gl_api_->glIsTransformFeedbackFn(id);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glIsVertexArrayOESFn(GLuint array) {
  GL_SERVICE_LOG("glIsVertexArrayOES"
                 << "(" << array << ")");
  GLboolean result = gl_api_->glIsVertexArrayOESFn(array);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glLineWidthFn(GLfloat width) {
  GL_SERVICE_LOG("glLineWidth"
                 << "(" << width << ")");
  gl_api_->glLineWidthFn(width);
}

DISABLE_CFI_ICALL
void DebugGLApi::glLinkProgramFn(GLuint program) {
  GL_SERVICE_LOG("glLinkProgram"
                 << "(" << program << ")");
  gl_api_->glLinkProgramFn(program);
}

DISABLE_CFI_ICALL
void* DebugGLApi::glMapBufferFn(GLenum target, GLenum access) {
  GL_SERVICE_LOG("glMapBuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(access) << ")");
  void* result = gl_api_->glMapBufferFn(target, access);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void* DebugGLApi::glMapBufferRangeFn(GLenum target,
                                     GLintptr offset,
                                     GLsizeiptr length,
                                     GLbitfield access) {
  GL_SERVICE_LOG("glMapBufferRange"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << offset
                 << ", " << length << ", " << access << ")");
  void* result = gl_api_->glMapBufferRangeFn(target, offset, length, access);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glMatrixLoadfEXTFn(GLenum matrixMode, const GLfloat* m) {
  GL_SERVICE_LOG("glMatrixLoadfEXT"
                 << "(" << GLEnums::GetStringEnum(matrixMode) << ", "
                 << static_cast<const void*>(m) << ")");
  gl_api_->glMatrixLoadfEXTFn(matrixMode, m);
}

DISABLE_CFI_ICALL
void DebugGLApi::glMatrixLoadIdentityEXTFn(GLenum matrixMode) {
  GL_SERVICE_LOG("glMatrixLoadIdentityEXT"
                 << "(" << GLEnums::GetStringEnum(matrixMode) << ")");
  gl_api_->glMatrixLoadIdentityEXTFn(matrixMode);
}

DISABLE_CFI_ICALL
void DebugGLApi::glMemoryBarrierEXTFn(GLbitfield barriers) {
  GL_SERVICE_LOG("glMemoryBarrierEXT"
                 << "(" << barriers << ")");
  gl_api_->glMemoryBarrierEXTFn(barriers);
}

DISABLE_CFI_ICALL
void DebugGLApi::glObjectLabelFn(GLenum identifier,
                                 GLuint name,
                                 GLsizei length,
                                 const char* label) {
  GL_SERVICE_LOG("glObjectLabel"
                 << "(" << GLEnums::GetStringEnum(identifier) << ", " << name
                 << ", " << length << ", " << label << ")");
  gl_api_->glObjectLabelFn(identifier, name, length, label);
}

DISABLE_CFI_ICALL
void DebugGLApi::glObjectPtrLabelFn(void* ptr,
                                    GLsizei length,
                                    const char* label) {
  GL_SERVICE_LOG("glObjectPtrLabel"
                 << "(" << static_cast<const void*>(ptr) << ", " << length
                 << ", " << label << ")");
  gl_api_->glObjectPtrLabelFn(ptr, length, label);
}

DISABLE_CFI_ICALL
void DebugGLApi::glPathCommandsNVFn(GLuint path,
                                    GLsizei numCommands,
                                    const GLubyte* commands,
                                    GLsizei numCoords,
                                    GLenum coordType,
                                    const GLvoid* coords) {
  GL_SERVICE_LOG("glPathCommandsNV"
                 << "(" << path << ", " << numCommands << ", "
                 << static_cast<const void*>(commands) << ", " << numCoords
                 << ", " << GLEnums::GetStringEnum(coordType) << ", "
                 << static_cast<const void*>(coords) << ")");
  gl_api_->glPathCommandsNVFn(path, numCommands, commands, numCoords, coordType,
                              coords);
}

DISABLE_CFI_ICALL
void DebugGLApi::glPathParameterfNVFn(GLuint path,
                                      GLenum pname,
                                      GLfloat value) {
  GL_SERVICE_LOG("glPathParameterfNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << value << ")");
  gl_api_->glPathParameterfNVFn(path, pname, value);
}

DISABLE_CFI_ICALL
void DebugGLApi::glPathParameteriNVFn(GLuint path, GLenum pname, GLint value) {
  GL_SERVICE_LOG("glPathParameteriNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << value << ")");
  gl_api_->glPathParameteriNVFn(path, pname, value);
}

DISABLE_CFI_ICALL
void DebugGLApi::glPathStencilFuncNVFn(GLenum func, GLint ref, GLuint mask) {
  GL_SERVICE_LOG("glPathStencilFuncNV"
                 << "(" << GLEnums::GetStringEnum(func) << ", " << ref << ", "
                 << mask << ")");
  gl_api_->glPathStencilFuncNVFn(func, ref, mask);
}

DISABLE_CFI_ICALL
void DebugGLApi::glPauseTransformFeedbackFn(void) {
  GL_SERVICE_LOG("glPauseTransformFeedback"
                 << "("
                 << ")");
  gl_api_->glPauseTransformFeedbackFn();
}

DISABLE_CFI_ICALL
void DebugGLApi::glPixelStoreiFn(GLenum pname, GLint param) {
  GL_SERVICE_LOG("glPixelStorei"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << param
                 << ")");
  gl_api_->glPixelStoreiFn(pname, param);
}

DISABLE_CFI_ICALL
void DebugGLApi::glPointParameteriFn(GLenum pname, GLint param) {
  GL_SERVICE_LOG("glPointParameteri"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << param
                 << ")");
  gl_api_->glPointParameteriFn(pname, param);
}

DISABLE_CFI_ICALL
void DebugGLApi::glPolygonModeFn(GLenum face, GLenum mode) {
  GL_SERVICE_LOG("glPolygonMode"
                 << "(" << GLEnums::GetStringEnum(face) << ", "
                 << GLEnums::GetStringEnum(mode) << ")");
  gl_api_->glPolygonModeFn(face, mode);
}

DISABLE_CFI_ICALL
void DebugGLApi::glPolygonOffsetFn(GLfloat factor, GLfloat units) {
  GL_SERVICE_LOG("glPolygonOffset"
                 << "(" << factor << ", " << units << ")");
  gl_api_->glPolygonOffsetFn(factor, units);
}

DISABLE_CFI_ICALL
void DebugGLApi::glPopDebugGroupFn() {
  GL_SERVICE_LOG("glPopDebugGroup"
                 << "("
                 << ")");
  gl_api_->glPopDebugGroupFn();
}

DISABLE_CFI_ICALL
void DebugGLApi::glPopGroupMarkerEXTFn(void) {
  GL_SERVICE_LOG("glPopGroupMarkerEXT"
                 << "("
                 << ")");
  gl_api_->glPopGroupMarkerEXTFn();
}

DISABLE_CFI_ICALL
void DebugGLApi::glPrimitiveRestartIndexFn(GLuint index) {
  GL_SERVICE_LOG("glPrimitiveRestartIndex"
                 << "(" << index << ")");
  gl_api_->glPrimitiveRestartIndexFn(index);
}

DISABLE_CFI_ICALL
void DebugGLApi::glProgramBinaryFn(GLuint program,
                                   GLenum binaryFormat,
                                   const GLvoid* binary,
                                   GLsizei length) {
  GL_SERVICE_LOG("glProgramBinary"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(binaryFormat) << ", "
                 << static_cast<const void*>(binary) << ", " << length << ")");
  gl_api_->glProgramBinaryFn(program, binaryFormat, binary, length);
}

DISABLE_CFI_ICALL
void DebugGLApi::glProgramParameteriFn(GLuint program,
                                       GLenum pname,
                                       GLint value) {
  GL_SERVICE_LOG("glProgramParameteri"
                 << "(" << program << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << value << ")");
  gl_api_->glProgramParameteriFn(program, pname, value);
}

DISABLE_CFI_ICALL
void DebugGLApi::glProgramPathFragmentInputGenNVFn(GLuint program,
                                                   GLint location,
                                                   GLenum genMode,
                                                   GLint components,
                                                   const GLfloat* coeffs) {
  GL_SERVICE_LOG("glProgramPathFragmentInputGenNV"
                 << "(" << program << ", " << location << ", "
                 << GLEnums::GetStringEnum(genMode) << ", " << components
                 << ", " << static_cast<const void*>(coeffs) << ")");
  gl_api_->glProgramPathFragmentInputGenNVFn(program, location, genMode,
                                             components, coeffs);
}

DISABLE_CFI_ICALL
void DebugGLApi::glPushDebugGroupFn(GLenum source,
                                    GLuint id,
                                    GLsizei length,
                                    const char* message) {
  GL_SERVICE_LOG("glPushDebugGroup"
                 << "(" << GLEnums::GetStringEnum(source) << ", " << id << ", "
                 << length << ", " << message << ")");
  gl_api_->glPushDebugGroupFn(source, id, length, message);
}

DISABLE_CFI_ICALL
void DebugGLApi::glPushGroupMarkerEXTFn(GLsizei length, const char* marker) {
  GL_SERVICE_LOG("glPushGroupMarkerEXT"
                 << "(" << length << ", " << marker << ")");
  gl_api_->glPushGroupMarkerEXTFn(length, marker);
}

DISABLE_CFI_ICALL
void DebugGLApi::glQueryCounterFn(GLuint id, GLenum target) {
  GL_SERVICE_LOG("glQueryCounter"
                 << "(" << id << ", " << GLEnums::GetStringEnum(target) << ")");
  gl_api_->glQueryCounterFn(id, target);
}

DISABLE_CFI_ICALL
void DebugGLApi::glReadBufferFn(GLenum src) {
  GL_SERVICE_LOG("glReadBuffer"
                 << "(" << GLEnums::GetStringEnum(src) << ")");
  gl_api_->glReadBufferFn(src);
}

DISABLE_CFI_ICALL
void DebugGLApi::glReadnPixelsRobustANGLEFn(GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLenum type,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLsizei* columns,
                                            GLsizei* rows,
                                            void* data) {
  GL_SERVICE_LOG("glReadnPixelsRobustANGLE"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(columns) << ", "
                 << static_cast<const void*>(rows) << ", "
                 << static_cast<const void*>(data) << ")");
  gl_api_->glReadnPixelsRobustANGLEFn(x, y, width, height, format, type,
                                      bufSize, length, columns, rows, data);
}

DISABLE_CFI_ICALL
void DebugGLApi::glReadPixelsFn(GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                GLenum format,
                                GLenum type,
                                void* pixels) {
  GL_SERVICE_LOG("glReadPixels"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glReadPixelsFn(x, y, width, height, format, type, pixels);
}

DISABLE_CFI_ICALL
void DebugGLApi::glReadPixelsRobustANGLEFn(GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLsizei* columns,
                                           GLsizei* rows,
                                           void* pixels) {
  GL_SERVICE_LOG("glReadPixelsRobustANGLE"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(columns) << ", "
                 << static_cast<const void*>(rows) << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glReadPixelsRobustANGLEFn(x, y, width, height, format, type, bufSize,
                                     length, columns, rows, pixels);
}

DISABLE_CFI_ICALL
void DebugGLApi::glReleaseShaderCompilerFn(void) {
  GL_SERVICE_LOG("glReleaseShaderCompiler"
                 << "("
                 << ")");
  gl_api_->glReleaseShaderCompilerFn();
}

DISABLE_CFI_ICALL
void DebugGLApi::glRenderbufferStorageEXTFn(GLenum target,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height) {
  GL_SERVICE_LOG("glRenderbufferStorageEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(internalformat) << ", " << width
                 << ", " << height << ")");
  gl_api_->glRenderbufferStorageEXTFn(target, internalformat, width, height);
}

DISABLE_CFI_ICALL
void DebugGLApi::glRenderbufferStorageMultisampleFn(GLenum target,
                                                    GLsizei samples,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height) {
  GL_SERVICE_LOG("glRenderbufferStorageMultisample"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << samples
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ")");
  gl_api_->glRenderbufferStorageMultisampleFn(target, samples, internalformat,
                                              width, height);
}

DISABLE_CFI_ICALL
void DebugGLApi::glRenderbufferStorageMultisampleEXTFn(GLenum target,
                                                       GLsizei samples,
                                                       GLenum internalformat,
                                                       GLsizei width,
                                                       GLsizei height) {
  GL_SERVICE_LOG("glRenderbufferStorageMultisampleEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << samples
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ")");
  gl_api_->glRenderbufferStorageMultisampleEXTFn(target, samples,
                                                 internalformat, width, height);
}

DISABLE_CFI_ICALL
void DebugGLApi::glRequestExtensionANGLEFn(const char* name) {
  GL_SERVICE_LOG("glRequestExtensionANGLE"
                 << "(" << name << ")");
  gl_api_->glRequestExtensionANGLEFn(name);
}

DISABLE_CFI_ICALL
void DebugGLApi::glResumeTransformFeedbackFn(void) {
  GL_SERVICE_LOG("glResumeTransformFeedback"
                 << "("
                 << ")");
  gl_api_->glResumeTransformFeedbackFn();
}

DISABLE_CFI_ICALL
void DebugGLApi::glSampleCoverageFn(GLclampf value, GLboolean invert) {
  GL_SERVICE_LOG("glSampleCoverage"
                 << "(" << value << ", " << GLEnums::GetStringBool(invert)
                 << ")");
  gl_api_->glSampleCoverageFn(value, invert);
}

DISABLE_CFI_ICALL
void DebugGLApi::glSamplerParameterfFn(GLuint sampler,
                                       GLenum pname,
                                       GLfloat param) {
  GL_SERVICE_LOG("glSamplerParameterf"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << param << ")");
  gl_api_->glSamplerParameterfFn(sampler, pname, param);
}

DISABLE_CFI_ICALL
void DebugGLApi::glSamplerParameterfvFn(GLuint sampler,
                                        GLenum pname,
                                        const GLfloat* params) {
  GL_SERVICE_LOG("glSamplerParameterfv"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glSamplerParameterfvFn(sampler, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLfloat* param) {
  GL_SERVICE_LOG("glSamplerParameterfvRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(param)
                 << ")");
  gl_api_->glSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize, param);
}

DISABLE_CFI_ICALL
void DebugGLApi::glSamplerParameteriFn(GLuint sampler,
                                       GLenum pname,
                                       GLint param) {
  GL_SERVICE_LOG("glSamplerParameteri"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << param << ")");
  gl_api_->glSamplerParameteriFn(sampler, pname, param);
}

DISABLE_CFI_ICALL
void DebugGLApi::glSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    const GLint* param) {
  GL_SERVICE_LOG("glSamplerParameterIivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(param)
                 << ")");
  gl_api_->glSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize, param);
}

DISABLE_CFI_ICALL
void DebugGLApi::glSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     const GLuint* param) {
  GL_SERVICE_LOG("glSamplerParameterIuivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(param)
                 << ")");
  gl_api_->glSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize, param);
}

DISABLE_CFI_ICALL
void DebugGLApi::glSamplerParameterivFn(GLuint sampler,
                                        GLenum pname,
                                        const GLint* params) {
  GL_SERVICE_LOG("glSamplerParameteriv"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  gl_api_->glSamplerParameterivFn(sampler, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLint* param) {
  GL_SERVICE_LOG("glSamplerParameterivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(param)
                 << ")");
  gl_api_->glSamplerParameterivRobustANGLEFn(sampler, pname, bufSize, param);
}

DISABLE_CFI_ICALL
void DebugGLApi::glScissorFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  GL_SERVICE_LOG("glScissor"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ")");
  gl_api_->glScissorFn(x, y, width, height);
}

DISABLE_CFI_ICALL
void DebugGLApi::glSetFenceAPPLEFn(GLuint fence) {
  GL_SERVICE_LOG("glSetFenceAPPLE"
                 << "(" << fence << ")");
  gl_api_->glSetFenceAPPLEFn(fence);
}

DISABLE_CFI_ICALL
void DebugGLApi::glSetFenceNVFn(GLuint fence, GLenum condition) {
  GL_SERVICE_LOG("glSetFenceNV"
                 << "(" << fence << ", " << GLEnums::GetStringEnum(condition)
                 << ")");
  gl_api_->glSetFenceNVFn(fence, condition);
}

DISABLE_CFI_ICALL
void DebugGLApi::glShaderBinaryFn(GLsizei n,
                                  const GLuint* shaders,
                                  GLenum binaryformat,
                                  const void* binary,
                                  GLsizei length) {
  GL_SERVICE_LOG("glShaderBinary"
                 << "(" << n << ", " << static_cast<const void*>(shaders)
                 << ", " << GLEnums::GetStringEnum(binaryformat) << ", "
                 << static_cast<const void*>(binary) << ", " << length << ")");
  gl_api_->glShaderBinaryFn(n, shaders, binaryformat, binary, length);
}

DISABLE_CFI_ICALL
void DebugGLApi::glShaderSourceFn(GLuint shader,
                                  GLsizei count,
                                  const char* const* str,
                                  const GLint* length) {
  GL_SERVICE_LOG("glShaderSource"
                 << "(" << shader << ", " << count << ", "
                 << static_cast<const void*>(str) << ", "
                 << static_cast<const void*>(length) << ")");
  gl_api_->glShaderSourceFn(shader, count, str, length);

  GL_SERVICE_LOG_CODE_BLOCK({
    for (GLsizei ii = 0; ii < count; ++ii) {
      if (str[ii]) {
        if (length && length[ii] >= 0) {
          std::string source(str[ii], length[ii]);
          GL_SERVICE_LOG("  " << ii << ": ---\n" << source << "\n---");
        } else {
          GL_SERVICE_LOG("  " << ii << ": ---\n" << str[ii] << "\n---");
        }
      } else {
        GL_SERVICE_LOG("  " << ii << ": NULL");
      }
    }
  });
}

DISABLE_CFI_ICALL
void DebugGLApi::glStencilFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  GL_SERVICE_LOG("glStencilFillPathInstancedNV"
                 << "(" << numPaths << ", "
                 << GLEnums::GetStringEnum(pathNameType) << ", "
                 << static_cast<const void*>(paths) << ", " << pathBase << ", "
                 << GLEnums::GetStringEnum(fillMode) << ", " << mask << ", "
                 << GLEnums::GetStringEnum(transformType) << ", "
                 << static_cast<const void*>(transformValues) << ")");
  gl_api_->glStencilFillPathInstancedNVFn(numPaths, pathNameType, paths,
                                          pathBase, fillMode, mask,
                                          transformType, transformValues);
}

DISABLE_CFI_ICALL
void DebugGLApi::glStencilFillPathNVFn(GLuint path,
                                       GLenum fillMode,
                                       GLuint mask) {
  GL_SERVICE_LOG("glStencilFillPathNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(fillMode)
                 << ", " << mask << ")");
  gl_api_->glStencilFillPathNVFn(path, fillMode, mask);
}

DISABLE_CFI_ICALL
void DebugGLApi::glStencilFuncFn(GLenum func, GLint ref, GLuint mask) {
  GL_SERVICE_LOG("glStencilFunc"
                 << "(" << GLEnums::GetStringEnum(func) << ", " << ref << ", "
                 << mask << ")");
  gl_api_->glStencilFuncFn(func, ref, mask);
}

DISABLE_CFI_ICALL
void DebugGLApi::glStencilFuncSeparateFn(GLenum face,
                                         GLenum func,
                                         GLint ref,
                                         GLuint mask) {
  GL_SERVICE_LOG("glStencilFuncSeparate"
                 << "(" << GLEnums::GetStringEnum(face) << ", "
                 << GLEnums::GetStringEnum(func) << ", " << ref << ", " << mask
                 << ")");
  gl_api_->glStencilFuncSeparateFn(face, func, ref, mask);
}

DISABLE_CFI_ICALL
void DebugGLApi::glStencilMaskFn(GLuint mask) {
  GL_SERVICE_LOG("glStencilMask"
                 << "(" << mask << ")");
  gl_api_->glStencilMaskFn(mask);
}

DISABLE_CFI_ICALL
void DebugGLApi::glStencilMaskSeparateFn(GLenum face, GLuint mask) {
  GL_SERVICE_LOG("glStencilMaskSeparate"
                 << "(" << GLEnums::GetStringEnum(face) << ", " << mask << ")");
  gl_api_->glStencilMaskSeparateFn(face, mask);
}

DISABLE_CFI_ICALL
void DebugGLApi::glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) {
  GL_SERVICE_LOG("glStencilOp"
                 << "(" << GLEnums::GetStringEnum(fail) << ", "
                 << GLEnums::GetStringEnum(zfail) << ", "
                 << GLEnums::GetStringEnum(zpass) << ")");
  gl_api_->glStencilOpFn(fail, zfail, zpass);
}

DISABLE_CFI_ICALL
void DebugGLApi::glStencilOpSeparateFn(GLenum face,
                                       GLenum fail,
                                       GLenum zfail,
                                       GLenum zpass) {
  GL_SERVICE_LOG("glStencilOpSeparate"
                 << "(" << GLEnums::GetStringEnum(face) << ", "
                 << GLEnums::GetStringEnum(fail) << ", "
                 << GLEnums::GetStringEnum(zfail) << ", "
                 << GLEnums::GetStringEnum(zpass) << ")");
  gl_api_->glStencilOpSeparateFn(face, fail, zfail, zpass);
}

DISABLE_CFI_ICALL
void DebugGLApi::glStencilStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  GL_SERVICE_LOG(
      "glStencilStrokePathInstancedNV"
      << "(" << numPaths << ", " << GLEnums::GetStringEnum(pathNameType) << ", "
      << static_cast<const void*>(paths) << ", " << pathBase << ", " << ref
      << ", " << mask << ", " << GLEnums::GetStringEnum(transformType) << ", "
      << static_cast<const void*>(transformValues) << ")");
  gl_api_->glStencilStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                            pathBase, ref, mask, transformType,
                                            transformValues);
}

DISABLE_CFI_ICALL
void DebugGLApi::glStencilStrokePathNVFn(GLuint path,
                                         GLint reference,
                                         GLuint mask) {
  GL_SERVICE_LOG("glStencilStrokePathNV"
                 << "(" << path << ", " << reference << ", " << mask << ")");
  gl_api_->glStencilStrokePathNVFn(path, reference, mask);
}

DISABLE_CFI_ICALL
void DebugGLApi::glStencilThenCoverFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  GL_SERVICE_LOG("glStencilThenCoverFillPathInstancedNV"
                 << "(" << numPaths << ", "
                 << GLEnums::GetStringEnum(pathNameType) << ", "
                 << static_cast<const void*>(paths) << ", " << pathBase << ", "
                 << GLEnums::GetStringEnum(fillMode) << ", " << mask << ", "
                 << GLEnums::GetStringEnum(coverMode) << ", "
                 << GLEnums::GetStringEnum(transformType) << ", "
                 << static_cast<const void*>(transformValues) << ")");
  gl_api_->glStencilThenCoverFillPathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, fillMode, mask, coverMode,
      transformType, transformValues);
}

DISABLE_CFI_ICALL
void DebugGLApi::glStencilThenCoverFillPathNVFn(GLuint path,
                                                GLenum fillMode,
                                                GLuint mask,
                                                GLenum coverMode) {
  GL_SERVICE_LOG("glStencilThenCoverFillPathNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(fillMode)
                 << ", " << mask << ", " << GLEnums::GetStringEnum(coverMode)
                 << ")");
  gl_api_->glStencilThenCoverFillPathNVFn(path, fillMode, mask, coverMode);
}

DISABLE_CFI_ICALL
void DebugGLApi::glStencilThenCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  GL_SERVICE_LOG(
      "glStencilThenCoverStrokePathInstancedNV"
      << "(" << numPaths << ", " << GLEnums::GetStringEnum(pathNameType) << ", "
      << static_cast<const void*>(paths) << ", " << pathBase << ", " << ref
      << ", " << mask << ", " << GLEnums::GetStringEnum(coverMode) << ", "
      << GLEnums::GetStringEnum(transformType) << ", "
      << static_cast<const void*>(transformValues) << ")");
  gl_api_->glStencilThenCoverStrokePathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, ref, mask, coverMode,
      transformType, transformValues);
}

DISABLE_CFI_ICALL
void DebugGLApi::glStencilThenCoverStrokePathNVFn(GLuint path,
                                                  GLint reference,
                                                  GLuint mask,
                                                  GLenum coverMode) {
  GL_SERVICE_LOG("glStencilThenCoverStrokePathNV"
                 << "(" << path << ", " << reference << ", " << mask << ", "
                 << GLEnums::GetStringEnum(coverMode) << ")");
  gl_api_->glStencilThenCoverStrokePathNVFn(path, reference, mask, coverMode);
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glTestFenceAPPLEFn(GLuint fence) {
  GL_SERVICE_LOG("glTestFenceAPPLE"
                 << "(" << fence << ")");
  GLboolean result = gl_api_->glTestFenceAPPLEFn(fence);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glTestFenceNVFn(GLuint fence) {
  GL_SERVICE_LOG("glTestFenceNV"
                 << "(" << fence << ")");
  GLboolean result = gl_api_->glTestFenceNVFn(fence);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexBufferFn(GLenum target,
                               GLenum internalformat,
                               GLuint buffer) {
  GL_SERVICE_LOG("glTexBuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(internalformat) << ", " << buffer
                 << ")");
  gl_api_->glTexBufferFn(target, internalformat, buffer);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexBufferRangeFn(GLenum target,
                                    GLenum internalformat,
                                    GLuint buffer,
                                    GLintptr offset,
                                    GLsizeiptr size) {
  GL_SERVICE_LOG("glTexBufferRange"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(internalformat) << ", " << buffer
                 << ", " << offset << ", " << size << ")");
  gl_api_->glTexBufferRangeFn(target, internalformat, buffer, offset, size);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexImage2DFn(GLenum target,
                                GLint level,
                                GLint internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLint border,
                                GLenum format,
                                GLenum type,
                                const void* pixels) {
  GL_SERVICE_LOG("glTexImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << internalformat << ", " << width << ", " << height
                 << ", " << border << ", " << GLEnums::GetStringEnum(format)
                 << ", " << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexImage2DFn(target, level, internalformat, width, height, border,
                          format, type, pixels);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexImage2DRobustANGLEFn(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLint border,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           const void* pixels) {
  GL_SERVICE_LOG("glTexImage2DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << internalformat << ", " << width << ", " << height
                 << ", " << border << ", " << GLEnums::GetStringEnum(format)
                 << ", " << GLEnums::GetStringEnum(type) << ", " << bufSize
                 << ", " << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexImage2DRobustANGLEFn(target, level, internalformat, width,
                                     height, border, format, type, bufSize,
                                     pixels);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexImage3DFn(GLenum target,
                                GLint level,
                                GLint internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth,
                                GLint border,
                                GLenum format,
                                GLenum type,
                                const void* pixels) {
  GL_SERVICE_LOG("glTexImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << internalformat << ", " << width << ", " << height
                 << ", " << depth << ", " << border << ", "
                 << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexImage3DFn(target, level, internalformat, width, height, depth,
                          border, format, type, pixels);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexImage3DRobustANGLEFn(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLsizei depth,
                                           GLint border,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           const void* pixels) {
  GL_SERVICE_LOG("glTexImage3DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << internalformat << ", " << width << ", " << height
                 << ", " << depth << ", " << border << ", "
                 << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexImage3DRobustANGLEFn(target, level, internalformat, width,
                                     height, depth, border, format, type,
                                     bufSize, pixels);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexParameterfFn(GLenum target, GLenum pname, GLfloat param) {
  GL_SERVICE_LOG("glTexParameterf"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << param << ")");
  gl_api_->glTexParameterfFn(target, pname, param);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexParameterfvFn(GLenum target,
                                    GLenum pname,
                                    const GLfloat* params) {
  GL_SERVICE_LOG("glTexParameterfv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glTexParameterfvFn(target, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexParameterfvRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLfloat* params) {
  GL_SERVICE_LOG("glTexParameterfvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glTexParameterfvRobustANGLEFn(target, pname, bufSize, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexParameteriFn(GLenum target, GLenum pname, GLint param) {
  GL_SERVICE_LOG("glTexParameteri"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << param << ")");
  gl_api_->glTexParameteriFn(target, pname, param);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexParameterIivRobustANGLEFn(GLenum target,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                const GLint* params) {
  GL_SERVICE_LOG("glTexParameterIivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glTexParameterIivRobustANGLEFn(target, pname, bufSize, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexParameterIuivRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 const GLuint* params) {
  GL_SERVICE_LOG("glTexParameterIuivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glTexParameterIuivRobustANGLEFn(target, pname, bufSize, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexParameterivFn(GLenum target,
                                    GLenum pname,
                                    const GLint* params) {
  GL_SERVICE_LOG("glTexParameteriv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glTexParameterivFn(target, pname, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexParameterivRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLint* params) {
  GL_SERVICE_LOG("glTexParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  gl_api_->glTexParameterivRobustANGLEFn(target, pname, bufSize, params);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexStorage2DEXTFn(GLenum target,
                                     GLsizei levels,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height) {
  GL_SERVICE_LOG("glTexStorage2DEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << levels
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ")");
  gl_api_->glTexStorage2DEXTFn(target, levels, internalformat, width, height);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexStorage3DFn(GLenum target,
                                  GLsizei levels,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth) {
  GL_SERVICE_LOG("glTexStorage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << levels
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ", " << depth << ")");
  gl_api_->glTexStorage3DFn(target, levels, internalformat, width, height,
                            depth);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexSubImage2DFn(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum format,
                                   GLenum type,
                                   const void* pixels) {
  GL_SERVICE_LOG("glTexSubImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << width << ", "
                 << height << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexSubImage2DFn(target, level, xoffset, yoffset, width, height,
                             format, type, pixels);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexSubImage2DRobustANGLEFn(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLsizei width,
                                              GLsizei height,
                                              GLenum format,
                                              GLenum type,
                                              GLsizei bufSize,
                                              const void* pixels) {
  GL_SERVICE_LOG("glTexSubImage2DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << width << ", "
                 << height << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexSubImage2DRobustANGLEFn(target, level, xoffset, yoffset, width,
                                        height, format, type, bufSize, pixels);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexSubImage3DFn(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLint zoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLenum format,
                                   GLenum type,
                                   const void* pixels) {
  GL_SERVICE_LOG("glTexSubImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << zoffset
                 << ", " << width << ", " << height << ", " << depth << ", "
                 << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, width,
                             height, depth, format, type, pixels);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTexSubImage3DRobustANGLEFn(GLenum target,
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
                                              const void* pixels) {
  GL_SERVICE_LOG("glTexSubImage3DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << zoffset
                 << ", " << width << ", " << height << ", " << depth << ", "
                 << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(pixels) << ")");
  gl_api_->glTexSubImage3DRobustANGLEFn(target, level, xoffset, yoffset,
                                        zoffset, width, height, depth, format,
                                        type, bufSize, pixels);
}

DISABLE_CFI_ICALL
void DebugGLApi::glTransformFeedbackVaryingsFn(GLuint program,
                                               GLsizei count,
                                               const char* const* varyings,
                                               GLenum bufferMode) {
  GL_SERVICE_LOG("glTransformFeedbackVaryings"
                 << "(" << program << ", " << count << ", "
                 << static_cast<const void*>(varyings) << ", "
                 << GLEnums::GetStringEnum(bufferMode) << ")");
  gl_api_->glTransformFeedbackVaryingsFn(program, count, varyings, bufferMode);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform1fFn(GLint location, GLfloat x) {
  GL_SERVICE_LOG("glUniform1f"
                 << "(" << location << ", " << x << ")");
  gl_api_->glUniform1fFn(location, x);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform1fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  GL_SERVICE_LOG("glUniform1fv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform1fvFn(location, count, v);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform1iFn(GLint location, GLint x) {
  GL_SERVICE_LOG("glUniform1i"
                 << "(" << location << ", " << x << ")");
  gl_api_->glUniform1iFn(location, x);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform1ivFn(GLint location, GLsizei count, const GLint* v) {
  GL_SERVICE_LOG("glUniform1iv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform1ivFn(location, count, v);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform1uiFn(GLint location, GLuint v0) {
  GL_SERVICE_LOG("glUniform1ui"
                 << "(" << location << ", " << v0 << ")");
  gl_api_->glUniform1uiFn(location, v0);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform1uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  GL_SERVICE_LOG("glUniform1uiv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform1uivFn(location, count, v);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform2fFn(GLint location, GLfloat x, GLfloat y) {
  GL_SERVICE_LOG("glUniform2f"
                 << "(" << location << ", " << x << ", " << y << ")");
  gl_api_->glUniform2fFn(location, x, y);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform2fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  GL_SERVICE_LOG("glUniform2fv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform2fvFn(location, count, v);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform2iFn(GLint location, GLint x, GLint y) {
  GL_SERVICE_LOG("glUniform2i"
                 << "(" << location << ", " << x << ", " << y << ")");
  gl_api_->glUniform2iFn(location, x, y);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform2ivFn(GLint location, GLsizei count, const GLint* v) {
  GL_SERVICE_LOG("glUniform2iv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform2ivFn(location, count, v);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform2uiFn(GLint location, GLuint v0, GLuint v1) {
  GL_SERVICE_LOG("glUniform2ui"
                 << "(" << location << ", " << v0 << ", " << v1 << ")");
  gl_api_->glUniform2uiFn(location, v0, v1);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform2uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  GL_SERVICE_LOG("glUniform2uiv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform2uivFn(location, count, v);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform3fFn(GLint location,
                               GLfloat x,
                               GLfloat y,
                               GLfloat z) {
  GL_SERVICE_LOG("glUniform3f"
                 << "(" << location << ", " << x << ", " << y << ", " << z
                 << ")");
  gl_api_->glUniform3fFn(location, x, y, z);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform3fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  GL_SERVICE_LOG("glUniform3fv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform3fvFn(location, count, v);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform3iFn(GLint location, GLint x, GLint y, GLint z) {
  GL_SERVICE_LOG("glUniform3i"
                 << "(" << location << ", " << x << ", " << y << ", " << z
                 << ")");
  gl_api_->glUniform3iFn(location, x, y, z);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform3ivFn(GLint location, GLsizei count, const GLint* v) {
  GL_SERVICE_LOG("glUniform3iv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform3ivFn(location, count, v);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform3uiFn(GLint location,
                                GLuint v0,
                                GLuint v1,
                                GLuint v2) {
  GL_SERVICE_LOG("glUniform3ui"
                 << "(" << location << ", " << v0 << ", " << v1 << ", " << v2
                 << ")");
  gl_api_->glUniform3uiFn(location, v0, v1, v2);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform3uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  GL_SERVICE_LOG("glUniform3uiv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform3uivFn(location, count, v);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform4fFn(GLint location,
                               GLfloat x,
                               GLfloat y,
                               GLfloat z,
                               GLfloat w) {
  GL_SERVICE_LOG("glUniform4f"
                 << "(" << location << ", " << x << ", " << y << ", " << z
                 << ", " << w << ")");
  gl_api_->glUniform4fFn(location, x, y, z, w);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform4fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  GL_SERVICE_LOG("glUniform4fv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform4fvFn(location, count, v);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform4iFn(GLint location,
                               GLint x,
                               GLint y,
                               GLint z,
                               GLint w) {
  GL_SERVICE_LOG("glUniform4i"
                 << "(" << location << ", " << x << ", " << y << ", " << z
                 << ", " << w << ")");
  gl_api_->glUniform4iFn(location, x, y, z, w);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform4ivFn(GLint location, GLsizei count, const GLint* v) {
  GL_SERVICE_LOG("glUniform4iv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform4ivFn(location, count, v);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform4uiFn(GLint location,
                                GLuint v0,
                                GLuint v1,
                                GLuint v2,
                                GLuint v3) {
  GL_SERVICE_LOG("glUniform4ui"
                 << "(" << location << ", " << v0 << ", " << v1 << ", " << v2
                 << ", " << v3 << ")");
  gl_api_->glUniform4uiFn(location, v0, v1, v2, v3);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniform4uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  GL_SERVICE_LOG("glUniform4uiv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  gl_api_->glUniform4uivFn(location, count, v);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniformBlockBindingFn(GLuint program,
                                         GLuint uniformBlockIndex,
                                         GLuint uniformBlockBinding) {
  GL_SERVICE_LOG("glUniformBlockBinding"
                 << "(" << program << ", " << uniformBlockIndex << ", "
                 << uniformBlockBinding << ")");
  gl_api_->glUniformBlockBindingFn(program, uniformBlockIndex,
                                   uniformBlockBinding);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniformMatrix2fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix2fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix2fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniformMatrix2x3fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix2x3fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix2x3fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniformMatrix2x4fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix2x4fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix2x4fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniformMatrix3fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix3fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix3fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniformMatrix3x2fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix3x2fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix3x2fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniformMatrix3x4fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix3x4fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix3x4fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniformMatrix4fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix4fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix4fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniformMatrix4x2fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix4x2fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix4x2fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
void DebugGLApi::glUniformMatrix4x3fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix4x3fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  gl_api_->glUniformMatrix4x3fvFn(location, count, transpose, value);
}

DISABLE_CFI_ICALL
GLboolean DebugGLApi::glUnmapBufferFn(GLenum target) {
  GL_SERVICE_LOG("glUnmapBuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ")");
  GLboolean result = gl_api_->glUnmapBufferFn(target);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

DISABLE_CFI_ICALL
void DebugGLApi::glUseProgramFn(GLuint program) {
  GL_SERVICE_LOG("glUseProgram"
                 << "(" << program << ")");
  gl_api_->glUseProgramFn(program);
}

DISABLE_CFI_ICALL
void DebugGLApi::glValidateProgramFn(GLuint program) {
  GL_SERVICE_LOG("glValidateProgram"
                 << "(" << program << ")");
  gl_api_->glValidateProgramFn(program);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttrib1fFn(GLuint indx, GLfloat x) {
  GL_SERVICE_LOG("glVertexAttrib1f"
                 << "(" << indx << ", " << x << ")");
  gl_api_->glVertexAttrib1fFn(indx, x);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) {
  GL_SERVICE_LOG("glVertexAttrib1fv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  gl_api_->glVertexAttrib1fvFn(indx, values);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) {
  GL_SERVICE_LOG("glVertexAttrib2f"
                 << "(" << indx << ", " << x << ", " << y << ")");
  gl_api_->glVertexAttrib2fFn(indx, x, y);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) {
  GL_SERVICE_LOG("glVertexAttrib2fv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  gl_api_->glVertexAttrib2fvFn(indx, values);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttrib3fFn(GLuint indx,
                                    GLfloat x,
                                    GLfloat y,
                                    GLfloat z) {
  GL_SERVICE_LOG("glVertexAttrib3f"
                 << "(" << indx << ", " << x << ", " << y << ", " << z << ")");
  gl_api_->glVertexAttrib3fFn(indx, x, y, z);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) {
  GL_SERVICE_LOG("glVertexAttrib3fv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  gl_api_->glVertexAttrib3fvFn(indx, values);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttrib4fFn(GLuint indx,
                                    GLfloat x,
                                    GLfloat y,
                                    GLfloat z,
                                    GLfloat w) {
  GL_SERVICE_LOG("glVertexAttrib4f"
                 << "(" << indx << ", " << x << ", " << y << ", " << z << ", "
                 << w << ")");
  gl_api_->glVertexAttrib4fFn(indx, x, y, z, w);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) {
  GL_SERVICE_LOG("glVertexAttrib4fv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  gl_api_->glVertexAttrib4fvFn(indx, values);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttribDivisorANGLEFn(GLuint index, GLuint divisor) {
  GL_SERVICE_LOG("glVertexAttribDivisorANGLE"
                 << "(" << index << ", " << divisor << ")");
  gl_api_->glVertexAttribDivisorANGLEFn(index, divisor);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttribI4iFn(GLuint indx,
                                     GLint x,
                                     GLint y,
                                     GLint z,
                                     GLint w) {
  GL_SERVICE_LOG("glVertexAttribI4i"
                 << "(" << indx << ", " << x << ", " << y << ", " << z << ", "
                 << w << ")");
  gl_api_->glVertexAttribI4iFn(indx, x, y, z, w);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttribI4ivFn(GLuint indx, const GLint* values) {
  GL_SERVICE_LOG("glVertexAttribI4iv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  gl_api_->glVertexAttribI4ivFn(indx, values);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttribI4uiFn(GLuint indx,
                                      GLuint x,
                                      GLuint y,
                                      GLuint z,
                                      GLuint w) {
  GL_SERVICE_LOG("glVertexAttribI4ui"
                 << "(" << indx << ", " << x << ", " << y << ", " << z << ", "
                 << w << ")");
  gl_api_->glVertexAttribI4uiFn(indx, x, y, z, w);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttribI4uivFn(GLuint indx, const GLuint* values) {
  GL_SERVICE_LOG("glVertexAttribI4uiv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  gl_api_->glVertexAttribI4uivFn(indx, values);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttribIPointerFn(GLuint indx,
                                          GLint size,
                                          GLenum type,
                                          GLsizei stride,
                                          const void* ptr) {
  GL_SERVICE_LOG("glVertexAttribIPointer"
                 << "(" << indx << ", " << size << ", "
                 << GLEnums::GetStringEnum(type) << ", " << stride << ", "
                 << static_cast<const void*>(ptr) << ")");
  gl_api_->glVertexAttribIPointerFn(indx, size, type, stride, ptr);
}

DISABLE_CFI_ICALL
void DebugGLApi::glVertexAttribPointerFn(GLuint indx,
                                         GLint size,
                                         GLenum type,
                                         GLboolean normalized,
                                         GLsizei stride,
                                         const void* ptr) {
  GL_SERVICE_LOG("glVertexAttribPointer"
                 << "(" << indx << ", " << size << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << GLEnums::GetStringBool(normalized) << ", " << stride << ", "
                 << static_cast<const void*>(ptr) << ")");
  gl_api_->glVertexAttribPointerFn(indx, size, type, normalized, stride, ptr);
}

DISABLE_CFI_ICALL
void DebugGLApi::glViewportFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  GL_SERVICE_LOG("glViewport"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ")");
  gl_api_->glViewportFn(x, y, width, height);
}

DISABLE_CFI_ICALL
void DebugGLApi::glWaitSyncFn(GLsync sync, GLbitfield flags, GLuint64 timeout) {
  GL_SERVICE_LOG("glWaitSync"
                 << "(" << sync << ", " << flags << ", " << timeout << ")");
  gl_api_->glWaitSyncFn(sync, flags, timeout);
}

DISABLE_CFI_ICALL
void DebugGLApi::glWindowRectanglesEXTFn(GLenum mode,
                                         GLsizei n,
                                         const GLint* box) {
  GL_SERVICE_LOG("glWindowRectanglesEXT"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << n << ", "
                 << static_cast<const void*>(box) << ")");
  gl_api_->glWindowRectanglesEXTFn(mode, n, box);
}

namespace {
void NoContextHelper(const char* method_name) {
  NOTREACHED() << "Trying to call " << method_name
               << " without current GL context";
  LOG(ERROR) << "Trying to call " << method_name
             << " without current GL context";
}
}  // namespace

void NoContextGLApi::glActiveTextureFn(GLenum texture) {
  NoContextHelper("glActiveTexture");
}

void NoContextGLApi::glApplyFramebufferAttachmentCMAAINTELFn(void) {
  NoContextHelper("glApplyFramebufferAttachmentCMAAINTEL");
}

void NoContextGLApi::glAttachShaderFn(GLuint program, GLuint shader) {
  NoContextHelper("glAttachShader");
}

void NoContextGLApi::glBeginQueryFn(GLenum target, GLuint id) {
  NoContextHelper("glBeginQuery");
}

void NoContextGLApi::glBeginTransformFeedbackFn(GLenum primitiveMode) {
  NoContextHelper("glBeginTransformFeedback");
}

void NoContextGLApi::glBindAttribLocationFn(GLuint program,
                                            GLuint index,
                                            const char* name) {
  NoContextHelper("glBindAttribLocation");
}

void NoContextGLApi::glBindBufferFn(GLenum target, GLuint buffer) {
  NoContextHelper("glBindBuffer");
}

void NoContextGLApi::glBindBufferBaseFn(GLenum target,
                                        GLuint index,
                                        GLuint buffer) {
  NoContextHelper("glBindBufferBase");
}

void NoContextGLApi::glBindBufferRangeFn(GLenum target,
                                         GLuint index,
                                         GLuint buffer,
                                         GLintptr offset,
                                         GLsizeiptr size) {
  NoContextHelper("glBindBufferRange");
}

void NoContextGLApi::glBindFragDataLocationFn(GLuint program,
                                              GLuint colorNumber,
                                              const char* name) {
  NoContextHelper("glBindFragDataLocation");
}

void NoContextGLApi::glBindFragDataLocationIndexedFn(GLuint program,
                                                     GLuint colorNumber,
                                                     GLuint index,
                                                     const char* name) {
  NoContextHelper("glBindFragDataLocationIndexed");
}

void NoContextGLApi::glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) {
  NoContextHelper("glBindFramebufferEXT");
}

void NoContextGLApi::glBindImageTextureEXTFn(GLuint index,
                                             GLuint texture,
                                             GLint level,
                                             GLboolean layered,
                                             GLint layer,
                                             GLenum access,
                                             GLint format) {
  NoContextHelper("glBindImageTextureEXT");
}

void NoContextGLApi::glBindRenderbufferEXTFn(GLenum target,
                                             GLuint renderbuffer) {
  NoContextHelper("glBindRenderbufferEXT");
}

void NoContextGLApi::glBindSamplerFn(GLuint unit, GLuint sampler) {
  NoContextHelper("glBindSampler");
}

void NoContextGLApi::glBindTextureFn(GLenum target, GLuint texture) {
  NoContextHelper("glBindTexture");
}

void NoContextGLApi::glBindTransformFeedbackFn(GLenum target, GLuint id) {
  NoContextHelper("glBindTransformFeedback");
}

void NoContextGLApi::glBindUniformLocationCHROMIUMFn(GLuint program,
                                                     GLint location,
                                                     const char* name) {
  NoContextHelper("glBindUniformLocationCHROMIUM");
}

void NoContextGLApi::glBindVertexArrayOESFn(GLuint array) {
  NoContextHelper("glBindVertexArrayOES");
}

void NoContextGLApi::glBlendBarrierKHRFn(void) {
  NoContextHelper("glBlendBarrierKHR");
}

void NoContextGLApi::glBlendColorFn(GLclampf red,
                                    GLclampf green,
                                    GLclampf blue,
                                    GLclampf alpha) {
  NoContextHelper("glBlendColor");
}

void NoContextGLApi::glBlendEquationFn(GLenum mode) {
  NoContextHelper("glBlendEquation");
}

void NoContextGLApi::glBlendEquationSeparateFn(GLenum modeRGB,
                                               GLenum modeAlpha) {
  NoContextHelper("glBlendEquationSeparate");
}

void NoContextGLApi::glBlendFuncFn(GLenum sfactor, GLenum dfactor) {
  NoContextHelper("glBlendFunc");
}

void NoContextGLApi::glBlendFuncSeparateFn(GLenum srcRGB,
                                           GLenum dstRGB,
                                           GLenum srcAlpha,
                                           GLenum dstAlpha) {
  NoContextHelper("glBlendFuncSeparate");
}

void NoContextGLApi::glBlitFramebufferFn(GLint srcX0,
                                         GLint srcY0,
                                         GLint srcX1,
                                         GLint srcY1,
                                         GLint dstX0,
                                         GLint dstY0,
                                         GLint dstX1,
                                         GLint dstY1,
                                         GLbitfield mask,
                                         GLenum filter) {
  NoContextHelper("glBlitFramebuffer");
}

void NoContextGLApi::glBufferDataFn(GLenum target,
                                    GLsizeiptr size,
                                    const void* data,
                                    GLenum usage) {
  NoContextHelper("glBufferData");
}

void NoContextGLApi::glBufferSubDataFn(GLenum target,
                                       GLintptr offset,
                                       GLsizeiptr size,
                                       const void* data) {
  NoContextHelper("glBufferSubData");
}

GLenum NoContextGLApi::glCheckFramebufferStatusEXTFn(GLenum target) {
  NoContextHelper("glCheckFramebufferStatusEXT");
  return static_cast<GLenum>(0);
}

void NoContextGLApi::glClearFn(GLbitfield mask) {
  NoContextHelper("glClear");
}

void NoContextGLApi::glClearBufferfiFn(GLenum buffer,
                                       GLint drawbuffer,
                                       const GLfloat depth,
                                       GLint stencil) {
  NoContextHelper("glClearBufferfi");
}

void NoContextGLApi::glClearBufferfvFn(GLenum buffer,
                                       GLint drawbuffer,
                                       const GLfloat* value) {
  NoContextHelper("glClearBufferfv");
}

void NoContextGLApi::glClearBufferivFn(GLenum buffer,
                                       GLint drawbuffer,
                                       const GLint* value) {
  NoContextHelper("glClearBufferiv");
}

void NoContextGLApi::glClearBufferuivFn(GLenum buffer,
                                        GLint drawbuffer,
                                        const GLuint* value) {
  NoContextHelper("glClearBufferuiv");
}

void NoContextGLApi::glClearColorFn(GLclampf red,
                                    GLclampf green,
                                    GLclampf blue,
                                    GLclampf alpha) {
  NoContextHelper("glClearColor");
}

void NoContextGLApi::glClearDepthFn(GLclampd depth) {
  NoContextHelper("glClearDepth");
}

void NoContextGLApi::glClearDepthfFn(GLclampf depth) {
  NoContextHelper("glClearDepthf");
}

void NoContextGLApi::glClearStencilFn(GLint s) {
  NoContextHelper("glClearStencil");
}

GLenum NoContextGLApi::glClientWaitSyncFn(GLsync sync,
                                          GLbitfield flags,
                                          GLuint64 timeout) {
  NoContextHelper("glClientWaitSync");
  return static_cast<GLenum>(0);
}

void NoContextGLApi::glColorMaskFn(GLboolean red,
                                   GLboolean green,
                                   GLboolean blue,
                                   GLboolean alpha) {
  NoContextHelper("glColorMask");
}

void NoContextGLApi::glCompileShaderFn(GLuint shader) {
  NoContextHelper("glCompileShader");
}

void NoContextGLApi::glCompressedCopyTextureCHROMIUMFn(GLuint sourceId,
                                                       GLuint destId) {
  NoContextHelper("glCompressedCopyTextureCHROMIUM");
}

void NoContextGLApi::glCompressedTexImage2DFn(GLenum target,
                                              GLint level,
                                              GLenum internalformat,
                                              GLsizei width,
                                              GLsizei height,
                                              GLint border,
                                              GLsizei imageSize,
                                              const void* data) {
  NoContextHelper("glCompressedTexImage2D");
}

void NoContextGLApi::glCompressedTexImage2DRobustANGLEFn(GLenum target,
                                                         GLint level,
                                                         GLenum internalformat,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLint border,
                                                         GLsizei imageSize,
                                                         GLsizei dataSize,
                                                         const void* data) {
  NoContextHelper("glCompressedTexImage2DRobustANGLE");
}

void NoContextGLApi::glCompressedTexImage3DFn(GLenum target,
                                              GLint level,
                                              GLenum internalformat,
                                              GLsizei width,
                                              GLsizei height,
                                              GLsizei depth,
                                              GLint border,
                                              GLsizei imageSize,
                                              const void* data) {
  NoContextHelper("glCompressedTexImage3D");
}

void NoContextGLApi::glCompressedTexImage3DRobustANGLEFn(GLenum target,
                                                         GLint level,
                                                         GLenum internalformat,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLsizei depth,
                                                         GLint border,
                                                         GLsizei imageSize,
                                                         GLsizei dataSize,
                                                         const void* data) {
  NoContextHelper("glCompressedTexImage3DRobustANGLE");
}

void NoContextGLApi::glCompressedTexSubImage2DFn(GLenum target,
                                                 GLint level,
                                                 GLint xoffset,
                                                 GLint yoffset,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLenum format,
                                                 GLsizei imageSize,
                                                 const void* data) {
  NoContextHelper("glCompressedTexSubImage2D");
}

void NoContextGLApi::glCompressedTexSubImage2DRobustANGLEFn(GLenum target,
                                                            GLint level,
                                                            GLint xoffset,
                                                            GLint yoffset,
                                                            GLsizei width,
                                                            GLsizei height,
                                                            GLenum format,
                                                            GLsizei imageSize,
                                                            GLsizei dataSize,
                                                            const void* data) {
  NoContextHelper("glCompressedTexSubImage2DRobustANGLE");
}

void NoContextGLApi::glCompressedTexSubImage3DFn(GLenum target,
                                                 GLint level,
                                                 GLint xoffset,
                                                 GLint yoffset,
                                                 GLint zoffset,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLsizei depth,
                                                 GLenum format,
                                                 GLsizei imageSize,
                                                 const void* data) {
  NoContextHelper("glCompressedTexSubImage3D");
}

void NoContextGLApi::glCompressedTexSubImage3DRobustANGLEFn(GLenum target,
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
                                                            const void* data) {
  NoContextHelper("glCompressedTexSubImage3DRobustANGLE");
}

void NoContextGLApi::glCopyBufferSubDataFn(GLenum readTarget,
                                           GLenum writeTarget,
                                           GLintptr readOffset,
                                           GLintptr writeOffset,
                                           GLsizeiptr size) {
  NoContextHelper("glCopyBufferSubData");
}

void NoContextGLApi::glCopySubTextureCHROMIUMFn(
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
    GLboolean unpackUnmultiplyAlpha) {
  NoContextHelper("glCopySubTextureCHROMIUM");
}

void NoContextGLApi::glCopyTexImage2DFn(GLenum target,
                                        GLint level,
                                        GLenum internalformat,
                                        GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height,
                                        GLint border) {
  NoContextHelper("glCopyTexImage2D");
}

void NoContextGLApi::glCopyTexSubImage2DFn(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height) {
  NoContextHelper("glCopyTexSubImage2D");
}

void NoContextGLApi::glCopyTexSubImage3DFn(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint zoffset,
                                           GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height) {
  NoContextHelper("glCopyTexSubImage3D");
}

void NoContextGLApi::glCopyTextureCHROMIUMFn(GLuint sourceId,
                                             GLint sourceLevel,
                                             GLenum destTarget,
                                             GLuint destId,
                                             GLint destLevel,
                                             GLint internalFormat,
                                             GLenum destType,
                                             GLboolean unpackFlipY,
                                             GLboolean unpackPremultiplyAlpha,
                                             GLboolean unpackUnmultiplyAlpha) {
  NoContextHelper("glCopyTextureCHROMIUM");
}

void NoContextGLApi::glCoverageModulationNVFn(GLenum components) {
  NoContextHelper("glCoverageModulationNV");
}

void NoContextGLApi::glCoverFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  NoContextHelper("glCoverFillPathInstancedNV");
}

void NoContextGLApi::glCoverFillPathNVFn(GLuint path, GLenum coverMode) {
  NoContextHelper("glCoverFillPathNV");
}

void NoContextGLApi::glCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  NoContextHelper("glCoverStrokePathInstancedNV");
}

void NoContextGLApi::glCoverStrokePathNVFn(GLuint name, GLenum coverMode) {
  NoContextHelper("glCoverStrokePathNV");
}

GLuint NoContextGLApi::glCreateProgramFn(void) {
  NoContextHelper("glCreateProgram");
  return 0U;
}

GLuint NoContextGLApi::glCreateShaderFn(GLenum type) {
  NoContextHelper("glCreateShader");
  return 0U;
}

void NoContextGLApi::glCullFaceFn(GLenum mode) {
  NoContextHelper("glCullFace");
}

void NoContextGLApi::glDebugMessageCallbackFn(GLDEBUGPROC callback,
                                              const void* userParam) {
  NoContextHelper("glDebugMessageCallback");
}

void NoContextGLApi::glDebugMessageControlFn(GLenum source,
                                             GLenum type,
                                             GLenum severity,
                                             GLsizei count,
                                             const GLuint* ids,
                                             GLboolean enabled) {
  NoContextHelper("glDebugMessageControl");
}

void NoContextGLApi::glDebugMessageInsertFn(GLenum source,
                                            GLenum type,
                                            GLuint id,
                                            GLenum severity,
                                            GLsizei length,
                                            const char* buf) {
  NoContextHelper("glDebugMessageInsert");
}

void NoContextGLApi::glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) {
  NoContextHelper("glDeleteBuffersARB");
}

void NoContextGLApi::glDeleteFencesAPPLEFn(GLsizei n, const GLuint* fences) {
  NoContextHelper("glDeleteFencesAPPLE");
}

void NoContextGLApi::glDeleteFencesNVFn(GLsizei n, const GLuint* fences) {
  NoContextHelper("glDeleteFencesNV");
}

void NoContextGLApi::glDeleteFramebuffersEXTFn(GLsizei n,
                                               const GLuint* framebuffers) {
  NoContextHelper("glDeleteFramebuffersEXT");
}

void NoContextGLApi::glDeletePathsNVFn(GLuint path, GLsizei range) {
  NoContextHelper("glDeletePathsNV");
}

void NoContextGLApi::glDeleteProgramFn(GLuint program) {
  NoContextHelper("glDeleteProgram");
}

void NoContextGLApi::glDeleteQueriesFn(GLsizei n, const GLuint* ids) {
  NoContextHelper("glDeleteQueries");
}

void NoContextGLApi::glDeleteRenderbuffersEXTFn(GLsizei n,
                                                const GLuint* renderbuffers) {
  NoContextHelper("glDeleteRenderbuffersEXT");
}

void NoContextGLApi::glDeleteSamplersFn(GLsizei n, const GLuint* samplers) {
  NoContextHelper("glDeleteSamplers");
}

void NoContextGLApi::glDeleteShaderFn(GLuint shader) {
  NoContextHelper("glDeleteShader");
}

void NoContextGLApi::glDeleteSyncFn(GLsync sync) {
  NoContextHelper("glDeleteSync");
}

void NoContextGLApi::glDeleteTexturesFn(GLsizei n, const GLuint* textures) {
  NoContextHelper("glDeleteTextures");
}

void NoContextGLApi::glDeleteTransformFeedbacksFn(GLsizei n,
                                                  const GLuint* ids) {
  NoContextHelper("glDeleteTransformFeedbacks");
}

void NoContextGLApi::glDeleteVertexArraysOESFn(GLsizei n,
                                               const GLuint* arrays) {
  NoContextHelper("glDeleteVertexArraysOES");
}

void NoContextGLApi::glDepthFuncFn(GLenum func) {
  NoContextHelper("glDepthFunc");
}

void NoContextGLApi::glDepthMaskFn(GLboolean flag) {
  NoContextHelper("glDepthMask");
}

void NoContextGLApi::glDepthRangeFn(GLclampd zNear, GLclampd zFar) {
  NoContextHelper("glDepthRange");
}

void NoContextGLApi::glDepthRangefFn(GLclampf zNear, GLclampf zFar) {
  NoContextHelper("glDepthRangef");
}

void NoContextGLApi::glDetachShaderFn(GLuint program, GLuint shader) {
  NoContextHelper("glDetachShader");
}

void NoContextGLApi::glDisableFn(GLenum cap) {
  NoContextHelper("glDisable");
}

void NoContextGLApi::glDisableVertexAttribArrayFn(GLuint index) {
  NoContextHelper("glDisableVertexAttribArray");
}

void NoContextGLApi::glDiscardFramebufferEXTFn(GLenum target,
                                               GLsizei numAttachments,
                                               const GLenum* attachments) {
  NoContextHelper("glDiscardFramebufferEXT");
}

void NoContextGLApi::glDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  NoContextHelper("glDrawArrays");
}

void NoContextGLApi::glDrawArraysInstancedANGLEFn(GLenum mode,
                                                  GLint first,
                                                  GLsizei count,
                                                  GLsizei primcount) {
  NoContextHelper("glDrawArraysInstancedANGLE");
}

void NoContextGLApi::glDrawBufferFn(GLenum mode) {
  NoContextHelper("glDrawBuffer");
}

void NoContextGLApi::glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) {
  NoContextHelper("glDrawBuffersARB");
}

void NoContextGLApi::glDrawElementsFn(GLenum mode,
                                      GLsizei count,
                                      GLenum type,
                                      const void* indices) {
  NoContextHelper("glDrawElements");
}

void NoContextGLApi::glDrawElementsInstancedANGLEFn(GLenum mode,
                                                    GLsizei count,
                                                    GLenum type,
                                                    const void* indices,
                                                    GLsizei primcount) {
  NoContextHelper("glDrawElementsInstancedANGLE");
}

void NoContextGLApi::glDrawRangeElementsFn(GLenum mode,
                                           GLuint start,
                                           GLuint end,
                                           GLsizei count,
                                           GLenum type,
                                           const void* indices) {
  NoContextHelper("glDrawRangeElements");
}

void NoContextGLApi::glEGLImageTargetRenderbufferStorageOESFn(
    GLenum target,
    GLeglImageOES image) {
  NoContextHelper("glEGLImageTargetRenderbufferStorageOES");
}

void NoContextGLApi::glEGLImageTargetTexture2DOESFn(GLenum target,
                                                    GLeglImageOES image) {
  NoContextHelper("glEGLImageTargetTexture2DOES");
}

void NoContextGLApi::glEnableFn(GLenum cap) {
  NoContextHelper("glEnable");
}

void NoContextGLApi::glEnableVertexAttribArrayFn(GLuint index) {
  NoContextHelper("glEnableVertexAttribArray");
}

void NoContextGLApi::glEndQueryFn(GLenum target) {
  NoContextHelper("glEndQuery");
}

void NoContextGLApi::glEndTransformFeedbackFn(void) {
  NoContextHelper("glEndTransformFeedback");
}

GLsync NoContextGLApi::glFenceSyncFn(GLenum condition, GLbitfield flags) {
  NoContextHelper("glFenceSync");
  return NULL;
}

void NoContextGLApi::glFinishFn(void) {
  NoContextHelper("glFinish");
}

void NoContextGLApi::glFinishFenceAPPLEFn(GLuint fence) {
  NoContextHelper("glFinishFenceAPPLE");
}

void NoContextGLApi::glFinishFenceNVFn(GLuint fence) {
  NoContextHelper("glFinishFenceNV");
}

void NoContextGLApi::glFlushFn(void) {
  NoContextHelper("glFlush");
}

void NoContextGLApi::glFlushMappedBufferRangeFn(GLenum target,
                                                GLintptr offset,
                                                GLsizeiptr length) {
  NoContextHelper("glFlushMappedBufferRange");
}

void NoContextGLApi::glFramebufferRenderbufferEXTFn(GLenum target,
                                                    GLenum attachment,
                                                    GLenum renderbuffertarget,
                                                    GLuint renderbuffer) {
  NoContextHelper("glFramebufferRenderbufferEXT");
}

void NoContextGLApi::glFramebufferTexture2DEXTFn(GLenum target,
                                                 GLenum attachment,
                                                 GLenum textarget,
                                                 GLuint texture,
                                                 GLint level) {
  NoContextHelper("glFramebufferTexture2DEXT");
}

void NoContextGLApi::glFramebufferTexture2DMultisampleEXTFn(GLenum target,
                                                            GLenum attachment,
                                                            GLenum textarget,
                                                            GLuint texture,
                                                            GLint level,
                                                            GLsizei samples) {
  NoContextHelper("glFramebufferTexture2DMultisampleEXT");
}

void NoContextGLApi::glFramebufferTextureLayerFn(GLenum target,
                                                 GLenum attachment,
                                                 GLuint texture,
                                                 GLint level,
                                                 GLint layer) {
  NoContextHelper("glFramebufferTextureLayer");
}

void NoContextGLApi::glFrontFaceFn(GLenum mode) {
  NoContextHelper("glFrontFace");
}

void NoContextGLApi::glGenBuffersARBFn(GLsizei n, GLuint* buffers) {
  NoContextHelper("glGenBuffersARB");
}

void NoContextGLApi::glGenerateMipmapEXTFn(GLenum target) {
  NoContextHelper("glGenerateMipmapEXT");
}

void NoContextGLApi::glGenFencesAPPLEFn(GLsizei n, GLuint* fences) {
  NoContextHelper("glGenFencesAPPLE");
}

void NoContextGLApi::glGenFencesNVFn(GLsizei n, GLuint* fences) {
  NoContextHelper("glGenFencesNV");
}

void NoContextGLApi::glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) {
  NoContextHelper("glGenFramebuffersEXT");
}

GLuint NoContextGLApi::glGenPathsNVFn(GLsizei range) {
  NoContextHelper("glGenPathsNV");
  return 0U;
}

void NoContextGLApi::glGenQueriesFn(GLsizei n, GLuint* ids) {
  NoContextHelper("glGenQueries");
}

void NoContextGLApi::glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) {
  NoContextHelper("glGenRenderbuffersEXT");
}

void NoContextGLApi::glGenSamplersFn(GLsizei n, GLuint* samplers) {
  NoContextHelper("glGenSamplers");
}

void NoContextGLApi::glGenTexturesFn(GLsizei n, GLuint* textures) {
  NoContextHelper("glGenTextures");
}

void NoContextGLApi::glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) {
  NoContextHelper("glGenTransformFeedbacks");
}

void NoContextGLApi::glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) {
  NoContextHelper("glGenVertexArraysOES");
}

void NoContextGLApi::glGetActiveAttribFn(GLuint program,
                                         GLuint index,
                                         GLsizei bufsize,
                                         GLsizei* length,
                                         GLint* size,
                                         GLenum* type,
                                         char* name) {
  NoContextHelper("glGetActiveAttrib");
}

void NoContextGLApi::glGetActiveUniformFn(GLuint program,
                                          GLuint index,
                                          GLsizei bufsize,
                                          GLsizei* length,
                                          GLint* size,
                                          GLenum* type,
                                          char* name) {
  NoContextHelper("glGetActiveUniform");
}

void NoContextGLApi::glGetActiveUniformBlockivFn(GLuint program,
                                                 GLuint uniformBlockIndex,
                                                 GLenum pname,
                                                 GLint* params) {
  NoContextHelper("glGetActiveUniformBlockiv");
}

void NoContextGLApi::glGetActiveUniformBlockivRobustANGLEFn(
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  NoContextHelper("glGetActiveUniformBlockivRobustANGLE");
}

void NoContextGLApi::glGetActiveUniformBlockNameFn(GLuint program,
                                                   GLuint uniformBlockIndex,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   char* uniformBlockName) {
  NoContextHelper("glGetActiveUniformBlockName");
}

void NoContextGLApi::glGetActiveUniformsivFn(GLuint program,
                                             GLsizei uniformCount,
                                             const GLuint* uniformIndices,
                                             GLenum pname,
                                             GLint* params) {
  NoContextHelper("glGetActiveUniformsiv");
}

void NoContextGLApi::glGetAttachedShadersFn(GLuint program,
                                            GLsizei maxcount,
                                            GLsizei* count,
                                            GLuint* shaders) {
  NoContextHelper("glGetAttachedShaders");
}

GLint NoContextGLApi::glGetAttribLocationFn(GLuint program, const char* name) {
  NoContextHelper("glGetAttribLocation");
  return 0;
}

void NoContextGLApi::glGetBooleani_vRobustANGLEFn(GLenum target,
                                                  GLuint index,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLboolean* data) {
  NoContextHelper("glGetBooleani_vRobustANGLE");
}

void NoContextGLApi::glGetBooleanvFn(GLenum pname, GLboolean* params) {
  NoContextHelper("glGetBooleanv");
}

void NoContextGLApi::glGetBooleanvRobustANGLEFn(GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLboolean* data) {
  NoContextHelper("glGetBooleanvRobustANGLE");
}

void NoContextGLApi::glGetBufferParameteri64vRobustANGLEFn(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint64* params) {
  NoContextHelper("glGetBufferParameteri64vRobustANGLE");
}

void NoContextGLApi::glGetBufferParameterivFn(GLenum target,
                                              GLenum pname,
                                              GLint* params) {
  NoContextHelper("glGetBufferParameteriv");
}

void NoContextGLApi::glGetBufferParameterivRobustANGLEFn(GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei* length,
                                                         GLint* params) {
  NoContextHelper("glGetBufferParameterivRobustANGLE");
}

void NoContextGLApi::glGetBufferPointervRobustANGLEFn(GLenum target,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      void** params) {
  NoContextHelper("glGetBufferPointervRobustANGLE");
}

void NoContextGLApi::glGetDebugMessageLogFn(GLuint count,
                                            GLsizei bufSize,
                                            GLenum* sources,
                                            GLenum* types,
                                            GLuint* ids,
                                            GLenum* severities,
                                            GLsizei* lengths,
                                            char* messageLog) {
  NoContextHelper("glGetDebugMessageLog");
}

GLenum NoContextGLApi::glGetErrorFn(void) {
  NoContextHelper("glGetError");
  return static_cast<GLenum>(0);
}

void NoContextGLApi::glGetFenceivNVFn(GLuint fence,
                                      GLenum pname,
                                      GLint* params) {
  NoContextHelper("glGetFenceivNV");
}

void NoContextGLApi::glGetFloatvFn(GLenum pname, GLfloat* params) {
  NoContextHelper("glGetFloatv");
}

void NoContextGLApi::glGetFloatvRobustANGLEFn(GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLfloat* data) {
  NoContextHelper("glGetFloatvRobustANGLE");
}

GLint NoContextGLApi::glGetFragDataIndexFn(GLuint program, const char* name) {
  NoContextHelper("glGetFragDataIndex");
  return 0;
}

GLint NoContextGLApi::glGetFragDataLocationFn(GLuint program,
                                              const char* name) {
  NoContextHelper("glGetFragDataLocation");
  return 0;
}

void NoContextGLApi::glGetFramebufferAttachmentParameterivEXTFn(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLint* params) {
  NoContextHelper("glGetFramebufferAttachmentParameterivEXT");
}

void NoContextGLApi::glGetFramebufferAttachmentParameterivRobustANGLEFn(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  NoContextHelper("glGetFramebufferAttachmentParameterivRobustANGLE");
}

void NoContextGLApi::glGetFramebufferParameterivRobustANGLEFn(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLint* params) {
  NoContextHelper("glGetFramebufferParameterivRobustANGLE");
}

GLenum NoContextGLApi::glGetGraphicsResetStatusARBFn(void) {
  NoContextHelper("glGetGraphicsResetStatusARB");
  return static_cast<GLenum>(0);
}

void NoContextGLApi::glGetInteger64i_vFn(GLenum target,
                                         GLuint index,
                                         GLint64* data) {
  NoContextHelper("glGetInteger64i_v");
}

void NoContextGLApi::glGetInteger64i_vRobustANGLEFn(GLenum target,
                                                    GLuint index,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint64* data) {
  NoContextHelper("glGetInteger64i_vRobustANGLE");
}

void NoContextGLApi::glGetInteger64vFn(GLenum pname, GLint64* params) {
  NoContextHelper("glGetInteger64v");
}

void NoContextGLApi::glGetInteger64vRobustANGLEFn(GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint64* data) {
  NoContextHelper("glGetInteger64vRobustANGLE");
}

void NoContextGLApi::glGetIntegeri_vFn(GLenum target,
                                       GLuint index,
                                       GLint* data) {
  NoContextHelper("glGetIntegeri_v");
}

void NoContextGLApi::glGetIntegeri_vRobustANGLEFn(GLenum target,
                                                  GLuint index,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* data) {
  NoContextHelper("glGetIntegeri_vRobustANGLE");
}

void NoContextGLApi::glGetIntegervFn(GLenum pname, GLint* params) {
  NoContextHelper("glGetIntegerv");
}

void NoContextGLApi::glGetIntegervRobustANGLEFn(GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* data) {
  NoContextHelper("glGetIntegervRobustANGLE");
}

void NoContextGLApi::glGetInternalformativFn(GLenum target,
                                             GLenum internalformat,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLint* params) {
  NoContextHelper("glGetInternalformativ");
}

void NoContextGLApi::glGetInternalformativRobustANGLEFn(GLenum target,
                                                        GLenum internalformat,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLint* params) {
  NoContextHelper("glGetInternalformativRobustANGLE");
}

void NoContextGLApi::glGetMultisamplefvFn(GLenum pname,
                                          GLuint index,
                                          GLfloat* val) {
  NoContextHelper("glGetMultisamplefv");
}

void NoContextGLApi::glGetMultisamplefvRobustANGLEFn(GLenum pname,
                                                     GLuint index,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLfloat* val) {
  NoContextHelper("glGetMultisamplefvRobustANGLE");
}

void NoContextGLApi::glGetnUniformfvRobustANGLEFn(GLuint program,
                                                  GLint location,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLfloat* params) {
  NoContextHelper("glGetnUniformfvRobustANGLE");
}

void NoContextGLApi::glGetnUniformivRobustANGLEFn(GLuint program,
                                                  GLint location,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  NoContextHelper("glGetnUniformivRobustANGLE");
}

void NoContextGLApi::glGetnUniformuivRobustANGLEFn(GLuint program,
                                                   GLint location,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLuint* params) {
  NoContextHelper("glGetnUniformuivRobustANGLE");
}

void NoContextGLApi::glGetObjectLabelFn(GLenum identifier,
                                        GLuint name,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        char* label) {
  NoContextHelper("glGetObjectLabel");
}

void NoContextGLApi::glGetObjectPtrLabelFn(void* ptr,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           char* label) {
  NoContextHelper("glGetObjectPtrLabel");
}

void NoContextGLApi::glGetPointervFn(GLenum pname, void** params) {
  NoContextHelper("glGetPointerv");
}

void NoContextGLApi::glGetPointervRobustANGLERobustANGLEFn(GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           void** params) {
  NoContextHelper("glGetPointervRobustANGLERobustANGLE");
}

void NoContextGLApi::glGetProgramBinaryFn(GLuint program,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLenum* binaryFormat,
                                          GLvoid* binary) {
  NoContextHelper("glGetProgramBinary");
}

void NoContextGLApi::glGetProgramInfoLogFn(GLuint program,
                                           GLsizei bufsize,
                                           GLsizei* length,
                                           char* infolog) {
  NoContextHelper("glGetProgramInfoLog");
}

void NoContextGLApi::glGetProgramInterfaceivFn(GLuint program,
                                               GLenum programInterface,
                                               GLenum pname,
                                               GLint* params) {
  NoContextHelper("glGetProgramInterfaceiv");
}

void NoContextGLApi::glGetProgramInterfaceivRobustANGLEFn(
    GLuint program,
    GLenum programInterface,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  NoContextHelper("glGetProgramInterfaceivRobustANGLE");
}

void NoContextGLApi::glGetProgramivFn(GLuint program,
                                      GLenum pname,
                                      GLint* params) {
  NoContextHelper("glGetProgramiv");
}

void NoContextGLApi::glGetProgramivRobustANGLEFn(GLuint program,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  NoContextHelper("glGetProgramivRobustANGLE");
}

void NoContextGLApi::glGetProgramResourceivFn(GLuint program,
                                              GLenum programInterface,
                                              GLuint index,
                                              GLsizei propCount,
                                              const GLenum* props,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* params) {
  NoContextHelper("glGetProgramResourceiv");
}

GLint NoContextGLApi::glGetProgramResourceLocationFn(GLuint program,
                                                     GLenum programInterface,
                                                     const char* name) {
  NoContextHelper("glGetProgramResourceLocation");
  return 0;
}

void NoContextGLApi::glGetProgramResourceNameFn(GLuint program,
                                                GLenum programInterface,
                                                GLuint index,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLchar* name) {
  NoContextHelper("glGetProgramResourceName");
}

void NoContextGLApi::glGetQueryivFn(GLenum target,
                                    GLenum pname,
                                    GLint* params) {
  NoContextHelper("glGetQueryiv");
}

void NoContextGLApi::glGetQueryivRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLint* params) {
  NoContextHelper("glGetQueryivRobustANGLE");
}

void NoContextGLApi::glGetQueryObjecti64vFn(GLuint id,
                                            GLenum pname,
                                            GLint64* params) {
  NoContextHelper("glGetQueryObjecti64v");
}

void NoContextGLApi::glGetQueryObjecti64vRobustANGLEFn(GLuint id,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint64* params) {
  NoContextHelper("glGetQueryObjecti64vRobustANGLE");
}

void NoContextGLApi::glGetQueryObjectivFn(GLuint id,
                                          GLenum pname,
                                          GLint* params) {
  NoContextHelper("glGetQueryObjectiv");
}

void NoContextGLApi::glGetQueryObjectivRobustANGLEFn(GLuint id,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  NoContextHelper("glGetQueryObjectivRobustANGLE");
}

void NoContextGLApi::glGetQueryObjectui64vFn(GLuint id,
                                             GLenum pname,
                                             GLuint64* params) {
  NoContextHelper("glGetQueryObjectui64v");
}

void NoContextGLApi::glGetQueryObjectui64vRobustANGLEFn(GLuint id,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLuint64* params) {
  NoContextHelper("glGetQueryObjectui64vRobustANGLE");
}

void NoContextGLApi::glGetQueryObjectuivFn(GLuint id,
                                           GLenum pname,
                                           GLuint* params) {
  NoContextHelper("glGetQueryObjectuiv");
}

void NoContextGLApi::glGetQueryObjectuivRobustANGLEFn(GLuint id,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLuint* params) {
  NoContextHelper("glGetQueryObjectuivRobustANGLE");
}

void NoContextGLApi::glGetRenderbufferParameterivEXTFn(GLenum target,
                                                       GLenum pname,
                                                       GLint* params) {
  NoContextHelper("glGetRenderbufferParameterivEXT");
}

void NoContextGLApi::glGetRenderbufferParameterivRobustANGLEFn(GLenum target,
                                                               GLenum pname,
                                                               GLsizei bufSize,
                                                               GLsizei* length,
                                                               GLint* params) {
  NoContextHelper("glGetRenderbufferParameterivRobustANGLE");
}

void NoContextGLApi::glGetSamplerParameterfvFn(GLuint sampler,
                                               GLenum pname,
                                               GLfloat* params) {
  NoContextHelper("glGetSamplerParameterfv");
}

void NoContextGLApi::glGetSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLfloat* params) {
  NoContextHelper("glGetSamplerParameterfvRobustANGLE");
}

void NoContextGLApi::glGetSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params) {
  NoContextHelper("glGetSamplerParameterIivRobustANGLE");
}

void NoContextGLApi::glGetSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLuint* params) {
  NoContextHelper("glGetSamplerParameterIuivRobustANGLE");
}

void NoContextGLApi::glGetSamplerParameterivFn(GLuint sampler,
                                               GLenum pname,
                                               GLint* params) {
  NoContextHelper("glGetSamplerParameteriv");
}

void NoContextGLApi::glGetSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params) {
  NoContextHelper("glGetSamplerParameterivRobustANGLE");
}

void NoContextGLApi::glGetShaderInfoLogFn(GLuint shader,
                                          GLsizei bufsize,
                                          GLsizei* length,
                                          char* infolog) {
  NoContextHelper("glGetShaderInfoLog");
}

void NoContextGLApi::glGetShaderivFn(GLuint shader,
                                     GLenum pname,
                                     GLint* params) {
  NoContextHelper("glGetShaderiv");
}

void NoContextGLApi::glGetShaderivRobustANGLEFn(GLuint shader,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* params) {
  NoContextHelper("glGetShaderivRobustANGLE");
}

void NoContextGLApi::glGetShaderPrecisionFormatFn(GLenum shadertype,
                                                  GLenum precisiontype,
                                                  GLint* range,
                                                  GLint* precision) {
  NoContextHelper("glGetShaderPrecisionFormat");
}

void NoContextGLApi::glGetShaderSourceFn(GLuint shader,
                                         GLsizei bufsize,
                                         GLsizei* length,
                                         char* source) {
  NoContextHelper("glGetShaderSource");
}

const GLubyte* NoContextGLApi::glGetStringFn(GLenum name) {
  NoContextHelper("glGetString");
  return NULL;
}

const GLubyte* NoContextGLApi::glGetStringiFn(GLenum name, GLuint index) {
  NoContextHelper("glGetStringi");
  return NULL;
}

void NoContextGLApi::glGetSyncivFn(GLsync sync,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   GLsizei* length,
                                   GLint* values) {
  NoContextHelper("glGetSynciv");
}

void NoContextGLApi::glGetTexLevelParameterfvFn(GLenum target,
                                                GLint level,
                                                GLenum pname,
                                                GLfloat* params) {
  NoContextHelper("glGetTexLevelParameterfv");
}

void NoContextGLApi::glGetTexLevelParameterfvRobustANGLEFn(GLenum target,
                                                           GLint level,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLfloat* params) {
  NoContextHelper("glGetTexLevelParameterfvRobustANGLE");
}

void NoContextGLApi::glGetTexLevelParameterivFn(GLenum target,
                                                GLint level,
                                                GLenum pname,
                                                GLint* params) {
  NoContextHelper("glGetTexLevelParameteriv");
}

void NoContextGLApi::glGetTexLevelParameterivRobustANGLEFn(GLenum target,
                                                           GLint level,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params) {
  NoContextHelper("glGetTexLevelParameterivRobustANGLE");
}

void NoContextGLApi::glGetTexParameterfvFn(GLenum target,
                                           GLenum pname,
                                           GLfloat* params) {
  NoContextHelper("glGetTexParameterfv");
}

void NoContextGLApi::glGetTexParameterfvRobustANGLEFn(GLenum target,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  NoContextHelper("glGetTexParameterfvRobustANGLE");
}

void NoContextGLApi::glGetTexParameterIivRobustANGLEFn(GLenum target,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  NoContextHelper("glGetTexParameterIivRobustANGLE");
}

void NoContextGLApi::glGetTexParameterIuivRobustANGLEFn(GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLuint* params) {
  NoContextHelper("glGetTexParameterIuivRobustANGLE");
}

void NoContextGLApi::glGetTexParameterivFn(GLenum target,
                                           GLenum pname,
                                           GLint* params) {
  NoContextHelper("glGetTexParameteriv");
}

void NoContextGLApi::glGetTexParameterivRobustANGLEFn(GLenum target,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  NoContextHelper("glGetTexParameterivRobustANGLE");
}

void NoContextGLApi::glGetTransformFeedbackVaryingFn(GLuint program,
                                                     GLuint index,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLsizei* size,
                                                     GLenum* type,
                                                     char* name) {
  NoContextHelper("glGetTransformFeedbackVarying");
}

void NoContextGLApi::glGetTranslatedShaderSourceANGLEFn(GLuint shader,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        char* source) {
  NoContextHelper("glGetTranslatedShaderSourceANGLE");
}

GLuint NoContextGLApi::glGetUniformBlockIndexFn(GLuint program,
                                                const char* uniformBlockName) {
  NoContextHelper("glGetUniformBlockIndex");
  return 0U;
}

void NoContextGLApi::glGetUniformfvFn(GLuint program,
                                      GLint location,
                                      GLfloat* params) {
  NoContextHelper("glGetUniformfv");
}

void NoContextGLApi::glGetUniformfvRobustANGLEFn(GLuint program,
                                                 GLint location,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* params) {
  NoContextHelper("glGetUniformfvRobustANGLE");
}

void NoContextGLApi::glGetUniformIndicesFn(GLuint program,
                                           GLsizei uniformCount,
                                           const char* const* uniformNames,
                                           GLuint* uniformIndices) {
  NoContextHelper("glGetUniformIndices");
}

void NoContextGLApi::glGetUniformivFn(GLuint program,
                                      GLint location,
                                      GLint* params) {
  NoContextHelper("glGetUniformiv");
}

void NoContextGLApi::glGetUniformivRobustANGLEFn(GLuint program,
                                                 GLint location,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  NoContextHelper("glGetUniformivRobustANGLE");
}

GLint NoContextGLApi::glGetUniformLocationFn(GLuint program, const char* name) {
  NoContextHelper("glGetUniformLocation");
  return 0;
}

void NoContextGLApi::glGetUniformuivFn(GLuint program,
                                       GLint location,
                                       GLuint* params) {
  NoContextHelper("glGetUniformuiv");
}

void NoContextGLApi::glGetUniformuivRobustANGLEFn(GLuint program,
                                                  GLint location,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLuint* params) {
  NoContextHelper("glGetUniformuivRobustANGLE");
}

void NoContextGLApi::glGetVertexAttribfvFn(GLuint index,
                                           GLenum pname,
                                           GLfloat* params) {
  NoContextHelper("glGetVertexAttribfv");
}

void NoContextGLApi::glGetVertexAttribfvRobustANGLEFn(GLuint index,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  NoContextHelper("glGetVertexAttribfvRobustANGLE");
}

void NoContextGLApi::glGetVertexAttribIivRobustANGLEFn(GLuint index,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  NoContextHelper("glGetVertexAttribIivRobustANGLE");
}

void NoContextGLApi::glGetVertexAttribIuivRobustANGLEFn(GLuint index,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLuint* params) {
  NoContextHelper("glGetVertexAttribIuivRobustANGLE");
}

void NoContextGLApi::glGetVertexAttribivFn(GLuint index,
                                           GLenum pname,
                                           GLint* params) {
  NoContextHelper("glGetVertexAttribiv");
}

void NoContextGLApi::glGetVertexAttribivRobustANGLEFn(GLuint index,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  NoContextHelper("glGetVertexAttribivRobustANGLE");
}

void NoContextGLApi::glGetVertexAttribPointervFn(GLuint index,
                                                 GLenum pname,
                                                 void** pointer) {
  NoContextHelper("glGetVertexAttribPointerv");
}

void NoContextGLApi::glGetVertexAttribPointervRobustANGLEFn(GLuint index,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            void** pointer) {
  NoContextHelper("glGetVertexAttribPointervRobustANGLE");
}

void NoContextGLApi::glHintFn(GLenum target, GLenum mode) {
  NoContextHelper("glHint");
}

void NoContextGLApi::glInsertEventMarkerEXTFn(GLsizei length,
                                              const char* marker) {
  NoContextHelper("glInsertEventMarkerEXT");
}

void NoContextGLApi::glInvalidateFramebufferFn(GLenum target,
                                               GLsizei numAttachments,
                                               const GLenum* attachments) {
  NoContextHelper("glInvalidateFramebuffer");
}

void NoContextGLApi::glInvalidateSubFramebufferFn(GLenum target,
                                                  GLsizei numAttachments,
                                                  const GLenum* attachments,
                                                  GLint x,
                                                  GLint y,
                                                  GLint width,
                                                  GLint height) {
  NoContextHelper("glInvalidateSubFramebuffer");
}

GLboolean NoContextGLApi::glIsBufferFn(GLuint buffer) {
  NoContextHelper("glIsBuffer");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsEnabledFn(GLenum cap) {
  NoContextHelper("glIsEnabled");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsFenceAPPLEFn(GLuint fence) {
  NoContextHelper("glIsFenceAPPLE");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsFenceNVFn(GLuint fence) {
  NoContextHelper("glIsFenceNV");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsFramebufferEXTFn(GLuint framebuffer) {
  NoContextHelper("glIsFramebufferEXT");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsPathNVFn(GLuint path) {
  NoContextHelper("glIsPathNV");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsProgramFn(GLuint program) {
  NoContextHelper("glIsProgram");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsQueryFn(GLuint query) {
  NoContextHelper("glIsQuery");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsRenderbufferEXTFn(GLuint renderbuffer) {
  NoContextHelper("glIsRenderbufferEXT");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsSamplerFn(GLuint sampler) {
  NoContextHelper("glIsSampler");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsShaderFn(GLuint shader) {
  NoContextHelper("glIsShader");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsSyncFn(GLsync sync) {
  NoContextHelper("glIsSync");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsTextureFn(GLuint texture) {
  NoContextHelper("glIsTexture");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsTransformFeedbackFn(GLuint id) {
  NoContextHelper("glIsTransformFeedback");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsVertexArrayOESFn(GLuint array) {
  NoContextHelper("glIsVertexArrayOES");
  return GL_FALSE;
}

void NoContextGLApi::glLineWidthFn(GLfloat width) {
  NoContextHelper("glLineWidth");
}

void NoContextGLApi::glLinkProgramFn(GLuint program) {
  NoContextHelper("glLinkProgram");
}

void* NoContextGLApi::glMapBufferFn(GLenum target, GLenum access) {
  NoContextHelper("glMapBuffer");
  return NULL;
}

void* NoContextGLApi::glMapBufferRangeFn(GLenum target,
                                         GLintptr offset,
                                         GLsizeiptr length,
                                         GLbitfield access) {
  NoContextHelper("glMapBufferRange");
  return NULL;
}

void NoContextGLApi::glMatrixLoadfEXTFn(GLenum matrixMode, const GLfloat* m) {
  NoContextHelper("glMatrixLoadfEXT");
}

void NoContextGLApi::glMatrixLoadIdentityEXTFn(GLenum matrixMode) {
  NoContextHelper("glMatrixLoadIdentityEXT");
}

void NoContextGLApi::glMemoryBarrierEXTFn(GLbitfield barriers) {
  NoContextHelper("glMemoryBarrierEXT");
}

void NoContextGLApi::glObjectLabelFn(GLenum identifier,
                                     GLuint name,
                                     GLsizei length,
                                     const char* label) {
  NoContextHelper("glObjectLabel");
}

void NoContextGLApi::glObjectPtrLabelFn(void* ptr,
                                        GLsizei length,
                                        const char* label) {
  NoContextHelper("glObjectPtrLabel");
}

void NoContextGLApi::glPathCommandsNVFn(GLuint path,
                                        GLsizei numCommands,
                                        const GLubyte* commands,
                                        GLsizei numCoords,
                                        GLenum coordType,
                                        const GLvoid* coords) {
  NoContextHelper("glPathCommandsNV");
}

void NoContextGLApi::glPathParameterfNVFn(GLuint path,
                                          GLenum pname,
                                          GLfloat value) {
  NoContextHelper("glPathParameterfNV");
}

void NoContextGLApi::glPathParameteriNVFn(GLuint path,
                                          GLenum pname,
                                          GLint value) {
  NoContextHelper("glPathParameteriNV");
}

void NoContextGLApi::glPathStencilFuncNVFn(GLenum func,
                                           GLint ref,
                                           GLuint mask) {
  NoContextHelper("glPathStencilFuncNV");
}

void NoContextGLApi::glPauseTransformFeedbackFn(void) {
  NoContextHelper("glPauseTransformFeedback");
}

void NoContextGLApi::glPixelStoreiFn(GLenum pname, GLint param) {
  NoContextHelper("glPixelStorei");
}

void NoContextGLApi::glPointParameteriFn(GLenum pname, GLint param) {
  NoContextHelper("glPointParameteri");
}

void NoContextGLApi::glPolygonModeFn(GLenum face, GLenum mode) {
  NoContextHelper("glPolygonMode");
}

void NoContextGLApi::glPolygonOffsetFn(GLfloat factor, GLfloat units) {
  NoContextHelper("glPolygonOffset");
}

void NoContextGLApi::glPopDebugGroupFn() {
  NoContextHelper("glPopDebugGroup");
}

void NoContextGLApi::glPopGroupMarkerEXTFn(void) {
  NoContextHelper("glPopGroupMarkerEXT");
}

void NoContextGLApi::glPrimitiveRestartIndexFn(GLuint index) {
  NoContextHelper("glPrimitiveRestartIndex");
}

void NoContextGLApi::glProgramBinaryFn(GLuint program,
                                       GLenum binaryFormat,
                                       const GLvoid* binary,
                                       GLsizei length) {
  NoContextHelper("glProgramBinary");
}

void NoContextGLApi::glProgramParameteriFn(GLuint program,
                                           GLenum pname,
                                           GLint value) {
  NoContextHelper("glProgramParameteri");
}

void NoContextGLApi::glProgramPathFragmentInputGenNVFn(GLuint program,
                                                       GLint location,
                                                       GLenum genMode,
                                                       GLint components,
                                                       const GLfloat* coeffs) {
  NoContextHelper("glProgramPathFragmentInputGenNV");
}

void NoContextGLApi::glPushDebugGroupFn(GLenum source,
                                        GLuint id,
                                        GLsizei length,
                                        const char* message) {
  NoContextHelper("glPushDebugGroup");
}

void NoContextGLApi::glPushGroupMarkerEXTFn(GLsizei length,
                                            const char* marker) {
  NoContextHelper("glPushGroupMarkerEXT");
}

void NoContextGLApi::glQueryCounterFn(GLuint id, GLenum target) {
  NoContextHelper("glQueryCounter");
}

void NoContextGLApi::glReadBufferFn(GLenum src) {
  NoContextHelper("glReadBuffer");
}

void NoContextGLApi::glReadnPixelsRobustANGLEFn(GLint x,
                                                GLint y,
                                                GLsizei width,
                                                GLsizei height,
                                                GLenum format,
                                                GLenum type,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLsizei* columns,
                                                GLsizei* rows,
                                                void* data) {
  NoContextHelper("glReadnPixelsRobustANGLE");
}

void NoContextGLApi::glReadPixelsFn(GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLenum format,
                                    GLenum type,
                                    void* pixels) {
  NoContextHelper("glReadPixels");
}

void NoContextGLApi::glReadPixelsRobustANGLEFn(GLint x,
                                               GLint y,
                                               GLsizei width,
                                               GLsizei height,
                                               GLenum format,
                                               GLenum type,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLsizei* columns,
                                               GLsizei* rows,
                                               void* pixels) {
  NoContextHelper("glReadPixelsRobustANGLE");
}

void NoContextGLApi::glReleaseShaderCompilerFn(void) {
  NoContextHelper("glReleaseShaderCompiler");
}

void NoContextGLApi::glRenderbufferStorageEXTFn(GLenum target,
                                                GLenum internalformat,
                                                GLsizei width,
                                                GLsizei height) {
  NoContextHelper("glRenderbufferStorageEXT");
}

void NoContextGLApi::glRenderbufferStorageMultisampleFn(GLenum target,
                                                        GLsizei samples,
                                                        GLenum internalformat,
                                                        GLsizei width,
                                                        GLsizei height) {
  NoContextHelper("glRenderbufferStorageMultisample");
}

void NoContextGLApi::glRenderbufferStorageMultisampleEXTFn(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  NoContextHelper("glRenderbufferStorageMultisampleEXT");
}

void NoContextGLApi::glRequestExtensionANGLEFn(const char* name) {
  NoContextHelper("glRequestExtensionANGLE");
}

void NoContextGLApi::glResumeTransformFeedbackFn(void) {
  NoContextHelper("glResumeTransformFeedback");
}

void NoContextGLApi::glSampleCoverageFn(GLclampf value, GLboolean invert) {
  NoContextHelper("glSampleCoverage");
}

void NoContextGLApi::glSamplerParameterfFn(GLuint sampler,
                                           GLenum pname,
                                           GLfloat param) {
  NoContextHelper("glSamplerParameterf");
}

void NoContextGLApi::glSamplerParameterfvFn(GLuint sampler,
                                            GLenum pname,
                                            const GLfloat* params) {
  NoContextHelper("glSamplerParameterfv");
}

void NoContextGLApi::glSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       const GLfloat* param) {
  NoContextHelper("glSamplerParameterfvRobustANGLE");
}

void NoContextGLApi::glSamplerParameteriFn(GLuint sampler,
                                           GLenum pname,
                                           GLint param) {
  NoContextHelper("glSamplerParameteri");
}

void NoContextGLApi::glSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        const GLint* param) {
  NoContextHelper("glSamplerParameterIivRobustANGLE");
}

void NoContextGLApi::glSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         const GLuint* param) {
  NoContextHelper("glSamplerParameterIuivRobustANGLE");
}

void NoContextGLApi::glSamplerParameterivFn(GLuint sampler,
                                            GLenum pname,
                                            const GLint* params) {
  NoContextHelper("glSamplerParameteriv");
}

void NoContextGLApi::glSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       const GLint* param) {
  NoContextHelper("glSamplerParameterivRobustANGLE");
}

void NoContextGLApi::glScissorFn(GLint x,
                                 GLint y,
                                 GLsizei width,
                                 GLsizei height) {
  NoContextHelper("glScissor");
}

void NoContextGLApi::glSetFenceAPPLEFn(GLuint fence) {
  NoContextHelper("glSetFenceAPPLE");
}

void NoContextGLApi::glSetFenceNVFn(GLuint fence, GLenum condition) {
  NoContextHelper("glSetFenceNV");
}

void NoContextGLApi::glShaderBinaryFn(GLsizei n,
                                      const GLuint* shaders,
                                      GLenum binaryformat,
                                      const void* binary,
                                      GLsizei length) {
  NoContextHelper("glShaderBinary");
}

void NoContextGLApi::glShaderSourceFn(GLuint shader,
                                      GLsizei count,
                                      const char* const* str,
                                      const GLint* length) {
  NoContextHelper("glShaderSource");
}

void NoContextGLApi::glStencilFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  NoContextHelper("glStencilFillPathInstancedNV");
}

void NoContextGLApi::glStencilFillPathNVFn(GLuint path,
                                           GLenum fillMode,
                                           GLuint mask) {
  NoContextHelper("glStencilFillPathNV");
}

void NoContextGLApi::glStencilFuncFn(GLenum func, GLint ref, GLuint mask) {
  NoContextHelper("glStencilFunc");
}

void NoContextGLApi::glStencilFuncSeparateFn(GLenum face,
                                             GLenum func,
                                             GLint ref,
                                             GLuint mask) {
  NoContextHelper("glStencilFuncSeparate");
}

void NoContextGLApi::glStencilMaskFn(GLuint mask) {
  NoContextHelper("glStencilMask");
}

void NoContextGLApi::glStencilMaskSeparateFn(GLenum face, GLuint mask) {
  NoContextHelper("glStencilMaskSeparate");
}

void NoContextGLApi::glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) {
  NoContextHelper("glStencilOp");
}

void NoContextGLApi::glStencilOpSeparateFn(GLenum face,
                                           GLenum fail,
                                           GLenum zfail,
                                           GLenum zpass) {
  NoContextHelper("glStencilOpSeparate");
}

void NoContextGLApi::glStencilStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  NoContextHelper("glStencilStrokePathInstancedNV");
}

void NoContextGLApi::glStencilStrokePathNVFn(GLuint path,
                                             GLint reference,
                                             GLuint mask) {
  NoContextHelper("glStencilStrokePathNV");
}

void NoContextGLApi::glStencilThenCoverFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  NoContextHelper("glStencilThenCoverFillPathInstancedNV");
}

void NoContextGLApi::glStencilThenCoverFillPathNVFn(GLuint path,
                                                    GLenum fillMode,
                                                    GLuint mask,
                                                    GLenum coverMode) {
  NoContextHelper("glStencilThenCoverFillPathNV");
}

void NoContextGLApi::glStencilThenCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  NoContextHelper("glStencilThenCoverStrokePathInstancedNV");
}

void NoContextGLApi::glStencilThenCoverStrokePathNVFn(GLuint path,
                                                      GLint reference,
                                                      GLuint mask,
                                                      GLenum coverMode) {
  NoContextHelper("glStencilThenCoverStrokePathNV");
}

GLboolean NoContextGLApi::glTestFenceAPPLEFn(GLuint fence) {
  NoContextHelper("glTestFenceAPPLE");
  return GL_FALSE;
}

GLboolean NoContextGLApi::glTestFenceNVFn(GLuint fence) {
  NoContextHelper("glTestFenceNV");
  return GL_FALSE;
}

void NoContextGLApi::glTexBufferFn(GLenum target,
                                   GLenum internalformat,
                                   GLuint buffer) {
  NoContextHelper("glTexBuffer");
}

void NoContextGLApi::glTexBufferRangeFn(GLenum target,
                                        GLenum internalformat,
                                        GLuint buffer,
                                        GLintptr offset,
                                        GLsizeiptr size) {
  NoContextHelper("glTexBufferRange");
}

void NoContextGLApi::glTexImage2DFn(GLenum target,
                                    GLint level,
                                    GLint internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLint border,
                                    GLenum format,
                                    GLenum type,
                                    const void* pixels) {
  NoContextHelper("glTexImage2D");
}

void NoContextGLApi::glTexImage2DRobustANGLEFn(GLenum target,
                                               GLint level,
                                               GLint internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               GLint border,
                                               GLenum format,
                                               GLenum type,
                                               GLsizei bufSize,
                                               const void* pixels) {
  NoContextHelper("glTexImage2DRobustANGLE");
}

void NoContextGLApi::glTexImage3DFn(GLenum target,
                                    GLint level,
                                    GLint internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLsizei depth,
                                    GLint border,
                                    GLenum format,
                                    GLenum type,
                                    const void* pixels) {
  NoContextHelper("glTexImage3D");
}

void NoContextGLApi::glTexImage3DRobustANGLEFn(GLenum target,
                                               GLint level,
                                               GLint internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               GLsizei depth,
                                               GLint border,
                                               GLenum format,
                                               GLenum type,
                                               GLsizei bufSize,
                                               const void* pixels) {
  NoContextHelper("glTexImage3DRobustANGLE");
}

void NoContextGLApi::glTexParameterfFn(GLenum target,
                                       GLenum pname,
                                       GLfloat param) {
  NoContextHelper("glTexParameterf");
}

void NoContextGLApi::glTexParameterfvFn(GLenum target,
                                        GLenum pname,
                                        const GLfloat* params) {
  NoContextHelper("glTexParameterfv");
}

void NoContextGLApi::glTexParameterfvRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLfloat* params) {
  NoContextHelper("glTexParameterfvRobustANGLE");
}

void NoContextGLApi::glTexParameteriFn(GLenum target,
                                       GLenum pname,
                                       GLint param) {
  NoContextHelper("glTexParameteri");
}

void NoContextGLApi::glTexParameterIivRobustANGLEFn(GLenum target,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    const GLint* params) {
  NoContextHelper("glTexParameterIivRobustANGLE");
}

void NoContextGLApi::glTexParameterIuivRobustANGLEFn(GLenum target,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     const GLuint* params) {
  NoContextHelper("glTexParameterIuivRobustANGLE");
}

void NoContextGLApi::glTexParameterivFn(GLenum target,
                                        GLenum pname,
                                        const GLint* params) {
  NoContextHelper("glTexParameteriv");
}

void NoContextGLApi::glTexParameterivRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLint* params) {
  NoContextHelper("glTexParameterivRobustANGLE");
}

void NoContextGLApi::glTexStorage2DEXTFn(GLenum target,
                                         GLsizei levels,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height) {
  NoContextHelper("glTexStorage2DEXT");
}

void NoContextGLApi::glTexStorage3DFn(GLenum target,
                                      GLsizei levels,
                                      GLenum internalformat,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth) {
  NoContextHelper("glTexStorage3D");
}

void NoContextGLApi::glTexSubImage2DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLenum format,
                                       GLenum type,
                                       const void* pixels) {
  NoContextHelper("glTexSubImage2D");
}

void NoContextGLApi::glTexSubImage2DRobustANGLEFn(GLenum target,
                                                  GLint level,
                                                  GLint xoffset,
                                                  GLint yoffset,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLenum format,
                                                  GLenum type,
                                                  GLsizei bufSize,
                                                  const void* pixels) {
  NoContextHelper("glTexSubImage2DRobustANGLE");
}

void NoContextGLApi::glTexSubImage3DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       GLenum format,
                                       GLenum type,
                                       const void* pixels) {
  NoContextHelper("glTexSubImage3D");
}

void NoContextGLApi::glTexSubImage3DRobustANGLEFn(GLenum target,
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
                                                  const void* pixels) {
  NoContextHelper("glTexSubImage3DRobustANGLE");
}

void NoContextGLApi::glTransformFeedbackVaryingsFn(GLuint program,
                                                   GLsizei count,
                                                   const char* const* varyings,
                                                   GLenum bufferMode) {
  NoContextHelper("glTransformFeedbackVaryings");
}

void NoContextGLApi::glUniform1fFn(GLint location, GLfloat x) {
  NoContextHelper("glUniform1f");
}

void NoContextGLApi::glUniform1fvFn(GLint location,
                                    GLsizei count,
                                    const GLfloat* v) {
  NoContextHelper("glUniform1fv");
}

void NoContextGLApi::glUniform1iFn(GLint location, GLint x) {
  NoContextHelper("glUniform1i");
}

void NoContextGLApi::glUniform1ivFn(GLint location,
                                    GLsizei count,
                                    const GLint* v) {
  NoContextHelper("glUniform1iv");
}

void NoContextGLApi::glUniform1uiFn(GLint location, GLuint v0) {
  NoContextHelper("glUniform1ui");
}

void NoContextGLApi::glUniform1uivFn(GLint location,
                                     GLsizei count,
                                     const GLuint* v) {
  NoContextHelper("glUniform1uiv");
}

void NoContextGLApi::glUniform2fFn(GLint location, GLfloat x, GLfloat y) {
  NoContextHelper("glUniform2f");
}

void NoContextGLApi::glUniform2fvFn(GLint location,
                                    GLsizei count,
                                    const GLfloat* v) {
  NoContextHelper("glUniform2fv");
}

void NoContextGLApi::glUniform2iFn(GLint location, GLint x, GLint y) {
  NoContextHelper("glUniform2i");
}

void NoContextGLApi::glUniform2ivFn(GLint location,
                                    GLsizei count,
                                    const GLint* v) {
  NoContextHelper("glUniform2iv");
}

void NoContextGLApi::glUniform2uiFn(GLint location, GLuint v0, GLuint v1) {
  NoContextHelper("glUniform2ui");
}

void NoContextGLApi::glUniform2uivFn(GLint location,
                                     GLsizei count,
                                     const GLuint* v) {
  NoContextHelper("glUniform2uiv");
}

void NoContextGLApi::glUniform3fFn(GLint location,
                                   GLfloat x,
                                   GLfloat y,
                                   GLfloat z) {
  NoContextHelper("glUniform3f");
}

void NoContextGLApi::glUniform3fvFn(GLint location,
                                    GLsizei count,
                                    const GLfloat* v) {
  NoContextHelper("glUniform3fv");
}

void NoContextGLApi::glUniform3iFn(GLint location, GLint x, GLint y, GLint z) {
  NoContextHelper("glUniform3i");
}

void NoContextGLApi::glUniform3ivFn(GLint location,
                                    GLsizei count,
                                    const GLint* v) {
  NoContextHelper("glUniform3iv");
}

void NoContextGLApi::glUniform3uiFn(GLint location,
                                    GLuint v0,
                                    GLuint v1,
                                    GLuint v2) {
  NoContextHelper("glUniform3ui");
}

void NoContextGLApi::glUniform3uivFn(GLint location,
                                     GLsizei count,
                                     const GLuint* v) {
  NoContextHelper("glUniform3uiv");
}

void NoContextGLApi::glUniform4fFn(GLint location,
                                   GLfloat x,
                                   GLfloat y,
                                   GLfloat z,
                                   GLfloat w) {
  NoContextHelper("glUniform4f");
}

void NoContextGLApi::glUniform4fvFn(GLint location,
                                    GLsizei count,
                                    const GLfloat* v) {
  NoContextHelper("glUniform4fv");
}

void NoContextGLApi::glUniform4iFn(GLint location,
                                   GLint x,
                                   GLint y,
                                   GLint z,
                                   GLint w) {
  NoContextHelper("glUniform4i");
}

void NoContextGLApi::glUniform4ivFn(GLint location,
                                    GLsizei count,
                                    const GLint* v) {
  NoContextHelper("glUniform4iv");
}

void NoContextGLApi::glUniform4uiFn(GLint location,
                                    GLuint v0,
                                    GLuint v1,
                                    GLuint v2,
                                    GLuint v3) {
  NoContextHelper("glUniform4ui");
}

void NoContextGLApi::glUniform4uivFn(GLint location,
                                     GLsizei count,
                                     const GLuint* v) {
  NoContextHelper("glUniform4uiv");
}

void NoContextGLApi::glUniformBlockBindingFn(GLuint program,
                                             GLuint uniformBlockIndex,
                                             GLuint uniformBlockBinding) {
  NoContextHelper("glUniformBlockBinding");
}

void NoContextGLApi::glUniformMatrix2fvFn(GLint location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat* value) {
  NoContextHelper("glUniformMatrix2fv");
}

void NoContextGLApi::glUniformMatrix2x3fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NoContextHelper("glUniformMatrix2x3fv");
}

void NoContextGLApi::glUniformMatrix2x4fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NoContextHelper("glUniformMatrix2x4fv");
}

void NoContextGLApi::glUniformMatrix3fvFn(GLint location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat* value) {
  NoContextHelper("glUniformMatrix3fv");
}

void NoContextGLApi::glUniformMatrix3x2fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NoContextHelper("glUniformMatrix3x2fv");
}

void NoContextGLApi::glUniformMatrix3x4fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NoContextHelper("glUniformMatrix3x4fv");
}

void NoContextGLApi::glUniformMatrix4fvFn(GLint location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat* value) {
  NoContextHelper("glUniformMatrix4fv");
}

void NoContextGLApi::glUniformMatrix4x2fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NoContextHelper("glUniformMatrix4x2fv");
}

void NoContextGLApi::glUniformMatrix4x3fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NoContextHelper("glUniformMatrix4x3fv");
}

GLboolean NoContextGLApi::glUnmapBufferFn(GLenum target) {
  NoContextHelper("glUnmapBuffer");
  return GL_FALSE;
}

void NoContextGLApi::glUseProgramFn(GLuint program) {
  NoContextHelper("glUseProgram");
}

void NoContextGLApi::glValidateProgramFn(GLuint program) {
  NoContextHelper("glValidateProgram");
}

void NoContextGLApi::glVertexAttrib1fFn(GLuint indx, GLfloat x) {
  NoContextHelper("glVertexAttrib1f");
}

void NoContextGLApi::glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) {
  NoContextHelper("glVertexAttrib1fv");
}

void NoContextGLApi::glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) {
  NoContextHelper("glVertexAttrib2f");
}

void NoContextGLApi::glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) {
  NoContextHelper("glVertexAttrib2fv");
}

void NoContextGLApi::glVertexAttrib3fFn(GLuint indx,
                                        GLfloat x,
                                        GLfloat y,
                                        GLfloat z) {
  NoContextHelper("glVertexAttrib3f");
}

void NoContextGLApi::glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) {
  NoContextHelper("glVertexAttrib3fv");
}

void NoContextGLApi::glVertexAttrib4fFn(GLuint indx,
                                        GLfloat x,
                                        GLfloat y,
                                        GLfloat z,
                                        GLfloat w) {
  NoContextHelper("glVertexAttrib4f");
}

void NoContextGLApi::glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) {
  NoContextHelper("glVertexAttrib4fv");
}

void NoContextGLApi::glVertexAttribDivisorANGLEFn(GLuint index,
                                                  GLuint divisor) {
  NoContextHelper("glVertexAttribDivisorANGLE");
}

void NoContextGLApi::glVertexAttribI4iFn(GLuint indx,
                                         GLint x,
                                         GLint y,
                                         GLint z,
                                         GLint w) {
  NoContextHelper("glVertexAttribI4i");
}

void NoContextGLApi::glVertexAttribI4ivFn(GLuint indx, const GLint* values) {
  NoContextHelper("glVertexAttribI4iv");
}

void NoContextGLApi::glVertexAttribI4uiFn(GLuint indx,
                                          GLuint x,
                                          GLuint y,
                                          GLuint z,
                                          GLuint w) {
  NoContextHelper("glVertexAttribI4ui");
}

void NoContextGLApi::glVertexAttribI4uivFn(GLuint indx, const GLuint* values) {
  NoContextHelper("glVertexAttribI4uiv");
}

void NoContextGLApi::glVertexAttribIPointerFn(GLuint indx,
                                              GLint size,
                                              GLenum type,
                                              GLsizei stride,
                                              const void* ptr) {
  NoContextHelper("glVertexAttribIPointer");
}

void NoContextGLApi::glVertexAttribPointerFn(GLuint indx,
                                             GLint size,
                                             GLenum type,
                                             GLboolean normalized,
                                             GLsizei stride,
                                             const void* ptr) {
  NoContextHelper("glVertexAttribPointer");
}

void NoContextGLApi::glViewportFn(GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height) {
  NoContextHelper("glViewport");
}

void NoContextGLApi::glWaitSyncFn(GLsync sync,
                                  GLbitfield flags,
                                  GLuint64 timeout) {
  NoContextHelper("glWaitSync");
}

void NoContextGLApi::glWindowRectanglesEXTFn(GLenum mode,
                                             GLsizei n,
                                             const GLint* box) {
  NoContextHelper("glWindowRectanglesEXT");
}

}  // namespace gl
