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

#include <cstdint>

#include "gav1/frame_buffer.h"

// All the declarations in this file are part of the public ABI.

namespace libgav1 {

// Applications must populate this structure before creating a decoder instance.
struct DecoderSettings {
  // Number of threads to use when decoding. Must be greater than 0. The
  // library will create at most |threads|-1 new threads, the calling thread is
  // considered part of the library's thread count. Defaults to 1 (no new
  // threads will be created).
  int threads = 1;
  // Do frame parallel decoding.
  //
  // NOTE: Frame parallel decoding is not implemented. Don't set frame_parallel
  // to true.
  bool frame_parallel = false;
  // Get frame buffer callback.
  GetFrameBufferCallback get = nullptr;
  // Release frame buffer callback.
  ReleaseFrameBufferCallback release = nullptr;
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
#endif  // LIBGAV1_SRC_GAV1_DECODER_SETTINGS_H_
