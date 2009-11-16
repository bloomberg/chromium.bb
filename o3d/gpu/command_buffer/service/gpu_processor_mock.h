// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_MOCK_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_MOCK_H_

#include "gpu/command_buffer/service/gpu_processor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace command_buffer {

class MockGPUProcessor : public GPUProcessor {
 public:
  explicit MockGPUProcessor(CommandBuffer* command_buffer)
      : GPUProcessor(NULL, command_buffer) {
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

}  // namespace command_buffer

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_MOCK_H_
