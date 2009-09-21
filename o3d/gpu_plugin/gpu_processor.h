// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_GPU_PROCESSOR_H_
#define O3D_GPU_PLUGIN_GPU_PROCESSOR_H_

#include "base/ref_counted.h"
#include "o3d/gpu_plugin/command_buffer.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"

namespace o3d {
namespace gpu_plugin {

// This class processes commands in a command buffer. It is event driven and
// posts tasks to the current message loop to do additional work.
class GPUProcessor : public base::RefCountedThreadSafe<GPUProcessor> {
 public:
  explicit GPUProcessor(const NPObjectPointer<CommandBuffer>& command_buffer);

  void ProcessCommands();

#if defined(OS_WIN)
  void SetWindow(HWND handle, int width, int height);
#endif

  void DrawRectangle(uint32 color, int left, int top, int right, int bottom);

 private:
  NPObjectPointer<CommandBuffer> command_buffer_;

#if defined(OS_WIN)
  HWND window_handle_;
  int window_width_;
  int window_height_;
#endif
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_GPU_PROCESSOR_H_
