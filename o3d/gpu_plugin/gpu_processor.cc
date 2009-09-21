// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/gpu_processor.h"

namespace o3d {
namespace gpu_plugin {

// Placeholder command processing.
void GPUProcessor::ProcessCommands() {
  int32 get_offset = command_buffer_->GetGetOffset();
  int32 put_offset = command_buffer_->GetPutOffset();
  int32 size = command_buffer_->GetSize();
  if (size == 0)
    return;

  NPObjectPointer<CHRSharedMemory> shared_memory =
      command_buffer_->GetRingBuffer();
  if (!shared_memory.Get())
    return;
  if (!shared_memory->ptr)
    return;

  int8* ptr = static_cast<int8*>(shared_memory->ptr);

  int32 end_offset = (get_offset + sizeof(int32)) % size;
  if (get_offset > put_offset || end_offset <= put_offset) {
    int32 command = *reinterpret_cast<int32*>(ptr + get_offset);

    switch (command) {
      case 0:
        get_offset = end_offset;
        command_buffer_->SetGetOffset(get_offset);
        break;

      // Rectangle case is temporary and not tested.
      case 1: {
        end_offset += 20;
        if (end_offset >= size) {
          DCHECK(false);
          break;
        }

        if (get_offset <= put_offset && end_offset > put_offset) {
          DCHECK(false);
          break;
        }

        uint32 color = *reinterpret_cast<uint32*>(ptr + get_offset + 4);
        int32 left = *reinterpret_cast<int32*>(ptr + get_offset + 8);
        int32 top = *reinterpret_cast<int32*>(ptr + get_offset + 12);
        int32 right = *reinterpret_cast<int32*>(ptr + get_offset + 16);
        int32 bottom = *reinterpret_cast<int32*>(ptr + get_offset + 20);
        DrawRectangle(color, left, top, right, bottom);

        get_offset = end_offset;
        command_buffer_->SetGetOffset(get_offset);
        break;
      }
      default:
        DCHECK(false);
        break;
    }

  }

  // In practice, this will handle many more than one command before posting
  // the processing of the remainder to the message loop.
  if (get_offset != put_offset) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &GPUProcessor::ProcessCommands));
  }
}

}  // namespace gpu_plugin
}  // namespace o3d
