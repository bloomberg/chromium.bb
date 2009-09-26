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
  #define O3D_COMMAND_BUFFER_CMD_OP(name) {                              \
    cmd::name::kArgFlags,                                                \
    sizeof(cmd::name) / sizeof(CommandBufferEntry) - 1, },  /* NOLINT */ \

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
    const void* cmd_data) {
  if (command < arraysize(g_command_info)) {
    const CommandInfo& info = g_command_info[command];
  unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
    if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
        (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
      switch (command) {
        #define O3D_COMMAND_BUFFER_CMD_OP(name)                    \
          case cmd::name::kCmdId:                                  \
            return Handle ## name(                                 \
                arg_count,                                         \
                *static_cast<const cmd::name*>(cmd_data));         \

        O3D_COMMAND_BUFFER_CMDS

        #undef O3D_COMMAND_BUFFER_CMD_OP
      }
    } else {
      return BufferSyncInterface::kParseInvalidArguments;
    }
  }
  return BufferSyncInterface::kParseUnknownCommand;
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

BufferSyncInterface::ParseError GAPIDecoder::HandleNoop(
    uint32 arg_count,
    const cmd::Noop& args) {
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetToken(
    uint32 arg_count,
    const cmd::SetToken& args) {
  engine_->set_token(args.token);
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleBeginFrame(
    uint32 arg_count,
    const cmd::BeginFrame& args) {
  gapi_->BeginFrame();
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleEndFrame(
    uint32 arg_count,
    const cmd::EndFrame& args) {
  gapi_->EndFrame();
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleClear(
    uint32 arg_count,
    const cmd::Clear& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 buffers = args.buffers;
  if (buffers & ~GAPIInterface::ALL_BUFFERS)
    return BufferSyncInterface::kParseInvalidArguments;
  RGBA rgba;
  rgba.red = args.red;
  rgba.green = args.green;
  rgba.blue = args.blue;
  rgba.alpha = args.alpha;
  gapi_->Clear(buffers, rgba, args.depth, args.stencil);
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetViewport(
    uint32 arg_count,
    const cmd::SetViewport& args) {
  gapi_->SetViewport(args.left,
                     args.top,
                     args.width,
                     args.height,
                     args.z_min,
                     args.z_max);
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleCreateVertexBuffer(
    uint32 arg_count,
    const cmd::CreateVertexBuffer& args) {
  return gapi_->CreateVertexBuffer(args.id, args.size, args.flags);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleDestroyVertexBuffer(
    uint32 arg_count,
    const cmd::DestroyVertexBuffer& args) {
  return gapi_->DestroyVertexBuffer(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetVertexBufferDataImmediate(
    uint32 arg_count,
    const cmd::SetVertexBufferDataImmediate& args) {
  uint32 size = ImmediateDataSize(arg_count, args);
  if (size == 0) {
    return ParseError::kParseNoError;
  }
  return gapi_->SetVertexBufferData(args.id, args.offset,
                                    size,
                                    AddressAfterStruct(args));
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetVertexBufferData(
    uint32 arg_count,
    const cmd::SetVertexBufferData& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->SetVertexBufferData(args.id, args.offset, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleGetVertexBufferData(
    uint32 arg_count,
    const cmd::GetVertexBufferData& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->GetVertexBufferData(args.id, args.offset, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleCreateIndexBuffer(
    uint32 arg_count,
    const cmd::CreateIndexBuffer& args) {
  return gapi_->CreateIndexBuffer(args.id, args.size, args.flags);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleDestroyIndexBuffer(
    uint32 arg_count,
    const cmd::DestroyIndexBuffer& args) {
  return gapi_->DestroyIndexBuffer(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetIndexBufferDataImmediate(
    uint32 arg_count,
    const cmd::SetIndexBufferDataImmediate& args) {
  uint32 size = ImmediateDataSize(arg_count, args);
  if (size == 0) {
    return ParseError::kParseNoError;
  }
  return gapi_->SetIndexBufferData(args.id, args.offset, size,
                                   AddressAfterStruct(args));
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetIndexBufferData(
    uint32 arg_count,
    const cmd::SetIndexBufferData& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->SetIndexBufferData(args.id, args.offset, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleGetIndexBufferData(
    uint32 arg_count,
    const cmd::GetIndexBufferData& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->GetIndexBufferData(args.id, args.offset, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleCreateVertexStruct(
    uint32 arg_count,
    const cmd::CreateVertexStruct& args) {
  return gapi_->CreateVertexStruct(args.id, args.input_count);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleDestroyVertexStruct(
    uint32 arg_count,
    const cmd::DestroyVertexStruct& args) {
  return gapi_->DestroyVertexStruct(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetVertexInput(
    uint32 arg_count,
    const cmd::SetVertexInput& args) {
  namespace cmd = set_vertex_input_cmd;
  unsigned int type_stride_semantic = args.fixme4;
  unsigned int semantic_index = cmd::SemanticIndex::Get(type_stride_semantic);
  unsigned int semantic = cmd::Semantic::Get(type_stride_semantic);
  unsigned int type = cmd::Type::Get(type_stride_semantic);
  unsigned int stride = cmd::Stride::Get(type_stride_semantic);
  if (semantic >= vertex_struct::NUM_SEMANTICS ||
      type >= vertex_struct::NUM_TYPES || stride == 0)
    return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->SetVertexInput(
      args.vertex_struct_id, args.input_index, args.vertex_buffer_id,
      args.offset, stride,
      static_cast<vertex_struct::Type>(type),
      static_cast<vertex_struct::Semantic>(semantic),
      semantic_index);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetVertexStruct(
    uint32 arg_count,
    const cmd::SetVertexStruct& args) {
  return gapi_->SetVertexStruct(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleDraw(
    uint32 arg_count,
    const cmd::Draw& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 primitive_type = args.primitive_type;
  if (primitive_type >= GAPIInterface::MAX_PRIMITIVE_TYPE)
    return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->Draw(
      static_cast<GAPIInterface::PrimitiveType>(primitive_type),
      args.first, args.count);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleDrawIndexed(
    uint32 arg_count,
    const cmd::DrawIndexed& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 primitive_type = args.primitive_type;
  if (primitive_type >= GAPIInterface::MAX_PRIMITIVE_TYPE)
    return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->DrawIndexed(
      static_cast<GAPIInterface::PrimitiveType>(primitive_type),
      args.index_buffer_id,
      args.first, args.count, args.min_index, args.max_index);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleCreateEffect(
    uint32 arg_count,
    const cmd::CreateEffect& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->CreateEffect(args.id, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleCreateEffectImmediate(
    uint32 arg_count,
    const cmd::CreateEffectImmediate& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  uint32 data_size = ImmediateDataSize(arg_count, args);
  if (size > data_size)
    return BufferSyncInterface::kParseInvalidArguments;
  if (data_size == 0) {
    return ParseError::kParseNoError;
  }
  return gapi_->CreateEffect(args.id, size, AddressAfterStruct(args));
}

BufferSyncInterface::ParseError GAPIDecoder::HandleDestroyEffect(
    uint32 arg_count,
    const cmd::DestroyEffect& args) {
  return gapi_->DestroyEffect(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetEffect(
    uint32 arg_count,
    const cmd::SetEffect& args) {
  return gapi_->SetEffect(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleGetParamCount(
    uint32 arg_count,
    const cmd::GetParamCount& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->GetParamCount(args.id, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleCreateParam(
    uint32 arg_count,
    const cmd::CreateParam& args) {
  return gapi_->CreateParam(args.param_id, args.effect_id, args.index);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleCreateParamByName(
    uint32 arg_count,
    const cmd::CreateParamByName& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->CreateParamByName(args.param_id, args.effect_id, size,
                                  data);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleCreateParamByNameImmediate(
    uint32 arg_count,
    const cmd::CreateParamByNameImmediate& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  uint32 data_size = ImmediateDataSize(arg_count, args);
  if (size > data_size)
    return BufferSyncInterface::kParseInvalidArguments;
  if (data_size == 0) {
    return ParseError::kParseNoError;
  }
  return gapi_->CreateParamByName(args.param_id, args.effect_id, size,
                                  AddressAfterStruct(args));
}

BufferSyncInterface::ParseError GAPIDecoder::HandleDestroyParam(
    uint32 arg_count,
    const cmd::DestroyParam& args) {
  return gapi_->DestroyParam(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetParamData(
    uint32 arg_count,
    const cmd::SetParamData& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->SetParamData(args.id, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetParamDataImmediate(
    uint32 arg_count,
    const cmd::SetParamDataImmediate& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  uint32 data_size = ImmediateDataSize(arg_count, args);
  if (size > data_size)
    return BufferSyncInterface::kParseInvalidArguments;
  if (data_size == 0) {
    return ParseError::kParseNoError;
  }
  return gapi_->SetParamData(args.id, size, AddressAfterStruct(args));
}

BufferSyncInterface::ParseError GAPIDecoder::HandleGetParamDesc(
    uint32 arg_count,
    const cmd::GetParamDesc& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->GetParamDesc(args.id, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleGetStreamCount(
    uint32 arg_count,
    const cmd::GetStreamCount& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->GetStreamCount(args.id, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleGetStreamDesc(
    uint32 arg_count,
    const cmd::GetStreamDesc& args) {
  // Pull out some values so they can't be changed by another thread after we've
  // validated them.
  uint32 size = args.size;
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset,
                                      size);
  if (!data) return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->GetStreamDesc(args.id, args.index, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleDestroyTexture(
    uint32 arg_count,
    const cmd::DestroyTexture& args) {
  return gapi_->DestroyTexture(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleCreateTexture2d(
    uint32 arg_count,
    const cmd::CreateTexture2d& args) {
  namespace cmd = create_texture_2d_cmd;
  unsigned int width_height = args.fixme1;
  unsigned int levels_format_flags = args.fixme2;
  unsigned int width = cmd::Width::Get(width_height);
  unsigned int height = cmd::Height::Get(width_height);
  unsigned int levels = cmd::Levels::Get(levels_format_flags);
  unsigned int unused = cmd::Unused::Get(levels_format_flags);
  unsigned int format = cmd::Format::Get(levels_format_flags);
  unsigned int flags = cmd::Flags::Get(levels_format_flags);
  unsigned int max_levels =
      1 + base::bits::Log2Ceiling(std::max(width, height));
  if ((width == 0) || (height == 0) || (levels > max_levels) ||
      (unused != 0) || (format >= texture::NUM_FORMATS) || (levels == 0))
    return BufferSyncInterface::kParseInvalidArguments;
  bool enable_render_surfaces = !!flags;
  return gapi_->CreateTexture2D(args.texture_id, width, height, levels,
                                static_cast<texture::Format>(format), flags,
                                enable_render_surfaces);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleCreateTexture3d(
    uint32 arg_count,
    const cmd::CreateTexture3d& args) {
  namespace cmd = create_texture_3d_cmd;
  unsigned int width_height = args.fixme1;
  unsigned int depth_unused = args.fixme2;
  unsigned int levels_format_flags = args.fixme3;
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
      (format >= texture::NUM_FORMATS) || (levels == 0))
    return BufferSyncInterface::kParseInvalidArguments;
  bool enable_render_surfaces = !!flags;
  return gapi_->CreateTexture3D(args.texture_id, width, height, depth, levels,
                                static_cast<texture::Format>(format), flags,
                                enable_render_surfaces);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleCreateTextureCube(
    uint32 arg_count,
    const cmd::CreateTextureCube& args) {
  namespace cmd = create_texture_cube_cmd;
  unsigned int side_unused = args.edge_length;
  unsigned int levels_format_flags = args.fixme2;
  unsigned int side = cmd::Side::Get(side_unused);
  unsigned int unused1 = cmd::Unused1::Get(side_unused);
  unsigned int levels = cmd::Levels::Get(levels_format_flags);
  unsigned int unused2 = cmd::Unused2::Get(levels_format_flags);
  unsigned int format = cmd::Format::Get(levels_format_flags);
  unsigned int flags = cmd::Flags::Get(levels_format_flags);
  unsigned int max_levels = 1 + base::bits::Log2Ceiling(side);
  if ((side == 0) || (levels > max_levels) || (unused1 != 0) ||
      (unused2 != 0) || (format >= texture::NUM_FORMATS) || (levels == 0))
    return BufferSyncInterface::kParseInvalidArguments;
  bool enable_render_surfaces = !!flags;
  return gapi_->CreateTextureCube(args.texture_id, side, levels,
                                  static_cast<texture::Format>(format),
                                  flags, enable_render_surfaces);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetTextureData(
    uint32 arg_count,
    const cmd::SetTextureData& args) {
  namespace cmd = set_texture_data_cmd;
  unsigned int x_y = args.fixme1;
  unsigned int width_height = args.fixme2;
  unsigned int z_depth = args.fixme3;
  unsigned int level_face = args.fixme4;
  unsigned int size = args.size;
  unsigned int x = cmd::X::Get(x_y);
  unsigned int y = cmd::Y::Get(x_y);
  unsigned int width = cmd::Width::Get(width_height);
  unsigned int height = cmd::Height::Get(width_height);
  unsigned int z = cmd::Z::Get(z_depth);
  unsigned int depth = cmd::Depth::Get(z_depth);
  unsigned int level = cmd::Level::Get(level_face);
  unsigned int face = cmd::Face::Get(level_face);
  unsigned int unused = cmd::Unused::Get(level_face);
  const void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                            args.shared_memory.offset, size);
  if (face >= 6 || unused != 0 || !data)
    return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->SetTextureData(
      args.texture_id, x, y, z, width, height, depth, level,
      static_cast<texture::Face>(face), args.row_pitch,
      args.slice_pitch, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetTextureDataImmediate(
    uint32 arg_count,
    const cmd::SetTextureDataImmediate& args) {
  namespace cmd = set_texture_data_immediate_cmd;
  unsigned int x_y = args.fixme1;
  unsigned int width_height = args.fixme2;
  unsigned int z_depth = args.fixme3;
  unsigned int level_face = args.fixme4;
  unsigned int size = args.size;
  unsigned int x = cmd::X::Get(x_y);
  unsigned int y = cmd::Y::Get(x_y);
  unsigned int width = cmd::Width::Get(width_height);
  unsigned int height = cmd::Height::Get(width_height);
  unsigned int z = cmd::Z::Get(z_depth);
  unsigned int depth = cmd::Depth::Get(z_depth);
  unsigned int level = cmd::Level::Get(level_face);
  unsigned int face = cmd::Face::Get(level_face);
  unsigned int unused = cmd::Unused::Get(level_face);
  uint32 data_size = ImmediateDataSize(arg_count, args);
  if (face >= 6 || unused != 0 ||
      size > data_size)
    return BufferSyncInterface::kParseInvalidArguments;
  if (data_size == 0) {
    return ParseError::kParseNoError;
  }
  return gapi_->SetTextureData(
      args.texture_id, x, y, z, width, height, depth, level,
      static_cast<texture::Face>(face), args.row_pitch,
      args.slice_pitch, size, AddressAfterStruct(args));
}

BufferSyncInterface::ParseError GAPIDecoder::HandleGetTextureData(
    uint32 arg_count,
    const cmd::GetTextureData& args) {
  namespace cmd = get_texture_data_cmd;
  unsigned int x_y = args.fixme1;
  unsigned int width_height = args.fixme2;
  unsigned int z_depth = args.fixme3;
  unsigned int level_face = args.fixme4;
  unsigned int size = args.size;
  unsigned int x = cmd::X::Get(x_y);
  unsigned int y = cmd::Y::Get(x_y);
  unsigned int width = cmd::Width::Get(width_height);
  unsigned int height = cmd::Height::Get(width_height);
  unsigned int z = cmd::Z::Get(z_depth);
  unsigned int depth = cmd::Depth::Get(z_depth);
  unsigned int level = cmd::Level::Get(level_face);
  unsigned int face = cmd::Face::Get(level_face);
  unsigned int unused = cmd::Unused::Get(level_face);
  void *data = GetAddressAndCheckSize(args.shared_memory.id,
                                      args.shared_memory.offset, size);
  if (face >= 6 || unused != 0 || !data)
    return BufferSyncInterface::kParseInvalidArguments;
  return gapi_->GetTextureData(
      args.texture_id, x, y, z, width, height, depth, level,
      static_cast<texture::Face>(face), args.row_pitch,
      args.slice_pitch, size, data);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleCreateSampler(
    uint32 arg_count,
    const cmd::CreateSampler& args) {
  return gapi_->CreateSampler(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleDestroySampler(
    uint32 arg_count,
    const cmd::DestroySampler& args) {
  return gapi_->DestroySampler(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetSamplerStates(
    uint32 arg_count,
    const cmd::SetSamplerStates& args) {
  namespace cmd = set_sampler_states;
  Uint32 arg = args.fixme1;
  if (cmd::Unused::Get(arg) != 0)
    return BufferSyncInterface::kParseInvalidArguments;
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
    return BufferSyncInterface::kParseInvalidArguments;
  }
  gapi_->SetSamplerStates(
      args.id,
      static_cast<sampler::AddressingMode>(address_u_value),
      static_cast<sampler::AddressingMode>(address_v_value),
      static_cast<sampler::AddressingMode>(address_w_value),
      static_cast<sampler::FilteringMode>(mag_filter_value),
      static_cast<sampler::FilteringMode>(min_filter_value),
      static_cast<sampler::FilteringMode>(mip_filter_value),
      max_anisotropy);
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetSamplerBorderColor(
    uint32 arg_count,
    const cmd::SetSamplerBorderColor& args) {
  RGBA rgba;
  rgba.red = args.red;
  rgba.green = args.green;
  rgba.blue = args.blue;
  rgba.alpha = args.alpha;
  return gapi_->SetSamplerBorderColor(args.id, rgba);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetSamplerTexture(
    uint32 arg_count,
    const cmd::SetSamplerTexture& args) {
  return gapi_->SetSamplerTexture(args.id, args.texture_id);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetScissor(
    uint32 arg_count,
    const cmd::SetScissor& args) {
  namespace cmd = set_scissor;
  Uint32 x_y_enable = args.fixme0;
  if (cmd::Unused::Get(x_y_enable) != 0)
    return BufferSyncInterface::kParseInvalidArguments;
  unsigned int x = cmd::X::Get(x_y_enable);
  unsigned int y = cmd::Y::Get(x_y_enable);
  bool enable = cmd::Enable::Get(x_y_enable) != 0;
  Uint32 width_height = args.fixme1;
  unsigned int width = cmd::Width::Get(width_height);
  unsigned int height = cmd::Height::Get(width_height);
  gapi_->SetScissor(enable, x, y, width, height);
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetPolygonOffset(
    uint32 arg_count,
    const cmd::SetPolygonOffset& args) {
  gapi_->SetPolygonOffset(args.slope_factor, args.units);
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetPointLineRaster(
    uint32 arg_count,
    const cmd::SetPointLineRaster& args) {
  namespace cmd = set_point_line_raster;
  Uint32 enables = args.fixme0;
  if (cmd::Unused::Get(enables) != 0)
    return BufferSyncInterface::kParseInvalidArguments;
  bool line_smooth = !!cmd::LineSmoothEnable::Get(enables);
  bool point_sprite = !!cmd::PointSpriteEnable::Get(enables);
  gapi_->SetPointLineRaster(line_smooth, point_sprite, args.point_size);
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetPolygonRaster(
    uint32 arg_count,
    const cmd::SetPolygonRaster& args) {
  namespace cmd = set_polygon_raster;
  Uint32 fill_cull = args.fixme0;
  unsigned int fill_value = cmd::FillMode::Get(fill_cull);
  unsigned int cull_value = cmd::CullMode::Get(fill_cull);
  if (cmd::Unused::Get(fill_cull) != 0 ||
      fill_value >= GAPIInterface::NUM_POLYGON_MODE ||
      cull_value >= GAPIInterface::NUM_FACE_CULL_MODE)
    return BufferSyncInterface::kParseInvalidArguments;
  gapi_->SetPolygonRaster(
      static_cast<GAPIInterface::PolygonMode>(fill_value),
      static_cast<GAPIInterface::FaceCullMode>(cull_value));
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetAlphaTest(
    uint32 arg_count,
    const cmd::SetAlphaTest& args) {
  namespace cmd = set_alpha_test;
  Uint32 func_enable = args.fixme0;
  if (cmd::Unused::Get(func_enable) != 0)
    return BufferSyncInterface::kParseInvalidArguments;
  // Check that the bitmask get cannot generate values outside of the
  // allowed range.
  COMPILE_ASSERT(cmd::Func::kMask < GAPIInterface::NUM_COMPARISON,
                 set_alpha_test_Func_may_produce_invalid_values);
  GAPIInterface::Comparison comp =
      static_cast<GAPIInterface::Comparison>(cmd::Func::Get(func_enable));
  bool enable = cmd::Enable::Get(func_enable) != 0;
  gapi_->SetAlphaTest(enable, args.value, comp);
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetDepthTest(
    uint32 arg_count,
    const cmd::SetDepthTest& args) {
  namespace cmd = set_depth_test;
  Uint32 func_enable = args.fixme0;
  if (cmd::Unused::Get(func_enable) != 0)
    return BufferSyncInterface::kParseInvalidArguments;
  // Check that the bitmask get cannot generate values outside of the
  // allowed range.
  COMPILE_ASSERT(cmd::Func::kMask < GAPIInterface::NUM_COMPARISON,
                 set_alpha_test_Func_may_produce_invalid_values);
  GAPIInterface::Comparison comp =
      static_cast<GAPIInterface::Comparison>(cmd::Func::Get(func_enable));
  bool write_enable = cmd::WriteEnable::Get(func_enable) != 0;
  bool enable = cmd::Enable::Get(func_enable) != 0;
  gapi_->SetDepthTest(enable, write_enable, comp);
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetStencilTest(
    uint32 arg_count,
    const cmd::SetStencilTest& args) {
  namespace cmd = set_stencil_test;
  Uint32 arg0 = args.fixme0;
  Uint32 arg1 = args.fixme1;
  if (cmd::Unused0::Get(arg0) != 0 ||
      cmd::Unused1::Get(arg1) != 0 ||
      cmd::Unused2::Get(arg1) != 0)
    return BufferSyncInterface::kParseInvalidArguments;
  unsigned int write_mask = cmd::WriteMask::Get(arg0);
  unsigned int compare_mask = cmd::CompareMask::Get(arg0);
  unsigned int ref = cmd::ReferenceValue::Get(arg0);
  bool enable = cmd::Enable::Get(arg0) != 0;
  bool separate_ccw = cmd::SeparateCCW::Get(arg0) != 0;
  gapi_->SetStencilTest(enable, separate_ccw, write_mask, compare_mask, ref,
                        arg1);
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetColorWrite(
    uint32 arg_count,
    const cmd::SetColorWrite& args) {
  namespace cmd = set_color_write;
  Uint32 enables = args.flags;
  if (cmd::Unused::Get(enables) != 0)
    return BufferSyncInterface::kParseInvalidArguments;
  bool red = cmd::RedMask::Get(enables) != 0;
  bool green = cmd::GreenMask::Get(enables) != 0;
  bool blue = cmd::BlueMask::Get(enables) != 0;
  bool alpha = cmd::AlphaMask::Get(enables) != 0;
  bool dither = cmd::DitherEnable::Get(enables) != 0;
  gapi_->SetColorWrite(red, green, blue, alpha, dither);
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetBlending(
    uint32 arg_count,
    const cmd::SetBlending& args) {
  namespace cmd = set_blending;
  Uint32 arg = args.fixme0;
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
    return BufferSyncInterface::kParseInvalidArguments;
  gapi_->SetBlending(enable,
                     separate_alpha,
                     static_cast<GAPIInterface::BlendEq>(color_eq),
                     static_cast<GAPIInterface::BlendFunc>(color_src),
                     static_cast<GAPIInterface::BlendFunc>(color_dst),
                     static_cast<GAPIInterface::BlendEq>(alpha_eq),
                     static_cast<GAPIInterface::BlendFunc>(alpha_src),
                     static_cast<GAPIInterface::BlendFunc>(alpha_dst));
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetBlendingColor(
    uint32 arg_count,
    const cmd::SetBlendingColor& args) {
  RGBA rgba;
  rgba.red = args.red;
  rgba.green = args.green;
  rgba.blue = args.blue;
  rgba.alpha = args.alpha;
  gapi_->SetBlendingColor(rgba);
  return BufferSyncInterface::kParseNoError;
}

BufferSyncInterface::ParseError GAPIDecoder::HandleCreateRenderSurface(
    uint32 arg_count,
    const cmd::CreateRenderSurface& args) {
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

BufferSyncInterface::ParseError GAPIDecoder::HandleDestroyRenderSurface(
    uint32 arg_count,
    const cmd::DestroyRenderSurface& args) {
  return gapi_->DestroyRenderSurface(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleCreateDepthSurface(
    uint32 arg_count,
    const cmd::CreateDepthSurface& args) {
  namespace cmd = create_depth_surface_cmd;
  unsigned int width_height = args.fixme1;
  unsigned int width = cmd::Width::Get(width_height);
  unsigned int height = cmd::Height::Get(width_height);
  return gapi_->CreateDepthSurface(args.id, width, height);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleDestroyDepthSurface(
    uint32 arg_count,
    const cmd::DestroyDepthSurface& args) {
  return gapi_->DestroyDepthSurface(args.id);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetRenderSurface(
    uint32 arg_count,
    const cmd::SetRenderSurface& args) {
  return gapi_->SetRenderSurface(args.render_surface_id, args.depth_surface_id);
}

BufferSyncInterface::ParseError GAPIDecoder::HandleSetBackSurfaces(
    uint32 arg_count,
    const cmd::SetBackSurfaces& args) {
  gapi_->SetBackSurfaces();
  return  BufferSyncInterface::kParseNoError;
}

}  // namespace command_buffer
}  // namespace o3d
