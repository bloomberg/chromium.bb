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

#include <stddef.h>
#include "core/cross/message_commands.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

#define O3D_OFFSETOF(type, field) \
  static_cast<size_t>( \
    reinterpret_cast<char*>(&(reinterpret_cast<type*>(1))->field) - \
  reinterpret_cast<char *>(1))

class MessageCommandsTest : public testing::Test {
};

TEST_F(MessageCommandsTest, GetMessageDescription) {
  EXPECT_STREQ(IMCMessage::GetMessageDescription(
      IMCMessage::ALLOCATE_SHARED_MEMORY), "ALLOCATE_SHARED_MEMORY");
}

TEST_F(MessageCommandsTest, IMCMessage) {
  IMCMessage msg(IMCMessage::INVALID_ID);
  EXPECT_EQ(IMCMessage::INVALID_ID, msg.message_id);
  EXPECT_EQ(4u, sizeof msg);
  EXPECT_EQ(0u, O3D_OFFSETOF(IMCMessage, message_id));
}

TEST_F(MessageCommandsTest, MessageInvalidIdTest) {
  MessageInvalidId msg;
  EXPECT_EQ(static_cast<int>(IMCMessage::INVALID_ID), 0);
  //EXPECT_EQ(IMCMessage::INVALID_ID, MessageInvalidId::kMessageId);
  EXPECT_EQ(IMCMessage::INVALID_ID, msg.message_id);
  EXPECT_EQ(4u, sizeof(msg));
  EXPECT_EQ(0u, O3D_OFFSETOF(MessageInvalidId, message_id));
}

TEST_F(MessageCommandsTest, MessageHelloTest) {
  MessageHello msg;
  EXPECT_EQ(static_cast<int>(IMCMessage::HELLO), 1);
  //EXPECT_EQ(IMCMessage::HELLO, MessageHello::kMessageId);
  EXPECT_EQ(msg.message_id, IMCMessage::HELLO);
  EXPECT_EQ(4u, sizeof msg);
  EXPECT_EQ(0u, O3D_OFFSETOF(MessageHello, message_id));
}

TEST_F(MessageCommandsTest, MessageAllocateSharedMemoryTest) {
  MessageAllocateSharedMemory msg;
  EXPECT_EQ(static_cast<int>(IMCMessage::ALLOCATE_SHARED_MEMORY), 2);
  //EXPECT_EQ(IMCMessage::ALLOCATE_SHARED_MEMORY,
  //          MessageAllocateSharedMemory::kMessageId);
  EXPECT_EQ(IMCMessage::ALLOCATE_SHARED_MEMORY, msg.message_id);
  EXPECT_EQ(8u, sizeof msg);
  EXPECT_EQ(0u, O3D_OFFSETOF(MessageAllocateSharedMemory, message_id));
  EXPECT_EQ(4u, O3D_OFFSETOF(MessageAllocateSharedMemory, mem_size));
}

TEST_F(MessageCommandsTest, MessageUpdateTexture2D) {
  MessageUpdateTexture2D msg;
  EXPECT_EQ(static_cast<int>(IMCMessage::UPDATE_TEXTURE2D), 3);
  //EXPECT_EQ(IMCMessage::UPDATE_TEXTURE2D, MessageUpdateTexture2D::kMessageId);
  EXPECT_EQ(IMCMessage::UPDATE_TEXTURE2D, msg.message_id);
  EXPECT_EQ(24u, sizeof msg);
  EXPECT_EQ(0u, O3D_OFFSETOF(MessageUpdateTexture2D, message_id));
  EXPECT_EQ(4u, O3D_OFFSETOF(MessageUpdateTexture2D, texture_id));
  EXPECT_EQ(8u, O3D_OFFSETOF(MessageUpdateTexture2D, level));
  EXPECT_EQ(12u, O3D_OFFSETOF(MessageUpdateTexture2D, shared_memory_id));
  EXPECT_EQ(16u, O3D_OFFSETOF(MessageUpdateTexture2D, offset));
  EXPECT_EQ(20u, O3D_OFFSETOF(MessageUpdateTexture2D, number_of_bytes));
}

TEST_F(MessageCommandsTest, MessageRegisterSharedMemory) {
  MessageRegisterSharedMemory msg;
  EXPECT_EQ(static_cast<int>(IMCMessage::REGISTER_SHARED_MEMORY), 4);
  //EXPECT_EQ(IMCMessage::REGISTER_SHARED_MEMORY,
  //          MessageRegisterSharedMemory::kMessageId);
  EXPECT_EQ(IMCMessage::REGISTER_SHARED_MEMORY, msg.message_id);
  EXPECT_EQ(8u, sizeof msg);
  EXPECT_EQ(0u, O3D_OFFSETOF(MessageRegisterSharedMemory, message_id));
  EXPECT_EQ(4u, O3D_OFFSETOF(MessageRegisterSharedMemory, mem_size));
}

TEST_F(MessageCommandsTest, MessageUnregisterSharedMemory) {
  MessageUnregisterSharedMemory msg;
  EXPECT_EQ(static_cast<int>(IMCMessage::UNREGISTER_SHARED_MEMORY), 5);
  //EXPECT_EQ(IMCMessage::UNREGISTER_SHARED_MEMORY,
  //          MessageUnregisterSharedMemory::kMessageId);
  EXPECT_EQ(IMCMessage::UNREGISTER_SHARED_MEMORY, msg.message_id);
  EXPECT_EQ(8u, sizeof msg);
  EXPECT_EQ(0u, O3D_OFFSETOF(MessageUnregisterSharedMemory, message_id));
  EXPECT_EQ(4u, O3D_OFFSETOF(MessageUnregisterSharedMemory, buffer_id));
}

TEST_F(MessageCommandsTest, MessageUpdateTexture2DRect) {
  MessageUpdateTexture2DRect msg;
  EXPECT_EQ(static_cast<int>(IMCMessage::UPDATE_TEXTURE2D_RECT), 6);
  //EXPECT_EQ(IMCMessage::UPDATE_TEXTURE2D_RECT,
  //          MessageUpdateTexture2DRect::kMessageId);
  EXPECT_EQ(IMCMessage::UPDATE_TEXTURE2D_RECT, msg.message_id);
  EXPECT_EQ(40u, sizeof msg);
  EXPECT_EQ(0u, O3D_OFFSETOF(MessageUpdateTexture2DRect, message_id));
  EXPECT_EQ(4u, O3D_OFFSETOF(MessageUpdateTexture2DRect, texture_id));
  EXPECT_EQ(8u, O3D_OFFSETOF(MessageUpdateTexture2DRect, level));
  EXPECT_EQ(12u, O3D_OFFSETOF(MessageUpdateTexture2DRect, x));
  EXPECT_EQ(16u, O3D_OFFSETOF(MessageUpdateTexture2DRect, y));
  EXPECT_EQ(20u, O3D_OFFSETOF(MessageUpdateTexture2DRect, width));
  EXPECT_EQ(24u, O3D_OFFSETOF(MessageUpdateTexture2DRect, height));
  EXPECT_EQ(28u, O3D_OFFSETOF(MessageUpdateTexture2DRect, shared_memory_id));
  EXPECT_EQ(32u, O3D_OFFSETOF(MessageUpdateTexture2DRect, offset));
  EXPECT_EQ(36u, O3D_OFFSETOF(MessageUpdateTexture2DRect, number_of_bytes));
}

}  // namespace o3d


