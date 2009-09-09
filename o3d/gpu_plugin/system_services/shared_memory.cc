// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/system_services/shared_memory.h"

namespace o3d {
namespace gpu_plugin {

SharedMemory::SharedMemory(NPP npp)
    : npp_(npp),
      shared_memory_(NULL) {
  handle = NULL;
  ptr = NULL;
  size = 0;
}

SharedMemory::~SharedMemory() {
  if (shared_memory_) {
    delete shared_memory_;
  }
}

void SharedMemory::Initialize(base::SharedMemory* shared_memory, int32 size) {
  DCHECK(shared_memory);
  shared_memory_ = shared_memory;
  this->handle = shared_memory->handle();
  this->size = size;
}

bool SharedMemory::Initialize(int32 size) {
  if (size < 0)
    return false;

  if (shared_memory_)
    return false;

  shared_memory_ = new base::SharedMemory();
  if (!shared_memory_->Create(std::wstring(), false, false, size)) {
    delete shared_memory_;
    shared_memory_ = NULL;
    return false;
  }

  handle = shared_memory_->handle();
  this->size = size;
  return true;
}

bool SharedMemory::Map() {
  if (!shared_memory_)
    return false;

  if (!shared_memory_->memory()) {
    if (!shared_memory_->Map(shared_memory_->max_size()))
      return false;

    ptr = shared_memory_->memory();
  }

  return true;
}

}  // namespace gpu_plugin
}  // namespace o3d
