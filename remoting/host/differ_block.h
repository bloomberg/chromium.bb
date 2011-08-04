// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DIFFER_BLOCK_H_
#define REMOTING_HOST_DIFFER_BLOCK_H_

#include "base/basictypes.h"

namespace remoting {

// Size (in pixels) of each square block used for diffing.
// This must be a multiple of sizeof(uint64)/8.
static const int kBlockSize = 32;

// Format: BGRA 32 bit.
static const int kBytesPerPixel = 4;

// Low level functions to compare 2 blocks of pixels.
//   zero means the blocks are identical.
//   one means  the blocks are different.
int BlockDifference(const uint8* image1, const uint8* image2, int stride);

}  // namespace remoting

#endif  // REMOTING_HOST_DIFFER_BLOCK_H_
