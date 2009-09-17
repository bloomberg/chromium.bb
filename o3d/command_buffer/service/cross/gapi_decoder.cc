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


// This class contains the implementation of the GAPI decoder class, decoding
// GAPI commands into calls to a GAPIInterface class.

#include "base/cross/bits.h"
#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/service/cross/gapi_decoder.h"
#include "command_buffer/service/cross/cmd_buffer_engine.h"

namespace o3d {
namespace command_buffer {

namespace {

template <typename T>
const void* AddressAfterStruct(const T& pod) {
  return reinterpret_cast<const uint8*>(&pod) + sizeof(pod);
}

template <typename T>
unsigned int ImmediateSize(uint32 arg_count, const T& pod) {
  return (arg_count - sizeof(pod) / sizeof(uint32)) * sizeof(uint32);
}

// TODO(gman): Remove this.
CommandBufferEntry* TempHack(const void* foo) {
  return reinterpret_cast<CommandBufferEntry*>(const_cast<void*>(foo));
}

struct CommandInfo {
  int arg_flags;
  int arg_count;
};

const CommandInfo g_command_info[] = {
  #define O3D_COMMAND_BUFFER_CMD_OP(name) {  \
    cmd::name::kArgFlags,                    \
    sizeof(cmd::name) / sizeof(uint32), },   \

  O3D_COMMAND_BUFFER_CMDS

  #undef O3D_COMMAND_BUFFER_CMD_OP
};

}  // anonymous namespace.

// Decode command with its arguments, and call the corresponding GAPIInterface
// method.
// Note: args is a pointer to the command buffer. As such, it could be changed
// by a (malicious) client at any time, so if validation has to happen, it
// should operate on a copy of them.
BufferSyncInterface::ParseError GAPIDecoder::DoCommand(
    unsigned int command,
    unsigned int arg_count,
    const void* args) {
  if (command < arraysize(g_command_info)) {
    const CommandInfo& info = g_command_info[command];
    if ((info.arg_flags == cmd::kFixed && arg_count == info.arg_count) ||
        (info.arg_flags == cmd::kAtLeastN && arg_count > info.arg_count)) {
      switch (command) {
        #define O3D_COMMAND_BUFFER_CMD_OP(name)                    \
          case cmd::name::kCmdId:                                  \
            return Handle_ ## name(                                \
                arg_count,                                         \
                *static_cast<const cmd::name*>(args));             \

        O3D_COMMAND_BUFFER_CMDS

        #undef O3D_COMMAND_BUFFER_CMD_OP
      }
    } else {
      return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
    }
  }
  return BufferSyncInterface::PARSE_UNKNOWN_COMMAND;
}


// Decodes the SET_VERTEX_INPUT command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeSetVertexInput(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  namespace cmd = set_vertex_input_cmd;
  if (arg_count != 5) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  ResourceID vertex_struct_id = args[0].value_uint32;
  unsigned int input_index = args[1].value_uint32;
  ResourceID vertex_buffer_id = args[2].value_uint32;
  unsigned int offset = args[3].value_uint32;
  unsigned int type_stride_semantic = args[4].value_uint32;
  unsigned int semantic_index = cmd::SemanticIndex::Get(type_stride_semantic);
  unsigned int semantic = cmd::Semantic::Get(type_stride_semantic);
  unsigned int type = cmd::Type::Get(type_stride_semantic);
  unsigned int stride = cmd::Stride::Get(type_stride_semantic);
  if (semantic >= vertex_struct::NUM_SEMANTICS ||
      type >= vertex_struct::NUM_TYPES || stride == 0)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->SetVertexInput(vertex_struct_id, input_index, vertex_buffer_id,
                               offset, stride,
                               static_cast<vertex_struct::Type>(type),
                               static_cast<vertex_struct::Semantic>(semantic),
                               semantic_index);
}

// Decodes the CREATE_TEXTURE_2D command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeCreateTexture2D(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  if (arg_count == 3) {
    namespace cmd = create_texture_2d_cmd;
    unsigned int id = args[0].value_uint32;
    unsigned int width_height = args[1].value_uint32;
    unsigned int levels_format_flags = args[2].value_uint32;
    unsigned int width = cmd::Width::Get(width_height);
    unsigned int height = cmd::Height::Get(width_height);
    unsigned int levels = cmd::Levels::Get(levels_format_flags);
    unsigned int unused = cmd::Unused::Get(levels_format_flags);
    unsigned int format = cmd::Format::Get(levels_format_flags);
    unsigned int flags = cmd::Flags::Get(levels_format_flags);
    unsigned int max_levels =
        1 + base::bits::Log2Ceiling(std::max(width, height));
    if ((width == 0) || (height == 0) || (levels > max_levels) ||
        (unused != 0) || (format >= texture::NUM_FORMATS))
      return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
    if (levels == 0) levels = max_levels;
    bool enable_render_surfaces = !!flags;
    return gapi_->CreateTexture2D(id, width, height, levels,
                                  static_cast<texture::Format>(format), flags,
                                  enable_render_surfaces);
  } else {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
}

// Decodes the CREATE_TEXTURE_3D command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeCreateTexture3D(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  if (arg_count == 4) {
    namespace cmd = create_texture_3d_cmd;
    unsigned int id = args[0].value_uint32;
    unsigned int width_height = args[1].value_uint32;
    unsigned int depth_unused = args[2].value_uint32;
    unsigned int levels_format_flags = args[3].value_uint32;
    unsigned int width = cmd::Width::Get(width_height);
    unsigned int height = cmd::Height::Get(width_height);
    unsigned int depth = cmd::Depth::Get(depth_unused);
    unsigned int unused1 = cmd::Unused1::Get(depth_unused);
    unsigned int levels = cmd::Levels::Get(levels_format_flags);
    unsigned int unused2 = cmd::Unused2::Get(levels_format_flags);
    unsigned int format = cmd::Format::Get(levels_format_flags);
    unsigned int flags = cmd::Flags::Get(levels_format_flags);
    unsigned int max_levels =
        1 + base::bits::Log2Ceiling(std::max(depth, std::max(width, height)));
    if ((width == 0) || (height == 0) || (depth == 0) ||
        (levels > max_levels) || (unused1 != 0) || (unused2 != 0) ||
        (format >= texture::NUM_FORMATS))
      return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
    if (levels == 0) levels = max_levels;
    bool enable_render_surfaces = !!flags;
    return gapi_->CreateTexture3D(id, width, height, depth, levels,
                                  static_cast<texture::Format>(format), flags,
                                  enable_render_surfaces);
  } else {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
}

// Decodes the CREATE_TEXTURE_CUBE command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeCreateTextureCube(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  if (arg_count == 3) {
    namespace cmd = create_texture_cube_cmd;
    unsigned int id = args[0].value_uint32;
    unsigned int side_unused = args[1].value_uint32;
    unsigned int levels_format_flags = args[2].value_uint32;
    unsigned int side = cmd::Side::Get(side_unused);
    unsigned int unused1 = cmd::Unused1::Get(side_unused);
    unsigned int levels = cmd::Levels::Get(levels_format_flags);
    unsigned int unused2 = cmd::Unused2::Get(levels_format_flags);
    unsigned int format = cmd::Format::Get(levels_format_flags);
    unsigned int flags = cmd::Flags::Get(levels_format_flags);
    unsigned int max_levels = 1 + base::bits::Log2Ceiling(side);
    if ((side == 0) || (levels > max_levels) || (unused1 != 0) ||
        (unused2 != 0) || (format >= texture::NUM_FORMATS))
      return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
    if (levels == 0) levels = max_levels;
    bool enable_render_surfaces = !!flags;
    return gapi_->CreateTextureCube(id, side, levels,
                                    static_cast<texture::Format>(format),
                                    flags, enable_render_surfaces);
  } else {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
}

// Decodes the SET_TEXTURE_DATA command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeSetTextureData(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  if (arg_count == 10) {
    namespace cmd = set_texture_data_cmd;
    unsigned int id = args[0].value_uint32;
    unsigned int x_y = args[1].value_uint32;
    unsigned int width_height = args[2].value_uint32;
    unsigned int z_depth = args[3].value_uint32;
    unsigned int level_face = args[4].value_uint32;
    unsigned int row_pitch = args[5].value_uint32;
    unsigned int slice_pitch = args[6].value_uint32;
    unsigned int size = args[7].value_uint32;
    unsigned int shm_id = args[8].value_uint32;
    unsigned int offset = args[9].value_uint32;
    unsigned int x = cmd::X::Get(x_y);
    unsigned int y = cmd::Y::Get(x_y);
    unsigned int width = cmd::Width::Get(width_height);
    unsigned int height = cmd::Height::Get(width_height);
    unsigned int z = cmd::Z::Get(z_depth);
    unsigned int depth = cmd::Depth::Get(z_depth);
    unsigned int level = cmd::Level::Get(level_face);
    unsigned int face = cmd::Face::Get(level_face);
    unsigned int unused = cmd::Unused::Get(level_face);
    const void *data = GetAddressAndCheckSize(shm_id, offset, size);
    if (face >= 6 || unused != 0 || !data)
      return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
    return gapi_->SetTextureData(id, x, y, z, width, height, depth, level,
                                 static_cast<texture::Face>(face), row_pitch,
                                 slice_pitch, size, data);

  } else {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
}

// Decodes the SET_TEXTURE_DATA_IMMEDIATE command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeSetTextureDataImmediate(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  if (arg_count > 8) {
    namespace cmd = set_texture_data_immediate_cmd;
    unsigned int id = args[0].value_uint32;
    unsigned int x_y = args[1].value_uint32;
    unsigned int width_height = args[2].value_uint32;
    unsigned int z_depth = args[3].value_uint32;
    unsigned int level_face = args[4].value_uint32;
    unsigned int row_pitch = args[5].value_uint32;
    unsigned int slice_pitch = args[6].value_uint32;
    unsigned int size = args[7].value_uint32;
    unsigned int x = cmd::X::Get(x_y);
    unsigned int y = cmd::Y::Get(x_y);
    unsigned int width = cmd::Width::Get(width_height);
    unsigned int height = cmd::Height::Get(width_height);
    unsigned int z = cmd::Z::Get(z_depth);
    unsigned int depth = cmd::Depth::Get(z_depth);
    unsigned int level = cmd::Level::Get(level_face);
    unsigned int face = cmd::Face::Get(level_face);
    unsigned int unused = cmd::Unused::Get(level_face);
    if (face >= 6 || unused != 0 ||
        size > (arg_count - 5) * sizeof(args[0]))
      return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
    return gapi_->SetTextureData(id, x, y, z, width, height, depth, level,
                                 static_cast<texture::Face>(face), row_pitch,
                                 slice_pitch, size, args + 8);
  } else {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
}

// Decodes the GET_TEXTURE_DATA command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeGetTextureData(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  if (arg_count == 10) {
    namespace cmd = get_texture_data_cmd;
    unsigned int id = args[0].value_uint32;
    unsigned int x_y = args[1].value_uint32;
    unsigned int width_height = args[2].value_uint32;
    unsigned int z_depth = args[3].value_uint32;
    unsigned int level_face = args[4].value_uint32;
    unsigned int row_pitch = args[5].value_uint32;
    unsigned int slice_pitch = args[6].value_uint32;
    unsigned int size = args[7].value_uint32;
    unsigned int shm_id = args[8].value_uint32;
    unsigned int offset = args[9].value_uint32;
    unsigned int x = cmd::X::Get(x_y);
    unsigned int y = cmd::Y::Get(x_y);
    unsigned int width = cmd::Width::Get(width_height);
    unsigned int height = cmd::Height::Get(width_height);
    unsigned int z = cmd::Z::Get(z_depth);
    unsigned int depth = cmd::Depth::Get(z_depth);
    unsigned int level = cmd::Level::Get(level_face);
    unsigned int face = cmd::Face::Get(level_face);
    unsigned int unused = cmd::Unused::Get(level_face);
    void *data = GetAddressAndCheckSize(shm_id, offset, size);
    if (face >= 6 || unused != 0 || !data)
      return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
    return gapi_->GetTextureData(id, x, y, z, width, height, depth, level,
                                 static_cast<texture::Face>(face), row_pitch,
                                 slice_pitch, size, data);

  } else {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
}

// Decodes the SET_SAMPLER_STATES command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeSetSamplerStates(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  namespace cmd = set_sampler_states;
  if (arg_count != 2)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  ResourceID id = args[0].value_uint32;
  Uint32 arg = args[1].value_uint32;
  if (cmd::Unused::Get(arg) != 0)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  unsigned int address_u_value = cmd::AddressingU::Get(arg);
  unsigned int address_v_value = cmd::AddressingV::Get(arg);
  unsigned int address_w_value = cmd::AddressingW::Get(arg);
  unsigned int mag_filter_value = cmd::MagFilter::Get(arg);
  unsigned int min_filter_value = cmd::MinFilter::Get(arg);
  unsigned int mip_filter_value = cmd::MipFilter::Get(arg);
  unsigned int max_anisotropy = cmd::MaxAnisotropy::Get(arg);
  if (address_u_value >= sampler::NUM_ADDRESSING_MODE ||
      address_v_value >= sampler::NUM_ADDRESSING_MODE ||
      address_w_value >= sampler::NUM_ADDRESSING_MODE ||
      mag_filter_value >= sampler::NUM_FILTERING_MODE ||
      min_filter_value >= sampler::NUM_FILTERING_MODE ||
      mip_filter_value >= sampler::NUM_FILTERING_MODE ||
      mag_filter_value == sampler::NONE ||
      min_filter_value == sampler::NONE ||
      max_anisotropy == 0) {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
  gapi_->SetSamplerStates(
      id,
      static_cast<sampler::AddressingMode>(address_u_value),
      static_cast<sampler::AddressingMode>(address_v_value),
      static_cast<sampler::AddressingMode>(address_w_value),
      static_cast<sampler::FilteringMode>(mag_filter_value),
      static_cast<sampler::FilteringMode>(min_filter_value),
      static_cast<sampler::FilteringMode>(mip_filter_value),
      max_anisotropy);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

// Decodes the SET_STENCIL_TEST command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeSetStencilTest(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  namespace cmd = set_stencil_test;
  if (arg_count != 2)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  Uint32 arg0 = args[0].value_uint32;
  Uint32 arg1 = args[1].value_uint32;
  if (cmd::Unused0::Get(arg0) != 0 ||
      cmd::Unused1::Get(arg1) != 0 ||
      cmd::Unused2::Get(arg1) != 0)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  unsigned int write_mask = cmd::WriteMask::Get(arg0);
  unsigned int compare_mask = cmd::CompareMask::Get(arg0);
  unsigned int ref = cmd::ReferenceValue::Get(arg0);
  bool enable = cmd::Enable::Get(arg0) != 0;
  bool separate_ccw = cmd::SeparateCCW::Get(arg0) != 0;
  gapi_->SetStencilTest(enable, separate_ccw, write_mask, compare_mask, ref,
                        arg1);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

// Decodes the SET_BLENDING command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeSetBlending(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  namespace cmd = set_blending;
  if (arg_count != 1)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  Uint32 arg = args[0].value_uint32;
  bool enable = cmd::Enable::Get(arg) != 0;
  bool separate_alpha = cmd::SeparateAlpha::Get(arg) != 0;
  unsigned int color_eq = cmd::ColorEq::Get(arg);
  unsigned int color_src = cmd::ColorSrcFunc::Get(arg);
  unsigned int color_dst = cmd::ColorDstFunc::Get(arg);
  unsigned int alpha_eq = cmd::AlphaEq::Get(arg);
  unsigned int alpha_src = cmd::AlphaSrcFunc::Get(arg);
  unsigned int alpha_dst = cmd::AlphaDstFunc::Get(arg);
  if (cmd::Unused0::Get(arg) != 0 ||
      cmd::Unused1::Get(arg) != 0 ||
      color_eq >= GAPIInterface::NUM_BLEND_EQ ||
      color_src >= GAPIInterface::NUM_BLEND_FUNC ||
      color_dst >= GAPIInterface::NUM_BLEND_FUNC ||
      alpha_eq >= GAPIInterface::NUM_BLEND_EQ ||
      alpha_src >= GAPIInterface::NUM_BLEND_FUNC ||
      alpha_dst >= GAPIInterface::NUM_BLEND_FUNC)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  gapi_->SetBlending(enable,
                     separate_alpha,
                     static_cast<GAPIInterface::BlendEq>(color_eq),
                     static_cast<GAPIInterface::BlendFunc>(color_src),
                     static_cast<GAPIInterface::BlendFunc>(color_dst),
                     static_cast<GAPIInterface::BlendEq>(alpha_eq),
                     static_cast<GAPIInterface::BlendFunc>(alpha_src),
                     static_cast<GAPIInterface::BlendFunc>(alpha_dst));
  return BufferSyncInterface::PARSE_NO_ERROR;
}

void *GAPIDecoder::GetAddressAndCheckSize(unsigned int shm_id,
                                          unsigned int offset,
                                          unsigned int size) {
  void * shm_addr = engine_->GetSharedMemoryAddress(shm_id);
  if (!shm_addr) return NULL;
  size_t shm_size = engine_->GetSharedMemorySize(shm_id);
  if (offset + size > shm_size) return NULL;
  return static_cast<char *>(shm_addr) + offset;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_NOOP(
    uint32 arg_count,
    const cmd::NOOP& args) {
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_TOKEN(
    uint32 arg_count,
    const cmd::SET_TOKEN& args) {
  engine_->set_token(args.token);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_BEGIN_FRAME(
    uint32 arg_count,
    const cmd::BEGIN_FRAME& args) {
  gapi_->BeginFrame();
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_END_FRAME(
    uint32 arg_count,
    const cmd::END_FRAME& args) {
  gapi_->EndFrame();
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CLEAR(
    uint32 arg_count,
    const cmd::CLEAR& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 buffers = args.buffers;
  if (buffers & ~GAPIInterface::ALL_BUFFERS)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  RGBA rgba;
  rgba.red = args.red;
  rgba.green = args.green;
  rgba.blue = args.blue;
  rgba.alpha = args.alpha;
  gapi_->Clear(buffers, rgba, args.depth, args.stencil);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_VIEWPORT(
    uint32 arg_count,
    const cmd::SET_VIEWPORT& args) {
  gapi_->SetViewport(args.left,
                     args.top,
                     args.width,
                     args.height,
                     args.z_min,
                     args.z_max);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CREATE_VERTEX_BUFFER(
    uint32 arg_count,
    const cmd::CREATE_VERTEX_BUFFER& args) {
  return gapi_->CreateVertexBuffer(args.id, args.size, args.flags);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_DESTROY_VERTEX_BUFFER(
    uint32 arg_count,
    const cmd::DESTROY_VERTEX_BUFFER& args) {
  return gapi_->DestroyVertexBuffer(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_VERTEX_BUFFER_DATA_IMMEDIATE(
    uint32 arg_count,
    const cmd::SET_VERTEX_BUFFER_DATA_IMMEDIATE& args) {
  return gapi_->SetVertexBufferData(args.id, args.offset,
                                    ImmediateSize(arg_count, args),
                                    AddressAfterStruct(args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_VERTEX_BUFFER_DATA(
    uint32 arg_count,
    const cmd::SET_VERTEX_BUFFER_DATA& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->SetVertexBufferData(args.id, args.offset, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_GET_VERTEX_BUFFER_DATA(
    uint32 arg_count,
    const cmd::GET_VERTEX_BUFFER_DATA& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->GetVertexBufferData(args.id, args.offset, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CREATE_INDEX_BUFFER(
    uint32 arg_count,
    const cmd::CREATE_INDEX_BUFFER& args) {
  return gapi_->CreateIndexBuffer(args.id, args.size, args.flags);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_DESTROY_INDEX_BUFFER(
    uint32 arg_count,
    const cmd::DESTROY_INDEX_BUFFER& args) {
  return gapi_->DestroyIndexBuffer(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_INDEX_BUFFER_DATA_IMMEDIATE(
    uint32 arg_count,
    const cmd::SET_INDEX_BUFFER_DATA_IMMEDIATE& args) {
  return gapi_->SetIndexBufferData(args.id, args.offset,
                                   ImmediateSize(arg_count, args),
                                   AddressAfterStruct(args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_INDEX_BUFFER_DATA(
    uint32 arg_count,
    const cmd::SET_INDEX_BUFFER_DATA& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->SetIndexBufferData(args.id, args.offset, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_GET_INDEX_BUFFER_DATA(
    uint32 arg_count,
    const cmd::GET_INDEX_BUFFER_DATA& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->GetIndexBufferData(args.id, args.offset, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CREATE_VERTEX_STRUCT(
    uint32 arg_count,
    const cmd::CREATE_VERTEX_STRUCT& args) {
  return gapi_->CreateVertexStruct(args.id, args.input_count);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_DESTROY_VERTEX_STRUCT(
    uint32 arg_count,
    const cmd::DESTROY_VERTEX_STRUCT& args) {
  return gapi_->DestroyVertexStruct(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_VERTEX_INPUT(
    uint32 arg_count,
    const cmd::SET_VERTEX_INPUT& args) {
  // TODO(gman): fix.
  return DecodeSetVertexInput(arg_count, TempHack(&args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_VERTEX_STRUCT(
    uint32 arg_count,
    const cmd::SET_VERTEX_STRUCT& args) {
  return gapi_->SetVertexStruct(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_DRAW(
    uint32 arg_count,
    const cmd::DRAW& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 primitive_type = args.primitive_type;
  if (primitive_type >= GAPIInterface::MAX_PRIMITIVE_TYPE)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->Draw(
      static_cast<GAPIInterface::PrimitiveType>(primitive_type),
      args.first, args.count);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_DRAW_INDEXED(
    uint32 arg_count,
    const cmd::DRAW_INDEXED& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 primitive_type = args.primitive_type;
  if (primitive_type >= GAPIInterface::MAX_PRIMITIVE_TYPE)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->DrawIndexed(
      static_cast<GAPIInterface::PrimitiveType>(primitive_type),
      args.index_buffer_id,
      args.first, args.count, args.min_index, args.max_index);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CREATE_EFFECT(
    uint32 arg_count,
    const cmd::CREATE_EFFECT& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->CreateEffect(args.id, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CREATE_EFFECT_IMMEDIATE(
    uint32 arg_count,
    const cmd::CREATE_EFFECT_IMMEDIATE& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  if (size > ImmediateSize(arg_count, args))
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->CreateEffect(args.id, size, AddressAfterStruct(args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_DESTROY_EFFECT(
    uint32 arg_count,
    const cmd::DESTROY_EFFECT& args) {
  return gapi_->DestroyEffect(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_EFFECT(
    uint32 arg_count,
    const cmd::SET_EFFECT& args) {
  return gapi_->SetEffect(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_GET_PARAM_COUNT(
    uint32 arg_count,
    const cmd::GET_PARAM_COUNT& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->GetParamCount(args.id, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CREATE_PARAM(
    uint32 arg_count,
    const cmd::CREATE_PARAM& args) {
  return gapi_->CreateParam(args.param_id, args.effect_id, args.index);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CREATE_PARAM_BY_NAME(
    uint32 arg_count,
    const cmd::CREATE_PARAM_BY_NAME& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->CreateParamByName(args.param_id, args.effect_id, size,
                                  data);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CREATE_PARAM_BY_NAME_IMMEDIATE(
    uint32 arg_count,
    const cmd::CREATE_PARAM_BY_NAME_IMMEDIATE& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  if (size > ImmediateSize(arg_count, args))
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->CreateParamByName(args.param_id, args.effect_id, size,
                                  AddressAfterStruct(args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_DESTROY_PARAM(
    uint32 arg_count,
    const cmd::DESTROY_PARAM& args) {
  return gapi_->DestroyParam(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_PARAM_DATA(
    uint32 arg_count,
    const cmd::SET_PARAM_DATA& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->SetParamData(args.id, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_PARAM_DATA_IMMEDIATE(
    uint32 arg_count,
    const cmd::SET_PARAM_DATA_IMMEDIATE& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  if (size > ImmediateSize(arg_count, args))
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->SetParamData(args.id, size, AddressAfterStruct(args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_GET_PARAM_DESC(
    uint32 arg_count,
    const cmd::GET_PARAM_DESC& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->GetParamDesc(args.id, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_GET_STREAM_COUNT(
    uint32 arg_count,
    const cmd::GET_STREAM_COUNT& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->GetStreamCount(args.id, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_GET_STREAM_DESC(
    uint32 arg_count,
    const cmd::GET_STREAM_DESC& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->GetStreamDesc(args.id, args.index, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_DESTROY_TEXTURE(
    uint32 arg_count,
    const cmd::DESTROY_TEXTURE& args) {
  return gapi_->DestroyTexture(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CREATE_TEXTURE_2D(
    uint32 arg_count,
    const cmd::CREATE_TEXTURE_2D& args) {
  // TODO(gman): fix.
  return DecodeCreateTexture2D(arg_count, TempHack(&args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CREATE_TEXTURE_3D(
    uint32 arg_count,
    const cmd::CREATE_TEXTURE_3D& args) {
  // TODO(gman): fix.
  return DecodeCreateTexture3D(arg_count, TempHack(&args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CREATE_TEXTURE_CUBE(
    uint32 arg_count,
    const cmd::CREATE_TEXTURE_CUBE& args) {
  // TODO(gman): fix.
  return DecodeCreateTextureCube(arg_count, TempHack(&args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_TEXTURE_DATA(
    uint32 arg_count,
    const cmd::SET_TEXTURE_DATA& args) {
  // TODO(gman): fix.
  return DecodeSetTextureData(arg_count, TempHack(&args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_TEXTURE_DATA_IMMEDIATE(
    uint32 arg_count,
    const cmd::SET_TEXTURE_DATA_IMMEDIATE& args) {
  // TODO(gman): fix.
  return DecodeSetTextureDataImmediate(arg_count, TempHack(&args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_GET_TEXTURE_DATA(
    uint32 arg_count,
    const cmd::GET_TEXTURE_DATA& args) {
  // TODO(gman): fix.
  return DecodeGetTextureData(arg_count, TempHack(&args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CREATE_SAMPLER(
    uint32 arg_count,
    const cmd::CREATE_SAMPLER& args) {
  return gapi_->CreateSampler(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_DESTROY_SAMPLER(
    uint32 arg_count,
    const cmd::DESTROY_SAMPLER& args) {
  return gapi_->DestroySampler(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_SAMPLER_STATES(
    uint32 arg_count,
    const cmd::SET_SAMPLER_STATES& args) {
  // TODO(gman): fix.
  return DecodeSetSamplerStates(arg_count, TempHack(&args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_SAMPLER_BORDER_COLOR(
    uint32 arg_count,
    const cmd::SET_SAMPLER_BORDER_COLOR& args) {
  RGBA rgba;
  rgba.red = args.red;
  rgba.green = args.green;
  rgba.blue = args.blue;
  rgba.alpha = args.alpha;
  return gapi_->SetSamplerBorderColor(args.id, rgba);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_SAMPLER_TEXTURE(
    uint32 arg_count,
    const cmd::SET_SAMPLER_TEXTURE& args) {
  return gapi_->SetSamplerTexture(args.id, args.texture_id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_SCISSOR(
    uint32 arg_count,
    const cmd::SET_SCISSOR& args) {
  namespace cmd = set_scissor;
  Uint32 x_y_enable = args.fixme0;
  if (cmd::Unused::Get(x_y_enable) != 0)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  unsigned int x = cmd::X::Get(x_y_enable);
  unsigned int y = cmd::Y::Get(x_y_enable);
  bool enable = cmd::Enable::Get(x_y_enable) != 0;
  Uint32 width_height = args.fixme1;
  unsigned int width = cmd::Width::Get(width_height);
  unsigned int height = cmd::Height::Get(width_height);
  gapi_->SetScissor(enable, x, y, width, height);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_POLYGON_OFFSET(
    uint32 arg_count,
    const cmd::SET_POLYGON_OFFSET& args) {
  gapi_->SetPolygonOffset(args.slope_factor, args.units);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_POINT_LINE_RASTER(
    uint32 arg_count,
    const cmd::SET_POINT_LINE_RASTER& args) {
  namespace cmd = set_point_line_raster;
  Uint32 enables = args.fixme0;
  if (cmd::Unused::Get(enables) != 0)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  bool line_smooth = !!cmd::LineSmoothEnable::Get(enables);
  bool point_sprite = !!cmd::PointSpriteEnable::Get(enables);
  gapi_->SetPointLineRaster(line_smooth, point_sprite, args.point_size);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_POLYGON_RASTER(
    uint32 arg_count,
    const cmd::SET_POLYGON_RASTER& args) {
  namespace cmd = set_polygon_raster;
  Uint32 fill_cull = args.fixme0;
  unsigned int fill_value = cmd::FillMode::Get(fill_cull);
  unsigned int cull_value = cmd::CullMode::Get(fill_cull);
  if (cmd::Unused::Get(fill_cull) != 0 ||
      fill_value >= GAPIInterface::NUM_POLYGON_MODE ||
      cull_value >= GAPIInterface::NUM_FACE_CULL_MODE)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  gapi_->SetPolygonRaster(
      static_cast<GAPIInterface::PolygonMode>(fill_value),
      static_cast<GAPIInterface::FaceCullMode>(cull_value));
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_ALPHA_TEST(
    uint32 arg_count,
    const cmd::SET_ALPHA_TEST& args) {
  namespace cmd = set_alpha_test;
  Uint32 func_enable = args.fixme0;
  if (cmd::Unused::Get(func_enable) != 0)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  // Check that the bitmask get cannot generate values outside of the
  // allowed range.
  COMPILE_ASSERT(cmd::Func::kMask < GAPIInterface::NUM_COMPARISON,
                 set_alpha_test_Func_may_produce_invalid_values);
  GAPIInterface::Comparison comp =
      static_cast<GAPIInterface::Comparison>(cmd::Func::Get(func_enable));
  bool enable = cmd::Enable::Get(func_enable) != 0;
  gapi_->SetAlphaTest(enable, args.value, comp);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_DEPTH_TEST(
    uint32 arg_count,
    const cmd::SET_DEPTH_TEST& args) {
  namespace cmd = set_depth_test;
  Uint32 func_enable = args.fixme0;
  if (cmd::Unused::Get(func_enable) != 0)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  // Check that the bitmask get cannot generate values outside of the
  // allowed range.
  COMPILE_ASSERT(cmd::Func::kMask < GAPIInterface::NUM_COMPARISON,
                 set_alpha_test_Func_may_produce_invalid_values);
  GAPIInterface::Comparison comp =
      static_cast<GAPIInterface::Comparison>(cmd::Func::Get(func_enable));
  bool write_enable = cmd::WriteEnable::Get(func_enable) != 0;
  bool enable = cmd::Enable::Get(func_enable) != 0;
  gapi_->SetDepthTest(enable, write_enable, comp);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_STENCIL_TEST(
    uint32 arg_count,
    const cmd::SET_STENCIL_TEST& args) {
  // TODO(gman): fix.
  return DecodeSetStencilTest(arg_count, TempHack(&args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_COLOR_WRITE(
    uint32 arg_count,
    const cmd::SET_COLOR_WRITE& args) {
  namespace cmd = set_color_write;
  Uint32 enables = args.flags;
  if (cmd::Unused::Get(enables) != 0)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  bool red = cmd::RedMask::Get(enables) != 0;
  bool green = cmd::GreenMask::Get(enables) != 0;
  bool blue = cmd::BlueMask::Get(enables) != 0;
  bool alpha = cmd::AlphaMask::Get(enables) != 0;
  bool dither = cmd::DitherEnable::Get(enables) != 0;
  gapi_->SetColorWrite(red, green, blue, alpha, dither);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_BLENDING(
    uint32 arg_count,
    const cmd::SET_BLENDING& args) {
  return DecodeSetBlending(arg_count, TempHack(&args));
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_BLENDING_COLOR(
    uint32 arg_count,
    const cmd::SET_BLENDING_COLOR& args) {
  RGBA rgba;
  rgba.red = args.red;
  rgba.green = args.green;
  rgba.blue = args.blue;
  rgba.alpha = args.alpha;
  gapi_->SetBlendingColor(rgba);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CREATE_RENDER_SURFACE(
    uint32 arg_count,
    const cmd::CREATE_RENDER_SURFACE& args) {
  namespace cmd = create_render_surface_cmd;
  unsigned int width_height = args.fixme1;
  unsigned int width = cmd::Width::Get(width_height);
  unsigned int height = cmd::Height::Get(width_height);
  unsigned int levels_side = args.fixme2;
  unsigned int mip_level = cmd::Levels::Get(levels_side);
  unsigned int side = cmd::Side::Get(levels_side);
  return gapi_->CreateRenderSurface(args.id, width, height, mip_level,
                                    side, args.texture_id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_DESTROY_RENDER_SURFACE(
    uint32 arg_count,
    const cmd::DESTROY_RENDER_SURFACE& args) {
  return gapi_->DestroyRenderSurface(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_CREATE_DEPTH_SURFACE(
    uint32 arg_count,
    const cmd::CREATE_DEPTH_SURFACE& args) {
  namespace cmd = create_render_surface_cmd;
  unsigned int width_height = args.fixme1;
  unsigned int width = cmd::Width::Get(width_height);
  unsigned int height = cmd::Height::Get(width_height);
  return gapi_->CreateDepthSurface(args.id, width, height);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_DESTROY_DEPTH_SURFACE(
    uint32 arg_count,
    const cmd::DESTROY_DEPTH_SURFACE& args) {
  return gapi_->DestroyDepthSurface(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_RENDER_SURFACE(
    uint32 arg_count,
    const cmd::SET_RENDER_SURFACE& args) {
  return gapi_->SetRenderSurface(args.render_surface_id, args.depth_surface_id);
}

BufferSyncInterface::ParseError GAPIDecoder::Handle_SET_BACK_SURFACES(
    uint32 arg_count,
    const cmd::SET_BACK_SURFACES& args) {
  gapi_->SetBackSurfaces();
  return  BufferSyncInterface::PARSE_NO_ERROR;
}

}  // namespace command_buffer
}  // namespace o3d
