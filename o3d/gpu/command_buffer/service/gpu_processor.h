// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/cmd_parser.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/np_utils/np_object_pointer.h"

namespace command_buffer {

// This class processes commands in a command buffer. It is event driven and
// posts tasks to the current message loop to do additional work.
class GPUProcessor : public ::base::RefCounted<GPUProcessor>,
                     public command_buffer::CommandBufferEngine {
 public:
  GPUProcessor(NPP npp, CommandBuffer* command_buffer);

  // This constructor is for unit tests.
  GPUProcessor(CommandBuffer* command_buffer,
               gles2::GLES2Decoder* decoder,
               CommandParser* parser,
               int commands_per_update);

  virtual bool Initialize(HWND hwnd);

  virtual ~GPUProcessor();

  virtual void Destroy();

  virtual void ProcessCommands();

#if defined(OS_WIN)
  virtual bool SetWindow(HWND handle, int width, int height);
#endif

  // Implementation of CommandBufferEngine.

  // Gets the base address of a registered shared memory buffer.
  // Parameters:
  //   shm_id: the identifier for the shared memory buffer.
  virtual void *GetSharedMemoryAddress(int32 shm_id);

  // Gets the size of a registered shared memory buffer.
  // Parameters:
  //   shm_id: the identifier for the shared memory buffer.
  virtual size_t GetSharedMemorySize(int32 shm_id);

  // Sets the token value.
  virtual void set_token(int32 token);

 private:
  NPP npp_;

  // The GPUProcessor holds a weak reference to the CommandBuffer. The
  // CommandBuffer owns the GPUProcessor and holds a strong reference to it
  // through the ProcessCommands callback.
  CommandBuffer* command_buffer_;

  scoped_ptr< ::base::SharedMemory> mapped_ring_buffer_;
  int commands_per_update_;

  scoped_ptr<gles2::GLES2Decoder> decoder_;
  scoped_ptr<CommandParser> parser_;
};

}  // namespace command_buffer

// Callbacks to the GPUProcessor hold a reference count.
template <typename Method>
class CallbackStorage<command_buffer::GPUProcessor, Method> {
 public:
  CallbackStorage(command_buffer::GPUProcessor* obj, Method method)
      : obj_(obj),
        meth_(method) {
    DCHECK(obj_);
    obj_->AddRef();
  }

  ~CallbackStorage() {
    obj_->Release();
  }

 protected:
  command_buffer::GPUProcessor* obj_;
  Method meth_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CallbackStorage);
};

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_H_
