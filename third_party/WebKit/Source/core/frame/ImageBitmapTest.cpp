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
#include "core/frame/ImageBitmap.h"

#include "SkPixelRef.h"
#include "core/dom/Document.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/MockImageResourceClient.h"
#include "core/fetch/ResourcePtr.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/canvas/CanvasRenderingContext2D.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/skia/NativeImageSkia.h"
#include "platform/network/ResourceRequest.h"
#include "wtf/OwnPtr.h"

#include <gtest/gtest.h>

namespace WebCore {

class ImageBitmapTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
        m_bitmap.allocPixels();
        m_bitmap.eraseColor(0xFFFFFFFF);

        m_bitmap2.setConfig(SkBitmap::kARGB_8888_Config, 5, 5);
        m_bitmap2.allocPixels();
        m_bitmap2.eraseColor(0xAAAAAAAA);

        // Save the global memory cache to restore it upon teardown.
        m_globalMemoryCache = adoptPtr(memoryCache());
        // Create the test memory cache instance and hook it in.
        m_testingMemoryCache = adoptPtr(new MemoryCache());
        setMemoryCacheForTesting(m_testingMemoryCache.leakPtr());
    }
    virtual void TearDown()
    {
        // Regain the ownership of testing memory cache, so that it will be
        // destroyed.
        m_testingMemoryCache = adoptPtr(memoryCache());
        // Yield the ownership of the global memory cache back.
        setMemoryCacheForTesting(m_globalMemoryCache.leakPtr());
    }

    SkBitmap m_bitmap, m_bitmap2;
    OwnPtr<MemoryCache> m_testingMemoryCache;
    OwnPtr<MemoryCache> m_globalMemoryCache;
};

// Verifies that the image resource held by an ImageBitmap is the same as the
// one held by the HTMLImageElement.
TEST_F(ImageBitmapTest, ImageResourceConsistency)
{
    RefPtr<HTMLImageElement> imageElement = HTMLImageElement::create(*Document::create().get());
    imageElement->setImageResource(new ImageResource(BitmapImage::create(NativeImageSkia::create(m_bitmap)).get()));

    RefPtr<ImageBitmap> imageBitmapNoCrop = ImageBitmap::create(imageElement.get(), IntRect(0, 0, m_bitmap.width(), m_bitmap.height()));
    RefPtr<ImageBitmap> imageBitmapInteriorCrop = ImageBitmap::create(imageElement.get(), IntRect(m_bitmap.width() / 2, m_bitmap.height() / 2, m_bitmap.width() / 2, m_bitmap.height() / 2));
    RefPtr<ImageBitmap> imageBitmapExteriorCrop = ImageBitmap::create(imageElement.get(), IntRect(-m_bitmap.width() / 2, -m_bitmap.height() / 2, m_bitmap.width(), m_bitmap.height()));
    RefPtr<ImageBitmap> imageBitmapOutsideCrop = ImageBitmap::create(imageElement.get(), IntRect(-m_bitmap.width(), -m_bitmap.height(), m_bitmap.width(), m_bitmap.height()));

    ASSERT_EQ(imageBitmapNoCrop->bitmapImage().get(), imageElement->cachedImage()->image());
    ASSERT_EQ(imageBitmapInteriorCrop->bitmapImage().get(), imageElement->cachedImage()->image());
    ASSERT_EQ(imageBitmapExteriorCrop->bitmapImage().get(), imageElement->cachedImage()->image());

    RefPtr<Image> emptyImage = imageBitmapOutsideCrop->bitmapImage();
    ASSERT_NE(emptyImage.get(), imageElement->cachedImage()->image());
}

// Verifies that HTMLImageElements are given an elevated CacheLiveResourcePriority when used to construct an ImageBitmap.
// ImageBitmaps that have crop rects outside of the bounds of the HTMLImageElement do not have elevated CacheLiveResourcePriority.
TEST_F(ImageBitmapTest, ImageBitmapLiveResourcePriority)
{
    RefPtr<HTMLImageElement> imageNoCrop = HTMLImageElement::create(*Document::create().get());
    ResourcePtr<ImageResource> cachedImageNoCrop = new ImageResource(BitmapImage::create(NativeImageSkia::create(m_bitmap)).get());
    imageNoCrop->setImageResource(cachedImageNoCrop.get());

    RefPtr<HTMLImageElement> imageInteriorCrop = HTMLImageElement::create(*Document::create().get());
    ResourcePtr<ImageResource> cachedImageInteriorCrop = new ImageResource(BitmapImage::create(NativeImageSkia::create(m_bitmap)).get());
    imageInteriorCrop->setImageResource(cachedImageInteriorCrop.get());

    RefPtr<HTMLImageElement> imageExteriorCrop = HTMLImageElement::create(*Document::create().get());
    ResourcePtr<ImageResource> cachedImageExteriorCrop = new ImageResource(BitmapImage::create(NativeImageSkia::create(m_bitmap)).get());
    imageExteriorCrop->setImageResource(cachedImageExteriorCrop.get());

    RefPtr<HTMLImageElement> imageOutsideCrop = HTMLImageElement::create(*Document::create().get());
    ResourcePtr<ImageResource> cachedImageOutsideCrop = new ImageResource(BitmapImage::create(NativeImageSkia::create(m_bitmap)).get());
    imageOutsideCrop->setImageResource(cachedImageOutsideCrop.get());

    MockImageResourceClient mockClient1, mockClient2, mockClient3, mockClient4;
    cachedImageNoCrop->addClient(&mockClient1);
    cachedImageInteriorCrop->addClient(&mockClient2);
    cachedImageExteriorCrop->addClient(&mockClient3);
    cachedImageOutsideCrop->addClient(&mockClient4);

    memoryCache()->add(cachedImageNoCrop.get());
    memoryCache()->add(cachedImageInteriorCrop.get());
    memoryCache()->add(cachedImageExteriorCrop.get());
    memoryCache()->add(cachedImageOutsideCrop.get());
    memoryCache()->insertInLiveDecodedResourcesList(cachedImageNoCrop.get());
    memoryCache()->insertInLiveDecodedResourcesList(cachedImageInteriorCrop.get());
    memoryCache()->insertInLiveDecodedResourcesList(cachedImageExteriorCrop.get());
    memoryCache()->insertInLiveDecodedResourcesList(cachedImageOutsideCrop.get());

    // HTMLImageElements should default to CacheLiveResourcePriorityLow.
    ASSERT_EQ(imageNoCrop->cachedImage()->cacheLiveResourcePriority(), Resource::CacheLiveResourcePriorityLow);
    ASSERT_EQ(imageInteriorCrop->cachedImage()->cacheLiveResourcePriority(), Resource::CacheLiveResourcePriorityLow);
    ASSERT_EQ(imageExteriorCrop->cachedImage()->cacheLiveResourcePriority(), Resource::CacheLiveResourcePriorityLow);
    ASSERT_EQ(imageOutsideCrop->cachedImage()->cacheLiveResourcePriority(), Resource::CacheLiveResourcePriorityLow);

    RefPtr<ImageBitmap> imageBitmapInteriorCrop = ImageBitmap::create(imageInteriorCrop.get(), IntRect(m_bitmap.width() / 2, m_bitmap.height() / 2, m_bitmap.width(), m_bitmap.height()));
    {
        RefPtr<ImageBitmap> imageBitmapNoCrop = ImageBitmap::create(imageNoCrop.get(), IntRect(0, 0, m_bitmap.width(), m_bitmap.height()));
        RefPtr<ImageBitmap> imageBitmapInteriorCrop2 = ImageBitmap::create(imageInteriorCrop.get(), IntRect(m_bitmap.width() / 2, m_bitmap.height() / 2, m_bitmap.width(), m_bitmap.height()));
        RefPtr<ImageBitmap> imageBitmapExteriorCrop = ImageBitmap::create(imageExteriorCrop.get(), IntRect(-m_bitmap.width() / 2, -m_bitmap.height() / 2, m_bitmap.width(), m_bitmap.height()));
        RefPtr<ImageBitmap> imageBitmapOutsideCrop = ImageBitmap::create(imageOutsideCrop.get(), IntRect(-m_bitmap.width(), -m_bitmap.height(), m_bitmap.width(), m_bitmap.height()));

        // Images that are referenced by ImageBitmaps have CacheLiveResourcePriorityHigh.
        ASSERT_EQ(imageNoCrop->cachedImage()->cacheLiveResourcePriority(), Resource::CacheLiveResourcePriorityHigh);
        ASSERT_EQ(imageInteriorCrop->cachedImage()->cacheLiveResourcePriority(), Resource::CacheLiveResourcePriorityHigh);
        ASSERT_EQ(imageExteriorCrop->cachedImage()->cacheLiveResourcePriority(), Resource::CacheLiveResourcePriorityHigh);

        // ImageBitmaps that do not contain any of the source image do not elevate CacheLiveResourcePriority.
        ASSERT_EQ(imageOutsideCrop->cachedImage()->cacheLiveResourcePriority(), Resource::CacheLiveResourcePriorityLow);
    }

    // CacheLiveResourcePriroity should return to CacheLiveResourcePriorityLow when no ImageBitmaps reference the image.
    ASSERT_EQ(imageNoCrop->cachedImage()->cacheLiveResourcePriority(), Resource::CacheLiveResourcePriorityLow);
    ASSERT_EQ(imageExteriorCrop->cachedImage()->cacheLiveResourcePriority(), Resource::CacheLiveResourcePriorityLow);
    ASSERT_EQ(imageOutsideCrop->cachedImage()->cacheLiveResourcePriority(), Resource::CacheLiveResourcePriorityLow);

    // There is still an ImageBitmap that references this image.
    ASSERT_EQ(imageInteriorCrop->cachedImage()->cacheLiveResourcePriority(), Resource::CacheLiveResourcePriorityHigh);
}

// Verifies that ImageBitmaps constructed from HTMLImageElements hold a reference to the original Image if the HTMLImageElement src is changed.
TEST_F(ImageBitmapTest, ImageBitmapSourceChanged)
{
    RefPtr<HTMLImageElement> image = HTMLImageElement::create(*Document::create().get());
    ResourcePtr<ImageResource> originalImageResource = new ImageResource(BitmapImage::create(NativeImageSkia::create(m_bitmap)).get());
    image->setImageResource(originalImageResource.get());

    RefPtr<ImageBitmap> imageBitmap = ImageBitmap::create(image.get(), IntRect(0, 0, m_bitmap.width(), m_bitmap.height()));
    ASSERT_EQ(imageBitmap->bitmapImage().get(), originalImageResource->image());

    ResourcePtr<ImageResource> newImageResource = new ImageResource(BitmapImage::create(NativeImageSkia::create(m_bitmap2)).get());
    image->setImageResource(newImageResource.get());

    // The ImageBitmap should contain the same data as the original cached image but should no longer hold a reference.
    ASSERT_NE(imageBitmap->bitmapImage().get(), originalImageResource->image());
    ASSERT_EQ(imageBitmap->bitmapImage()->nativeImageForCurrentFrame()->bitmap().pixelRef()->pixels(),
        originalImageResource->image()->nativeImageForCurrentFrame()->bitmap().pixelRef()->pixels());

    ASSERT_NE(imageBitmap->bitmapImage().get(), newImageResource->image());
    ASSERT_NE(imageBitmap->bitmapImage()->nativeImageForCurrentFrame()->bitmap().pixelRef()->pixels(),
        newImageResource->image()->nativeImageForCurrentFrame()->bitmap().pixelRef()->pixels());
}

// Verifies that ImageBitmaps constructed from ImageBitmaps hold onto their own Image.
TEST_F(ImageBitmapTest, ImageResourceLifetime)
{
    RefPtr<HTMLCanvasElement> canvasElement = HTMLCanvasElement::create(*Document::create().get());
    canvasElement->setHeight(40);
    canvasElement->setWidth(40);
    RefPtr<ImageBitmap> imageBitmapDerived;
    {
        RefPtr<ImageBitmap> imageBitmapFromCanvas = ImageBitmap::create(canvasElement.get(), IntRect(0, 0, canvasElement->width(), canvasElement->height()));
        imageBitmapDerived = ImageBitmap::create(imageBitmapFromCanvas.get(), IntRect(0, 0, 20, 20));
    }
    CanvasRenderingContext* context = canvasElement->getContext("2d");
    TrackExceptionState exceptionState;
    toCanvasRenderingContext2D(context)->drawImage(imageBitmapDerived.get(), 0, 0, exceptionState);
}

} // namespace
