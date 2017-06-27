/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/platform/WebImage.h"

#include "platform/SharedBuffer.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebData.h"
#include "public/platform/WebSize.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static RefPtr<SharedBuffer> ReadFile(const char* file_name) {
  String file_path = testing::WebTestDataPath(file_name);

  return testing::ReadFromFile(file_path);
}

TEST(WebImageTest, PNGImage) {
  RefPtr<SharedBuffer> data = ReadFile("white-1x1.png");
  ASSERT_TRUE(data.Get());

  WebImage image = WebImage::FromData(WebData(data), WebSize());
  EXPECT_TRUE(image.Size() == WebSize(1, 1));
  EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255),
            image.GetSkBitmap().getColor(0, 0));
}

TEST(WebImageTest, ICOImage) {
  RefPtr<SharedBuffer> data = ReadFile("black-and-white.ico");
  ASSERT_TRUE(data.Get());

  WebVector<WebImage> images = WebImage::FramesFromData(WebData(data));
  ASSERT_EQ(2u, images.size());
  EXPECT_TRUE(images[0].Size() == WebSize(2, 2));
  EXPECT_TRUE(images[1].Size() == WebSize(1, 1));
  EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255),
            images[0].GetSkBitmap().getColor(0, 0));
  EXPECT_EQ(SkColorSetARGB(255, 0, 0, 0),
            images[1].GetSkBitmap().getColor(0, 0));
}

TEST(WebImageTest, ICOValidHeaderMissingBitmap) {
  RefPtr<SharedBuffer> data = ReadFile("valid_header_missing_bitmap.ico");
  ASSERT_TRUE(data.Get());

  WebVector<WebImage> images = WebImage::FramesFromData(WebData(data));
  ASSERT_TRUE(images.IsEmpty());
}

TEST(WebImageTest, BadImage) {
  const char kBadImage[] = "hello world";
  WebVector<WebImage> images = WebImage::FramesFromData(WebData(kBadImage));
  ASSERT_EQ(0u, images.size());

  WebImage image = WebImage::FromData(WebData(kBadImage), WebSize());
  EXPECT_TRUE(image.GetSkBitmap().empty());
  EXPECT_TRUE(image.GetSkBitmap().isNull());
}

}  // namespace blink
