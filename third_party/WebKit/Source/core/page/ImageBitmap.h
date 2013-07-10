// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageBitmap_h
#define ImageBitmap_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/platform/graphics/BitmapImage.h"
#include "core/platform/graphics/ImageBuffer.h"
#include "core/platform/graphics/IntRect.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageData;
class ImageBitmapCallback;

class ImageBitmap : public RefCounted<ImageBitmap>, public ScriptWrappable {

public:
    static PassRefPtr<ImageBitmap> create(HTMLImageElement*, IntRect);
    static PassRefPtr<ImageBitmap> create(HTMLVideoElement*, IntRect);
    static PassRefPtr<ImageBitmap> create(HTMLCanvasElement*, IntRect);
    static PassRefPtr<ImageBitmap> create(ImageData*, IntRect);
    static PassRefPtr<ImageBitmap> create(ImageBitmap*, IntRect);

    BitmapImage* bitmapImage() { return m_bitmap.get(); }

    int width() { return m_bitmap->width(); }
    int height() { return m_bitmap->height(); }
    IntSize size() { return IntSize(m_bitmap->width(), m_bitmap->height()); }
    IntPoint originOffset() { return m_originOffset; }

    ~ImageBitmap() { };

private:
    ImageBitmap(HTMLImageElement*, IntRect);
    ImageBitmap(HTMLVideoElement*, IntRect);
    ImageBitmap(HTMLCanvasElement*, IntRect);
    ImageBitmap(ImageData*, IntRect);
    ImageBitmap(ImageBitmap*, IntRect);

    RefPtr<BitmapImage> m_bitmap;
    OwnPtr<ImageBuffer> m_buffer;

    IntPoint m_originOffset; // offset applied to the image when it is drawn to the context
};

} // namespace WebCore

#endif // ImageBitmap_h
