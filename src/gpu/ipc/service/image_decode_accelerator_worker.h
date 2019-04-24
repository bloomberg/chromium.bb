// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_
#define GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace gpu {

// An ImageDecodeAcceleratorWorker handles the actual hardware-accelerated
// decode of an image of a specific type (e.g., JPEG, WebP, etc.).
class ImageDecodeAcceleratorWorker {
 public:
  virtual ~ImageDecodeAcceleratorWorker() {}

  using CompletedDecodeCB =
      base::OnceCallback<void(std::vector<uint8_t> /* output */,
                              size_t /* row_bytes */,
                              SkImageInfo /* image_info */)>;

  // Enqueue a decode of |encoded_data|. The |decode_cb| is called
  // asynchronously when the decode completes passing as parameters a vector
  // containing the decoded image (|output|), the stride (|row_bytes|), and a
  // SkImageInfo (|image_info|) with information about the decoded output.
  // For a successful decode, implementations must guarantee that:
  //
  // 1) |image_info|.width() == |output_size|.width().
  // 2) |image_info|.height() == |output_size|.height().
  // 3) |row_bytes| >= |image_info|.minRowBytes().
  // 4) |output|.size() == |image_info|.computeByteSize(|row_bytes|).
  //
  // If the decode fails, |decode_cb| is called asynchronously with an empty
  // vector. Callbacks should be called in the order that this method is called.
  virtual void Decode(std::vector<uint8_t> encoded_data,
                      const gfx::Size& output_size,
                      CompletedDecodeCB decode_cb) = 0;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_
