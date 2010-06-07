// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DIFFER_BLOCK_H_
#define REMOTING_HOST_DIFFER_BLOCK_H_

#include "base/basictypes.h"

namespace remoting {

// Low level functions to return the difference between 2 blocks of pixels.
// The amount of difference is returned as an int.
//   zero means the blocks are identical.
//   larger values indicate larger changes.
// Pixel format of the captured screen may be platform specific, but constant.
// Size of block is constant.

int BlockDifference(const uint8* image1, const uint8* image2, int stride);

}  // namespace remoting

#endif  // REMOTING_HOST_DIFFER_BLOCK_H_
