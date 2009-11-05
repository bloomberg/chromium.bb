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

namespace command_buffer {
namespace o3d {

namespace {

// Returns the address of the first byte after a struct.
template <typename T>
const void* AddressAfterStruct(const T& pod) {
  return reinterpret_cast<const uint8*>(&pod) + sizeof(pod);
}

// Returns the size in bytes of the data of an Immediate command, a command with
// its data inline in the command buffer.
template <typename T>
unsigned int ImmediateDataSize(uint32 arg_count, const T& pod) {
  return static_cast<unsigned int>(
      (arg_count + 1 - ComputeNumEntries(sizeof(pod))) *
      sizeof(CommandBufferEntry));  // NOLINT
}

// A struct to hold info about each command.
struct CommandInfo {
  int arg_flags;  // How to handle the arguments for this command
  int arg_count;  // How many arguments are expected for this command.
};

// A table of CommandInfo for all the commands.
const CommandInfo g_command_info[] = {
  #define O3D_COMMAND_BUFFER_CMD_OP(name) {                         \
    name::kArgFlags,                                                \
    sizeof(name) / sizeof(CommandBufferEntry) - 1, },  /* NOLINT */ \

  O3D_COMMAND_BUFFER_CMDS(O3D_COMMAND_BUFFER_CMD_OP)

  #undef O3D_COMMAND_BUFFER_CMD_OP
};

}  // anonymous namespace.

// Decode command with its arguments, and call the corresponding method.
// Note: args is a pointer to the command buffer. As such, it could be changed
// by a (malicious) client at any time, so if validation has to happen, it
// should operate on a copy of them.
parse_error::ParseError GAPIDecoder::DoCommand(
    unsigned int command,
    unsigned int arg_count,
    const void* cmd_data) {
  unsigned int command_index = command - kStartPoint - 1;
  if (command_index < arraysize(g_command_info)) {
    const CommandInfo& info = g_command_info[command_index];
    unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
    if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
        (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
      switch (command) {
        #define O3D_COMMAND_BUFFER_CMD_OP(name)                    \
          case name::kCmdId:                                       \
            return Handle ## name(                                 \
                arg_count,                                         \
                *static_cast<const name*>(cmd_data));              \

        O3D_COMMAND_BUFFER_CMDS(O3D_COMMAND_BUFFER_CMD_OP)

        #undef O3D_COMMAND_BUFFER_CMD_OP
      }
    } else {
      return parse_error::kParseInvalidArguments;
    }
  }
  return DoCommonCommand(command, arg_count, cmd_data);
  return parse_error::kParseUnknownCommand;
}

  // Overridden from AsyncAPIInterface.
const char* GAPIDecoder::GetCommandName(unsigned int command_id) const {
  if (command_id > kStartPoint && command_id < kNumCommands) {
    return o3d::GetCommandName(static_cast<CommandId>(command_id));
  }
  return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
}

parse_error::ParseError GAPIDecoder::HandleBeginFrame(
    uint32 arg_count,
    const BeginFrame& args) {
  gapi_->BeginFrame();
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleEndFrame(
    uint32 arg_count,
    const EndFrame& args) {
  gapi_->EndFrame();
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleClear(
    uint32 arg_count,
    const Clear& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 buffers = args.buffers;
  if (buffers & ~kAllBuffers)
    return parse_error::kParseInvalidArguments;
  RGBA rgba;
  rgba.red = args.red;
  rgba.green = args.green;
  rgba.blue = args.blue;
  rgba.alpha = args.alpha;
  gapi_->Clear(buffers, rgba, args.depth, args.stencil);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleSetViewport(
    uint32 arg_count,
    const SetViewport& args) {
  gapi_->SetViewport(args.left,
                     args.top,
                     args.width,
                     args.height,
                     args.z_min,
                     args.z_max);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleCreateVertexBuffer(
    uint32 arg_count,
    const CreateVertexBuffer& args) {
  return gapi_->CreateVertexBuffer(
      args.vertex_buffer_id, args.size, args.flags);
}

parse_error::ParseError GAPIDecoder::HandleDestroyVertexBuffer(
    uint32 arg_count,
    const DestroyVertexBuffer& args) {
  return gapi_->DestroyVertexBuffer(args.vertex_buffer_id);
}

parse_error::ParseError GAPIDecoder::HandleSetVertexBufferDataImmediate(
    uint32 arg_count,
    const SetVertexBufferDataImmediate& args) {
  uint32 size = ImmediateDataSize(arg_count, args);
  if (size == 0) {
    return parse_error::kParseNoError;
  }
  return gapi_->SetVertexBufferData(args.vertex_buffer_id, args.offset,
                                    size,
                                    AddressAfterStruct(args));
}

parse_error::ParseError GAPIDecoder::HandleSetVertexBufferData(
    uint32 arg_count,
    const SetVertexBufferData& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return parse_error::kParseInvalidArguments;
  return gapi_->SetVertexBufferData(
      args.vertex_buffer_id, args.offset, size, data);
}

parse_error::ParseError GAPIDecoder::HandleGetVertexBufferData(
    uint32 arg_count,
    const GetVertexBufferData& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return parse_error::kParseInvalidArguments;
  return gapi_->GetVertexBufferData(
      args.vertex_buffer_id, args.offset, size, data);
}

parse_error::ParseError GAPIDecoder::HandleCreateIndexBuffer(
    uint32 arg_count,
    const CreateIndexBuffer& args) {
  return gapi_->CreateIndexBuffer(args.index_buffer_id, args.size, args.flags);
}

parse_error::ParseError GAPIDecoder::HandleDestroyIndexBuffer(
    uint32 arg_count,
    const DestroyIndexBuffer& args) {
  return gapi_->DestroyIndexBuffer(args.index_buffer_id);
}

parse_error::ParseError GAPIDecoder::HandleSetIndexBufferDataImmediate(
    uint32 arg_count,
    const SetIndexBufferDataImmediate& args) {
  uint32 size = ImmediateDataSize(arg_count, args);
  if (size == 0) {
    return parse_error::kParseNoError;
  }
  return gapi_->SetIndexBufferData(args.index_buffer_id, args.offset, size,
                                   AddressAfterStruct(args));
}

parse_error::ParseError GAPIDecoder::HandleSetIndexBufferData(
    uint32 arg_count,
    const SetIndexBufferData& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return parse_error::kParseInvalidArguments;
  return gapi_->SetIndexBufferData(
      args.index_buffer_id, args.offset, size, data);
}

parse_error::ParseError GAPIDecoder::HandleGetIndexBufferData(
    uint32 arg_count,
    const GetIndexBufferData& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return parse_error::kParseInvalidArguments;
  return gapi_->GetIndexBufferData(
      args.index_buffer_id, args.offset, size, data);
}

parse_error::ParseError GAPIDecoder::HandleCreateVertexStruct(
    uint32 arg_count,
    const CreateVertexStruct& args) {
  return gapi_->CreateVertexStruct(args.vertex_struct_id, args.input_count);
}

parse_error::ParseError GAPIDecoder::HandleDestroyVertexStruct(
    uint32 arg_count,
    const DestroyVertexStruct& args) {
  return gapi_->DestroyVertexStruct(args.vertex_struct_id);
}

parse_error::ParseError GAPIDecoder::HandleSetVertexInput(
    uint32 arg_count,
    const SetVertexInput& args) {
  unsigned int type_stride_semantic = args.type_stride_semantic;
  unsigned int semantic_index =
      SetVertexInput::SemanticIndex::Get(type_stride_semantic);
  unsigned int semantic =
      SetVertexInput::Semantic::Get(type_stride_semantic);
  unsigned int type =
      SetVertexInput::Type::Get(type_stride_semantic);
  unsigned int stride =
      SetVertexInput::Stride::Get(type_stride_semantic);
  if (semantic >= vertex_struct::kNumSemantics ||
      type >= vertex_struct::kNumTypes || stride == 0)
    return parse_error::kParseInvalidArguments;
  return gapi_->SetVertexInput(
      args.vertex_struct_id, args.input_index, args.vertex_buffer_id,
      args.offset, stride,
      static_cast<vertex_struct::Type>(type),
      static_cast<vertex_struct::Semantic>(semantic),
      semantic_index);
}

parse_error::ParseError GAPIDecoder::HandleSetVertexStruct(
    uint32 arg_count,
    const SetVertexStruct& args) {
  return gapi_->SetVertexStruct(args.vertex_struct_id);
}

parse_error::ParseError GAPIDecoder::HandleDraw(
    uint32 arg_count,
    const Draw& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 primitive_type = args.primitive_type;
  if (primitive_type >= kMaxPrimitiveType)
    return parse_error::kParseInvalidArguments;
  return gapi_->Draw(
      static_cast<PrimitiveType>(primitive_type),
      args.first, args.count);
}

parse_error::ParseError GAPIDecoder::HandleDrawIndexed(
    uint32 arg_count,
    const DrawIndexed& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 primitive_type = args.primitive_type;
  if (primitive_type >= kMaxPrimitiveType)
    return parse_error::kParseInvalidArguments;
  return gapi_->DrawIndexed(
      static_cast<PrimitiveType>(primitive_type),
      args.index_buffer_id,
      args.first, args.count, args.min_index, args.max_index);
}

parse_error::ParseError GAPIDecoder::HandleCreateEffect(
    uint32 arg_count,
    const CreateEffect& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return parse_error::kParseInvalidArguments;
  return gapi_->CreateEffect(args.effect_id, size, data);
}

parse_error::ParseError GAPIDecoder::HandleCreateEffectImmediate(
    uint32 arg_count,
    const CreateEffectImmediate& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  uint32 data_size = ImmediateDataSize(arg_count, args);
  if (size > data_size) {
    return parse_error::kParseInvalidArguments;
  }
  if (data_size == 0) {
    return parse_error::kParseNoError;
  }
  return gapi_->CreateEffect(args.effect_id, size, AddressAfterStruct(args));
}

parse_error::ParseError GAPIDecoder::HandleDestroyEffect(
    uint32 arg_count,
    const DestroyEffect& args) {
  return gapi_->DestroyEffect(args.effect_id);
}

parse_error::ParseError GAPIDecoder::HandleSetEffect(
    uint32 arg_count,
    const SetEffect& args) {
  return gapi_->SetEffect(args.effect_id);
}

parse_error::ParseError GAPIDecoder::HandleGetParamCount(
    uint32 arg_count,
    const GetParamCount& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return parse_error::kParseInvalidArguments;
  return gapi_->GetParamCount(args.effect_id, size, data);
}

parse_error::ParseError GAPIDecoder::HandleCreateParam(
    uint32 arg_count,
    const CreateParam& args) {
  return gapi_->CreateParam(args.param_id, args.effect_id, args.index);
}

parse_error::ParseError GAPIDecoder::HandleCreateParamByName(
    uint32 arg_count,
    const CreateParamByName& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return parse_error::kParseInvalidArguments;
  return gapi_->CreateParamByName(args.param_id, args.effect_id, size,
                                  data);
}

parse_error::ParseError GAPIDecoder::HandleCreateParamByNameImmediate(
    uint32 arg_count,
    const CreateParamByNameImmediate& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  uint32 data_size = ImmediateDataSize(arg_count, args);
  if (size > data_size)
    return parse_error::kParseInvalidArguments;
  if (data_size == 0) {
    return parse_error::kParseNoError;
  }
  return gapi_->CreateParamByName(args.param_id, args.effect_id, size,
                                  AddressAfterStruct(args));
}

parse_error::ParseError GAPIDecoder::HandleDestroyParam(
    uint32 arg_count,
    const DestroyParam& args) {
  return gapi_->DestroyParam(args.param_id);
}

parse_error::ParseError GAPIDecoder::HandleSetParamData(
    uint32 arg_count,
    const SetParamData& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return parse_error::kParseInvalidArguments;
  return gapi_->SetParamData(args.param_id, size, data);
}

parse_error::ParseError GAPIDecoder::HandleSetParamDataImmediate(
    uint32 arg_count,
    const SetParamDataImmediate& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  uint32 data_size = ImmediateDataSize(arg_count, args);
  if (size > data_size) {
    return parse_error::kParseInvalidArguments;
  }
  if (data_size == 0) {
    return parse_error::kParseNoError;
  }
  return gapi_->SetParamData(args.param_id, size, AddressAfterStruct(args));
}

parse_error::ParseError GAPIDecoder::HandleGetParamDesc(
    uint32 arg_count,
    const GetParamDesc& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return parse_error::kParseInvalidArguments;
  return gapi_->GetParamDesc(args.param_id, size, data);
}

parse_error::ParseError GAPIDecoder::HandleGetStreamCount(
    uint32 arg_count,
    const GetStreamCount& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return parse_error::kParseInvalidArguments;
  return gapi_->GetStreamCount(args.effect_id, size, data);
}

parse_error::ParseError GAPIDecoder::HandleGetStreamDesc(
    uint32 arg_count,
    const GetStreamDesc& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return parse_error::kParseInvalidArguments;
  return gapi_->GetStreamDesc(args.effect_id, args.index, size, data);
}

parse_error::ParseError GAPIDecoder::HandleDestroyTexture(
    uint32 arg_count,
    const DestroyTexture& args) {
  return gapi_->DestroyTexture(args.texture_id);
}

parse_error::ParseError GAPIDecoder::HandleCreateTexture2d(
    uint32 arg_count,
    const CreateTexture2d& args) {
  unsigned int width_height = args.width_height;
  unsigned int levels_format_flags = args.levels_format_flags;
  unsigned int width = CreateTexture2d::Width::Get(width_height);
  unsigned int height = CreateTexture2d::Height::Get(width_height);
  unsigned int levels = CreateTexture2d::Levels::Get(levels_format_flags);
  unsigned int unused = CreateTexture2d::Unused::Get(levels_format_flags);
  unsigned int format = CreateTexture2d::Format::Get(levels_format_flags);
  unsigned int flags = CreateTexture2d::Flags::Get(levels_format_flags);
  unsigned int max_levels =
      1 + ::o3d::base::bits::Log2Ceiling(std::max(width, height));
  if ((width == 0) || (height == 0) || (levels > max_levels) ||
      (unused != 0) || (format == texture::kUnknown) || (levels == 0))
    return parse_error::kParseInvalidArguments;
  bool enable_render_surfaces = !!flags;
  return gapi_->CreateTexture2D(args.texture_id, width, height, levels,
                                static_cast<texture::Format>(format), flags,
                                enable_render_surfaces);
}

parse_error::ParseError GAPIDecoder::HandleCreateTexture3d(
    uint32 arg_count,
    const CreateTexture3d& args) {
  unsigned int width_height = args.width_height;
  unsigned int depth_unused = args.depth_unused;
  unsigned int levels_format_flags = args.levels_format_flags;
  unsigned int width = CreateTexture3d::Width::Get(width_height);
  unsigned int height = CreateTexture3d::Height::Get(width_height);
  unsigned int depth = CreateTexture3d::Depth::Get(depth_unused);
  unsigned int unused1 = CreateTexture3d::Unused1::Get(depth_unused);
  unsigned int levels = CreateTexture3d::Levels::Get(levels_format_flags);
  unsigned int unused2 = CreateTexture3d::Unused2::Get(levels_format_flags);
  unsigned int format = CreateTexture3d::Format::Get(levels_format_flags);
  unsigned int flags = CreateTexture3d::Flags::Get(levels_format_flags);
  unsigned int max_levels =
      1 + ::o3d::base::bits::Log2Ceiling(std::max(depth, std::max(width, height)));
  if ((width == 0) || (height == 0) || (depth == 0) ||
      (levels > max_levels) || (unused1 != 0) || (unused2 != 0) ||
      (format == texture::kUnknown) || (levels == 0))
    return parse_error::kParseInvalidArguments;
  bool enable_render_surfaces = !!flags;
  return gapi_->CreateTexture3D(args.texture_id, width, height, depth, levels,
                                static_cast<texture::Format>(format), flags,
                                enable_render_surfaces);
}

parse_error::ParseError GAPIDecoder::HandleCreateTextureCube(
    uint32 arg_count,
    const CreateTextureCube& args) {
  unsigned int side_unused = args.edge_length;
  unsigned int levels_format_flags = args.levels_format_flags;
  unsigned int side = CreateTextureCube::Side::Get(side_unused);
  unsigned int unused1 = CreateTextureCube::Unused1::Get(side_unused);
  unsigned int levels = CreateTextureCube::Levels::Get(levels_format_flags);
  unsigned int unused2 = CreateTextureCube::Unused2::Get(levels_format_flags);
  unsigned int format = CreateTextureCube::Format::Get(levels_format_flags);
  unsigned int flags = CreateTextureCube::Flags::Get(levels_format_flags);
  unsigned int max_levels = 1 + ::o3d::base::bits::Log2Ceiling(side);
  if ((side == 0) || (levels > max_levels) || (unused1 != 0) ||
      (unused2 != 0) || (format == texture::kUnknown) || (levels == 0))
    return parse_error::kParseInvalidArguments;
  bool enable_render_surfaces = !!flags;
  return gapi_->CreateTextureCube(args.texture_id, side, levels,
                                  static_cast<texture::Format>(format),
                                  flags, enable_render_surfaces);
}

parse_error::ParseError GAPIDecoder::HandleSetTextureData(
    uint32 arg_count,
    const SetTextureData& args) {
  unsigned int x_y = args.x_y;
  unsigned int width_height = args.width_height;
  unsigned int z_depth = args.z_depth;
  unsigned int level_face = args.level_face;
  unsigned int size = args.size;
  unsigned int x = SetTextureData::X::Get(x_y);
  unsigned int y = SetTextureData::Y::Get(x_y);
  unsigned int width = SetTextureData::Width::Get(width_height);
  unsigned int height = SetTextureData::Height::Get(width_height);
  unsigned int z = SetTextureData::Z::Get(z_depth);
  unsigned int depth = SetTextureData::Depth::Get(z_depth);
  unsigned int level = SetTextureData::Level::Get(level_face);
  unsigned int face = SetTextureData::Face::Get(level_face);
  unsigned int unused = SetTextureData::Unused::Get(level_face);
  const void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                            args.shared_memory.offset, size);
  if (face >= 6 || unused != 0 || !data)
    return parse_error::kParseInvalidArguments;
  return gapi_->SetTextureData(
      args.texture_id, x, y, z, width, height, depth, level,
      static_cast<texture::Face>(face), args.row_pitch,
      args.slice_pitch, size, data);
}

parse_error::ParseError GAPIDecoder::HandleSetTextureDataImmediate(
    uint32 arg_count,
    const SetTextureDataImmediate& args) {
  unsigned int x_y = args.x_y;
  unsigned int width_height = args.width_height;
  unsigned int z_depth = args.z_depth;
  unsigned int level_face = args.level_face;
  unsigned int size = args.size;
  unsigned int x = SetTextureDataImmediate::X::Get(x_y);
  unsigned int y = SetTextureDataImmediate::Y::Get(x_y);
  unsigned int width = SetTextureDataImmediate::Width::Get(width_height);
  unsigned int height = SetTextureDataImmediate::Height::Get(width_height);
  unsigned int z = SetTextureDataImmediate::Z::Get(z_depth);
  unsigned int depth = SetTextureDataImmediate::Depth::Get(z_depth);
  unsigned int level = SetTextureDataImmediate::Level::Get(level_face);
  unsigned int face = SetTextureDataImmediate::Face::Get(level_face);
  unsigned int unused = SetTextureDataImmediate::Unused::Get(level_face);
  uint32 data_size = ImmediateDataSize(arg_count, args);
  if (face >= 6 || unused != 0 ||
      size > data_size)
    return parse_error::kParseInvalidArguments;
  if (data_size == 0) {
    return parse_error::kParseNoError;
  }
  return gapi_->SetTextureData(
      args.texture_id, x, y, z, width, height, depth, level,
      static_cast<texture::Face>(face), args.row_pitch,
      args.slice_pitch, size, AddressAfterStruct(args));
}

parse_error::ParseError GAPIDecoder::HandleGetTextureData(
    uint32 arg_count,
    const GetTextureData& args) {
  unsigned int x_y = args.x_y;
  unsigned int width_height = args.width_height;
  unsigned int z_depth = args.z_depth;
  unsigned int level_face = args.level_face;
  unsigned int size = args.size;
  unsigned int x = GetTextureData::X::Get(x_y);
  unsigned int y = GetTextureData::Y::Get(x_y);
  unsigned int width = GetTextureData::Width::Get(width_height);
  unsigned int height = GetTextureData::Height::Get(width_height);
  unsigned int z = GetTextureData::Z::Get(z_depth);
  unsigned int depth = GetTextureData::Depth::Get(z_depth);
  unsigned int level = GetTextureData::Level::Get(level_face);
  unsigned int face = GetTextureData::Face::Get(level_face);
  unsigned int unused = GetTextureData::Unused::Get(level_face);
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset, size);
  if (face >= 6 || unused != 0 || !data)
    return parse_error::kParseInvalidArguments;
  return gapi_->GetTextureData(
      args.texture_id, x, y, z, width, height, depth, level,
      static_cast<texture::Face>(face), args.row_pitch,
      args.slice_pitch, size, data);
}

parse_error::ParseError GAPIDecoder::HandleCreateSampler(
    uint32 arg_count,
    const CreateSampler& args) {
  return gapi_->CreateSampler(args.sampler_id);
}

parse_error::ParseError GAPIDecoder::HandleDestroySampler(
    uint32 arg_count,
    const DestroySampler& args) {
  return gapi_->DestroySampler(args.sampler_id);
}

parse_error::ParseError GAPIDecoder::HandleSetSamplerStates(
    uint32 arg_count,
    const SetSamplerStates& args) {
  Uint32 arg = args.sampler_states;
  if (SetSamplerStates::Unused::Get(arg) != 0)
    return parse_error::kParseInvalidArguments;
  unsigned int address_u_value = SetSamplerStates::AddressingU::Get(arg);
  unsigned int address_v_value = SetSamplerStates::AddressingV::Get(arg);
  unsigned int address_w_value = SetSamplerStates::AddressingW::Get(arg);
  unsigned int mag_filter_value = SetSamplerStates::MagFilter::Get(arg);
  unsigned int min_filter_value = SetSamplerStates::MinFilter::Get(arg);
  unsigned int mip_filter_value = SetSamplerStates::MipFilter::Get(arg);
  unsigned int max_anisotropy = SetSamplerStates::MaxAnisotropy::Get(arg);
  if (address_u_value >= sampler::kNumAddressingMode ||
      address_v_value >= sampler::kNumAddressingMode ||
      address_w_value >= sampler::kNumAddressingMode ||
      mag_filter_value >= sampler::kNumFilteringMode ||
      min_filter_value >= sampler::kNumFilteringMode ||
      mip_filter_value >= sampler::kNumFilteringMode ||
      mag_filter_value == sampler::kNone ||
      min_filter_value == sampler::kNone ||
      max_anisotropy == 0) {
    return parse_error::kParseInvalidArguments;
  }
  gapi_->SetSamplerStates(
      args.sampler_id,
      static_cast<sampler::AddressingMode>(address_u_value),
      static_cast<sampler::AddressingMode>(address_v_value),
      static_cast<sampler::AddressingMode>(address_w_value),
      static_cast<sampler::FilteringMode>(mag_filter_value),
      static_cast<sampler::FilteringMode>(min_filter_value),
      static_cast<sampler::FilteringMode>(mip_filter_value),
      max_anisotropy);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleSetSamplerBorderColor(
    uint32 arg_count,
    const SetSamplerBorderColor& args) {
  RGBA rgba;
  rgba.red = args.red;
  rgba.green = args.green;
  rgba.blue = args.blue;
  rgba.alpha = args.alpha;
  return gapi_->SetSamplerBorderColor(args.sampler_id, rgba);
}

parse_error::ParseError GAPIDecoder::HandleSetSamplerTexture(
    uint32 arg_count,
    const SetSamplerTexture& args) {
  return gapi_->SetSamplerTexture(args.sampler_id, args.texture_id);
}

parse_error::ParseError GAPIDecoder::HandleSetScissor(
    uint32 arg_count,
    const SetScissor& args) {
  Uint32 x_y_enable = args.x_y_enable;
  if (SetScissor::Unused::Get(x_y_enable) != 0)
    return parse_error::kParseInvalidArguments;
  unsigned int x = SetScissor::X::Get(x_y_enable);
  unsigned int y = SetScissor::Y::Get(x_y_enable);
  bool enable = SetScissor::Enable::Get(x_y_enable) != 0;
  Uint32 width_height = args.width_height;
  unsigned int width = SetScissor::Width::Get(width_height);
  unsigned int height = SetScissor::Height::Get(width_height);
  gapi_->SetScissor(enable, x, y, width, height);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleSetPolygonOffset(
    uint32 arg_count,
    const SetPolygonOffset& args) {
  gapi_->SetPolygonOffset(args.slope_factor, args.units);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleSetPointLineRaster(
    uint32 arg_count,
    const SetPointLineRaster& args) {
  Uint32 enables = args.enables;
  if (SetPointLineRaster::Unused::Get(enables) != 0)
    return parse_error::kParseInvalidArguments;
  bool line_smooth = !!SetPointLineRaster::LineSmoothEnable::Get(enables);
  bool point_sprite = !!SetPointLineRaster::PointSpriteEnable::Get(enables);
  gapi_->SetPointLineRaster(line_smooth, point_sprite, args.point_size);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleSetPolygonRaster(
    uint32 arg_count,
    const SetPolygonRaster& args) {
  Uint32 fill_cull = args.fill_cull;
  unsigned int fill_value = SetPolygonRaster::FillMode::Get(fill_cull);
  unsigned int cull_value = SetPolygonRaster::CullMode::Get(fill_cull);
  if (SetPolygonRaster::Unused::Get(fill_cull) != 0 ||
      fill_value >= kNumPolygonMode ||
      cull_value >= kNumFaceCullMode)
    return parse_error::kParseInvalidArguments;
  gapi_->SetPolygonRaster(
      static_cast<PolygonMode>(fill_value),
      static_cast<FaceCullMode>(cull_value));
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleSetAlphaTest(
    uint32 arg_count,
    const SetAlphaTest& args) {
  Uint32 func_enable = args.func_enable;
  if (SetAlphaTest::Unused::Get(func_enable) != 0)
    return parse_error::kParseInvalidArguments;
  // Check that the bitmask get cannot generate values outside of the
  // allowed range.
  COMPILE_ASSERT(SetAlphaTest::Func::kMask <
                 kNumComparison,
                 set_alpha_test_Func_may_produce_invalid_values);
  Comparison comp = static_cast<Comparison>(
      SetAlphaTest::Func::Get(func_enable));
  bool enable = SetAlphaTest::Enable::Get(func_enable) != 0;
  gapi_->SetAlphaTest(enable, args.value, comp);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleSetDepthTest(
    uint32 arg_count,
    const SetDepthTest& args) {
  Uint32 func_enable = args.func_enable;
  if (SetDepthTest::Unused::Get(func_enable) != 0)
    return parse_error::kParseInvalidArguments;
  // Check that the bitmask get cannot generate values outside of the
  // allowed range.
  COMPILE_ASSERT(SetDepthTest::Func::kMask <
                 kNumComparison,
                 set_alpha_test_Func_may_produce_invalid_values);
  Comparison comp = static_cast<Comparison>(
      SetDepthTest::Func::Get(func_enable));
  bool write_enable = SetDepthTest::WriteEnable::Get(func_enable) != 0;
  bool enable = SetDepthTest::Enable::Get(func_enable) != 0;
  gapi_->SetDepthTest(enable, write_enable, comp);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleSetStencilTest(
    uint32 arg_count,
    const SetStencilTest& args) {
  Uint32 arg0 = args.stencil_args0;
  Uint32 arg1 = args.stencil_args1;
  if (SetStencilTest::Unused0::Get(arg0) != 0 ||
      SetStencilTest::Unused1::Get(arg1) != 0 ||
      SetStencilTest::Unused2::Get(arg1) != 0)
    return parse_error::kParseInvalidArguments;
  unsigned int write_mask = SetStencilTest::WriteMask::Get(arg0);
  unsigned int compare_mask = SetStencilTest::CompareMask::Get(arg0);
  unsigned int ref = SetStencilTest::ReferenceValue::Get(arg0);
  bool enable = SetStencilTest::Enable::Get(arg0) != 0;
  bool separate_ccw = SetStencilTest::SeparateCCW::Get(arg0) != 0;
  gapi_->SetStencilTest(enable, separate_ccw, write_mask, compare_mask, ref,
                        arg1);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleSetColorWrite(
    uint32 arg_count,
    const SetColorWrite& args) {
  Uint32 enables = args.flags;
  if (SetColorWrite::Unused::Get(enables) != 0)
    return parse_error::kParseInvalidArguments;
  bool red = SetColorWrite::RedMask::Get(enables) != 0;
  bool green = SetColorWrite::GreenMask::Get(enables) != 0;
  bool blue = SetColorWrite::BlueMask::Get(enables) != 0;
  bool alpha = SetColorWrite::AlphaMask::Get(enables) != 0;
  bool dither = SetColorWrite::DitherEnable::Get(enables) != 0;
  gapi_->SetColorWrite(red, green, blue, alpha, dither);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleSetBlending(
    uint32 arg_count,
    const SetBlending& args) {
  Uint32 arg = args.blend_settings;
  bool enable = SetBlending::Enable::Get(arg) != 0;
  bool separate_alpha = SetBlending::SeparateAlpha::Get(arg) != 0;
  unsigned int color_eq = SetBlending::ColorEq::Get(arg);
  unsigned int color_src = SetBlending::ColorSrcFunc::Get(arg);
  unsigned int color_dst = SetBlending::ColorDstFunc::Get(arg);
  unsigned int alpha_eq = SetBlending::AlphaEq::Get(arg);
  unsigned int alpha_src = SetBlending::AlphaSrcFunc::Get(arg);
  unsigned int alpha_dst = SetBlending::AlphaDstFunc::Get(arg);
  if (SetBlending::Unused0::Get(arg) != 0 ||
      SetBlending::Unused1::Get(arg) != 0 ||
      color_eq >= kNumBlendEq ||
      color_src >= kNumBlendFunc ||
      color_dst >= kNumBlendFunc ||
      alpha_eq >= kNumBlendEq ||
      alpha_src >= kNumBlendFunc ||
      alpha_dst >= kNumBlendFunc)
    return parse_error::kParseInvalidArguments;
  gapi_->SetBlending(enable,
                     separate_alpha,
                     static_cast<BlendEq>(color_eq),
                     static_cast<BlendFunc>(color_src),
                     static_cast<BlendFunc>(color_dst),
                     static_cast<BlendEq>(alpha_eq),
                     static_cast<BlendFunc>(alpha_src),
                     static_cast<BlendFunc>(alpha_dst));
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleSetBlendingColor(
    uint32 arg_count,
    const SetBlendingColor& args) {
  RGBA rgba;
  rgba.red = args.red;
  rgba.green = args.green;
  rgba.blue = args.blue;
  rgba.alpha = args.alpha;
  gapi_->SetBlendingColor(rgba);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIDecoder::HandleCreateRenderSurface(
    uint32 arg_count,
    const CreateRenderSurface& args) {
  unsigned int width_height = args.width_height;
  unsigned int width = CreateRenderSurface::Width::Get(width_height);
  unsigned int height = CreateRenderSurface::Height::Get(width_height);
  unsigned int levels_side = args.levels_side;
  unsigned int mip_level = CreateRenderSurface::Levels::Get(levels_side);
  unsigned int side = CreateRenderSurface::Side::Get(levels_side);
  return gapi_->CreateRenderSurface(args.render_surface_id,
                                    width, height, mip_level,
                                    side, args.texture_id);
}

parse_error::ParseError GAPIDecoder::HandleDestroyRenderSurface(
    uint32 arg_count,
    const DestroyRenderSurface& args) {
  return gapi_->DestroyRenderSurface(args.render_surface_id);
}

parse_error::ParseError GAPIDecoder::HandleCreateDepthSurface(
    uint32 arg_count,
    const CreateDepthSurface& args) {
  unsigned int width_height = args.width_height;
  unsigned int width = CreateDepthSurface::Width::Get(width_height);
  unsigned int height = CreateDepthSurface::Height::Get(width_height);
  return gapi_->CreateDepthSurface(args.depth_surface_id, width, height);
}

parse_error::ParseError GAPIDecoder::HandleDestroyDepthSurface(
    uint32 arg_count,
    const DestroyDepthSurface& args) {
  return gapi_->DestroyDepthSurface(args.depth_surface_id);
}

parse_error::ParseError GAPIDecoder::HandleSetRenderSurface(
    uint32 arg_count,
    const SetRenderSurface& args) {
  return gapi_->SetRenderSurface(args.render_surface_id, args.depth_surface_id);
}

parse_error::ParseError GAPIDecoder::HandleSetBackSurfaces(
    uint32 arg_count,
    const SetBackSurfaces& args) {
  gapi_->SetBackSurfaces();
  return  parse_error::kParseNoError;
}

}  // namespace o3d
}  // namespace command_buffer
