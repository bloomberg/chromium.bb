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
    , m_initialSaveCount(0)
    , m_frameWasCleared(true)
    , m_fallbackFactory(fallbackFactory)
{
    initializeCurrentFrame();
}

RecordingImageBufferSurface::~RecordingImageBufferSurface()
{ }

bool RecordingImageBufferSurface::initializeCurrentFrame()
{
    StateStack stateStack;
    bool saved = false;
    if (m_currentFrame) {
        saved = saveState(m_currentFrame->getRecordingCanvas(), &stateStack);
        if (!saved)
            return false;
    }

    static SkRTreeFactory rTreeFactory;
    m_currentFrame = adoptPtr(new SkPictureRecorder);
    m_currentFrame->beginRecording(size().width(), size().height(), &rTreeFactory);
    m_initialSaveCount = m_currentFrame->getRecordingCanvas()->getSaveCount();
    if (m_imageBuffer) {
        m_imageBuffer->context()->resetCanvas(m_currentFrame->getRecordingCanvas());
        m_imageBuffer->context()->setRegionTrackingMode(GraphicsContext::RegionTrackingOverwrite);
    }

    if (saved)
        setCurrentState(m_currentFrame->getRecordingCanvas(), &stateStack);

    return true;
}

void RecordingImageBufferSurface::setImageBuffer(ImageBuffer* imageBuffer)
{
    m_imageBuffer = imageBuffer;
    if (m_currentFrame && m_imageBuffer) {
        m_imageBuffer->context()->setRegionTrackingMode(GraphicsContext::RegionTrackingOverwrite);
        m_imageBuffer->context()->resetCanvas(m_currentFrame->getRecordingCanvas());
    }
    if (m_fallbackSurface) {
        m_fallbackSurface->setImageBuffer(imageBuffer);
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
        m_previousFrame->draw(m_fallbackSurface->canvas());
        m_previousFrame.clear();
    }
    if (m_currentFrame) {
        RefPtr<SkPicture> currentPicture = adoptRef(m_currentFrame->endRecording());
        currentPicture->draw(m_fallbackSurface->canvas());
        m_currentFrame.clear();
    }

    if (m_imageBuffer) {
        m_imageBuffer->context()->setRegionTrackingMode(GraphicsContext::RegionTrackingDisabled);
        m_imageBuffer->context()->resetCanvas(m_fallbackSurface->canvas());
    }
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

bool RecordingImageBufferSurface::saveState(SkCanvas* srcCanvas, StateStack* stateStack)
{
    StateRec state;

    // we will remove all the saved state stack in endRecording anyway, so it makes no difference
    while (srcCanvas->getSaveCount() > m_initialSaveCount) {
        state.m_ctm = srcCanvas->getTotalMatrix();
        // FIXME: don't mess up the state in case we will have to fallback, crbug.com/408392
        if (!srcCanvas->getClipDeviceBounds(&state.m_clip))
            return false;
        stateStack->push(state);
        srcCanvas->restore();
    }

    state.m_ctm = srcCanvas->getTotalMatrix();
    // FIXME: don't mess up the state in case we will have to fallback, crbug.com/408392
    if (!srcCanvas->getClipDeviceBounds(&state.m_clip))
        return false;
    stateStack->push(state);

    return true;
}

void RecordingImageBufferSurface::setCurrentState(SkCanvas* dstCanvas, StateStack* stateStack)
{
    while (stateStack->size() > 1) {
        dstCanvas->resetMatrix();
        dstCanvas->clipRect(SkRect::MakeFromIRect(stateStack->peek().m_clip));
        dstCanvas->setMatrix(stateStack->peek().m_ctm);
        dstCanvas->save();
        stateStack->pop();
    }

    dstCanvas->resetMatrix();
    dstCanvas->clipRect(SkRect::MakeFromIRect(stateStack->peek().m_clip));
    dstCanvas->setMatrix(stateStack->peek().m_ctm);
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
