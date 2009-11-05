// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/np_utils/np_class.h"
#include "o3d/gpu_plugin/system_services/shared_memory.h"

namespace gpu_plugin {

SharedMemory::SharedMemory(NPP npp)
    : npp_(npp),
      shared_memory_(NULL) {
}

SharedMemory::~SharedMemory() {
}

bool SharedMemory::Initialize(int32 size) {
  if (size < 0)
    return false;

  if (shared_memory_.get())
    return false;

  shared_memory_.reset(new base::SharedMemory());
  if (!shared_memory_->Create(std::wstring(), false, false, size)) {
    shared_memory_.reset();
    return false;
  }

  return true;
}

int32 SharedMemory::GetSize() {
  return shared_memory_.get() ? shared_memory_->max_size() : 0;
}

}  // namespace gpu_plugin

void* NPN_MapMemory(NPP npp, NPObject* object, size_t* size) {
  *size = 0;

  // Check that the object really is shared memory.
  if (object->_class !=
      gpu_plugin::NPGetClass<gpu_plugin::SharedMemory>()) {
    return NULL;
  }

  gpu_plugin::SharedMemory* shared_memory_object =
      static_cast<gpu_plugin::SharedMemory*>(object);
  base::SharedMemory* shared_memory =
      shared_memory_object->shared_memory();
  if (!shared_memory) {
    return NULL;
  }

  if (!shared_memory->memory()) {
    shared_memory->Map(shared_memory->max_size());
  }

  *size = shared_memory->max_size();
  return shared_memory->memory();
}
