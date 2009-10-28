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


// This file contains the common parts of command buffer formats.

#ifndef O3D_COMMAND_BUFFER_COMMON_CROSS_CMD_BUFFER_COMMON_H_
#define O3D_COMMAND_BUFFER_COMMON_CROSS_CMD_BUFFER_COMMON_H_

#include "base/basictypes.h"
#include "command_buffer/common/cross/types.h"
#include "command_buffer/common/cross/bitfield_helpers.h"
#include "command_buffer/common/cross/logging.h"
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
    DCHECK_LT(_size, 256u);
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


namespace cmd {

// This macro is used to safely and convienently expand the list of commnad
// buffer commands in to various lists and never have them get out of sync. To
// add a new command, add it this list, create the corresponding structure below
// and then add a function in gapi_decoder.cc called Handle_COMMAND_NAME where
// COMMAND_NAME is the name of your command structure.
//
// NOTE: THE ORDER OF THESE MUST NOT CHANGE (their id is derived by order)
#define COMMON_COMMAND_BUFFER_CMDS(OP) \
  OP(Noop)                          /*  0 */ \
  OP(SetToken)                      /*  1 */ \

// Common commands.
enum CommandId {
  #define COMMON_COMMAND_BUFFER_CMD_OP(name) k ## name,

  COMMON_COMMAND_BUFFER_CMDS(COMMON_COMMAND_BUFFER_CMD_OP)

  #undef COMMON_COMMAND_BUFFER_CMD_OP

  kNumCommands,
  kLastCommonId = 1023,  // reserve 1024 spaces for common commands.
};

COMPILE_ASSERT(kNumCommands - 1 <= kLastCommonId, Too_many_common_commands);

const char* GetCommandName(CommandId id);

struct Noop {
  typedef Noop ValueType;
  static const CommandId kCmdId = kNoop;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;

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
  static const CommandId kCmdId = kSetToken;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;

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

}  // namespace cmd

O3D_POP_STRUCTURE_PACKING;

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_CMD_BUFFER_COMMON_H_

