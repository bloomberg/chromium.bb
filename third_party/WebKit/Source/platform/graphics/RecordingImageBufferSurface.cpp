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

RecordingImageBufferSurface::RecordingImageBufferSurface(const IntSize& size, OpacityMode opacityMode)
    : ImageBufferSurface(size, opacityMode)
    , m_imageBuffer(0)
    , m_initialSaveCount(0)
    , m_frameWasCleared(true)
{
    initializeCurrentFrame();
}

RecordingImageBufferSurface::~RecordingImageBufferSurface()
{ }

void RecordingImageBufferSurface::initializeCurrentFrame()
{
    static SkRTreeFactory rTreeFactory;
    m_currentFrame = adoptPtr(new SkPictureRecorder);
    m_currentFrame->beginRecording(size().width(), size().height(), &rTreeFactory);
    m_initialSaveCount = m_currentFrame->getRecordingCanvas()->getSaveCount();
    if (m_imageBuffer) {
        m_imageBuffer->context()->resetCanvas(m_currentFrame->getRecordingCanvas());
        m_imageBuffer->context()->setRegionTrackingMode(GraphicsContext::RegionTrackingOverwrite);
    }
}

void RecordingImageBufferSurface::setImageBuffer(ImageBuffer* imageBuffer)
{
    m_imageBuffer = imageBuffer;
    if (m_currentFrame && m_imageBuffer) {
        m_imageBuffer->context()->setRegionTrackingMode(GraphicsContext::RegionTrackingOverwrite);
        m_imageBuffer->context()->resetCanvas(m_currentFrame->getRecordingCanvas());
    }
}

void RecordingImageBufferSurface::willAccessPixels()
{
    fallBackToRasterCanvas();
}

void RecordingImageBufferSurface::fallBackToRasterCanvas()
{
    if (m_rasterCanvas) {
        ASSERT(!m_currentFrame);
        return;
    }

    m_rasterCanvas = adoptPtr(SkCanvas::NewRasterN32(size().width(), size().height()));

    if (m_previousFrame) {
        m_previousFrame->draw(m_rasterCanvas.get());
        m_previousFrame.clear();
    }
    if (m_currentFrame) {
        RefPtr<SkPicture> currentPicture = adoptRef(m_currentFrame->endRecording());
        currentPicture->draw(m_rasterCanvas.get());
        m_currentFrame.clear();
    }

    if (m_imageBuffer) {
        m_imageBuffer->context()->setRegionTrackingMode(GraphicsContext::RegionTrackingDisabled);
        m_imageBuffer->context()->resetCanvas(m_rasterCanvas.get());
    }
}

SkCanvas* RecordingImageBufferSurface::canvas() const
{
    if (m_rasterCanvas)
        return m_rasterCanvas.get();

    ASSERT(m_currentFrame->getRecordingCanvas());
    return m_currentFrame->getRecordingCanvas();
}

PassRefPtr<SkPicture> RecordingImageBufferSurface::getPicture()
{
    bool canUsePicture = finalizeFrameInternal();
    m_imageBuffer->didFinalizeFrame();

    if (canUsePicture) {
        return m_previousFrame;
    }

    if (!m_rasterCanvas)
        fallBackToRasterCanvas();
    return nullptr;
}

void RecordingImageBufferSurface::finalizeFrame(const FloatRect &)
{
    if (!finalizeFrameInternal() && !m_rasterCanvas) {
        fallBackToRasterCanvas();
    }
}

void RecordingImageBufferSurface::didClearCanvas()
{
    m_frameWasCleared = true;
}

bool RecordingImageBufferSurface::finalizeFrameInternal()
{
    if (!m_imageBuffer->isDirty()) {
        if (m_currentFrame && !m_previousFrame) {
            // Create an initial blank frame
            m_previousFrame = adoptRef(m_currentFrame->endRecording());
            initializeCurrentFrame();
        }
        return m_currentFrame;
    }

    if (!m_currentFrame) {
        return false;
    }

    IntRect canvasRect(IntPoint(0, 0), size());
    if (!m_frameWasCleared && !m_imageBuffer->context()->opaqueRegion().asRect().contains(canvasRect)) {
        return false;
    }

    StateStack stateStack;

    if (!saveState(m_currentFrame->getRecordingCanvas(), &stateStack)) {
        return false;
    }

    m_previousFrame = adoptRef(m_currentFrame->endRecording());
    initializeCurrentFrame();

    setCurrentState(m_currentFrame->getRecordingCanvas(), &stateStack);

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

} // namespace blink
