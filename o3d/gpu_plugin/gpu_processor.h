// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_GPU_PROCESSOR_H_
#define O3D_GPU_PLUGIN_GPU_PROCESSOR_H_

#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "o3d/command_buffer/service/cross/cmd_buffer_engine.h"
#include "o3d/command_buffer/service/cross/cmd_parser.h"
#include "o3d/command_buffer/service/cross/gapi_decoder.h"
#include "o3d/gpu_plugin/command_buffer.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"

#if defined(OS_WIN)
#include "o3d/command_buffer/service/win/d3d9/gapi_d3d9.h"
#endif

namespace o3d {
namespace gpu_plugin {

// This class processes commands in a command buffer. It is event driven and
// posts tasks to the current message loop to do additional work.
class GPUProcessor : public base::RefCountedThreadSafe<GPUProcessor>,
                     public command_buffer::CommandBufferUpcallInterface {
 public:
  GPUProcessor(NPP npp,
               const NPObjectPointer<CommandBuffer>& command_buffer);

#if defined(OS_WIN)
  // This constructor is for unit tests.
  GPUProcessor(NPP npp,
               const NPObjectPointer<CommandBuffer>& command_buffer,
               command_buffer::GAPID3D9* gapi,
               command_buffer::GAPIDecoder* decoder,
               command_buffer::CommandParser* parser,
               int commands_per_update);

  bool Initialize(HWND hwnd);
#endif  // OS_WIN

  void Destroy();

  void ProcessCommands();

#if defined(OS_WIN)
  void SetWindow(HWND handle, int width, int height);
#endif

  // Implementation of CommandBufferUpcallInterface.

  // Gets the base address of a registered shared memory buffer.
  // Parameters:
  //   shm_id: the identifier for the shared memory buffer.
  virtual void *GetSharedMemoryAddress(unsigned int shm_id);

  // Gets the size of a registered shared memory buffer.
  // Parameters:
  //   shm_id: the identifier for the shared memory buffer.
  virtual size_t GetSharedMemorySize(unsigned int shm_id);

  // Sets the token value.
  virtual void set_token(unsigned int token);

 private:
  NPP npp_;
  NPObjectPointer<CommandBuffer> command_buffer_;
  scoped_ptr<base::SharedMemory> mapped_ring_buffer_;
  int commands_per_update_;

#if defined(OS_WIN)
  scoped_ptr<command_buffer::GAPID3D9> gapi_;
  scoped_ptr<command_buffer::GAPIDecoder> decoder_;
  scoped_ptr<command_buffer::CommandParser> parser_;
#endif
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_GPU_PROCESSOR_H_
