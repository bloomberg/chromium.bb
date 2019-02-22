// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_MEMORY_BUFFER_MANAGER_SINGLETON_H_
#define CONTENT_BROWSER_GPU_GPU_MEMORY_BUFFER_MANAGER_SINGLETON_H_

#include "components/viz/host/host_gpu_memory_buffer_manager.h"

namespace content {

// This class ensures that there is at most one instance of
// |viz::HostGpuMemoryBufferManager| in content at any given time. Code in
// content must use this class to access the instance.
class GpuMemoryBufferManagerSingleton : public viz::HostGpuMemoryBufferManager {
 public:
  explicit GpuMemoryBufferManagerSingleton(int client_id);
  ~GpuMemoryBufferManagerSingleton() override;

  static GpuMemoryBufferManagerSingleton* GetInstance();

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferManagerSingleton);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_MEMORY_BUFFER_MANAGER_SINGLETON_H_
