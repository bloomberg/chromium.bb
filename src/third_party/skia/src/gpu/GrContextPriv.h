/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrContextPriv_DEFINED
#define GrContextPriv_DEFINED

#include "include/gpu/GrContext.h"

class GrAtlasManager;
class GrBackendFormat;
class GrBackendRenderTarget;
class GrOpMemoryPool;
class GrOnFlushCallbackObject;
class GrRenderTargetProxy;
class GrSemaphore;
class GrSurfaceProxy;

class SkDeferredDisplayList;
class SkTaskGroup;

/** Class that adds methods to GrContext that are only intended for use internal to Skia.
    This class is purely a privileged window into GrContext. It should never have additional
    data members or virtual methods. */
class GrContextPriv {
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

    GrStrikeCache* getGrStrikeCache() { return fContext->fStrikeCache.get(); }
    GrTextBlobCache* getTextBlobCache() { return fContext->getTextBlobCache(); }

    /**
     * Registers an object for flush-related callbacks. (See GrOnFlushCallbackObject.)
     *
     * NOTE: the drawing manager tracks this object as a raw pointer; it is up to the caller to
     * ensure its lifetime is tied to that of the context.
     */
    void addOnFlushCallbackObject(GrOnFlushCallbackObject*);

    GrAuditTrail* auditTrail() { return fContext->auditTrail(); }

    /**
     * Create a GrContext without a resource cache
     */
    static sk_sp<GrContext> MakeDDL(const sk_sp<GrContextThreadSafeProxy>&);

    /**
     * Finalizes all pending reads and writes to the surfaces and also performs an MSAA resolves
     * if necessary. The GrSurfaceProxy array is treated as a hint. If it is supplied the context
     * will guarantee that the draws required for those proxies are flushed but it could do more.
     * If no array is provided then all current work will be flushed.
     *
     * It is not necessary to call this before reading the render target via Skia/GrContext.
     * GrContext will detect when it must perform a resolve before reading pixels back from the
     * surface or using it as a texture.
     */
    GrSemaphoresSubmitted flushSurfaces(GrSurfaceProxy*[], int numProxies, const GrFlushInfo&);

    /** Version of above that flushes for a single proxy and uses a default GrFlushInfo. Null is
     * allowed. */
    void flushSurface(GrSurfaceProxy*);

    /**
     * Returns true if createPMToUPMEffect and createUPMToPMEffect will succeed. In other words,
     * did we find a pair of round-trip preserving conversion effects?
     */
    bool validPMUPMConversionExists();

    /**
     * These functions create premul <-> unpremul effects, using the specialized round-trip effects
     * from GrConfigConversionEffect.
     */
    std::unique_ptr<GrFragmentProcessor> createPMToUPMEffect(std::unique_ptr<GrFragmentProcessor>);
    std::unique_ptr<GrFragmentProcessor> createUPMToPMEffect(std::unique_ptr<GrFragmentProcessor>);

    SkTaskGroup* getTaskGroup() { return fContext->fTaskGroup.get(); }

    GrResourceProvider* resourceProvider() { return fContext->fResourceProvider; }
    const GrResourceProvider* resourceProvider() const { return fContext->fResourceProvider; }

    GrResourceCache* getResourceCache() { return fContext->fResourceCache; }

    GrGpu* getGpu() { return fContext->fGpu.get(); }
    const GrGpu* getGpu() const { return fContext->fGpu.get(); }

    // This accessor should only ever be called by the GrOpFlushState.
    GrAtlasManager* getAtlasManager() {
        return fContext->onGetAtlasManager();
    }

    void moveRenderTasksToDDL(SkDeferredDisplayList*);
    void copyRenderTasksFromDDL(const SkDeferredDisplayList*, GrRenderTargetProxy* newDest);

    bool compile(const GrProgramDesc&, const GrProgramInfo&);

    GrContextOptions::PersistentCache* getPersistentCache() { return fContext->fPersistentCache; }
    GrContextOptions::ShaderErrorHandler* getShaderErrorHandler() const {
        return fContext->fShaderErrorHandler;
    }

    GrClientMappedBufferManager* clientMappedBufferManager() {
        return fContext->fMappedBufferManager.get();
    }

#if GR_TEST_UTILS
    /** Reset GPU stats */
    void resetGpuStats() const;

    /** Prints cache stats to the string if GR_CACHE_STATS == 1. */
    void dumpCacheStats(SkString*) const;
    void dumpCacheStatsKeyValuePairs(SkTArray<SkString>* keys, SkTArray<double>* values) const;
    void printCacheStats() const;

    /** Prints GPU stats to the string if GR_GPU_STATS == 1. */
    void dumpGpuStats(SkString*) const;
    void dumpGpuStatsKeyValuePairs(SkTArray<SkString>* keys, SkTArray<double>* values) const;
    void printGpuStats() const;

    /** These are only active if GR_GPU_STATS == 1. */
    void resetContextStats() const;
    void dumpContextStats(SkString*) const;
    void dumpContextStatsKeyValuePairs(SkTArray<SkString>* keys, SkTArray<double>* values) const;
    void printContextStats() const;

    /** Specify the TextBlob cache limit. If the current cache exceeds this limit it will purge.
        this is for testing only */
    void testingOnly_setTextBlobCacheLimit(size_t bytes);

    /** Get pointer to atlas texture for given mask format. Note that this wraps an
        actively mutating texture in an SkImage. This could yield unexpected results
        if it gets cached or used more generally. */
    sk_sp<SkImage> testingOnly_getFontAtlasImage(GrMaskFormat format, unsigned int index = 0);

    /**
     * Purge all the unlocked resources from the cache.
     * This entry point is mainly meant for timing texture uploads
     * and is not defined in normal builds of Skia.
     */
    void testingOnly_purgeAllUnlockedResources();

    void testingOnly_flushAndRemoveOnFlushCallbackObject(GrOnFlushCallbackObject*);
#endif

private:
    explicit GrContextPriv(GrContext* context) : fContext(context) {}
    GrContextPriv(const GrContextPriv&); // unimpl
    GrContextPriv& operator=(const GrContextPriv&); // unimpl

    // No taking addresses of this type.
    const GrContextPriv* operator&() const;
    GrContextPriv* operator&();

    GrContext* fContext;

    friend class GrContext; // to construct/copy this type.
};

inline GrContextPriv GrContext::priv() { return GrContextPriv(this); }

inline const GrContextPriv GrContext::priv() const {
    return GrContextPriv(const_cast<GrContext*>(this));
}

#endif
