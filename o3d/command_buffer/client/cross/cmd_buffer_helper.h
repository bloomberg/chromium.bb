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
#include "command_buffer/common/cross/buffer_sync_api.h"
#include "command_buffer/common/cross/cmd_buffer_format.h"

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
// unsigned int token = helper.InsertToken();
// helper.AddCommand(...);
// helper.AddCommand(...);
// [...]
//
// helper.WaitForToken(token);  // this doesn't return until the first two
//                              // commands have been executed.
class CommandBufferHelper {
 public:
  // Constructs a CommandBufferHelper object. The helper needs to be
  // initialized by calling Init() before use.
  // Parameters:
  //   interface: the buffer interface the helper sends commands to.
  explicit CommandBufferHelper(BufferSyncInterface *interface);
  ~CommandBufferHelper();

  // Initializes the command buffer by allocating shared memory.
  // Parameters:
  //   entry_count: the number of entries in the buffer. Note that commands
  //     sent through the buffer must use at most entry_count-2 arguments
  //     (entry_count-1 size).
  // Returns:
  //   true if successful.
  bool Init(unsigned int entry_count);

  // Flushes the commands, setting the put pointer to let the buffer interface
  // know that new commands have been added.
  void Flush() {
    interface_->Put(put_);
  }

  // Waits until all the commands have been executed.
  void Finish();

  // Waits until a given number of available entries are available.
  // Parameters:
  //   count: number of entries needed. This value must be at most
  //     the size of the buffer minus one.
  void WaitForAvailableEntries(unsigned int count);

  // Adds a command data to the command buffer. This may wait until sufficient
  // space is available.
  // Parameters:
  //   entries: The command entries to add.
  //   count: The number of entries.
  void AddCommandData(const CommandBufferEntry* entries, unsigned int count) {
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
  void AddCommand(unsigned int command,
                  unsigned int arg_count,
                  const CommandBufferEntry *args) {
    CommandHeader header;
    header.size = arg_count + 1;
    header.command = command;
    WaitForAvailableEntries(header.size);
    entries_[put_++].value_header = header;
    for (unsigned int i = 0; i < arg_count; ++i) {
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
  //   the value of the new token.
  unsigned int InsertToken();

  // Waits until the token of a particular value has passed through the command
  // stream (i.e. commands inserted before that token have been executed).
  // NOTE: This will call Flush if it needs to block.
  // Parameters:
  //   the value of the token to wait for.
  void WaitForToken(unsigned int token);

  // Returns the buffer interface used to send synchronous commands.
  BufferSyncInterface *interface() { return interface_; }

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

  void CreateVertexBuffer(uint32 id, uint32 size, uint32 flags) {
    cmd::CreateVertexBuffer& cmd = GetCmdSpace<cmd::CreateVertexBuffer>();
    cmd.Init(id, size, flags);
  }

  void DestroyVertexBuffer(uint32 id) {
    cmd::DestroyVertexBuffer& cmd = GetCmdSpace<cmd::DestroyVertexBuffer>();
    cmd.Init(id);
  }

  void SetVertexBufferDataImmediate(
      uint32 id, uint32 offset,
      const void* data, uint32 size) {
    cmd::SetVertexBufferDataImmediate& cmd =
        GetImmediateCmdSpace<cmd::SetVertexBufferDataImmediate>(size);
    cmd.Init(id, offset, data, size);
  }

  void SetVertexBufferData(
      uint32 id, uint32 offset, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::SetVertexBufferData& cmd =
        GetCmdSpace<cmd::SetVertexBufferData>();
    cmd.Init(id, offset, size,
             shared_memory_id, shared_memory_offset);
  }

  void GetVertexBufferData(
      uint32 id, uint32 offset, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::GetVertexBufferData& cmd =
        GetCmdSpace<cmd::GetVertexBufferData>();
    cmd.Init(id, offset, size,
             shared_memory_id, shared_memory_offset);
  }

  void CreateIndexBuffer(uint32 id, uint32 size, uint32 flags) {
    cmd::CreateIndexBuffer& cmd =
        GetCmdSpace<cmd::CreateIndexBuffer>();
    cmd.Init(id, size, flags);
  }

  void DestroyIndexBuffer(uint32 id) {
    cmd::DestroyIndexBuffer& cmd = GetCmdSpace<cmd::DestroyIndexBuffer>();
    cmd.Init(id);
  }

  void SetIndexBufferDataImmediate(
      uint32 id, uint32 offset, const void* data, uint32 size) {
    cmd::SetIndexBufferDataImmediate& cmd =
        GetImmediateCmdSpace<cmd::SetIndexBufferDataImmediate>(size);
    cmd.Init(id, offset, data, size);
  }

  void SetIndexBufferData(
      uint32 id, uint32 offset, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::SetIndexBufferData& cmd = GetCmdSpace<cmd::SetIndexBufferData>();
    cmd.Init(id, offset, size, shared_memory_id, shared_memory_offset);
  }

  void GetIndexBufferData(
      uint32 id, uint32 offset, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::GetIndexBufferData& cmd = GetCmdSpace<cmd::GetIndexBufferData>();
    cmd.Init(id, offset, size, shared_memory_id, shared_memory_offset);
  }

  void CreateVertexStruct(uint32 id, uint32 input_count) {
    cmd::CreateVertexStruct& cmd = GetCmdSpace<cmd::CreateVertexStruct>();
    cmd.Init(id, input_count);
  }

  void DestroyVertexStruct(uint32 id) {
    cmd::DestroyVertexStruct& cmd = GetCmdSpace<cmd::DestroyVertexStruct>();
    cmd.Init(id);
  }

  void SetVertexInput(
      uint32 vertex_struct_id,
      uint32 input_index,
      uint32 vertex_buffer_id,
      uint32 offset,
      uint8 semantic,
      uint32 semantic_index,
      uint8 type,
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

  void SetVertexStruct(uint32 id) {
    cmd::SetVertexStruct& cmd = GetCmdSpace<cmd::SetVertexStruct>();
    cmd.Init(id);
  }

  void Draw(uint32 primitive_type, uint32 first, uint32 count) {
    cmd::Draw& cmd = GetCmdSpace<cmd::Draw>();
    cmd.Init(primitive_type, first, count);
  }

  void DrawIndexed(
      uint32 primitive_type,
      uint32 index_buffer_id,
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
      uint32 id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::CreateEffect& cmd = GetCmdSpace<cmd::CreateEffect>();
    cmd.Init(id, size, shared_memory_id, shared_memory_offset);
  }

  void CreateEffectImmediate(uint32 id, uint32 size, const void* data) {
    cmd::CreateEffectImmediate& cmd =
        GetImmediateCmdSpace<cmd::CreateEffectImmediate>(size);
    cmd.Init(id, size, data);
  }

  void DestroyEffect(uint32 id) {
    cmd::DestroyEffect& cmd = GetCmdSpace<cmd::DestroyEffect>();
    cmd.Init(id);
  }

  void SetEffect(uint32 id) {
    cmd::SetEffect& cmd = GetCmdSpace<cmd::SetEffect>();
    cmd.Init(id);
  }

  void GetParamCount(
      uint32 id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::GetParamCount& cmd = GetCmdSpace<cmd::GetParamCount>();
    cmd.Init(id, size, shared_memory_id, shared_memory_offset);
  }

  void CreateParam(uint32 param_id, uint32 effect_id, uint32 index) {
    cmd::CreateParam& cmd = GetCmdSpace<cmd::CreateParam>();
    cmd.Init(param_id, effect_id, index);
  }

  void CreateParamByName(
      uint32 param_id, uint32 effect_id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::CreateParamByName& cmd = GetCmdSpace<cmd::CreateParamByName>();
    cmd.Init(param_id, effect_id, size, shared_memory_id, shared_memory_offset);
  }

  void CreateParamByNameImmediate(
      uint32 param_id, uint32 effect_id, uint32 size, const void* data) {
    cmd::CreateParamByNameImmediate& cmd =
        GetImmediateCmdSpace<cmd::CreateParamByNameImmediate>(size);
    cmd.Init(param_id, effect_id, size, data);
  }

  void DestroyParam(uint32 id) {
    cmd::DestroyParam& cmd = GetCmdSpace<cmd::DestroyParam>();
    cmd.Init(id);
  }

  void SetParamData(
      uint32 id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::SetParamData& cmd = GetCmdSpace<cmd::SetParamData>();
    cmd.Init(id, size, shared_memory_id, shared_memory_offset);
  }

  void SetParamDataImmediate(uint32 id, uint32 size, const void* data) {
    cmd::SetParamDataImmediate& cmd =
        GetImmediateCmdSpace<cmd::SetParamDataImmediate>(size);
    cmd.Init(id, size, data);
  }

  void GetParamDesc(
      uint32 id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::GetParamDesc& cmd = GetCmdSpace<cmd::GetParamDesc>();
    cmd.Init(id, size, shared_memory_id, shared_memory_offset);
  }

  void GetStreamCount(
      uint32 id, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::GetStreamCount& cmd = GetCmdSpace<cmd::GetStreamCount>();
    cmd.Init(id, size, shared_memory_id, shared_memory_offset);
  }

  void GetStreamDesc(
      uint32 id, uint32 index, uint32 size,
      uint32 shared_memory_id, uint32 shared_memory_offset) {
    cmd::GetStreamDesc& cmd = GetCmdSpace<cmd::GetStreamDesc>();
    cmd.Init(id, index, size, shared_memory_id, shared_memory_offset);
  }

  void DestroyTexture(uint32 id) {
    cmd::DestroyTexture& cmd = GetCmdSpace<cmd::DestroyTexture>();
    cmd.Init(id);
  }

  void CreateTexture2d(
      uint32 texture_id,
      uint32 width, uint32 height,
      uint32 levels, uint32 format,
      uint32 enable_render_surfaces) {
    cmd::CreateTexture2d& cmd = GetCmdSpace<cmd::CreateTexture2d>();
    cmd.Init(texture_id,
             width, height, levels, format,
             enable_render_surfaces);
  }

  void CreateTexture3d(
      uint32 texture_id,
      uint32 width, uint32 height, uint32 depth,
      uint32 levels, uint32 format,
      uint32 enable_render_surfaces) {
    cmd::CreateTexture3d& cmd = GetCmdSpace<cmd::CreateTexture3d>();
    cmd.Init(texture_id,
             width, height, depth,
             levels, format,
             enable_render_surfaces);
  }

  void CreateTextureCube(
      uint32 texture_id,
      uint32 edge_length, uint32 levels, uint32 format,
      uint32 enable_render_surfaces) {
    cmd::CreateTextureCube& cmd = GetCmdSpace<cmd::CreateTextureCube>();
    cmd.Init(texture_id,
             edge_length, levels, format,
             enable_render_surfaces);
  }

  void SetTextureData(
      uint32 texture_id,
      uint32 x,
      uint32 y,
      uint32 z,
      uint32 width,
      uint32 height,
      uint32 depth,
      uint32 level,
      uint32 face,
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
      uint32 texture_id,
      uint32 x,
      uint32 y,
      uint32 z,
      uint32 width,
      uint32 height,
      uint32 depth,
      uint32 level,
      uint32 face,
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
      uint32 texture_id,
      uint32 x,
      uint32 y,
      uint32 z,
      uint32 width,
      uint32 height,
      uint32 depth,
      uint32 level,
      uint32 face,
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

  void CreateSampler(uint32 id) {
    cmd::CreateSampler& cmd = GetCmdSpace<cmd::CreateSampler>();
    cmd.Init(id);
  }

  void DestroySampler(uint32 id) {
    cmd::DestroySampler& cmd = GetCmdSpace<cmd::DestroySampler>();
    cmd.Init(id);
  }

  void SetSamplerStates(uint32 id,
      uint32 address_u_value,
      uint32 address_v_value,
      uint32 address_w_value,
      uint32 mag_filter_value,
      uint32 min_filter_value,
      uint32 mip_filter_value,
      uint32 max_anisotropy) {
    cmd::SetSamplerStates& cmd = GetCmdSpace<cmd::SetSamplerStates>();
    cmd.Init(
        id,
        address_u_value,
        address_v_value,
        address_w_value,
        mag_filter_value,
        min_filter_value,
        mip_filter_value,
        max_anisotropy);
  }

  void SetSamplerBorderColor(
      uint32 id,
      float red, float green, float blue, float alpha) {
    cmd::SetSamplerBorderColor& cmd =
        GetCmdSpace<cmd::SetSamplerBorderColor>();
    cmd.Init(id, red, green, blue, alpha);
  }

  void SetSamplerTexture(uint32 id, uint32 texture_id) {
    cmd::SetSamplerTexture& cmd = GetCmdSpace<cmd::SetSamplerTexture>();
    cmd.Init(id, texture_id);
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

  void SetPolygonRaster(uint32 fill_mode, uint32 cull_mode) {
    cmd::SetPolygonRaster& cmd = GetCmdSpace<cmd::SetPolygonRaster>();
    cmd.Init(fill_mode, cull_mode);
  }

  void SetAlphaTest(uint32 func, bool enable, float value) {
    cmd::SetAlphaTest& cmd = GetCmdSpace<cmd::SetAlphaTest>();
    cmd.Init(func, enable, value);
  }

  void SetDepthTest(uint32 func, bool write_enable, bool enable) {
    cmd::SetDepthTest& cmd = GetCmdSpace<cmd::SetDepthTest>();
    cmd.Init(func, write_enable, enable);
  }

  void SetStencilTest(
      uint8 write_mask,
      uint8 compare_mask,
      uint8 reference_value,
      bool separate_ccw,
      bool enable,
      uint8 cw_func,
      uint8 cw_pass_op,
      uint8 cw_fail_op,
      uint8 cw_z_fail_op,
      uint8 ccw_func,
      uint8 ccw_pass_op,
      uint8 ccw_fail_op,
      uint8 ccw_z_fail_op) {
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
      uint8 color_src_func,
      uint8 color_dst_func,
      uint8 color_eq,
      uint8 alpha_src_func,
      uint8 alpha_dst_func,
      uint8 alpha_eq,
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
      uint32 id, uint32 texture_id,
      uint32 width, uint32 height,
      uint32 level, uint32 side) {
    cmd::CreateRenderSurface& cmd = GetCmdSpace<cmd::CreateRenderSurface>();
    cmd.Init(id, texture_id, width, height, level, side);
  }

  void DestroyRenderSurface(uint32 id) {
    cmd::DestroyRenderSurface& cmd =
        GetCmdSpace<cmd::DestroyRenderSurface>();
    cmd.Init(id);
  }

  void CreateDepthSurface(uint32 id, uint32 width, uint32 height) {
    cmd::CreateDepthSurface& cmd = GetCmdSpace<cmd::CreateDepthSurface>();
    cmd.Init(id, width, height);
  }

  void DestroyDepthSurface(uint32 id) {
    cmd::DestroyDepthSurface& cmd = GetCmdSpace<cmd::DestroyDepthSurface>();
    cmd.Init(id);
  }

  void SetRenderSurface(uint32 render_surface_id, uint32 depth_surface_id) {
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
  unsigned int AvailableEntries() {
    return static_cast<unsigned int>(
        (get_ - put_ - 1 + entry_count_) % entry_count_);
  }

  BufferSyncInterface *interface_;
  CommandBufferEntry *entries_;
  unsigned int entry_count_;
  unsigned int token_;
  unsigned int last_token_read_;
  RPCShmHandle shm_handle_;
  unsigned int shm_id_;
  CommandBufferOffset get_;
  CommandBufferOffset put_;

  friend class CommandBufferHelperTest;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_CLIENT_CROSS_CMD_BUFFER_HELPER_H_
