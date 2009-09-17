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


// This file contains the binary format definition of the command buffer.

#ifndef O3D_COMMAND_BUFFER_COMMON_CROSS_CMD_BUFFER_FORMAT_H_
#define O3D_COMMAND_BUFFER_COMMON_CROSS_CMD_BUFFER_FORMAT_H_

#include "base/basictypes.h"
#include "command_buffer/common/cross/types.h"
#include "command_buffer/common/cross/bitfield_helpers.h"
#include "core/cross/packing.h"

namespace o3d {
namespace command_buffer {

// Struct that defines the command header in the command buffer.
struct CommandHeader {
  Uint32 size:8;
  Uint32 command:24;
};
COMPILE_ASSERT(sizeof(CommandHeader) == 4, Sizeof_CommandHeader_is_not_4);

// Union that defines possible command buffer entries.
union CommandBufferEntry {
  CommandHeader value_header;
  Uint32 value_uint32;
  Int32 value_int32;
  float value_float;
};

COMPILE_ASSERT(sizeof(CommandBufferEntry) == 4,
               Sizeof_CommandBufferEntry_is_not_4);

// Bitfields for the SET_VERTEX_INPUT command.
namespace set_vertex_input_cmd {
// argument 4
typedef BitField<0, 4> SemanticIndex;
typedef BitField<4, 4> Semantic;
typedef BitField<8, 8> Type;
typedef BitField<16, 16> Stride;
}  // namespace set_vertex_input_cmd

// Bitfields for the CREATE_TEXTURE_2D command.
namespace create_texture_2d_cmd {
// argument 1
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
// argument 2
typedef BitField<0, 4> Levels;
typedef BitField<4, 4> Unused;
typedef BitField<8, 8> Format;
typedef BitField<16, 16> Flags;
}  // namespace create_texture_2d_cmd

// Bitfields for the CREATE_TEXTURE_3D command.
namespace create_texture_3d_cmd {
// argument 1
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
// argument 2
typedef BitField<0, 16> Depth;
typedef BitField<16, 16> Unused1;
// argument 3
typedef BitField<0, 4> Levels;
typedef BitField<4, 4> Unused2;
typedef BitField<8, 8> Format;
typedef BitField<16, 16> Flags;
}  // namespace create_texture_3d_cmd

// Bitfields for the CREATE_TEXTURE_CUBE command.
namespace create_texture_cube_cmd {
// argument 1
typedef BitField<0, 16> Side;
typedef BitField<16, 16> Unused1;
// argument 2
typedef BitField<0, 4> Levels;
typedef BitField<4, 4> Unused2;
typedef BitField<8, 8> Format;
typedef BitField<16, 16> Flags;
}  // namespace create_texture_cube_cmd

// Bitfields for the CREATE_RENDER_SURFACE command.
namespace create_render_surface_cmd {
// argument 1
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
// argument 2 may refer to side or depth
typedef BitField<0, 16> Levels;
typedef BitField<16, 16> Side;
}  // namespace create_render_surface_cmd

// Bitfields for the SET_TEXTURE_DATA command.
namespace set_texture_data_cmd {
// argument 1
typedef BitField<0, 16> X;
typedef BitField<16, 16> Y;
// argument 2
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
// argument 3
typedef BitField<0, 16> Z;
typedef BitField<16, 16> Depth;
// argument 4
typedef BitField<0, 4> Level;
typedef BitField<4, 3> Face;
typedef BitField<7, 25> Unused;
}  // namespace set_texture_data_cmd

// Bitfields for the SET_TEXTURE_DATA_IMMEDIATE command.
namespace set_texture_data_immediate_cmd {
// argument 1
typedef BitField<0, 16> X;
typedef BitField<16, 16> Y;
// argument 2
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
// argument 3
typedef BitField<0, 16> Z;
typedef BitField<16, 16> Depth;
// argument 4
typedef BitField<0, 4> Level;
typedef BitField<4, 3> Face;
typedef BitField<7, 25> Unused;
}  // namespace set_texture_data_immediate_cmd

// Bitfields for the GET_TEXTURE_DATA command.
namespace get_texture_data_cmd {
// argument 1
typedef BitField<0, 16> X;
typedef BitField<16, 16> Y;
// argument 2
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
// argument 3
typedef BitField<0, 16> Z;
typedef BitField<16, 16> Depth;
// argument 4
typedef BitField<0, 4> Level;
typedef BitField<4, 3> Face;
typedef BitField<7, 25> Unused;
}  // namespace get_texture_data_cmd

// Bitfields for the SET_SAMPLER_STATES command.
namespace set_sampler_states {
// argument 2
typedef BitField<0, 3> AddressingU;
typedef BitField<3, 3> AddressingV;
typedef BitField<6, 3> AddressingW;
typedef BitField<9, 3> MagFilter;
typedef BitField<12, 3> MinFilter;
typedef BitField<15, 3> MipFilter;
typedef BitField<18, 6> Unused;
typedef BitField<24, 8> MaxAnisotropy;
}  // namespace get_texture_data_cmd

namespace set_scissor {
// argument 0
typedef BitField<0, 15> X;
typedef BitField<15, 1> Unused;
typedef BitField<16, 15> Y;
typedef BitField<31, 1> Enable;
// argument 1
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
}  // namespace set_scissor

namespace set_point_line_raster {
// argument 0
typedef BitField<0, 1> LineSmoothEnable;
typedef BitField<1, 1> PointSpriteEnable;
typedef BitField<2, 30> Unused;
}  // namespace set_point_line_raster

namespace set_polygon_raster {
// argument 0
typedef BitField<0, 2> FillMode;
typedef BitField<2, 2> CullMode;
typedef BitField<4, 28> Unused;
}  // namespace set_polygon_raster

namespace set_alpha_test {
// argument 0
typedef BitField<0, 3> Func;
typedef BitField<3, 28> Unused;
typedef BitField<31, 1> Enable;
}  // namespace set_alpha_test

namespace set_depth_test {
// argument 0
typedef BitField<0, 3> Func;
typedef BitField<3, 27> Unused;
typedef BitField<30, 1> WriteEnable;
typedef BitField<31, 1> Enable;
}  // namespace set_depth_test

namespace set_stencil_test {
// argument 0
typedef BitField<0, 8> WriteMask;
typedef BitField<8, 8> CompareMask;
typedef BitField<16, 8> ReferenceValue;
typedef BitField<24, 6> Unused0;
typedef BitField<30, 1> SeparateCCW;
typedef BitField<31, 1> Enable;
// argument 1
typedef BitField<0, 3> CWFunc;
typedef BitField<3, 3> CWPassOp;
typedef BitField<6, 3> CWFailOp;
typedef BitField<9, 3> CWZFailOp;
typedef BitField<12, 4> Unused1;
typedef BitField<16, 3> CCWFunc;
typedef BitField<19, 3> CCWPassOp;
typedef BitField<22, 3> CCWFailOp;
typedef BitField<25, 3> CCWZFailOp;
typedef BitField<28, 4> Unused2;
}  // namespace set_stencil_test

namespace set_color_write {
// argument 0
typedef BitField<0, 1> RedMask;
typedef BitField<1, 1> GreenMask;
typedef BitField<2, 1> BlueMask;
typedef BitField<3, 1> AlphaMask;
typedef BitField<0, 4> AllColorsMask;  // alias for RGBA
typedef BitField<4, 27> Unused;
typedef BitField<31, 1> DitherEnable;
}  // namespace set_color_write

namespace set_blending {
// argument 0
typedef BitField<0, 4> ColorSrcFunc;
typedef BitField<4, 4> ColorDstFunc;
typedef BitField<8, 3> ColorEq;
typedef BitField<11, 5> Unused0;
typedef BitField<16, 4> AlphaSrcFunc;
typedef BitField<20, 4> AlphaDstFunc;
typedef BitField<24, 3> AlphaEq;
typedef BitField<27, 3> Unused1;
typedef BitField<30, 1> SeparateAlpha;
typedef BitField<31, 1> Enable;
}  // namespace set_blending


// This macro is used to safely and convienently expand the list of commnad
// buffer commands in to various lists and never have them get out of sync. To
// add a new command, add it this list, create the corresponding structure below
// and then add a function in gapi_decoder.cc called Handle_COMMAND_NAME where
// COMMAND_NAME is the name of your command structure.
//
// NOTE: THE ORDER OF THESE MUST NOT CHANGE (their id is derived by order)
#define O3D_COMMAND_BUFFER_CMDS \
  O3D_COMMAND_BUFFER_CMD_OP(NOOP)                              /*  0 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SET_TOKEN)                                  \
  O3D_COMMAND_BUFFER_CMD_OP(BEGIN_FRAME)                                \
  O3D_COMMAND_BUFFER_CMD_OP(END_FRAME)                                  \
  O3D_COMMAND_BUFFER_CMD_OP(CLEAR)                                      \
  O3D_COMMAND_BUFFER_CMD_OP(CREATE_VERTEX_BUFFER)                       \
  O3D_COMMAND_BUFFER_CMD_OP(DESTROY_VERTEX_BUFFER)                      \
  O3D_COMMAND_BUFFER_CMD_OP(SET_VERTEX_BUFFER_DATA)                     \
  O3D_COMMAND_BUFFER_CMD_OP(SET_VERTEX_BUFFER_DATA_IMMEDIATE)           \
  O3D_COMMAND_BUFFER_CMD_OP(GET_VERTEX_BUFFER_DATA)                     \
  O3D_COMMAND_BUFFER_CMD_OP(CREATE_INDEX_BUFFER)               /* 10 */ \
  O3D_COMMAND_BUFFER_CMD_OP(DESTROY_INDEX_BUFFER)                       \
  O3D_COMMAND_BUFFER_CMD_OP(SET_INDEX_BUFFER_DATA)                      \
  O3D_COMMAND_BUFFER_CMD_OP(SET_INDEX_BUFFER_DATA_IMMEDIATE)            \
  O3D_COMMAND_BUFFER_CMD_OP(GET_INDEX_BUFFER_DATA)                      \
  O3D_COMMAND_BUFFER_CMD_OP(CREATE_VERTEX_STRUCT)                       \
  O3D_COMMAND_BUFFER_CMD_OP(DESTROY_VERTEX_STRUCT)                      \
  O3D_COMMAND_BUFFER_CMD_OP(SET_VERTEX_INPUT)                           \
  O3D_COMMAND_BUFFER_CMD_OP(SET_VERTEX_STRUCT)                          \
  O3D_COMMAND_BUFFER_CMD_OP(DRAW)                                       \
  O3D_COMMAND_BUFFER_CMD_OP(DRAW_INDEXED)                      /* 20 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CREATE_EFFECT)                              \
  O3D_COMMAND_BUFFER_CMD_OP(CREATE_EFFECT_IMMEDIATE)                    \
  O3D_COMMAND_BUFFER_CMD_OP(DESTROY_EFFECT)                             \
  O3D_COMMAND_BUFFER_CMD_OP(SET_EFFECT)                                 \
  O3D_COMMAND_BUFFER_CMD_OP(GET_PARAM_COUNT)                            \
  O3D_COMMAND_BUFFER_CMD_OP(CREATE_PARAM)                               \
  O3D_COMMAND_BUFFER_CMD_OP(CREATE_PARAM_BY_NAME)                       \
  O3D_COMMAND_BUFFER_CMD_OP(CREATE_PARAM_BY_NAME_IMMEDIATE)             \
  O3D_COMMAND_BUFFER_CMD_OP(DESTROY_PARAM)                              \
  O3D_COMMAND_BUFFER_CMD_OP(SET_PARAM_DATA)                    /* 30 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SET_PARAM_DATA_IMMEDIATE)                   \
  O3D_COMMAND_BUFFER_CMD_OP(GET_PARAM_DESC)                             \
  O3D_COMMAND_BUFFER_CMD_OP(GET_STREAM_COUNT)                           \
  O3D_COMMAND_BUFFER_CMD_OP(GET_STREAM_DESC)                            \
  O3D_COMMAND_BUFFER_CMD_OP(DESTROY_TEXTURE)                            \
  O3D_COMMAND_BUFFER_CMD_OP(CREATE_TEXTURE_2D)                          \
  O3D_COMMAND_BUFFER_CMD_OP(CREATE_TEXTURE_3D)                          \
  O3D_COMMAND_BUFFER_CMD_OP(CREATE_TEXTURE_CUBE)                        \
  O3D_COMMAND_BUFFER_CMD_OP(SET_TEXTURE_DATA)                           \
  O3D_COMMAND_BUFFER_CMD_OP(SET_TEXTURE_DATA_IMMEDIATE)        /* 40 */ \
  O3D_COMMAND_BUFFER_CMD_OP(GET_TEXTURE_DATA)                           \
  O3D_COMMAND_BUFFER_CMD_OP(CREATE_SAMPLER)                             \
  O3D_COMMAND_BUFFER_CMD_OP(DESTROY_SAMPLER)                            \
  O3D_COMMAND_BUFFER_CMD_OP(SET_SAMPLER_STATES)                         \
  O3D_COMMAND_BUFFER_CMD_OP(SET_SAMPLER_BORDER_COLOR)                   \
  O3D_COMMAND_BUFFER_CMD_OP(SET_SAMPLER_TEXTURE)                        \
  O3D_COMMAND_BUFFER_CMD_OP(SET_VIEWPORT)                               \
  O3D_COMMAND_BUFFER_CMD_OP(SET_SCISSOR)                                \
  O3D_COMMAND_BUFFER_CMD_OP(SET_POINT_LINE_RASTER)                      \
  O3D_COMMAND_BUFFER_CMD_OP(SET_POLYGON_RASTER)                /* 50 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SET_POLYGON_OFFSET)                         \
  O3D_COMMAND_BUFFER_CMD_OP(SET_ALPHA_TEST)                             \
  O3D_COMMAND_BUFFER_CMD_OP(SET_DEPTH_TEST)                             \
  O3D_COMMAND_BUFFER_CMD_OP(SET_STENCIL_TEST)                           \
  O3D_COMMAND_BUFFER_CMD_OP(SET_BLENDING)                               \
  O3D_COMMAND_BUFFER_CMD_OP(SET_BLENDING_COLOR)                         \
  O3D_COMMAND_BUFFER_CMD_OP(SET_COLOR_WRITE)                            \
  O3D_COMMAND_BUFFER_CMD_OP(CREATE_RENDER_SURFACE)                      \
  O3D_COMMAND_BUFFER_CMD_OP(DESTROY_RENDER_SURFACE)                     \
  O3D_COMMAND_BUFFER_CMD_OP(CREATE_DEPTH_SURFACE)              /* 60 */ \
  O3D_COMMAND_BUFFER_CMD_OP(DESTROY_DEPTH_SURFACE)                      \
  O3D_COMMAND_BUFFER_CMD_OP(SET_RENDER_SURFACE)                         \
  O3D_COMMAND_BUFFER_CMD_OP(SET_BACK_SURFACES)                          \


// GAPI commands.
enum CommandId {
  #define O3D_COMMAND_BUFFER_CMD_OP(name) name,

  O3D_COMMAND_BUFFER_CMDS

  #undef O3D_COMMAND_BUFFER_CMD_OP
};

namespace cmd {

// Make sure the compiler does not add extra padding to any of the command
// structures.
O3D_PUSH_STRUCTURE_PACKING_1;

enum ArgFlags {
  kFixed = 0x0,
  kAtLeastN = 0x1,
};

struct SharedMemory {
  uint32 id;
  uint32 offset;
};

struct NOOP {
  static const CommandId kCmdId = command_buffer::NOOP;
  static const ArgFlags kArgFlags = kAtLeastN;
};

struct SET_TOKEN {
  static const CommandId kCmdId = command_buffer::SET_TOKEN;
  static const ArgFlags kArgFlags = kFixed;
  uint32 token;
};

struct BEGIN_FRAME {
  static const CommandId kCmdId = command_buffer::BEGIN_FRAME;
  static const ArgFlags kArgFlags = kFixed;
};

struct END_FRAME {
  static const CommandId kCmdId = command_buffer::END_FRAME;
  static const ArgFlags kArgFlags = kFixed;
};

struct CLEAR {
  static const CommandId kCmdId = command_buffer::CLEAR;
  static const ArgFlags kArgFlags = kFixed;
  uint32 buffers;
  float red;
  float green;
  float blue;
  float alpha;
  float depth;
  uint32 stencil;
};

struct SET_VIEWPORT {
  static const CommandId kCmdId = command_buffer::SET_VIEWPORT;
  static const ArgFlags kArgFlags = kFixed;
  uint32 left;
  uint32 top;
  uint32 width;
  uint32 height;
  float z_min;
  float z_max;
};

struct CREATE_VERTEX_BUFFER {
  static const CommandId kCmdId = command_buffer::CREATE_VERTEX_BUFFER;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  uint32 size;
  uint32 flags;
};

struct DESTROY_VERTEX_BUFFER {
  static const CommandId kCmdId = command_buffer::DESTROY_VERTEX_BUFFER;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
};

struct SET_VERTEX_BUFFER_DATA_IMMEDIATE {
  static const CommandId kCmdId = command_buffer::SET_VERTEX_BUFFER_DATA_IMMEDIATE;
  static const ArgFlags kArgFlags = kAtLeastN;
  uint32 id;
  uint32 offset;
};

struct SET_VERTEX_BUFFER_DATA {
  static const CommandId kCmdId = command_buffer::SET_VERTEX_BUFFER_DATA;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  uint32 offset;
  uint32 size;
  SharedMemory shared_memory;
};

struct GET_VERTEX_BUFFER_DATA {
  static const CommandId kCmdId = command_buffer::GET_VERTEX_BUFFER_DATA;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  uint32 offset;
  uint32 size;
  SharedMemory shared_memory;
};

struct CREATE_INDEX_BUFFER {
  static const CommandId kCmdId = command_buffer::CREATE_INDEX_BUFFER;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  uint32 size;
  uint32 flags;
};

struct DESTROY_INDEX_BUFFER {
  static const CommandId kCmdId = command_buffer::DESTROY_INDEX_BUFFER;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
};

struct SET_INDEX_BUFFER_DATA_IMMEDIATE {
  static const CommandId kCmdId = command_buffer::SET_INDEX_BUFFER_DATA_IMMEDIATE;
  static const ArgFlags kArgFlags = kAtLeastN;
  uint32 id;
  uint32 offset;
};

struct SET_INDEX_BUFFER_DATA {
  static const CommandId kCmdId = command_buffer::SET_INDEX_BUFFER_DATA;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  uint32 offset;
  uint32 size;
  SharedMemory shared_memory;
};

struct GET_INDEX_BUFFER_DATA {
  static const CommandId kCmdId = command_buffer::GET_INDEX_BUFFER_DATA;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  uint32 offset;
  uint32 size;
  SharedMemory shared_memory;
};

struct CREATE_VERTEX_STRUCT {
  static const CommandId kCmdId = command_buffer::CREATE_VERTEX_STRUCT;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  uint32 input_count;
};

struct DESTROY_VERTEX_STRUCT {
  static const CommandId kCmdId = command_buffer::DESTROY_VERTEX_STRUCT;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
};

struct SET_VERTEX_INPUT {
  static const CommandId kCmdId = command_buffer::SET_VERTEX_INPUT;
  static const ArgFlags kArgFlags = kAtLeastN;
};

struct SET_VERTEX_STRUCT {
  static const CommandId kCmdId = command_buffer::SET_VERTEX_STRUCT;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
};

struct DRAW {
  static const CommandId kCmdId = command_buffer::DRAW;
  static const ArgFlags kArgFlags = kFixed;
  uint32 primitive_type;
  uint32 first;
  uint32 count;
};

struct DRAW_INDEXED {
  static const CommandId kCmdId = command_buffer::DRAW_INDEXED;
  static const ArgFlags kArgFlags = kFixed;
  uint32 primitive_type;
  uint32 index_buffer_id;
  uint32 first;
  uint32 count;
  uint32 min_index;
  uint32 max_index;
};

struct CREATE_EFFECT {
  static const CommandId kCmdId = command_buffer::CREATE_EFFECT;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  uint32 size;
  SharedMemory shared_memory;
};

struct CREATE_EFFECT_IMMEDIATE {
  static const CommandId kCmdId = command_buffer::CREATE_EFFECT_IMMEDIATE;
  static const ArgFlags kArgFlags = kAtLeastN;
  uint32 id;
  uint32 size;
};

struct DESTROY_EFFECT {
  static const CommandId kCmdId = command_buffer::DESTROY_EFFECT;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
};

struct SET_EFFECT {
  static const CommandId kCmdId = command_buffer::SET_EFFECT;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
};

struct GET_PARAM_COUNT {
  static const CommandId kCmdId = command_buffer::GET_PARAM_COUNT;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  uint32 size;
  SharedMemory shared_memory;
};

struct CREATE_PARAM {
  static const CommandId kCmdId = command_buffer::CREATE_PARAM;
  static const ArgFlags kArgFlags = kFixed;
  uint32 param_id;
  uint32 effect_id;
  uint32 index;
};

struct CREATE_PARAM_BY_NAME {
  static const CommandId kCmdId = command_buffer::CREATE_PARAM_BY_NAME;
  static const ArgFlags kArgFlags = kFixed;
  uint32 param_id;
  uint32 effect_id;
  uint32 size;
  SharedMemory shared_memory;
};

struct CREATE_PARAM_BY_NAME_IMMEDIATE {
  static const CommandId kCmdId = command_buffer::CREATE_PARAM_BY_NAME_IMMEDIATE;
  static const ArgFlags kArgFlags = kAtLeastN;
  uint32 param_id;
  uint32 effect_id;
  uint32 size;
};

struct DESTROY_PARAM {
  static const CommandId kCmdId = command_buffer::DESTROY_PARAM;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
};

struct SET_PARAM_DATA {
  static const CommandId kCmdId = command_buffer::SET_PARAM_DATA;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  uint32 size;
  SharedMemory shared_memory;
};

struct SET_PARAM_DATA_IMMEDIATE {
  static const CommandId kCmdId = command_buffer::SET_PARAM_DATA_IMMEDIATE;
  static const ArgFlags kArgFlags = kAtLeastN;
  uint32 id;
  uint32 size;
};

struct GET_PARAM_DESC {
  static const CommandId kCmdId = command_buffer::GET_PARAM_DESC;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  uint32 size;
  SharedMemory shared_memory;
};

struct GET_STREAM_COUNT {
  static const CommandId kCmdId = command_buffer::GET_STREAM_COUNT;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  uint32 size;
  SharedMemory shared_memory;
};

struct GET_STREAM_DESC {
  static const CommandId kCmdId = command_buffer::GET_STREAM_DESC;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  uint32 index;
  uint32 size;
  SharedMemory shared_memory;
};

struct DESTROY_TEXTURE {
  static const CommandId kCmdId = command_buffer::DESTROY_TEXTURE;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
};

struct CREATE_TEXTURE_2D {
  static const CommandId kCmdId = command_buffer::CREATE_TEXTURE_2D;
  static const ArgFlags kArgFlags = kAtLeastN;
  // TODO(gman): fix this to not use obfusticated fields.
};

struct CREATE_TEXTURE_3D {
  static const CommandId kCmdId = command_buffer::CREATE_TEXTURE_3D;
  static const ArgFlags kArgFlags = kAtLeastN;
  // TODO(gman): fix this to not use obfusticated fields.
};

struct CREATE_TEXTURE_CUBE {
  static const CommandId kCmdId = command_buffer::CREATE_TEXTURE_CUBE;
  static const ArgFlags kArgFlags = kAtLeastN;
  // TODO(gman): fix this to not use obfusticated fields.
};

struct SET_TEXTURE_DATA {
  static const CommandId kCmdId = command_buffer::SET_TEXTURE_DATA;
  static const ArgFlags kArgFlags = kAtLeastN;
  // TODO(gman): fix this to not use obfusticated fields.
};

struct SET_TEXTURE_DATA_IMMEDIATE {
  static const CommandId kCmdId = command_buffer::SET_TEXTURE_DATA_IMMEDIATE;
  static const ArgFlags kArgFlags = kAtLeastN;
  // TODO(gman): fix this to not use obfusticated fields.
};

struct GET_TEXTURE_DATA {
  static const CommandId kCmdId = command_buffer::GET_TEXTURE_DATA;
  static const ArgFlags kArgFlags = kAtLeastN;
  // TODO(gman): fix this to not use obfusticated fields.
};

struct CREATE_SAMPLER {
  static const CommandId kCmdId = command_buffer::CREATE_SAMPLER;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
};

struct DESTROY_SAMPLER {
  static const CommandId kCmdId = command_buffer::DESTROY_SAMPLER;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
};

struct SET_SAMPLER_STATES {
  static const CommandId kCmdId = command_buffer::SET_SAMPLER_STATES;
  static const ArgFlags kArgFlags = kAtLeastN;
  // TODO(gman): fix this to not use obfusticated fields.
};

struct SET_SAMPLER_BORDER_COLOR {
  static const CommandId kCmdId = command_buffer::SET_SAMPLER_BORDER_COLOR;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  float red;
  float blue;
  float green;
  float alpha;
};

struct SET_SAMPLER_TEXTURE {
  static const CommandId kCmdId = command_buffer::SET_SAMPLER_TEXTURE;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
  uint32 texture_id;
};

struct SET_SCISSOR {
  static const CommandId kCmdId = command_buffer::SET_SCISSOR;
  static const ArgFlags kArgFlags = kFixed;
  // TODO(gman): fix this to not use obfusticated fields.
  uint32 fixme0;
  uint32 fixme1;
};

struct SET_POLYGON_OFFSET {
  static const CommandId kCmdId = command_buffer::SET_POLYGON_OFFSET;
  static const ArgFlags kArgFlags = kFixed;
  float slope_factor;
  float units;
};

struct SET_POINT_LINE_RASTER {
  static const CommandId kCmdId = command_buffer::SET_POINT_LINE_RASTER;
  static const ArgFlags kArgFlags = kFixed;
  // TODO(gman): fix this to not use obfusticated fields.
  uint32 fixme0;
  float point_size;
};

struct SET_POLYGON_RASTER {
  static const CommandId kCmdId = command_buffer::SET_POLYGON_RASTER;
  static const ArgFlags kArgFlags = kFixed;
  // TODO(gman): fix this to not use obfusticated fields.
  uint32 fixme0;
};

struct SET_ALPHA_TEST {
  static const CommandId kCmdId = command_buffer::SET_ALPHA_TEST;
  static const ArgFlags kArgFlags = kFixed;
  // TODO(gman): fix this to not use obfusticated fields.
  uint32 fixme0;
  float value;
};

struct SET_DEPTH_TEST {
  static const CommandId kCmdId = command_buffer::SET_DEPTH_TEST;
  static const ArgFlags kArgFlags = kFixed;
  // TODO(gman): fix this to not use obfusticated fields.
  uint32 fixme0;
};

struct SET_STENCIL_TEST {
  static const CommandId kCmdId = command_buffer::SET_STENCIL_TEST;
  static const ArgFlags kArgFlags = kAtLeastN;
  // TODO(gman): fix this to not use obfusticated fields.
};

struct SET_COLOR_WRITE {
  static const CommandId kCmdId = command_buffer::SET_COLOR_WRITE;
  static const ArgFlags kArgFlags = kFixed;
  uint32 flags;
};

struct SET_BLENDING {
  static const CommandId kCmdId = command_buffer::SET_BLENDING;
  static const ArgFlags kArgFlags = kAtLeastN;
  // TODO(gman): fix this to not use obfusticated fields.
};

struct SET_BLENDING_COLOR {
  static const CommandId kCmdId = command_buffer::SET_BLENDING_COLOR;
  static const ArgFlags kArgFlags = kFixed;
  float red;
  float blue;
  float green;
  float alpha;
};

struct CREATE_RENDER_SURFACE {
  static const CommandId kCmdId = command_buffer::CREATE_RENDER_SURFACE;
  static const ArgFlags kArgFlags = kFixed;
  // TODO(gman): fix this to not use obfusticated fields.
  uint32 id;
  uint32 fixme1;
  uint32 fixme2;
  uint32 texture_id;
};

struct DESTROY_RENDER_SURFACE {
  static const CommandId kCmdId = command_buffer::DESTROY_RENDER_SURFACE;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
};

struct CREATE_DEPTH_SURFACE {
  static const CommandId kCmdId = command_buffer::CREATE_DEPTH_SURFACE;
  static const ArgFlags kArgFlags = kFixed;
  // TODO(gman): fix this to not use obfusticated fields.
  uint32 id;
  uint32 fixme1;
};

struct DESTROY_DEPTH_SURFACE {
  static const CommandId kCmdId = command_buffer::DESTROY_DEPTH_SURFACE;
  static const ArgFlags kArgFlags = kFixed;
  uint32 id;
};

struct SET_RENDER_SURFACE {
  static const CommandId kCmdId = command_buffer::SET_RENDER_SURFACE;
  static const ArgFlags kArgFlags = kFixed;
  uint32 render_surface_id;
  uint32 depth_surface_id;
};

struct SET_BACK_SURFACES {
  static const CommandId kCmdId = command_buffer::SET_BACK_SURFACES;
  static const ArgFlags kArgFlags = kFixed;
};

O3D_POP_STRUCTURE_PACKING;

}  // namespace cmd

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_CMD_BUFFER_FORMAT_H_
