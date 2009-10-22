// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_GPU_PROCESSOR_MOCK_H_
#define O3D_GPU_PLUGIN_GPU_PROCESSOR_MOCK_H_

#include "o3d/gpu_plugin/gpu_processor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace o3d {
namespace gpu_plugin {

class MockGPUProcessor : public GPUProcessor {
 public:
  MockGPUProcessor(NPP npp,
                   CommandBuffer* command_buffer)
      : GPUProcessor(npp, command_buffer) {
  }

#if defined(OS_WIN)
  MOCK_METHOD1(Initialize, bool(HWND handle));
#endif

  MOCK_METHOD0(Destroy, void());
  MOCK_METHOD0(ProcessCommands, void());

#if defined(OS_WIN)
  MOCK_METHOD3(SetWindow, bool(HWND handle, int width, int height));
#endif

  MOCK_METHOD1(GetSharedMemoryAddress, void*(int32 shm_id));
  MOCK_METHOD1(GetSharedMemorySize, size_t(int32 shm_id));
  MOCK_METHOD1(set_token, void(int32 token));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGPUProcessor);
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_GPU_PROCESSOR_MOCK_H_
