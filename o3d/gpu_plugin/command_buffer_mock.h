// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_COMMAND_BUFFER_MOCK_H_
#define O3D_GPU_PLUGIN_COMMAND_BUFFER_MOCK_H_

#include "o3d/gpu_plugin/command_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace o3d {
namespace gpu_plugin {

// An NPObject that implements a shared memory command buffer and a synchronous
// API to manage the put and get pointers.
class MockCommandBuffer : public CommandBuffer {
 public:
  explicit MockCommandBuffer(NPP npp) : CommandBuffer(npp) {
  }

  MOCK_METHOD1(Initialize, bool(int32));
  MOCK_METHOD0(GetBuffer, NPObjectPointer<NPObject>());
  MOCK_METHOD1(SetPutOffset, void(int32));
  MOCK_METHOD0(GetGetOffset, int32());
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_COMMAND_BUFFER_MOCK_H_
