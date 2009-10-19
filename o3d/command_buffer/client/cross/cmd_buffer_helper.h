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


// This file contains the command buffer helper class.

#ifndef O3D_COMMAND_BUFFER_CLIENT_CROSS_CMD_BUFFER_HELPER_H_
#define O3D_COMMAND_BUFFER_CLIENT_CROSS_CMD_BUFFER_HELPER_H_

#include "command_buffer/common/cross/logging.h"
#include "command_buffer/common/cross/constants.h"
#include "command_buffer/common/cross/cmd_buffer_format.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"

namespace o3d {
namespace command_buffer {

// Command buffer helper class. This class simplifies ring buffer management:
// it will allocate the buffer, give it to the buffer interface, and let the
// user add commands to it, while taking care of the synchronization (put and
// get). It also provides a way to ensure commands have been executed, through
// the token mechanism:
//
// helper.AddCommand(...);
// helper.AddCommand(...);
// int32 token = helper.InsertToken();
// helper.AddCommand(...);
// helper.AddCommand(...);
// [...]
//
// helper.WaitForToken(token);  // this doesn't return until the first two
//                              // commands have been executed.
class CommandBufferHelper {
 public:
  explicit CommandBufferHelper(
      NPP npp,
      const gpu_plugin::NPObjectPointer<NPObject>& command_buffer);
  ~CommandBufferHelper();

  bool Initialize();

  // Flushes the commands, setting the put pointer to let the buffer interface
  // know that new commands have been added. After a flush returns, the command
  // buffer service is aware of all pending commands and it is guaranteed to
  // have made some progress in processing them. Returns whether the flush was
  // successful. The flush will fail if the command buffer service has
  // disconnected.
  bool Flush();

  // Waits until all the commands have been executed. Returns whether it
  // was successful. The function will fail if the command buffer service has
  // disconnected.
  bool Finish();

  // Waits until a given number of available entries are available.
  // Parameters:
  //   count: number of entries needed. This value must be at most
  //     the size of the buffer minus one.
  void WaitForAvailableEntries(int32 count);

  // Adds a command data to the command buffer. This may wait until sufficient
  // space is available.
  // Parameters:
  //   entries: The command entries to add.
  //   count: The number of entries.
  void AddCommandData(const CommandBufferEntry* entries, int32 count) {
    WaitForAvailableEntries(count);
    for (; count > 0; --count) {
      entries_[put_++] = *entries++;
    }
    DCHECK_LE(put_, entry_count_);
    if (put_ == entry_count_) put_ = 0;
  }

  // A typed version of AddCommandData.
  template <typename T>
  void AddTypedCmdData(const T& cmd) {
    AddCommandData(reinterpret_cast<const CommandBufferEntry*>(&cmd),
                   ComputeNumEntries(sizeof(cmd)));
  }

  // Adds a command to the command buffer. This may wait until sufficient space
  // is available.
  // Parameters:
  //   command: the command index.
  //   arg_count: the number of arguments for the command.
  //   args: the arguments for the command (these are copied before the
  //     function returns).
  void AddCommand(int32 command,
                  int32 arg_count,
                  const CommandBufferEntry *args) {
    CommandHeader header;
    header.size = arg_count + 1;
    header.command = command;
    WaitForAvailableEntries(header.size);
    entries_[put_++].value_header = header;
    for (int i = 0; i < arg_count; ++i) {
      entries_[put_++] = args[i];
    }
    DCHECK_LE(put_, entry_count_);
    if (put_ == entry_count_) put_ = 0;
  }

  // Inserts a new token into the command buffer. This token either has a value
  // different from previously inserted tokens, or ensures that previously
  // inserted tokens with that value have already passed through the command
  // stream.
  // Returns:
  //   the value of the new token or -1 if the command buffer reader has
  //   shutdown.
  int32 InsertToken();

  // Waits until the token of a particular value has passed through the command
  // stream (i.e. commands inserted before that token have been executed).
  // NOTE: This will call Flush if it needs to block.
  // Parameters:
  //   the value of the token to wait for.
  void WaitForToken(int32 token);

  // Waits for a certain amount of space to be available. Returns address
  // of space.
  CommandBufferEntry* GetSpace(uint32 entries);

  // Typed version of GetSpace. Gets enough room for the given type and returns
  // a reference to it.
  template <typename T>
  T& GetCmdSpace() {
    COMPILE_ASSERT(T::kArgFlags == cmd::kFixed, Cmd_kArgFlags_not_kFixed);
    uint32 space_needed = ComputeNumEntries(sizeof(T));
    void* data = GetSpace(space_needed);
    return *reinterpret_cast<T*>(data);
  }

  // Typed version of GetSpace for immediate commands.
  template <typename T>
  T& GetImmediateCmdSpace(size_t space) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
    uint32 space_needed = ComputeNumEntries(sizeof(T) + space);
    void* data = GetSpace(space_needed);
    return *reinterpret_cast<T*>(data);
  }

  parse_error::ParseError GetParseError();

  // ------------------ Individual commands ----------------------

  void Noop(uint32 skip_count) {
    cmd::Noop& cmd = GetImmediateCmdSpace<cmd::Noop>(
        skip_count * sizeof(CommandBufferEntry));
    cmd.Init(skip_count);
  }


  void SetToken(uint32 token) {
    cmd::SetToken& cmd = GetCmdSpace<cmd::SetToken>();
    cmd.Init(token);
  }


  void BeginFrame() {
    cmd::BeginFrame& cmd = GetCmdSpace<cmd::BeginFrame>();
    cmd.Init();
  }


  void EndFrame() {
    cmd::EndFrame& cmd = GetCmdSpace<cmd::EndFrame>();
    cmd.Init();
  }

  void Clear(
      uint32 buffers,
      float red, float green, float blue, float alpha,
      float depth, uint32 stencil) {
    cmd::Clear& cmd = GetCmdSpace<cmd::Clear>();
    cmd.Init(buffers, red, green, blue, alpha, depth, stencil);
  }

  void SetViewport(
      uint32 left,
      uint32 top,
      uint32 width,
      uint32 height,
      float z_min,
      float z_max) {
    cmd::SetViewport& cmd = GetCmdSpace<cmd::SetViewport>();
    cmd.Init(left, top, width, height, z_min, z_max);
  }

  void CreateVertexBuffer(
      ResourceId vertex_buffer_id, uint32 size, vertex_buffer::Flags flags) {
    cmd::CreateVertexBuffer& cmd = GetCmdSpace<cmd::CreateVertexBuffer>();
    cmd.Init(vertex_buffer_id, size, flags);
  }

  void DestroyVertexBuffer(ResourceId vertex_buffer_id) {
    cmd::DestroyVertexBuffer& cmd = GetCmdSpace<cmd::DestroyVertexBuffer>();
    cmd.Init(vertex_buffer_id);
  }

  void SetVertexBufferDataImmediate(
      ResourceId vertex_buffer_id, uint32 offset,
      const void* data, uint32 size) {
    cmd::SetVertexBufferDataImmediate& cmd =
        GetImmediateCmdSpace<cmd::SetVertexBufferDataImmediate>(size);
    cmd.Init(vertex_buffer_id, offset, data, size);
  }

  void SetVertexBufferData(
      ResourceId vertex_buffer_id, uint32 offset, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::SetVertexBufferData& cmd =
        GetCmdSpace<cmd::SetVertexBufferData>();
    cmd.Init(vertex_buffer_id, offset, size,
             shared_memory_id, shared_memory_offset);
  }

  void GetVertexBufferData(
      ResourceId vertex_buffer_id, uint32 offset, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::GetVertexBufferData& cmd =
        GetCmdSpace<cmd::GetVertexBufferData>();
    cmd.Init(vertex_buffer_id, offset, size,
             shared_memory_id, shared_memory_offset);
  }

  void CreateIndexBuffer(
      ResourceId index_buffer_id, uint32 size, index_buffer::Flags flags) {
    cmd::CreateIndexBuffer& cmd =
        GetCmdSpace<cmd::CreateIndexBuffer>();
    cmd.Init(index_buffer_id, size, flags);
  }

  void DestroyIndexBuffer(ResourceId index_buffer_id) {
    cmd::DestroyIndexBuffer& cmd = GetCmdSpace<cmd::DestroyIndexBuffer>();
    cmd.Init(index_buffer_id);
  }

  void SetIndexBufferDataImmediate(
      ResourceId index_buffer_id, uint32 offset,
      const void* data, uint32 size) {
    cmd::SetIndexBufferDataImmediate& cmd =
        GetImmediateCmdSpace<cmd::SetIndexBufferDataImmediate>(size);
    cmd.Init(index_buffer_id, offset, data, size);
  }

  void SetIndexBufferData(
      ResourceId index_buffer_id, uint32 offset, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::SetIndexBufferData& cmd = GetCmdSpace<cmd::SetIndexBufferData>();
    cmd.Init(index_buffer_id, offset, size,
             shared_memory_id, shared_memory_offset);
  }

  void GetIndexBufferData(
      ResourceId index_buffer_id, uint32 offset, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::GetIndexBufferData& cmd = GetCmdSpace<cmd::GetIndexBufferData>();
    cmd.Init(index_buffer_id, offset, size,
             shared_memory_id, shared_memory_offset);
  }

  void CreateVertexStruct(ResourceId vertex_struct_id, uint32 input_count) {
    cmd::CreateVertexStruct& cmd = GetCmdSpace<cmd::CreateVertexStruct>();
    cmd.Init(vertex_struct_id, input_count);
  }

  void DestroyVertexStruct(ResourceId vertex_struct_id) {
    cmd::DestroyVertexStruct& cmd = GetCmdSpace<cmd::DestroyVertexStruct>();
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
    cmd::SetVertexInput& cmd = GetCmdSpace<cmd::SetVertexInput>();
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
    cmd::SetVertexStruct& cmd = GetCmdSpace<cmd::SetVertexStruct>();
    cmd.Init(vertex_struct_id);
  }

  void Draw(PrimitiveType primitive_type, uint32 first, uint32 count) {
    cmd::Draw& cmd = GetCmdSpace<cmd::Draw>();
    cmd.Init(primitive_type, first, count);
  }

  void DrawIndexed(
      PrimitiveType primitive_type,
      ResourceId index_buffer_id,
      uint32 first,
      uint32 count,
      uint32 min_index,
      uint32 max_index) {
    cmd::DrawIndexed& cmd = GetCmdSpace<cmd::DrawIndexed>();
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
    cmd::CreateEffect& cmd = GetCmdSpace<cmd::CreateEffect>();
    cmd.Init(effect_id, size, shared_memory_id, shared_memory_offset);
  }

  void CreateEffectImmediate(
      ResourceId effect_id, uint32 size, const void* data) {
    cmd::CreateEffectImmediate& cmd =
        GetImmediateCmdSpace<cmd::CreateEffectImmediate>(size);
    cmd.Init(effect_id, size, data);
  }

  void DestroyEffect(ResourceId effect_id) {
    cmd::DestroyEffect& cmd = GetCmdSpace<cmd::DestroyEffect>();
    cmd.Init(effect_id);
  }

  void SetEffect(ResourceId effect_id) {
    cmd::SetEffect& cmd = GetCmdSpace<cmd::SetEffect>();
    cmd.Init(effect_id);
  }

  void GetParamCount(
      ResourceId effect_id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::GetParamCount& cmd = GetCmdSpace<cmd::GetParamCount>();
    cmd.Init(effect_id, size, shared_memory_id, shared_memory_offset);
  }

  void CreateParam(ResourceId param_id, ResourceId effect_id, uint32 index) {
    cmd::CreateParam& cmd = GetCmdSpace<cmd::CreateParam>();
    cmd.Init(param_id, effect_id, index);
  }

  void CreateParamByName(
      ResourceId param_id, ResourceId effect_id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::CreateParamByName& cmd = GetCmdSpace<cmd::CreateParamByName>();
    cmd.Init(param_id, effect_id, size, shared_memory_id, shared_memory_offset);
  }

  void CreateParamByNameImmediate(
      ResourceId param_id, ResourceId effect_id,
      uint32 size, const void* data) {
    cmd::CreateParamByNameImmediate& cmd =
        GetImmediateCmdSpace<cmd::CreateParamByNameImmediate>(size);
    cmd.Init(param_id, effect_id, size, data);
  }

  void DestroyParam(ResourceId param_id) {
    cmd::DestroyParam& cmd = GetCmdSpace<cmd::DestroyParam>();
    cmd.Init(param_id);
  }

  void SetParamData(
      ResourceId param_id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::SetParamData& cmd = GetCmdSpace<cmd::SetParamData>();
    cmd.Init(param_id, size, shared_memory_id, shared_memory_offset);
  }

  void SetParamDataImmediate(
      ResourceId param_id, uint32 size, const void* data) {
    cmd::SetParamDataImmediate& cmd =
        GetImmediateCmdSpace<cmd::SetParamDataImmediate>(size);
    cmd.Init(param_id, size, data);
  }

  void GetParamDesc(
      ResourceId param_id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::GetParamDesc& cmd = GetCmdSpace<cmd::GetParamDesc>();
    cmd.Init(param_id, size, shared_memory_id, shared_memory_offset);
  }

  void GetStreamCount(
      ResourceId effect_id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::GetStreamCount& cmd = GetCmdSpace<cmd::GetStreamCount>();
    cmd.Init(effect_id, size, shared_memory_id, shared_memory_offset);
  }

  void GetStreamDesc(
      ResourceId effect_id, uint32 index, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::GetStreamDesc& cmd = GetCmdSpace<cmd::GetStreamDesc>();
    cmd.Init(effect_id, index, size, shared_memory_id, shared_memory_offset);
  }

  void DestroyTexture(ResourceId texture_id) {
    cmd::DestroyTexture& cmd = GetCmdSpace<cmd::DestroyTexture>();
    cmd.Init(texture_id);
  }

  void CreateTexture2d(
      ResourceId texture_id,
      uint32 width, uint32 height,
      uint32 levels, texture::Format format,
      bool enable_render_surfaces) {
    cmd::CreateTexture2d& cmd = GetCmdSpace<cmd::CreateTexture2d>();
    cmd.Init(texture_id,
             width, height, levels, format,
             enable_render_surfaces);
  }

  void CreateTexture3d(
      ResourceId texture_id,
      uint32 width, uint32 height, uint32 depth,
      uint32 levels, texture::Format format,
      bool enable_render_surfaces) {
    cmd::CreateTexture3d& cmd = GetCmdSpace<cmd::CreateTexture3d>();
    cmd.Init(texture_id,
             width, height, depth,
             levels, format,
             enable_render_surfaces);
  }

  void CreateTextureCube(
      ResourceId texture_id,
      uint32 edge_length, uint32 levels, texture::Format format,
      bool enable_render_surfaces) {
    cmd::CreateTextureCube& cmd = GetCmdSpace<cmd::CreateTextureCube>();
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
    cmd::SetTextureData& cmd = GetCmdSpace<cmd::SetTextureData>();
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
    cmd::SetTextureDataImmediate& cmd =
        GetImmediateCmdSpace<cmd::SetTextureDataImmediate>(size);
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
    cmd::GetTextureData& cmd = GetCmdSpace<cmd::GetTextureData>();
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
    cmd::CreateSampler& cmd = GetCmdSpace<cmd::CreateSampler>();
    cmd.Init(sampler_id);
  }

  void DestroySampler(ResourceId sampler_id) {
    cmd::DestroySampler& cmd = GetCmdSpace<cmd::DestroySampler>();
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
    cmd::SetSamplerStates& cmd = GetCmdSpace<cmd::SetSamplerStates>();
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
    cmd::SetSamplerBorderColor& cmd =
        GetCmdSpace<cmd::SetSamplerBorderColor>();
    cmd.Init(sampler_id, red, green, blue, alpha);
  }

  void SetSamplerTexture(ResourceId sampler_id, ResourceId texture_id) {
    cmd::SetSamplerTexture& cmd = GetCmdSpace<cmd::SetSamplerTexture>();
    cmd.Init(sampler_id, texture_id);
  }

  void SetScissor(
      uint32 x,
      uint32 y,
      uint32 width,
      uint32 height,
      bool enable) {
    cmd::SetScissor& cmd = GetCmdSpace<cmd::SetScissor>();
    cmd.Init(
        x,
        y,
        width,
        height,
        enable);
  }

  void SetPolygonOffset(float slope_factor, float units) {
    cmd::SetPolygonOffset& cmd = GetCmdSpace<cmd::SetPolygonOffset>();
    cmd.Init(slope_factor, units);
  }

  void SetPointLineRaster(
      bool line_smooth_enable, bool point_sprite_enable, float point_size) {
    cmd::SetPointLineRaster& cmd = GetCmdSpace<cmd::SetPointLineRaster>();
    cmd.Init(line_smooth_enable, point_sprite_enable, point_size);
  }

  void SetPolygonRaster(PolygonMode fill_mode, FaceCullMode cull_mode) {
    cmd::SetPolygonRaster& cmd = GetCmdSpace<cmd::SetPolygonRaster>();
    cmd.Init(fill_mode, cull_mode);
  }

  void SetAlphaTest(Comparison func, bool enable, float value) {
    cmd::SetAlphaTest& cmd = GetCmdSpace<cmd::SetAlphaTest>();
    cmd.Init(func, enable, value);
  }

  void SetDepthTest(Comparison func, bool write_enable, bool enable) {
    cmd::SetDepthTest& cmd = GetCmdSpace<cmd::SetDepthTest>();
    cmd.Init(func, write_enable, enable);
  }

  void SetStencilTest(
      uint8 write_mask,
      uint8 compare_mask,
      uint8 reference_value,
      bool separate_ccw,
      bool enable,
      Comparison cw_func,
      StencilOp cw_pass_op,
      StencilOp cw_fail_op,
      StencilOp cw_z_fail_op,
      Comparison ccw_func,
      StencilOp ccw_pass_op,
      StencilOp ccw_fail_op,
      StencilOp ccw_z_fail_op) {
    cmd::SetStencilTest& cmd = GetCmdSpace<cmd::SetStencilTest>();
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
    cmd::SetColorWrite& cmd = GetCmdSpace<cmd::SetColorWrite>();
    cmd.Init(mask, dither_enable);
  }

  void SetBlending(
      BlendFunc color_src_func,
      BlendFunc color_dst_func,
      BlendEq color_eq,
      BlendFunc alpha_src_func,
      BlendFunc alpha_dst_func,
      BlendEq alpha_eq,
      bool separate_alpha,
      bool enable) {
    cmd::SetBlending& cmd = GetCmdSpace<cmd::SetBlending>();
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
    cmd::SetBlendingColor& cmd = GetCmdSpace<cmd::SetBlendingColor>();
    cmd.Init(red, green, blue, alpha);
  }

  void CreateRenderSurface(
      ResourceId render_surface_id, ResourceId texture_id,
      uint32 width, uint32 height,
      uint32 level, uint32 side) {
    cmd::CreateRenderSurface& cmd = GetCmdSpace<cmd::CreateRenderSurface>();
    cmd.Init(render_surface_id, texture_id, width, height, level, side);
  }

  void DestroyRenderSurface(ResourceId render_surface_id) {
    cmd::DestroyRenderSurface& cmd =
        GetCmdSpace<cmd::DestroyRenderSurface>();
    cmd.Init(render_surface_id);
  }

  void CreateDepthSurface(
      ResourceId depth_surface_id, uint32 width, uint32 height) {
    cmd::CreateDepthSurface& cmd = GetCmdSpace<cmd::CreateDepthSurface>();
    cmd.Init(depth_surface_id, width, height);
  }

  void DestroyDepthSurface(ResourceId depth_surface_id) {
    cmd::DestroyDepthSurface& cmd = GetCmdSpace<cmd::DestroyDepthSurface>();
    cmd.Init(depth_surface_id);
  }

  void SetRenderSurface(
      ResourceId render_surface_id, ResourceId depth_surface_id) {
    cmd::SetRenderSurface& cmd = GetCmdSpace<cmd::SetRenderSurface>();
    cmd.Init(render_surface_id, depth_surface_id);
  }

  void SetBackSurfaces() {
    cmd::SetBackSurfaces& cmd = GetCmdSpace<cmd::SetBackSurfaces>();
    cmd.Init();
  }

 private:
  // Waits until get changes, updating the value of get_.
  void WaitForGetChange();

  // Returns the number of available entries (they may not be contiguous).
  int32 AvailableEntries() {
    return (get_ - put_ - 1 + entry_count_) % entry_count_;
  }

  NPP npp_;
  gpu_plugin::NPObjectPointer<NPObject> command_buffer_;
  gpu_plugin::NPObjectPointer<NPObject> ring_buffer_;
  CommandBufferEntry *entries_;
  int32 entry_count_;
  int32 token_;
  int32 last_token_read_;
  int32 get_;
  int32 put_;

  friend class CommandBufferHelperTest;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_CLIENT_CROSS_CMD_BUFFER_HELPER_H_
