// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageBitmap_h
#define ImageBitmap_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/heap/Handle.h"
#include "third_party/skia/include/core/SkImage.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace blink {

class HTMLCanvasElement;
class HTMLVideoElement;
class ImageData;

class CORE_EXPORT ImageBitmap final : public RefCountedWillBeGarbageCollectedFinalized<ImageBitmap>, public ScriptWrappable, public ImageLoaderClient, public CanvasImageSource {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ImageBitmap);
public:
    static PassRefPtrWillBeRawPtr<ImageBitmap> create(HTMLImageElement*, const IntRect&);
    static PassRefPtrWillBeRawPtr<ImageBitmap> create(HTMLVideoElement*, const IntRect&);
    static PassRefPtrWillBeRawPtr<ImageBitmap> create(HTMLCanvasElement*, const IntRect&);
    static PassRefPtrWillBeRawPtr<ImageBitmap> create(ImageData*, const IntRect&);
    static PassRefPtrWillBeRawPtr<ImageBitmap> create(ImageBitmap*, const IntRect&);
    static PassRefPtrWillBeRawPtr<ImageBitmap> create(Image*, const IntRect&);

    SkImage* skImage() const { return (m_image) ? m_image.get() : nullptr; }
    int width() const { return (m_image) ? m_image->width(): 0; }
    int height() const { return (m_image) ? m_image->height(): 0; }
    IntSize size() const { return (m_image) ? IntSize(m_image->width(), m_image->height()) : IntSize(); }

    ~ImageBitmap() override;

    // CanvasImageSource implementation
    PassRefPtr<Image> getSourceImageForCanvas(SourceImageStatus*, AccelerationHint) const override;
    bool wouldTaintOrigin(SecurityOrigin*) const override { return false; }
    void adjustDrawRects(FloatRect* srcRect, FloatRect* dstRect) const override;
    FloatSize elementSize() const override;

    DECLARE_VIRTUAL_TRACE();

private:
    ImageBitmap(HTMLImageElement*, const IntRect&);
    ImageBitmap(HTMLVideoElement*, const IntRect&);
    ImageBitmap(HTMLCanvasElement*, const IntRect&);
    ImageBitmap(ImageData*, const IntRect&);
    ImageBitmap(ImageBitmap*, const IntRect&);
    ImageBitmap(Image*, const IntRect&);

    PassRefPtr<SkImage> cropImage(PassRefPtr<SkImage>, const IntRect&);

    // ImageLoaderClient
    void notifyImageSourceChanged() override;
    bool requestsHighLiveResourceCachePriority() override { return true; }

    RefPtr<SkImage> m_image;
};

} // namespace blink

#endif // ImageBitmap_h
