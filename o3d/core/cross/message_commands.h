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

// This file contains the declarations of various IMC messages for O3D.

#ifndef O3D_CORE_CROSS_MESSAGE_COMMANDS_H_
#define O3D_CORE_CROSS_MESSAGE_COMMANDS_H_

#include "core/cross/types.h"
#include "core/cross/packing.h"

namespace o3d {

// Make sure the compiler does not add extra padding to any of the message
// structures.
O3D_PUSH_STRUCTURE_PACKING_1;

// This macro is used to safely and convienently expand the list of possible IMC
// messages in to various lists and never have them get out of sync. To add a
// new message add a this list, the first argument is the enum Id, the second
// argument is the name of the structure that describes the message. Once you've
// added it to this list, create the structure below and then add a function in
// message_queue.cc called ProcessMessageStructureName where
// MessageStructureName is the name of your message structure.
//
// NOTE: THE ORDER OF THESE MUST NOT CHANGE (their id is derived by order)
#define O3D_IMC_MESSAGE_LIST(OP) \
  OP(INVALID_ID, MessageInvalidId) \
  OP(HELLO, MessageHello) \
  OP(ALLOCATE_SHARED_MEMORY, MessageAllocateSharedMemory) \
  OP(UPDATE_TEXTURE2D, MessageUpdateTexture2D) \
  OP(REGISTER_SHARED_MEMORY, MessageRegisterSharedMemory) \
  OP(UNREGISTER_SHARED_MEMORY, MessageUnregisterSharedMemory) \
  OP(UPDATE_TEXTURE2D_RECT, MessageUpdateTexture2DRect) \


// The base of all IMCMessages
struct IMCMessage {
  enum MessageId {
    #define O3D_IMC_MESSAGE_OP(id, class_name)  id,
    O3D_IMC_MESSAGE_LIST(O3D_IMC_MESSAGE_OP)
    #undef O3D_IMC_MESSAGE_OP

    MAX_NUM_IDS,

    ID_FORCE_DWORD = 0x7fffffff  // Forces a 32-bit size enum
  };

  explicit IMCMessage(MessageId id) : message_id(id) {
  }

  // Returns a string by message ID.
  static const char* GetMessageDescription(MessageId id);

  int32 message_id;
};

// An invalid message. This is mostly a place holder for id 0.
struct MessageInvalidId : public IMCMessage {
  static const IMCMessage::MessageId kMessageId = INVALID_ID;

  MessageInvalidId()
      : IMCMessage(kMessageId) {
  }
};

// The first message you send.
struct MessageHello : public IMCMessage {
  static const IMCMessage::MessageId kMessageId = HELLO;

  MessageHello()
      : IMCMessage(kMessageId) {
  }
};

// A message to allocate shared memory
struct MessageAllocateSharedMemory : public IMCMessage {
  static const MessageId kMessageId = ALLOCATE_SHARED_MEMORY;
  static const int32 kMaxSharedMemSize = 1024 * 1024 * 128;   // 128MB

  MessageAllocateSharedMemory()
      : IMCMessage(kMessageId) {
  }

  // Parameters:
  //   in_mem_size: The number of bytes to allocate.
  explicit MessageAllocateSharedMemory(int32 in_mem_size)
      : IMCMessage(kMessageId),
        mem_size(in_mem_size) {
  }

  // The amount of memory to allocate.
  int32 mem_size;
};

// A message to register shared memory.
struct MessageRegisterSharedMemory : public IMCMessage {
  static const MessageId kMessageId = REGISTER_SHARED_MEMORY;
  static const int32 kMaxSharedMemSize = 1024 * 1024 * 128;   // 128MB

  MessageRegisterSharedMemory()
      : IMCMessage(kMessageId) {
  }
  explicit MessageRegisterSharedMemory(int32 in_mem_size)
      : IMCMessage(kMessageId),
        mem_size(in_mem_size) {
  }

  int32 mem_size;
};

// A message to unregister shared memory.
struct MessageUnregisterSharedMemory : public IMCMessage {
  static const MessageId kMessageId = UNREGISTER_SHARED_MEMORY;

  MessageUnregisterSharedMemory()
      : IMCMessage(kMessageId) {
  }

  // Parameters:
  //   in_buffer_id: The id of the buffer to unregister.
  explicit MessageUnregisterSharedMemory(int32 in_buffer_id)
      : IMCMessage(kMessageId),
        buffer_id(in_buffer_id) {
  }

  int32 buffer_id;
};

// A message to update the entire contents of a 2D texture. The number
// of bytes MUST equal the size of the entire texture to be updated including
// all mips.
struct MessageUpdateTexture2D : public IMCMessage {
  static const MessageId kMessageId = UPDATE_TEXTURE2D;

  MessageUpdateTexture2D()
      : IMCMessage(kMessageId) {
  }

  // Parameters:
  //   in_texture_id: The id of the texture to set.
  //   in_level: The mip level of the texture to set.
  //   in_shared_memory_id: The id of the shared memory the contains the data
  //       to use to set the texture.
  //   in_offset: The offset inside the shared memory where the texture data
  //       starts.
  //   in_number_of_bytes: The number of bytes to get out of shared memory.
  //       NOTE: this number MUST match the size of the texture. For example for
  //       an ARGB texture it must be mip_width * mip_height * 4 * sizeof(uint8)
  MessageUpdateTexture2D(Id in_texture_id,
                         int32 in_level,
                         int32 in_shared_memory_id,
                         int32 in_offset,
                         int32 in_number_of_bytes)
      : IMCMessage(kMessageId),
        texture_id(in_texture_id),
        level(in_level),
        shared_memory_id(in_shared_memory_id),
        offset(in_offset),
        number_of_bytes(in_number_of_bytes) {
  }

  // The id of the texture to set.
  Id texture_id;

  // The mip level of the texture to set.
  int32 level;

  // The id of the shared memory the contains the data to use to set the
  // texture.
  int32 shared_memory_id;

  // The offset inside the shared memory where the texture data starts.
  int32 offset;

  // The number of bytes to get out of shared memory.
  // NOTE: this number MUST match the size of the texture. For example for an
  // ARGB texture it must be mip_width * mip_height * 4 * sizeof(uint8)
  int32 number_of_bytes;
};

// A message to update a portion of a 2D texture. The number of bytes MUST equal
// the size of the portion of the texture to be updated.
struct MessageUpdateTexture2DRect : public IMCMessage {
  static const MessageId kMessageId = UPDATE_TEXTURE2D_RECT;

  MessageUpdateTexture2DRect()
      : IMCMessage(kMessageId) {
  }

  // Parameters:
  //   in_texture_id: The id of the texture to set.
  //   in_level: The mip level of the texture to set.
  //   in_x: The left edge of the rectangle to update in the texture.
  //   in_y: The top edge of the rectangle to update in the texture.
  //   in_width: The width of the rectangle to update in the texture.
  //   in_height: The height of the rectangle to update in the texture.
  //   in_shared_memory_id: The id of the shared memory the contains the data to
  //       use to set the texture.
  //   in_offset: The offset inside the shared memory where the texture data
  //       starts.
  //   in_number_of_bytes: The number of bytes to get out of shared memory.
  //       NOTE: this number MUST match the size of the area in the texture to
  //       be updated. For example for an ARGB texture it must be
  //       width * height 4 * sizeof(uint8)
  MessageUpdateTexture2DRect(Id in_texture_id,
                             int32 in_level,
                             int32 in_x,
                             int32 in_y,
                             int32 in_width,
                             int32 in_height,
                             int32 in_shared_memory_id,
                             int32 in_offset,
                             int32 in_number_of_bytes)
      : IMCMessage(kMessageId),
        texture_id(in_texture_id),
        level(in_level),
        x(in_x),
        y(in_y),
        width(in_width),
        height(in_height),
        shared_memory_id(in_shared_memory_id),
        offset(in_offset),
        number_of_bytes(in_number_of_bytes) {
  }

  // The id of the texture to set.
  Id texture_id;

  // The mip level of the texture to set.
  int32 level;

  // The left edge of the rectangle to update in the texture.
  int32 x;

  // The top edge of the rectangle to update in the texture.
  int32 y;

  // The width of the rectangle to update in the texture.
  int32 width;

  // The height of the rectangle to update in the texture.
  int32 height;

  // The id of the shared memory the contains the data to use to set the
  // texture.
  int32 shared_memory_id;

  // The offset inside the shared memory where the texture data starts.
  int32 offset;

  // The number of bytes to get out of shared memory.
  // NOTE: this number MUST match the size of the area in the texture to be
  // updated. For example for an ARGB texture it must be
  // width * height 4 * sizeof(uint8)
  int32 number_of_bytes;
};

O3D_POP_STRUCTURE_PACKING;

}  // namespace o3d

#endif  // O3D_CORE_CROSS_MESSAGE_COMMANDS_H_

