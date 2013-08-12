/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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
#include "core/page/ImageBitmap.h"

#include "core/dom/Document.h"
#include "core/html/HTMLImageElement.h"
#include "core/loader/cache/ImageResource.h"
#include "core/platform/graphics/BitmapImage.h"
#include "core/platform/graphics/skia/NativeImageSkia.h"

#include <gtest/gtest.h>

namespace WebCore {

class ImageBitmapTest : public ::testing::Test {
};

// Verifies that the cached image held by an ImageBitmap is the same as the
// one held by the HTMLImageElement.
TEST_F(ImageBitmapTest, ImageResourceConsistency)
{
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    bitmap.allocPixels();
    bitmap.eraseColor(0xFFFFFFFF);

    RefPtr<HTMLImageElement> imageElement = HTMLImageElement::create(Document::create().get());
    imageElement->setImageResource(new ImageResource(BitmapImage::create(NativeImageSkia::create(bitmap)).get()));

    RefPtr<ImageBitmap> imageBitmapNoCrop = ImageBitmap::create(imageElement.get(), IntRect(0, 0, bitmap.width(), bitmap.height()));
    RefPtr<ImageBitmap> imageBitmapInteriorCrop = ImageBitmap::create(imageElement.get(), IntRect(bitmap.width() / 2, bitmap.height() / 2, bitmap.width() / 2, bitmap.height() / 2));
    RefPtr<ImageBitmap> imageBitmapExteriorCrop = ImageBitmap::create(imageElement.get(), IntRect(-bitmap.width() / 2, -bitmap.height() / 2, bitmap.width(), bitmap.height()));
    RefPtr<ImageBitmap> imageBitmapOutsideCrop = ImageBitmap::create(imageElement.get(), IntRect(-bitmap.width(), -bitmap.height(), bitmap.width(), bitmap.height()));

    ASSERT_EQ(imageBitmapNoCrop->bitmapImage().get(), imageElement->cachedImage()->image());
    ASSERT_EQ(imageBitmapInteriorCrop->bitmapImage().get(), imageElement->cachedImage()->image());
    ASSERT_EQ(imageBitmapExteriorCrop->bitmapImage().get(), imageElement->cachedImage()->image());

    RefPtr<Image> emptyImage = imageBitmapOutsideCrop->bitmapImage();
    ASSERT_NE(emptyImage.get(), imageElement->cachedImage()->image());
}

} // namespace
