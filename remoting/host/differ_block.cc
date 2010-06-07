// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "differ_block.h"

#include <stdlib.h>

namespace remoting {

// TODO(fbarchard): Use common header for block size.
int kBlockWidth = 32;
int kBlockHeight = 32;
int kBytesPerPixel = 3;

int BlockDifference(const uint8* image1, const uint8* image2, int stride) {
  int diff = 0;
  for (int y = 0; y < kBlockHeight; ++y) {
    for (int x = 0; x < kBlockWidth * kBytesPerPixel; ++x) {
      diff += abs(image1[x] - image2[x]);
    }
    image1 += stride;
    image2 += stride;
  }
  return diff;
}

}  // namespace remoting
