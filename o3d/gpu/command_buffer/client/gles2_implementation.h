// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H

#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/id_allocator.h"

namespace command_buffer {
namespace gles2 {

// A class to help with shared memory.
class SharedMemoryHelper {
 public:
  SharedMemoryHelper(
      unsigned int id,
      void* address)
      : id_(id),
        address_(address) {
  }

  unsigned int GetOffset(void* address) const {
    return static_cast<int8*>(address) -
           static_cast<int8*>(address_);
  }

  void* GetAddress(unsigned int offset) const {
    return static_cast<int8*>(address_) + offset;
  }

  template <typename T>
  T GetAddressAs(unsigned int offset) const {
    return static_cast<T>(GetAddress(offset));
  }

  unsigned int GetId() const {
    return id_;
  }

 private:
  ResourceId id_;
  void* address_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemoryHelper);
};

// This class emulates GLES2 over command buffers. It can be used by a client
// program so that the program does not need deal with shared memory and command
// buffer management. See gl2_lib.h.  Note that there is a performance gain to
// be had by changing your code to use command buffers directly by using the
// GLES2CmdHelper but that entails changing your code to use and deal with
// shared memory and synchronization issues.
class GLES2Implementation {
 public:
  GLES2Implementation(
      GLES2CmdHelper* helper,
      ResourceId shared_memory_id,
      void* shared_memory);

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gpu/command_buffer/client/gles2_implementation_autogen.h"

 private:
  // Makes a set of Ids for glGen___ functions.
  void MakeIds(GLsizei n, GLuint* ids);

  // Frees a set of Ids for glDelete___ functions.
  void FreeIds(GLsizei n, const GLuint* ids);

  GLES2Util util_;
  GLES2CmdHelper* helper_;
  IdAllocator id_allocator_;
  SharedMemoryHelper shared_memory_;

  // pack alignment as last set by glPixelStorei
  GLint pack_alignment_;

  // unpack alignment as last set by glPixelStorei
  GLint unpack_alignment_;

  DISALLOW_COPY_AND_ASSIGN(GLES2Implementation);
};


}  // namespace gles2
}  // namespace command_buffer

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H

