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
      put_offset_(0),
      token_(0),
      error_(ERROR_NO_ERROR) {
  // Element zero is always NULL.
  registered_objects_.push_back(NPObjectPointer<NPObject>());
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

  // TODO(spatrick): validate NPClass before assuming a CHRSharedMemory is
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

int32 CommandBuffer::RegisterObject(NPObjectPointer<NPObject> object) {
  if (!object.Get())
    return 0;

  if (unused_registered_object_elements_.empty()) {
    // Check we haven't exceeded the range that fits in a 32-bit integer.
    int32 handle = static_cast<int32>(registered_objects_.size());
    if (handle != registered_objects_.size())
      return -1;

    registered_objects_.push_back(object);
    return handle;
  }

  int32 handle = *unused_registered_object_elements_.begin();
  unused_registered_object_elements_.erase(
      unused_registered_object_elements_.begin());
  DCHECK(!registered_objects_[handle].Get());
  registered_objects_[handle] = object;
  return handle;
}

void CommandBuffer::UnregisterObject(NPObjectPointer<NPObject> object,
                                     int32 handle) {
  if (handle <= 0)
    return;

  if (static_cast<size_t>(handle) >= registered_objects_.size())
    return;

  if (registered_objects_[handle] != object)
    return;

  registered_objects_[handle] = NPObjectPointer<NPObject>();
  unused_registered_object_elements_.insert(handle);

  // Remove all null objects from the end of the vector. This allows the vector
  // to shrink when, for example, all objects are unregistered. Note that this
  // loop never removes element zero, which is always NULL.
  while (registered_objects_.size() > 1 && !registered_objects_.back().Get()) {
    registered_objects_.pop_back();
    unused_registered_object_elements_.erase(
        static_cast<int32>(registered_objects_.size()));
  }
}

NPObjectPointer<NPObject> CommandBuffer::GetRegisteredObject(int32 handle) {
  DCHECK_GE(handle, 0);
  DCHECK_LT(static_cast<size_t>(handle), registered_objects_.size());

  return registered_objects_[handle];
}

int32 CommandBuffer::ResetError() {
  int32 last_error = error_;
  error_ = ERROR_NO_ERROR;
  return last_error;
}

}  // namespace gpu_plugin
}  // namespace o3d
