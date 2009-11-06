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

namespace o3d {

// Max length of the string storing the version of O3D as returned by the
// GET_VERSION message
#define MAX_VERSION_STRING_LENGTH 128

// Make sure the compiler does not add extra padding to any of the message
// structures.
#pragma pack(push, 1)

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
  OP(RENDER, MessageRender) \
  OP(GET_VERSION, MessageGetVersion) \


namespace imc {
enum MessageId {
  #define O3D_IMC_MESSAGE_OP(id, class_name)  id,
  O3D_IMC_MESSAGE_LIST(O3D_IMC_MESSAGE_OP)
  #undef O3D_IMC_MESSAGE_OP

  MAX_NUM_IDS,

  ID_FORCE_DWORD = 0x7fffffff  // Forces a 32-bit size enum
};

// Returns a string by message ID.
const char* GetMessageDescription(MessageId id);
};

// An invalid message. This is mostly a place holder for id 0.
struct MessageInvalidId {
  // Message Content.
  struct Msg {
    static const imc::MessageId kMessageId = imc::INVALID_ID;

    imc::MessageId message_id;
  };

  MessageInvalidId() {
    msg.message_id = Msg::kMessageId;
  }

  Msg msg;
};

// The first message you send.
struct MessageHello {
  // Message Content.
  struct Msg {
    static const imc::MessageId kMessageId = imc::HELLO;

    imc::MessageId message_id;
  };

  MessageHello() {
    msg.message_id = Msg::kMessageId;
  }

  Msg msg;
};

// A message to allocate shared memory
struct MessageAllocateSharedMemory {
  // Message Content.
  struct Msg {
    static const imc::MessageId kMessageId = imc::ALLOCATE_SHARED_MEMORY;
    static const int32 kMaxSharedMemSize = 1024 * 1024 * 128;   // 128MB

    imc::MessageId message_id;

    // The amount of memory to allocate.
    int32 mem_size;
  };

  // The response data.
  struct ResponseData {
    int32 buffer_id;
  };

  // A wrapper to manage the response data.
  struct Response {
   public:
    Response(int32 buffer_id) {
      data.buffer_id = buffer_id;
    }

    ResponseData data;
  };

  MessageAllocateSharedMemory() {
    msg.message_id = Msg::kMessageId;
  }

  // Parameters:
  //   mem_size: The number of bytes to allocate.
  explicit MessageAllocateSharedMemory(int32 mem_size) {
    msg.message_id = Msg::kMessageId;
    msg.mem_size = mem_size;
  }

  Msg msg;
};

// A message to update the entire contents of a 2D texture. The number
// of bytes MUST equal the size of the entire texture to be updated including
// all mips.
struct MessageUpdateTexture2D {
  // Message Content.
  struct Msg {
    static const imc::MessageId kMessageId = imc::UPDATE_TEXTURE2D;

    imc::MessageId message_id;

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

  MessageUpdateTexture2D() {
    msg.message_id = Msg::kMessageId;
  }

  // Parameters:
  //   texture_id: The id of the texture to set.
  //   level: The mip level of the texture to set.
  //   shared_memory_id: The id of the shared memory the contains the data
  //       to use to set the texture.
  //   offset: The offset inside the shared memory where the texture data
  //       starts.
  //   number_of_bytes: The number of bytes to get out of shared memory.
  //       NOTE: this number MUST match the size of the texture. For example for
  //       an ARGB texture it must be mip_width * mip_height * 4 * sizeof(uint8)
  MessageUpdateTexture2D(Id texture_id,
                         int32 level,
                         int32 shared_memory_id,
                         int32 offset,
                         int32 number_of_bytes) {
    msg.message_id = Msg::kMessageId;
    msg.texture_id = texture_id;
    msg.level = level;
    msg.shared_memory_id = shared_memory_id;
    msg.offset = offset;
    msg.number_of_bytes = number_of_bytes;
  }

  Msg msg;
};

// A message to register shared memory.
struct MessageRegisterSharedMemory {
  // Message Content.
  struct Msg {
    static const imc::MessageId kMessageId = imc::REGISTER_SHARED_MEMORY;
    static const int32 kMaxSharedMemSize = 1024 * 1024 * 128;   // 128MB

    imc::MessageId message_id;
    int32 mem_size;
  };

  MessageRegisterSharedMemory() {
    msg.message_id = Msg::kMessageId;
  }

  explicit MessageRegisterSharedMemory(int32 mem_size) {
    msg.message_id = Msg::kMessageId;
    msg.mem_size = mem_size;
  }

  Msg msg;
};

// A message to unregister shared memory.
struct MessageUnregisterSharedMemory {
  // Message Content.
  struct Msg {
    static const imc::MessageId kMessageId = imc::UNREGISTER_SHARED_MEMORY;

    imc::MessageId message_id;
    int32 buffer_id;
  };

  MessageUnregisterSharedMemory() {
    msg.message_id = Msg::kMessageId;
  }

  // Parameters:
  //   buffer_id: The id of the buffer to unregister.
  explicit MessageUnregisterSharedMemory(int32 buffer_id) {
    msg.message_id = Msg::kMessageId;
    msg.buffer_id = buffer_id;
  }

  Msg msg;
};

// A message to update a portion of a 2D texture. The number of bytes MUST equal
// the size of the portion of the texture to be updated.
struct MessageUpdateTexture2DRect {
  // Message Content.
  struct Msg {
    static const imc::MessageId kMessageId = imc::UPDATE_TEXTURE2D_RECT;

    imc::MessageId message_id;

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

    // The number of bytes bytes across 1 row in the source data.
    int32 pitch;
  };

  MessageUpdateTexture2DRect() {
    msg.message_id = Msg::kMessageId;
  }

  // Parameters:
  //   texture_id: The id of the texture to set.
  //   level: The mip level of the texture to set.
  //   x: The left edge of the rectangle to update in the texture.
  //   y: The top edge of the rectangle to update in the texture.
  //   width: The width of the rectangle to update in the texture.
  //   height: The height of the rectangle to update in the texture.
  //   shared_memory_id: The id of the shared memory the contains the data to
  //       use to set the texture.
  //   offset: The offset inside the shared memory where the texture data
  //       starts.
  //   pitch: The number of bytes bytes across 1 row in the source data.
  MessageUpdateTexture2DRect(Id texture_id,
                             int32 level,
                             int32 x,
                             int32 y,
                             int32 width,
                             int32 height,
                             int32 shared_memory_id,
                             int32 offset,
                             int32 pitch) {
    msg.message_id = Msg::kMessageId;
    msg.texture_id = texture_id;
    msg.level = level;
    msg.x = x;
    msg.y = y;
    msg.width = width;
    msg.height = height;
    msg.shared_memory_id = shared_memory_id;
    msg.offset = offset;
    msg.pitch = pitch;
  }

  Msg msg;
};

// Tell O3D to render. This is generally used when O3D is in Render on demand
// mode.
struct MessageRender {
  // Message Content.
  struct Msg {
    static const imc::MessageId kMessageId = imc::RENDER;

    imc::MessageId message_id;
  };

  MessageRender() {
    msg.message_id = Msg::kMessageId;
  }

  Msg msg;
};

// Get the O3D version.
struct MessageGetVersion {
  // Message Content.
  struct Msg {
    static const imc::MessageId kMessageId = imc::GET_VERSION;

    imc::MessageId message_id;
  };

  // The response data.
  struct ResponseData {
    // a null terminated version string in the format "x.x.x.x" where x is an
    // integer number. Note: There may be other data after the last digit which
    // is currently undefined.
    char version[MAX_VERSION_STRING_LENGTH];
  };

  // A wrapper to manage the response data.
  struct Response {
   public:
    Response(const char* version) {
      strncpy(data.version, version,
              std::min(strlen(version) + 1, sizeof(data.version)));
      data.version[sizeof(data.version) - 1] = '\0';
    }

    ResponseData data;
  };


  MessageGetVersion() {
    msg.message_id = Msg::kMessageId;
  }

  Msg msg;
};

#pragma pack(pop)

}  // namespace o3d

#endif  // O3D_CORE_CROSS_MESSAGE_COMMANDS_H_

