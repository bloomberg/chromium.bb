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

#include "command_buffer/common/cross/cmd_buffer_common.h"
#include "command_buffer/common/cross/resource.h"

namespace o3d {
namespace command_buffer {

// This macro is used to safely and convienently expand the list of commnad
// buffer commands in to various lists and never have them get out of sync. To
// add a new command, add it this list, create the corresponding structure below
// and then add a function in gapi_decoder.cc called Handle_COMMAND_NAME where
// COMMAND_NAME is the name of your command structure.
//
// NOTE: THE ORDER OF THESE MUST NOT CHANGE (their id is derived by order)
#define O3D_COMMAND_BUFFER_CMDS(OP) \
  OP(Noop)                          /* 1024 */ \
  OP(SetToken)                      /* 1025 */ \
  OP(BeginFrame)                    /* 1026 */ \
  OP(EndFrame)                      /* 1027 */ \
  OP(Clear)                         /* 1028 */ \
  OP(CreateVertexBuffer)            /* 1029 */ \
  OP(DestroyVertexBuffer)           /* 1030 */ \
  OP(SetVertexBufferData)           /* 1031 */ \
  OP(SetVertexBufferDataImmediate)  /* 1032 */ \
  OP(GetVertexBufferData)           /* 1033 */ \
  OP(CreateIndexBuffer)             /* 1034 */ \
  OP(DestroyIndexBuffer)            /* 1035 */ \
  OP(SetIndexBufferData)            /* 1036 */ \
  OP(SetIndexBufferDataImmediate)   /* 1037 */ \
  OP(GetIndexBufferData)            /* 1038 */ \
  OP(CreateVertexStruct)            /* 1039 */ \
  OP(DestroyVertexStruct)           /* 1040 */ \
  OP(SetVertexInput)                /* 1041 */ \
  OP(SetVertexStruct)               /* 1042 */ \
  OP(Draw)                          /* 1043 */ \
  OP(DrawIndexed)                   /* 1044 */ \
  OP(CreateEffect)                  /* 1045 */ \
  OP(CreateEffectImmediate)         /* 1046 */ \
  OP(DestroyEffect)                 /* 1047 */ \
  OP(SetEffect)                     /* 1048 */ \
  OP(GetParamCount)                 /* 1049 */ \
  OP(CreateParam)                   /* 1050 */ \
  OP(CreateParamByName)             /* 1051 */ \
  OP(CreateParamByNameImmediate)    /* 1052 */ \
  OP(DestroyParam)                  /* 1053 */ \
  OP(SetParamData)                  /* 1054 */ \
  OP(SetParamDataImmediate)         /* 1055 */ \
  OP(GetParamDesc)                  /* 1056 */ \
  OP(GetStreamCount)                /* 1057 */ \
  OP(GetStreamDesc)                 /* 1058 */ \
  OP(DestroyTexture)                /* 1059 */ \
  OP(CreateTexture2d)               /* 1060 */ \
  OP(CreateTexture3d)               /* 1061 */ \
  OP(CreateTextureCube)             /* 1062 */ \
  OP(SetTextureData)                /* 1063 */ \
  OP(SetTextureDataImmediate)       /* 1064 */ \
  OP(GetTextureData)                /* 1065 */ \
  OP(CreateSampler)                 /* 1066 */ \
  OP(DestroySampler)                /* 1067 */ \
  OP(SetSamplerStates)              /* 1068 */ \
  OP(SetSamplerBorderColor)         /* 1069 */ \
  OP(SetSamplerTexture)             /* 1070 */ \
  OP(SetViewport)                   /* 1071 */ \
  OP(SetScissor)                    /* 1072 */ \
  OP(SetPointLineRaster)            /* 1073 */ \
  OP(SetPolygonRaster)              /* 1074 */ \
  OP(SetPolygonOffset)              /* 1075 */ \
  OP(SetAlphaTest)                  /* 1076 */ \
  OP(SetDepthTest)                  /* 1077 */ \
  OP(SetStencilTest)                /* 1078 */ \
  OP(SetBlending)                   /* 1079 */ \
  OP(SetBlendingColor)              /* 1080 */ \
  OP(SetColorWrite)                 /* 1081 */ \
  OP(CreateRenderSurface)           /* 1082 */ \
  OP(DestroyRenderSurface)          /* 1083 */ \
  OP(CreateDepthSurface)            /* 1084 */ \
  OP(DestroyDepthSurface)           /* 1085 */ \
  OP(SetRenderSurface)              /* 1086 */ \
  OP(SetBackSurfaces)               /* 1087 */ \


// GAPI commands.
enum CommandId {
//  kStartPoint = cmd::kLastCommonId,  // All O3D commands start after this.
  #define O3D_COMMAND_BUFFER_CMD_OP(name) k ## name,

  O3D_COMMAND_BUFFER_CMDS(O3D_COMMAND_BUFFER_CMD_OP)

  #undef O3D_COMMAND_BUFFER_CMD_OP

  kNumCommands,
};

// Bit definitions for buffers to clear.
enum ClearBuffer {
  kColor = 0x1,
  kDepth = 0x2,
  kStencil = 0x4,
  kAllBuffers = kColor | kDepth | kStencil
};

// Polygon mode for SetPolygonRaster
enum PolygonMode {
  kPolygonModePoints,
  kPolygonModeLines,
  kPolygonModeFill,
  kNumPolygonMode
};

// Face culling mode for SetPolygonRaster
enum FaceCullMode {
  kCullNone,
  kCullCW,
  kCullCCW,
  kNumFaceCullMode
};

// Primitive type for Draw and DrawIndexed.
enum PrimitiveType {
  kPoints,
  kLines,
  kLineStrips,
  kTriangles,
  kTriangleStrips,
  kTriangleFans,
  kMaxPrimitiveType
};

// Comparison function for alpha or depth test
enum Comparison {
  kNever,
  kLess,
  kEqual,
  kLEqual,
  kGreater,
  kNotEqual,
  kGEqual,
  kAlways,
  kNumComparison
};

// Stencil operation
enum StencilOp {
  kKeep,
  kZero,
  kReplace,
  kIncNoWrap,
  kDecNoWrap,
  kInvert,
  kIncWrap,
  kDecWrap,
  kNumStencilOp
};

// Blend Equation
enum BlendEq {
  kBlendEqAdd,
  kBlendEqSub,
  kBlendEqRevSub,
  kBlendEqMin,
  kBlendEqMax,
  kNumBlendEq
};

// Blend Funtion
enum BlendFunc {
  kBlendFuncZero,
  kBlendFuncOne,
  kBlendFuncSrcColor,
  kBlendFuncInvSrcColor,
  kBlendFuncSrcAlpha,
  kBlendFuncInvSrcAlpha,
  kBlendFuncDstAlpha,
  kBlendFuncInvDstAlpha,
  kBlendFuncDstColor,
  kBlendFuncInvDstColor,
  kBlendFuncSrcAlphaSaturate,
  kBlendFuncBlendColor,
  kBlendFuncInvBlendColor,
  kNumBlendFunc
};

namespace cmd {

const char* GetCommandName(CommandId id);

// Make sure the compiler does not add extra padding to any of the command
// structures.
O3D_PUSH_STRUCTURE_PACKING_1;

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

  void Init(ResourceId _vertex_buffer_id, uint32 _size,
            vertex_buffer::Flags _flags) {
    SetHeader();
    vertex_buffer_id = _vertex_buffer_id;
    size = _size;
    flags = static_cast<uint32>(_flags);
  }

  static void* Set(void* cmd, ResourceId vertex_buffer_id,
                   uint32 size, vertex_buffer::Flags flags) {
    static_cast<ValueType*>(cmd)->Init(vertex_buffer_id, size, flags);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId vertex_buffer_id;
  uint32 size;
  uint32 flags;
};

COMPILE_ASSERT(sizeof(CreateVertexBuffer) == 16,
               Sizeof_CreateVertexBuffer_is_not_16);
COMPILE_ASSERT(offsetof(CreateVertexBuffer, header) == 0,
               OffsetOf_CreateVertexBuffer_header_not_0);
COMPILE_ASSERT(offsetof(CreateVertexBuffer, vertex_buffer_id) == 4,
               OffsetOf_CreateVertexBuffer_vertex_buffer_id_not_4);
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

  void Init(ResourceId _vertex_buffer_id) {
    SetHeader();
    vertex_buffer_id = _vertex_buffer_id;
  }

  static void* Set(void* cmd, ResourceId vertex_buffer_id) {
    static_cast<ValueType*>(cmd)->Init(vertex_buffer_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId vertex_buffer_id;
};

COMPILE_ASSERT(sizeof(DestroyVertexBuffer) == 8,
               Sizeof_DestroyVertexBuffer_is_not_8);
COMPILE_ASSERT(offsetof(DestroyVertexBuffer, header) == 0,
               OffsetOf_DestroyVertexBuffer_header_not_0);
COMPILE_ASSERT(offsetof(DestroyVertexBuffer, vertex_buffer_id) == 4,
               OffsetOf_DestroyVertexBuffer_vertex_buffer_id_not_4);

struct SetVertexBufferDataImmediate {
  typedef SetVertexBufferDataImmediate ValueType;
  static const CommandId kCmdId = command_buffer::kSetVertexBufferDataImmediate;
  static const ArgFlags kArgFlags = kAtLeastN;

  void SetHeader(uint32 size) {
    header.SetCmdBySize<ValueType>(size);
  }

  void Init(ResourceId _vertex_buffer_id, uint32 _offset,
            const void* data, uint32 size) {
    SetHeader(size);
    vertex_buffer_id = _vertex_buffer_id;
    offset = _offset;
    memcpy(ImmediateDataAddress(this), data, size);
  }

  static void* Set(void* cmd, ResourceId vertex_buffer_id, uint32 offset,
                   const void* data, uint32 size) {
    static_cast<ValueType*>(cmd)->Init(vertex_buffer_id, offset, data, size);
    return NextImmediateCmdAddress<ValueType>(cmd, size);
  }

  CommandHeader header;
  ResourceId vertex_buffer_id;
  uint32 offset;
};

COMPILE_ASSERT(sizeof(SetVertexBufferDataImmediate) == 12,
               Sizeof_SetVertexBufferDataImmediate_is_not_12);
COMPILE_ASSERT(offsetof(SetVertexBufferDataImmediate, header) == 0,
               OffsetOf_SetVertexBufferDataImmediate_header_not_0);
COMPILE_ASSERT(offsetof(SetVertexBufferDataImmediate, vertex_buffer_id) == 4,
               OffsetOf_SetVertexBufferDataImmediate_vertex_buffer_id_not_4);
COMPILE_ASSERT(offsetof(SetVertexBufferDataImmediate, offset) == 8,
               OffsetOf_SetVertexBufferDataImmediate_offset_not_8);

struct SetVertexBufferData {
  typedef SetVertexBufferData ValueType;
  static const CommandId kCmdId = command_buffer::kSetVertexBufferData;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _vertex_buffer_id, uint32 _offset, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    vertex_buffer_id = _vertex_buffer_id;
    offset = _offset;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, ResourceId vertex_buffer_id,
                   uint32 offset, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(vertex_buffer_id, offset, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId vertex_buffer_id;
  uint32 offset;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(SetVertexBufferData) == 24,
               Sizeof_SetVertexBufferData_is_not_24);
COMPILE_ASSERT(offsetof(SetVertexBufferData, header) == 0,
               OffsetOf_SetVertexBufferData_header_not_0);
COMPILE_ASSERT(offsetof(SetVertexBufferData, vertex_buffer_id) == 4,
               OffsetOf_SetVertexBufferData_vertex_buffer_id_not_4);
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

  void Init(ResourceId _vertex_buffer_id, uint32 _offset, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    vertex_buffer_id = _vertex_buffer_id;
    offset = _offset;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, ResourceId vertex_buffer_id,
                   uint32 offset, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(vertex_buffer_id, offset, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId vertex_buffer_id;
  uint32 offset;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(GetVertexBufferData) == 24,
               Sizeof_GetVertexBufferData_is_not_24);
COMPILE_ASSERT(offsetof(GetVertexBufferData, header) == 0,
               OffsetOf_GetVertexBufferData_header_not_0);
COMPILE_ASSERT(offsetof(GetVertexBufferData, vertex_buffer_id) == 4,
               OffsetOf_GetVertexBufferData_vertex_buffer_id_not_4);
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

  void Init(ResourceId _index_buffer_id, uint32 _size,
            index_buffer::Flags _flags) {
    SetHeader();
    index_buffer_id = _index_buffer_id;
    size = _size;
    flags = static_cast<uint32>(_flags);
  }

  static void* Set(void* cmd, ResourceId index_buffer_id,
                   uint32 size, index_buffer::Flags flags) {
    static_cast<ValueType*>(cmd)->Init(index_buffer_id, size, flags);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId index_buffer_id;
  uint32 size;
  uint32 flags;
};

COMPILE_ASSERT(sizeof(CreateIndexBuffer) == 16,
               Sizeof_CreateIndexBuffer_is_not_16);
COMPILE_ASSERT(offsetof(CreateIndexBuffer, header) == 0,
               OffsetOf_CreateIndexBuffer_header_not_0);
COMPILE_ASSERT(offsetof(CreateIndexBuffer, index_buffer_id) == 4,
               OffsetOf_CreateIndexBuffer_index_buffer_id_not_4);
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

  void Init(ResourceId _index_buffer_id) {
    SetHeader();
    index_buffer_id = _index_buffer_id;
  }

  static void* Set(void* cmd, ResourceId index_buffer_id) {
    static_cast<ValueType*>(cmd)->Init(index_buffer_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId index_buffer_id;
};

COMPILE_ASSERT(sizeof(DestroyIndexBuffer) == 8,
               Sizeof_DestroyIndexBuffer_is_not_8);
COMPILE_ASSERT(offsetof(DestroyIndexBuffer, header) == 0,
               OffsetOf_DestroyIndexBuffer_header_not_0);
COMPILE_ASSERT(offsetof(DestroyIndexBuffer, index_buffer_id) == 4,
               OffsetOf_DestroyIndexBuffer_index_buffer_id_not_4);

struct SetIndexBufferDataImmediate {
  typedef SetIndexBufferDataImmediate ValueType;
  static const CommandId kCmdId = command_buffer::kSetIndexBufferDataImmediate;
  static const ArgFlags kArgFlags = kAtLeastN;

  void SetHeader(uint32 size) {
    header.SetCmdBySize<ValueType>(size);
  }

  void Init(ResourceId _index_buffer_id, uint32 _offset,
            const void* data, uint32 size) {
    SetHeader(size);
    index_buffer_id = _index_buffer_id;
    offset = _offset;
    memcpy(ImmediateDataAddress(this), data, size);
  }

  static void* Set(void* cmd, ResourceId index_buffer_id, uint32 offset,
                   const void* data, uint32 size) {
    static_cast<ValueType*>(cmd)->Init(index_buffer_id, offset, data, size);
    return NextImmediateCmdAddress<ValueType>(cmd, size);
  }

  CommandHeader header;
  ResourceId index_buffer_id;
  uint32 offset;
};

COMPILE_ASSERT(sizeof(SetIndexBufferDataImmediate) == 12,
               Sizeof_SetIndexBufferDataImmediate_is_not_12);
COMPILE_ASSERT(offsetof(SetIndexBufferDataImmediate, header) == 0,
               OffsetOf_SetIndexBufferDataImmediate_header_not_0);
COMPILE_ASSERT(offsetof(SetIndexBufferDataImmediate, index_buffer_id) == 4,
               OffsetOf_SetIndexBufferDataImmediate_index_buffer_id_not_4);
COMPILE_ASSERT(offsetof(SetIndexBufferDataImmediate, offset) == 8,
               OffsetOf_SetIndexBufferDataImmediate_offset_not_8);

struct SetIndexBufferData {
  typedef SetIndexBufferData ValueType;
  static const CommandId kCmdId = command_buffer::kSetIndexBufferData;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _index_buffer_id, uint32 _offset, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    index_buffer_id = _index_buffer_id;
    offset = _offset;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd,
                   ResourceId index_buffer_id, uint32 offset, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(index_buffer_id, offset, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId index_buffer_id;
  uint32 offset;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(SetIndexBufferData) == 24,
               Sizeof_SetIndexBufferData_is_not_24);
COMPILE_ASSERT(offsetof(SetIndexBufferData, header) == 0,
               OffsetOf_SetIndexBufferData_header_not_0);
COMPILE_ASSERT(offsetof(SetIndexBufferData, index_buffer_id) == 4,
               OffsetOf_SetIndexBufferData_index_buffer_id_not_4);
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

  void Init(ResourceId _index_buffer_id, uint32 _offset, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    index_buffer_id = _index_buffer_id;
    offset = _offset;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, ResourceId index_buffer_id,
                   uint32 offset, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(index_buffer_id, offset, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId index_buffer_id;
  uint32 offset;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(GetIndexBufferData) == 24,
               Sizeof_GetIndexBufferData_is_not_24);
COMPILE_ASSERT(offsetof(GetIndexBufferData, header) == 0,
               OffsetOf_GetIndexBufferData_header_not_0);
COMPILE_ASSERT(offsetof(GetIndexBufferData, index_buffer_id) == 4,
               OffsetOf_GetIndexBufferData_index_buffer_id_not_4);
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

  void Init(ResourceId _vertex_struct_id, uint32 _input_count) {
    SetHeader();
    vertex_struct_id = _vertex_struct_id;
    input_count = _input_count;
  }

  static void* Set(void* cmd, ResourceId vertex_struct_id, uint32 input_count) {
    static_cast<ValueType*>(cmd)->Init(vertex_struct_id, input_count);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId vertex_struct_id;
  uint32 input_count;
};

COMPILE_ASSERT(sizeof(CreateVertexStruct) == 12,
               Sizeof_CreateVertexStruct_is_not_12);
COMPILE_ASSERT(offsetof(CreateVertexStruct, header) == 0,
               OffsetOf_CreateVertexStruct_header_not_0);
COMPILE_ASSERT(offsetof(CreateVertexStruct, vertex_struct_id) == 4,
               OffsetOf_CreateVertexStruct_vertex_struct_id_not_4);
COMPILE_ASSERT(offsetof(CreateVertexStruct, input_count) == 8,
               OffsetOf_CreateVertexStruct_input_count_not_8);

struct DestroyVertexStruct {
  typedef DestroyVertexStruct ValueType;
  static const CommandId kCmdId = command_buffer::kDestroyVertexStruct;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _vertex_struct_id) {
    SetHeader();
    vertex_struct_id = _vertex_struct_id;
  }

  static void* Set(void* cmd, ResourceId vertex_struct_id) {
    static_cast<ValueType*>(cmd)->Init(vertex_struct_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId vertex_struct_id;
};

COMPILE_ASSERT(sizeof(DestroyVertexStruct) == 8,
               Sizeof_DestroyVertexStruct_is_not_8);
COMPILE_ASSERT(offsetof(DestroyVertexStruct, header) == 0,
               OffsetOf_DestroyVertexStruct_header_not_0);
COMPILE_ASSERT(offsetof(DestroyVertexStruct, vertex_struct_id) == 4,
               OffsetOf_DestroyVertexStruct_vertex_struct_id_not_4);

struct SetVertexInput {
  typedef SetVertexInput ValueType;
  static const CommandId kCmdId = command_buffer::kSetVertexInput;
  static const ArgFlags kArgFlags = kFixed;

  // type_stride_semantic field.
  typedef BitField<0, 4> SemanticIndex;
  typedef BitField<4, 4> Semantic;
  typedef BitField<8, 8> Type;
  typedef BitField<16, 16> Stride;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _vertex_struct_id,
            uint32 _input_index,
            ResourceId _vertex_buffer_id,
            uint32 _offset,
            vertex_struct::Semantic _semantic,
            uint32 _semantic_index,
            vertex_struct::Type _type,
            uint32 _stride) {
    SetHeader();
    vertex_struct_id = _vertex_struct_id;
    input_index = _input_index;
    vertex_buffer_id = _vertex_buffer_id;
    offset = _offset;
    type_stride_semantic =
        Semantic::MakeValue(_semantic) |
        SemanticIndex::MakeValue(_semantic_index) |
        Type::MakeValue(_type) |
        Stride::MakeValue(_stride);
  }

  static void* Set(
      void* cmd,
      ResourceId vertex_struct_id,
      uint32 input_index,
      ResourceId vertex_buffer_id,
      uint32 offset,
      vertex_struct::Semantic semantic,
      uint32 semantic_index,
      vertex_struct::Type type,
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
  ResourceId vertex_struct_id;
  uint32 input_index;
  ResourceId vertex_buffer_id;
  uint32 offset;
  uint32 type_stride_semantic;
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
COMPILE_ASSERT(offsetof(SetVertexInput, type_stride_semantic) == 20,
               OffsetOf_SetVertexInput_type_stride_semantic_not_20);

struct SetVertexStruct {
  typedef SetVertexStruct ValueType;
  static const CommandId kCmdId = command_buffer::kSetVertexStruct;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _vertex_struct_id) {
    SetHeader();
    vertex_struct_id = _vertex_struct_id;
  }

  static void* Set(void* cmd, ResourceId vertex_struct_id) {
    static_cast<ValueType*>(cmd)->Init(vertex_struct_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId vertex_struct_id;
};

COMPILE_ASSERT(sizeof(SetVertexStruct) == 8,
               Sizeof_SetVertexStruct_is_not_8);
COMPILE_ASSERT(offsetof(SetVertexStruct, header) == 0,
               OffsetOf_SetVertexStruct_header_not_0);
COMPILE_ASSERT(offsetof(SetVertexStruct, vertex_struct_id) == 4,
               OffsetOf_SetVertexStruct_vertex_struct_id_not_4);

struct Draw {
  typedef Draw ValueType;
  static const CommandId kCmdId = command_buffer::kDraw;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(PrimitiveType _primitive_type, uint32 _first, uint32 _count) {
    SetHeader();
    primitive_type = _primitive_type;
    first = _first;
    count = _count;
  }

  static void* Set(void* cmd, PrimitiveType primitive_type, uint32 first,
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
      PrimitiveType _primitive_type,
      ResourceId _index_buffer_id,
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
      PrimitiveType primitive_type,
      ResourceId index_buffer_id,
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
  ResourceId index_buffer_id;
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

  void Init(ResourceId _effect_id, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    effect_id = _effect_id;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, ResourceId effect_id, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(effect_id, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId effect_id;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(CreateEffect) == 20, Sizeof_CreateEffect_is_not_20);
COMPILE_ASSERT(offsetof(CreateEffect, header) == 0,
               OffsetOf_CreateEffect_header_not_0);
COMPILE_ASSERT(offsetof(CreateEffect, effect_id) == 4,
               OffsetOf_CreateEffect_effect_id_not_4);
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

  void Init(ResourceId _effect_id, uint32 _size, const void* data) {
    SetHeader(_size);
    effect_id = _effect_id;
    size = _size;
  }

  static void* Set(void* cmd,
                   ResourceId effect_id, uint32 size, const void* data) {
    static_cast<ValueType*>(cmd)->Init(effect_id, size, data);
    return NextImmediateCmdAddress<ValueType>(cmd, size);
  }

  CommandHeader header;
  ResourceId effect_id;
  uint32 size;
};

COMPILE_ASSERT(sizeof(CreateEffectImmediate) == 12,
               Sizeof_CreateEffectImmediate_is_not_12);
COMPILE_ASSERT(offsetof(CreateEffectImmediate, header) == 0,
               OffsetOf_CreateEffectImmediate_header_not_0);
COMPILE_ASSERT(offsetof(CreateEffectImmediate, effect_id) == 4,
               OffsetOf_CreateEffectImmediate_effect_id_not_4);
COMPILE_ASSERT(offsetof(CreateEffectImmediate, size) == 8,
               OffsetOf_CreateEffectImmediate_size_not_8);

struct DestroyEffect {
  typedef DestroyEffect ValueType;
  static const CommandId kCmdId = command_buffer::kDestroyEffect;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _effect_id) {
    SetHeader();
    effect_id = _effect_id;
  }

  static void* Set(void* cmd, ResourceId effect_id) {
    static_cast<ValueType*>(cmd)->Init(effect_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId effect_id;
};

COMPILE_ASSERT(sizeof(DestroyEffect) == 8, Sizeof_DestroyEffect_is_not_8);
COMPILE_ASSERT(offsetof(DestroyEffect, header) == 0,
               OffsetOf_DestroyEffect_header_not_0);
COMPILE_ASSERT(offsetof(DestroyEffect, effect_id) == 4,
               OffsetOf_DestroyEffect_effect_id_not_4);

struct SetEffect {
  typedef SetEffect ValueType;
  static const CommandId kCmdId = command_buffer::kSetEffect;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _effect_id) {
    SetHeader();
    effect_id = _effect_id;
  }

  static void* Set(void* cmd, ResourceId effect_id) {
    static_cast<ValueType*>(cmd)->Init(effect_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId effect_id;
};

COMPILE_ASSERT(sizeof(SetEffect) == 8, Sizeof_SetEffect_is_not_8);
COMPILE_ASSERT(offsetof(SetEffect, header) == 0,
               OffsetOf_SetEffect_header_not_0);
COMPILE_ASSERT(offsetof(SetEffect, effect_id) == 4,
               OffsetOf_SetEffect_effect_id_not_4);

struct GetParamCount {
  typedef GetParamCount ValueType;
  static const CommandId kCmdId = command_buffer::kGetParamCount;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _effect_id, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    effect_id = _effect_id;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, ResourceId effect_id, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(effect_id, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId effect_id;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(GetParamCount) == 20, Sizeof_GetParamCount_is_not_20);
COMPILE_ASSERT(offsetof(GetParamCount, header) == 0,
               OffsetOf_GetParamCount_header_not_0);
COMPILE_ASSERT(offsetof(GetParamCount, effect_id) == 4,
               OffsetOf_GetParamCount_effect_id_not_4);
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

  void Init(ResourceId _param_id, ResourceId _effect_id, uint32 _index) {
    SetHeader();
    param_id = _param_id;
    effect_id = _effect_id;
    index = _index;
  }

  static void* Set(void* cmd,
                   ResourceId param_id, ResourceId effect_id, uint32 index) {
    static_cast<ValueType*>(cmd)->Init(param_id, effect_id, index);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId param_id;
  ResourceId effect_id;
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

  void Init(ResourceId _param_id, ResourceId _effect_id, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    param_id = _param_id;
    effect_id = _effect_id;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, ResourceId param_id, ResourceId effect_id,
                   uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(param_id, effect_id, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId param_id;
  ResourceId effect_id;
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

  void Init(ResourceId _param_id, ResourceId _effect_id, uint32 _size,
            const void* data) {
    SetHeader(_size);
    param_id = _param_id;
    effect_id = _effect_id;
    size = _size;
    memcpy(ImmediateDataAddress(this), data, _size);
  }

  static void* Set(void* cmd, ResourceId param_id, ResourceId effect_id,
                   uint32 size, const void* data) {
    static_cast<ValueType*>(cmd)->Init(param_id, effect_id, size, data);
    return NextImmediateCmdAddress<ValueType>(cmd, size);
  }

  CommandHeader header;
  ResourceId param_id;
  ResourceId effect_id;
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

  void Init(ResourceId _param_id) {
    SetHeader();
    param_id = _param_id;
  }

  static void* Set(void* cmd, ResourceId param_id) {
    static_cast<ValueType*>(cmd)->Init(param_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId param_id;
};

COMPILE_ASSERT(sizeof(DestroyParam) == 8, Sizeof_DestroyParam_is_not_8);
COMPILE_ASSERT(offsetof(DestroyParam, header) == 0,
               OffsetOf_DestroyParam_header_not_0);
COMPILE_ASSERT(offsetof(DestroyParam, param_id) == 4,
               OffsetOf_DestroyParam_param_id_not_4);

struct SetParamData {
  typedef SetParamData ValueType;
  static const CommandId kCmdId = command_buffer::kSetParamData;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _param_id, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    param_id = _param_id;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, ResourceId param_id, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(param_id, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId param_id;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(SetParamData) == 20, Sizeof_SetParamData_is_not_20);
COMPILE_ASSERT(offsetof(SetParamData, header) == 0,
               OffsetOf_SetParamData_header_not_0);
COMPILE_ASSERT(offsetof(SetParamData, param_id) == 4,
               OffsetOf_SetParamData_param_id_not_4);
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

  void Init(ResourceId _param_id, uint32 _size, const void* data) {
    SetHeader(_size);
    param_id = _param_id;
    size = _size;
    memcpy(ImmediateDataAddress(this), data, _size);
  }

  static void* Set(void* cmd, ResourceId param_id,
                   uint32 size, const void* data) {
    static_cast<ValueType*>(cmd)->Init(param_id, size, data);
    return NextImmediateCmdAddress<ValueType>(cmd, size);
  }

  CommandHeader header;
  ResourceId param_id;
  uint32 size;
};

COMPILE_ASSERT(sizeof(SetParamDataImmediate) == 12,
               Sizeof_SetParamDataImmediate_is_not_12);
COMPILE_ASSERT(offsetof(SetParamDataImmediate, header) == 0,
               OffsetOf_SetParamDataImmediate_header_not_0);
COMPILE_ASSERT(offsetof(SetParamDataImmediate, param_id) == 4,
               OffsetOf_SetParamDataImmediate_param_id_not_4);
COMPILE_ASSERT(offsetof(SetParamDataImmediate, size) == 8,
               OffsetOf_SetParamDataImmediate_size_not_8);

struct GetParamDesc {
  typedef GetParamDesc ValueType;
  static const CommandId kCmdId = command_buffer::kGetParamDesc;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _param_id, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    param_id = _param_id;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, ResourceId param_id, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(param_id, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId param_id;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(GetParamDesc) == 20, Sizeof_GetParamDesc_is_not_20);
COMPILE_ASSERT(offsetof(GetParamDesc, header) == 0,
               OffsetOf_GetParamDesc_header_not_0);
COMPILE_ASSERT(offsetof(GetParamDesc, param_id) == 4,
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

  void Init(ResourceId _effect_id, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    effect_id = _effect_id;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, ResourceId effect_id, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(effect_id, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId effect_id;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(GetStreamCount) == 20,
               Sizeof_GetStreamCount_is_not_20);
COMPILE_ASSERT(offsetof(GetStreamCount, header) == 0,
               OffsetOf_GetStreamCount_header_not_0);
COMPILE_ASSERT(offsetof(GetStreamCount, effect_id) == 4,
               OffsetOf_GetStreamCount_effect_id_not_4);
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

  void Init(ResourceId _effect_id, uint32 _index, uint32 _size,
            uint32 shared_memory_id, uint32 shared_memory_offset) {
    SetHeader();
    effect_id = _effect_id;
    index = _index;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(void* cmd, ResourceId effect_id, uint32 index, uint32 size,
                   uint32 shared_memory_id, uint32 shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(effect_id, index, size,
                                       shared_memory_id, shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId effect_id;
  uint32 index;
  uint32 size;
  SharedMemory shared_memory;
};

COMPILE_ASSERT(sizeof(GetStreamDesc) == 24, Sizeof_GetStreamDesc_is_not_24);
COMPILE_ASSERT(offsetof(GetStreamDesc, header) == 0,
               OffsetOf_GetStreamDesc_header_not_0);
COMPILE_ASSERT(offsetof(GetStreamDesc, effect_id) ==4 ,
               OffsetOf_GetStreamDesc_effect_id_not_4);
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

  void Init(ResourceId _texture_id) {
    SetHeader();
    texture_id = _texture_id;
  }

  static void* Set(void* cmd, ResourceId texture_id) {
    static_cast<ValueType*>(cmd)->Init(texture_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId texture_id;
};

COMPILE_ASSERT(sizeof(DestroyTexture) == 8, Sizeof_DestroyTexture_is_not_8);
COMPILE_ASSERT(offsetof(DestroyTexture, header) == 0,
               OffsetOf_DestroyTexture_header_not_0);
COMPILE_ASSERT(offsetof(DestroyTexture, texture_id) == 4,
               OffsetOf_DestroyTexture_texture_id_not_4);

struct CreateTexture2d {
  typedef CreateTexture2d ValueType;
  static const CommandId kCmdId = command_buffer::kCreateTexture2d;
  static const ArgFlags kArgFlags = kFixed;

  // argument 1
  typedef BitField<0, 16> Width;
  typedef BitField<16, 16> Height;
  // argument 2
  typedef BitField<0, 4> Levels;
  typedef BitField<4, 4> Unused;
  typedef BitField<8, 8> Format;
  typedef BitField<16, 16> Flags;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _texture_id,
            uint32 _width, uint32 _height, uint32 _levels,
            texture::Format _format,
            bool _enable_render_surfaces) {
    SetHeader();
    texture_id = _texture_id;
    width_height = Width::MakeValue(_width) | Height::MakeValue(_height);
    levels_format_flags =
        Levels::MakeValue(_levels) |
        Format::MakeValue(_format) |
        Flags::MakeValue(_enable_render_surfaces ? 1 : 0);
  }

  static void* Set(void* cmd, ResourceId texture_id,
                   uint32 width, uint32 height, uint32 levels,
                   texture::Format format,
                   bool enable_render_surfaces) {
    static_cast<ValueType*>(cmd)->Init(texture_id,
                                       width, height, levels, format,
                                       enable_render_surfaces);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  ResourceId texture_id;
  uint32 width_height;
  uint32 levels_format_flags;
};

COMPILE_ASSERT(sizeof(CreateTexture2d) == 16,
               Sizeof_CreateTexture2d_is_not_16);
COMPILE_ASSERT(offsetof(CreateTexture2d, header) == 0,
               OffsetOf_CreateTexture2d_header_not_0);
COMPILE_ASSERT(offsetof(CreateTexture2d, texture_id) == 4,
               OffsetOf_CreateTexture2d_texture_id_not_4);
COMPILE_ASSERT(offsetof(CreateTexture2d, width_height) == 8,
               OffsetOf_CreateTexture2d_width_height_not_8);
COMPILE_ASSERT(offsetof(CreateTexture2d, levels_format_flags) == 12,
               OffsetOf_CreateTexture2d_levels_format_flags_not_12);

struct CreateTexture3d {
  typedef CreateTexture3d ValueType;
  static const CommandId kCmdId = command_buffer::kCreateTexture3d;
  static const ArgFlags kArgFlags = kFixed;

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

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _texture_id,
            uint32 _width, uint32 _height, uint32 _depth,
            uint32 _levels, texture::Format _format,
            bool _enable_render_surfaces) {
    SetHeader();
    texture_id = _texture_id;
    width_height = Width::MakeValue(_width) | Height::MakeValue(_height);
    depth_unused = Depth::MakeValue(_depth);
    levels_format_flags =
        Levels::MakeValue(_levels) |
        Format::MakeValue(_format) |
        Flags::MakeValue(_enable_render_surfaces ? 1 : 0);
  }

  static void* Set(void* cmd, ResourceId texture_id,
                   uint32 width, uint32 height, uint32 depth,
                   uint32 levels, texture::Format format,
                   bool enable_render_surfaces) {
    static_cast<ValueType*>(cmd)->Init(texture_id,
                                       width, height, depth,
                                       levels, format,
                                       enable_render_surfaces);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  ResourceId texture_id;
  uint32 width_height;
  uint32 depth_unused;
  uint32 levels_format_flags;
};

COMPILE_ASSERT(sizeof(CreateTexture3d) == 20,
               Sizeof_CreateTexture3d_is_not_20);
COMPILE_ASSERT(offsetof(CreateTexture3d, header) == 0,
               OffsetOf_CreateTexture3d_header_not_0);
COMPILE_ASSERT(offsetof(CreateTexture3d, texture_id) == 4,
               OffsetOf_CreateTexture3d_texture_id_not_4);
COMPILE_ASSERT(offsetof(CreateTexture3d, width_height) == 8,
               OffsetOf_CreateTexture3d_width_height_not_8);
COMPILE_ASSERT(offsetof(CreateTexture3d, depth_unused) == 12,
               OffsetOf_CreateTexture3d_depth_unused_not_12);
COMPILE_ASSERT(offsetof(CreateTexture3d, levels_format_flags) == 16,
               OffsetOf_CreateTexture3d_levels_format_flags_not_16);

struct CreateTextureCube {
  typedef CreateTextureCube ValueType;
  static const CommandId kCmdId = command_buffer::kCreateTextureCube;
  static const ArgFlags kArgFlags = kFixed;

  // argument 1
  typedef BitField<0, 16> Side;
  typedef BitField<16, 16> Unused1;
  // argument 2
  typedef BitField<0, 4> Levels;
  typedef BitField<4, 4> Unused2;
  typedef BitField<8, 8> Format;
  typedef BitField<16, 16> Flags;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _texture_id,
            uint32 _edge_length, uint32 _levels, texture::Format _format,
            bool _enable_render_surfaces) {
    SetHeader();
    texture_id = _texture_id;
    edge_length = _edge_length;
    levels_format_flags =
        Levels::MakeValue(_levels) |
        Format::MakeValue(_format) |
        Flags::MakeValue(_enable_render_surfaces ? 1 : 0);
  }

  static void* Set(void* cmd, ResourceId texture_id,
                   uint32 edge_length, uint32 levels, texture::Format format,
                   bool enable_render_surfaces) {
    static_cast<ValueType*>(cmd)->Init(texture_id,
                                       edge_length, levels, format,
                                       enable_render_surfaces);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  ResourceId texture_id;
  uint32 edge_length;
  uint32 levels_format_flags;
};

COMPILE_ASSERT(sizeof(CreateTextureCube) == 16,
               Sizeof_CreateTextureCube_is_not_16);
COMPILE_ASSERT(offsetof(CreateTextureCube, header) == 0,
               OffsetOf_CreateTextureCube_header_not_0);
COMPILE_ASSERT(offsetof(CreateTextureCube, texture_id) == 4,
               OffsetOf_CreateTextureCube_texture_id_not_4);
COMPILE_ASSERT(offsetof(CreateTextureCube, edge_length) == 8,
               OffsetOf_CreateTextureCube_edge_length_not_8);
COMPILE_ASSERT(offsetof(CreateTextureCube, levels_format_flags) == 12,
               OffsetOf_CreateTextureCube_levels_format_flags_not_12);

struct SetTextureData {
  typedef SetTextureData ValueType;
  static const CommandId kCmdId = command_buffer::kSetTextureData;
  static const ArgFlags kArgFlags = kFixed;

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

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(
      ResourceId _texture_id,
      uint32 _x,
      uint32 _y,
      uint32 _z,
      uint32 _width,
      uint32 _height,
      uint32 _depth,
      uint32 _level,
      texture::Face _face,
      uint32 _row_pitch,
      uint32 _slice_pitch,
      uint32 _size,
      uint32 shared_memory_id,
      uint32 shared_memory_offset) {
    SetHeader();
    texture_id = _texture_id;
    x_y = X::MakeValue(_x) | Y::MakeValue(_y);
    width_height = Width::MakeValue(_width) | Height::MakeValue(_height);
    z_depth = Z::MakeValue(_z) | Depth::MakeValue(_depth);
    level_face = Level::MakeValue(_level) | Face::MakeValue(_face);
    row_pitch = _row_pitch;
    slice_pitch = _slice_pitch;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(
      void* cmd,
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
  ResourceId texture_id;
  uint32 x_y;
  uint32 width_height;
  uint32 z_depth;
  uint32 level_face;
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
COMPILE_ASSERT(offsetof(SetTextureData, x_y) == 8,
               OffsetOf_SetTextureData_x_y_not_8);
COMPILE_ASSERT(offsetof(SetTextureData, width_height) == 12,
               OffsetOf_SetTextureData_width_height_not_12);
COMPILE_ASSERT(offsetof(SetTextureData, z_depth) == 16,
               OffsetOf_SetTextureData_z_depth_not_16);
COMPILE_ASSERT(offsetof(SetTextureData, level_face) == 20,
               OffsetOf_SetTextureData_level_face_not_20);
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

  void SetHeader(uint32 size) {
    header.SetCmdBySize<ValueType>(size);
  }

  void Init(
      ResourceId _texture_id,
      uint32 _x,
      uint32 _y,
      uint32 _z,
      uint32 _width,
      uint32 _height,
      uint32 _depth,
      uint32 _level,
      texture::Face _face,
      uint32 _row_pitch,
      uint32 _slice_pitch,
      uint32 _size,
      const void* data) {
    SetHeader(_size);
    texture_id = _texture_id;
    x_y = X::MakeValue(_x) | Y::MakeValue(_y);
    width_height = Width::MakeValue(_width) | Height::MakeValue(_height);
    z_depth = Z::MakeValue(_z) | Depth::MakeValue(_depth);
    level_face = Level::MakeValue(_level) | Face::MakeValue(_face);
    row_pitch = _row_pitch;
    slice_pitch = _slice_pitch;
    size = _size;
    memcpy(ImmediateDataAddress(this), data, _size);
  }

  static void* Set(
      void* cmd,
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
  ResourceId texture_id;
  uint32 x_y;
  uint32 width_height;
  uint32 z_depth;
  uint32 level_face;
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
COMPILE_ASSERT(offsetof(SetTextureDataImmediate, x_y) == 8,
               OffsetOf_SetTextureDataImmediate_x_y_not_8);
COMPILE_ASSERT(offsetof(SetTextureDataImmediate, width_height) == 12,
               OffsetOf_SetTextureDataImmediate_width_height_not_12);
COMPILE_ASSERT(offsetof(SetTextureDataImmediate, z_depth) == 16,
               OffsetOf_SetTextureDataImmediate_z_depth_not_16);
COMPILE_ASSERT(offsetof(SetTextureDataImmediate, level_face) == 20,
               OffsetOf_SetTextureDataImmediate_level_face_not_20);
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

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(
      ResourceId _texture_id,
      uint32 _x,
      uint32 _y,
      uint32 _z,
      uint32 _width,
      uint32 _height,
      uint32 _depth,
      uint32 _level,
      texture::Face _face,
      uint32 _row_pitch,
      uint32 _slice_pitch,
      uint32 _size,
      uint32 shared_memory_id,
      uint32 shared_memory_offset) {
    SetHeader();
    texture_id = _texture_id;
    x_y = X::MakeValue(_x) | Y::MakeValue(_y);
    width_height = Width::MakeValue(_width) | Height::MakeValue(_height);
    z_depth = Z::MakeValue(_z) | Depth::MakeValue(_depth);
    level_face = Level::MakeValue(_level) | Face::MakeValue(_face);
    row_pitch = _row_pitch;
    slice_pitch = _slice_pitch;
    size = _size;
    shared_memory.Init(shared_memory_id, shared_memory_offset);
  }

  static void* Set(
      void* cmd,
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
  ResourceId texture_id;
  uint32 x_y;
  uint32 width_height;
  uint32 z_depth;
  uint32 level_face;
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
COMPILE_ASSERT(offsetof(GetTextureData, x_y) == 8,
               OffsetOf_GetTextureData_x_y_not_8);
COMPILE_ASSERT(offsetof(GetTextureData, width_height) == 12,
               OffsetOf_GetTextureData_width_height_not_12);
COMPILE_ASSERT(offsetof(GetTextureData, z_depth) == 16,
               OffsetOf_GetTextureData_z_depth_not_16);
COMPILE_ASSERT(offsetof(GetTextureData, level_face) == 20,
               OffsetOf_GetTextureData_level_face_not_20);
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

  void Init(ResourceId _sampler_id) {
    SetHeader();
    sampler_id = _sampler_id;
  }

  static void* Set(void* cmd, ResourceId sampler_id) {
    static_cast<ValueType*>(cmd)->Init(sampler_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId sampler_id;
};

COMPILE_ASSERT(sizeof(CreateSampler) == 8, Sizeof_CreateSampler_is_not_8);
COMPILE_ASSERT(offsetof(CreateSampler, header) == 0,
               OffsetOf_CreateSampler_header_not_0);
COMPILE_ASSERT(offsetof(CreateSampler, sampler_id) == 4,
               OffsetOf_CreateSampler_sampler_id_not_4);

struct DestroySampler {
  typedef DestroySampler ValueType;
  static const CommandId kCmdId = command_buffer::kDestroySampler;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _sampler_id) {
    SetHeader();
    sampler_id = _sampler_id;
  }

  static void* Set(void* cmd, ResourceId sampler_id) {
    static_cast<ValueType*>(cmd)->Init(sampler_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId sampler_id;
};

COMPILE_ASSERT(sizeof(DestroySampler) == 8, Sizeof_DestroySampler_is_not_8);
COMPILE_ASSERT(offsetof(DestroySampler, header) == 0,
               OffsetOf_DestroySampler_header_not_0);
COMPILE_ASSERT(offsetof(DestroySampler, sampler_id) == 4,
               OffsetOf_DestroySampler_sampler_id_not_4);

struct SetSamplerStates {
  typedef SetSamplerStates ValueType;
  static const CommandId kCmdId = command_buffer::kSetSamplerStates;
  static const ArgFlags kArgFlags = kFixed;

  // argument 2
  typedef BitField<0, 3> AddressingU;
  typedef BitField<3, 3> AddressingV;
  typedef BitField<6, 3> AddressingW;
  typedef BitField<9, 3> MagFilter;
  typedef BitField<12, 3> MinFilter;
  typedef BitField<15, 3> MipFilter;
  typedef BitField<18, 6> Unused;
  typedef BitField<24, 8> MaxAnisotropy;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(
      ResourceId _sampler_id,
      sampler::AddressingMode _address_u_value,
      sampler::AddressingMode _address_v_value,
      sampler::AddressingMode _address_w_value,
      sampler::FilteringMode _mag_filter_value,
      sampler::FilteringMode _min_filter_value,
      sampler::FilteringMode _mip_filter_value,
      uint8 _max_anisotropy) {
    SetHeader();
    sampler_id = _sampler_id;
    sampler_states =
        AddressingU::MakeValue(_address_u_value) |
        AddressingV::MakeValue(_address_v_value) |
        AddressingW::MakeValue(_address_w_value) |
        MagFilter::MakeValue(_mag_filter_value) |
        MinFilter::MakeValue(_min_filter_value) |
        MipFilter::MakeValue(_mip_filter_value) |
        MaxAnisotropy::MakeValue(_max_anisotropy);
  }

  static void* Set(void* cmd,
      ResourceId sampler_id,
      sampler::AddressingMode address_u_value,
      sampler::AddressingMode address_v_value,
      sampler::AddressingMode address_w_value,
      sampler::FilteringMode mag_filter_value,
      sampler::FilteringMode min_filter_value,
      sampler::FilteringMode mip_filter_value,
      uint8 max_anisotropy) {
    static_cast<ValueType*>(cmd)->Init(
        sampler_id,
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
  ResourceId sampler_id;
  uint32 sampler_states;
};

COMPILE_ASSERT(sizeof(SetSamplerStates) == 12,
               Sizeof_SetSamplerStates_is_not_12);
COMPILE_ASSERT(offsetof(SetSamplerStates, header) == 0,
               OffsetOf_SetSamplerStates_header_not_0);
COMPILE_ASSERT(offsetof(SetSamplerStates, sampler_id) == 4,
               OffsetOf_SetSamplerStates_sampler_id_not_4);
COMPILE_ASSERT(offsetof(SetSamplerStates, sampler_states) == 8,
               OffsetOf_SetSamplerStates_sampler_states_not_8);

struct SetSamplerBorderColor {
  typedef SetSamplerBorderColor ValueType;
  static const CommandId kCmdId = command_buffer::kSetSamplerBorderColor;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _sampler_id,
            float _red, float _green, float _blue, float _alpha) {
    SetHeader();
    sampler_id = _sampler_id;
    red = _red;
    green = _green;
    blue = _blue;
    alpha = _alpha;
  }

  static void* Set(void* cmd, ResourceId sampler_id,
                   float red, float green, float blue, float alpha) {
    static_cast<ValueType*>(cmd)->Init(sampler_id, red, green, blue, alpha);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId sampler_id;
  float red;
  float blue;
  float green;
  float alpha;
};

COMPILE_ASSERT(sizeof(SetSamplerBorderColor) == 24,
               Sizeof_SetSamplerBorderColor_is_not_24);
COMPILE_ASSERT(offsetof(SetSamplerBorderColor, header) == 0,
               OffsetOf_SetSamplerBorderColor_header_not_0);
COMPILE_ASSERT(offsetof(SetSamplerBorderColor, sampler_id) == 4,
               OffsetOf_SetSamplerBorderColor_sampler_id_not_4);
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

  void Init(ResourceId _sampler_id, ResourceId _texture_id) {
    SetHeader();
    sampler_id = _sampler_id;
    texture_id = _texture_id;
  }

  static void* Set(void* cmd, ResourceId sampler_id, ResourceId texture_id) {
    static_cast<ValueType*>(cmd)->Init(sampler_id, texture_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId sampler_id;
  ResourceId texture_id;
};

COMPILE_ASSERT(sizeof(SetSamplerTexture) == 12,
               Sizeof_SetSamplerTexture_is_not_12);
COMPILE_ASSERT(offsetof(SetSamplerTexture, header) == 0,
               OffsetOf_SetSamplerTexture_header_not_0);
COMPILE_ASSERT(offsetof(SetSamplerTexture, sampler_id) == 4,
               OffsetOf_SetSamplerTexture_sampler_id_not_4);
COMPILE_ASSERT(offsetof(SetSamplerTexture, texture_id) == 8,
               OffsetOf_SetSamplerTexture_texture_id_not_8);

struct SetScissor {
  typedef SetScissor ValueType;
  static const CommandId kCmdId = command_buffer::kSetScissor;
  static const ArgFlags kArgFlags = kFixed;

  // argument 0
  typedef BitField<0, 15> X;
  typedef BitField<15, 1> Unused;
  typedef BitField<16, 15> Y;
  typedef BitField<31, 1> Enable;
  // argument 1
  typedef BitField<0, 16> Width;
  typedef BitField<16, 16> Height;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _x,
            uint32 _y,
            uint32 _width,
            uint32 _height,
            bool _enable) {
    SetHeader();
    x_y_enable =
        X::MakeValue(_x) |
        Y::MakeValue(_y) |
        Enable::MakeValue(_enable ? 1 : 0);
    width_height = Width::MakeValue(_width) | Height::MakeValue(_height);
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
  uint32 x_y_enable;
  uint32 width_height;
};

COMPILE_ASSERT(sizeof(SetScissor) == 12, Sizeof_SetScissor_is_not_12);
COMPILE_ASSERT(offsetof(SetScissor, header) == 0,
               OffsetOf_SetScissor_header_not_0);
COMPILE_ASSERT(offsetof(SetScissor, x_y_enable) == 4,
               OffsetOf_SetScissor_x_y_enable_not_4);
COMPILE_ASSERT(offsetof(SetScissor, width_height) == 8,
               OffsetOf_SetScissor_width_height_not_8);

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

  // argument 0
  typedef BitField<0, 1> LineSmoothEnable;
  typedef BitField<1, 1> PointSpriteEnable;
  typedef BitField<2, 30> Unused;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(bool _line_smooth_enable, bool _point_sprite_enable,
            float _point_size) {
    SetHeader();
    enables =
        LineSmoothEnable::MakeValue(_line_smooth_enable ? 1 : 0) |
        PointSpriteEnable::MakeValue(_point_sprite_enable ? 1 : 0);
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
  uint32 enables;
  float point_size;
};

COMPILE_ASSERT(sizeof(SetPointLineRaster) == 12,
               Sizeof_SetPointLineRaster_is_not_12);
COMPILE_ASSERT(offsetof(SetPointLineRaster, header) == 0,
               OffsetOf_SetPointLineRaster_header_not_0);
COMPILE_ASSERT(offsetof(SetPointLineRaster, enables) == 4,
               OffsetOf_SetPointLineRaster_enables_not_4);
COMPILE_ASSERT(offsetof(SetPointLineRaster, point_size) == 8,
               OffsetOf_SetPointLineRaster_point_size_not_8);

struct SetPolygonRaster {
  typedef SetPolygonRaster ValueType;
  static const CommandId kCmdId = command_buffer::kSetPolygonRaster;
  static const ArgFlags kArgFlags = kFixed;

  // argument 0
  typedef BitField<0, 2> FillMode;
  typedef BitField<2, 2> CullMode;
  typedef BitField<4, 28> Unused;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(PolygonMode _fill_mode, FaceCullMode _cull_mode) {
    SetHeader();
    fill_cull = FillMode::MakeValue(_fill_mode) |
                CullMode::MakeValue(_cull_mode);
  }

  static void* Set(void* cmd, PolygonMode fill_mode, FaceCullMode cull_mode) {
    static_cast<ValueType*>(cmd)->Init(fill_mode, cull_mode);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 fill_cull;
};

COMPILE_ASSERT(sizeof(SetPolygonRaster) == 8,
               Sizeof_SetPolygonRaster_is_not_8);
COMPILE_ASSERT(offsetof(SetPolygonRaster, header) == 0,
               OffsetOf_SetPolygonRaster_header_not_0);
COMPILE_ASSERT(offsetof(SetPolygonRaster, fill_cull) == 4,
               OffsetOf_SetPolygonRaster_fill_cull_not_4);

struct SetAlphaTest {
  typedef SetAlphaTest ValueType;
  static const CommandId kCmdId = command_buffer::kSetAlphaTest;
  static const ArgFlags kArgFlags = kFixed;

  // argument 0
  typedef BitField<0, 3> Func;
  typedef BitField<3, 28> Unused;
  typedef BitField<31, 1> Enable;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(Comparison _func, bool _enable, float _value) {
    SetHeader();
    func_enable = Func::MakeValue(_func) | Enable::MakeValue(_enable ? 1 : 0);
    value = _value;
  }

  static void* Set(void* cmd, Comparison func, bool enable, float value) {
    static_cast<ValueType*>(cmd)->Init(func, enable, value);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 func_enable;
  float value;
};

COMPILE_ASSERT(sizeof(SetAlphaTest) == 12, Sizeof_SetAlphaTest_is_not_12);
COMPILE_ASSERT(offsetof(SetAlphaTest, header) == 0,
               OffsetOf_SetAlphaTest_header_not_0);
COMPILE_ASSERT(offsetof(SetAlphaTest, func_enable) == 4,
               OffsetOf_SetAlphaTest_func_enable_not_4);
COMPILE_ASSERT(offsetof(SetAlphaTest, value) == 8,
               OffsetOf_SetAlphaTest_value_not_8);

struct SetDepthTest {
  typedef SetDepthTest ValueType;
  static const CommandId kCmdId = command_buffer::kSetDepthTest;
  static const ArgFlags kArgFlags = kFixed;

  // argument 0
  typedef BitField<0, 3> Func;
  typedef BitField<3, 27> Unused;
  typedef BitField<30, 1> WriteEnable;
  typedef BitField<31, 1> Enable;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(Comparison _func, bool _write_enable, bool _enable) {
    SetHeader();
    func_enable =
        Func::MakeValue(_func) |
        WriteEnable::MakeValue(_write_enable ? 1 : 0) |
        Enable::MakeValue(_enable ? 1 : 0);
  }

  static void* Set(void* cmd,
                   Comparison func, bool write_enable, bool enable) {
    static_cast<ValueType*>(cmd)->Init(func, write_enable, enable);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  uint32 func_enable;
};

COMPILE_ASSERT(sizeof(SetDepthTest) == 8, Sizeof_SetDepthTest_is_not_8);
COMPILE_ASSERT(offsetof(SetDepthTest, header) == 0,
               OffsetOf_SetDepthTest_header_not_0);
COMPILE_ASSERT(offsetof(SetDepthTest, func_enable) == 4,
               OffsetOf_SetDepthTest_func_enable_not_4);

struct SetStencilTest {
  typedef SetStencilTest ValueType;
  static const CommandId kCmdId = command_buffer::kSetStencilTest;
  static const ArgFlags kArgFlags = kFixed;

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

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint8 _write_mask,
            uint8 _compare_mask,
            uint8 _reference_value,
            bool _separate_ccw,
            bool _enable,
            Comparison _cw_func,
            StencilOp _cw_pass_op,
            StencilOp _cw_fail_op,
            StencilOp _cw_z_fail_op,
            Comparison _ccw_func,
            StencilOp _ccw_pass_op,
            StencilOp _ccw_fail_op,
            StencilOp _ccw_z_fail_op) {
    SetHeader();
    stencil_args0 =
        WriteMask::MakeValue(_write_mask) |
        CompareMask::MakeValue(_compare_mask) |
        ReferenceValue::MakeValue(_reference_value) |
        SeparateCCW::MakeValue(_separate_ccw ? 1 : 0) |
        Enable::MakeValue(_enable ? 1 : 0);
    stencil_args1 =
        CWFunc::MakeValue(_cw_func) |
        CWPassOp::MakeValue(_cw_pass_op) |
        CWFailOp::MakeValue(_cw_fail_op) |
        CWZFailOp::MakeValue(_cw_z_fail_op) |
        CCWFunc::MakeValue(_ccw_func) |
        CCWPassOp::MakeValue(_ccw_pass_op) |
        CCWFailOp::MakeValue(_ccw_fail_op) |
        CCWZFailOp::MakeValue(_ccw_z_fail_op);
  }

  static void* Set(
      void* cmd,
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
  uint32 stencil_args0;
  uint32 stencil_args1;
};

COMPILE_ASSERT(sizeof(SetStencilTest) == 12,
               Sizeof_SetStencilTest_is_not_12);
COMPILE_ASSERT(offsetof(SetStencilTest, header) == 0,
               OffsetOf_SetStencilTest_header_not_0);
COMPILE_ASSERT(offsetof(SetStencilTest, stencil_args0) == 4,
               OffsetOf_SetStencilTest_stencil_args0_not_4);
COMPILE_ASSERT(offsetof(SetStencilTest, stencil_args1) == 8,
               OffsetOf_SetStencilTest_stencil_args1_not_8);

struct SetColorWrite {
  typedef SetColorWrite ValueType;
  static const CommandId kCmdId = command_buffer::kSetColorWrite;
  static const ArgFlags kArgFlags = kFixed;

  // argument 0
  typedef BitField<0, 1> RedMask;
  typedef BitField<1, 1> GreenMask;
  typedef BitField<2, 1> BlueMask;
  typedef BitField<3, 1> AlphaMask;
  typedef BitField<0, 4> AllColorsMask;  // alias for RGBA
  typedef BitField<4, 27> Unused;
  typedef BitField<31, 1> DitherEnable;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint8 _mask, bool _dither_enable) {
    SetHeader();
    flags =
        RedMask::MakeValue((_mask | 0x01) != 0 ? 1 : 0) |
        GreenMask::MakeValue((_mask | 0x02) != 0 ? 1 : 0) |
        BlueMask::MakeValue((_mask | 0x02) != 0 ? 1 : 0) |
        AlphaMask::MakeValue((_mask | 0x02) != 0 ? 1 : 0) |
        DitherEnable::MakeValue(_dither_enable ? 1 : 0);
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

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(
      BlendFunc _color_src_func,
      BlendFunc _color_dst_func,
      BlendEq _color_eq,
      BlendFunc _alpha_src_func,
      BlendFunc _alpha_dst_func,
      BlendEq _alpha_eq,
      bool _separate_alpha,
      bool _enable) {
    SetHeader();
    blend_settings =
        ColorSrcFunc::MakeValue(_color_src_func) |
        ColorDstFunc::MakeValue(_color_dst_func) |
        ColorEq::MakeValue(_color_eq) |
        AlphaSrcFunc::MakeValue(_alpha_src_func) |
        AlphaDstFunc::MakeValue(_alpha_dst_func) |
        AlphaEq::MakeValue(_alpha_eq) |
        SeparateAlpha::MakeValue(_separate_alpha ? 1 : 0) |
        Enable::MakeValue(_enable ? 1 : 0);
  }

  static void* Set(
      void* cmd,
      BlendFunc color_src_func,
      BlendFunc color_dst_func,
      BlendEq color_eq,
      BlendFunc alpha_src_func,
      BlendFunc alpha_dst_func,
      BlendEq alpha_eq,
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
  uint32 blend_settings;
};

COMPILE_ASSERT(sizeof(SetBlending) == 8, Sizeof_SetBlending_is_not_8);
COMPILE_ASSERT(offsetof(SetBlending, header) == 0,
               OffsetOf_SetBlending_header_not_0);
COMPILE_ASSERT(offsetof(SetBlending, blend_settings) == 4,
               OffsetOf_SetBlending_blend_settings_not_4);

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

  // argument 1
  typedef BitField<0, 16> Width;
  typedef BitField<16, 16> Height;
  // argument 2 may refer to side or depth
  typedef BitField<0, 16> Levels;
  typedef BitField<16, 16> Side;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _render_surface_id,
            ResourceId _texture_id, uint32 _width, uint32 _height,
            uint32 _level, uint32 _side) {
    SetHeader();
    render_surface_id = _render_surface_id;
    // TODO(gman): Why does this need a width and height. It's inherited from
    // the texture isn't it?
    width_height = Width::MakeValue(_width) | Height::MakeValue(_height);
    levels_side = Levels::MakeValue(_level) | Side::MakeValue(_side);
    texture_id = _texture_id;
  }

  static void* Set(void* cmd,
                   ResourceId render_surface_id, ResourceId texture_id,
                   uint32 width, uint32 height,
                   uint32 level, uint32 side) {
    static_cast<ValueType*>(cmd)->Init(render_surface_id, texture_id,
                                       width, height,
                                       level, side);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  ResourceId render_surface_id;
  uint32 width_height;
  uint32 levels_side;
  ResourceId texture_id;
};

COMPILE_ASSERT(sizeof(CreateRenderSurface) == 20,
               Sizeof_CreateRenderSurface_is_not_20);
COMPILE_ASSERT(offsetof(CreateRenderSurface, header) == 0,
               OffsetOf_CreateRenderSurface_header_not_0);
COMPILE_ASSERT(offsetof(CreateRenderSurface, render_surface_id) == 4,
               OffsetOf_CreateRenderSurface_render_surface_id_not_4);
COMPILE_ASSERT(offsetof(CreateRenderSurface, width_height) == 8,
               OffsetOf_CreateRenderSurface_width_height_not_8);
COMPILE_ASSERT(offsetof(CreateRenderSurface, levels_side) == 12,
               OffsetOf_CreateRenderSurface_levels_side_not_12);
COMPILE_ASSERT(offsetof(CreateRenderSurface, texture_id) == 16,
               OffsetOf_CreateRenderSurface_texture_id_not_16);

struct DestroyRenderSurface {
  typedef DestroyRenderSurface ValueType;
  static const CommandId kCmdId = command_buffer::kDestroyRenderSurface;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _render_surface_id) {
    SetHeader();
    render_surface_id = _render_surface_id;
  }

  static void* Set(void* cmd, ResourceId render_surface_id) {
    static_cast<ValueType*>(cmd)->Init(render_surface_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId render_surface_id;
};

COMPILE_ASSERT(sizeof(DestroyRenderSurface) == 8,
               Sizeof_DestroyRenderSurface_is_not_8);
COMPILE_ASSERT(offsetof(DestroyRenderSurface, header) == 0,
               OffsetOf_DestroyRenderSurface_header_not_0);
COMPILE_ASSERT(offsetof(DestroyRenderSurface, render_surface_id) == 4,
               OffsetOf_DestroyRenderSurface_render_surface_id_not_4);

struct CreateDepthSurface {
  typedef CreateDepthSurface ValueType;
  static const CommandId kCmdId = command_buffer::kCreateDepthSurface;
  static const ArgFlags kArgFlags = kFixed;

  // argument 1
  typedef BitField<0, 16> Width;
  typedef BitField<16, 16> Height;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _depth_surface_id, uint32 _width, uint32 _height) {
    SetHeader();
    depth_surface_id = _depth_surface_id;
    width_height = Width::MakeValue(_width) | Height::MakeValue(_height);
  }

  static void* Set(void* cmd, ResourceId depth_surface_id,
                   uint32 width, uint32 height) {
    static_cast<ValueType*>(cmd)->Init(depth_surface_id, width, height);
    return NextCmdAddress<ValueType>(cmd);
  }

  // TODO(gman): fix this to not use obfusticated fields.
  CommandHeader header;
  ResourceId depth_surface_id;
  uint32 width_height;
};

COMPILE_ASSERT(sizeof(CreateDepthSurface) == 12,
               Sizeof_CreateDepthSurface_is_not_12);
COMPILE_ASSERT(offsetof(CreateDepthSurface, header) == 0,
               OffsetOf_CreateDepthSurface_header_not_0);
COMPILE_ASSERT(offsetof(CreateDepthSurface, depth_surface_id) == 4,
               OffsetOf_CreateDepthSurface_depth_surface_id_not_4);
COMPILE_ASSERT(offsetof(CreateDepthSurface, width_height) == 8,
               OffsetOf_CreateDepthSurface_width_height_not_8);

struct DestroyDepthSurface {
  typedef DestroyDepthSurface ValueType;
  static const CommandId kCmdId = command_buffer::kDestroyDepthSurface;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _depth_surface_id) {
    SetHeader();
    depth_surface_id = _depth_surface_id;
  }

  static void* Set(void* cmd, ResourceId depth_surface_id) {
    static_cast<ValueType*>(cmd)->Init(depth_surface_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId depth_surface_id;
};

COMPILE_ASSERT(sizeof(DestroyDepthSurface) == 8,
               Sizeof_DestroyDepthSurface_is_not_8);
COMPILE_ASSERT(offsetof(DestroyDepthSurface, header) == 0,
               OffsetOf_DestroyDepthSurface_header_not_0);
COMPILE_ASSERT(offsetof(DestroyDepthSurface, depth_surface_id) == 4,
               OffsetOf_DestroyDepthdepth_surface_id_not_4);

struct SetRenderSurface {
  typedef SetRenderSurface ValueType;
  static const CommandId kCmdId = command_buffer::kSetRenderSurface;
  static const ArgFlags kArgFlags = kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(ResourceId _render_surface_id, ResourceId _depth_surface_id) {
    SetHeader();
    render_surface_id = _render_surface_id;
    depth_surface_id = _depth_surface_id;
  }

  static void* Set(void* cmd,
                   ResourceId render_surface_id, ResourceId depth_surface_id) {
    static_cast<ValueType*>(cmd)->Init(render_surface_id, depth_surface_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  ResourceId render_surface_id;
  ResourceId depth_surface_id;
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

}  // namespace o3d
}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_CMD_BUFFER_FORMAT_H_
