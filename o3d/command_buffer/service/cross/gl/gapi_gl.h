/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the GAPIGL class, implementing the GAPI interface for
// GL.

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_GAPI_GL_H_
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_GAPI_GL_H_

#include <build/build_config.h>
#include <GL/glew.h>
#include <Cg/cg.h>
#include <Cg/cgGL.h>
#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/service/cross/gl/gl_utils.h"
#include "command_buffer/service/cross/gl/effect_gl.h"
#include "command_buffer/service/cross/gl/geometry_gl.h"
#include "command_buffer/service/cross/gl/render_surface_gl.h"
#include "command_buffer/service/cross/gl/sampler_gl.h"
#include "command_buffer/service/cross/gl/texture_gl.h"

namespace o3d {
namespace command_buffer {
namespace o3d {
#if defined(OS_LINUX)
class XWindowWrapper;
#endif  // defined(OS_LINUX)

// This class implements the GAPI interface for GL.
class GAPIGL : public GAPIInterface {
 public:
  GAPIGL();
  virtual ~GAPIGL();

#if defined(OS_LINUX)
  void set_window_wrapper(XWindowWrapper *window) { window_ = window; }
#elif defined(OS_WIN)
  void set_hwnd(HWND hwnd) { hwnd_ = hwnd; }

  HWND hwnd() const {
    return hwnd_;
  }
#endif

  // Initializes the graphics context, bound to a window.
  // Returns:
  //   true if successful.
  virtual bool Initialize();

  // Initailizes Cg.
  void InitCG();

  // Helper function for Initailize that inits just glew.
  bool InitGlew();

  // Destroys the graphics context.
  virtual void Destroy();

  // Implements the BeginFrame function for GL.
  virtual void BeginFrame();

  // Implements the EndFrame function for GL.
  virtual void EndFrame();

  // Implements the Clear function for GL.
  virtual void Clear(unsigned int buffers,
                     const RGBA &color,
                     float depth,
                     unsigned int stencil);

  // Implements the SetViewport function for GL.
  virtual void SetViewport(unsigned int x,
                           unsigned int y,
                           unsigned int width,
                           unsigned int height,
                           float z_min,
                           float z_max);

  // Implements the CreateVertexBuffer function for GL.
  virtual ParseError CreateVertexBuffer(ResourceId id,
                                        unsigned int size,
                                        unsigned int flags);

  // Implements the DestroyVertexBuffer function for GL.
  virtual ParseError DestroyVertexBuffer(ResourceId id);

  // Implements the SetVertexBufferData function for GL.
  virtual ParseError SetVertexBufferData(ResourceId id,
                                         unsigned int offset,
                                         unsigned int size,
                                         const void *data);

  // Implements the GetVertexBufferData function for GL.
  virtual ParseError GetVertexBufferData(ResourceId id,
                                         unsigned int offset,
                                         unsigned int size,
                                         void *data);

  // Implements the CreateIndexBuffer function for GL.
  virtual ParseError CreateIndexBuffer(ResourceId id,
                                       unsigned int size,
                                       unsigned int flags);

  // Implements the DestroyIndexBuffer function for GL.
  virtual ParseError DestroyIndexBuffer(ResourceId id);

  // Implements the SetIndexBufferData function for GL.
  virtual ParseError SetIndexBufferData(ResourceId id,
                                        unsigned int offset,
                                        unsigned int size,
                                        const void *data);

  // Implements the GetIndexBufferData function for GL.
  virtual ParseError GetIndexBufferData(ResourceId id,
                                        unsigned int offset,
                                        unsigned int size,
                                        void *data);

  // Implements the CreateVertexStruct function for GL.
  virtual ParseError CreateVertexStruct(ResourceId id,
                                        unsigned int input_count);

  // Implements the DestroyVertexStruct function for GL.
  virtual ParseError DestroyVertexStruct(ResourceId id);

  // Implements the SetVertexInput function for GL.
  virtual ParseError SetVertexInput(ResourceId vertex_struct_id,
                                    unsigned int input_index,
                                    ResourceId vertex_buffer_id,
                                    unsigned int offset,
                                    unsigned int stride,
                                    vertex_struct::Type type,
                                    vertex_struct::Semantic semantic,
                                    unsigned int semantic_index);

  // Implements the SetVertexStruct function for GL.
  virtual ParseError SetVertexStruct(ResourceId id);

  // Implements the Draw function for GL.
  virtual ParseError Draw(PrimitiveType primitive_type,
                          unsigned int first,
                          unsigned int count);

  // Implements the DrawIndexed function for GL.
  virtual ParseError DrawIndexed(PrimitiveType primitive_type,
                                 ResourceId index_buffer_id,
                                 unsigned int first,
                                 unsigned int count,
                                 unsigned int min_index,
                                 unsigned int max_index);

  // Implements the CreateEffect function for GL.
  virtual ParseError CreateEffect(ResourceId id,
                                  unsigned int size,
                                  const void *data);

  // Implements the DestroyEffect function for GL.
  virtual ParseError DestroyEffect(ResourceId id);

  // Implements the SetEffect function for GL.
  virtual ParseError SetEffect(ResourceId id);

  // Implements the GetParamCount function for GL.
  virtual ParseError GetParamCount(ResourceId id,
                                   unsigned int size,
                                   void *data);

  // Implements the CreateParam function for GL.
  virtual ParseError CreateParam(ResourceId param_id,
                                 ResourceId effect_id,
                                 unsigned int index);

  // Implements the CreateParamByName function for GL.
  virtual ParseError CreateParamByName(ResourceId param_id,
                                       ResourceId effect_id,
                                       unsigned int size,
                                       const void *name);

  // Implements the DestroyParam function for GL.
  virtual ParseError DestroyParam(ResourceId id);

  // Implements the SetParamData function for GL.
  virtual ParseError SetParamData(ResourceId id,
                                  unsigned int size,
                                  const void *data);

  // Implements the GetParamDesc function for GL.
  virtual ParseError GetParamDesc(ResourceId id,
                                  unsigned int size,
                                  void *data);

  // Implements the GetStreamCount function for GL.
  virtual ParseError GetStreamCount(ResourceId id,
                                    unsigned int size,
                                    void *data);

  // Implements the GetStreamDesc function for GL.
  virtual ParseError GetStreamDesc(ResourceId id,
                                   unsigned int index,
                                   unsigned int size,
                                   void *data);

  // Implements the CreateTexture2D function for GL.
  virtual ParseError CreateTexture2D(ResourceId id,
                                     unsigned int width,
                                     unsigned int height,
                                     unsigned int levels,
                                     texture::Format format,
                                     unsigned int flags,
                                     bool enable_render_surfaces);

  // Implements the CreateTexture3D function for GL.
  virtual ParseError CreateTexture3D(ResourceId id,
                                     unsigned int width,
                                     unsigned int height,
                                     unsigned int depth,
                                     unsigned int levels,
                                     texture::Format format,
                                     unsigned int flags,
                                     bool enable_render_surfaces);

  // Implements the CreateTextureCube function for GL.
  virtual ParseError CreateTextureCube(ResourceId id,
                                       unsigned int side,
                                       unsigned int levels,
                                       texture::Format format,
                                       unsigned int flags,
                                       bool enable_render_surfaces);

  // Implements the SetTextureData function for GL.
  virtual ParseError SetTextureData(ResourceId id,
                                    unsigned int x,
                                    unsigned int y,
                                    unsigned int z,
                                    unsigned int width,
                                    unsigned int height,
                                    unsigned int depth,
                                    unsigned int level,
                                    texture::Face face,
                                    unsigned int pitch,
                                    unsigned int slice_pitch,
                                    unsigned int size,
                                    const void *data);

  // Implements the GetTextureData function for GL.
  virtual ParseError GetTextureData(ResourceId id,
                                    unsigned int x,
                                    unsigned int y,
                                    unsigned int z,
                                    unsigned int width,
                                    unsigned int height,
                                    unsigned int depth,
                                    unsigned int level,
                                    texture::Face face,
                                    unsigned int pitch,
                                    unsigned int slice_pitch,
                                    unsigned int size,
                                    void *data);

  // Implements the DestroyTexture function for GL.
  virtual ParseError DestroyTexture(ResourceId id);

  // Implements the CreateSampler function for GL.
  virtual ParseError CreateSampler(ResourceId id);

  // Implements the DestroySampler function for GL.
  virtual ParseError DestroySampler(ResourceId id);

  // Implements the SetSamplerStates function for GL.
  virtual ParseError SetSamplerStates(ResourceId id,
                                      sampler::AddressingMode addressing_u,
                                      sampler::AddressingMode addressing_v,
                                      sampler::AddressingMode addressing_w,
                                      sampler::FilteringMode mag_filter,
                                      sampler::FilteringMode min_filter,
                                      sampler::FilteringMode mip_filter,
                                      unsigned int max_anisotropy);

  // Implements the SetSamplerBorderColor function for GL.
  virtual ParseError SetSamplerBorderColor(ResourceId id, const RGBA &color);

  // Implements the SetSamplerTexture function for GL.
  virtual ParseError SetSamplerTexture(ResourceId id, ResourceId texture_id);

  // Implements the SetScissor function for GL.
  virtual void SetScissor(bool enable,
                          unsigned int x,
                          unsigned int y,
                          unsigned int width,
                          unsigned int height);

  // Implements the SetPointLineRaster function for GL.
  virtual void SetPointLineRaster(bool line_smooth,
                                  bool point_sprite,
                                  float point_size);

  // Implements the SetPolygonOffset function for GL.
  virtual void SetPolygonOffset(float slope_factor, float units);

  // Implements the SetPolygonRaster function for GL.
  virtual void SetPolygonRaster(PolygonMode fill_mode,
                                FaceCullMode cull_mode);

  // Implements the SetAlphaTest function for GL.
  virtual void SetAlphaTest(bool enable,
                            float reference,
                            Comparison comp);

  // Implements the SetDepthTest function for GL.
  virtual void SetDepthTest(bool enable,
                            bool write_enable,
                            Comparison comp);

  // Implements the SetStencilTest function for GL.
  virtual void SetStencilTest(bool enable,
                              bool separate_ccw,
                              unsigned int write_mask,
                              unsigned int compare_mask,
                              unsigned int ref,
                              Uint32 func_ops);

  // Implements the SetColorWritefunction for GL.
  virtual void SetColorWrite(bool red,
                             bool green,
                             bool blue,
                             bool alpha,
                             bool dither);

  // Implements the SetBlending function for GL.
  virtual void SetBlending(bool enable,
                           bool separate_alpha,
                           BlendEq color_eq,
                           BlendFunc color_src_func,
                           BlendFunc color_dst_func,
                           BlendEq alpha_eq,
                           BlendFunc alpha_src_func,
                           BlendFunc alpha_dst_func);

  // Implements the SetBlendingColor function for GL.
  virtual void SetBlendingColor(const RGBA &color);

  // Implements the CreateRenderSurface function for GL.
  virtual ParseError CreateRenderSurface(ResourceId id,
                                         unsigned int width,
                                         unsigned int height,
                                         unsigned int mip_level,
                                         unsigned int side,
                                         ResourceId texture_id);

  // Implements the DestroyRenderSurface function for GL.
  virtual ParseError DestroyRenderSurface(ResourceId id);

  // Implements the CreateDepthSurface function for GL.
  virtual ParseError CreateDepthSurface(ResourceId id,
                                        unsigned int width,
                                        unsigned int height);

  // Implements the DestroyDepthSurface function for GL.
  virtual ParseError DestroyDepthSurface(ResourceId id);

  // Implements the SetRenderSurface function for GL.
  virtual ParseError SetRenderSurface(ResourceId render_surface_id,
                                      ResourceId depth_stencil_id);

  // Implements the SetBackSurfaces function for GL.
  virtual void SetBackSurfaces();

  // Gets a vertex buffer by resource ID.
  VertexBufferGL *GetVertexBuffer(ResourceId id) {
    return vertex_buffers_.Get(id);
  }

  // Gets a texture by resource ID.
  TextureGL *GetTexture(ResourceId id) {
    return textures_.Get(id);
  }

  // Gets a sampler by resource ID.
  SamplerGL *GetSampler(ResourceId id) {
    return samplers_.Get(id);
  }
  void set_anti_aliased(bool anti_aliased) { anti_aliased_ = anti_aliased; }

  bool anti_aliased() const { return anti_aliased_; }

  CGcontext cg_context() const { return cg_context_; }

  EffectGL *current_effect() const { return current_effect_; }
  // "Dirty" the current effect. This resets the vertex and fragment program,
  // and requires ValidateEffect() to be called before further draws occur.
  void DirtyEffect();
 private:
  bool InitPlatformSpecific();
  bool InitCommon();
  // Validates the current vertex struct to GL, setting the vertex attributes.
  bool ValidateStreams();
  // Validates the current effect to GL. This sets the vertex and fragment
  // programs, and updates parameters if needed.
  bool ValidateEffect();

#if defined(OS_LINUX)
  XWindowWrapper *window_;
#elif defined(OS_WIN)
  // Handle to the GL device.
  HWND hwnd_;
  HDC device_context_;
  HGLRC gl_context_;
#endif

  CGcontext cg_context_;

  bool anti_aliased_;
  ResourceId current_vertex_struct_;
  bool validate_streams_;
  unsigned int max_vertices_;
  ResourceId current_effect_id_;
  bool validate_effect_;
  EffectGL *current_effect_;
  ResourceId current_surface_id_;
  ResourceId current_depth_surface_id_;
  GLuint render_surface_framebuffer_;

  ResourceMap<VertexBufferGL> vertex_buffers_;
  ResourceMap<IndexBufferGL> index_buffers_;
  ResourceMap<VertexStructGL> vertex_structs_;
  ResourceMap<EffectGL> effects_;
  ResourceMap<EffectParamGL> effect_params_;
  ResourceMap<TextureGL> textures_;
  ResourceMap<SamplerGL> samplers_;
  ResourceMap<RenderSurfaceGL> render_surfaces_;
  ResourceMap<RenderDepthStencilSurfaceGL> depth_surfaces_;
};

}  // namespace o3d
}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_GAPI_GL_H_
