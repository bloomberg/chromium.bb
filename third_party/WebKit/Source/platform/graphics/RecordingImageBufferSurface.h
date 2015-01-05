// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RecordingImageBufferSurface_h
#define RecordingImageBufferSurface_h

#include "platform/graphics/ImageBufferSurface.h"
#include "public/platform/WebThread.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "wtf/LinkedStack.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"

class SkPicture;
class SkPictureRecorder;
class RecordingImageBufferSurfaceTest;

namespace blink {

class ImageBuffer;

class RecordingImageBufferFallbackSurfaceFactory {
public:
    virtual PassOwnPtr<ImageBufferSurface> createSurface(const IntSize&, OpacityMode) = 0;
    virtual ~RecordingImageBufferFallbackSurfaceFactory() { }
};

class PLATFORM_EXPORT RecordingImageBufferSurface : public ImageBufferSurface {
    WTF_MAKE_NONCOPYABLE(RecordingImageBufferSurface); WTF_MAKE_FAST_ALLOCATED;
public:
    RecordingImageBufferSurface(const IntSize&, PassOwnPtr<RecordingImageBufferFallbackSurfaceFactory> fallbackFactory, OpacityMode = NonOpaque);
    virtual ~RecordingImageBufferSurface();

    // Implementation of ImageBufferSurface interfaces
    virtual SkCanvas* canvas() const override;
    virtual PassRefPtr<SkPicture> getPicture() override;
    virtual void willDrawVideo() override;
    virtual bool isValid() const override { return true; }
    virtual bool isRecording() const override { return !m_fallbackSurface; }
    virtual void willAccessPixels() override;
    virtual void finalizeFrame(const FloatRect&) override;
    virtual void didClearCanvas() override;
    virtual void setImageBuffer(ImageBuffer*) override;
    virtual PassRefPtr<SkImage> newImageSnapshot() const override;
    virtual bool needsClipTracking() const override { return !m_fallbackSurface; }
    virtual void draw(GraphicsContext*, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator, WebBlendMode, bool needsCopy) override;

    // Passthroughs to fallback surface
    virtual const SkBitmap& bitmap() override;
    virtual bool restore() override;
    virtual WebLayer* layer() const override;
    virtual bool isAccelerated() const override;
    virtual Platform3DObject getBackingTexture() const override;
    virtual bool cachedBitmapEnabled() const override;
    virtual const SkBitmap& cachedBitmap() const override;
    virtual void invalidateCachedBitmap() override;
    virtual void updateCachedBitmapIfNeeded() override;
    virtual void setIsHidden(bool) override;

private:
    friend class ::RecordingImageBufferSurfaceTest; // for unit testing
    void fallBackToRasterCanvas();
    bool initializeCurrentFrame();
    bool finalizeFrameInternal();

    OwnPtr<SkPictureRecorder> m_currentFrame;
    RefPtr<SkPicture> m_previousFrame;
    OwnPtr<ImageBufferSurface> m_fallbackSurface;
    ImageBuffer* m_imageBuffer;
    int m_initialSaveCount;
    bool m_frameWasCleared;
    OwnPtr<RecordingImageBufferFallbackSurfaceFactory> m_fallbackFactory;
};

} // namespace blink

#endif
