// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "o3d/gpu_plugin/gpu_processor.h"

namespace o3d {
namespace gpu_plugin {

GPUProcessor::GPUProcessor(const NPObjectPointer<CommandBuffer>& command_buffer)
    : command_buffer_(command_buffer),
      window_handle_(NULL),
      window_width_(0),
      window_height_(0) {
}

void GPUProcessor::SetWindow(HWND handle, int width, int height) {
  window_handle_ = handle;
  window_width_ = width;
  window_height_ = height;
}

void GPUProcessor::DrawRectangle(uint32 color,
                                 int left, int top, int right, int bottom) {
  if (!window_handle_)
    return;

  HBRUSH brush = ::CreateSolidBrush(color);
  HDC dc = ::GetDC(window_handle_);
  RECT rect = { left, right, top, bottom };
  ::FillRect(dc, &rect, brush);
  ::ReleaseDC(window_handle_, dc);
  ::DeleteObject(brush);
}

}  // namespace gpu_plugin
}  // namespace o3d
