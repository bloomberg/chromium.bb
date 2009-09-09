// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/command_buffer.h"

namespace o3d {
namespace gpu_plugin {

CommandBuffer::CommandBuffer(NPP npp) : npp_(npp) {
}

CommandBuffer::~CommandBuffer() {
}

bool CommandBuffer::Initialize(int32 size) {
  if (shared_memory_.Get())
    return false;

  NPObjectPointer<NPObject> window = NPObjectPointer<NPObject>::FromReturned(
      NPBrowser::get()->GetWindowNPObject(npp_));
  if (!window.Get())
    return false;

  NPObjectPointer<NPObject> chromium;
  if (!NPGetProperty(npp_, window, "chromium", &chromium)) {
    return false;
  }

  NPObjectPointer<NPObject> system;
  if (!NPGetProperty(npp_, chromium, "system", &system)) {
    return false;
  }

  NPObjectPointer<NPObject> result;
  if (!NPInvoke(npp_, system, "createSharedMemory", size,
                &result)) {
    return false;
  }

  // TODO(spatrick): validate NPClass before assuming a CHRSHaredMemory is
  //    returned.
  shared_memory_ = NPObjectPointer<CHRSharedMemory>(
      static_cast<CHRSharedMemory*>(result.Get()));
  if (!shared_memory_.Get())
    return false;

  bool mapped;
  if (!NPInvoke(npp_, shared_memory_, "map", &mapped) || !mapped) {
    shared_memory_ = NPObjectPointer<CHRSharedMemory>();
    return false;
  }

  return true;
}

NPObjectPointer<NPObject> CommandBuffer::GetSharedMemory() {
  return shared_memory_;
}

void CommandBuffer::SetPutOffset(int32 offset) {
}

int32 CommandBuffer::GetGetOffset() {
  return 0;
}

}  // namespace gpu_plugin
}  // namespace o3d
