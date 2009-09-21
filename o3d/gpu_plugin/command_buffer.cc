// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/command_buffer.h"

namespace o3d {
namespace gpu_plugin {

CommandBuffer::CommandBuffer(NPP npp)
    : npp_(npp),
      size_(0),
      get_offset_(0),
      put_offset_(0) {
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

  size_ = size;
  return true;
}

NPObjectPointer<CHRSharedMemory> CommandBuffer::GetRingBuffer() {
  return shared_memory_;
}

int32 CommandBuffer::GetSize() {
  return size_;
}

int32 CommandBuffer::SyncOffsets(int32 put_offset) {
  if (put_offset < 0 || put_offset >= size_)
    return -1;

  put_offset_ = put_offset;

  if (put_offset_change_callback_.get()) {
    put_offset_change_callback_->Run();
  }

  return get_offset_;
}

int32 CommandBuffer::GetGetOffset() {
  return get_offset_;
}

void CommandBuffer::SetGetOffset(int32 get_offset) {
  DCHECK(get_offset >= 0 && get_offset < size_);
  get_offset_ = get_offset;
}

int32 CommandBuffer::GetPutOffset() {
  return put_offset_;
}

void CommandBuffer::SetPutOffsetChangeCallback(Callback0::Type* callback) {
  put_offset_change_callback_.reset(callback);
}

}  // namespace gpu_plugin
}  // namespace o3d
