// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_COMMAND_BUFFER_H_
#define O3D_GPU_PLUGIN_COMMAND_BUFFER_H_

#include "o3d/gpu_plugin/np_utils/dispatched_np_object.h"

namespace o3d {
namespace gpu_plugin {

// An NPObject that implements a shared memory command buffer and a synchronous
// API to manage the put and get pointers.
class CommandBuffer : public DispatchedNPObject {
 public:
  explicit CommandBuffer(NPP npp);
  virtual ~CommandBuffer();

  // Create a shared memory buffer of the given size.
  virtual bool Initialize(int32 size);

  // Gets the shared memory object for the command buffer.
  virtual NPObjectPointer<NPObject> GetBuffer();

  // The client calls this to update its put offset.
  virtual void SetPutOffset(int32 offset);

  // The client calls this to get the servers current get offset.
  virtual int32 GetGetOffset();

 protected:
  NP_UTILS_BEGIN_DISPATCHER_CHAIN(CommandBuffer, DispatchedNPObject)
    NP_UTILS_DISPATCHER(Initialize, bool(int32))
    NP_UTILS_DISPATCHER(SetPutOffset, void(int32))
    NP_UTILS_DISPATCHER(GetGetOffset, int32())
    NP_UTILS_DISPATCHER(GetBuffer, NPObjectPointer<NPObject>())
  NP_UTILS_END_DISPATCHER_CHAIN

 private:
  NPObjectPointer<NPObject> buffer_object_;
  NPSharedMemory* shared_memory_;
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_COMMAND_BUFFER_H_
