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

#include "core/frame/ImageBitmap.h"

#include "SkPixelRef.h" // FIXME: qualify this skia header file.
#include "bindings/core/v8/UnionTypesCore.h"
#include "core/dom/Document.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/MockImageResourceClient.h"
#include "core/fetch/ResourcePtr.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/heap/Handle.h"
#include "platform/network/ResourceRequest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/OwnPtr.h"

namespace blink {

class ImageBitmapTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        RefPtr<SkSurface> surface = adoptRef(SkSurface::NewRasterN32Premul(10, 10));
        surface->getCanvas()->clear(0xFFFFFFFF);
        m_image = adoptRef(surface->newImageSnapshot());

        RefPtr<SkSurface> surface2 = adoptRef(SkSurface::NewRasterN32Premul(5, 5));
        surface2->getCanvas()->clear(0xAAAAAAAA);
        m_image2 = adoptRef(surface2->newImageSnapshot());

        // Save the global memory cache to restore it upon teardown.
        m_globalMemoryCache = replaceMemoryCacheForTesting(MemoryCache::create());
    }
    virtual void TearDown()
    {
        // Garbage collection is required prior to switching out the
        // test's memory cache; image resources are released, evicting
        // them from the cache.
        Heap::collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::ForcedGC);

        replaceMemoryCacheForTesting(m_globalMemoryCache.release());
    }

    RefPtr<SkImage> m_image, m_image2;
    Persistent<MemoryCache> m_globalMemoryCache;
};

TEST_F(ImageBitmapTest, ImageResourceConsistency)
{
    RefPtrWillBeRawPtr<HTMLImageElement> imageElement = HTMLImageElement::create(*Document::create().get());
    imageElement->setImageResource(new ImageResource(StaticBitmapImage::create(m_image).get()));

    RefPtrWillBeRawPtr<ImageBitmap> imageBitmapNoCrop = ImageBitmap::create(imageElement.get(),
        IntRect(0, 0, m_image->width(), m_image->height()),
        &(imageElement->document()));
    RefPtrWillBeRawPtr<ImageBitmap> imageBitmapInteriorCrop = ImageBitmap::create(imageElement.get(),
        IntRect(m_image->width() / 2, m_image->height() / 2, m_image->width() / 2, m_image->height() / 2),
        &(imageElement->document()));
    RefPtrWillBeRawPtr<ImageBitmap> imageBitmapExteriorCrop = ImageBitmap::create(imageElement.get(),
        IntRect(-m_image->width() / 2, -m_image->height() / 2, m_image->width(), m_image->height()),
        &(imageElement->document()));
    RefPtrWillBeRawPtr<ImageBitmap> imageBitmapOutsideCrop = ImageBitmap::create(imageElement.get(),
        IntRect(-m_image->width(), -m_image->height(), m_image->width(), m_image->height()),
        &(imageElement->document()));

    ASSERT_EQ(imageBitmapNoCrop->bitmapImage()->imageForCurrentFrame(), imageElement->cachedImage()->image()->imageForCurrentFrame());
    ASSERT_NE(imageBitmapInteriorCrop->bitmapImage()->imageForCurrentFrame(), imageElement->cachedImage()->image()->imageForCurrentFrame());
    ASSERT_NE(imageBitmapExteriorCrop->bitmapImage()->imageForCurrentFrame(), imageElement->cachedImage()->image()->imageForCurrentFrame());

    StaticBitmapImage* emptyImage = imageBitmapOutsideCrop->bitmapImage();
    ASSERT_NE(emptyImage->imageForCurrentFrame(), imageElement->cachedImage()->image()->imageForCurrentFrame());
}

// Verifies that HTMLImageElements are given an elevated CacheLiveResourcePriority when used to construct an ImageBitmap.
// ImageBitmaps that have crop rects outside of the bounds of the HTMLImageElement do not have elevated CacheLiveResourcePriority.
TEST_F(ImageBitmapTest, ImageBitmapLiveResourcePriority)
{
    RefPtrWillBePersistent<HTMLImageElement> imageNoCrop = HTMLImageElement::create(*Document::create().get());
    ResourcePtr<ImageResource> cachedImageNoCrop = new ImageResource(ResourceRequest("http://foo.com/1"),
        StaticBitmapImage::create(m_image).get());
    imageNoCrop->setImageResource(cachedImageNoCrop.get());

    RefPtrWillBePersistent<HTMLImageElement> imageInteriorCrop = HTMLImageElement::create(*Document::create().get());
    ResourcePtr<ImageResource> cachedImageInteriorCrop = new ImageResource(ResourceRequest("http://foo.com/2"),
        StaticBitmapImage::create(m_image).get());
    imageInteriorCrop->setImageResource(cachedImageInteriorCrop.get());

    RefPtrWillBePersistent<HTMLImageElement> imageExteriorCrop = HTMLImageElement::create(*Document::create().get());
    ResourcePtr<ImageResource> cachedImageExteriorCrop = new ImageResource(ResourceRequest("http://foo.com/3"),
        StaticBitmapImage::create(m_image).get());
    imageExteriorCrop->setImageResource(cachedImageExteriorCrop.get());

    RefPtrWillBePersistent<HTMLImageElement> imageOutsideCrop = HTMLImageElement::create(*Document::create().get());
    ResourcePtr<ImageResource> cachedImageOutsideCrop = new ImageResource(ResourceRequest("http://foo.com/4"),
        StaticBitmapImage::create(m_image).get());
    imageOutsideCrop->setImageResource(cachedImageOutsideCrop.get());

    MockImageResourceClient mockClient1(cachedImageNoCrop);
    MockImageResourceClient mockClient2(cachedImageInteriorCrop);
    MockImageResourceClient mockClient3(cachedImageExteriorCrop);
    MockImageResourceClient mockClient4(cachedImageOutsideCrop);

    memoryCache()->add(cachedImageNoCrop.get());
    memoryCache()->add(cachedImageInteriorCrop.get());
    memoryCache()->add(cachedImageExteriorCrop.get());
    memoryCache()->add(cachedImageOutsideCrop.get());
    memoryCache()->updateDecodedResource(cachedImageNoCrop.get(), UpdateForPropertyChange);
    memoryCache()->updateDecodedResource(cachedImageInteriorCrop.get(), UpdateForPropertyChange);
    memoryCache()->updateDecodedResource(cachedImageExteriorCrop.get(), UpdateForPropertyChange);
    memoryCache()->updateDecodedResource(cachedImageOutsideCrop.get(), UpdateForPropertyChange);

    // HTMLImageElements should default to CacheLiveResourcePriorityLow.
    ASSERT_EQ(memoryCache()->priority(imageNoCrop->cachedImage()), MemoryCacheLiveResourcePriorityLow);
    ASSERT_EQ(memoryCache()->priority(imageInteriorCrop->cachedImage()), MemoryCacheLiveResourcePriorityLow);
    ASSERT_EQ(memoryCache()->priority(imageExteriorCrop->cachedImage()), MemoryCacheLiveResourcePriorityLow);
    ASSERT_EQ(memoryCache()->priority(imageOutsideCrop->cachedImage()), MemoryCacheLiveResourcePriorityLow);

    RefPtrWillBePersistent<ImageBitmap> imageBitmapInteriorCrop = ImageBitmap::create(imageInteriorCrop.get(),
        IntRect(m_image->width() / 2, m_image->height() / 2, m_image->width(), m_image->height()),
        &(imageInteriorCrop->document()));
    {
        RefPtrWillBePersistent<ImageBitmap> imageBitmapNoCrop = ImageBitmap::create(imageNoCrop.get(),
            IntRect(0, 0, m_image->width(), m_image->height()),
            &(imageNoCrop->document()));
        RefPtrWillBePersistent<ImageBitmap> imageBitmapInteriorCrop2 = ImageBitmap::create(imageInteriorCrop.get(),
            IntRect(m_image->width() / 2, m_image->height() / 2, m_image->width(), m_image->height()),
            &(imageInteriorCrop->document()));
        RefPtrWillBePersistent<ImageBitmap> imageBitmapExteriorCrop = ImageBitmap::create(imageExteriorCrop.get(),
            IntRect(-m_image->width() / 2, -m_image->height() / 2, m_image->width(), m_image->height()),
            &(imageExteriorCrop->document()));
        RefPtrWillBePersistent<ImageBitmap> imageBitmapOutsideCrop = ImageBitmap::create(imageOutsideCrop.get(),
            IntRect(-m_image->width(), -m_image->height(), m_image->width(), m_image->height()),
            &(imageOutsideCrop->document()));

        // Images are not referenced by ImageBitmap anymore, so always CacheLiveResourcePriorityLow
        ASSERT_EQ(memoryCache()->priority(imageNoCrop->cachedImage()), MemoryCacheLiveResourcePriorityLow);
        ASSERT_EQ(memoryCache()->priority(imageInteriorCrop->cachedImage()), MemoryCacheLiveResourcePriorityLow);
        ASSERT_EQ(memoryCache()->priority(imageExteriorCrop->cachedImage()), MemoryCacheLiveResourcePriorityLow);

        // ImageBitmaps that do not contain any of the source image do not elevate CacheLiveResourcePriority.
        ASSERT_EQ(memoryCache()->priority(imageOutsideCrop->cachedImage()), MemoryCacheLiveResourcePriorityLow);
    }
    // Force a garbage collection to sweep out the local ImageBitmaps.
    Heap::collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::ForcedGC);

    // CacheLiveResourcePriroity should return to CacheLiveResourcePriorityLow when no ImageBitmaps reference the image.
    ASSERT_EQ(memoryCache()->priority(imageNoCrop->cachedImage()), MemoryCacheLiveResourcePriorityLow);
    ASSERT_EQ(memoryCache()->priority(imageExteriorCrop->cachedImage()), MemoryCacheLiveResourcePriorityLow);
    ASSERT_EQ(memoryCache()->priority(imageOutsideCrop->cachedImage()), MemoryCacheLiveResourcePriorityLow);

    // There is still an ImageBitmap that references this image.
    ASSERT_EQ(memoryCache()->priority(imageInteriorCrop->cachedImage()), MemoryCacheLiveResourcePriorityLow);
    imageBitmapInteriorCrop = nullptr;
}

// Verifies that ImageBitmaps constructed from HTMLImageElements hold a reference to the original Image if the HTMLImageElement src is changed.
TEST_F(ImageBitmapTest, ImageBitmapSourceChanged)
{
    RefPtrWillBeRawPtr<HTMLImageElement> image = HTMLImageElement::create(*Document::create().get());
    ResourcePtr<ImageResource> originalImageResource = new ImageResource(
        StaticBitmapImage::create(m_image).get());
    image->setImageResource(originalImageResource.get());

    RefPtrWillBeRawPtr<ImageBitmap> imageBitmap = ImageBitmap::create(image.get(),
        IntRect(0, 0, m_image->width(), m_image->height()),
        &(image->document()));
    ASSERT_EQ(imageBitmap->bitmapImage()->imageForCurrentFrame(), originalImageResource->image()->imageForCurrentFrame());

    ResourcePtr<ImageResource> newImageResource = new ImageResource(
        StaticBitmapImage::create(m_image2).get());
    image->setImageResource(newImageResource.get());

    // The ImageBitmap should contain the same data as the original cached image
    {
        ASSERT_EQ(imageBitmap->bitmapImage()->imageForCurrentFrame(), originalImageResource->image()->imageForCurrentFrame());
        SkImage* image1 = imageBitmap->bitmapImage()->imageForCurrentFrame().get();
        ASSERT_NE(image1, nullptr);
        SkImage* image2 = originalImageResource->image()->imageForCurrentFrame().get();
        ASSERT_NE(image2, nullptr);
        ASSERT_EQ(image1, image2);
    }

    {
        ASSERT_NE(imageBitmap->bitmapImage()->imageForCurrentFrame(), newImageResource->image()->imageForCurrentFrame());
        SkImage* image1 = imageBitmap->bitmapImage()->imageForCurrentFrame().get();
        ASSERT_NE(image1, nullptr);
        SkImage* image2 = newImageResource->image()->imageForCurrentFrame().get();
        ASSERT_NE(image2, nullptr);
        ASSERT_NE(image1, image2);
    }
}

} // namespace
