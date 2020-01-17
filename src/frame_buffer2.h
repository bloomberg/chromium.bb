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

#ifndef LIBGAV1_SRC_FRAME_BUFFER2_H_
#define LIBGAV1_SRC_FRAME_BUFFER2_H_

// TODO(wtc): This header file is now an internal header while we experiment
// with the new frame buffer callback API. When the new frame buffer callback
// API is stable, replace src/gav1/frame_buffer.h with this header.

// All the declarations in this file are part of the public ABI. This file may
// be included by both C and C++ files.

#include <stdint.h>

#include "src/gav1/decoder_buffer.h"

// The callback functions use the C linkage conventions.
#if defined(__cplusplus)
extern "C" {
#endif

// This structure represents an allocated frame buffer.
struct Libgav1FrameBuffer2 {
  // In the |plane| and |stride| arrays, the elements at indexes 0, 1, and 2
  // are for the Y, U, and V planes, respectively.
  uint8_t* plane[3];   // Pointers to the plane buffers.
  int stride[3];       // Row strides in bytes.
  void* private_data;  // Frame buffer's private data. Available for use by the
                       // release frame buffer callback. Also copied to the
                       // |buffer_private_data| field of DecoderBuffer for use
                       // by the consumer of a DecoderBuffer.
};

// This callback is invoked by the decoder to provide information on the
// subsequent frames in the video, until the next invocation of this callback
// or the end of the video.
//
// |width| and |height| are the maximum frame width and height in pixels.
// |left_border|, |right_border|, |top_border|, and |bottom_border| are the
// maximum left, right, top, and bottom border sizes in pixels.
// |stride_alignment| specifies the alignment of the row stride in bytes.
//
// NOTE: The callback may ignore the information if it is not useful.
typedef int (*Libgav1FrameBufferSizeChangedCallback)(
    void* callback_private_data, int bitdepth, Libgav1ImageFormat image_format,
    int width, int height, int left_border, int right_border, int top_border,
    int bottom_border, int stride_alignment);

// This callback is invoked by the decoder to allocate a frame buffer, which
// consists of three data buffers, for the Y, U, and V planes, respectively.
//
// The callback must set |frame_buffer->plane[i]| to point to the data buffers
// of the planes, and set |frame_buffer->stride[i]| to the row strides of the
// planes. If |image_format| is kLibgav1ImageFormatMonochrome400, the callback
// should set |frame_buffer->plane[1]| and |frame_buffer->plane[2]| to a null
// pointer and set |frame_buffer->stride[1]| and |frame_buffer->stride[2]| to
// 0. The callback may set |frame_buffer->private_data| to a value that will
// be useful to the release frame buffer callback and the consumer of a
// DecoderBuffer.
//
// Returns 0 on success, -1 on failure.

// |width| and |height| are the frame width and height in pixels.
// |left_border|, |right_border|, |top_border|, and |bottom_border| are the
// left, right, top, and bottom border sizes in pixels. |stride_alignment|
// specifies the alignment of the row stride in bytes.
typedef int (*Libgav1GetFrameBufferCallback2)(
    void* callback_private_data, int bitdepth, Libgav1ImageFormat image_format,
    int width, int height, int left_border, int right_border, int top_border,
    int bottom_border, int stride_alignment, Libgav1FrameBuffer2* frame_buffer);

// After a frame buffer is allocated, the decoder starts to write decoded video
// to the frame buffer. When the frame buffer is ready for consumption, it is
// made available to the application in a Decoder::DequeueFrame() call.
// Afterwards, the decoder may continue to use the frame buffer in read-only
// mode. When the decoder is finished using the frame buffer, it notifies the
// application by calling the Libgav1ReleaseFrameBufferCallback.

// This callback is invoked by the decoder to release a frame buffer.
//
// Returns 0 on success, -1 on failure.
typedef int (*Libgav1ReleaseFrameBufferCallback2)(void* callback_private_data,
                                                  void* buffer_private_data);

#if defined(__cplusplus)
}  // extern "C"

// Declare type aliases for C++.
namespace libgav1 {

using FrameBuffer2 = Libgav1FrameBuffer2;
using FrameBufferSizeChangedCallback = Libgav1FrameBufferSizeChangedCallback;
using GetFrameBufferCallback2 = Libgav1GetFrameBufferCallback2;
using ReleaseFrameBufferCallback2 = Libgav1ReleaseFrameBufferCallback2;

}  // namespace libgav1
#endif  // defined(__cplusplus)

#endif  // LIBGAV1_SRC_FRAME_BUFFER2_H_
