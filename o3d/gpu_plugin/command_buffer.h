// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_COMMAND_BUFFER_H_
#define O3D_GPU_PLUGIN_COMMAND_BUFFER_H_

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "o3d/gpu_plugin/np_utils/default_np_object.h"
#include "o3d/gpu_plugin/np_utils/np_dispatcher.h"
#include "o3d/gpu_plugin/system_services/shared_memory_public.h"

namespace o3d {
namespace gpu_plugin {

// An NPObject that implements a shared memory command buffer and a synchronous
// API to manage the put and get pointers.
class CommandBuffer : public DefaultNPObject<NPObject> {
 public:
  explicit CommandBuffer(NPP npp);
  virtual ~CommandBuffer();

  // Create a shared memory buffer of the given size.
  virtual bool Initialize(int32 size);

  // Gets the shared memory ring buffer object for the command buffer.
  virtual NPObjectPointer<CHRSharedMemory> GetRingBuffer();

  virtual int32 GetSize();

  // The writer calls this to update its put offset. This function returns the
  // reader's most recent get offset. Does not return until after the put offset
  // change callback has been invoked. Returns -1 if the put offset is invalid.
  virtual int32 SyncOffsets(int32 put_offset);

  // Returns the current get offset. This can be called from any thread.
  virtual int32 GetGetOffset();

  // Sets the current get offset. This can be called from any thread.
  virtual void SetGetOffset(int32 get_offset);

  // Returns the current put offset. This can be called from any thread.
  virtual int32 GetPutOffset();

  // Sets a callback that should be posted on another thread whenever the put
  // offset is changed. The callback must not return until some progress has
  // been made (unless the command buffer is empty), i.e. the
  // get offset must have changed. It need not process the entire command
  // buffer though. This allows concurrency between the writer and the reader
  // while giving the writer a means of waiting for the reader to make some
  // progress before attempting to write more to the command buffer. Avoiding
  // the use of a synchronization primitive like a condition variable to
  // synchronize reader and writer reduces the risk of deadlock.
  // Takes ownership of callback. The callback is invoked on the plugin thread.
  virtual void SetPutOffsetChangeCallback(Callback0::Type* callback);
  
  NP_UTILS_BEGIN_DISPATCHER_CHAIN(CommandBuffer, DefaultNPObject<NPObject>)
    NP_UTILS_DISPATCHER(Initialize, bool(int32))
    NP_UTILS_DISPATCHER(GetRingBuffer, NPObjectPointer<CHRSharedMemory>())
    NP_UTILS_DISPATCHER(GetSize, int32())
    NP_UTILS_DISPATCHER(SyncOffsets, int32(int32))
    NP_UTILS_DISPATCHER(GetGetOffset, int32());
    NP_UTILS_DISPATCHER(GetPutOffset, int32());
  NP_UTILS_END_DISPATCHER_CHAIN

 private:
  NPP npp_;
  NPObjectPointer<CHRSharedMemory> shared_memory_;
  int32 size_;
  int32 get_offset_;
  int32 put_offset_;
  scoped_ptr<Callback0::Type> put_offset_change_callback_;
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_COMMAND_BUFFER_H_