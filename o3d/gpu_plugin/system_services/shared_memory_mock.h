// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_SYSTEM_SERVICES_SHARED_MEMORY_MOCK_H_
#define O3D_GPU_PLUGIN_SYSTEM_SERVICES_SHARED_MEMORY_MOCK_H_

#include "o3d/gpu_plugin/system_services/shared_memory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu_plugin {

class MockSharedMemory : public SharedMemory {
 public:
  explicit MockSharedMemory(NPP npp) : SharedMemory(npp) {
  }

  MOCK_METHOD1(Initialize, bool(int32 size));
  MOCK_METHOD0(GetSize, int32());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSharedMemory);
};

}  // namespace gpu_plugin

#endif  // O3D_GPU_PLUGIN_SYSTEM_SERVICES_SHARED_MEMORY_MOCK_H_
