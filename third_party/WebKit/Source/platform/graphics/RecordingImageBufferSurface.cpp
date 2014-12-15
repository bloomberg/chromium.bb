// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "platform/graphics/RecordingImageBufferSurface.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "public/platform/Platform.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"

namespace blink {

RecordingImageBufferSurface::RecordingImageBufferSurface(const IntSize& size, PassOwnPtr<RecordingImageBufferFallbackSurfaceFactory> fallbackFactory, OpacityMode opacityMode)
    : ImageBufferSurface(size, opacityMode)
    , m_imageBuffer(0)
    , m_frameWasCleared(true)
    , m_fallbackFactory(fallbackFactory)
{
    initializeCurrentFrame();
}

RecordingImageBufferSurface::~RecordingImageBufferSurface()
{ }

bool RecordingImageBufferSurface::initializeCurrentFrame()
{
    static SkRTreeFactory rTreeFactory;
    m_currentFrame = adoptPtr(new SkPictureRecorder);
    m_currentFrame->beginRecording(size().width(), size().height(), &rTreeFactory);
    if (m_imageBuffer) {
        m_imageBuffer->resetCanvas(m_currentFrame->getRecordingCanvas());
        m_imageBuffer->context()->setRegionTrackingMode(GraphicsContext::RegionTrackingOverwrite);
    }

    return true;
}

void RecordingImageBufferSurface::setImageBuffer(ImageBuffer* imageBuffer)
{
    m_imageBuffer = imageBuffer;
    if (m_currentFrame && m_imageBuffer) {
        m_imageBuffer->context()->setRegionTrackingMode(GraphicsContext::RegionTrackingOverwrite);
        m_imageBuffer->resetCanvas(m_currentFrame->getRecordingCanvas());
    }
    if (m_fallbackSurface) {
        m_fallbackSurface->setImageBuffer(imageBuffer);
        m_imageBuffer->context()->setRegionTrackingMode(GraphicsContext::RegionTrackingDisabled);
    }
}

void RecordingImageBufferSurface::willAccessPixels()
{
    if (m_fallbackSurface)
        m_fallbackSurface->willAccessPixels();
    else
        fallBackToRasterCanvas();
}

void RecordingImageBufferSurface::fallBackToRasterCanvas()
{
    if (m_fallbackSurface) {
        ASSERT(!m_currentFrame);
        return;
    }

    m_fallbackSurface = m_fallbackFactory->createSurface(size(), opacityMode());
    m_fallbackSurface->setImageBuffer(m_imageBuffer);

    if (m_previousFrame) {
        m_previousFrame->playback(m_fallbackSurface->canvas());
        m_previousFrame.clear();
    }

    if (m_currentFrame) {
        RefPtr<SkPicture> currentPicture = adoptRef(m_currentFrame->endRecording());
        currentPicture->playback(m_fallbackSurface->canvas());
        m_currentFrame.clear();
    }

    if (m_imageBuffer) {
        m_imageBuffer->context()->setRegionTrackingMode(GraphicsContext::RegionTrackingDisabled);
        m_imageBuffer->resetCanvas(m_fallbackSurface->canvas());
        m_imageBuffer->context()->setAccelerated(m_fallbackSurface->isAccelerated());
    }

}

PassRefPtr<SkImage> RecordingImageBufferSurface::newImageSnapshot() const
{
    if (!m_fallbackSurface)
        const_cast<RecordingImageBufferSurface*>(this)->fallBackToRasterCanvas();
    return m_fallbackSurface->newImageSnapshot();
}

SkCanvas* RecordingImageBufferSurface::canvas() const
{
    if (m_fallbackSurface)
        return m_fallbackSurface->canvas();

    ASSERT(m_currentFrame->getRecordingCanvas());
    return m_currentFrame->getRecordingCanvas();
}

PassRefPtr<SkPicture> RecordingImageBufferSurface::getPicture()
{
    if (m_fallbackSurface)
        return nullptr;

    bool canUsePicture = finalizeFrameInternal();
    m_imageBuffer->didFinalizeFrame();

    if (canUsePicture) {
        return m_previousFrame;
    }

    if (!m_fallbackSurface)
        fallBackToRasterCanvas();
    return nullptr;
}

void RecordingImageBufferSurface::finalizeFrame(const FloatRect &dirtyRect)
{
    if (m_fallbackSurface) {
        m_fallbackSurface->finalizeFrame(dirtyRect);
        return;
    }

    if (!finalizeFrameInternal())
        fallBackToRasterCanvas();
}

void RecordingImageBufferSurface::didClearCanvas()
{
    if (m_fallbackSurface) {
        m_fallbackSurface->didClearCanvas();
        return;
    }
    m_frameWasCleared = true;
}

bool RecordingImageBufferSurface::finalizeFrameInternal()
{
    ASSERT(!m_fallbackSurface);

    if (!m_imageBuffer->isDirty()) {
        if (m_currentFrame && !m_previousFrame) {
            // Create an initial blank frame
            m_previousFrame = adoptRef(m_currentFrame->endRecording());
            initializeCurrentFrame();
        }
        return m_currentFrame;
    }

    IntRect canvasRect(IntPoint(0, 0), size());
    if (!m_frameWasCleared && !m_imageBuffer->context()->opaqueRegion().asRect().contains(canvasRect)) {
        return false;
    }

    m_previousFrame = adoptRef(m_currentFrame->endRecording());
    if (!initializeCurrentFrame())
        return false;


    m_frameWasCleared = false;
    return true;
}

void RecordingImageBufferSurface::willDrawVideo()
{
    // Video draws need to be synchronous
    if (!m_fallbackSurface)
        fallBackToRasterCanvas();
}

void RecordingImageBufferSurface::draw(GraphicsContext* context, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, WebBlendMode blendMode, bool needsCopy)
{
    if (m_fallbackSurface) {
        m_fallbackSurface->draw(context, destRect, srcRect, op, blendMode, needsCopy);
        return;
    }

    RefPtr<SkPicture> picture = getPicture();
    if (picture) {
        context->compositePicture(picture.get(), destRect, srcRect, op, blendMode);
    } else {
        ImageBufferSurface::draw(context, destRect, srcRect, op, blendMode, needsCopy);
    }
}

// Fallback passthroughs

const SkBitmap& RecordingImageBufferSurface::bitmap()
{
    if (m_fallbackSurface)
        return m_fallbackSurface->bitmap();
    return ImageBufferSurface::bitmap();
}

bool RecordingImageBufferSurface::restore()
{
    if (m_fallbackSurface)
        return m_fallbackSurface->restore();
    return ImageBufferSurface::restore();
}

WebLayer* RecordingImageBufferSurface::layer() const
{
    if (m_fallbackSurface)
        return m_fallbackSurface->layer();
    return ImageBufferSurface::layer();
}

bool RecordingImageBufferSurface::isAccelerated() const
{
    if (m_fallbackSurface)
        return m_fallbackSurface->isAccelerated();
    return ImageBufferSurface::isAccelerated();
}

Platform3DObject RecordingImageBufferSurface::getBackingTexture() const
{
    if (m_fallbackSurface)
        return m_fallbackSurface->getBackingTexture();
    return ImageBufferSurface::getBackingTexture();
}

bool RecordingImageBufferSurface::cachedBitmapEnabled() const
{
    if (m_fallbackSurface)
        return m_fallbackSurface->cachedBitmapEnabled();
    return ImageBufferSurface::cachedBitmapEnabled();
}

const SkBitmap& RecordingImageBufferSurface::cachedBitmap() const
{
    if (m_fallbackSurface)
        return m_fallbackSurface->cachedBitmap();
    return ImageBufferSurface::cachedBitmap();
}

void RecordingImageBufferSurface::invalidateCachedBitmap()
{
    if (m_fallbackSurface)
        m_fallbackSurface->invalidateCachedBitmap();
    else
        ImageBufferSurface::invalidateCachedBitmap();
}

void RecordingImageBufferSurface::updateCachedBitmapIfNeeded()
{
    if (m_fallbackSurface)
        m_fallbackSurface->updateCachedBitmapIfNeeded();
    else
        ImageBufferSurface::updateCachedBitmapIfNeeded();
}

void RecordingImageBufferSurface::setIsHidden(bool hidden)
{
    if (m_fallbackSurface)
        m_fallbackSurface->setIsHidden(hidden);
    else
        ImageBufferSurface::setIsHidden(hidden);
}

} // namespace blink
