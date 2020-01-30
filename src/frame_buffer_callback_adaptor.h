/*
 * Copyright 2020 The libgav1 Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBGAV1_SRC_FRAME_BUFFER_CALLBACK_ADAPTOR_H_
#define LIBGAV1_SRC_FRAME_BUFFER_CALLBACK_ADAPTOR_H_

#include "src/gav1/frame_buffer.h"
#include "src/gav1/frame_buffer2.h"

namespace libgav1 {

// Adaptors for the version 1 frame buffer callbacks.
//
// How to use the callback adaptors:
// 1. Create a V1FrameBufferCallbacks struct and set its members to the version
//    1 frame buffer callbacks and their private data.
// 2. Pass a pointer to the V1FrameBufferCallbacks struct as the
//    |callback_private_data| argument to the callback adaptors.

struct V1FrameBufferCallbacks {
  V1FrameBufferCallbacks() = default;

  V1FrameBufferCallbacks(GetFrameBufferCallback get,
                         ReleaseFrameBufferCallback release,
                         void* callback_private_data)
      : get(get),
        release(release),
        callback_private_data(callback_private_data) {}

  // Frame buffer callbacks.
  GetFrameBufferCallback get;
  ReleaseFrameBufferCallback release;
  // Private data associated with the frame buffer callbacks.
  void* callback_private_data;
};

extern "C" int OnFrameBufferSizeChangedAdaptor(
    void* callback_private_data, int bitdepth, Libgav1ImageFormat image_format,
    int width, int height, int left_border, int right_border, int top_border,
    int bottom_border, int stride_alignment);

extern "C" int GetFrameBufferAdaptor(void* callback_private_data, int bitdepth,
                                     Libgav1ImageFormat image_format, int width,
                                     int height, int left_border,
                                     int right_border, int top_border,
                                     int bottom_border, int stride_alignment,
                                     Libgav1FrameBuffer2* frame_buffer2);

extern "C" void ReleaseFrameBufferAdaptor(void* callback_private_data,
                                          void* buffer_private_data);

}  // namespace libgav1

#endif  // LIBGAV1_SRC_FRAME_BUFFER_CALLBACK_ADAPTOR_H_
