/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrGpu_DEFINED
#define GrGpu_DEFINED

#include "include/core/SkPath.h"
#include "include/core/SkSurface.h"
#include "include/gpu/GrTypes.h"
#include "include/private/SkTArray.h"
#include "src/core/SkSpan.h"
#include "src/core/SkTInternalLList.h"
#include "src/gpu/GrAttachment.h"
#include "src/gpu/GrCaps.h"
#include "src/gpu/GrOpsRenderPass.h"
#include "src/gpu/GrSamplePatternDictionary.h"
#include "src/gpu/GrSwizzle.h"
#include "src/gpu/GrTextureProducer.h"
#include "src/gpu/GrXferProcessor.h"

class GrBackendRenderTarget;
class GrBackendSemaphore;
class GrDirectContext;
class GrGpuBuffer;
struct GrContextOptions;
class GrGLContext;
class GrPath;
class GrPathRenderer;
class GrPathRendererChain;
class GrPathRendering;
class GrPipeline;
class GrPrimitiveProcessor;
class GrRenderTarget;
class GrRingBuffer;
class GrSemaphore;
class GrStagingBufferManager;
class GrAttachment;
class GrStencilSettings;
class GrSurface;
class GrTexture;
class SkJSONWriter;

class GrGpu : public SkRefCnt {
public:
    GrGpu(GrDirectContext* direct);
    ~GrGpu() override;

    GrDirectContext* getContext() { return fContext; }
    const GrDirectContext* getContext() const { return fContext; }

    /**
     * Gets the capabilities of the draw target.
     */
    const GrCaps* caps() const { return fCaps.get(); }
    sk_sp<const GrCaps> refCaps() const { return fCaps; }

    GrPathRendering* pathRendering() { return fPathRendering.get();  }

    virtual GrStagingBufferManager* stagingBufferManager() { return nullptr; }

    virtual GrRingBuffer* uniformsRingBuffer() { return nullptr; }

    enum class DisconnectType {
        // No cleanup should be attempted, immediately cease making backend API calls
        kAbandon,
        // Free allocated resources (not known by GrResourceCache) before returning and
        // ensure no backend backend 3D API calls will be made after disconnect() returns.
        kCleanup,
    };

    // Called by context when the underlying backend context is already or will be destroyed
    // before GrDirectContext.
    virtual void disconnect(DisconnectType);

    // Called by GrDirectContext::isContextLost. Returns true if the backend Gpu object has gotten
    // into an unrecoverable, lost state.
    virtual bool isDeviceLost() const { return false; }

    /**
     * The GrGpu object normally assumes that no outsider is setting state
     * within the underlying 3D API's context/device/whatever. This call informs
     * the GrGpu that the state was modified and it shouldn't make assumptions
     * about the state.
     */
    void markContextDirty(uint32_t state = kAll_GrBackendState) { fResetBits |= state; }

    /**
     * Creates a texture object. If renderable is kYes then the returned texture can
     * be used as a render target by calling GrTexture::asRenderTarget(). Not all
     * pixel configs can be used as render targets. Support for configs as textures
     * or render targets can be checked using GrCaps.
     *
     * @param dimensions     dimensions of the texture to be created.
     * @param format         the format for the texture (not currently used).
     * @param renderable     should the resulting texture be renderable
     * @param renderTargetSampleCnt The number of samples to use for rendering if renderable is
     *                       kYes. If renderable is kNo then this must be 1.
     * @param budgeted       does this texture count against the resource cache budget?
     * @param isProtected    should the texture be created as protected.
     * @param texels         array of mipmap levels containing texel data to load.
     *                       If level i has pixels then it is assumed that its dimensions are
     *                       max(1, floor(dimensions.fWidth / 2)) by
     *                       max(1, floor(dimensions.fHeight / 2)).
     *                       If texels[i].fPixels == nullptr for all i <= mipLevelCount or
     *                       mipLevelCount is 0 then the texture's contents are uninitialized.
     *                       If a level has non-null pixels, its row bytes must be a multiple of the
     *                       config's bytes-per-pixel. The row bytes must be tight to the
     *                       level width if !caps->writePixelsRowBytesSupport().
     *                       If mipLevelCount > 1 and texels[i].fPixels != nullptr for any i > 0
     *                       then all levels must have non-null pixels. All levels must have
     *                       non-null pixels if GrCaps::createTextureMustSpecifyAllLevels() is true.
     * @param textureColorType The color type interpretation of the texture for the purpose of
     *                       of uploading texel data.
     * @param srcColorType   The color type of data in texels[].
     * @param texelLevelCount the number of levels in 'texels'. May be 0, 1, or
     *                       floor(max((log2(dimensions.fWidth), log2(dimensions.fHeight)))). It
     *                       must be the latter if GrCaps::createTextureMustSpecifyAllLevels() is
     *                       true.
     * @return  The texture object if successful, otherwise nullptr.
     */
    sk_sp<GrTexture> createTexture(SkISize dimensions,
                                   const GrBackendFormat& format,
                                   GrRenderable renderable,
                                   int renderTargetSampleCnt,
                                   SkBudgeted budgeted,
                                   GrProtected isProtected,
                                   GrColorType textureColorType,
                                   GrColorType srcColorType,
                                   const GrMipLevel texels[],
                                   int texelLevelCount);

    /**
     * Simplified createTexture() interface for when there is no initial texel data to upload.
     */
    sk_sp<GrTexture> createTexture(SkISize dimensions,
                                   const GrBackendFormat& format,
                                   GrRenderable renderable,
                                   int renderTargetSampleCnt,
                                   GrMipmapped mipMapped,
                                   SkBudgeted budgeted,
                                   GrProtected isProtected);

    sk_sp<GrTexture> createCompressedTexture(SkISize dimensions,
                                             const GrBackendFormat& format,
                                             SkBudgeted budgeted,
                                             GrMipmapped mipMapped,
                                             GrProtected isProtected,
                                             const void* data, size_t dataSize);

    /**
     * Implements GrResourceProvider::wrapBackendTexture
     */
    sk_sp<GrTexture> wrapBackendTexture(const GrBackendTexture&,
                                        GrWrapOwnership,
                                        GrWrapCacheable,
                                        GrIOType);

    sk_sp<GrTexture> wrapCompressedBackendTexture(const GrBackendTexture&,
                                                  GrWrapOwnership,
                                                  GrWrapCacheable);

    /**
     * Implements GrResourceProvider::wrapRenderableBackendTexture
     */
    sk_sp<GrTexture> wrapRenderableBackendTexture(const GrBackendTexture&,
                                                  int sampleCnt,
                                                  GrWrapOwnership,
                                                  GrWrapCacheable);

    /**
     * Implements GrResourceProvider::wrapBackendRenderTarget
     */
    sk_sp<GrRenderTarget> wrapBackendRenderTarget(const GrBackendRenderTarget&);

    /**
     * Implements GrResourceProvider::wrapVulkanSecondaryCBAsRenderTarget
     */
    sk_sp<GrRenderTarget> wrapVulkanSecondaryCBAsRenderTarget(const SkImageInfo&,
                                                              const GrVkDrawableInfo&);

    /**
     * Creates a buffer in GPU memory. For a client-side buffer use GrBuffer::CreateCPUBacked.
     *
     * @param size            size of buffer to create.
     * @param intendedType    hint to the graphics subsystem about what the buffer will be used for.
     * @param accessPattern   hint to the graphics subsystem about how the data will be accessed.
     * @param data            optional data with which to initialize the buffer.
     *
     * @return the buffer if successful, otherwise nullptr.
     */
    sk_sp<GrGpuBuffer> createBuffer(size_t size, GrGpuBufferType intendedType,
                                    GrAccessPattern accessPattern, const void* data = nullptr);

    /**
     * Resolves MSAA. The resolveRect must already be in the native destination space.
     */
    void resolveRenderTarget(GrRenderTarget*, const SkIRect& resolveRect);

    /**
     * Uses the base of the texture to recompute the contents of the other levels.
     */
    bool regenerateMipMapLevels(GrTexture*);

    /**
     * If the backend API has stateful texture bindings, this resets them back to defaults.
     */
    void resetTextureBindings();

    /**
     * Reads a rectangle of pixels from a render target. No sRGB/linear conversions are performed.
     *
     * @param surface           The surface to read from
     * @param left              left edge of the rectangle to read (inclusive)
     * @param top               top edge of the rectangle to read (inclusive)
     * @param width             width of rectangle to read in pixels.
     * @param height            height of rectangle to read in pixels.
     * @param surfaceColorType  the color type for this use of the surface.
     * @param dstColorType      the color type of the destination buffer.
     * @param buffer            memory to read the rectangle into.
     * @param rowBytes          the number of bytes between consecutive rows. Must be a multiple of
     *                          dstColorType's bytes-per-pixel. Must be tight to width if
     *                          !caps->readPixelsRowBytesSupport().
     *
     * @return true if the read succeeded, false if not. The read can fail
     *              because of the surface doesn't support reading, the color type
     *              is not allowed for the format of the surface or if the rectangle
     *              read is not contained in the surface.
     */
    bool readPixels(GrSurface* surface, int left, int top, int width, int height,
                    GrColorType surfaceColorType, GrColorType dstColorType, void* buffer,
                    size_t rowBytes);

    /**
     * Updates the pixels in a rectangle of a surface.  No sRGB/linear conversions are performed.
     *
     * @param surface            The surface to write to.
     * @param left               left edge of the rectangle to write (inclusive)
     * @param top                top edge of the rectangle to write (inclusive)
     * @param width              width of rectangle to write in pixels.
     * @param height             height of rectangle to write in pixels.
     * @param surfaceColorType   the color type for this use of the surface.
     * @param srcColorType       the color type of the source buffer.
     * @param texels             array of mipmap levels containing texture data. Row bytes must be a
     *                           multiple of srcColorType's bytes-per-pixel. Must be tight to level
     *                           width if !caps->writePixelsRowBytesSupport().
     * @param mipLevelCount      number of levels in 'texels'
     * @param prepForTexSampling After doing write pixels should the surface be prepared for texture
     *                           sampling. This is currently only used by Vulkan for inline uploads
     *                           to set that layout back to sampled after doing the upload. Inline
     *                           uploads currently can happen between draws in a single op so it is
     *                           not trivial to break up the GrOpsTask into two tasks when we see
     *                           an inline upload. However, once we are able to support doing that
     *                           we can remove this parameter.
     *
     * @return true if the write succeeded, false if not. The read can fail
     *              because of the surface doesn't support writing (e.g. read only),
     *              the color type is not allowed for the format of the surface or
     *              if the rectangle written is not contained in the surface.
     */
    bool writePixels(GrSurface* surface, int left, int top, int width, int height,
                     GrColorType surfaceColorType, GrColorType srcColorType,
                     const GrMipLevel texels[], int mipLevelCount, bool prepForTexSampling = false);

    /**
     * Helper for the case of a single level.
     */
    bool writePixels(GrSurface* surface, int left, int top, int width, int height,
                     GrColorType surfaceColorType, GrColorType srcColorType, const void* buffer,
                     size_t rowBytes, bool prepForTexSampling = false) {
        GrMipLevel mipLevel = {buffer, rowBytes};
        return this->writePixels(surface, left, top, width, height, surfaceColorType, srcColorType,
                                 &mipLevel, 1, prepForTexSampling);
    }

    /**
     * Updates the pixels in a rectangle of a texture using a buffer. If the texture is MIP mapped,
     * the base level is written to.
     *
     * @param texture          The texture to write to.
     * @param left             left edge of the rectangle to write (inclusive)
     * @param top              top edge of the rectangle to write (inclusive)
     * @param width            width of rectangle to write in pixels.
     * @param height           height of rectangle to write in pixels.
     * @param textureColorType the color type for this use of the surface.
     * @param bufferColorType  the color type of the transfer buffer's pixel data
     * @param transferBuffer   GrBuffer to read pixels from (type must be "kXferCpuToGpu")
     * @param offset           offset from the start of the buffer
     * @param rowBytes         number of bytes between consecutive rows in the buffer. Must be a
     *                         multiple of bufferColorType's bytes-per-pixel. Must be tight to width
     *                         if !caps->writePixelsRowBytesSupport().
     */
    bool transferPixelsTo(GrTexture* texture, int left, int top, int width, int height,
                          GrColorType textureColorType, GrColorType bufferColorType,
                          GrGpuBuffer* transferBuffer, size_t offset, size_t rowBytes);

    /**
     * Reads the pixels from a rectangle of a surface into a buffer. Use
     * GrCaps::SupportedRead::fOffsetAlignmentForTransferBuffer to determine the requirements for
     * the buffer offset alignment. If the surface is a MIP mapped texture, the base level is read.
     *
     * If successful the row bytes in the buffer is always:
     *   GrColorTypeBytesPerPixel(bufferColorType) * width
     *
     * Asserts that the caller has passed a properly aligned offset and that the buffer is
     * large enough to hold the result
     *
     * @param surface          The surface to read from.
     * @param left             left edge of the rectangle to read (inclusive)
     * @param top              top edge of the rectangle to read (inclusive)
     * @param width            width of rectangle to read in pixels.
     * @param height           height of rectangle to read in pixels.
     * @param surfaceColorType the color type for this use of the surface.
     * @param bufferColorType  the color type of the transfer buffer's pixel data
     * @param transferBuffer   GrBuffer to write pixels to (type must be "kXferGpuToCpu")
     * @param offset           offset from the start of the buffer
     */
    bool transferPixelsFrom(GrSurface* surface, int left, int top, int width, int height,
                            GrColorType surfaceColorType, GrColorType bufferColorType,
                            GrGpuBuffer* transferBuffer, size_t offset);

    // Called to perform a surface to surface copy. Fallbacks to issuing a draw from the src to dst
    // take place at higher levels and this function implement faster copy paths. The rect
    // and point are pre-clipped. The src rect and implied dst rect are guaranteed to be within the
    // src/dst bounds and non-empty. They must also be in their exact device space coords, including
    // already being transformed for origin if need be. If canDiscardOutsideDstRect is set to true
    // then we don't need to preserve any data on the dst surface outside of the copy.
    bool copySurface(GrSurface* dst, GrSurface* src, const SkIRect& srcRect,
                     const SkIPoint& dstPoint);

    // Queries the per-pixel HW sample locations for the given render target, and then finds or
    // assigns a key that uniquely identifies the sample pattern. The actual sample locations can be
    // retrieved with retrieveSampleLocations().
    int findOrAssignSamplePatternKey(GrRenderTarget*);

    // Retrieves the per-pixel HW sample locations for the given sample pattern key, and, as a
    // by-product, the actual number of samples in use. (This may differ from the number of samples
    // requested by the render target.) Sample locations are returned as 0..1 offsets relative to
    // the top-left corner of the pixel.
    const SkTArray<SkPoint>& retrieveSampleLocations(int samplePatternKey) const {
        return fSamplePatternDictionary.retrieveSampleLocations(samplePatternKey);
    }

    // Returns a GrOpsRenderPass which GrOpsTasks send draw commands to instead of directly
    // to the Gpu object. The 'bounds' rect is the content rect of the renderTarget.
    // If a 'stencil' is provided it will be the one bound to 'renderTarget'. If one is not
    // provided but 'renderTarget' has a stencil buffer then that is a signal that the
    // render target's stencil buffer should be ignored.
    GrOpsRenderPass* getOpsRenderPass(GrRenderTarget* renderTarget,
                                      GrAttachment* stencil,
                                      GrSurfaceOrigin,
                                      const SkIRect& bounds,
                                      const GrOpsRenderPass::LoadAndStoreInfo&,
                                      const GrOpsRenderPass::StencilLoadAndStoreInfo&,
                                      const SkTArray<GrSurfaceProxy*, true>& sampledProxies,
                                      GrXferBarrierFlags renderPassXferBarriers);

    // Called by GrDrawingManager when flushing.
    // Provides a hook for post-flush actions (e.g. Vulkan command buffer submits). This will also
    // insert any numSemaphore semaphores on the gpu and set the backendSemaphores to match the
    // inserted semaphores.
    void executeFlushInfo(SkSpan<GrSurfaceProxy*>,
                          SkSurface::BackendSurfaceAccess access,
                          const GrFlushInfo&,
                          const GrBackendSurfaceMutableState* newState);

    bool submitToGpu(bool syncCpu);

    virtual void submit(GrOpsRenderPass*) = 0;

    virtual GrFence SK_WARN_UNUSED_RESULT insertFence() = 0;
    virtual bool waitFence(GrFence) = 0;
    virtual void deleteFence(GrFence) const = 0;

    virtual std::unique_ptr<GrSemaphore> SK_WARN_UNUSED_RESULT makeSemaphore(
            bool isOwned = true) = 0;
    virtual std::unique_ptr<GrSemaphore> wrapBackendSemaphore(const GrBackendSemaphore& semaphore,
            GrResourceProvider::SemaphoreWrapType wrapType, GrWrapOwnership ownership) = 0;
    virtual void insertSemaphore(GrSemaphore* semaphore) = 0;
    virtual void waitSemaphore(GrSemaphore* semaphore) = 0;

    virtual void addFinishedProc(GrGpuFinishedProc finishedProc,
                                 GrGpuFinishedContext finishedContext) = 0;
    virtual void checkFinishProcs() = 0;

    virtual void takeOwnershipOfBuffer(sk_sp<GrGpuBuffer>) {}

    /**
     * Checks if we detected an OOM from the underlying 3D API and if so returns true and resets
     * the internal OOM state to false. Otherwise, returns false.
     */
    bool checkAndResetOOMed();

    /**
     *  Put this texture in a safe and known state for use across multiple contexts. Depending on
     *  the backend, this may return a GrSemaphore. If so, other contexts should wait on that
     *  semaphore before using this texture.
     */
    virtual std::unique_ptr<GrSemaphore> prepareTextureForCrossContextUsage(GrTexture*) = 0;

    ///////////////////////////////////////////////////////////////////////////
    // Debugging and Stats

    class Stats {
    public:
        enum class ProgramCacheResult {
            kHit,       // the program was found in the cache
            kMiss,      // the program was not found in the cache (and was, thus, compiled)
            kPartial,   // a precompiled version was found in the persistent cache

            kLast = kPartial
        };

        static const int kNumProgramCacheResults = (int)ProgramCacheResult::kLast + 1;

#if GR_GPU_STATS
        Stats() = default;

        void reset() { *this = {}; }

        int renderTargetBinds() const { return fRenderTargetBinds; }
        void incRenderTargetBinds() { fRenderTargetBinds++; }

        int shaderCompilations() const { return fShaderCompilations; }
        void incShaderCompilations() { fShaderCompilations++; }

        int textureCreates() const { return fTextureCreates; }
        void incTextureCreates() { fTextureCreates++; }

        int textureUploads() const { return fTextureUploads; }
        void incTextureUploads() { fTextureUploads++; }

        int transfersToTexture() const { return fTransfersToTexture; }
        void incTransfersToTexture() { fTransfersToTexture++; }

        int transfersFromSurface() const { return fTransfersFromSurface; }
        void incTransfersFromSurface() { fTransfersFromSurface++; }

        int stencilAttachmentCreates() const { return fStencilAttachmentCreates; }
        void incStencilAttachmentCreates() { fStencilAttachmentCreates++; }

        int msaaAttachmentCreates() const { return fMSAAAttachmentCreates; }
        void incMSAAAttachmentCreates() { fMSAAAttachmentCreates++; }

        int numDraws() const { return fNumDraws; }
        void incNumDraws() { fNumDraws++; }

        int numFailedDraws() const { return fNumFailedDraws; }
        void incNumFailedDraws() { ++fNumFailedDraws; }

        int numSubmitToGpus() const { return fNumSubmitToGpus; }
        void incNumSubmitToGpus() { ++fNumSubmitToGpus; }

        int numScratchTexturesReused() const { return fNumScratchTexturesReused; }
        void incNumScratchTexturesReused() { ++fNumScratchTexturesReused; }

        int numScratchMSAAAttachmentsReused() const { return fNumScratchMSAAAttachmentsReused; }
        void incNumScratchMSAAAttachmentsReused() { ++fNumScratchMSAAAttachmentsReused; }

        int numInlineCompilationFailures() const { return fNumInlineCompilationFailures; }
        void incNumInlineCompilationFailures() { ++fNumInlineCompilationFailures; }

        int numInlineProgramCacheResult(ProgramCacheResult stat) const {
            return fInlineProgramCacheStats[(int) stat];
        }
        void incNumInlineProgramCacheResult(ProgramCacheResult stat) {
            ++fInlineProgramCacheStats[(int) stat];
        }

        int numPreCompilationFailures() const { return fNumPreCompilationFailures; }
        void incNumPreCompilationFailures() { ++fNumPreCompilationFailures; }

        int numPreProgramCacheResult(ProgramCacheResult stat) const {
            return fPreProgramCacheStats[(int) stat];
        }
        void incNumPreProgramCacheResult(ProgramCacheResult stat) {
            ++fPreProgramCacheStats[(int) stat];
        }

        int numCompilationFailures() const { return fNumCompilationFailures; }
        void incNumCompilationFailures() { ++fNumCompilationFailures; }

        int numPartialCompilationSuccesses() const { return fNumPartialCompilationSuccesses; }
        void incNumPartialCompilationSuccesses() { ++fNumPartialCompilationSuccesses; }

        int numCompilationSuccesses() const { return fNumCompilationSuccesses; }
        void incNumCompilationSuccesses() { ++fNumCompilationSuccesses; }

#if GR_TEST_UTILS
        void dump(SkString*);
        void dumpKeyValuePairs(SkTArray<SkString>* keys, SkTArray<double>* values);
#endif
    private:
        int fRenderTargetBinds = 0;
        int fShaderCompilations = 0;
        int fTextureCreates = 0;
        int fTextureUploads = 0;
        int fTransfersToTexture = 0;
        int fTransfersFromSurface = 0;
        int fStencilAttachmentCreates = 0;
        int fMSAAAttachmentCreates = 0;
        int fNumDraws = 0;
        int fNumFailedDraws = 0;
        int fNumSubmitToGpus = 0;
        int fNumScratchTexturesReused = 0;
        int fNumScratchMSAAAttachmentsReused = 0;

        int fNumInlineCompilationFailures = 0;
        int fInlineProgramCacheStats[kNumProgramCacheResults] = { 0 };

        int fNumPreCompilationFailures = 0;
        int fPreProgramCacheStats[kNumProgramCacheResults] = { 0 };

        int fNumCompilationFailures = 0;
        int fNumPartialCompilationSuccesses = 0;
        int fNumCompilationSuccesses = 0;

#else

#if GR_TEST_UTILS
        void dump(SkString*) {}
        void dumpKeyValuePairs(SkTArray<SkString>*, SkTArray<double>*) {}
#endif
        void incRenderTargetBinds() {}
        void incShaderCompilations() {}
        void incTextureCreates() {}
        void incTextureUploads() {}
        void incTransfersToTexture() {}
        void incTransfersFromSurface() {}
        void incStencilAttachmentCreates() {}
        void incMSAAAttachmentCreates() {}
        void incNumDraws() {}
        void incNumFailedDraws() {}
        void incNumSubmitToGpus() {}
        void incNumScratchTexturesReused() {}
        void incNumScratchMSAAAttachmentsReused() {}
        void incNumInlineCompilationFailures() {}
        void incNumInlineProgramCacheResult(ProgramCacheResult stat) {}
        void incNumPreCompilationFailures() {}
        void incNumPreProgramCacheResult(ProgramCacheResult stat) {}
        void incNumCompilationFailures() {}
        void incNumPartialCompilationSuccesses() {}
        void incNumCompilationSuccesses() {}
#endif
    };

    Stats* stats() { return &fStats; }
    void dumpJSON(SkJSONWriter*) const;

    /** Used to initialize a backend texture with either a constant color, pixmaps or
     *  compressed data.
     */
    class BackendTextureData {
    public:
        enum class Type { kColor, kPixmaps, kCompressed };
        BackendTextureData() = default;
        BackendTextureData(const SkColor4f& color) : fType(Type::kColor), fColor(color) {}
        BackendTextureData(const SkPixmap pixmaps[]) : fType(Type::kPixmaps), fPixmaps(pixmaps) {
            SkASSERT(pixmaps);
        }
        BackendTextureData(const void* data, size_t size) : fType(Type::kCompressed) {
            SkASSERT(data);
            fCompressed.fData = data;
            fCompressed.fSize = size;
        }

        Type type() const { return fType; }
        SkColor4f color() const {
            SkASSERT(this->type() == Type::kColor);
            return fColor;
        }

        const SkPixmap& pixmap(int i) const {
            SkASSERT(this->type() == Type::kPixmaps);
            return fPixmaps[i];
        }
        const SkPixmap* pixmaps() const {
            SkASSERT(this->type() == Type::kPixmaps);
            return fPixmaps;
        }

        const void* compressedData() const {
            SkASSERT(this->type() == Type::kCompressed);
            return fCompressed.fData;
        }
        size_t compressedSize() const {
            SkASSERT(this->type() == Type::kCompressed);
            return fCompressed.fSize;
        }


    private:
        Type fType = Type::kColor;
        union {
            SkColor4f fColor = {0, 0, 0, 0};
            const SkPixmap* fPixmaps;
            struct {
                const void*  fData;
                size_t       fSize;
            } fCompressed;
        };
    };

    /**
     * Creates a texture directly in the backend API without wrapping it in a GrTexture.
     * Must be matched with a call to deleteBackendTexture().
     *
     * If data is null the texture is uninitialized.
     *
     * If data represents a color then all texture levels are cleared to that color.
     *
     * If data represents pixmaps then it must have a either one pixmap or, if mipmapping
     * is specified, a complete MIP hierarchy of pixmaps. Additionally, if provided, the mip
     * levels must be sized correctly according to the MIP sizes implied by dimensions. They
     * must all have the same color type and that color type must be compatible with the
     * texture format.
     */
    GrBackendTexture createBackendTexture(SkISize dimensions,
                                          const GrBackendFormat&,
                                          GrRenderable,
                                          GrMipmapped,
                                          GrProtected);

    bool updateBackendTexture(const GrBackendTexture&,
                              sk_sp<GrRefCntedCallback> finishedCallback,
                              const BackendTextureData*);

    /**
     * Same as the createBackendTexture case except compressed backend textures can
     * never be renderable.
     */
    GrBackendTexture createCompressedBackendTexture(SkISize dimensions,
                                                    const GrBackendFormat&,
                                                    GrMipmapped,
                                                    GrProtected);

    bool updateCompressedBackendTexture(const GrBackendTexture&,
                                        sk_sp<GrRefCntedCallback> finishedCallback,
                                        const BackendTextureData*);

    virtual bool setBackendTextureState(const GrBackendTexture&,
                                        const GrBackendSurfaceMutableState&,
                                        GrBackendSurfaceMutableState* previousState,
                                        sk_sp<GrRefCntedCallback> finishedCallback) {
        return false;
    }

    virtual bool setBackendRenderTargetState(const GrBackendRenderTarget&,
                                             const GrBackendSurfaceMutableState&,
                                             GrBackendSurfaceMutableState* previousState,
                                             sk_sp<GrRefCntedCallback> finishedCallback) {
        return false;
    }

    /**
     * Frees a texture created by createBackendTexture(). If ownership of the backend
     * texture has been transferred to a context using adopt semantics this should not be called.
     */
    virtual void deleteBackendTexture(const GrBackendTexture&) = 0;

    /**
     * In this case we have a program descriptor and a program info but no render target.
     */
    virtual bool compile(const GrProgramDesc&, const GrProgramInfo&) = 0;

    virtual bool precompileShader(const SkData& key, const SkData& data) { return false; }

#if GR_TEST_UTILS
    /** Check a handle represents an actual texture in the backend API that has not been freed. */
    virtual bool isTestingOnlyBackendTexture(const GrBackendTexture&) const = 0;

    /**
     * Creates a GrBackendRenderTarget that can be wrapped using
     * SkSurface::MakeFromBackendRenderTarget. Ideally this is a non-textureable allocation to
     * differentiate from testing with SkSurface::MakeFromBackendTexture. When sampleCnt > 1 this
     * is used to test client wrapped allocations with MSAA where Skia does not allocate a separate
     * buffer for resolving. If the color is non-null the backing store should be cleared to the
     * passed in color.
     */
    virtual GrBackendRenderTarget createTestingOnlyBackendRenderTarget(
            SkISize dimensions,
            GrColorType,
            int sampleCount = 1,
            GrProtected = GrProtected::kNo) = 0;

    /**
     * Deletes a GrBackendRenderTarget allocated with the above. Synchronization to make this safe
     * is up to the caller.
     */
    virtual void deleteTestingOnlyBackendRenderTarget(const GrBackendRenderTarget&) = 0;

    // This is only to be used in GL-specific tests.
    virtual const GrGLContext* glContextForTesting() const { return nullptr; }

    // This is only to be used by testing code
    virtual void resetShaderCacheForTesting() const {}

    /**
     * Flushes all work to the gpu and forces the GPU to wait until all the gpu work has completed.
     * This is for testing purposes only.
     */
    virtual void testingOnly_flushGpuAndSync() = 0;

    /**
     * Inserted as a pair around a block of code to do a GPU frame capture.
     * Currently only works with the Metal backend.
     */
    virtual void testingOnly_startCapture() {}
    virtual void testingOnly_endCapture() {}
#endif

    // width and height may be larger than rt (if underlying API allows it).
    // Returns nullptr if compatible sb could not be created, otherwise the caller owns the ref on
    // the GrAttachment.
    virtual sk_sp<GrAttachment> makeStencilAttachmentForRenderTarget(const GrRenderTarget*,
                                                                     SkISize dimensions,
                                                                     int numStencilSamples) = 0;

    virtual GrBackendFormat getPreferredStencilFormat(const GrBackendFormat&) = 0;

    // Creates an MSAA surface to be used as an MSAA attachment on a framebuffer.
    virtual sk_sp<GrAttachment> makeMSAAAttachment(SkISize dimensions,
                                                   const GrBackendFormat& format,
                                                   int numSamples,
                                                   GrProtected isProtected) = 0;

    void handleDirtyContext() {
        if (fResetBits) {
            this->resetContext();
        }
    }

    virtual void storeVkPipelineCacheData() {}

    // http://skbug.com/9739
    virtual void insertManualFramebufferBarrier() {
        SkASSERT(!this->caps()->requiresManualFBBarrierAfterTessellatedStencilDraw());
        SK_ABORT("Manual framebuffer barrier not supported.");
    }

    // Called before certain draws in order to guarantee coherent results from dst reads.
    virtual void xferBarrier(GrRenderTarget*, GrXferBarrierType) = 0;

protected:
    static bool MipMapsAreCorrect(SkISize dimensions, GrMipmapped, const BackendTextureData*);
    static bool CompressedDataIsCorrect(SkISize dimensions, SkImage::CompressionType,
                                        GrMipmapped, const BackendTextureData*);

    // Handles cases where a surface will be updated without a call to flushRenderTarget.
    void didWriteToSurface(GrSurface* surface, GrSurfaceOrigin origin, const SkIRect* bounds,
                           uint32_t mipLevels = 1) const;

    void setOOMed() { fOOMed = true; }

    Stats                            fStats;
    std::unique_ptr<GrPathRendering> fPathRendering;
    // Subclass must initialize this in its constructor.
    sk_sp<const GrCaps>              fCaps;

private:
    virtual GrBackendTexture onCreateBackendTexture(SkISize dimensions,
                                                    const GrBackendFormat&,
                                                    GrRenderable,
                                                    GrMipmapped,
                                                    GrProtected) = 0;

    virtual GrBackendTexture onCreateCompressedBackendTexture(
            SkISize dimensions, const GrBackendFormat&, GrMipmapped, GrProtected) = 0;

    virtual bool onUpdateBackendTexture(const GrBackendTexture&,
                                        sk_sp<GrRefCntedCallback> finishedCallback,
                                        const BackendTextureData*) = 0;

    virtual bool onUpdateCompressedBackendTexture(const GrBackendTexture&,
                                                  sk_sp<GrRefCntedCallback> finishedCallback,
                                                  const BackendTextureData*) = 0;

    // called when the 3D context state is unknown. Subclass should emit any
    // assumed 3D context state and dirty any state cache.
    virtual void onResetContext(uint32_t resetBits) = 0;

    // Implementation of resetTextureBindings.
    virtual void onResetTextureBindings() {}

    // Queries the effective number of samples in use by the hardware for the given render target,
    // and queries the individual sample locations.
    virtual void querySampleLocations(GrRenderTarget*, SkTArray<SkPoint>*) = 0;

    // overridden by backend-specific derived class to create objects.
    // Texture size, renderablility, format support, sample count will have already been validated
    // in base class before onCreateTexture is called.
    // If the ith bit is set in levelClearMask then the ith MIP level should be cleared.
    virtual sk_sp<GrTexture> onCreateTexture(SkISize dimensions,
                                             const GrBackendFormat&,
                                             GrRenderable,
                                             int renderTargetSampleCnt,
                                             SkBudgeted,
                                             GrProtected,
                                             int mipLevelCoont,
                                             uint32_t levelClearMask) = 0;
    virtual sk_sp<GrTexture> onCreateCompressedTexture(SkISize dimensions,
                                                       const GrBackendFormat&,
                                                       SkBudgeted,
                                                       GrMipmapped,
                                                       GrProtected,
                                                       const void* data, size_t dataSize) = 0;
    virtual sk_sp<GrTexture> onWrapBackendTexture(const GrBackendTexture&,
                                                  GrWrapOwnership,
                                                  GrWrapCacheable,
                                                  GrIOType) = 0;

    virtual sk_sp<GrTexture> onWrapCompressedBackendTexture(const GrBackendTexture&,
                                                            GrWrapOwnership,
                                                            GrWrapCacheable) = 0;

    virtual sk_sp<GrTexture> onWrapRenderableBackendTexture(const GrBackendTexture&,
                                                            int sampleCnt,
                                                            GrWrapOwnership,
                                                            GrWrapCacheable) = 0;
    virtual sk_sp<GrRenderTarget> onWrapBackendRenderTarget(const GrBackendRenderTarget&) = 0;
    virtual sk_sp<GrRenderTarget> onWrapVulkanSecondaryCBAsRenderTarget(const SkImageInfo&,
                                                                        const GrVkDrawableInfo&);

    virtual sk_sp<GrGpuBuffer> onCreateBuffer(size_t size, GrGpuBufferType intendedType,
                                              GrAccessPattern, const void* data) = 0;

    // overridden by backend-specific derived class to perform the surface read
    virtual bool onReadPixels(GrSurface*, int left, int top, int width, int height,
                              GrColorType surfaceColorType, GrColorType dstColorType, void* buffer,
                              size_t rowBytes) = 0;

    // overridden by backend-specific derived class to perform the surface write
    virtual bool onWritePixels(GrSurface*, int left, int top, int width, int height,
                               GrColorType surfaceColorType, GrColorType srcColorType,
                               const GrMipLevel texels[], int mipLevelCount,
                               bool prepForTexSampling) = 0;

    // overridden by backend-specific derived class to perform the texture transfer
    virtual bool onTransferPixelsTo(GrTexture*, int left, int top, int width, int height,
                                    GrColorType textiueColorType, GrColorType bufferColorType,
                                    GrGpuBuffer* transferBuffer, size_t offset,
                                    size_t rowBytes) = 0;
    // overridden by backend-specific derived class to perform the surface transfer
    virtual bool onTransferPixelsFrom(GrSurface*, int left, int top, int width, int height,
                                      GrColorType surfaceColorType, GrColorType bufferColorType,
                                      GrGpuBuffer* transferBuffer, size_t offset) = 0;

    // overridden by backend-specific derived class to perform the resolve
    virtual void onResolveRenderTarget(GrRenderTarget* target, const SkIRect& resolveRect) = 0;

    // overridden by backend specific derived class to perform mip map level regeneration.
    virtual bool onRegenerateMipMapLevels(GrTexture*) = 0;

    // overridden by backend specific derived class to perform the copy surface
    virtual bool onCopySurface(GrSurface* dst, GrSurface* src, const SkIRect& srcRect,
                               const SkIPoint& dstPoint) = 0;

    virtual GrOpsRenderPass* onGetOpsRenderPass(
            GrRenderTarget* renderTarget,
            GrAttachment* stencil,
            GrSurfaceOrigin,
            const SkIRect& bounds,
            const GrOpsRenderPass::LoadAndStoreInfo&,
            const GrOpsRenderPass::StencilLoadAndStoreInfo&,
            const SkTArray<GrSurfaceProxy*, true>& sampledProxies,
            GrXferBarrierFlags renderPassXferBarriers) = 0;

    virtual void prepareSurfacesForBackendAccessAndStateUpdates(
            SkSpan<GrSurfaceProxy*> proxies,
            SkSurface::BackendSurfaceAccess access,
            const GrBackendSurfaceMutableState* newState) {}

    virtual bool onSubmitToGpu(bool syncCpu) = 0;

#ifdef SK_ENABLE_DUMP_GPU
    virtual void onDumpJSON(SkJSONWriter*) const {}
#endif

    sk_sp<GrTexture> createTextureCommon(SkISize,
                                         const GrBackendFormat&,
                                         GrRenderable,
                                         int renderTargetSampleCnt,
                                         SkBudgeted,
                                         GrProtected,
                                         int mipLevelCnt,
                                         uint32_t levelClearMask);

    void resetContext() {
        this->onResetContext(fResetBits);
        fResetBits = 0;
    }

    void callSubmittedProcs(bool success);

    uint32_t fResetBits;
    // The context owns us, not vice-versa, so this ptr is not ref'ed by Gpu.
    GrDirectContext* fContext;
    GrSamplePatternDictionary fSamplePatternDictionary;

    struct SubmittedProc {
        SubmittedProc(GrGpuSubmittedProc proc, GrGpuSubmittedContext context)
                : fProc(proc), fContext(context) {}

        GrGpuSubmittedProc fProc;
        GrGpuSubmittedContext fContext;
    };
    SkSTArray<4, SubmittedProc> fSubmittedProcs;

    bool fOOMed = false;

#if SK_HISTOGRAMS_ENABLED
    int fCurrentSubmitRenderPassCount = 0;
#endif

    friend class GrPathRendering;
    using INHERITED = SkRefCnt;
};

#endif
