// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageBitmap_h
#define ImageBitmap_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/html/HTMLImageElement.h"
#include "core/platform/graphics/Image.h"
#include "core/platform/graphics/ImageBuffer.h"
#include "core/platform/graphics/IntRect.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class HTMLCanvasElement;
class HTMLVideoElement;
class ImageData;

class ImageBitmap : public RefCounted<ImageBitmap>, public ScriptWrappable, public ImageLoaderClient {

public:
    static PassRefPtr<ImageBitmap> create(HTMLImageElement*, const IntRect&);
    static PassRefPtr<ImageBitmap> create(HTMLVideoElement*, const IntRect&);
    static PassRefPtr<ImageBitmap> create(HTMLCanvasElement*, const IntRect&);
    static PassRefPtr<ImageBitmap> create(ImageData*, const IntRect&);
    static PassRefPtr<ImageBitmap> create(ImageBitmap*, const IntRect&);

    PassRefPtr<Image> bitmapImage() const;
    PassRefPtr<HTMLImageElement> imageElement() const { return m_imageElement; }

    IntRect bitmapRect() const { return m_bitmapRect; }

    int width() const { return m_cropRect.width(); }
    int height() const { return m_cropRect.height(); }
    IntSize size() const { return m_cropRect.size(); }

    ~ImageBitmap();

private:
    ImageBitmap(HTMLImageElement*, const IntRect&);
    ImageBitmap(HTMLVideoElement*, const IntRect&);
    ImageBitmap(HTMLCanvasElement*, const IntRect&);
    ImageBitmap(ImageData*, const IntRect&);
    ImageBitmap(ImageBitmap*, const IntRect&);

    // ImageLoaderClient
    virtual void notifyImageSourceChanged();
    virtual bool requestsHighLiveResourceCachePriority() { return true; }

    // ImageBitmaps constructed from HTMLImageElements hold a reference to the HTMLImageElement until
    // the image source changes.
    RefPtr<HTMLImageElement> m_imageElement;
    RefPtr<Image> m_bitmap;
    OwnPtr<ImageBuffer> m_buffer;

    IntRect m_bitmapRect; // The rect where the underlying Image should be placed in reference to the ImageBitmap.
    IntRect m_cropRect;
};

} // namespace WebCore

#endif // ImageBitmap_h
