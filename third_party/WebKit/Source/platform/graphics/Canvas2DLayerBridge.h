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

#ifndef Canvas2DLayerBridge_h
#define Canvas2DLayerBridge_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/ImageBufferSurface.h"
#include "public/platform/WebExternalTextureLayer.h"
#include "public/platform/WebExternalTextureLayerClient.h"
#include "public/platform/WebExternalTextureMailbox.h"
#include "public/platform/WebThread.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkImage.h"
#include "wtf/Allocator.h"
#include "wtf/Deque.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/WeakPtr.h"

class SkPictureRecorder;

namespace blink {

class Canvas2DLayerBridgeHistogramLogger;
class Canvas2DLayerBridgeTest;
class ImageBuffer;
class WebGraphicsContext3D;
class WebGraphicsContext3DProvider;
class SharedContextRateLimiter;

class PLATFORM_EXPORT Canvas2DLayerBridge : public WebExternalTextureLayerClient, public WebThread::TaskObserver, public RefCounted<Canvas2DLayerBridge> {
    WTF_MAKE_NONCOPYABLE(Canvas2DLayerBridge);
public:
    enum AccelerationMode {
        DisableAcceleration,
        EnableAcceleration,
        ForceAccelerationForTesting,
    };

    static PassRefPtr<Canvas2DLayerBridge> create(const IntSize&, int msaaSampleCount, OpacityMode, AccelerationMode);

    ~Canvas2DLayerBridge() override;

    // WebExternalTextureLayerClient implementation.
    bool prepareMailbox(WebExternalTextureMailbox*, WebExternalBitmap*) override;
    void mailboxReleased(const WebExternalTextureMailbox&, bool lostResource) override;

    // ImageBufferSurface implementation
    void finalizeFrame(const FloatRect &dirtyRect);
    void willWritePixels();
    void willOverwriteAllPixels();
    void willOverwriteCanvas();
    SkCanvas* canvas();
    void disableDeferral(DisableDeferralReason);
    bool checkSurfaceValid();
    bool restoreSurface();
    WebLayer* layer() const;
    bool isAccelerated() const;
    void setFilterQuality(SkFilterQuality);
    void setIsHidden(bool);
    void setImageBuffer(ImageBuffer*);
    void didDraw(const FloatRect&);
    bool writePixels(const SkImageInfo&, const void* pixels, size_t rowBytes, int x, int y);
    void flush();
    void flushGpu();
    void prepareSurfaceForPaintingIfNeeded();
    bool isHidden() { return m_isHidden; }

    void beginDestruction();
    void hibernate();
    bool isHibernating() const { return m_hibernationImage; }

    PassRefPtr<SkImage> newImageSnapshot(AccelerationHint, SnapshotReason);

    // The values of the enum entries must not change because they are used for
    // usage metrics histograms. New values can be added to the end.
    enum HibernationEvent {
        HibernationScheduled = 0,
        HibernationAbortedDueToDestructionWhileHibernatePending = 1,
        HibernationAbortedDueToPendingDestruction = 2,
        HibernationAbortedDueToVisibilityChange = 3,
        HibernationAbortedDueGpuContextLoss = 4,
        HibernationAbortedDueToSwitchToUnacceleratedRendering = 5,
        HibernationAbortedDueToAllocationFailure = 6,
        HibernationEndedNormally = 7,
        HibernationEndedWithSwitchToBackgroundRendering = 8,
        HibernationEndedWithFallbackToSW = 9,
        HibernationEndedWithTeardown = 10,
        HibernationAbortedBecauseNoSurface = 11,

        HibernationEventCount = 12,
    };

    class PLATFORM_EXPORT Logger {
    public:
        virtual void reportHibernationEvent(HibernationEvent);
        virtual void didStartHibernating() { }
        virtual ~Logger() { }
    };

    void setLoggerForTesting(PassOwnPtr<Logger>);

private:
    Canvas2DLayerBridge(PassOwnPtr<WebGraphicsContext3DProvider>, const IntSize&, int msaaSampleCount, OpacityMode, AccelerationMode);
    WebGraphicsContext3D* context();
    void startRecording();
    void skipQueuedDrawCommands();
    void flushRecordingOnly();
    void unregisterTaskObserver();
    void reportSurfaceCreationFailure();

    // WebThread::TaskOberver implementation
    void willProcessTask() override;
    void didProcessTask() override;

    SkSurface* getOrCreateSurface(AccelerationHint = PreferAcceleration);
    bool shouldAccelerate(AccelerationHint) const;

    OwnPtr<SkPictureRecorder> m_recorder;
    RefPtr<SkSurface> m_surface;
    RefPtr<SkImage> m_hibernationImage;
    int m_initialSurfaceSaveCount;
    OwnPtr<WebExternalTextureLayer> m_layer;
    OwnPtr<WebGraphicsContext3DProvider> m_contextProvider;
    OwnPtr<SharedContextRateLimiter> m_rateLimiter;
    OwnPtr<Logger> m_logger;
    WeakPtrFactory<Canvas2DLayerBridge> m_weakPtrFactory;
    ImageBuffer* m_imageBuffer;
    int m_msaaSampleCount;
    size_t m_bytesAllocated;
    bool m_haveRecordedDrawCommands;
    bool m_destructionInProgress;
    SkFilterQuality m_filterQuality;
    bool m_isHidden;
    bool m_isDeferralEnabled;
    bool m_isRegisteredTaskObserver;
    bool m_renderingTaskCompletedForCurrentFrame;
    bool m_softwareRenderingWhileHidden;
    bool m_surfaceCreationFailedAtLeastOnce = false;

    friend class Canvas2DLayerBridgeTest;

    struct MailboxInfo {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
        WebExternalTextureMailbox m_mailbox;
        RefPtr<SkImage> m_image;
        RefPtr<Canvas2DLayerBridge> m_parentLayerBridge;

        MailboxInfo(const MailboxInfo&);
        MailboxInfo() {}
    };

    uint32_t m_lastImageId;

    enum {
        // We should normally not have more that two active mailboxes at a time,
        // but sometime we may have three due to the async nature of mailbox handling.
        MaxActiveMailboxes = 3,
    };

    Deque<MailboxInfo, MaxActiveMailboxes> m_mailboxes;
    GLenum m_lastFilter;
    AccelerationMode m_accelerationMode;
    OpacityMode m_opacityMode;
    IntSize m_size;
    int m_recordingPixelCount;
};

} // namespace blink

#endif
