/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/Canvas2DLayerBridge.h"

#include "platform/TraceEvent.h"
#include "platform/graphics/CanvasMetrics.h"
#include "platform/graphics/ExpensiveCanvasHeuristicParameters.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/gpu/SharedContextRateLimiter.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebGraphicsContext3D.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebTraceLocation.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "wtf/RefCountedLeakCounter.h"

namespace {
enum {
    InvalidMailboxIndex = -1,
    MaxCanvasAnimationBacklog = 2, // Make sure the the GPU is never more than two animation frames behind.
};

#ifndef NDEBUG
WTF::RefCountedLeakCounter& canvas2DLayerBridgeInstanceCounter()
{
    DEFINE_STATIC_LOCAL(WTF::RefCountedLeakCounter, staticCanvas2DLayerBridgeInstanceCounter, ("Canvas2DLayerBridge"));
    return staticCanvas2DLayerBridgeInstanceCounter;
}
#endif

} // namespace

namespace blink {

static PassRefPtr<SkSurface> createSkSurface(GrContext* gr, const IntSize& size, int msaaSampleCount, OpacityMode opacityMode, bool* surfaceIsAccelerated)
{
    if (gr)
        gr->resetContext();

    SkAlphaType alphaType = (Opaque == opacityMode) ? kOpaque_SkAlphaType : kPremul_SkAlphaType;
    SkImageInfo info = SkImageInfo::MakeN32(size.width(), size.height(), alphaType);
    SkSurfaceProps disableLCDProps(0, kUnknown_SkPixelGeometry);
    RefPtr<SkSurface> surface;

    if (gr) {
        *surfaceIsAccelerated = true;
        surface = adoptRef(SkSurface::NewRenderTarget(gr, SkSurface::kNo_Budgeted, info, msaaSampleCount, Opaque == opacityMode ? 0 : &disableLCDProps));
    }

    if (!surface) {
        *surfaceIsAccelerated = false;
        surface = adoptRef(SkSurface::NewRaster(info, Opaque == opacityMode ? 0 : &disableLCDProps));
    }

    if (surface) {
        if (opacityMode == Opaque) {
            surface->getCanvas()->clear(SK_ColorBLACK);
        } else {
            surface->getCanvas()->clear(SK_ColorTRANSPARENT);
        }
    }
    return surface;
}

PassRefPtr<Canvas2DLayerBridge> Canvas2DLayerBridge::create(const IntSize& size, int msaaSampleCount, OpacityMode opacityMode, AccelerationMode accelerationMode)
{
    TRACE_EVENT_INSTANT0("test_gpu", "Canvas2DLayerBridgeCreation", TRACE_EVENT_SCOPE_GLOBAL);
    OwnPtr<WebGraphicsContext3DProvider> contextProvider = adoptPtr(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (!contextProvider)
        return nullptr;
    RefPtr<Canvas2DLayerBridge> layerBridge;
    layerBridge = adoptRef(new Canvas2DLayerBridge(contextProvider.release(), size, msaaSampleCount, opacityMode, accelerationMode));
    return layerBridge.release();
}

Canvas2DLayerBridge::Canvas2DLayerBridge(PassOwnPtr<WebGraphicsContext3DProvider> contextProvider, const IntSize& size, int msaaSampleCount, OpacityMode opacityMode, AccelerationMode accelerationMode)
    : m_contextProvider(std::move(contextProvider))
    , m_logger(adoptPtr(new Logger))
    , m_weakPtrFactory(this)
    , m_imageBuffer(0)
    , m_msaaSampleCount(msaaSampleCount)
    , m_bytesAllocated(0)
    , m_haveRecordedDrawCommands(false)
    , m_destructionInProgress(false)
    , m_filterQuality(kLow_SkFilterQuality)
    , m_isHidden(false)
    , m_isDeferralEnabled(true)
    , m_isRegisteredTaskObserver(false)
    , m_renderingTaskCompletedForCurrentFrame(false)
    , m_softwareRenderingWhileHidden(false)
    , m_lastImageId(0)
    , m_lastFilter(GL_LINEAR)
    , m_accelerationMode(accelerationMode)
    , m_opacityMode(opacityMode)
    , m_size(size)
{
    ASSERT(m_contextProvider);
    // Used by browser tests to detect the use of a Canvas2DLayerBridge.
    TRACE_EVENT_INSTANT0("test_gpu", "Canvas2DLayerBridgeCreation", TRACE_EVENT_SCOPE_GLOBAL);
    startRecording();
#ifndef NDEBUG
    canvas2DLayerBridgeInstanceCounter().increment();
#endif
}

Canvas2DLayerBridge::~Canvas2DLayerBridge()
{
    ASSERT(m_destructionInProgress);
    m_layer.clear();
    ASSERT(m_mailboxes.size() == 0);
#ifndef NDEBUG
    canvas2DLayerBridgeInstanceCounter().decrement();
#endif
}

void Canvas2DLayerBridge::startRecording()
{
    ASSERT(m_isDeferralEnabled);
    m_recorder = adoptPtr(new SkPictureRecorder);
    m_recorder->beginRecording(m_size.width(), m_size.height(), nullptr);
    if (m_imageBuffer) {
        m_imageBuffer->resetCanvas(m_recorder->getRecordingCanvas());
    }
    m_recordingPixelCount = 0;
}

void Canvas2DLayerBridge::setLoggerForTesting(PassOwnPtr<Logger> logger)
{
    m_logger = std::move(logger);
}

bool Canvas2DLayerBridge::shouldAccelerate(AccelerationHint hint) const
{
    bool accelerate;
    if (m_softwareRenderingWhileHidden)
        accelerate = false;
    else if (m_accelerationMode == ForceAccelerationForTesting)
        accelerate = true;
    else if (m_accelerationMode == DisableAcceleration)
        accelerate = false;
    else
        accelerate = hint == PreferAcceleration || hint == PreferAccelerationAfterVisibilityChange;

    if (accelerate && (!m_contextProvider || m_contextProvider->context3d()->isContextLost()))
        accelerate = false;
    return accelerate;
}

bool Canvas2DLayerBridge::isAccelerated() const
{
    if (isHibernating())
        return false;
    if (m_softwareRenderingWhileHidden)
        return false;
    if (m_layer) // We don't check m_surface, so this returns true if context was lost (m_surface is null) with restoration pending.
        return true;
    if (m_surface) // && !m_layer is implied
        return false;

    // Whether or not to accelerate is not yet resolved, determine whether immediate presentation
    // of the canvas would result in the canvas being accelerated. Presentation is assumed to be
    // a 'PreferAcceleration' operation.
    return shouldAccelerate(PreferAcceleration);
}

static void hibernateWrapper(WeakPtr<Canvas2DLayerBridge> bridge, double /*idleDeadline*/)
{
    if (bridge) {
        bridge->hibernate();
    } else {
        Canvas2DLayerBridge::Logger localLogger;
        localLogger.reportHibernationEvent(Canvas2DLayerBridge::HibernationAbortedDueToDestructionWhileHibernatePending);
    }
}

void Canvas2DLayerBridge::hibernate()
{
    ASSERT(!isHibernating());

    if (m_destructionInProgress) {
        m_logger->reportHibernationEvent(HibernationAbortedDueToPendingDestruction);
        return;
    }

    if (!m_surface) {
        m_logger->reportHibernationEvent(HibernationAbortedBecauseNoSurface);
        return;
    }

    if (!isHidden()) {
        m_logger->reportHibernationEvent(HibernationAbortedDueToVisibilityChange);
        return;
    }

    if (!checkSurfaceValid()) {
        m_logger->reportHibernationEvent(HibernationAbortedDueGpuContextLoss);
        return;
    }

    if (!isAccelerated()) {
        m_logger->reportHibernationEvent(HibernationAbortedDueToSwitchToUnacceleratedRendering);
        return;
    }

    TRACE_EVENT0("cc", "Canvas2DLayerBridge::hibernate");
    RefPtr<SkSurface> tempHibernationSurface = adoptRef(SkSurface::NewRasterN32Premul(m_size.width(), m_size.height()));
    if (!tempHibernationSurface) {
        m_logger->reportHibernationEvent(HibernationAbortedDueToAllocationFailure);
        return;
    }
    // No HibernationEvent reported on success. This is on purppose to avoid
    // non-complementary stats. Each HibernationScheduled event is paired with
    // exactly one failure or exit event.
    flushRecordingOnly();
    // The following checks that the flush succeeded, which should always be the
    // case because flushRecordingOnly should only fail it it fails to allocate
    // a surface, and we have an early exit at the top of this function for when
    // 'this' does not already have a surface.
    ASSERT(!m_haveRecordedDrawCommands);
    SkPaint copyPaint;
    copyPaint.setXfermodeMode(SkXfermode::kSrc_Mode);
    m_surface->draw(tempHibernationSurface->getCanvas(), 0, 0, &copyPaint); // GPU readback
    m_hibernationImage = adoptRef(tempHibernationSurface->newImageSnapshot());
    m_surface.clear(); // destroy the GPU-backed buffer
    m_layer->clearTexture();
    m_logger->didStartHibernating();

}

void Canvas2DLayerBridge::reportSurfaceCreationFailure()
{
    if (!m_surfaceCreationFailedAtLeastOnce) {
        // Only count the failure once per instance so that the histogram may
        // reflect the proportion of Canvas2DLayerBridge instances with surface
        // allocation failures.
        CanvasMetrics::countCanvasContextUsage(CanvasMetrics::GPUAccelerated2DCanvasSurfaceCreationFailed);
        m_surfaceCreationFailedAtLeastOnce = true;
    }
}

SkSurface* Canvas2DLayerBridge::getOrCreateSurface(AccelerationHint hint)
{
    if (m_surface)
        return m_surface.get();

    if (m_layer && !isHibernating() && hint == PreferAcceleration) {
        return nullptr; // re-creation will happen through restore()
    }

    bool wantAcceleration = shouldAccelerate(hint);
    bool surfaceIsAccelerated;

    if (isHidden() && wantAcceleration) {
        wantAcceleration = false;
        m_softwareRenderingWhileHidden = true;
    }

    m_surface = createSkSurface(wantAcceleration ? m_contextProvider->grContext() : nullptr, m_size, m_msaaSampleCount, m_opacityMode, &surfaceIsAccelerated);

    if (!m_surface)
        reportSurfaceCreationFailure();

    if (m_surface && surfaceIsAccelerated && !m_layer) {
        m_layer = adoptPtr(Platform::current()->compositorSupport()->createExternalTextureLayer(this));
        m_layer->setOpaque(m_opacityMode == Opaque);
        m_layer->setBlendBackgroundColor(m_opacityMode != Opaque);
        GraphicsLayer::registerContentsLayer(m_layer->layer());
        m_layer->setNearestNeighbor(m_filterQuality == kNone_SkFilterQuality);
    }

    if (m_surface && isHibernating()) {
        if (surfaceIsAccelerated) {
            m_logger->reportHibernationEvent(HibernationEndedNormally);
        } else {
            if (isHidden())
                m_logger->reportHibernationEvent(HibernationEndedWithSwitchToBackgroundRendering);
            else
                m_logger->reportHibernationEvent(HibernationEndedWithFallbackToSW);
        }

        SkPaint copyPaint;
        copyPaint.setXfermodeMode(SkXfermode::kSrc_Mode);
        m_surface->getCanvas()->drawImage(m_hibernationImage.get(), 0, 0, &copyPaint);
        m_hibernationImage.clear();

        if (m_imageBuffer)
            m_imageBuffer->updateGPUMemoryUsage();

        if (m_imageBuffer && !m_isDeferralEnabled)
            m_imageBuffer->resetCanvas(m_surface->getCanvas());
    }

    return m_surface.get();
}

SkCanvas* Canvas2DLayerBridge::canvas()
{
    if (!m_isDeferralEnabled) {
        SkSurface* s = getOrCreateSurface();
        return s ? s->getCanvas() : nullptr;
    }
    return m_recorder->getRecordingCanvas();
}

void Canvas2DLayerBridge::disableDeferral()
{
    // Disabling deferral is permanent: once triggered by disableDeferral()
    // we stay in immediate mode indefinitely. This is a performance heuristic
    // that significantly helps a number of use cases. The rationale is that if
    // immediate rendering was needed once, it is likely to be needed at least
    // once per frame, which eliminates the possibility for inter-frame
    // overdraw optimization. Furthermore, in cases where immediate mode is
    // required multiple times per frame, the repeated flushing of deferred
    // commands would cause significant overhead, so it is better to just stop
    // trying to defer altogether.
    if (!m_isDeferralEnabled)
        return;

    CanvasMetrics::countCanvasContextUsage(CanvasMetrics::GPUAccelerated2DCanvasDeferralDisabled);
    flushRecordingOnly();
    // Because we will be discarding the recorder, if the flush failed
    // content will be lost -> force m_haveRecordedDrawCommands to false
    m_haveRecordedDrawCommands = false;

    m_isDeferralEnabled = false;
    m_recorder.clear();
    // install the current matrix/clip stack onto the immediate canvas
    SkSurface* surface = getOrCreateSurface();
    if (m_imageBuffer && surface)
        m_imageBuffer->resetCanvas(surface->getCanvas());
}

void Canvas2DLayerBridge::setImageBuffer(ImageBuffer* imageBuffer)
{
    m_imageBuffer = imageBuffer;
    if (m_imageBuffer && m_isDeferralEnabled) {
        m_imageBuffer->resetCanvas(m_recorder->getRecordingCanvas());
    }
}

void Canvas2DLayerBridge::beginDestruction()
{
    if (m_destructionInProgress)
        return;
    if (isHibernating())
        m_logger->reportHibernationEvent(HibernationEndedWithTeardown);
    m_hibernationImage.clear();
    m_recorder.clear();
    m_imageBuffer = nullptr;
    m_destructionInProgress = true;
    setIsHidden(true);
    m_surface.clear();

    unregisterTaskObserver();

    if (m_layer) {
        GraphicsLayer::unregisterContentsLayer(m_layer->layer());
        m_layer->clearTexture();
        // Orphaning the layer is required to trigger the recration of a new layer
        // in the case where destruction is caused by a canvas resize. Test:
        // virtual/gpu/fast/canvas/canvas-resize-after-paint-without-layout.html
        m_layer->layer()->removeFromParent();
    }

    ASSERT(!m_bytesAllocated);
}

void Canvas2DLayerBridge::unregisterTaskObserver()
{
    if (m_isRegisteredTaskObserver) {
        Platform::current()->currentThread()->removeTaskObserver(this);
        m_isRegisteredTaskObserver = false;
    }
}

void Canvas2DLayerBridge::setFilterQuality(SkFilterQuality filterQuality)
{
    ASSERT(!m_destructionInProgress);
    m_filterQuality = filterQuality;
    if (m_layer)
        m_layer->setNearestNeighbor(m_filterQuality == kNone_SkFilterQuality);
}

void Canvas2DLayerBridge::setIsHidden(bool hidden)
{
    bool newHiddenValue = hidden || m_destructionInProgress;
    if (m_isHidden == newHiddenValue)
        return;

    m_isHidden = newHiddenValue;
    if (m_surface && isHidden() && !m_destructionInProgress) {
        if (m_layer)
            m_layer->clearTexture();
        m_logger->reportHibernationEvent(HibernationScheduled);
        Platform::current()->currentThread()->scheduler()->postIdleTask(BLINK_FROM_HERE, WTF::bind<double>(&hibernateWrapper, m_weakPtrFactory.createWeakPtr()));
    }
    if (!isHidden() && m_softwareRenderingWhileHidden) {
        flushRecordingOnly();
        SkPaint copyPaint;
        copyPaint.setXfermodeMode(SkXfermode::kSrc_Mode);
        RefPtr<SkSurface> oldSurface = m_surface.release();
        m_softwareRenderingWhileHidden = false;
        SkSurface* newSurface = getOrCreateSurface(PreferAccelerationAfterVisibilityChange);
        if (newSurface) {
            if (oldSurface)
                oldSurface->draw(newSurface->getCanvas(), 0, 0, &copyPaint);
            if (m_imageBuffer && !m_isDeferralEnabled) {
                m_imageBuffer->resetCanvas(m_surface->getCanvas());
            }
        }
    }
    if (!isHidden() && isHibernating()) {
        getOrCreateSurface(); // Rude awakening
    }
}

bool Canvas2DLayerBridge::writePixels(const SkImageInfo& origInfo, const void* pixels, size_t rowBytes, int x, int y)
{
    if (!getOrCreateSurface())
        return false;
    if (x <= 0 && y <= 0 && x + origInfo.width() >= m_size.width() && y + origInfo.height() >= m_size.height()) {
        skipQueuedDrawCommands();
    } else {
        flush();
    }
    ASSERT(!m_haveRecordedDrawCommands);
    // call write pixels on the surface, not the recording canvas.
    // No need to call beginDirectSurfaceAccessModeIfNeeded() because writePixels
    // ignores the matrix and clip state.
    return getOrCreateSurface()->getCanvas()->writePixels(origInfo, pixels, rowBytes, x, y);
}

void Canvas2DLayerBridge::skipQueuedDrawCommands()
{
    if (m_haveRecordedDrawCommands) {
        adoptRef(m_recorder->endRecording());
        startRecording();
        m_haveRecordedDrawCommands = false;
    }

    if (m_isDeferralEnabled) {
        unregisterTaskObserver();
        if (m_rateLimiter)
            m_rateLimiter->reset();
    }
}

void Canvas2DLayerBridge::flushRecordingOnly()
{
    ASSERT(!m_destructionInProgress);

    if (m_haveRecordedDrawCommands && getOrCreateSurface()) {
        TRACE_EVENT0("cc", "Canvas2DLayerBridge::flushRecordingOnly");
        RefPtr<SkPicture> picture = adoptRef(m_recorder->endRecording());
        picture->playback(getOrCreateSurface()->getCanvas());
        if (m_isDeferralEnabled)
            startRecording();
        m_haveRecordedDrawCommands = false;
    }
}

void Canvas2DLayerBridge::flush()
{
    if (!getOrCreateSurface())
        return;
    TRACE_EVENT0("cc", "Canvas2DLayerBridge::flush");
    flushRecordingOnly();
    getOrCreateSurface()->getCanvas()->flush();
}

void Canvas2DLayerBridge::flushGpu()
{
    TRACE_EVENT0("cc", "Canvas2DLayerBridge::flushGpu");
    flush();
    WebGraphicsContext3D* webContext = context();
    if (isAccelerated() && webContext)
        webContext->flush();
}


WebGraphicsContext3D* Canvas2DLayerBridge::context()
{
    // Check on m_layer is necessary because context() may be called during
    // the destruction of m_layer
    if (m_layer && !m_destructionInProgress)
        checkSurfaceValid(); // To ensure rate limiter is disabled if context is lost.
    return m_contextProvider ? m_contextProvider->context3d() : 0;
}

bool Canvas2DLayerBridge::checkSurfaceValid()
{
    ASSERT(!m_destructionInProgress);
    if (m_destructionInProgress)
        return false;
    if (isHibernating())
        return true;
    if (!m_layer)
        return true;
    if (!m_surface)
        return false;
    if (m_contextProvider->context3d()->isContextLost()) {
        m_surface.clear();
        for (auto mailboxInfo = m_mailboxes.begin(); mailboxInfo != m_mailboxes.end(); ++mailboxInfo) {
            if (mailboxInfo->m_image)
                mailboxInfo->m_image.clear();
        }
        if (m_imageBuffer)
            m_imageBuffer->notifySurfaceInvalid();
        CanvasMetrics::countCanvasContextUsage(CanvasMetrics::Accelerated2DCanvasGPUContextLost);
    }
    return m_surface;
}

bool Canvas2DLayerBridge::restoreSurface()
{
    ASSERT(!m_destructionInProgress);
    if (m_destructionInProgress)
        return false;
    ASSERT(isAccelerated() && !m_surface);

    WebGraphicsContext3D* sharedContext = 0;
    m_layer->clearTexture();
    m_contextProvider = adoptPtr(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (m_contextProvider)
        sharedContext = m_contextProvider->context3d();

    if (sharedContext && !sharedContext->isContextLost()) {
        GrContext* grCtx = m_contextProvider->grContext();
        bool surfaceIsAccelerated;
        RefPtr<SkSurface> surface(createSkSurface(grCtx, m_size, m_msaaSampleCount, m_opacityMode, &surfaceIsAccelerated));

        if (!m_surface)
            reportSurfaceCreationFailure();

        // Current paradigm does support switching from accelerated to non-accelerated, which would be tricky
        // due to changes to the layer tree, which can only happen at specific times during the document lifecycle.
        // Therefore, we can only accept the restored surface if it is accelerated.
        if (surface && surfaceIsAccelerated) {
            m_surface = surface.release();
            // FIXME: draw sad canvas picture into new buffer crbug.com/243842
        }
    }
    if (m_imageBuffer)
        m_imageBuffer->updateGPUMemoryUsage();

    return m_surface;
}

bool Canvas2DLayerBridge::prepareMailbox(WebExternalTextureMailbox* outMailbox, WebExternalBitmap* bitmap)
{
    if (m_destructionInProgress) {
        // It can be hit in the following sequence.
        // 1. Canvas draws something.
        // 2. The compositor begins the frame.
        // 3. Javascript makes a context be lost.
        // 4. Here.
        return false;
    }
    ASSERT(isAccelerated() || isHibernating() || m_softwareRenderingWhileHidden);

    // if hibernating but not hidden, we want to wake up from
    // hibernation
    if ((isHibernating() || m_softwareRenderingWhileHidden) && isHidden())
        return false;

    if (bitmap) {
        // Using accelerated 2d canvas with software renderer, which
        // should only happen in tests that use fake graphics contexts
        // or in Android WebView in software mode. In this case, we do
        // not care about producing any results for this canvas.
        skipQueuedDrawCommands();
        m_lastImageId = 0;
        return false;
    }

    RefPtr<SkImage> image = newImageSnapshot(PreferAcceleration);
    if (!image || !image->getTexture())
        return false;

    WebGraphicsContext3D* webContext = context();

    // Early exit if canvas was not drawn to since last prepareMailbox
    GLenum filter = m_filterQuality == kNone_SkFilterQuality ? GL_NEAREST : GL_LINEAR;
    if (image->uniqueID() == m_lastImageId && filter == m_lastFilter)
        return false;
    m_lastImageId = image->uniqueID();
    m_lastFilter = filter;

    {
        MailboxInfo tmp;
        tmp.m_image = image;
        tmp.m_parentLayerBridge = this;
        m_mailboxes.prepend(tmp);
    }
    MailboxInfo& mailboxInfo = m_mailboxes.first();

    mailboxInfo.m_mailbox.nearestNeighbor = filter == GL_NEAREST;

    GrContext* grContext = m_contextProvider->grContext();
    if (!grContext)
        return true; // for testing: skip gl stuff when using a mock graphics context.

    // Need to flush skia's internal queue because texture is about to be accessed directly
    grContext->flush();

    // Because of texture sharing with the compositor, we must invalidate
    // the state cached in skia so that the deferred copy on write
    // in SkSurface_Gpu does not make any false assumptions.
    mailboxInfo.m_image->getTexture()->textureParamsModified();
    mailboxInfo.m_mailbox.textureTarget = GL_TEXTURE_2D;

    webContext->bindTexture(GL_TEXTURE_2D, mailboxInfo.m_image->getTexture()->getTextureHandle());
    webContext->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    webContext->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    webContext->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    webContext->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Re-use the texture's existing mailbox, if there is one.
    if (image->getTexture()->getCustomData()) {
        ASSERT(image->getTexture()->getCustomData()->size() == sizeof(mailboxInfo.m_mailbox.name));
        memcpy(&mailboxInfo.m_mailbox.name[0], image->getTexture()->getCustomData()->data(), sizeof(mailboxInfo.m_mailbox.name));
    } else {
        context()->genMailboxCHROMIUM(mailboxInfo.m_mailbox.name);
        RefPtr<SkData> mailboxNameData = adoptRef(SkData::NewWithCopy(&mailboxInfo.m_mailbox.name[0], sizeof(mailboxInfo.m_mailbox.name)));
        image->getTexture()->setCustomData(mailboxNameData.get());
        webContext->produceTextureCHROMIUM(GL_TEXTURE_2D, mailboxInfo.m_mailbox.name);
    }

    if (isHidden()) {
        // With hidden canvases, we release the SkImage immediately because
        // there is no need for animations to be double buffered.
        mailboxInfo.m_image.clear();
    } else {
        // FIXME: We'd rather insert a syncpoint than perform a flush here,
        // but currentlythe canvas will flicker if we don't flush here.
        webContext->flush();
        // mailboxInfo.m_mailbox.syncPoint = webContext->insertSyncPoint();
    }
    webContext->bindTexture(GL_TEXTURE_2D, 0);
    // Because we are changing the texture binding without going through skia,
    // we must dirty the context.
    grContext->resetContext(kTextureBinding_GrGLBackendState);

    *outMailbox = mailboxInfo.m_mailbox;
    return true;
}

void Canvas2DLayerBridge::mailboxReleased(const WebExternalTextureMailbox& mailbox, bool lostResource)
{
    ASSERT(isAccelerated() || isHibernating());
    bool contextLost = !isHibernating() && (!m_surface || m_contextProvider->context3d()->isContextLost());
    ASSERT(m_mailboxes.last().m_parentLayerBridge.get() == this);

    // Mailboxes are typically released in FIFO order, so we iterate
    // from the end of m_mailboxes.
    auto releasedMailboxInfo = m_mailboxes.end();
    auto firstMailbox = m_mailboxes.begin();

    while (true) {
        --releasedMailboxInfo;
        if (nameEquals(releasedMailboxInfo->m_mailbox, mailbox)) {
            break;
        }
        ASSERT(releasedMailboxInfo != firstMailbox);
    }

    if (!contextLost) {
        // Invalidate texture state in case the compositor altered it since the copy-on-write.
        if (releasedMailboxInfo->m_image) {
            if (mailbox.validSyncToken) {
                context()->waitSyncTokenCHROMIUM(mailbox.syncToken);
            }
            GrTexture* texture = releasedMailboxInfo->m_image->getTexture();
            if (texture) {
                if (lostResource) {
                    texture->abandon();
                } else {
                    texture->textureParamsModified();
                }
            }
        }
    }

    RefPtr<Canvas2DLayerBridge> selfRef;
    if (m_destructionInProgress) {
        // To avoid memory use after free, take a scoped self-reference
        // to postpone destruction until the end of this function.
        selfRef = this;
    }

    // The destruction of 'releasedMailboxInfo' will:
    // 1) Release the self reference held by the mailboxInfo, which may trigger
    //    the self-destruction of this Canvas2DLayerBridge
    // 2) Release the SkImage, which will return the texture to skia's scratch
    //    texture pool.
    m_mailboxes.remove(releasedMailboxInfo);
}

WebLayer* Canvas2DLayerBridge::layer() const
{
    ASSERT(!m_destructionInProgress);
    ASSERT(m_layer);
    return m_layer->layer();
}

void Canvas2DLayerBridge::didDraw(const FloatRect& rect)
{
    if (m_isDeferralEnabled) {
        m_haveRecordedDrawCommands = true;
        IntRect pixelBounds = enclosingIntRect(rect);
        m_recordingPixelCount += pixelBounds.width() * pixelBounds.height();
        if (m_recordingPixelCount >= (m_size.width() * m_size.height() * ExpensiveCanvasHeuristicParameters::ExpensiveOverdrawThreshold)) {
            disableDeferral();
        }
    }
    if (!m_isRegisteredTaskObserver) {
        Platform::current()->currentThread()->addTaskObserver(this);
        m_isRegisteredTaskObserver = true;
    }
}

void Canvas2DLayerBridge::prepareSurfaceForPaintingIfNeeded()
{
    getOrCreateSurface(PreferAcceleration);
}

void Canvas2DLayerBridge::finalizeFrame(const FloatRect &dirtyRect)
{
    ASSERT(!m_destructionInProgress);
    if (m_layer)
        m_layer->layer()->invalidateRect(enclosingIntRect(dirtyRect));
    if (m_rateLimiter)
        m_rateLimiter->reset();
    m_renderingTaskCompletedForCurrentFrame = false;
}

void Canvas2DLayerBridge::didProcessTask()
{
    TRACE_EVENT0("cc", "Canvas2DLayerBridge::didProcessTask");
    ASSERT(m_isRegisteredTaskObserver);
    // If m_renderTaskProcessedForCurrentFrame is already set to true,
    // it means that rendering tasks are not synchronized with the compositor
    // (i.e. not using requestAnimationFrame), so we are at risk of posting
    // a multi-frame backlog to the GPU
    if (m_renderingTaskCompletedForCurrentFrame) {
        if (isAccelerated()) {
            flushGpu();
            if (!m_rateLimiter) {
                m_rateLimiter = SharedContextRateLimiter::create(MaxCanvasAnimationBacklog);
            }
        } else {
            flush();
        }
    }

    if (m_rateLimiter) {
        m_rateLimiter->tick();
    }

    m_renderingTaskCompletedForCurrentFrame = true;
    unregisterTaskObserver();
}

void Canvas2DLayerBridge::willProcessTask()
{
    ASSERT_NOT_REACHED();
}

PassRefPtr<SkImage> Canvas2DLayerBridge::newImageSnapshot(AccelerationHint hint)
{
    if (!checkSurfaceValid())
        return nullptr;
    if (!getOrCreateSurface(hint))
        return nullptr;
    flush();
    // A readback operation may alter the texture parameters, which may affect
    // the compositor's behavior. Therefore, we must trigger copy-on-write
    // even though we are not technically writing to the texture, only to its
    // parameters.
    getOrCreateSurface()->notifyContentWillChange(SkSurface::kRetain_ContentChangeMode);
    return adoptRef(m_surface->newImageSnapshot());
}

void Canvas2DLayerBridge::willOverwriteCanvas()
{
    skipQueuedDrawCommands();
}

Canvas2DLayerBridge::MailboxInfo::MailboxInfo(const MailboxInfo& other)
{
    memcpy(&m_mailbox, &other.m_mailbox, sizeof(m_mailbox));
    m_image = other.m_image;
    m_parentLayerBridge = other.m_parentLayerBridge;
}

void Canvas2DLayerBridge::Logger::reportHibernationEvent(HibernationEvent event)
{
    blink::Platform::current()->histogramEnumeration("Canvas.HibernationEvents", event, HibernationEventCount);
}

} // namespace blink
