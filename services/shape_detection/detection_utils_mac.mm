// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/detection_utils_mac.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/shared_memory.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/shape_detection/barcode_detection_impl.h"

namespace shape_detection {

// These formats are available but not public until Mac 10.11.
#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11
const int kCIFormatRGBA8 = 24;
#else
//static_assert(kCIFormatRGBA8 == 24, "RGBA8 format enum index.");
#endif

base::scoped_nsobject<CIImage> CreateCIImageFromSharedMemory(
    mojo::ScopedSharedBufferHandle frame_data,
    uint32_t width,
    uint32_t height) {
  base::CheckedNumeric<uint32_t> num_pixels =
      base::CheckedNumeric<uint32_t>(width) * height;
  base::CheckedNumeric<uint32_t> num_bytes = num_pixels * 4;
  if (!num_bytes.IsValid()) {
    DLOG(ERROR) << "Data overflow";
    return base::scoped_nsobject<CIImage>();
  }

  base::SharedMemoryHandle memory_handle;
  size_t memory_size = 0;
  bool read_only_flag = false;
  const MojoResult result = mojo::UnwrapSharedMemoryHandle(
      std::move(frame_data), &memory_handle, &memory_size, &read_only_flag);
  DCHECK_EQ(MOJO_RESULT_OK, result) << "Failed to unwrap SharedBufferHandle";
  if (!memory_size || memory_size != num_bytes.ValueOrDie()) {
    DLOG(ERROR) << "Invalid image size";
    return base::scoped_nsobject<CIImage>();
  }

  auto shared_memory =
      base::MakeUnique<base::SharedMemory>(memory_handle, true /* read_only */);
  if (!shared_memory->Map(memory_size)) {
    DLOG(ERROR) << "Failed to map bytes from shared memory";
    return base::scoped_nsobject<CIImage>();
  }

  NSData* byte_data = [NSData dataWithBytesNoCopy:shared_memory->memory()
                                           length:num_bytes.ValueOrDie()
                                     freeWhenDone:NO];

  base::ScopedCFTypeRef<CGColorSpaceRef> colorspace(
      CGColorSpaceCreateWithName(kCGColorSpaceSRGB));

  // CIImage will return nil if RGBA8 is not supported in a certain version.
  base::scoped_nsobject<CIImage> ci_image([[CIImage alloc]
      initWithBitmapData:byte_data
             bytesPerRow:width * 4
                    size:CGSizeMake(width, height)
                  format:kCIFormatRGBA8
              colorSpace:colorspace]);
  if (!ci_image) {
    DLOG(ERROR) << "Failed to create CIImage";
    return base::scoped_nsobject<CIImage>();
  }
  return ci_image;
}

}  // namespace shape_detection
