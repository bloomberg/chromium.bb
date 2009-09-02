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

class MessageCommandsTest : public testing::Test {
};

TEST_F(MessageCommandsTest, GetMessageDescription) {
  EXPECT_STREQ(imc::GetMessageDescription(
      imc::ALLOCATE_SHARED_MEMORY), "ALLOCATE_SHARED_MEMORY");
}

TEST_F(MessageCommandsTest, MessageInvalidIdTest) {
  EXPECT_EQ(static_cast<int>(imc::INVALID_ID), 0);
  EXPECT_EQ(0u, offsetof(MessageInvalidId::Msg, message_id));
  MessageInvalidId msg;
  EXPECT_EQ(imc::INVALID_ID, msg.msg.message_id);
  EXPECT_EQ(4u, sizeof msg.msg);
}

TEST_F(MessageCommandsTest, MessageHelloTest) {
  EXPECT_EQ(static_cast<int>(imc::HELLO), 1);
  EXPECT_EQ(0u, offsetof(MessageHello::Msg, message_id));
  MessageHello msg;
  EXPECT_EQ(imc::HELLO, msg.msg.message_id);
  EXPECT_EQ(4u, sizeof msg.msg);
}

TEST_F(MessageCommandsTest, MessageAllocateSharedMemoryTest) {
  EXPECT_EQ(static_cast<int>(imc::ALLOCATE_SHARED_MEMORY), 2);
  EXPECT_EQ(0u, offsetof(MessageAllocateSharedMemory::Msg, message_id));
  EXPECT_EQ(4u, offsetof(MessageAllocateSharedMemory::Msg, mem_size));
  MessageAllocateSharedMemory msg;
  EXPECT_EQ(imc::ALLOCATE_SHARED_MEMORY, msg.msg.message_id);
  EXPECT_EQ(8u, sizeof msg.msg);
  MessageAllocateSharedMemory msg2(3);
  EXPECT_EQ(imc::ALLOCATE_SHARED_MEMORY, msg2.msg.message_id);
  EXPECT_EQ(3, msg2.msg.mem_size);
}

TEST_F(MessageCommandsTest, MessageUpdateTexture2D) {
  EXPECT_EQ(static_cast<int>(imc::UPDATE_TEXTURE2D), 3);
  EXPECT_EQ(0u, offsetof(MessageUpdateTexture2D::Msg, message_id));
  EXPECT_EQ(4u, offsetof(MessageUpdateTexture2D::Msg, texture_id));
  EXPECT_EQ(8u, offsetof(MessageUpdateTexture2D::Msg, level));
  EXPECT_EQ(12u, offsetof(MessageUpdateTexture2D::Msg, shared_memory_id));
  EXPECT_EQ(16u, offsetof(MessageUpdateTexture2D::Msg, offset));
  EXPECT_EQ(20u, offsetof(MessageUpdateTexture2D::Msg, number_of_bytes));
  MessageUpdateTexture2D msg;
  EXPECT_EQ(imc::UPDATE_TEXTURE2D, msg.msg.message_id);
  EXPECT_EQ(24u, sizeof msg.msg);
  MessageUpdateTexture2D msg2(2, 3, 4, 5, 6);
  EXPECT_EQ(imc::UPDATE_TEXTURE2D, msg2.msg.message_id);
  EXPECT_EQ(2u, msg2.msg.texture_id);
  EXPECT_EQ(3, msg2.msg.level);
  EXPECT_EQ(4, msg2.msg.shared_memory_id);
  EXPECT_EQ(5, msg2.msg.offset);
  EXPECT_EQ(6, msg2.msg.number_of_bytes);
}

TEST_F(MessageCommandsTest, MessageRegisterSharedMemory) {
  EXPECT_EQ(static_cast<int>(imc::REGISTER_SHARED_MEMORY), 4);
  EXPECT_EQ(0u, offsetof(MessageRegisterSharedMemory::Msg, message_id));
  EXPECT_EQ(4u, offsetof(MessageRegisterSharedMemory::Msg, mem_size));
  MessageRegisterSharedMemory msg;
  EXPECT_EQ(imc::REGISTER_SHARED_MEMORY, msg.msg.message_id);
  EXPECT_EQ(8u, sizeof msg.msg);
  MessageRegisterSharedMemory msg2(123);
  EXPECT_EQ(imc::REGISTER_SHARED_MEMORY, msg2.msg.message_id);
  EXPECT_EQ(123, msg2.msg.mem_size);
}

TEST_F(MessageCommandsTest, MessageUnregisterSharedMemory) {
  EXPECT_EQ(static_cast<int>(imc::UNREGISTER_SHARED_MEMORY), 5);
  EXPECT_EQ(0u, offsetof(MessageUnregisterSharedMemory::Msg, message_id));
  EXPECT_EQ(4u, offsetof(MessageUnregisterSharedMemory::Msg, buffer_id));
  MessageUnregisterSharedMemory msg;
  EXPECT_EQ(imc::UNREGISTER_SHARED_MEMORY, msg.msg.message_id);
  EXPECT_EQ(8u, sizeof msg.msg);
  MessageUnregisterSharedMemory msg2(2);
  EXPECT_EQ(imc::UNREGISTER_SHARED_MEMORY, msg2.msg.message_id);
  EXPECT_EQ(2, msg2.msg.buffer_id);
}

TEST_F(MessageCommandsTest, MessageUpdateTexture2DRect) {
  EXPECT_EQ(static_cast<int>(imc::UPDATE_TEXTURE2D_RECT), 6);
  EXPECT_EQ(0u, offsetof(MessageUpdateTexture2DRect::Msg, message_id));
  EXPECT_EQ(4u, offsetof(MessageUpdateTexture2DRect::Msg, texture_id));
  EXPECT_EQ(8u, offsetof(MessageUpdateTexture2DRect::Msg, level));
  EXPECT_EQ(12u, offsetof(MessageUpdateTexture2DRect::Msg, x));
  EXPECT_EQ(16u, offsetof(MessageUpdateTexture2DRect::Msg, y));
  EXPECT_EQ(20u, offsetof(MessageUpdateTexture2DRect::Msg, width));
  EXPECT_EQ(24u, offsetof(MessageUpdateTexture2DRect::Msg, height));
  EXPECT_EQ(28u, offsetof(MessageUpdateTexture2DRect::Msg, shared_memory_id));
  EXPECT_EQ(32u, offsetof(MessageUpdateTexture2DRect::Msg, offset));
  EXPECT_EQ(36u, offsetof(MessageUpdateTexture2DRect::Msg, pitch));
  MessageUpdateTexture2DRect msg;
  EXPECT_EQ(imc::UPDATE_TEXTURE2D_RECT, msg.msg.message_id);
  EXPECT_EQ(40u, sizeof msg.msg);
  MessageUpdateTexture2DRect msg2(2, 3, 4, 5, 6, 7, 8, 9, 10);
  EXPECT_EQ(imc::UPDATE_TEXTURE2D_RECT, msg2.msg.message_id);
  EXPECT_EQ(2u, msg2.msg.texture_id);
  EXPECT_EQ(3, msg2.msg.level);
  EXPECT_EQ(4, msg2.msg.x);
  EXPECT_EQ(5, msg2.msg.y);
  EXPECT_EQ(6, msg2.msg.width);
  EXPECT_EQ(7, msg2.msg.height);
  EXPECT_EQ(8, msg2.msg.shared_memory_id);
  EXPECT_EQ(9, msg2.msg.offset);
  EXPECT_EQ(10, msg2.msg.pitch);
}

TEST_F(MessageCommandsTest, MessageRender) {
  EXPECT_EQ(static_cast<int>(imc::RENDER), 7);
  EXPECT_EQ(0u, offsetof(MessageRender::Msg, message_id));
  MessageRender msg;
  EXPECT_EQ(imc::RENDER, msg.msg.message_id);
  EXPECT_EQ(4u, sizeof msg.msg);
}

TEST_F(MessageCommandsTest, MessageGetVersion) {
  EXPECT_EQ(static_cast<int>(imc::GET_VERSION), 8);
  EXPECT_EQ(0u, offsetof(MessageGetVersion::Msg, message_id));
  MessageGetVersion msg;
  EXPECT_EQ(imc::GET_VERSION, msg.msg.message_id);
  EXPECT_EQ(4u, sizeof msg.msg);
  const char* kVersion = "0.1.2.3";
  MessageGetVersion::Response response(kVersion);
  EXPECT_EQ(0u, offsetof(MessageGetVersion::ResponseData, version));
  EXPECT_EQ(128u, sizeof(MessageGetVersion::ResponseData));  // NOLINT
  EXPECT_STREQ(kVersion, response.data.version);
}


}  // namespace o3d


