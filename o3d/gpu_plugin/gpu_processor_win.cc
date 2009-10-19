// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "o3d/gpu_plugin/gpu_processor.h"

namespace o3d {
namespace gpu_plugin {

GPUProcessor::GPUProcessor(NPP npp,
                           CommandBuffer* command_buffer)
    : npp_(npp),
      command_buffer_(command_buffer),
      commands_per_update_(100) {
  DCHECK(command_buffer);
  gapi_.reset(new command_buffer::GAPID3D9);
  decoder_.reset(new command_buffer::GAPIDecoder(gapi_.get()));
  decoder_->set_engine(this);
}

GPUProcessor::GPUProcessor(NPP npp,
                           CommandBuffer* command_buffer,
                           command_buffer::GAPID3D9* gapi,
                           command_buffer::GAPIDecoder* decoder,
                           command_buffer::CommandParser* parser,
                           int commands_per_update)
    : npp_(npp),
      command_buffer_(command_buffer),
      commands_per_update_(commands_per_update) {
  DCHECK(command_buffer);
  gapi_.reset(gapi);
  decoder_.reset(decoder);
  parser_.reset(parser);
}

bool GPUProcessor::Initialize(HWND handle) {
  // Cannot reinitialize.
  DCHECK(gapi_->hwnd() == NULL);

  // Map the ring buffer and create the parser.
  NPObjectPointer<NPObject> ring_buffer =
      command_buffer_->GetRingBuffer();

  if (ring_buffer.Get()) {
    size_t size;
    void* ptr = NPBrowser::get()->MapMemory(npp_,
                                            ring_buffer.Get(),
                                            &size);
    if (ptr == NULL) {
      return false;
    }

    parser_.reset(new command_buffer::CommandParser(ptr, size, 0, size, 0,
                                                    decoder_.get()));
  } else {
    parser_.reset(new command_buffer::CommandParser(NULL, 0, 0, 0, 0,
                                                    decoder_.get()));
  }

  // Initialize GAPI immediately if the window handle is valid.
  if (handle) {
    gapi_->set_hwnd(handle);
    return gapi_->Initialize();
  } else {
    return true;
  }
}

void GPUProcessor::Destroy() {
  // Destroy GAPI if window handle has not already become invalid.
  if (gapi_->hwnd()) {
    gapi_->Destroy();
    gapi_->set_hwnd(NULL);
  }
}

void GPUProcessor::SetWindow(HWND handle, int width, int height) {
  if (handle == NULL) {
    // Destroy GAPI when the window handle becomes invalid.
    Destroy();
  } else {
    if (handle != gapi_->hwnd()) {
      Initialize(handle);
    }
  }
}

}  // namespace gpu_plugin
}  // namespace o3d
