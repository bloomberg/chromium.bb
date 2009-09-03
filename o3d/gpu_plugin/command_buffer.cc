// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/command_buffer.h"

namespace o3d {
namespace gpu_plugin {

CommandBuffer::CommandBuffer(NPP npp) : DispatchedNPObject(npp) {
}

CommandBuffer::~CommandBuffer() {
  if (shared_memory_) {
    NPBrowser::get()->UnmapSharedMemory(npp(), shared_memory_);
  }
}

bool CommandBuffer::Initialize(int32 size) {
  if (buffer_object_.Get())
    return false;

  NPObjectPointer<NPObject> window = NPObjectPointer<NPObject>::FromReturned(
      NPBrowser::get()->GetWindowNPObject(npp()));
  if (!window.Get())
    return false;

  NPObjectPointer<NPObject> chromium;
  if (!NPGetProperty(npp(), window, "chromium", &chromium)) {
    return false;
  }

  NPObjectPointer<NPObject> system;
  if (!NPGetProperty(npp(), chromium, "system", &system)) {
    return false;
  }

  if (!NPInvoke(npp(), system, "createSharedMemory", size,
                &buffer_object_)) {
    return false;
  }

  shared_memory_ = NPBrowser::get()->MapSharedMemory(
      npp(), buffer_object_.Get(), size, false);
  if (!shared_memory_) {
    buffer_object_ = NPObjectPointer<NPObject>();
    return false;
  }

  return true;
}

NPObjectPointer<NPObject> CommandBuffer::GetBuffer() {
  return buffer_object_;
}

void CommandBuffer::SetPutOffset(int32 offset) {
}

int32 CommandBuffer::GetGetOffset() {
  return 0;
}

}  // namespace gpu_plugin
}  // namespace o3d
