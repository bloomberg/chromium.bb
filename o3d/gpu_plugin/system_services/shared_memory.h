// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_SYSTEM_SERVICES_SHARED_MEMORY_H_
#define O3D_GPU_PLUGIN_SYSTEM_SERVICES_SHARED_MEMORY_H_

#include "base/shared_memory.h"
#include "o3d/gpu_plugin/np_utils/default_np_object.h"
#include "o3d/gpu_plugin/np_utils/np_dispatcher.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"
#include "o3d/gpu_plugin/system_services/shared_memory_public.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace o3d {
namespace gpu_plugin {

// An NPObject holding a shared memory handle.
class SharedMemory : public DefaultNPObject<CHRSharedMemory> {
 public:
  explicit SharedMemory(NPP npp);
  ~SharedMemory();

  // Initialize from an existing base::SharedMemory. Takes ownership of the
  // base::SharedMemory.
  void Initialize(base::SharedMemory* shared_memory, int32 size);

  virtual bool Initialize(int32 size);

  virtual int32 GetSize() {
    return size;
  }

  virtual bool Map();

  base::SharedMemory* shared_memory() const {
    return shared_memory_;
  }

  NP_UTILS_BEGIN_DISPATCHER_CHAIN(SharedMemory, DefaultNPObject<NPObject>)
    NP_UTILS_DISPATCHER(Initialize, bool(int32));
    NP_UTILS_DISPATCHER(GetSize, int32())
    NP_UTILS_DISPATCHER(Map, bool())
  NP_UTILS_END_DISPATCHER_CHAIN

 private:
  NPP npp_;
  base::SharedMemory* shared_memory_;
  DISALLOW_COPY_AND_ASSIGN(SharedMemory);
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_SYSTEM_SERVICES_SHARED_MEMORY_H_
