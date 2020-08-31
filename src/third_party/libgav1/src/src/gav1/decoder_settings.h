/*
 * Copyright 2019 The libgav1 Authors
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

#ifndef LIBGAV1_SRC_GAV1_DECODER_SETTINGS_H_
#define LIBGAV1_SRC_GAV1_DECODER_SETTINGS_H_

#if defined(__cplusplus)
#include <cstdint>
#else
#include <stdint.h>
#endif  // defined(__cplusplus)

#include "gav1/frame_buffer.h"
#include "gav1/symbol_visibility.h"

// All the declarations in this file are part of the public ABI.

#if defined(__cplusplus)
extern "C" {
#endif

// This callback is invoked by the decoder when it is done using an input frame
// buffer. When frame_parallel is set to true, this callback must not be
// nullptr. Otherwise, this callback is optional.
//
// |buffer_private_data| is the value passed in the EnqueueFrame() call.
typedef void (*Libgav1ReleaseInputBufferCallback)(void* callback_private_data,
                                                  void* buffer_private_data);

typedef struct Libgav1DecoderSettings {
  // Number of threads to use when decoding. Must be greater than 0. The
  // library will create at most |threads|-1 new threads, the calling thread is
  // considered part of the library's thread count. Defaults to 1 (no new
  // threads will be created).
  int threads;
  // A boolean. Do frame parallel decoding.
  //
  // NOTE: Frame parallel decoding is not implemented, this setting is
  // currently ignored.
  int frame_parallel;
  // A boolean. In frame parallel mode, should Libgav1DecoderDequeueFrame wait
  // until a enqueued frame is available for dequeueing.
  //
  // If frame_parallel is 0, this setting is ignored.
  int blocking_dequeue;
  // Called when the first sequence header or a sequence header with a
  // different frame size (which includes bitdepth, monochrome, subsampling_x,
  // subsampling_y, maximum frame width, or maximum frame height) is received.
  Libgav1FrameBufferSizeChangedCallback on_frame_buffer_size_changed;
  // Get frame buffer callback.
  Libgav1GetFrameBufferCallback get_frame_buffer;
  // Release frame buffer callback.
  Libgav1ReleaseFrameBufferCallback release_frame_buffer;
  // Release input frame buffer callback.
  Libgav1ReleaseInputBufferCallback release_input_buffer;
  // Passed as the private_data argument to the callbacks.
  void* callback_private_data;
  // Mask indicating the post processing filters that need to be applied to the
  // reconstructed frame. From LSB:
  //   Bit 0: Loop filter (deblocking filter).
  //   Bit 1: Cdef.
  //   Bit 2: SuperRes.
  //   Bit 3: Loop restoration.
  //   Bit 4: Film grain synthesis.
  //   All the bits other than the last 5 are ignored.
  uint8_t post_filter_mask;
} Libgav1DecoderSettings;

LIBGAV1_PUBLIC void Libgav1DecoderSettingsInitDefault(
    Libgav1DecoderSettings* settings);

#if defined(__cplusplus)
}  // extern "C"

namespace libgav1 {

using ReleaseInputBufferCallback = Libgav1ReleaseInputBufferCallback;

// Applications must populate this structure before creating a decoder instance.
struct DecoderSettings {
  // Number of threads to use when decoding. Must be greater than 0. The
  // library will create at most |threads|-1 new threads, the calling thread is
  // considered part of the library's thread count. Defaults to 1 (no new
  // threads will be created).
  int threads = 1;
  // Do frame parallel decoding.
  //
  // NOTE: Frame parallel decoding is not implemented, this setting is
  // currently ignored.
  bool frame_parallel = false;
  // In frame parallel mode, should DequeueFrame wait until a enqueued frame is
  // available for dequeueing.
  //
  // If frame_parallel is false, this setting is ignored.
  bool blocking_dequeue = false;
  // Called when the first sequence header or a sequence header with a
  // different frame size (which includes bitdepth, monochrome, subsampling_x,
  // subsampling_y, maximum frame width, or maximum frame height) is received.
  FrameBufferSizeChangedCallback on_frame_buffer_size_changed = nullptr;
  // Get frame buffer callback.
  GetFrameBufferCallback get_frame_buffer = nullptr;
  // Release frame buffer callback.
  ReleaseFrameBufferCallback release_frame_buffer = nullptr;
  // Release input frame buffer callback.
  ReleaseInputBufferCallback release_input_buffer = nullptr;
  // Passed as the private_data argument to the callbacks.
  void* callback_private_data = nullptr;
  // Mask indicating the post processing filters that need to be applied to the
  // reconstructed frame. From LSB:
  //   Bit 0: Loop filter (deblocking filter).
  //   Bit 1: Cdef.
  //   Bit 2: SuperRes.
  //   Bit 3: Loop restoration.
  //   Bit 4: Film grain synthesis.
  //   All the bits other than the last 5 are ignored.
  uint8_t post_filter_mask = 0x1f;
};

}  // namespace libgav1
#endif  // defined(__cplusplus)
#endif  // LIBGAV1_SRC_GAV1_DECODER_SETTINGS_H_
