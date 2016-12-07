// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/chunk_stream.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

TEST(ChunkStreamTest, Simple) {
  ChunkStream stream;
  stream.Preallocate(1000);
  EXPECT_FALSE(stream.IsRangeAvailable(100, 500));
}

}  // namespace chrome_pdf
