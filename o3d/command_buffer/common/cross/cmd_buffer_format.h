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


// This file contains the binary format definition of the command buffer and
// command buffer commands.
// It is recommended you use the CommandBufferHelper object to create commands
// but if you want to go lower level you can use the structures here to help.
//
// A few ways to use them:
//
// Fill out a structure in place:
//
// cmd::SetViewport::Set(ptrToSomeSharedMemory,
//                       left, right, width, height, z_min, z_max);
//
// Fill out consecutive commands:
//
// Note that each cmd::XXX::Set function returns a pointer to the place
// the next command should go.
//
// void* dest = ptrToSomeSharedMemory;
// dest = cmd::SetViewport::Set(dest, left, right, width, height, min, max);
// dest = cmd::Clear::Set(dest, buffers, r, g, b, a, depth, stencil);
// dest = cmd::Draw::Set(dest, primitive_type, first, count);
//
// NOTE: The types in this file must be POD types. That means they can not have
// constructors, destructors, virtual functions or inheritance and they can only
// use other POD types or intrinsics as members.

#ifndef O3D_COMMAND_BUFFER_COMMON_CROSS_CMD_BUFFER_FORMAT_H_
#define O3D_COMMAND_BUFFER_COMMON_CROSS_CMD_BUFFER_FORMAT_H_

#include "base/basictypes.h"
#include "command_buffer/common/cross/types.h"
#include "command_buffer/common/cross/bitfield_helpers.h"
#include "core/cross/packing.h"

namespace o3d {
namespace command_buffer {

namespace cmd {
  enum ArgFlags {
    kFixed = 0x0,
    kAtLeastN = 0x1,
  };
}  // namespace cmd

// Computes the number of command buffer entries needed for a certain size. In
// other words it rounds up to a multiple of entries.
inline uint32 ComputeNumEntries(size_t size_in_bytes) {
  return static_cast<uint32>(
      (size_in_bytes + sizeof(uint32) - 1) / sizeof(uint32));  // NOLINT
}

// Rounds up to a multiple of entries in bytes.
inline size_t RoundSizeToMultipleOfEntries(size_t size_in_bytes) {
  return ComputeNumEntries(size_in_bytes) * sizeof(uint32);  // NOLINT
}

// Struct that defines the command header in the command buffer.
struct CommandHeader {
  Uint32 size:8;
  Uint32 command:24;

  void Init(uint32 _command, uint32 _size) {
    command = _command;
    size = _size;
  }

  // Sets the header based on the passed in command. Can not be used for
  // variable sized commands like immediate commands or Noop.
  template <typename T>
  void SetCmd() {
    COMPILE_ASSERT(T::kArgFlags == cmd::kFixed, Cmd_kArgFlags_not_kFixed);
    Init(T::kCmdId, ComputeNumEntries(sizeof(T)));  // NOLINT
  }

  // Sets the header by a size in bytes.
  template <typename T>
  void SetCmdBySize(uint32 size_in_bytes) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
    Init(T::kCmdId, ComputeNumEntries(sizeof(T) + size_in_bytes));  // NOLINT
  }
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

// Bitfields for the SetVertexInput command.
namespace set_vertex_input_cmd {
// argument 4
typedef BitField<0, 4> SemanticIndex;  // TODO(gman): shouldn't this be bigger
                                       // than 4 bits for future expansion?
typedef BitField<4, 4> Semantic;
typedef BitField<8, 8> Type;
typedef BitField<16, 16> Stride;
}  // namespace set_vertex_input_cmd

// Bitfields for the CreateTexture2d command.
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

// Bitfields for the CreateTexture3d command.
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

// Bitfields for the CreateTextureCube command.
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

// Bitfields for the CreateRenderSurface command.
namespace create_render_surface_cmd {
// argument 1
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
// argument 2 may refer to side or depth
typedef BitField<0, 16> Levels;
typedef BitField<16, 16> Side;
}  // namespace create_render_surface_cmd

// Bitfields for the CreateDepthSurface command.
namespace create_depth_surface_cmd {
// argument 1
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
}  // namespace create_depth_surface_cmd

// Bitfields for the SetTextureData command.
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

// Bitfields for the SetTextureDataImmediate command.
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

// Bitfields for the GetTextureData command.
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

// Bitfields for the SetSamplerStates command.
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
  O3D_COMMAND_BUFFER_CMD_OP(Noop)                          /*  0 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetToken)                      /*  1 */ \
  O3D_COMMAND_BUFFER_CMD_OP(BeginFrame)                    /*  2 */ \
  O3D_COMMAND_BUFFER_CMD_OP(EndFrame)                      /*  3 */ \
  O3D_COMMAND_BUFFER_CMD_OP(Clear)                         /*  4 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CreateVertexBuffer)            /*  5 */ \
  O3D_COMMAND_BUFFER_CMD_OP(DestroyVertexBuffer)           /*  6 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetVertexBufferData)           /*  7 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetVertexBufferDataImmediate)  /*  8 */ \
  O3D_COMMAND_BUFFER_CMD_OP(GetVertexBufferData)           /*  9 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CreateIndexBuffer)             /* 10 */ \
  O3D_COMMAND_BUFFER_CMD_OP(DestroyIndexBuffer)            /* 11 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetIndexBufferData)            /* 12 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetIndexBufferDataImmediate)   /* 13 */ \
  O3D_COMMAND_BUFFER_CMD_OP(GetIndexBufferData)            /* 14 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CreateVertexStruct)            /* 15 */ \
  O3D_COMMAND_BUFFER_CMD_OP(DestroyVertexStruct)           /* 16 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetVertexInput)                /* 17 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetVertexStruct)               /* 18 */ \
  O3D_COMMAND_BUFFER_CMD_OP(Draw)                          /* 19 */ \
  O3D_COMMAND_BUFFER_CMD_OP(DrawIndexed)                   /* 20 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CreateEffect)                  /* 21 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CreateEffectImmediate)         /* 22 */ \
  O3D_COMMAND_BUFFER_CMD_OP(DestroyEffect)                 /* 23 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetEffect)                     /* 24 */ \
  O3D_COMMAND_BUFFER_CMD_OP(GetParamCount)                 /* 25 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CreateParam)                   /* 26 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CreateParamByName)             /* 27 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CreateParamByNameImmediate)    /* 28 */ \
  O3D_COMMAND_BUFFER_CMD_OP(DestroyParam)                  /* 29 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetParamData)                  /* 30 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetParamDataImmediate)         /* 31 */ \
  O3D_COMMAND_BUFFER_CMD_OP(GetParamDesc)                  /* 32 */ \
  O3D_COMMAND_BUFFER_CMD_OP(GetStreamCount)                /* 33 */ \
  O3D_COMMAND_BUFFER_CMD_OP(GetStreamDesc)                 /* 34 */ \
  O3D_COMMAND_BUFFER_CMD_OP(DestroyTexture)                /* 35 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CreateTexture2d)               /* 36 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CreateTexture3d)               /* 37 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CreateTextureCube)             /* 38 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetTextureData)                /* 39 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetTextureDataImmediate)       /* 40 */ \
  O3D_COMMAND_BUFFER_CMD_OP(GetTextureData)                /* 41 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CreateSampler)                 /* 42 */ \
  O3D_COMMAND_BUFFER_CMD_OP(DestroySampler)                /* 43 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetSamplerStates)              /* 44 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetSamplerBorderColor)         /* 45 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetSamplerTexture)             /* 46 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetViewport)                   /* 47 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetScissor)                    /* 48 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetPointLineRaster)            /* 49 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetPolygonRaster)              /* 50 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetPolygonOffset)              /* 51 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetAlphaTest)                  /* 52 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetDepthTest)                  /* 53 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetStencilTest)                /* 54 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetBlending)                   /* 55 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetBlendingColor)              /* 56 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetColorWrite)                 /* 57 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CreateRenderSurface)           /* 58 */ \
  O3D_COMMAND_BUFFER_CMD_OP(DestroyRenderSurface)          /* 59 */ \
  O3D_COMMAND_BUFFER_CMD_OP(CreateDepthSurface)            /* 60 */ \
  O3D_COMMAND_BUFFER_CMD_OP(DestroyDepthSurface)           /* 61 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetRenderSurface)              /* 62 */ \
  O3D_COMMAND_BUFFER_CMD_OP(SetBackSurfaces)               /* 63 */ \


// GAPI commands.
enum CommandId {
  #define O3D_COMMAND_BUFFER_CMD_OP(name) k ## name,

  O3D_COMMAND_BUFFER_CMDS

  #undef O3D_COMMAND_BUFFER_CMD_OP
};

namespace cmd {

// Make sure the compiler does not add extra padding to any of the command
// structures.
O3D_PUSH_STRUCTURE_PACKING_1;

// Gets the address of memory just after a structure in a typesafe way. This is
// used for IMMEDIATE commands to get the address of the place to put the data.
// Immediate command put their data direclty in the command buffer.
// Parameters:
//   cmd: Address of command.
template <typename T>
void* ImmediateDataAddress(T* cmd) {
  COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
  return reinterpret_cast<char*>(cmd) + sizeof(*cmd);
}

// Gets the address of the place to put the next command in a typesafe way.
// This can only be used for fixed sized commands.
template <typename T>
// Parameters:
//   cmd: Address of command.
void* NextCmdAddress(void* cmd) {
  COMPILE_ASSERT(T::kArgFlags == cmd::kFixed, Cmd_kArgFlags_not_kFixed);
  return reinterpret_cast<char*>(cmd) + sizeof(T);
}

// Gets the address of the place to put the next command in a typesafe way.
// This can only be used for variable sized command like IMMEDIATE commands.
// Parameters:
//   cmd: Address of command.
//   size_of_data_in_bytes: Size of the data for the command.
template <typename T>
void* NextImmediateCmdAddress(void* cmd, uint32 size_of_data_in_bytes) {
  COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
  return reinterpret_cast<char*>(cmd) + sizeof(T) +   // NOLINT
      RoundSizeToMultipleOfEntries(size_of_data_in_bytes);
}

struct SharedMemory {
  void Init(uint32 _id, uint32 _offset) {
    id = _id;
    offset = _offset;
  }

  uint32 id;
  uint32 offset;
};

COMPILE_ASSERT(offsetof(SharedMemory, id) == 0,
               Offsetof_SharedMemory_id_not_0);
COMPILE_ASSERT(offsetof(SharedMemory, offset) == 4,
               Offsetof_SharedMemory_offset_not_4);

struct Noop {
  typedef Noop ValueType;
  static const CommandId kCmdId = command_buffer::kNoop;
  static const ArgFlags kArgFlags = kAtLeastN;

  void SetHeader(uint32 skip_count) {
    header.Init(kCmdId, skip_count + 1);
  }

  void Init(uint32 skip_count) {
    SetHeader(skip_count);
  }

  static void* Set(void* cmd, uint32 skip_count) {
    static_cast<ValueType*>(cmd)->Init(skip_count);
    return NextImmediateCmdAddress<ValueType>(
        cmd, skip_count * sizeof(CommandBufferEntry));  // NOLINT
  }

  CommandHeader header;
};

COMPILE_ASSERT(sizeof(Noop) == 4, Sizeof_Noop_is_not_4);
COMPILE_ASSERT(offsetof(Noop, header) == 0, Offsetof_Noop_header_not_0);

struct SetToken {
  typedef SetToken ValueType;
  static const CommandId kCmdId = command_buffer::kSetToken;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _token) {
    SetHeader();
    token = _token;
  }
  static void* Set(void* cmd, uint32 token) {
    static_cast<ValueType*>(cmd)->Init(token);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 token;
};

COMPILE_ASSERT(sizeof(SetToken) == 8, Sizeof_SetToken_is_not_8);
COMPILE_ASSERT(offsetof(SetToken, header) == 0,
               Offsetof_SetToken_header_not_0);
COMPILE_ASSERT(offsetof(SetToken, token) == 4,
               Offsetof_SetToken_token_not_4);

struct BeginFrame {
  typedef BeginFrame ValueType;
  static const CommandId kCmdId = command_buffer::kBeginFrame;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init() {
    SetHeader();
  }
  static void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
};

COMPILE_ASSERT(sizeof(BeginFrame) == 4, Sizeof_BeginFrame_is_not_4);
COMPILE_ASSERT(offsetof(BeginFrame, header) == 0,
               OffsetOf_BeginFrame_header_not_0);

struct EndFrame {
  typedef EndFrame ValueType;
  static const CommandId kCmdId = command_buffer::kEndFrame;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init() {
    SetHeader();
  }
  static void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
};

COMPILE_ASSERT(sizeof(EndFrame) == 4, Sizeof_EndFrame_is_not_4);
COMPILE_ASSERT(offsetof(EndFrame, header) == 0,
               OffsetOf_EndFrame_header_not_0);

struct Clear {
  typedef Clear ValueType;
  static const CommandId kCmdId = command_buffer::kClear;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _buffers, float _red, float _green, float _blue,
            float _alpha, float _depth, uint32 _stencil) {
    SetHeader();
    buffers = _buffers;
    red = _red;
    green = _green;
    blue = _blue;
    alpha = _alpha;
    depth = _depth;
    stencil = _stencil;
  }

  static void* Set(void* cmd, uint32 buffers,
                   float red, float green, float blue, float alpha,
                   float depth,
                   uint32 stencil) {
    static_cast<ValueType*>(cmd)->Init(
        buffers, red, green, blue, alpha, depth, stencil);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 buffers;
  float red;
  float green;
  float blue;
  float alpha;
  float depth;
  uint32 stencil;
};

COMPILE_ASSERT(sizeof(Clear) == 32, Sizeof_Clear_is_not_32);
COMPILE_ASSERT(offsetof(Clear, header) == 0,
               OffsetOf_Clear_header_not_0);
COMPILE_ASSERT(offsetof(Clear, buffers) == 4,
               OffsetOf_Clear_buffers_not_4);
COMPILE_ASSERT(offsetof(Clear, red) == 8,
               OffsetOf_Clear_red_not_8);
COMPILE_ASSERT(offsetof(Clear, green) == 12,
               OffsetOf_Clear_green_not_12);
COMPILE_ASSERT(offsetof(Clear, blue) == 16,
               OffsetOf_Clear_blue_not_16);
COMPILE_ASSERT(offsetof(Clear, alpha) == 20,
               OffsetOf_Clear_alpha_not_20);
COMPILE_ASSERT(offsetof(Clear, depth) == 24,
               OffsetOf_Clear_depth_not_24);
COMPILE_ASSERT(offsetof(Clear, stencil) == 28,
               OffsetOf_Clear_stencil_not_28);

struct SetViewport {
  typedef SetViewport ValueType;
  static const CommandId kCmdId = command_buffer::kSetViewport;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(
      uint32 _left,
      uint32 _top,
      uint32 _width,
      uint32 _height,
      float _z_min,
      float _z_max) {
    SetHeader();
    left = _left;
    top = _top;
    width = _width;
    height = _height;
    z_min = _z_min;
    z_max = _z_max;
  }

  static void* Set(void* cmd,
      uint32 left,
      uint32 top,
      uint32 width,
      uint32 height,
      float z_min,
      float z_max) {
    static_cast<ValueType*>(cmd)->Init(
      left,
      top,
      width,
      height,
      z_min,
      z_max);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 left;
  uint32 top;
  uint32 width;
  uint32 height;
  float z_min;
  float z_max;
};

COMPILE_ASSERT(sizeof(SetViewport) == 28, Sizeof_SetViewport_is_not_28);
COMPILE_ASSERT(offsetof(SetViewport, header) == 0,
               OffsetOf_SetViewport_header_not_0);
COMPILE_ASSERT(offsetof(SetViewport, left) == 4,
               OffsetOf_SetViewport_left_not_4);
COMPILE_ASSERT(offsetof(SetViewport, top) == 8,
               OffsetOf_SetViewport_top_not_8);
COMPILE_ASSERT(offsetof(SetViewport, width) == 12,
               OffsetOf_SetViewport_width_not_12);
COMPILE_ASSERT(offsetof(SetViewport, height) == 16,
               OffsetOf_SetViewport_height_not_16);
COMPILE_ASSERT(offsetof(SetViewport, z_min) == 20,
               OffsetOf_SetViewport_z_min_not_20);
COMPILE_ASSERT(offsetof(SetViewport, z_max) == 24,
               OffsetOf_SetViewport_z_max_not_24);

struct CreateVertexBuffer {
  typedef CreateVertexBuffer ValueType;
  static const CommandId kCmdId = command_buffer::kCreateVertexBuffer;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _size, uint32 _flags) {
    SetHeader();
    id = _id;
    size = _size;
    flags = _flags;
  }

  static void* Set(void* cmd, uint32 id, uint32 size, uint32 flags) {
    static_cast<ValueType*>(cmd)->Init(id, size, flags);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  uint32 size;
  uint32 flags;
};

COMPILE_ASSERT(sizeof(CreateVertexBuffer) == 16,
               Sizeof_CreateVertexBuffer_is_not_16);
COMPILE_ASSERT(offsetof(CreateVertexBuffer, header) == 0,
               OffsetOf_CreateVertexBuffer_header_not_0);
COMPILE_ASSERT(offsetof(CreateVertexBuffer, id) == 4,
               OffsetOf_CreateVertexBuffer_id_not_4);
COMPILE_ASSERT(offsetof(CreateVertexBuffer, size) == 8,
               OffsetOf_CreateVertexBuffer_size_not_8);
COMPILE_ASSERT(offsetof(CreateVertexBuffer, flags) == 12,
               OffsetOf_CreateVertexBuffer_flags_not_12);

struct DestroyVertexBuffer {
  typedef DestroyVertexBuffer ValueType;
  static const CommandId kCmdId = command_buffer::kDestroyVertexBuffer;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id) {
    SetHeader();
    id = _id;
  }

  static void* Set(void* cmd, uint32 id) {
    static_cast<ValueType*>(cmd)->Init(id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
};

COMPILE_ASSERT(sizeof(DestroyVertexBuffer) == 8,
               Sizeof_DestroyVertexBuffer_is_not_8);
COMPILE_ASSERT(offsetof(DestroyVertexBuffer, header) == 0,
               OffsetOf_DestroyVertexBuffer_header_not_0);
COMPILE_ASSERT(offsetof(DestroyVertexBuffer, id) == 4,
               OffsetOf_DestroyVertexBuffer_id_not_4);

struct SetVertexBufferDataImmediate {
  typedef SetVertexBufferDataImmediate ValueType;
  static const CommandId kCmdId = command_buffer::kSetVertexBufferDataImmediate;
  static const ArgFlags kArgFlags = kAtLeastN;

  void SetHeader(uint32 size) {
    header.SetCmdBySize<ValueType>(size);
  }

  void Init(uint32 _id, uint32 _offset, const void* data, uint32 size) {
    SetHeader(size);
    id = _id;
    offset = _offset;
    memcpy(ImmediateDataAddress(this), data, size);
  }

  static void* Set(void* cmd, uint32 id, uint32 offset,
                   const void* data, uint32 size) {
    static_cast<ValueType*>(cmd)->Init(id, offset, data, size);
    return NextImmediateCmdAddress<ValueType>(cmd, size);
  }

  CommandHeader header;
  uint32 id;
  uint32 offset;
};

COMPILE_ASSERT(sizeof(SetVertexBufferDataImmediate) == 12,
               Sizeof_SetVertexBufferDataImmediate_is_not_12);
COMPILE_ASSERT(offsetof(SetVertexBufferDataImmediate, header) == 0,
               OffsetOf_SetVertexBufferDataImmediate_header_not_0);
COMPILE_ASSERT(offsetof(SetVertexBufferDataImmediate, id) == 4,
               OffsetOf_SetVertexBufferDataImmediate_id_not_4);
COMPILE_ASSERT(offsetof(SetVertexBufferDataImmediate, offset) == 8,
               OffsetOf_SetVertexBufferDataImmediate_offset_not_8);

struct SetVertexBufferData {
  typedef SetVertexBufferData ValueType;
  static const CommandId kCmdId = command_buffer::kSetVertexBufferData;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _offset, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    id = _id;
    offset = _offset;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, uint32 id, uint32 offset, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(id, offset, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  uint32 offset;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(SetVertexBufferData) == 24,
               Sizeof_SetVertexBufferData_is_not_24);
COMPILE_ASSERT(offsetof(SetVertexBufferData, header) == 0,
               OffsetOf_SetVertexBufferData_header_not_0);
COMPILE_ASSERT(offsetof(SetVertexBufferData, id) == 4,
               OffsetOf_SetVertexBufferData_id_not_4);
COMPILE_ASSERT(offsetof(SetVertexBufferData, offset) == 8,
               OffsetOf_SetVertexBufferData_offset_not_8);
COMPILE_ASSERT(offsetof(SetVertexBufferData, size) == 12,
               OffsetOf_SetVertexBufferData_size_not_12);
COMPILE_ASSERT(offsetof(SetVertexBufferData, shared_memory) == 16,
               OffsetOf_SetVertexBufferData_shared_memory_not_16);

struct GetVertexBufferData {
  typedef GetVertexBufferData ValueType;
  static const CommandId kCmdId = command_buffer::kGetVertexBufferData;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _offset, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    id = _id;
    offset = _offset;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, uint32 id, uint32 offset, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(id, offset, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  uint32 offset;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(GetVertexBufferData) == 24,
               Sizeof_GetVertexBufferData_is_not_24);
COMPILE_ASSERT(offsetof(GetVertexBufferData, header) == 0,
               OffsetOf_GetVertexBufferData_header_not_0);
COMPILE_ASSERT(offsetof(GetVertexBufferData, id) == 4,
               OffsetOf_GetVertexBufferData_id_not_4);
COMPILE_ASSERT(offsetof(GetVertexBufferData, offset) == 8,
               OffsetOf_GetVertexBufferData_offset_not_8);
COMPILE_ASSERT(offsetof(GetVertexBufferData, size) == 12,
               OffsetOf_GetVertexBufferData_size_not_12);
COMPILE_ASSERT(offsetof(GetVertexBufferData, shared_memory) == 16,
               OffsetOf_GetVertexBufferData_shared_memory_not_16);

struct CreateIndexBuffer {
  typedef CreateIndexBuffer ValueType;
  static const CommandId kCmdId = command_buffer::kCreateIndexBuffer;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _size, uint32 _flags) {
    SetHeader();
    id = _id;
    size = _size;
    flags = _flags;
  }

  static void* Set(void* cmd, uint32 id, uint32 size, uint32 flags) {
    static_cast<ValueType*>(cmd)->Init(id, size, flags);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  uint32 size;
  uint32 flags;
};

COMPILE_ASSERT(sizeof(CreateIndexBuffer) == 16,
               Sizeof_CreateIndexBuffer_is_not_16);
COMPILE_ASSERT(offsetof(CreateIndexBuffer, header) == 0,
               OffsetOf_CreateIndexBuffer_header_not_0);
COMPILE_ASSERT(offsetof(CreateIndexBuffer, id) == 4,
               OffsetOf_CreateIndexBuffer_id_not_4);
COMPILE_ASSERT(offsetof(CreateIndexBuffer, size) == 8,
               OffsetOf_CreateIndexBuffer_size_not_8);
COMPILE_ASSERT(offsetof(CreateIndexBuffer, flags) == 12,
               OffsetOf_CreateIndexBuffer_flags_not_12);

struct DestroyIndexBuffer {
  typedef DestroyIndexBuffer ValueType;
  static const CommandId kCmdId = command_buffer::kDestroyIndexBuffer;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id) {
    SetHeader();
    id = _id;
  }

  static void* Set(void* cmd, uint32 id) {
    static_cast<ValueType*>(cmd)->Init(id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
};

COMPILE_ASSERT(sizeof(DestroyIndexBuffer) == 8,
               Sizeof_DestroyIndexBuffer_is_not_8);
COMPILE_ASSERT(offsetof(DestroyIndexBuffer, header) == 0,
               OffsetOf_DestroyIndexBuffer_header_not_0);
COMPILE_ASSERT(offsetof(DestroyIndexBuffer, id) == 4,
               OffsetOf_DestroyIndexBuffer_id_not_4);

struct SetIndexBufferDataImmediate {
  typedef SetIndexBufferDataImmediate ValueType;
  static const CommandId kCmdId = command_buffer::kSetIndexBufferDataImmediate;
  static const ArgFlags kArgFlags = kAtLeastN;

  void SetHeader(uint32 size) {
    header.SetCmdBySize<ValueType>(size);
  }

  void Init(uint32 _id, uint32 _offset, const void* data, uint32 size) {
    SetHeader(size);
    id = _id;
    offset = _offset;
    memcpy(ImmediateDataAddress(this), data, size);
  }

  static void* Set(void* cmd, uint32 id, uint32 offset, const void* data,
                   uint32 size) {
    static_cast<ValueType*>(cmd)->Init(id, offset, data, size);
    return NextImmediateCmdAddress<ValueType>(cmd, size);
  }

  CommandHeader header;
  uint32 id;
  uint32 offset;
};

COMPILE_ASSERT(sizeof(SetIndexBufferDataImmediate) == 12,
               Sizeof_SetIndexBufferDataImmediate_is_not_12);
COMPILE_ASSERT(offsetof(SetIndexBufferDataImmediate, header) == 0,
               OffsetOf_SetIndexBufferDataImmediate_header_not_0);
COMPILE_ASSERT(offsetof(SetIndexBufferDataImmediate, id) == 4,
               OffsetOf_SetIndexBufferDataImmediate_id_not_4);
COMPILE_ASSERT(offsetof(SetIndexBufferDataImmediate, offset) == 8,
               OffsetOf_SetIndexBufferDataImmediate_offset_not_8);

struct SetIndexBufferData {
  typedef SetIndexBufferData ValueType;
  static const CommandId kCmdId = command_buffer::kSetIndexBufferData;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _offset, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    id = _id;
    offset = _offset;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, uint32 id, uint32 offset, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(id, offset, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  uint32 offset;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(SetIndexBufferData) == 24,
               Sizeof_SetIndexBufferData_is_not_24);
COMPILE_ASSERT(offsetof(SetIndexBufferData, header) == 0,
               OffsetOf_SetIndexBufferData_header_not_0);
COMPILE_ASSERT(offsetof(SetIndexBufferData, id) == 4,
               OffsetOf_SetIndexBufferData_id_not_4);
COMPILE_ASSERT(offsetof(SetIndexBufferData, offset) == 8,
               OffsetOf_SetIndexBufferData_offset_not_8);
COMPILE_ASSERT(offsetof(SetIndexBufferData, size) == 12,
               OffsetOf_SetIndexBufferData_size_not_12);
COMPILE_ASSERT(offsetof(SetIndexBufferData, shared_memory) == 16,
               OffsetOf_SetIndexBufferData_shared_memory_not_16);

struct GetIndexBufferData {
  typedef GetIndexBufferData ValueType;
  static const CommandId kCmdId = command_buffer::kGetIndexBufferData;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _offset, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    id = _id;
    offset = _offset;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, uint32 id, uint32 offset, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(id, offset, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  uint32 offset;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(GetIndexBufferData) == 24,
               Sizeof_GetIndexBufferData_is_not_24);
COMPILE_ASSERT(offsetof(GetIndexBufferData, header) == 0,
               OffsetOf_GetIndexBufferData_header_not_0);
COMPILE_ASSERT(offsetof(GetIndexBufferData, id) == 4,
               OffsetOf_GetIndexBufferData_id_not_4);
COMPILE_ASSERT(offsetof(GetIndexBufferData, offset) == 8,
               OffsetOf_GetIndexBufferData_offset_not_8);
COMPILE_ASSERT(offsetof(GetIndexBufferData, size) == 12,
               OffsetOf_GetIndexBufferData_size_not_12);
COMPILE_ASSERT(offsetof(GetIndexBufferData, shared_memory) == 16,
               OffsetOf_GetIndexBufferData_shared_memory_not_16);

struct CreateVertexStruct {
  typedef CreateVertexStruct ValueType;
  static const CommandId kCmdId = command_buffer::kCreateVertexStruct;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _input_count) {
    SetHeader();
    id = _id;
    input_count = _input_count;
  }

  static void* Set(void* cmd, uint32 id, uint32 input_count) {
    static_cast<ValueType*>(cmd)->Init(id, input_count);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  uint32 input_count;
};

COMPILE_ASSERT(sizeof(CreateVertexStruct) == 12,
               Sizeof_CreateVertexStruct_is_not_12);
COMPILE_ASSERT(offsetof(CreateVertexStruct, header) == 0,
               OffsetOf_CreateVertexStruct_header_not_0);
COMPILE_ASSERT(offsetof(CreateVertexStruct, id) == 4,
               OffsetOf_CreateVertexStruct_id_not_4);
COMPILE_ASSERT(offsetof(CreateVertexStruct, input_count) == 8,
               OffsetOf_CreateVertexStruct_input_count_not_8);

struct DestroyVertexStruct {
  typedef DestroyVertexStruct ValueType;
  static const CommandId kCmdId = command_buffer::kDestroyVertexStruct;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id) {
    SetHeader();
    id = _id;
  }

  static void* Set(void* cmd, uint32 id) {
    static_cast<ValueType*>(cmd)->Init(id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
};

COMPILE_ASSERT(sizeof(DestroyVertexStruct) == 8,
               Sizeof_DestroyVertexStruct_is_not_8);
COMPILE_ASSERT(offsetof(DestroyVertexStruct, header) == 0,
               OffsetOf_DestroyVertexStruct_header_not_0);
COMPILE_ASSERT(offsetof(DestroyVertexStruct, id) == 4,
               OffsetOf_DestroyVertexStruct_id_not_4);

struct SetVertexInput {
  typedef SetVertexInput ValueType;
  static const CommandId kCmdId = command_buffer::kSetVertexInput;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _vertex_struct_id,
            uint32 _input_index,
            uint32 _vertex_buffer_id,
            uint32 _offset,
            uint8 _semantic,
            uint32 _semantic_index,
            uint8 _type,
            uint32 _stride) {
    SetHeader();
    vertex_struct_id = _vertex_struct_id;
    input_index = _input_index;
    vertex_buffer_id = _vertex_buffer_id;
    offset = _offset;
    fixme4 =
        set_vertex_input_cmd::Semantic::MakeValue(_semantic) |
        set_vertex_input_cmd::SemanticIndex::MakeValue(_semantic_index) |
        set_vertex_input_cmd::Type::MakeValue(_type) |
        set_vertex_input_cmd::Stride::MakeValue(_stride);
  }

  static void* Set(
      void* cmd,
      uint32 vertex_struct_id,
      uint32 input_index,
      uint32 vertex_buffer_id,
      uint32 offset,
      uint8 semantic,
      uint32 semantic_index,
      uint8 type,
      uint32 stride) {
    static_cast<ValueType*>(cmd)->Init(
        vertex_struct_id,
        input_index,
        vertex_buffer_id,
        offset,
        semantic,
        semantic_index,
        type,
        stride);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 vertex_struct_id;
  uint32 input_index;
  uint32 vertex_buffer_id;
  uint32 offset;
  uint32 fixme4;
};

COMPILE_ASSERT(sizeof(SetVertexInput) == 24,
               Sizeof_SetVertexInput_is_not_24);
COMPILE_ASSERT(offsetof(SetVertexInput, header) == 0,
               OffsetOf_SetVertexInput_header_not_0);
COMPILE_ASSERT(offsetof(SetVertexInput, vertex_struct_id) == 4,
               OffsetOf_SetVertexInput_vertex_struct_id_not_4);
COMPILE_ASSERT(offsetof(SetVertexInput, input_index) == 8,
               OffsetOf_SetVertexInput_input_index_not_8);
COMPILE_ASSERT(offsetof(SetVertexInput, vertex_buffer_id) == 12,
               OffsetOf_SetVertexInput_vertex_buffer_id_not_12);
COMPILE_ASSERT(offsetof(SetVertexInput, offset) == 16,
               OffsetOf_SetVertexInput_offset_not_16);
COMPILE_ASSERT(offsetof(SetVertexInput, fixme4) == 20,
               OffsetOf_SetVertexInput_fixme4_not_20);

struct SetVertexStruct {
  typedef SetVertexStruct ValueType;
  static const CommandId kCmdId = command_buffer::kSetVertexStruct;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id) {
    SetHeader();
    id = _id;
  }

  static void* Set(void* cmd, uint32 id) {
    static_cast<ValueType*>(cmd)->Init(id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
};

COMPILE_ASSERT(sizeof(SetVertexStruct) == 8,
               Sizeof_SetVertexStruct_is_not_8);
COMPILE_ASSERT(offsetof(SetVertexStruct, header) == 0,
               OffsetOf_SetVertexStruct_header_not_0);
COMPILE_ASSERT(offsetof(SetVertexStruct, id) == 4,
               OffsetOf_SetVertexStruct_id_not_4);

struct Draw {
  typedef Draw ValueType;
  static const CommandId kCmdId = command_buffer::kDraw;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _primitive_type, uint32 _first, uint32 _count) {
    SetHeader();
    primitive_type = _primitive_type;
    first = _first;
    count = _count;
  }

  static void* Set(void* cmd, uint32 primitive_type, uint32 first,
                   uint32 count) {
    static_cast<ValueType*>(cmd)->Init(primitive_type, first, count);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 primitive_type;
  uint32 first;
  uint32 count;
};

COMPILE_ASSERT(sizeof(Draw) == 16, Sizeof_DRAW_is_not_16);
COMPILE_ASSERT(offsetof(Draw, header) == 0,
               OffsetOf_Draw_header_not_0);
COMPILE_ASSERT(offsetof(Draw, primitive_type) == 4,
               OffsetOf_Draw_primitive_type_not_4);
COMPILE_ASSERT(offsetof(Draw, first) == 8,
               OffsetOf_Draw_first_not_8);
COMPILE_ASSERT(offsetof(Draw, count) == 12,
               OffsetOf_Draw_count_not_12);

struct DrawIndexed {
  typedef DrawIndexed ValueType;
  static const CommandId kCmdId = command_buffer::kDrawIndexed;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(
      uint32 _primitive_type,
      uint32 _index_buffer_id,
      uint32 _first,
      uint32 _count,
      uint32 _min_index,
      uint32 _max_index) {
    SetHeader();
    primitive_type = _primitive_type;
    index_buffer_id = _index_buffer_id;
    first = _first;
    count = _count;
    min_index = _min_index;
    max_index = _max_index;
  }

  static void* Set(void* cmd,
      uint32 primitive_type,
      uint32 index_buffer_id,
      uint32 first,
      uint32 count,
      uint32 min_index,
      uint32 max_index) {
    static_cast<ValueType*>(cmd)->Init(
        primitive_type,
        index_buffer_id,
        first,
        count,
        min_index,
        max_index);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 primitive_type;
  uint32 index_buffer_id;
  uint32 first;
  uint32 count;
  uint32 min_index;
  uint32 max_index;
};

COMPILE_ASSERT(sizeof(DrawIndexed) == 28, Sizeof_DrawIndexed_is_not_28);
COMPILE_ASSERT(offsetof(DrawIndexed, header) == 0,
               OffsetOf_DrawIndexed_header_not_0);
COMPILE_ASSERT(offsetof(DrawIndexed, primitive_type) == 4,
               OffsetOf_DrawIndexed_primitive_type_not_4);
COMPILE_ASSERT(offsetof(DrawIndexed, index_buffer_id) == 8,
               OffsetOf_DrawIndexed_index_buffer_id_not_8);
COMPILE_ASSERT(offsetof(DrawIndexed, first) == 12,
               OffsetOf_DrawIndexed_first_not_12);
COMPILE_ASSERT(offsetof(DrawIndexed, count) == 16,
               OffsetOf_DrawIndexed_count_not_16);
COMPILE_ASSERT(offsetof(DrawIndexed, min_index) == 20,
               OffsetOf_DrawIndexed_min_index_not_20);
COMPILE_ASSERT(offsetof(DrawIndexed, max_index) == 24,
               OffsetOf_DrawIndexed_max_index_not_24);

struct CreateEffect {
  typedef CreateEffect ValueType;
  static const CommandId kCmdId = command_buffer::kCreateEffect;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    id = _id;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, uint32 id, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(id, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(CreateEffect) == 20, Sizeof_CreateEffect_is_not_20);
COMPILE_ASSERT(offsetof(CreateEffect, header) == 0,
               OffsetOf_CreateEffect_header_not_0);
COMPILE_ASSERT(offsetof(CreateEffect, id) == 4,
               OffsetOf_CreateEffect_id_not_4);
COMPILE_ASSERT(offsetof(CreateEffect, size) == 8,
               OffsetOf_CreateEffect_size_not_8);
COMPILE_ASSERT(offsetof(CreateEffect, shared_memory) == 12,
               OffsetOf_CreateEffect_shared_memory_not_12);

struct CreateEffectImmediate {
  typedef CreateEffectImmediate ValueType;
  static const CommandId kCmdId = command_buffer::kCreateEffectImmediate;
  static const ArgFlags kArgFlags = kAtLeastN;

  void SetHeader(uint32 size) {
    header.SetCmdBySize<ValueType>(size);
  }

  void Init(uint32 _id, uint32 _size, const void* data) {
    SetHeader(_size);
    id = _id;
    size = _size;
  }

  static void* Set(void* cmd, uint32 id, uint32 size, const void* data) {
    static_cast<ValueType*>(cmd)->Init(id, size, data);
    return NextImmediateCmdAddress<ValueType>(cmd, size);
  }

  CommandHeader header;
  uint32 id;
  uint32 size;
};

COMPILE_ASSERT(sizeof(CreateEffectImmediate) == 12,
               Sizeof_CreateEffectImmediate_is_not_12);
COMPILE_ASSERT(offsetof(CreateEffectImmediate, header) == 0,
               OffsetOf_CreateEffectImmediate_header_not_0);
COMPILE_ASSERT(offsetof(CreateEffectImmediate, id) == 4,
               OffsetOf_CreateEffectImmediate_id_not_4);
COMPILE_ASSERT(offsetof(CreateEffectImmediate, size) == 8,
               OffsetOf_CreateEffectImmediate_size_not_8);

struct DestroyEffect {
  typedef DestroyEffect ValueType;
  static const CommandId kCmdId = command_buffer::kDestroyEffect;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id) {
    SetHeader();
    id = _id;
  }

  static void* Set(void* cmd, uint32 id) {
    static_cast<ValueType*>(cmd)->Init(id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
};

COMPILE_ASSERT(sizeof(DestroyEffect) == 8, Sizeof_DestroyEffect_is_not_8);
COMPILE_ASSERT(offsetof(DestroyEffect, header) == 0,
               OffsetOf_DestroyEffect_header_not_0);
COMPILE_ASSERT(offsetof(DestroyEffect, id) == 4,
               OffsetOf_DestroyEffect_id_not_4);

struct SetEffect {
  typedef SetEffect ValueType;
  static const CommandId kCmdId = command_buffer::kSetEffect;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id) {
    SetHeader();
    id = _id;
  }

  static void* Set(void* cmd, uint32 id) {
    static_cast<ValueType*>(cmd)->Init(id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
};

COMPILE_ASSERT(sizeof(SetEffect) == 8, Sizeof_SetEffect_is_not_8);
COMPILE_ASSERT(offsetof(SetEffect, header) == 0,
               OffsetOf_SetEffect_header_not_0);
COMPILE_ASSERT(offsetof(SetEffect, id) == 4,
               OffsetOf_SetEffect_id_not_4);

struct GetParamCount {
  typedef GetParamCount ValueType;
  static const CommandId kCmdId = command_buffer::kGetParamCount;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    id = _id;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, uint32 id, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(id, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(GetParamCount) == 20, Sizeof_GetParamCount_is_not_20);
COMPILE_ASSERT(offsetof(GetParamCount, header) == 0,
               OffsetOf_GetParamCount_header_not_0);
COMPILE_ASSERT(offsetof(GetParamCount, id) == 4,
               OffsetOf_GetParamCount_id_not_4);
COMPILE_ASSERT(offsetof(GetParamCount, size) == 8,
               OffsetOf_GetParamCount_size_not_8);
COMPILE_ASSERT(offsetof(GetParamCount, shared_memory) == 12,
               OffsetOf_GetParamCount_shared_memory_not_12);

struct CreateParam {
  typedef CreateParam ValueType;
  static const CommandId kCmdId = command_buffer::kCreateParam;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _param_id, uint32 _effect_id, uint32 _index) {
    SetHeader();
    param_id = _param_id;
    effect_id = _effect_id;
    index = _index;
  }

  static void* Set(void* cmd,
                   uint32 param_id, uint32 effect_id, uint32 index) {
    static_cast<ValueType*>(cmd)->Init(param_id, effect_id, index);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 param_id;
  uint32 effect_id;
  uint32 index;
};

COMPILE_ASSERT(sizeof(CreateParam) == 16, Sizeof_CreateParam_is_not_16);
COMPILE_ASSERT(offsetof(CreateParam, header) == 0,
               OffsetOf_CreateParam_header_not_0);
COMPILE_ASSERT(offsetof(CreateParam, param_id) == 4,
               OffsetOf_CreateParam_param_id_not_4);
COMPILE_ASSERT(offsetof(CreateParam, effect_id) == 8,
               OffsetOf_CreateParam_effect_id_not_8);
COMPILE_ASSERT(offsetof(CreateParam, index) == 12,
               OffsetOf_CreateParam_index_not_12);

struct CreateParamByName {
  typedef CreateParamByName ValueType;
  static const CommandId kCmdId = command_buffer::kCreateParamByName;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _param_id, uint32 _effect_id, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    param_id = _param_id;
    effect_id = _effect_id;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, uint32 param_id, uint32 effect_id, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(param_id, effect_id, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 param_id;
  uint32 effect_id;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(CreateParamByName) == 24,
               Sizeof_CreateParamByName_is_not_24);
COMPILE_ASSERT(offsetof(CreateParamByName, header) == 0,
               OffsetOf_CreateParamByName_header_not_0);
COMPILE_ASSERT(offsetof(CreateParamByName, param_id) == 4,
               OffsetOf_CreateParamByName_param_id_not_4);
COMPILE_ASSERT(offsetof(CreateParamByName, effect_id) == 8,
               OffsetOf_CreateParamByName_effect_id_not_8);
COMPILE_ASSERT(offsetof(CreateParamByName, size) == 12,
               OffsetOf_CreateParamByName_size_not_12);
COMPILE_ASSERT(offsetof(CreateParamByName, shared_memory) == 16,
               OffsetOf_CreateParamByName_shared_memory_not_16);

struct CreateParamByNameImmediate {
  typedef CreateParamByNameImmediate ValueType;
  static const CommandId kCmdId = command_buffer::kCreateParamByNameImmediate;
  static const ArgFlags kArgFlags = kAtLeastN;

  void SetHeader(uint32 size) {
    header.SetCmdBySize<ValueType>(size);
  }

  void Init(uint32 _param_id, uint32 _effect_id, uint32 _size,
            const void* data) {
    SetHeader(_size);
    param_id = _param_id;
    effect_id = _effect_id;
    size = _size;
    memcpy(ImmediateDataAddress(this), data, _size);
  }

  static void* Set(void* cmd, uint32 param_id, uint32 effect_id, uint32 size,
                   const void* data) {
    static_cast<ValueType*>(cmd)->Init(param_id, effect_id, size, data);
    return NextImmediateCmdAddress<ValueType>(cmd, size);
  }

  CommandHeader header;
  uint32 param_id;
  uint32 effect_id;
  uint32 size;
};

COMPILE_ASSERT(sizeof(CreateParamByNameImmediate) == 16,
               Sizeof_CreateParamByNameImmediate_is_not_16);
COMPILE_ASSERT(offsetof(CreateParamByNameImmediate, header) == 0,
               OffsetOf_CreateParamByNameImmediate_header_not_0);
COMPILE_ASSERT(offsetof(CreateParamByNameImmediate, param_id) == 4,
               OffsetOf_CreateParamByNameImmediate_param_id_not_4);
COMPILE_ASSERT(offsetof(CreateParamByNameImmediate, effect_id) == 8,
               OffsetOf_CreateParamByNameImmediate_effect_id_not_8);
COMPILE_ASSERT(offsetof(CreateParamByNameImmediate, size) == 12,
               OffsetOf_CreateParamByNameImmediate_size_not_12);

struct DestroyParam {
  typedef DestroyParam ValueType;
  static const CommandId kCmdId = command_buffer::kDestroyParam;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id) {
    SetHeader();
    id = _id;
  }

  static void* Set(void* cmd, uint32 id) {
    static_cast<ValueType*>(cmd)->Init(id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
};

COMPILE_ASSERT(sizeof(DestroyParam) == 8, Sizeof_DestroyParam_is_not_8);
COMPILE_ASSERT(offsetof(DestroyParam, header) == 0,
               OffsetOf_DestroyParam_header_not_0);
COMPILE_ASSERT(offsetof(DestroyParam, id) == 4,
               OffsetOf_DestroyParam_id_not_4);

struct SetParamData {
  typedef SetParamData ValueType;
  static const CommandId kCmdId = command_buffer::kSetParamData;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    id = _id;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, uint32 id, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(id, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(SetParamData) == 20, Sizeof_SetParamData_is_not_20);
COMPILE_ASSERT(offsetof(SetParamData, header) == 0,
               OffsetOf_SetParamData_header_not_0);
COMPILE_ASSERT(offsetof(SetParamData, id) == 4,
               OffsetOf_SetParamData_id_not_4);
COMPILE_ASSERT(offsetof(SetParamData, size) == 8,
               OffsetOf_SetParamData_size_not_8);
COMPILE_ASSERT(offsetof(SetParamData, shared_memory) == 12,
               OffsetOf_SetParamData_shared_memory_not_12);

struct SetParamDataImmediate {
  typedef SetParamDataImmediate ValueType;
  static const CommandId kCmdId = command_buffer::kSetParamDataImmediate;
  static const ArgFlags kArgFlags = kAtLeastN;

  void SetHeader(uint32 size) {
    header.SetCmdBySize<ValueType>(size);
  }

  void Init(uint32 _id, uint32 _size, const void* data) {
    SetHeader(_size);
    id = _id;
    size = _size;
    memcpy(ImmediateDataAddress(this), data, _size);
  }

  static void* Set(void* cmd, uint32 id, uint32 size, const void* data) {
    static_cast<ValueType*>(cmd)->Init(id, size, data);
    return NextImmediateCmdAddress<ValueType>(cmd, size);
  }

  CommandHeader header;
  uint32 id;
  uint32 size;
};

COMPILE_ASSERT(sizeof(SetParamDataImmediate) == 12,
               Sizeof_SetParamDataImmediate_is_not_12);
COMPILE_ASSERT(offsetof(SetParamDataImmediate, header) == 0,
               OffsetOf_SetParamDataImmediate_header_not_0);
COMPILE_ASSERT(offsetof(SetParamDataImmediate, id) == 4,
               OffsetOf_SetParamDataImmediate_id_not_4);
COMPILE_ASSERT(offsetof(SetParamDataImmediate, size) == 8,
               OffsetOf_SetParamDataImmediate_size_not_8);

struct GetParamDesc {
  typedef GetParamDesc ValueType;
  static const CommandId kCmdId = command_buffer::kGetParamDesc;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    id = _id;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, uint32 id, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(id, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(GetParamDesc) == 20, Sizeof_GetParamDesc_is_not_20);
COMPILE_ASSERT(offsetof(GetParamDesc, header) == 0,
               OffsetOf_GetParamDesc_header_not_0);
COMPILE_ASSERT(offsetof(GetParamDesc, id) == 4,
               OffsetOf_GetParamDesc_id_not_4);
COMPILE_ASSERT(offsetof(GetParamDesc, size) == 8,
               OffsetOf_GetParamDesc_size_not_8);
COMPILE_ASSERT(offsetof(GetParamDesc, shared_memory) == 12,
               OffsetOf_GetParamDesc_shared_memory_not_12);

struct GetStreamCount {
  typedef GetStreamCount ValueType;
  static const CommandId kCmdId = command_buffer::kGetStreamCount;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    id = _id;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, uint32 id, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(id, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(GetStreamCount) == 20,
               Sizeof_GetStreamCount_is_not_20);
COMPILE_ASSERT(offsetof(GetStreamCount, header) == 0,
               OffsetOf_GetStreamCount_header_not_0);
COMPILE_ASSERT(offsetof(GetStreamCount, id) == 4,
               OffsetOf_GetStreamCount_id_not_4);
COMPILE_ASSERT(offsetof(GetStreamCount, size) == 8,
               OffsetOf_GetStreamCount_size_not_8);
COMPILE_ASSERT(offsetof(GetStreamCount, shared_memory) == 12,
               OffsetOf_GetStreamCount_shared_memory_not_12);

struct GetStreamDesc {
  typedef GetStreamDesc ValueType;
  static const CommandId kCmdId = command_buffer::kGetStreamDesc;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _index, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    id = _id;
    index = _index;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, uint32 id, uint32 index, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(id, index, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  uint32 index;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(GetStreamDesc) == 24, Sizeof_GetStreamDesc_is_not_24);
COMPILE_ASSERT(offsetof(GetStreamDesc, header) == 0,
               OffsetOf_GetStreamDesc_header_not_0);
COMPILE_ASSERT(offsetof(GetStreamDesc, id) ==4 ,
               OffsetOf_GetStreamDesc_id_not_4);
COMPILE_ASSERT(offsetof(GetStreamDesc, index) == 8,
               OffsetOf_GetStreamDesc_index_not_8);
COMPILE_ASSERT(offsetof(GetStreamDesc, size) == 12,
               OffsetOf_GetStreamDesc_size_not_12);
COMPILE_ASSERT(offsetof(GetStreamDesc, shared_memory) == 16,
               OffsetOf_GetStreamDesc_shared_memory_not_16);

struct DestroyTexture {
  typedef DestroyTexture ValueType;
  static const CommandId kCmdId = command_buffer::kDestroyTexture;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id) {
    SetHeader();
    id = _id;
  }

  static void* Set(void* cmd, uint32 id) {
    static_cast<ValueType*>(cmd)->Init(id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
};

COMPILE_ASSERT(sizeof(DestroyTexture) == 8, Sizeof_DestroyTexture_is_not_8);
COMPILE_ASSERT(offsetof(DestroyTexture, header) == 0,
               OffsetOf_DestroyTexture_header_not_0);
COMPILE_ASSERT(offsetof(DestroyTexture, id) == 4,
               OffsetOf_DestroyTexture_id_not_4);

struct CreateTexture2d {
  typedef CreateTexture2d ValueType;
  static const CommandId kCmdId = command_buffer::kCreateTexture2d;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _texture_id,
            uint32 _width, uint32 _height, uint32 _levels, uint32 _format,
            uint32 _enable_render_surfaces) {
    SetHeader();
    texture_id = _texture_id;
    fixme1 =
        create_texture_2d_cmd::Width::MakeValue(_width) |
        create_texture_2d_cmd::Height::MakeValue(_height);
    fixme2 =
        create_texture_2d_cmd::Levels::MakeValue(_levels) |
        create_texture_2d_cmd::Format::MakeValue(_format) |
        create_texture_2d_cmd::Flags::MakeValue(_enable_render_surfaces);
  }

  static void* Set(void* cmd, uint32 texture_id,
                   uint32 width, uint32 height, uint32 levels, uint32 format,
                   uint32 enable_render_surfaces) {
    static_cast<ValueType*>(cmd)->Init(texture_id,
                                       width, height, levels, format,
                                       enable_render_surfaces);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 texture_id;
  uint32 fixme1;
  uint32 fixme2;
};

COMPILE_ASSERT(sizeof(CreateTexture2d) == 16,
               Sizeof_CreateTexture2d_is_not_16);
COMPILE_ASSERT(offsetof(CreateTexture2d, header) == 0,
               OffsetOf_CreateTexture2d_header_not_0);
COMPILE_ASSERT(offsetof(CreateTexture2d, texture_id) == 4,
               OffsetOf_CreateTexture2d_texture_id_not_4);
COMPILE_ASSERT(offsetof(CreateTexture2d, fixme1) == 8,
               OffsetOf_CreateTexture2d_fixme1_not_8);
COMPILE_ASSERT(offsetof(CreateTexture2d, fixme2) == 12,
               OffsetOf_CreateTexture2d_fixme2_not_12);

struct CreateTexture3d {
  typedef CreateTexture3d ValueType;
  static const CommandId kCmdId = command_buffer::kCreateTexture3d;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _texture_id,
            uint32 _width, uint32 _height, uint32 _depth,
            uint32 _levels, uint32 _format,
            uint32 _enable_render_surfaces) {
    SetHeader();
    texture_id = _texture_id;
    fixme1 =
        create_texture_3d_cmd::Width::MakeValue(_width) |
        create_texture_3d_cmd::Height::MakeValue(_height);
    fixme2 =
        create_texture_3d_cmd::Depth::MakeValue(_depth);
    fixme3 =
        create_texture_3d_cmd::Levels::MakeValue(_levels) |
        create_texture_3d_cmd::Format::MakeValue(_format) |
        create_texture_3d_cmd::Flags::MakeValue(_enable_render_surfaces);
  }

  static void* Set(void* cmd, uint32 texture_id,
                   uint32 width, uint32 height, uint32 depth,
                   uint32 levels, uint32 format,
                   uint32 enable_render_surfaces) {
    static_cast<ValueType*>(cmd)->Init(texture_id,
                                       width, height, depth,
                                       levels, format,
                                       enable_render_surfaces);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 texture_id;
  uint32 fixme1;
  uint32 fixme2;
  uint32 fixme3;
};

COMPILE_ASSERT(sizeof(CreateTexture3d) == 20,
               Sizeof_CreateTexture3d_is_not_20);
COMPILE_ASSERT(offsetof(CreateTexture3d, header) == 0,
               OffsetOf_CreateTexture3d_header_not_0);
COMPILE_ASSERT(offsetof(CreateTexture3d, texture_id) == 4,
               OffsetOf_CreateTexture3d_texture_id_not_4);
COMPILE_ASSERT(offsetof(CreateTexture3d, fixme1) == 8,
               OffsetOf_CreateTexture3d_fixme1_not_8);
COMPILE_ASSERT(offsetof(CreateTexture3d, fixme2) == 12,
               OffsetOf_CreateTexture3d_fixme2_not_12);
COMPILE_ASSERT(offsetof(CreateTexture3d, fixme3) == 16,
               OffsetOf_CreateTexture3d_fixme3_not_16);

struct CreateTextureCube {
  typedef CreateTextureCube ValueType;
  static const CommandId kCmdId = command_buffer::kCreateTextureCube;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _texture_id,
            uint32 _edge_length, uint32 _levels, uint32 _format,
            uint32 _enable_render_surfaces) {
    SetHeader();
    texture_id = _texture_id;
    edge_length = _edge_length;
    fixme2 =
        create_texture_2d_cmd::Levels::MakeValue(_levels) |
        create_texture_2d_cmd::Format::MakeValue(_format) |
        create_texture_2d_cmd::Flags::MakeValue(_enable_render_surfaces);
  }

  static void* Set(void* cmd, uint32 texture_id,
                   uint32 edge_length, uint32 levels, uint32 format,
                   uint32 enable_render_surfaces) {
    static_cast<ValueType*>(cmd)->Init(texture_id,
                                       edge_length, levels, format,
                                       enable_render_surfaces);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 texture_id;
  uint32 edge_length;
  uint32 fixme2;
};

COMPILE_ASSERT(sizeof(CreateTextureCube) == 16,
               Sizeof_CreateTextureCube_is_not_16);
COMPILE_ASSERT(offsetof(CreateTextureCube, header) == 0,
               OffsetOf_CreateTextureCube_header_not_0);
COMPILE_ASSERT(offsetof(CreateTextureCube, texture_id) == 4,
               OffsetOf_CreateTextureCube_texture_id_not_4);
COMPILE_ASSERT(offsetof(CreateTextureCube, edge_length) == 8,
               OffsetOf_CreateTextureCube_edge_length_not_8);
COMPILE_ASSERT(offsetof(CreateTextureCube, fixme2) == 12,
               OffsetOf_CreateTextureCube_fixme2_not_12);

struct SetTextureData {
  typedef SetTextureData ValueType;
  static const CommandId kCmdId = command_buffer::kSetTextureData;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(
      uint32 _texture_id,
      uint32 _x,
      uint32 _y,
      uint32 _z,
      uint32 _width,
      uint32 _height,
      uint32 _depth,
      uint32 _level,
      uint32 _face,
      uint32 _row_pitch,
      uint32 _slice_pitch,
      uint32 _size,
      uint32 shared_memory_id,
      uint32 shared_memory_offset) {
    SetHeader();
    texture_id = _texture_id;
    fixme1 =
        set_texture_data_cmd::X::MakeValue(_x) |
        set_texture_data_cmd::Y::MakeValue(_y);
    fixme2 =
        set_texture_data_cmd::Width::MakeValue(_width) |
        set_texture_data_cmd::Height::MakeValue(_height);
    fixme3 =
        set_texture_data_cmd::Z::MakeValue(_z) |
        set_texture_data_cmd::Depth::MakeValue(_depth);
    fixme4 =
        set_texture_data_cmd::Level::MakeValue(_level) |
        set_texture_data_cmd::Face::MakeValue(_face);
    row_pitch = _row_pitch;
    slice_pitch = _slice_pitch;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(
      void* cmd,
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
    static_cast<ValueType*>(cmd)->Init(
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
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 texture_id;
  uint32 fixme1;
  uint32 fixme2;
  uint32 fixme3;
  uint32 fixme4;
  uint32 row_pitch;
  uint32 slice_pitch;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(SetTextureData) == 44,
               Sizeof_SetTextureData_is_not_44);
COMPILE_ASSERT(offsetof(SetTextureData, header) == 0,
               OffsetOf_SetTextureData_header_not_0);
COMPILE_ASSERT(offsetof(SetTextureData, texture_id) == 4,
               OffsetOf_SetTextureData_texture_id_not_4);
COMPILE_ASSERT(offsetof(SetTextureData, fixme1) == 8,
               OffsetOf_SetTextureData_fixme1_not_8);
COMPILE_ASSERT(offsetof(SetTextureData, fixme2) == 12,
               OffsetOf_SetTextureData_fixme2_not_12);
COMPILE_ASSERT(offsetof(SetTextureData, fixme3) == 16,
               OffsetOf_SetTextureData_fixme3_not_16);
COMPILE_ASSERT(offsetof(SetTextureData, fixme4) == 20,
               OffsetOf_SetTextureData_fixme4_not_20);
COMPILE_ASSERT(offsetof(SetTextureData, row_pitch) == 24,
               OffsetOf_SetTextureData_row_pitch_not_24);
COMPILE_ASSERT(offsetof(SetTextureData, slice_pitch) == 28,
               OffsetOf_SetTextureData_slice_pitch_not_28);
COMPILE_ASSERT(offsetof(SetTextureData, size) == 32,
               OffsetOf_SetTextureData_size_not_32);
COMPILE_ASSERT(offsetof(SetTextureData, shared_memory) == 36,
               OffsetOf_SetTextureData_shared_memory_not_36);

struct SetTextureDataImmediate {
  typedef SetTextureDataImmediate ValueType;
  static const CommandId kCmdId = command_buffer::kSetTextureDataImmediate;
  static const ArgFlags kArgFlags = kAtLeastN;

  void SetHeader(uint32 size) {
    header.SetCmdBySize<ValueType>(size);
  }

  void Init(
      uint32 _texture_id,
      uint32 _x,
      uint32 _y,
      uint32 _z,
      uint32 _width,
      uint32 _height,
      uint32 _depth,
      uint32 _level,
      uint32 _face,
      uint32 _row_pitch,
      uint32 _slice_pitch,
      uint32 _size,
      const void* data) {
    SetHeader(_size);
    texture_id = _texture_id;
    fixme1 =
        set_texture_data_cmd::X::MakeValue(_x) |
        set_texture_data_cmd::Y::MakeValue(_y);
    fixme2 =
        set_texture_data_cmd::Width::MakeValue(_width) |
        set_texture_data_cmd::Height::MakeValue(_height);
    fixme3 =
        set_texture_data_cmd::Z::MakeValue(_z) |
        set_texture_data_cmd::Depth::MakeValue(_depth);
    fixme4 =
        set_texture_data_cmd::Level::MakeValue(_level) |
        set_texture_data_cmd::Face::MakeValue(_face);
    row_pitch = _row_pitch;
    slice_pitch = _slice_pitch;
    size = _size;
    memcpy(ImmediateDataAddress(this), data, _size);
  }

  static void* Set(
      void* cmd,
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
    static_cast<ValueType*>(cmd)->Init(
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
    return NextImmediateCmdAddress<ValueType>(cmd, size);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 texture_id;
  uint32 fixme1;
  uint32 fixme2;
  uint32 fixme3;
  uint32 fixme4;
  uint32 row_pitch;
  uint32 slice_pitch;
  uint32 size;
};

COMPILE_ASSERT(sizeof(SetTextureDataImmediate) == 36,
               Sizeof_SetTextureDataImmediate_is_not_36);
COMPILE_ASSERT(offsetof(SetTextureDataImmediate, header) == 0,
               OffsetOf_SetTextureDataImmediate_header_not_0);
COMPILE_ASSERT(offsetof(SetTextureDataImmediate, texture_id) == 4,
               OffsetOf_SetTextureDataImmediate_texture_id_not_4);
COMPILE_ASSERT(offsetof(SetTextureDataImmediate, fixme1) == 8,
               OffsetOf_SetTextureDataImmediate_fixme1_not_8);
COMPILE_ASSERT(offsetof(SetTextureDataImmediate, fixme2) == 12,
               OffsetOf_SetTextureDataImmediate_fixme2_not_12);
COMPILE_ASSERT(offsetof(SetTextureDataImmediate, fixme3) == 16,
               OffsetOf_SetTextureDataImmediate_fixme3_not_16);
COMPILE_ASSERT(offsetof(SetTextureDataImmediate, fixme4) == 20,
               OffsetOf_SetTextureDataImmediate_fixme4_not_20);
COMPILE_ASSERT(offsetof(SetTextureDataImmediate, row_pitch) == 24,
               OffsetOf_SetTextureDataImmediate_row_pitch_not_24);
COMPILE_ASSERT(offsetof(SetTextureDataImmediate, slice_pitch) == 28,
               OffsetOf_SetTextureDataImmediate_slice_pitch_not_28);
COMPILE_ASSERT(offsetof(SetTextureDataImmediate, size) == 32,
               OffsetOf_SetTextureDataImmediate_size_not_32);

struct GetTextureData {
  typedef GetTextureData ValueType;
  static const CommandId kCmdId = command_buffer::kGetTextureData;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(
      uint32 _texture_id,
      uint32 _x,
      uint32 _y,
      uint32 _z,
      uint32 _width,
      uint32 _height,
      uint32 _depth,
      uint32 _level,
      uint32 _face,
      uint32 _row_pitch,
      uint32 _slice_pitch,
      uint32 _size,
      uint32 shared_memory_id,
      uint32 shared_memory_offset) {
    SetHeader();
    texture_id = _texture_id;
    fixme1 =
        set_texture_data_cmd::X::MakeValue(_x) |
        set_texture_data_cmd::Y::MakeValue(_y);
    fixme2 =
        set_texture_data_cmd::Width::MakeValue(_width) |
        set_texture_data_cmd::Height::MakeValue(_height);
    fixme3 =
        set_texture_data_cmd::Z::MakeValue(_z) |
        set_texture_data_cmd::Depth::MakeValue(_depth);
    fixme4 =
        set_texture_data_cmd::Level::MakeValue(_level) |
        set_texture_data_cmd::Face::MakeValue(_face);
    row_pitch = _row_pitch;
    slice_pitch = _slice_pitch;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(
      void* cmd,
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
    static_cast<ValueType*>(cmd)->Init(
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
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 texture_id;
  uint32 fixme1;
  uint32 fixme2;
  uint32 fixme3;
  uint32 fixme4;
  uint32 row_pitch;
  uint32 slice_pitch;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(GetTextureData) == 44,
               Sizeof_GetTextureData_is_not_44);
COMPILE_ASSERT(offsetof(GetTextureData, header) == 0,
               OffsetOf_GetTextureData_header_not_0);
COMPILE_ASSERT(offsetof(GetTextureData, texture_id) == 4,
               OffsetOf_GetTextureData_texture_id_not_4);
COMPILE_ASSERT(offsetof(GetTextureData, fixme1) == 8,
               OffsetOf_GetTextureData_fixme1_not_8);
COMPILE_ASSERT(offsetof(GetTextureData, fixme2) == 12,
               OffsetOf_GetTextureData_fixme2_not_12);
COMPILE_ASSERT(offsetof(GetTextureData, fixme3) == 16,
               OffsetOf_GetTextureData_fixme3_not_16);
COMPILE_ASSERT(offsetof(GetTextureData, fixme4) == 20,
               OffsetOf_GetTextureData_fixme4_not_20);
COMPILE_ASSERT(offsetof(GetTextureData, row_pitch) == 24,
               OffsetOf_GetTextureData_row_pitch_not_24);
COMPILE_ASSERT(offsetof(GetTextureData, slice_pitch) == 28,
               OffsetOf_GetTextureData_slice_pitch_not_28);
COMPILE_ASSERT(offsetof(GetTextureData, size) == 32,
               OffsetOf_GetTextureData_size_not_32);
COMPILE_ASSERT(offsetof(GetTextureData, shared_memory) == 36,
               OffsetOf_GetTextureData_shared_memory_not_36);

struct CreateSampler {
  typedef CreateSampler ValueType;
  static const CommandId kCmdId = command_buffer::kCreateSampler;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id) {
    SetHeader();
    id = _id;
  }

  static void* Set(void* cmd, uint32 id) {
    static_cast<ValueType*>(cmd)->Init(id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
};

COMPILE_ASSERT(sizeof(CreateSampler) == 8, Sizeof_CreateSampler_is_not_8);
COMPILE_ASSERT(offsetof(CreateSampler, header) == 0,
               OffsetOf_CreateSampler_header_not_0);
COMPILE_ASSERT(offsetof(CreateSampler, id) == 4,
               OffsetOf_CreateSampler_id_not_4);

struct DestroySampler {
  typedef DestroySampler ValueType;
  static const CommandId kCmdId = command_buffer::kDestroySampler;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id) {
    SetHeader();
    id = _id;
  }

  static void* Set(void* cmd, uint32 id) {
    static_cast<ValueType*>(cmd)->Init(id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
};

COMPILE_ASSERT(sizeof(DestroySampler) == 8, Sizeof_DestroySampler_is_not_8);
COMPILE_ASSERT(offsetof(DestroySampler, header) == 0,
               OffsetOf_DestroySampler_header_not_0);
COMPILE_ASSERT(offsetof(DestroySampler, id) == 4,
               OffsetOf_DestroySampler_id_not_4);

struct SetSamplerStates {
  typedef SetSamplerStates ValueType;
  static const CommandId kCmdId = command_buffer::kSetSamplerStates;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(
      uint32 _id,
      uint32 _address_u_value,
      uint32 _address_v_value,
      uint32 _address_w_value,
      uint32 _mag_filter_value,
      uint32 _min_filter_value,
      uint32 _mip_filter_value,
      uint32 _max_anisotropy) {
    SetHeader();
    id = _id;
    fixme1 =
        set_sampler_states::AddressingU::MakeValue(_address_u_value) |
        set_sampler_states::AddressingV::MakeValue(_address_v_value) |
        set_sampler_states::AddressingW::MakeValue(_address_w_value) |
        set_sampler_states::MagFilter::MakeValue(_mag_filter_value) |
        set_sampler_states::MinFilter::MakeValue(_min_filter_value) |
        set_sampler_states::MipFilter::MakeValue(_mip_filter_value) |
        set_sampler_states::MaxAnisotropy::MakeValue(_max_anisotropy);
  }

  static void* Set(void* cmd,
      uint32 id,
      uint32 address_u_value,
      uint32 address_v_value,
      uint32 address_w_value,
      uint32 mag_filter_value,
      uint32 min_filter_value,
      uint32 mip_filter_value,
      uint32 max_anisotropy) {
    static_cast<ValueType*>(cmd)->Init(
        id,
        address_u_value,
        address_v_value,
        address_w_value,
        mag_filter_value,
        min_filter_value,
        mip_filter_value,
        max_anisotropy);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 id;
  uint32 fixme1;
};

COMPILE_ASSERT(sizeof(SetSamplerStates) == 12,
               Sizeof_SetSamplerStates_is_not_12);
COMPILE_ASSERT(offsetof(SetSamplerStates, header) == 0,
               OffsetOf_SetSamplerStates_header_not_0);
COMPILE_ASSERT(offsetof(SetSamplerStates, id) == 4,
               OffsetOf_SetSamplerStates_id_not_4);
COMPILE_ASSERT(offsetof(SetSamplerStates, fixme1) == 8,
               OffsetOf_SetSamplerStates_fixme1_not_8);

struct SetSamplerBorderColor {
  typedef SetSamplerBorderColor ValueType;
  static const CommandId kCmdId = command_buffer::kSetSamplerBorderColor;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id,
            float _red, float _green, float _blue, float _alpha) {
    SetHeader();
    id = _id;
    red = _red;
    green = _green;
    blue = _blue;
    alpha = _alpha;
  }

  static void* Set(void* cmd, uint32 id,
                   float red, float green, float blue, float alpha) {
    static_cast<ValueType*>(cmd)->Init(id, red, green, blue, alpha);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  float red;
  float blue;
  float green;
  float alpha;
};

COMPILE_ASSERT(sizeof(SetSamplerBorderColor) == 24,
               Sizeof_SetSamplerBorderColor_is_not_24);
COMPILE_ASSERT(offsetof(SetSamplerBorderColor, header) == 0,
               OffsetOf_SetSamplerBorderColor_header_not_0);
COMPILE_ASSERT(offsetof(SetSamplerBorderColor, id) == 4,
               OffsetOf_SetSamplerBorderColor_id_not_4);
COMPILE_ASSERT(offsetof(SetSamplerBorderColor, red) == 8,
               OffsetOf_SetSamplerBorderColor_red_not_8);
COMPILE_ASSERT(offsetof(SetSamplerBorderColor, blue) == 12,
               OffsetOf_SetSamplerBorderColor_blue_not_12);
COMPILE_ASSERT(offsetof(SetSamplerBorderColor, green) == 16,
               OffsetOf_SetSamplerBorderColor_green_not_16);
COMPILE_ASSERT(offsetof(SetSamplerBorderColor, alpha) == 20,
               OffsetOf_SetSamplerBorderColor_alpha_not_20);

struct SetSamplerTexture {
  typedef SetSamplerTexture ValueType;
  static const CommandId kCmdId = command_buffer::kSetSamplerTexture;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _texture_id) {
    SetHeader();
    id = _id;
    texture_id = _texture_id;
  }

  static void* Set(void* cmd, uint32 id, uint32 texture_id) {
    static_cast<ValueType*>(cmd)->Init(id, texture_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
  uint32 texture_id;
};

COMPILE_ASSERT(sizeof(SetSamplerTexture) == 12,
               Sizeof_SetSamplerTexture_is_not_12);
COMPILE_ASSERT(offsetof(SetSamplerTexture, header) == 0,
               OffsetOf_SetSamplerTexture_header_not_0);
COMPILE_ASSERT(offsetof(SetSamplerTexture, id) == 4,
               OffsetOf_SetSamplerTexture_id_not_4);
COMPILE_ASSERT(offsetof(SetSamplerTexture, texture_id) == 8,
               OffsetOf_SetSamplerTexture_texture_id_not_8);

struct SetScissor {
  typedef SetScissor ValueType;
  static const CommandId kCmdId = command_buffer::kSetScissor;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _x,
            uint32 _y,
            uint32 _width,
            uint32 _height,
            bool _enable) {
    SetHeader();
    fixme0 =
        set_scissor::X::MakeValue(_x) |
        set_scissor::Y::MakeValue(_y) |
        set_scissor::Enable::MakeValue(_enable ? 1 : 0);
    fixme1 =
      set_scissor::Width::MakeValue(_width) |
      set_scissor::Height::MakeValue(_height);
  }

  static void* Set(
      void* cmd,
      uint32 x,
      uint32 y,
      uint32 width,
      uint32 height,
      bool enable) {
    static_cast<ValueType*>(cmd)->Init(
        x,
        y,
        width,
        height,
        enable);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 fixme0;
  uint32 fixme1;
};

COMPILE_ASSERT(sizeof(SetScissor) == 12, Sizeof_SetScissor_is_not_12);
COMPILE_ASSERT(offsetof(SetScissor, header) == 0,
               OffsetOf_SetScissor_header_not_0);
COMPILE_ASSERT(offsetof(SetScissor, fixme0) == 4,
               OffsetOf_SetScissor_fixme0_not_4);
COMPILE_ASSERT(offsetof(SetScissor, fixme1) == 8,
               OffsetOf_SetScissor_fixme1_not_8);

struct SetPolygonOffset {
  typedef SetPolygonOffset ValueType;
  static const CommandId kCmdId = command_buffer::kSetPolygonOffset;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(float _slope_factor, float _units) {
    SetHeader();
    slope_factor = _slope_factor;
    units = _units;
  }

  static void* Set(void* cmd, float slope_factor, float units) {
    static_cast<ValueType*>(cmd)->Init(slope_factor, units);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  float slope_factor;
  float units;
};

COMPILE_ASSERT(sizeof(SetPolygonOffset) == 12,
               Sizeof_SetPolygonOffset_is_not_12);
COMPILE_ASSERT(offsetof(SetPolygonOffset, header) == 0,
               OffsetOf_SetPolygonOffset_header_not_0);
COMPILE_ASSERT(offsetof(SetPolygonOffset, slope_factor) == 4,
               OffsetOf_SetPolygonOffset_slope_factor_not_4);
COMPILE_ASSERT(offsetof(SetPolygonOffset, units) == 8,
               OffsetOf_SetPolygonOffset_units_not_8);

struct SetPointLineRaster {
  typedef SetPointLineRaster ValueType;
  static const CommandId kCmdId = command_buffer::kSetPointLineRaster;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(bool _line_smooth_enable, bool _point_sprite_enable,
            float _point_size) {
    SetHeader();
    fixme0 =
        set_point_line_raster::LineSmoothEnable::MakeValue(
            _line_smooth_enable ? 1 : 0) |
        set_point_line_raster::PointSpriteEnable::MakeValue(
            _point_sprite_enable ? 1 : 0);
    point_size = _point_size;
  }

  static void* Set(void* cmd, bool line_smooth_enable, bool point_sprite_enable,
                   float point_size) {
    static_cast<ValueType*>(cmd)->Init(line_smooth_enable, point_sprite_enable,
                                       point_size);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 fixme0;
  float point_size;
};

COMPILE_ASSERT(sizeof(SetPointLineRaster) == 12,
               Sizeof_SetPointLineRaster_is_not_12);
COMPILE_ASSERT(offsetof(SetPointLineRaster, header) == 0,
               OffsetOf_SetPointLineRaster_header_not_0);
COMPILE_ASSERT(offsetof(SetPointLineRaster, fixme0) == 4,
               OffsetOf_SetPointLineRaster_fixme0_not_4);
COMPILE_ASSERT(offsetof(SetPointLineRaster, point_size) == 8,
               OffsetOf_SetPointLineRaster_point_size_not_8);

struct SetPolygonRaster {
  typedef SetPolygonRaster ValueType;
  static const CommandId kCmdId = command_buffer::kSetPolygonRaster;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _fill_mode, uint32 _cull_mode) {
    SetHeader();
    fixme0 =
        set_polygon_raster::FillMode::MakeValue(_fill_mode) |
        set_polygon_raster::CullMode::MakeValue(_cull_mode);
  }

  static void* Set(void* cmd, uint32 fill_mode, uint32 cull_mode) {
    static_cast<ValueType*>(cmd)->Init(fill_mode, cull_mode);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 fixme0;
};

COMPILE_ASSERT(sizeof(SetPolygonRaster) == 8,
               Sizeof_SetPolygonRaster_is_not_8);
COMPILE_ASSERT(offsetof(SetPolygonRaster, header) == 0,
               OffsetOf_SetPolygonRaster_header_not_0);
COMPILE_ASSERT(offsetof(SetPolygonRaster, fixme0) == 4,
               OffsetOf_SetPolygonRaster_fixme0_not_4);

struct SetAlphaTest {
  typedef SetAlphaTest ValueType;
  static const CommandId kCmdId = command_buffer::kSetAlphaTest;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _func, bool _enable, float _value) {
    SetHeader();
    fixme0 =
        set_alpha_test::Func::MakeValue(_func) |
        set_alpha_test::Enable::MakeValue(_enable ? 1 : 0);
    value = _value;
  }

  static void* Set(void* cmd, uint32 func, bool enable, float value) {
    static_cast<ValueType*>(cmd)->Init(func, enable, value);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 fixme0;
  float value;
};

COMPILE_ASSERT(sizeof(SetAlphaTest) == 12, Sizeof_SetAlphaTest_is_not_12);
COMPILE_ASSERT(offsetof(SetAlphaTest, header) == 0,
               OffsetOf_SetAlphaTest_header_not_0);
COMPILE_ASSERT(offsetof(SetAlphaTest, fixme0) == 4,
               OffsetOf_SetAlphaTest_fixme0_not_4);
COMPILE_ASSERT(offsetof(SetAlphaTest, value) == 8,
               OffsetOf_SetAlphaTest_value_not_8);

struct SetDepthTest {
  typedef SetDepthTest ValueType;
  static const CommandId kCmdId = command_buffer::kSetDepthTest;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _func, bool _write_enable, bool _enable) {
    SetHeader();
    fixme0 =
        set_depth_test::Func::MakeValue(_func) |
        set_depth_test::WriteEnable::MakeValue(_write_enable ? 1 : 0) |
        set_depth_test::Enable::MakeValue(_enable ? 1 : 0);
  }

  static void* Set(void* cmd, uint32 func, bool write_enable, bool enable) {
    static_cast<ValueType*>(cmd)->Init(func, write_enable, enable);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 fixme0;
};

COMPILE_ASSERT(sizeof(SetDepthTest) == 8, Sizeof_SetDepthTest_is_not_8);
COMPILE_ASSERT(offsetof(SetDepthTest, header) == 0,
               OffsetOf_SetDepthTest_header_not_0);
COMPILE_ASSERT(offsetof(SetDepthTest, fixme0) == 4,
               OffsetOf_SetDepthTest_fixme0_not_4);

struct SetStencilTest {
  typedef SetStencilTest ValueType;
  static const CommandId kCmdId = command_buffer::kSetStencilTest;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint8 _write_mask,
            uint8 _compare_mask,
            uint8 _reference_value,
            bool _separate_ccw,
            bool _enable,
            uint8 _cw_func,
            uint8 _cw_pass_op,
            uint8 _cw_fail_op,
            uint8 _cw_z_fail_op,
            uint8 _ccw_func,
            uint8 _ccw_pass_op,
            uint8 _ccw_fail_op,
            uint8 _ccw_z_fail_op) {
    SetHeader();
    fixme0 =
        set_stencil_test::WriteMask::MakeValue(_write_mask) |
        set_stencil_test::CompareMask::MakeValue(_compare_mask) |
        set_stencil_test::ReferenceValue::MakeValue(_reference_value) |
        set_stencil_test::SeparateCCW::MakeValue(_separate_ccw ? 1 : 0) |
        set_stencil_test::Enable::MakeValue(_enable ? 1 : 0);
    fixme1 =
        set_stencil_test::CWFunc::MakeValue(_cw_func) |
        set_stencil_test::CWPassOp::MakeValue(_cw_pass_op) |
        set_stencil_test::CWFailOp::MakeValue(_cw_fail_op) |
        set_stencil_test::CWZFailOp::MakeValue(_cw_z_fail_op) |
        set_stencil_test::CCWFunc::MakeValue(_ccw_func) |
        set_stencil_test::CCWPassOp::MakeValue(_ccw_pass_op) |
        set_stencil_test::CCWFailOp::MakeValue(_ccw_fail_op) |
        set_stencil_test::CCWZFailOp::MakeValue(_ccw_z_fail_op);
  }

  static void* Set(
      void* cmd,
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
    static_cast<ValueType*>(cmd)->Init(
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
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 fixme0;
  uint32 fixme1;
};

COMPILE_ASSERT(sizeof(SetStencilTest) == 12,
               Sizeof_SetStencilTest_is_not_12);
COMPILE_ASSERT(offsetof(SetStencilTest, header) == 0,
               OffsetOf_SetStencilTest_header_not_0);
COMPILE_ASSERT(offsetof(SetStencilTest, fixme0) == 4,
               OffsetOf_SetStencilTest_fixme0_not_4);
COMPILE_ASSERT(offsetof(SetStencilTest, fixme1) == 8,
               OffsetOf_SetStencilTest_fixme1_not_8);

struct SetColorWrite {
  typedef SetColorWrite ValueType;
  static const CommandId kCmdId = command_buffer::kSetColorWrite;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint8 _mask, bool _dither_enable) {
    SetHeader();
    flags =
        set_color_write::RedMask::MakeValue((_mask | 0x01) != 0 ? 1 : 0) |
        set_color_write::GreenMask::MakeValue((_mask | 0x02) != 0 ? 1 : 0) |
        set_color_write::BlueMask::MakeValue((_mask | 0x02) != 0 ? 1 : 0) |
        set_color_write::AlphaMask::MakeValue((_mask | 0x02) != 0 ? 1 : 0) |
        set_color_write::DitherEnable::MakeValue(_dither_enable ? 1 : 0);
  }

  static void* Set(void* cmd, uint8 mask, bool dither_enable) {
    static_cast<ValueType*>(cmd)->Init(mask, dither_enable);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 flags;
};

COMPILE_ASSERT(sizeof(SetColorWrite) == 8, Sizeof_SetColorWrite_is_not_8);
COMPILE_ASSERT(offsetof(SetColorWrite, header) == 0,
               OffsetOf_SetColorWrite_header_not_0);
COMPILE_ASSERT(offsetof(SetColorWrite, flags) == 4,
               OffsetOf_SetColorWrite_flags_not_4);

struct SetBlending {
  typedef SetBlending ValueType;
  static const CommandId kCmdId = command_buffer::kSetBlending;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(
      uint8 _color_src_func,
      uint8 _color_dst_func,
      uint8 _color_eq,
      uint8 _alpha_src_func,
      uint8 _alpha_dst_func,
      uint8 _alpha_eq,
      bool _separate_alpha,
      bool _enable) {
    SetHeader();
    fixme0 =
        set_blending::ColorSrcFunc::MakeValue(_color_src_func) |
        set_blending::ColorDstFunc::MakeValue(_color_dst_func) |
        set_blending::ColorEq::MakeValue(_color_eq) |
        set_blending::AlphaSrcFunc::MakeValue(_alpha_src_func) |
        set_blending::AlphaDstFunc::MakeValue(_alpha_dst_func) |
        set_blending::AlphaEq::MakeValue(_alpha_eq) |
        set_blending::SeparateAlpha::MakeValue(_separate_alpha ? 1 : 0) |
        set_blending::Enable::MakeValue(_enable ? 1 : 0);
  }

  static void* Set(
      void* cmd,
      uint8 color_src_func,
      uint8 color_dst_func,
      uint8 color_eq,
      uint8 alpha_src_func,
      uint8 alpha_dst_func,
      uint8 alpha_eq,
      bool separate_alpha,
      bool enable) {
    static_cast<ValueType*>(cmd)->Init(
        color_src_func,
        color_dst_func,
        color_eq,
        alpha_src_func,
        alpha_dst_func,
        alpha_eq,
        separate_alpha,
        enable);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 fixme0;
};

COMPILE_ASSERT(sizeof(SetBlending) == 8, Sizeof_SetBlending_is_not_8);
COMPILE_ASSERT(offsetof(SetBlending, header) == 0,
               OffsetOf_SetBlending_header_not_0);
COMPILE_ASSERT(offsetof(SetBlending, fixme0) == 4,
               OffsetOf_SetBlending_fixme0_not_4);

struct SetBlendingColor {
  typedef SetBlendingColor ValueType;
  static const CommandId kCmdId = command_buffer::kSetBlendingColor;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(float _red, float _green, float _blue, float _alpha) {
    SetHeader();
    red = _red;
    green = _green;
    blue = _blue;
    alpha = _alpha;
  }

  static void* Set(void* cmd,
                   float red, float green, float blue, float alpha) {
    static_cast<ValueType*>(cmd)->Init(red, green, blue, alpha);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  float red;
  float blue;
  float green;
  float alpha;
};

COMPILE_ASSERT(sizeof(SetBlendingColor) == 20,
               Sizeof_SetBlendingColor_is_not_20);
COMPILE_ASSERT(offsetof(SetBlendingColor, header) == 0,
               OffsetOf_SetBlendingColor_header_not_0);
COMPILE_ASSERT(offsetof(SetBlendingColor, red) == 4,
               OffsetOf_SetBlendingColor_red_not_4);
COMPILE_ASSERT(offsetof(SetBlendingColor, blue) == 8,
               OffsetOf_SetBlendingColor_blue_not_8);
COMPILE_ASSERT(offsetof(SetBlendingColor, green) == 12,
               OffsetOf_SetBlendingColor_green_not_12);
COMPILE_ASSERT(offsetof(SetBlendingColor, alpha) == 16,
               OffsetOf_SetBlendingColor_alpha_not_16);

struct CreateRenderSurface {
  typedef CreateRenderSurface ValueType;
  static const CommandId kCmdId = command_buffer::kCreateRenderSurface;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _texture_id, uint32 _width, uint32 _height,
            uint32 _level, uint32 _side) {
    SetHeader();
    id = _id;
    // TODO(gman): Why does this need a width and height. It's inherited from
    // the texture isn't it?
    fixme1 =
        create_render_surface_cmd::Width::MakeValue(_width) |
        create_render_surface_cmd::Height::MakeValue(_height);
    fixme2 =
        create_render_surface_cmd::Levels::MakeValue(_level) |
        create_render_surface_cmd::Side::MakeValue(_side);
    texture_id = _texture_id;
  }

  static void* Set(void* cmd, uint32 id, uint32 texture_id,
                   uint32 width, uint32 height,
                   uint32 level, uint32 side) {
    static_cast<ValueType*>(cmd)->Init(id, texture_id, width, height,
                                       level, side);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 id;
  uint32 fixme1;
  uint32 fixme2;
  uint32 texture_id;
};

COMPILE_ASSERT(sizeof(CreateRenderSurface) == 20,
               Sizeof_CreateRenderSurface_is_not_20);
COMPILE_ASSERT(offsetof(CreateRenderSurface, header) == 0,
               OffsetOf_CreateRenderSurface_header_not_0);
COMPILE_ASSERT(offsetof(CreateRenderSurface, id) == 4,
               OffsetOf_CreateRenderSurface_id_not_4);
COMPILE_ASSERT(offsetof(CreateRenderSurface, fixme1) == 8,
               OffsetOf_CreateRenderSurface_fixme1_not_8);
COMPILE_ASSERT(offsetof(CreateRenderSurface, fixme2) == 12,
               OffsetOf_CreateRenderSurface_fixme2_not_12);
COMPILE_ASSERT(offsetof(CreateRenderSurface, texture_id) == 16,
               OffsetOf_CreateRenderSurface_texture_id_not_16);

struct DestroyRenderSurface {
  typedef DestroyRenderSurface ValueType;
  static const CommandId kCmdId = command_buffer::kDestroyRenderSurface;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id) {
    SetHeader();
    id = _id;
  }

  static void* Set(void* cmd, uint32 id) {
    static_cast<ValueType*>(cmd)->Init(id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
};

COMPILE_ASSERT(sizeof(DestroyRenderSurface) == 8,
               Sizeof_DestroyRenderSurface_is_not_8);
COMPILE_ASSERT(offsetof(DestroyRenderSurface, header) == 0,
               OffsetOf_DestroyRenderSurface_header_not_0);
COMPILE_ASSERT(offsetof(DestroyRenderSurface, id) == 4,
               OffsetOf_DestroyRenderSurface_id_not_4);

struct CreateDepthSurface {
  typedef CreateDepthSurface ValueType;
  static const CommandId kCmdId = command_buffer::kCreateDepthSurface;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id, uint32 _width, uint32 _height) {
    SetHeader();
    id = _id;
    fixme1 =
        create_depth_surface_cmd::Width::MakeValue(_width) |
        create_depth_surface_cmd::Height::MakeValue(_height);
  }

  static void* Set(void* cmd, uint32 id, uint32 width, uint32 height) {
    static_cast<ValueType*>(cmd)->Init(id, width, height);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 id;
  uint32 fixme1;
};

COMPILE_ASSERT(sizeof(CreateDepthSurface) == 12,
               Sizeof_CreateDepthSurface_is_not_12);
COMPILE_ASSERT(offsetof(CreateDepthSurface, header) == 0,
               OffsetOf_CreateDepthSurface_header_not_0);
COMPILE_ASSERT(offsetof(CreateDepthSurface, id) == 4,
               OffsetOf_CreateDepthSurface_id_not_4);
COMPILE_ASSERT(offsetof(CreateDepthSurface, fixme1) == 8,
               OffsetOf_CreateDepthSurface_fixme1_not_8);

struct DestroyDepthSurface {
  typedef DestroyDepthSurface ValueType;
  static const CommandId kCmdId = command_buffer::kDestroyDepthSurface;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _id) {
    SetHeader();
    id = _id;
  }

  static void* Set(void* cmd, uint32 id) {
    static_cast<ValueType*>(cmd)->Init(id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 id;
};

COMPILE_ASSERT(sizeof(DestroyDepthSurface) == 8,
               Sizeof_DestroyDepthSurface_is_not_8);
COMPILE_ASSERT(offsetof(DestroyDepthSurface, header) == 0,
               OffsetOf_DestroyDepthSurface_header_not_0);
COMPILE_ASSERT(offsetof(DestroyDepthSurface, id) == 4,
               OffsetOf_DestroyDepthSurface_id_not_4);

struct SetRenderSurface {
  typedef SetRenderSurface ValueType;
  static const CommandId kCmdId = command_buffer::kSetRenderSurface;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _render_surface_id, uint32 _depth_surface_id) {
    SetHeader();
    render_surface_id = _render_surface_id;
    depth_surface_id = _depth_surface_id;
  }

  static void* Set(void* cmd,
                   uint32 render_surface_id, uint32 depth_surface_id) {
    static_cast<ValueType*>(cmd)->Init(render_surface_id, depth_surface_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 render_surface_id;
  uint32 depth_surface_id;
};

COMPILE_ASSERT(sizeof(SetRenderSurface) == 12,
               Sizeof_SetRenderSurface_is_not_12);
COMPILE_ASSERT(offsetof(SetRenderSurface, header) == 0,
               OffsetOf_SetRenderSurface_header_not_0);
COMPILE_ASSERT(offsetof(SetRenderSurface, render_surface_id) == 4,
               OffsetOf_SetRenderSurface_render_surface_id_not_4);
COMPILE_ASSERT(offsetof(SetRenderSurface, depth_surface_id) == 8,
               OffsetOf_SetRenderSurface_depth_surface_id_not_8);

struct SetBackSurfaces {
  typedef SetBackSurfaces ValueType;
  static const CommandId kCmdId = command_buffer::kSetBackSurfaces;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init() {
    SetHeader();
  }

  static void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
};

COMPILE_ASSERT(sizeof(SetBackSurfaces) == 4,
               Sizeof_SetBackSurfaces_is_not_4);
COMPILE_ASSERT(offsetof(SetBackSurfaces, header) == 0,
               OffsetOf_SetBackSurfaces_header_not_0);

O3D_POP_STRUCTURE_PACKING;

}  // namespace cmd

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_CMD_BUFFER_FORMAT_H_
