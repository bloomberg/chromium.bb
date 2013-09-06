/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"

#include "core/platform/image-decoders/jpeg/JPEGImageDecoder.h"

#include "core/platform/SharedBuffer.h"
#include "public/platform/Platform.h"
#include "public/platform/WebData.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebUnitTestSupport.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/StringHasher.h"
#include "wtf/Vector.h"

#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKit;

namespace {

PassRefPtr<SharedBuffer> readFile(const char* fileName)
{
    String filePath = Platform::current()->unitTestSupport()->webKitRootDir();
    filePath.append(fileName);

    return Platform::current()->unitTestSupport()->readFromFile(filePath);
}

PassOwnPtr<JPEGImageDecoder> createDecoder(const IntSize& maxDecodedSize)
{
    return adoptPtr(new JPEGImageDecoder(ImageSource::AlphaNotPremultiplied, ImageSource::GammaAndColorProfileApplied, maxDecodedSize));
}

} // namespace

void downsample(unsigned width, unsigned height, unsigned* outputWidth, unsigned* outputHeight)
{
    RefPtr<SharedBuffer> data = readFile("/LayoutTests/fast/images/resources/lenna.jpg");
    ASSERT_TRUE(data.get());

    OwnPtr<JPEGImageDecoder> decoder = createDecoder(IntSize(width, height));
    decoder->setData(data.get(), true);

    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    ASSERT_TRUE(frame);
    *outputWidth = frame->getSkBitmap().width();
    *outputHeight = frame->getSkBitmap().height();
}

// Tests that a small size doesn't result in an empty image.
TEST(JPEGImageDecoderTest, downsample0)
{
    unsigned outputWidth, outputHeight;
    downsample(1, 1, &outputWidth, &outputHeight);
    EXPECT_EQ(32u, outputWidth);
    EXPECT_EQ(32u, outputHeight);
}

// Tests that JPEG decoder can downsample from 1/8 to 7/8.
TEST(JPEGImageDecoderTest, downsample1Over8To7Over8)
{
    unsigned outputWidth, outputHeight;

    // 1/8 downsample.
    downsample(40, 40, &outputWidth, &outputHeight);
    EXPECT_EQ(32u, outputWidth);
    EXPECT_EQ(32u, outputHeight);

    // 2/8 downsample.
    downsample(70, 70, &outputWidth, &outputHeight);
    EXPECT_EQ(64u, outputWidth);
    EXPECT_EQ(64u, outputHeight);

    // 3/8 downsample.
    downsample(100, 100, &outputWidth, &outputHeight);
    EXPECT_EQ(96u, outputWidth);
    EXPECT_EQ(96u, outputHeight);

    // 4/8 downsample.
    downsample(130, 130, &outputWidth, &outputHeight);
    EXPECT_EQ(128u, outputWidth);
    EXPECT_EQ(128u, outputHeight);

    // 5/8 downsample.
    downsample(170, 170, &outputWidth, &outputHeight);
    EXPECT_EQ(160u, outputWidth);
    EXPECT_EQ(160u, outputHeight);

    // 6/8 downsample.
    downsample(200, 200, &outputWidth, &outputHeight);
    EXPECT_EQ(192u, outputWidth);
    EXPECT_EQ(192u, outputHeight);

    // 7/8 downsample.
    downsample(230, 230, &outputWidth, &outputHeight);
    EXPECT_EQ(224u, outputWidth);
    EXPECT_EQ(224u, outputHeight);
}

// Tests that output image fits in a rectangular size.
TEST(JPEGImageDecoderTest, downsampleRectangle)
{
    unsigned outputWidth, outputHeight;
    downsample(130, 256, &outputWidth, &outputHeight);
    EXPECT_EQ(128u, outputWidth);
    EXPECT_EQ(128u, outputHeight);
}

// Tests that upsampling is not allowed.
TEST(JPEGImageDecoderTest, upsample)
{
    unsigned outputWidth, outputHeight;
    downsample(1000, 1000, &outputWidth, &outputHeight);
    EXPECT_EQ(256u, outputWidth);
    EXPECT_EQ(256u, outputHeight);
}
