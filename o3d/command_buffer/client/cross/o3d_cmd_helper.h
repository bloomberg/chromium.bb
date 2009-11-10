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


// This file contains the o3d command buffer helper class.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CROSS_O3D_CMD_HELPER_H_
#define GPU_COMMAND_BUFFER_CLIENT_CROSS_O3D_CMD_HELPER_H_

#include "command_buffer/common/cross/logging.h"
#include "command_buffer/common/cross/constants.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"
#include "command_buffer/common/cross/o3d_cmd_format.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"

namespace command_buffer {

// A helper for O3D command buffers.
class O3DCmdHelper : public CommandBufferHelper {
 public:
  O3DCmdHelper(
      NPP npp,
      const gpu_plugin::NPObjectPointer<gpu_plugin::CommandBuffer>&
          command_buffer)
      : CommandBufferHelper(npp, command_buffer) {
  }
  virtual ~O3DCmdHelper() {
  }

  // ------------------ Individual commands ----------------------

  void BeginFrame() {
    o3d::BeginFrame& cmd = GetCmdSpace<o3d::BeginFrame>();
    cmd.Init();
  }


  void EndFrame() {
    o3d::EndFrame& cmd = GetCmdSpace<o3d::EndFrame>();
    cmd.Init();
  }

  void Clear(
      uint32 buffers,
      float red, float green, float blue, float alpha,
      float depth, uint32 stencil) {
    o3d::Clear& cmd = GetCmdSpace<o3d::Clear>();
    cmd.Init(buffers, red, green, blue, alpha, depth, stencil);
  }

  void SetViewport(
      uint32 left,
      uint32 top,
      uint32 width,
      uint32 height,
      float z_min,
      float z_max) {
    o3d::SetViewport& cmd = GetCmdSpace<o3d::SetViewport>();
    cmd.Init(left, top, width, height, z_min, z_max);
  }

  void CreateVertexBuffer(
      ResourceId vertex_buffer_id, uint32 size, vertex_buffer::Flags flags) {
    o3d::CreateVertexBuffer& cmd = GetCmdSpace<o3d::CreateVertexBuffer>();
    cmd.Init(vertex_buffer_id, size, flags);
  }

  void DestroyVertexBuffer(ResourceId vertex_buffer_id) {
    o3d::DestroyVertexBuffer& cmd = GetCmdSpace<o3d::DestroyVertexBuffer>();
    cmd.Init(vertex_buffer_id);
  }

  void SetVertexBufferDataImmediate(
      ResourceId vertex_buffer_id, uint32 offset,
      const void* data, uint32 size) {
    o3d::SetVertexBufferDataImmediate& cmd =
        GetImmediateCmdSpace<o3d::SetVertexBufferDataImmediate>(size);
    cmd.Init(vertex_buffer_id, offset, data, size);
  }

  void SetVertexBufferData(
      ResourceId vertex_buffer_id, uint32 offset, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    o3d::SetVertexBufferData& cmd =
        GetCmdSpace<o3d::SetVertexBufferData>();
    cmd.Init(vertex_buffer_id, offset, size,
             shared_memory_id, shared_memory_offset);
  }

  void GetVertexBufferData(
      ResourceId vertex_buffer_id, uint32 offset, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    o3d::GetVertexBufferData& cmd =
        GetCmdSpace<o3d::GetVertexBufferData>();
    cmd.Init(vertex_buffer_id, offset, size,
             shared_memory_id, shared_memory_offset);
  }

  void CreateIndexBuffer(
      ResourceId index_buffer_id, uint32 size, index_buffer::Flags flags) {
    o3d::CreateIndexBuffer& cmd =
        GetCmdSpace<o3d::CreateIndexBuffer>();
    cmd.Init(index_buffer_id, size, flags);
  }

  void DestroyIndexBuffer(ResourceId index_buffer_id) {
    o3d::DestroyIndexBuffer& cmd = GetCmdSpace<o3d::DestroyIndexBuffer>();
    cmd.Init(index_buffer_id);
  }

  void SetIndexBufferDataImmediate(
      ResourceId index_buffer_id, uint32 offset,
      const void* data, uint32 size) {
    o3d::SetIndexBufferDataImmediate& cmd =
        GetImmediateCmdSpace<o3d::SetIndexBufferDataImmediate>(size);
    cmd.Init(index_buffer_id, offset, data, size);
  }

  void SetIndexBufferData(
      ResourceId index_buffer_id, uint32 offset, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    o3d::SetIndexBufferData& cmd = GetCmdSpace<o3d::SetIndexBufferData>();
    cmd.Init(index_buffer_id, offset, size,
             shared_memory_id, shared_memory_offset);
  }

  void GetIndexBufferData(
      ResourceId index_buffer_id, uint32 offset, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    o3d::GetIndexBufferData& cmd = GetCmdSpace<o3d::GetIndexBufferData>();
    cmd.Init(index_buffer_id, offset, size,
             shared_memory_id, shared_memory_offset);
  }

  void CreateVertexStruct(ResourceId vertex_struct_id, uint32 input_count) {
    o3d::CreateVertexStruct& cmd = GetCmdSpace<o3d::CreateVertexStruct>();
    cmd.Init(vertex_struct_id, input_count);
  }

  void DestroyVertexStruct(ResourceId vertex_struct_id) {
    o3d::DestroyVertexStruct& cmd = GetCmdSpace<o3d::DestroyVertexStruct>();
    cmd.Init(vertex_struct_id);
  }

  void SetVertexInput(
      ResourceId vertex_struct_id,
      uint32 input_index,
      ResourceId vertex_buffer_id,
      uint32 offset,
      vertex_struct::Semantic semantic,
      uint32 semantic_index,
      vertex_struct::Type type,
      uint32 stride) {
    o3d::SetVertexInput& cmd = GetCmdSpace<o3d::SetVertexInput>();
    cmd.Init(
        vertex_struct_id,
        input_index,
        vertex_buffer_id,
        offset,
        semantic,
        semantic_index,
        type,
        stride);
  }

  void SetVertexStruct(ResourceId vertex_struct_id) {
    o3d::SetVertexStruct& cmd = GetCmdSpace<o3d::SetVertexStruct>();
    cmd.Init(vertex_struct_id);
  }

  void Draw(o3d::PrimitiveType primitive_type, uint32 first, uint32 count) {
    o3d::Draw& cmd = GetCmdSpace<o3d::Draw>();
    cmd.Init(primitive_type, first, count);
  }

  void DrawIndexed(
      o3d::PrimitiveType primitive_type,
      ResourceId index_buffer_id,
      uint32 first,
      uint32 count,
      uint32 min_index,
      uint32 max_index) {
    o3d::DrawIndexed& cmd = GetCmdSpace<o3d::DrawIndexed>();
    cmd.Init(
        primitive_type,
        index_buffer_id,
        first,
        count,
        min_index,
        max_index);
  }

  void CreateEffect(
      ResourceId effect_id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    o3d::CreateEffect& cmd = GetCmdSpace<o3d::CreateEffect>();
    cmd.Init(effect_id, size, shared_memory_id, shared_memory_offset);
  }

  void CreateEffectImmediate(
      ResourceId effect_id, uint32 size, const void* data) {
    o3d::CreateEffectImmediate& cmd =
        GetImmediateCmdSpace<o3d::CreateEffectImmediate>(size);
    cmd.Init(effect_id, size, data);
  }

  void DestroyEffect(ResourceId effect_id) {
    o3d::DestroyEffect& cmd = GetCmdSpace<o3d::DestroyEffect>();
    cmd.Init(effect_id);
  }

  void SetEffect(ResourceId effect_id) {
    o3d::SetEffect& cmd = GetCmdSpace<o3d::SetEffect>();
    cmd.Init(effect_id);
  }

  void GetParamCount(
      ResourceId effect_id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    o3d::GetParamCount& cmd = GetCmdSpace<o3d::GetParamCount>();
    cmd.Init(effect_id, size, shared_memory_id, shared_memory_offset);
  }

  void CreateParam(ResourceId param_id, ResourceId effect_id, uint32 index) {
    o3d::CreateParam& cmd = GetCmdSpace<o3d::CreateParam>();
    cmd.Init(param_id, effect_id, index);
  }

  void CreateParamByName(
      ResourceId param_id, ResourceId effect_id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    o3d::CreateParamByName& cmd = GetCmdSpace<o3d::CreateParamByName>();
    cmd.Init(param_id, effect_id, size, shared_memory_id, shared_memory_offset);
  }

  void CreateParamByNameImmediate(
      ResourceId param_id, ResourceId effect_id,
      uint32 size, const void* data) {
    o3d::CreateParamByNameImmediate& cmd =
        GetImmediateCmdSpace<o3d::CreateParamByNameImmediate>(size);
    cmd.Init(param_id, effect_id, size, data);
  }

  void DestroyParam(ResourceId param_id) {
    o3d::DestroyParam& cmd = GetCmdSpace<o3d::DestroyParam>();
    cmd.Init(param_id);
  }

  void SetParamData(
      ResourceId param_id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    o3d::SetParamData& cmd = GetCmdSpace<o3d::SetParamData>();
    cmd.Init(param_id, size, shared_memory_id, shared_memory_offset);
  }

  void SetParamDataImmediate(
      ResourceId param_id, uint32 size, const void* data) {
    o3d::SetParamDataImmediate& cmd =
        GetImmediateCmdSpace<o3d::SetParamDataImmediate>(size);
    cmd.Init(param_id, size, data);
  }

  void GetParamDesc(
      ResourceId param_id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    o3d::GetParamDesc& cmd = GetCmdSpace<o3d::GetParamDesc>();
    cmd.Init(param_id, size, shared_memory_id, shared_memory_offset);
  }

  void GetStreamCount(
      ResourceId effect_id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    o3d::GetStreamCount& cmd = GetCmdSpace<o3d::GetStreamCount>();
    cmd.Init(effect_id, size, shared_memory_id, shared_memory_offset);
  }

  void GetStreamDesc(
      ResourceId effect_id, uint32 index, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    o3d::GetStreamDesc& cmd = GetCmdSpace<o3d::GetStreamDesc>();
    cmd.Init(effect_id, index, size, shared_memory_id, shared_memory_offset);
  }

  void DestroyTexture(ResourceId texture_id) {
    o3d::DestroyTexture& cmd = GetCmdSpace<o3d::DestroyTexture>();
    cmd.Init(texture_id);
  }

  void CreateTexture2d(
      ResourceId texture_id,
      uint32 width, uint32 height,
      uint32 levels, texture::Format format,
      bool enable_render_surfaces) {
    o3d::CreateTexture2d& cmd = GetCmdSpace<o3d::CreateTexture2d>();
    cmd.Init(texture_id,
             width, height, levels, format,
             enable_render_surfaces);
  }

  void CreateTexture3d(
      ResourceId texture_id,
      uint32 width, uint32 height, uint32 depth,
      uint32 levels, texture::Format format,
      bool enable_render_surfaces) {
    o3d::CreateTexture3d& cmd = GetCmdSpace<o3d::CreateTexture3d>();
    cmd.Init(texture_id,
             width, height, depth,
             levels, format,
             enable_render_surfaces);
  }

  void CreateTextureCube(
      ResourceId texture_id,
      uint32 edge_length, uint32 levels, texture::Format format,
      bool enable_render_surfaces) {
    o3d::CreateTextureCube& cmd = GetCmdSpace<o3d::CreateTextureCube>();
    cmd.Init(texture_id,
             edge_length, levels, format,
             enable_render_surfaces);
  }

  void SetTextureData(
      ResourceId texture_id,
      uint32 x,
      uint32 y,
      uint32 z,
      uint32 width,
      uint32 height,
      uint32 depth,
      uint32 level,
      texture::Face face,
      uint32 row_pitch,
      uint32 slice_pitch,
      uint32 size,
      uint32 shared_memory_id,
      uint32 shared_memory_offset) {
    o3d::SetTextureData& cmd = GetCmdSpace<o3d::SetTextureData>();
    cmd.Init(
        texture_id,
        x,
        y,
        z,
        width,
        height,
        depth,
        level,
        face,
        row_pitch,
        slice_pitch,
        size,
        shared_memory_id,
        shared_memory_offset);
  }

  void SetTextureDataImmediate(
      ResourceId texture_id,
      uint32 x,
      uint32 y,
      uint32 z,
      uint32 width,
      uint32 height,
      uint32 depth,
      uint32 level,
      texture::Face face,
      uint32 row_pitch,
      uint32 slice_pitch,
      uint32 size,
      const void* data) {
    o3d::SetTextureDataImmediate& cmd =
        GetImmediateCmdSpace<o3d::SetTextureDataImmediate>(size);
    cmd.Init(
        texture_id,
        x,
        y,
        z,
        width,
        height,
        depth,
        level,
        face,
        row_pitch,
        slice_pitch,
        size,
        data);
  }

  void GetTextureData(
      ResourceId texture_id,
      uint32 x,
      uint32 y,
      uint32 z,
      uint32 width,
      uint32 height,
      uint32 depth,
      uint32 level,
      texture::Face face,
      uint32 row_pitch,
      uint32 slice_pitch,
      uint32 size,
      uint32 shared_memory_id,
      uint32 shared_memory_offset) {
    o3d::GetTextureData& cmd = GetCmdSpace<o3d::GetTextureData>();
    cmd.Init(
        texture_id,
        x,
        y,
        z,
        width,
        height,
        depth,
        level,
        face,
        row_pitch,
        slice_pitch,
        size,
        shared_memory_id,
        shared_memory_offset);
  }

  void CreateSampler(ResourceId sampler_id) {
    o3d::CreateSampler& cmd = GetCmdSpace<o3d::CreateSampler>();
    cmd.Init(sampler_id);
  }

  void DestroySampler(ResourceId sampler_id) {
    o3d::DestroySampler& cmd = GetCmdSpace<o3d::DestroySampler>();
    cmd.Init(sampler_id);
  }

  void SetSamplerStates(
      ResourceId sampler_id,
      sampler::AddressingMode address_u_value,
      sampler::AddressingMode address_v_value,
      sampler::AddressingMode address_w_value,
      sampler::FilteringMode mag_filter_value,
      sampler::FilteringMode min_filter_value,
      sampler::FilteringMode mip_filter_value,
      uint8 max_anisotropy) {
    o3d::SetSamplerStates& cmd = GetCmdSpace<o3d::SetSamplerStates>();
    cmd.Init(
        sampler_id,
        address_u_value,
        address_v_value,
        address_w_value,
        mag_filter_value,
        min_filter_value,
        mip_filter_value,
        max_anisotropy);
  }

  void SetSamplerBorderColor(
      ResourceId sampler_id,
      float red, float green, float blue, float alpha) {
    o3d::SetSamplerBorderColor& cmd =
        GetCmdSpace<o3d::SetSamplerBorderColor>();
    cmd.Init(sampler_id, red, green, blue, alpha);
  }

  void SetSamplerTexture(ResourceId sampler_id, ResourceId texture_id) {
    o3d::SetSamplerTexture& cmd = GetCmdSpace<o3d::SetSamplerTexture>();
    cmd.Init(sampler_id, texture_id);
  }

  void SetScissor(
      uint32 x,
      uint32 y,
      uint32 width,
      uint32 height,
      bool enable) {
    o3d::SetScissor& cmd = GetCmdSpace<o3d::SetScissor>();
    cmd.Init(
        x,
        y,
        width,
        height,
        enable);
  }

  void SetPolygonOffset(float slope_factor, float units) {
    o3d::SetPolygonOffset& cmd = GetCmdSpace<o3d::SetPolygonOffset>();
    cmd.Init(slope_factor, units);
  }

  void SetPointLineRaster(
      bool line_smooth_enable, bool point_sprite_enable, float point_size) {
    o3d::SetPointLineRaster& cmd = GetCmdSpace<o3d::SetPointLineRaster>();
    cmd.Init(line_smooth_enable, point_sprite_enable, point_size);
  }

  void SetPolygonRaster(o3d::PolygonMode fill_mode,
                        o3d::FaceCullMode cull_mode) {
    o3d::SetPolygonRaster& cmd = GetCmdSpace<o3d::SetPolygonRaster>();
    cmd.Init(fill_mode, cull_mode);
  }

  void SetAlphaTest(o3d::Comparison func, bool enable, float value) {
    o3d::SetAlphaTest& cmd = GetCmdSpace<o3d::SetAlphaTest>();
    cmd.Init(func, enable, value);
  }

  void SetDepthTest(o3d::Comparison func, bool write_enable, bool enable) {
    o3d::SetDepthTest& cmd = GetCmdSpace<o3d::SetDepthTest>();
    cmd.Init(func, write_enable, enable);
  }

  void SetStencilTest(
      uint8 write_mask,
      uint8 compare_mask,
      uint8 reference_value,
      bool separate_ccw,
      bool enable,
      o3d::Comparison cw_func,
      o3d::StencilOp cw_pass_op,
      o3d::StencilOp cw_fail_op,
      o3d::StencilOp cw_z_fail_op,
      o3d::Comparison ccw_func,
      o3d::StencilOp ccw_pass_op,
      o3d::StencilOp ccw_fail_op,
      o3d::StencilOp ccw_z_fail_op) {
    o3d::SetStencilTest& cmd = GetCmdSpace<o3d::SetStencilTest>();
    cmd.Init(
        write_mask,
        compare_mask,
        reference_value,
        separate_ccw,
        enable,
        cw_func,
        cw_pass_op,
        cw_fail_op,
        cw_z_fail_op,
        ccw_func,
        ccw_pass_op,
        ccw_fail_op,
        ccw_z_fail_op);
  }

  void SetColorWrite(uint8 mask, bool dither_enable) {
    o3d::SetColorWrite& cmd = GetCmdSpace<o3d::SetColorWrite>();
    cmd.Init(mask, dither_enable);
  }

  void SetBlending(
      o3d::BlendFunc color_src_func,
      o3d::BlendFunc color_dst_func,
      o3d::BlendEq color_eq,
      o3d::BlendFunc alpha_src_func,
      o3d::BlendFunc alpha_dst_func,
      o3d::BlendEq alpha_eq,
      bool separate_alpha,
      bool enable) {
    o3d::SetBlending& cmd = GetCmdSpace<o3d::SetBlending>();
    cmd.Init(
        color_src_func,
        color_dst_func,
        color_eq,
        alpha_src_func,
        alpha_dst_func,
        alpha_eq,
        separate_alpha,
        enable);
  }

  void SetBlendingColor(float red, float green, float blue, float alpha) {
    o3d::SetBlendingColor& cmd = GetCmdSpace<o3d::SetBlendingColor>();
    cmd.Init(red, green, blue, alpha);
  }

  void CreateRenderSurface(
      ResourceId render_surface_id, ResourceId texture_id,
      uint32 width, uint32 height,
      uint32 level, uint32 side) {
    o3d::CreateRenderSurface& cmd = GetCmdSpace<o3d::CreateRenderSurface>();
    cmd.Init(render_surface_id, texture_id, width, height, level, side);
  }

  void DestroyRenderSurface(ResourceId render_surface_id) {
    o3d::DestroyRenderSurface& cmd =
        GetCmdSpace<o3d::DestroyRenderSurface>();
    cmd.Init(render_surface_id);
  }

  void CreateDepthSurface(
      ResourceId depth_surface_id, uint32 width, uint32 height) {
    o3d::CreateDepthSurface& cmd = GetCmdSpace<o3d::CreateDepthSurface>();
    cmd.Init(depth_surface_id, width, height);
  }

  void DestroyDepthSurface(ResourceId depth_surface_id) {
    o3d::DestroyDepthSurface& cmd = GetCmdSpace<o3d::DestroyDepthSurface>();
    cmd.Init(depth_surface_id);
  }

  void SetRenderSurface(
      ResourceId render_surface_id, ResourceId depth_surface_id) {
    o3d::SetRenderSurface& cmd = GetCmdSpace<o3d::SetRenderSurface>();
    cmd.Init(render_surface_id, depth_surface_id);
  }

  void SetBackSurfaces() {
    o3d::SetBackSurfaces& cmd = GetCmdSpace<o3d::SetBackSurfaces>();
    cmd.Init();
  }
};

}  // namespace command_buffer

#endif  // GPU_COMMAND_BUFFER_CLIENT_CROSS_O3D_CMD_HELPER_H_

