// Copyright 2020 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/frame_buffer_callback_adaptor.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <new>

#include "src/utils/common.h"
#include "src/utils/logging.h"

namespace libgav1 {

extern "C" int OnFrameBufferSizeChangedAdaptor(
    void* callback_private_data, int bitdepth, Libgav1ImageFormat image_format,
    int width, int height, int left_border, int right_border, int top_border,
    int bottom_border, int stride_alignment) {
  static_cast<void>(callback_private_data);
  static_cast<void>(bitdepth);
  static_cast<void>(image_format);
  static_cast<void>(width);
  static_cast<void>(height);
  static_cast<void>(left_border);
  static_cast<void>(right_border);
  static_cast<void>(top_border);
  static_cast<void>(bottom_border);
  static_cast<void>(stride_alignment);
  return 0;
}

// Size conventions:
// * Widths, heights, and border sizes are in pixels.
// * Strides and plane sizes are in bytes.
extern "C" int GetFrameBufferAdaptor(void* callback_private_data, int bitdepth,
                                     Libgav1ImageFormat image_format, int width,
                                     int height, int left_border,
                                     int right_border, int top_border,
                                     int bottom_border, int stride_alignment,
                                     Libgav1FrameBuffer2* frame_buffer2) {
  Libgav1FrameBufferInfo info;
  Libgav1StatusCode status = Libgav1ComputeFrameBufferInfo(
      bitdepth, image_format, width, height, left_border, right_border,
      top_border, bottom_border, stride_alignment, &info);
  if (status != kLibgav1StatusOk) return -1;

  std::unique_ptr<FrameBuffer> frame_buffer1(new (std::nothrow) FrameBuffer);
  if (frame_buffer1 == nullptr) return -1;

  auto* v1_callbacks =
      static_cast<V1FrameBufferCallbacks*>(callback_private_data);
  if (v1_callbacks->get(v1_callbacks->callback_private_data, info.y_buffer_size,
                        info.uv_buffer_size, frame_buffer1.get()) < 0) {
    return -1;
  }

  if (frame_buffer1->data[0] == nullptr ||
      frame_buffer1->size[0] < info.y_buffer_size ||
      (info.uv_buffer_size != 0 && frame_buffer1->data[1] == nullptr) ||
      frame_buffer1->size[1] < info.uv_buffer_size ||
      (info.uv_buffer_size != 0 && frame_buffer1->data[2] == nullptr) ||
      frame_buffer1->size[2] < info.uv_buffer_size) {
    assert(false && "The version 1 get_frame_buffer callback malfunctioned.");
    LIBGAV1_DLOG(ERROR,
                 "The version 1 get_frame_buffer callback malfunctioned.");
    return -1;
  }

  uint8_t* const y_buffer = frame_buffer1->data[0];
  uint8_t* const u_buffer =
      (info.uv_buffer_size != 0) ? frame_buffer1->data[1] : nullptr;
  uint8_t* const v_buffer =
      (info.uv_buffer_size != 0) ? frame_buffer1->data[2] : nullptr;

  status = Libgav1SetFrameBuffer(&info, y_buffer, u_buffer, v_buffer,
                                 frame_buffer1.release(), frame_buffer2);
  return (status == kLibgav1StatusOk) ? 0 : -1;
}

extern "C" void ReleaseFrameBufferAdaptor(void* callback_private_data,
                                          void* buffer_private_data) {
  auto* v1_callbacks =
      static_cast<V1FrameBufferCallbacks*>(callback_private_data);
  auto* frame_buffer1 = static_cast<FrameBuffer*>(buffer_private_data);
  assert(frame_buffer1->data[0] != nullptr);
  const int rv =
      v1_callbacks->release(v1_callbacks->callback_private_data, frame_buffer1);
  static_cast<void>(rv);
  assert(rv == 0);
  delete frame_buffer1;
}

}  // namespace libgav1
