// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_util.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

TEST(ImageUtilTest, PNGEncodeAndDecode) {
  gfx::Image original = gfx::test::CreateImage(100, 100);

  std::vector<unsigned char> encoded;
  ASSERT_TRUE(gfx::PNGEncodedDataFromImage(original, &encoded));

  scoped_ptr<gfx::Image> decoded(
      gfx::ImageFromPNGEncodedData(&encoded.front(), encoded.size()));
  ASSERT_TRUE(decoded.get());

  // PNG is lossless, so test that the decoded image matches the original.
  EXPECT_TRUE(gfx::test::IsEqual(original, *decoded));
}

TEST(ImageUtilTest, JPEGEncodeAndDecode) {
  gfx::Image original = gfx::test::CreateImage(100, 100);

  std::vector<unsigned char> encoded;
  ASSERT_TRUE(gfx::JPEGEncodedDataFromImage(original, 80, &encoded));

  gfx::Image decoded =
      gfx::ImageFromJPEGEncodedData(&encoded.front(), encoded.size());

  // JPEG is lossy, so simply check that the image decoded successfully.
  EXPECT_FALSE(decoded.IsEmpty());
}
