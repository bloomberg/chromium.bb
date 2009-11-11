// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_COMMAND_BUFFER_MOCK_H_
#define O3D_GPU_PLUGIN_COMMAND_BUFFER_MOCK_H_

#include "o3d/gpu_plugin/command_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu_plugin {

// An NPObject that implements a shared memory command buffer and a synchronous
// API to manage the put and get pointers.
class MockCommandBuffer : public CommandBuffer {
 public:
  explicit MockCommandBuffer(NPP npp) : CommandBuffer(npp) {
    ON_CALL(*this, GetRingBuffer())
      .WillByDefault(testing::Return(static_cast<::base::SharedMemory*>(NULL)));
    ON_CALL(*this, GetTransferBuffer(testing::_))
      .WillByDefault(testing::Return(static_cast<::base::SharedMemory*>(NULL)));
  }

  MOCK_METHOD1(Initialize, bool(::base::SharedMemory* ring_buffer));
  MOCK_METHOD0(GetRingBuffer, ::base::SharedMemory*());
  MOCK_METHOD0(GetSize, int32());
  MOCK_METHOD1(SyncOffsets, int32(int32 put_offset));
  MOCK_METHOD0(GetGetOffset, int32());
  MOCK_METHOD1(SetGetOffset, void(int32 get_offset));
  MOCK_METHOD0(GetPutOffset, int32());
  MOCK_METHOD1(SetPutOffsetChangeCallback, void(Callback0::Type* callback));
  MOCK_METHOD1(CreateTransferBuffer, int32(size_t size));
  MOCK_METHOD1(DestroyTransferBuffer, void(int32 handle));
  MOCK_METHOD1(GetTransferBuffer, ::base::SharedMemory*(int32 handle));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCommandBuffer);
};

}  // namespace gpu_plugin

#endif  // O3D_GPU_PLUGIN_COMMAND_BUFFER_MOCK_H_
