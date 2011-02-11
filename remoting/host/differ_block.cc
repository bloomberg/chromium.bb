// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/differ_block.h"

#include "build/build_config.h"
#include "media/base/cpu_features.h"
#include "remoting/host/differ_block_internal.h"

namespace remoting {

int BlockDifference_C(const uint8* image1, const uint8* image2, int stride) {
  // Number of uint64s in each row of the block.
  // This must be an integral number.
  int int64s_per_row = (kBlockWidth * kBytesPerPixel) / sizeof(uint64);

  for (int y = 0; y < kBlockHeight; y++) {
    const uint64* prev = reinterpret_cast<const uint64*>(image1);
    const uint64* curr = reinterpret_cast<const uint64*>(image2);

    // Check each row in uint64-sized chunks.
    // Note that this check may straddle multiple pixels. This is OK because
    // we're interested in identifying whether or not there was change - we
    // don't care what the actual change is.
    for (int x = 0; x < int64s_per_row; x++) {
      if (*prev++ != *curr++) {
        return 1;
      }
    }
    image1 += stride;
    image2 += stride;
  }
  return 0;
}

int BlockDifference(const uint8* image1, const uint8* image2, int stride) {
  static int (*diff_proc)(const uint8*, const uint8*, int) = NULL;

  if (!diff_proc) {
#if defined(ARCH_CPU_ARM_FAMILY)
    // For ARM processors, always use C version.
    // TODO(hclam): Implement a NEON version.
    diff_proc = &BlockDifference_C;
#else
    // For x86 processors, check if SSE2 is supported.
    if (media::hasSSE2() && kBlockWidth == 32)
      diff_proc = &BlockDifference_SSE2_W32;
    else if (media::hasSSE2() && kBlockWidth == 16)
      diff_proc = &BlockDifference_SSE2_W16;
    else
      diff_proc = &BlockDifference_C;
#endif
  }

  return diff_proc(image1, image2, stride);
}

}  // namespace remoting
