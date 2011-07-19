// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header file is used only differ_block.h. It defines the SSE2 rountines
// for finding block difference.

#ifndef REMOTING_HOST_DIFFER_BLOCK_INTERNAL_H_
#define REMOTING_HOST_DIFFER_BLOCK_INTERNAL_H_

#include "base/basictypes.h"

namespace remoting {

// Find block difference of dimension 16x16.
extern int BlockDifference_SSE2_W16(const uint8* image1, const uint8* image2,
                                    int stride);

// Find block difference of dimension 32x32.
extern int BlockDifference_SSE2_W32(const uint8* image1, const uint8* image2,
                                    int stride);

}  // namespace remoting

#endif  // REMOTING_HOST_DIFFER_BLOCK_INTERNAL_H_
