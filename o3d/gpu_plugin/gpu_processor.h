// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_GPU_PROCESSOR_H_
#define O3D_GPU_PLUGIN_GPU_PROCESSOR_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "o3d/command_buffer/service/cross/cmd_buffer_engine.h"
#include "o3d/command_buffer/service/cross/cmd_parser.h"
#include "o3d/command_buffer/service/cross/gapi_decoder.h"
#include "o3d/gpu_plugin/command_buffer.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"

#if defined(CB_SERVICE_D3D9)
#include "o3d/command_buffer/service/win/d3d9/gapi_d3d9.h"
#elif defined(CB_SERVICE_GL)
#include "o3d/command_buffer/service/cross/gl/gapi_gl.h"
#else
#error command buffer service not defined
#endif

namespace gpu_plugin {

// This class processes commands in a command buffer. It is event driven and
// posts tasks to the current message loop to do additional work.
class GPUProcessor : public ::base::RefCounted<GPUProcessor>,
                     public command_buffer::CommandBufferEngine {
 public:
#if defined(CB_SERVICE_D3D9)
  typedef command_buffer::o3d::GAPID3D9 GPUGAPIInterface;
#elif defined(CB_SERVICE_GL)
  typedef command_buffer::o3d::GAPIGL GPUGAPIInterface;
#else
#error command buffer service not defined
#endif

  GPUProcessor(NPP npp,
               CommandBuffer* command_buffer);

  // This constructor is for unit tests.
  GPUProcessor(NPP npp,
               CommandBuffer* command_buffer,
               GPUGAPIInterface* gapi,
               command_buffer::o3d::GAPIDecoder* decoder,
               command_buffer::CommandParser* parser,
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

  scoped_ptr<GPUGAPIInterface> gapi_;
  scoped_ptr<command_buffer::o3d::GAPIDecoder> decoder_;
  scoped_ptr<command_buffer::CommandParser> parser_;
};

}  // namespace gpu_plugin

// Callbacks to the GPUProcessor hold a reference count.
template <typename Method>
class CallbackStorage<gpu_plugin::GPUProcessor, Method> {
 public:
  CallbackStorage(gpu_plugin::GPUProcessor* obj, Method method)
      : obj_(obj),
        meth_(method) {
    DCHECK(obj_);
    obj_->AddRef();
  }

  ~CallbackStorage() {
    obj_->Release();
  }

 protected:
  gpu_plugin::GPUProcessor* obj_;
  Method meth_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CallbackStorage);
};

#endif  // O3D_GPU_PLUGIN_GPU_PROCESSOR_H_
