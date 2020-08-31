/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrRecordingContextPriv_DEFINED
#define GrRecordingContextPriv_DEFINED

#include "include/private/GrRecordingContext.h"

/** Class that exposes methods to GrRecordingContext that are only intended for use internal to
    Skia. This class is purely a privileged window into GrRecordingContext. It should never have
    additional data members or virtual methods. */
class GrRecordingContextPriv {
public:
    // from GrContext_Base
    uint32_t contextID() const { return fContext->contextID(); }

    bool matches(GrContext_Base* candidate) const { return fContext->matches(candidate); }

    const GrContextOptions& options() const { return fContext->options(); }

    const GrCaps* caps() const { return fContext->caps(); }
    sk_sp<const GrCaps> refCaps() const;

    GrImageContext* asImageContext() { return fContext->asImageContext(); }
    GrRecordingContext* asRecordingContext() { return fContext->asRecordingContext(); }
    GrContext* asDirectContext() { return fContext->asDirectContext(); }

    // from GrImageContext
    GrProxyProvider* proxyProvider() { return fContext->proxyProvider(); }
    const GrProxyProvider* proxyProvider() const { return fContext->proxyProvider(); }

    bool abandoned() const { return fContext->abandoned(); }

    /** This is only useful for debug purposes */
    SkDEBUGCODE(GrSingleOwner* singleOwner() const { return fContext->singleOwner(); } )

    // from GrRecordingContext
    GrDrawingManager* drawingManager() { return fContext->drawingManager(); }

    GrOpMemoryPool* opMemoryPool() { return fContext->arenas().opMemoryPool(); }
    SkArenaAlloc* recordTimeAllocator() { return fContext->arenas().recordTimeAllocator(); }
    GrRecordingContext::Arenas arenas() { return fContext->arenas(); }

    GrRecordingContext::OwnedArenas&& detachArenas() { return fContext->detachArenas(); }

    void recordProgramInfo(const GrProgramInfo* programInfo) {
        fContext->recordProgramInfo(programInfo);
    }

    void detachProgramData(SkTArray<GrRecordingContext::ProgramData>* dst) {
        fContext->detachProgramData(dst);
    }

    GrTextBlobCache* getTextBlobCache() { return fContext->getTextBlobCache(); }

    /**
     * Registers an object for flush-related callbacks. (See GrOnFlushCallbackObject.)
     *
     * NOTE: the drawing manager tracks this object as a raw pointer; it is up to the caller to
     * ensure its lifetime is tied to that of the context.
     */
    void addOnFlushCallbackObject(GrOnFlushCallbackObject*);

    GrAuditTrail* auditTrail() { return fContext->auditTrail(); }

    // CONTEXT TODO: remove this backdoor
    // In order to make progress we temporarily need a way to break CL impasses.
    GrContext* backdoor();

#if GR_TEST_UTILS
    // Used by tests that intentionally exercise codepaths that print warning messages, in order to
    // not confuse users with output that looks like a testing failure.
    class AutoSuppressWarningMessages {
    public:
        AutoSuppressWarningMessages(GrRecordingContext* context) : fContext(context) {
            ++fContext->fSuppressWarningMessages;
        }
        ~AutoSuppressWarningMessages() {
            --fContext->fSuppressWarningMessages;
        }
    private:
        GrRecordingContext* fContext;
    };
    void incrSuppressWarningMessages() { ++fContext->fSuppressWarningMessages; }
    void decrSuppressWarningMessages() { --fContext->fSuppressWarningMessages; }
#endif

    void printWarningMessage(const char* msg) const {
#if GR_TEST_UTILS
        if (fContext->fSuppressWarningMessages > 0) {
            return;
        }
#endif
        SkDebugf(msg);
    }

    GrRecordingContext::Stats* stats() {
        return &fContext->fStats;
    }

private:
    explicit GrRecordingContextPriv(GrRecordingContext* context) : fContext(context) {}
    GrRecordingContextPriv(const GrRecordingContextPriv&); // unimpl
    GrRecordingContextPriv& operator=(const GrRecordingContextPriv&); // unimpl

    // No taking addresses of this type.
    const GrRecordingContextPriv* operator&() const;
    GrRecordingContextPriv* operator&();

    GrRecordingContext* fContext;

    friend class GrRecordingContext; // to construct/copy this type.
};

inline GrRecordingContextPriv GrRecordingContext::priv() { return GrRecordingContextPriv(this); }

inline const GrRecordingContextPriv GrRecordingContext::priv () const {
    return GrRecordingContextPriv(const_cast<GrRecordingContext*>(this));
}

#endif
