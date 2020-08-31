/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrGLGpu_DEFINED
#define GrGLGpu_DEFINED

#include "include/core/SkTypes.h"
#include "include/private/SkTArray.h"
#include "src/core/SkLRUCache.h"
#include "src/gpu/GrFinishCallbacks.h"
#include "src/gpu/GrGpu.h"
#include "src/gpu/GrNativeRect.h"
#include "src/gpu/GrProgramDesc.h"
#include "src/gpu/GrWindowRectsState.h"
#include "src/gpu/GrXferProcessor.h"
#include "src/gpu/gl/GrGLContext.h"
#include "src/gpu/gl/GrGLPathRendering.h"
#include "src/gpu/gl/GrGLProgram.h"
#include "src/gpu/gl/GrGLRenderTarget.h"
#include "src/gpu/gl/GrGLStencilAttachment.h"
#include "src/gpu/gl/GrGLTexture.h"
#include "src/gpu/gl/GrGLVertexArray.h"

class GrGLBuffer;
class GrGLOpsRenderPass;
class GrPipeline;
class GrSwizzle;

class GrGLGpu final : public GrGpu {
public:
    static sk_sp<GrGpu> Make(sk_sp<const GrGLInterface>, const GrContextOptions&, GrContext*);
    ~GrGLGpu() override;

    void disconnect(DisconnectType) override;

    const GrGLContext& glContext() const { return *fGLContext; }

    const GrGLInterface* glInterface() const { return fGLContext->glInterface(); }
    const GrGLContextInfo& ctxInfo() const { return *fGLContext; }
    GrGLStandard glStandard() const { return fGLContext->standard(); }
    GrGLVersion glVersion() const { return fGLContext->version(); }
    GrGLSLGeneration glslGeneration() const { return fGLContext->glslGeneration(); }
    const GrGLCaps& glCaps() const { return *fGLContext->caps(); }

    GrGLPathRendering* glPathRendering() {
        SkASSERT(glCaps().shaderCaps()->pathRenderingSupport());
        return static_cast<GrGLPathRendering*>(pathRendering());
    }

    // Used by GrGLProgram to configure OpenGL state.
    void bindTexture(int unitIdx, GrSamplerState samplerState, const GrSwizzle&, GrGLTexture*);

    // These functions should be used to bind GL objects. They track the GL state and skip redundant
    // bindings. Making the equivalent glBind calls directly will confuse the state tracking.
    void bindVertexArray(GrGLuint id) {
        fHWVertexArrayState.setVertexArrayID(this, id);
    }

    // These callbacks update state tracking when GL objects are deleted. They are called from
    // GrGLResource onRelease functions.
    void notifyVertexArrayDelete(GrGLuint id) {
        fHWVertexArrayState.notifyVertexArrayDelete(id);
    }

    // Binds a buffer to the GL target corresponding to 'type', updates internal state tracking, and
    // returns the GL target the buffer was bound to.
    // When 'type' is kIndex_GrBufferType, this function will also implicitly bind the default VAO.
    // If the caller wishes to bind an index buffer to a specific VAO, it can call glBind directly.
    GrGLenum bindBuffer(GrGpuBufferType type, const GrBuffer*);

    // Flushes state from GrProgramInfo to GL. Returns false if the state couldn't be set.
    bool flushGLState(GrRenderTarget*, const GrProgramInfo&);
    void flushScissorRect(const SkIRect&, int rtWidth, int rtHeight, GrSurfaceOrigin);

    // Returns the last program bound by flushGLState(), or nullptr if a different program has since
    // been put into use via some other method (e.g., resetContext, copySurfaceAsDraw).
    // The returned GrGLProgram can be used for binding textures and vertex attributes.
    GrGLProgram* currentProgram() {
        this->handleDirtyContext();
        return fHWProgram.get();
    }

    // Binds the vertex array that should be used for internal draws, enables 'numAttribs' vertex
    // arrays, and flushes the desired primitive restart settings. If an index buffer is provided,
    // it will be bound to the vertex array. Otherwise the index buffer binding will be left
    // unchanged.
    //
    // NOTE: This binds the default VAO (ID=zero) unless we are on a core profile, in which case we
    // use a dummy array instead.
    GrGLAttribArrayState* bindInternalVertexArray(const GrBuffer* indexBuffer, int numAttribs,
                                                  GrPrimitiveRestart primitiveRestart) {
        auto* attribState = fHWVertexArrayState.bindInternalVertexArray(this, indexBuffer);
        attribState->enableVertexArrays(this, numAttribs, primitiveRestart);
        return attribState;
    }

    // Applies any necessary workarounds and returns the GL primitive type to use in draw calls.
    GrGLenum prepareToDraw(GrPrimitiveType primitiveType);

    // The GrGLOpsRenderPass does not buffer up draws before submitting them to the gpu.
    // Thus this is the implementation of the clear call for the corresponding passthrough function
    // on GrGLOpsRenderPass.
    void clear(const GrFixedClip&, const SkPMColor4f&, GrRenderTarget*, GrSurfaceOrigin);

    // The GrGLOpsRenderPass does not buffer up draws before submitting them to the gpu.
    // Thus this is the implementation of the clearStencil call for the corresponding passthrough
    // function on GrGLOpsrenderPass.
    void clearStencilClip(const GrFixedClip&, bool insideStencilMask,
                          GrRenderTarget*, GrSurfaceOrigin);

    // FIXME (michaelludwig): Can this go away and just use clearStencilClip() + marking the
    // stencil buffer as not dirty?
    void clearStencil(GrRenderTarget*, int clearValue);

    void beginCommandBuffer(GrRenderTarget*, const SkIRect& bounds, GrSurfaceOrigin,
                            const GrOpsRenderPass::LoadAndStoreInfo& colorLoadStore,
                            const GrOpsRenderPass::StencilLoadAndStoreInfo& stencilLoadStore);

    void endCommandBuffer(GrRenderTarget*, const GrOpsRenderPass::LoadAndStoreInfo& colorLoadStore,
                          const GrOpsRenderPass::StencilLoadAndStoreInfo& stencilLoadStore);

    GrOpsRenderPass* getOpsRenderPass(
            GrRenderTarget*, GrSurfaceOrigin, const SkIRect&,
            const GrOpsRenderPass::LoadAndStoreInfo&,
            const GrOpsRenderPass::StencilLoadAndStoreInfo&,
            const SkTArray<GrSurfaceProxy*, true>& sampledProxies) override;

    void invalidateBoundRenderTarget() {
        fHWBoundRenderTargetUniqueID.makeInvalid();
    }

    GrStencilAttachment* createStencilAttachmentForRenderTarget(
            const GrRenderTarget* rt, int width, int height, int numStencilSamples) override;
    void deleteBackendTexture(const GrBackendTexture&) override;

    bool compile(const GrProgramDesc&, const GrProgramInfo&) override;

    bool precompileShader(const SkData& key, const SkData& data) override {
        return fProgramCache->precompileShader(key, data);
    }

#if GR_TEST_UTILS
    bool isTestingOnlyBackendTexture(const GrBackendTexture&) const override;

    GrBackendRenderTarget createTestingOnlyBackendRenderTarget(int w, int h, GrColorType) override;
    void deleteTestingOnlyBackendRenderTarget(const GrBackendRenderTarget&) override;

    const GrGLContext* glContextForTesting() const override { return &this->glContext(); }

    void resetShaderCacheForTesting() const override { fProgramCache->reset(); }

    void testingOnly_flushGpuAndSync() override;
#endif

    void submit(GrOpsRenderPass* renderPass) override;

    GrFence SK_WARN_UNUSED_RESULT insertFence() override;
    bool waitFence(GrFence) override;
    void deleteFence(GrFence) const override;

    std::unique_ptr<GrSemaphore> SK_WARN_UNUSED_RESULT makeSemaphore(bool isOwned) override;
    std::unique_ptr<GrSemaphore> wrapBackendSemaphore(
            const GrBackendSemaphore& semaphore,
            GrResourceProvider::SemaphoreWrapType wrapType,
            GrWrapOwnership ownership) override;
    void insertSemaphore(GrSemaphore* semaphore) override;
    void waitSemaphore(GrSemaphore* semaphore) override;

    void checkFinishProcs() override;

    std::unique_ptr<GrSemaphore> prepareTextureForCrossContextUsage(GrTexture*) override;

    void deleteSync(GrGLsync) const;

    void bindFramebuffer(GrGLenum fboTarget, GrGLuint fboid);
    void deleteFramebuffer(GrGLuint fboid);

    void insertManualFramebufferBarrier() override;

private:
    GrGLGpu(std::unique_ptr<GrGLContext>, GrContext*);

    // GrGpu overrides
    GrBackendTexture onCreateBackendTexture(SkISize dimensions,
                                            const GrBackendFormat&,
                                            GrRenderable,
                                            GrMipMapped,
                                            GrProtected) override;

    GrBackendTexture onCreateCompressedBackendTexture(SkISize dimensions,
                                                      const GrBackendFormat&,
                                                      GrMipMapped,
                                                      GrProtected,
                                                      sk_sp<GrRefCntedCallback> finishedCallback,
                                                      const BackendTextureData*) override;

    bool onUpdateBackendTexture(const GrBackendTexture&,
                                sk_sp<GrRefCntedCallback> finishedCallback,
                                const BackendTextureData*) override;

    void onResetContext(uint32_t resetBits) override;

    void onResetTextureBindings() override;

    void querySampleLocations(GrRenderTarget*, SkTArray<SkPoint>*) override;

    void xferBarrier(GrRenderTarget*, GrXferBarrierType) override;

    sk_sp<GrTexture> onCreateTexture(SkISize dimensions,
                                     const GrBackendFormat&,
                                     GrRenderable,
                                     int renderTargetSampleCnt,
                                     SkBudgeted,
                                     GrProtected,
                                     int mipLevelCount,
                                     uint32_t levelClearMask) override;
    sk_sp<GrTexture> onCreateCompressedTexture(SkISize dimensions,
                                               const GrBackendFormat&,
                                               SkBudgeted,
                                               GrMipMapped,
                                               GrProtected,
                                               const void* data, size_t dataSize) override;

    sk_sp<GrGpuBuffer> onCreateBuffer(size_t size, GrGpuBufferType intendedType, GrAccessPattern,
                                      const void* data) override;

    sk_sp<GrTexture> onWrapBackendTexture(const GrBackendTexture&,
                                          GrWrapOwnership,
                                          GrWrapCacheable,
                                          GrIOType) override;
    sk_sp<GrTexture> onWrapCompressedBackendTexture(const GrBackendTexture&,
                                                    GrWrapOwnership,
                                                    GrWrapCacheable) override;
    sk_sp<GrTexture> onWrapRenderableBackendTexture(const GrBackendTexture&,
                                                    int sampleCnt,
                                                    GrWrapOwnership,
                                                    GrWrapCacheable) override;
    sk_sp<GrRenderTarget> onWrapBackendRenderTarget(const GrBackendRenderTarget&) override;
    sk_sp<GrRenderTarget> onWrapBackendTextureAsRenderTarget(const GrBackendTexture&,
                                                             int sampleCnt) override;

    // Given a GL format return the index into the stencil format array on GrGLCaps to a
    // compatible stencil format, or negative if there is no compatible stencil format.
    int getCompatibleStencilIndex(GrGLFormat format);

    void onFBOChanged();

    // Returns whether the texture is successfully created. On success, a non-zero texture ID is
    // returned. On failure, zero is returned.
    // The texture is populated with |texels|, if it is non-null.
    // The texture parameters are cached in |initialTexParams|.
    GrGLuint createTexture2D(SkISize dimensions,
                             GrGLFormat,
                             GrRenderable,
                             GrGLTextureParameters::SamplerOverriddenState*,
                             int mipLevelCount);

    GrGLuint createCompressedTexture2D(SkISize dimensions,
                                       GrGLFormat,
                                       GrMipMapped,
                                       GrGLTextureParameters::SamplerOverriddenState*,
                                       const void* data, size_t dataSize);

    bool onReadPixels(GrSurface*, int left, int top, int width, int height,
                      GrColorType surfaceColorType, GrColorType dstColorType, void* buffer,
                      size_t rowBytes) override;

    bool onWritePixels(GrSurface*, int left, int top, int width, int height,
                       GrColorType surfaceColorType, GrColorType srcColorType,
                       const GrMipLevel texels[], int mipLevelCount,
                       bool prepForTexSampling) override;

    bool onTransferPixelsTo(GrTexture*, int left, int top, int width, int height,
                            GrColorType textureColorType, GrColorType bufferColorType,
                            GrGpuBuffer* transferBuffer, size_t offset, size_t rowBytes) override;
    bool onTransferPixelsFrom(GrSurface*, int left, int top, int width, int height,
                              GrColorType surfaceColorType, GrColorType bufferColorType,
                              GrGpuBuffer* transferBuffer, size_t offset) override;
    bool readOrTransferPixelsFrom(GrSurface*, int left, int top, int width, int height,
                                  GrColorType surfaceColorType, GrColorType dstColorType,
                                  void* offsetOrPtr, int rowWidthInPixels);

    // Before calling any variation of TexImage, TexSubImage, etc..., call this to ensure that the
    // PIXEL_UNPACK_BUFFER is unbound.
    void unbindCpuToGpuXferBuffer();

    void onResolveRenderTarget(GrRenderTarget* target, const SkIRect& resolveRect,
                               ForExternalIO) override;

    bool onRegenerateMipMapLevels(GrTexture*) override;

    bool onCopySurface(GrSurface* dst, GrSurface* src, const SkIRect& srcRect,
                       const SkIPoint& dstPoint) override;

    // binds texture unit in GL
    void setTextureUnit(int unitIdx);

    void flushProgram(sk_sp<GrGLProgram>);

    // Version for programs that aren't GrGLProgram.
    void flushProgram(GrGLuint);

    void flushBlendAndColorWrite(const GrXferProcessor::BlendInfo& blendInfo, const GrSwizzle&);

    void addFinishedProc(GrGpuFinishedProc finishedProc,
                         GrGpuFinishedContext finishedContext) override;

    bool onSubmitToGpu(bool syncCpu) override;

    bool waitSync(GrGLsync, uint64_t timeout, bool flush);

    bool copySurfaceAsDraw(GrSurface* dst, GrSurface* src, const SkIRect& srcRect,
                           const SkIPoint& dstPoint);
    void copySurfaceAsCopyTexSubImage(GrSurface* dst, GrSurface* src, const SkIRect& srcRect,
                                      const SkIPoint& dstPoint);
    bool copySurfaceAsBlitFramebuffer(GrSurface* dst, GrSurface* src, const SkIRect& srcRect,
                                      const SkIPoint& dstPoint);

    class ProgramCache : public ::SkNoncopyable {
    public:
        ProgramCache(GrGLGpu* gpu);
        ~ProgramCache();

        void abandon();
        void reset();
        sk_sp<GrGLProgram> findOrCreateProgram(GrRenderTarget*, const GrProgramInfo&);
        sk_sp<GrGLProgram> findOrCreateProgram(const GrProgramDesc& desc,
                                               const GrProgramInfo& programInfo,
                                               Stats::ProgramCacheResult* stat) {
            sk_sp<GrGLProgram> tmp = this->findOrCreateProgram(nullptr, desc, programInfo, stat);
            if (!tmp) {
                fGpu->fStats.incNumPreCompilationFailures();
            } else {
                fGpu->fStats.incNumPreProgramCacheResult(*stat);
            }

            return tmp;
        }
        bool precompileShader(const SkData& key, const SkData& data);

    private:
        struct Entry;

        sk_sp<GrGLProgram> findOrCreateProgram(GrRenderTarget*,
                                               const GrProgramDesc&,
                                               const GrProgramInfo&,
                                               Stats::ProgramCacheResult*);

        struct DescHash {
            uint32_t operator()(const GrProgramDesc& desc) const {
                return SkOpts::hash_fn(desc.asKey(), desc.keyLength(), 0);
            }
        };

        SkLRUCache<GrProgramDesc, std::unique_ptr<Entry>, DescHash> fMap;

        GrGLGpu* fGpu;
    };

    void flushPatchVertexCount(uint8_t count);

    void flushColorWrite(bool writeColor);
    void flushClearColor(const SkPMColor4f&);

    // flushes the scissor. see the note on flushBoundTextureAndParams about
    // flushing the scissor after that function is called.
    void flushScissor(const GrScissorState& scissorState, int rtWidth, int rtHeight,
                      GrSurfaceOrigin rtOrigin) {
        this->flushScissorTest(GrScissorTest(scissorState.enabled()));
        if (scissorState.enabled()) {
            this->flushScissorRect(scissorState.rect(), rtWidth, rtHeight, rtOrigin);
        }
    }
    void flushScissorTest(GrScissorTest);

    void flushWindowRectangles(const GrWindowRectsState&, const GrGLRenderTarget*, GrSurfaceOrigin);
    void disableWindowRectangles();

    int numTextureUnits() const { return this->caps()->shaderCaps()->maxFragmentSamplers(); }

    // Binds a texture to a target on the "scratch" texture unit to use for texture operations
    // other than usual draw flow (i.e. a GrGLProgram derived from a GrPipeline used to draw). It
    // ensures that such operations don't negatively interact with draws. The active texture unit
    // and the binding for 'target' will change.
    void bindTextureToScratchUnit(GrGLenum target, GrGLint textureID);

    // The passed bounds contains the render target's color values that will subsequently be
    // written.
    void flushRenderTarget(GrGLRenderTarget*, GrSurfaceOrigin, const SkIRect& bounds);
    // This version has an implicit bounds of the entire render target.
    void flushRenderTarget(GrGLRenderTarget*);
    // This version can be used when the render target's colors will not be written.
    void flushRenderTargetNoColorWrites(GrGLRenderTarget*);

    // Need not be called if flushRenderTarget is used.
    void flushViewport(int width, int height);

    void flushStencil(const GrStencilSettings&, GrSurfaceOrigin);
    void disableStencil();

    // rt is used only if useHWAA is true.
    void flushHWAAState(GrRenderTarget* rt, bool useHWAA);

    void flushConservativeRasterState(bool enable);

    void flushWireframeState(bool enable);

    void flushFramebufferSRGB(bool enable);

    bool uploadTexData(GrGLFormat textureFormat, GrColorType textureColorType, int texWidth,
                       int texHeight, GrGLenum target, int left, int top, int width, int height,
                       GrColorType srcColorType, const GrMipLevel texels[], int mipLevelCount,
                       GrMipMapsStatus* mipMapsStatus = nullptr);

    // Helper for onCreateCompressedTexture. Compressed textures are read-only so we only use this
    // to populate a new texture. Returns false if we failed to create and upload the texture.
    bool uploadCompressedTexData(GrGLFormat,
                                 SkISize dimensions,
                                 GrMipMapped,
                                 GrGLenum target,
                                 const void* data, size_t dataSize);

    // Calls one of various versions of renderBufferStorageMultisample.
    bool renderbufferStorageMSAA(const GrGLContext& ctx, int sampleCount, GrGLenum format,
                                 int width, int height);

    bool createRenderTargetObjects(const GrGLTexture::Desc&,
                                   int sampleCount,
                                   GrGLRenderTarget::IDs*);
    enum TempFBOTarget {
        kSrc_TempFBOTarget,
        kDst_TempFBOTarget
    };

    // Binds a surface as a FBO for copying, reading, or clearing. If the surface already owns an
    // FBO ID then that ID is bound. If not the surface is temporarily bound to a FBO and that FBO
    // is bound. This must be paired with a call to unbindSurfaceFBOForPixelOps().
    void bindSurfaceFBOForPixelOps(GrSurface* surface, int mipLevel, GrGLenum fboTarget,
                                   TempFBOTarget tempFBOTarget);

    // Must be called if bindSurfaceFBOForPixelOps was used to bind a surface for copying.
    void unbindSurfaceFBOForPixelOps(GrSurface* surface, int mipLevel, GrGLenum fboTarget);

#ifdef SK_ENABLE_DUMP_GPU
    void onDumpJSON(SkJSONWriter*) const override;
#endif

    bool createCopyProgram(GrTexture* srcTexture);
    bool createMipmapProgram(int progIdx);

    std::unique_ptr<GrGLContext> fGLContext;

    // GL program-related state
    std::unique_ptr<ProgramCache> fProgramCache;

    ///////////////////////////////////////////////////////////////////////////
    ///@name Caching of GL State
    ///@{
    int                         fHWActiveTextureUnitIdx;

    GrGLuint                    fHWProgramID;
    sk_sp<GrGLProgram>          fHWProgram;

    enum TriState {
        kNo_TriState,
        kYes_TriState,
        kUnknown_TriState
    };

    GrGLuint                    fTempSrcFBOID;
    GrGLuint                    fTempDstFBOID;

    GrGLuint                    fStencilClearFBOID;

    // last scissor / viewport scissor state seen by the GL.
    struct {
        TriState fEnabled;
        GrNativeRect fRect;
        void invalidate() {
            fEnabled = kUnknown_TriState;
            fRect.invalidate();
        }
    } fHWScissorSettings;

    class {
    public:
        bool valid() const { return kInvalidSurfaceOrigin != fRTOrigin; }
        void invalidate() { fRTOrigin = kInvalidSurfaceOrigin; }
        bool knownDisabled() const { return this->valid() && !fWindowState.enabled(); }
        void setDisabled() {
            fRTOrigin = kTopLeft_GrSurfaceOrigin;
            fWindowState.setDisabled();
        }

        void set(GrSurfaceOrigin rtOrigin, int width, int height,
                 const GrWindowRectsState& windowState) {
            fRTOrigin = rtOrigin;
            fWidth = width;
            fHeight = height;
            fWindowState = windowState;
        }

        bool knownEqualTo(GrSurfaceOrigin rtOrigin, int width, int height,
                          const GrWindowRectsState& windowState) const {
            if (!this->valid()) {
                return false;
            }
            if (fWindowState.numWindows() &&
                (fRTOrigin != rtOrigin || fWidth != width || fHeight != height)) {
                return false;
            }
            return fWindowState == windowState;
        }

    private:
        enum { kInvalidSurfaceOrigin = -1 };

        int                  fRTOrigin;
        int                  fWidth;
        int                  fHeight;
        GrWindowRectsState   fWindowState;
    } fHWWindowRectsState;

    GrNativeRect fHWViewport;

    /**
     * Tracks vertex attrib array state.
     */
    class HWVertexArrayState {
    public:
        HWVertexArrayState() : fCoreProfileVertexArray(nullptr) { this->invalidate(); }

        ~HWVertexArrayState() { delete fCoreProfileVertexArray; }

        void invalidate() {
            fBoundVertexArrayIDIsValid = false;
            fDefaultVertexArrayAttribState.invalidate();
            if (fCoreProfileVertexArray) {
                fCoreProfileVertexArray->invalidateCachedState();
            }
        }

        void notifyVertexArrayDelete(GrGLuint id) {
            if (fBoundVertexArrayIDIsValid && fBoundVertexArrayID == id) {
                // Does implicit bind to 0
                fBoundVertexArrayID = 0;
            }
        }

        void setVertexArrayID(GrGLGpu* gpu, GrGLuint arrayID) {
            if (!gpu->glCaps().vertexArrayObjectSupport()) {
                SkASSERT(0 == arrayID);
                return;
            }
            if (!fBoundVertexArrayIDIsValid || arrayID != fBoundVertexArrayID) {
                GR_GL_CALL(gpu->glInterface(), BindVertexArray(arrayID));
                fBoundVertexArrayIDIsValid = true;
                fBoundVertexArrayID = arrayID;
            }
        }

        /**
         * Binds the vertex array that should be used for internal draws, and returns its attrib
         * state. This binds the default VAO (ID=zero) unless we are on a core profile, in which
         * case we use a dummy array instead.
         *
         * If an index buffer is provided, it will be bound to the vertex array. Otherwise the
         * index buffer binding will be left unchanged.
         *
         * The returned GrGLAttribArrayState should be used to set vertex attribute arrays.
         */
        GrGLAttribArrayState* bindInternalVertexArray(GrGLGpu*, const GrBuffer* ibuff = nullptr);

    private:
        GrGLuint             fBoundVertexArrayID;
        bool                 fBoundVertexArrayIDIsValid;

        // We return a non-const pointer to this from bindArrayAndBuffersToDraw when vertex array 0
        // is bound. However, this class is internal to GrGLGpu and this object never leaks out of
        // GrGLGpu.
        GrGLAttribArrayState fDefaultVertexArrayAttribState;

        // This is used when we're using a core profile.
        GrGLVertexArray*     fCoreProfileVertexArray;
    } fHWVertexArrayState;

    uint8_t fHWPatchVertexCount;

    struct {
        GrGLenum                fGLTarget;
        GrGpuResource::UniqueID fBoundBufferUniqueID;
        bool                    fBufferZeroKnownBound;

        void invalidate() {
            fBoundBufferUniqueID.makeInvalid();
            fBufferZeroKnownBound = false;
        }
    }                                       fHWBufferState[kGrGpuBufferTypeCount];

    auto* hwBufferState(GrGpuBufferType type) {
        unsigned typeAsUInt = static_cast<unsigned>(type);
        SkASSERT(typeAsUInt < SK_ARRAY_COUNT(fHWBufferState));
        return &fHWBufferState[typeAsUInt];
    }

    enum class FlushType {
        kIfRequired,
        kForce,
    };

    // This calls glFlush if it is required for previous operations or kForce is passed.
    void flush(FlushType flushType = FlushType::kIfRequired);

    void setNeedsFlush() { fNeedsGLFlush = true; }

    struct {
        GrBlendEquation fEquation;
        GrBlendCoeff    fSrcCoeff;
        GrBlendCoeff    fDstCoeff;
        SkPMColor4f     fConstColor;
        bool            fConstColorValid;
        TriState        fEnabled;

        void invalidate() {
            fEquation = kIllegal_GrBlendEquation;
            fSrcCoeff = kIllegal_GrBlendCoeff;
            fDstCoeff = kIllegal_GrBlendCoeff;
            fConstColorValid = false;
            fEnabled = kUnknown_TriState;
        }
    }                                       fHWBlendState;

    TriState                                fMSAAEnabled;
    TriState                                fHWConservativeRasterEnabled;

    TriState                                fHWWireframeEnabled;

    GrStencilSettings                       fHWStencilSettings;
    GrSurfaceOrigin                         fHWStencilOrigin;
    TriState                                fHWStencilTestEnabled;

    TriState                                fHWWriteToColor;
    GrGpuResource::UniqueID                 fHWBoundRenderTargetUniqueID;
    TriState                                fHWSRGBFramebuffer;

    class TextureUnitBindings {
    public:
        TextureUnitBindings() = default;
        TextureUnitBindings(const TextureUnitBindings&) = delete;
        TextureUnitBindings& operator=(const TextureUnitBindings&) = delete;

        GrGpuResource::UniqueID boundID(GrGLenum target) const;
        bool hasBeenModified(GrGLenum target) const;
        void setBoundID(GrGLenum target, GrGpuResource::UniqueID);
        void invalidateForScratchUse(GrGLenum target);
        void invalidateAllTargets(bool markUnmodified);

    private:
        struct TargetBinding {
            GrGpuResource::UniqueID fBoundResourceID;
            bool fHasBeenModified = false;
        };
        TargetBinding fTargetBindings[3];
    };
    SkAutoTArray<TextureUnitBindings> fHWTextureUnitBindings;

    GrGLfloat fHWClearColor[4];

    GrGLuint fBoundDrawFramebuffer = 0;

    /** IDs for copy surface program. (3 sampler types) */
    struct {
        GrGLuint    fProgram = 0;
        GrGLint     fTextureUniform = 0;
        GrGLint     fTexCoordXformUniform = 0;
        GrGLint     fPosXformUniform = 0;
    }                                       fCopyPrograms[3];
    sk_sp<GrGLBuffer>                       fCopyProgramArrayBuffer;

    /** IDs for texture mipmap program. (4 filter configurations) */
    struct {
        GrGLuint    fProgram = 0;
        GrGLint     fTextureUniform = 0;
        GrGLint     fTexCoordXformUniform = 0;
    }                                       fMipmapPrograms[4];
    sk_sp<GrGLBuffer>                       fMipmapProgramArrayBuffer;

    static int TextureToCopyProgramIdx(GrTexture* texture);

    static int TextureSizeToMipmapProgramIdx(int width, int height) {
        const bool wide = (width > 1) && SkToBool(width & 0x1);
        const bool tall = (height > 1) && SkToBool(height & 0x1);
        return (wide ? 0x2 : 0x0) | (tall ? 0x1 : 0x0);
    }

    GrPrimitiveType fLastPrimitiveType;

    GrGLTextureParameters::ResetTimestamp fResetTimestampForTextureParameters = 0;

    class SamplerObjectCache;
    std::unique_ptr<SamplerObjectCache> fSamplerObjectCache;

    std::unique_ptr<GrGLOpsRenderPass> fCachedOpsRenderPass;
    GrFinishCallbacks fFinishCallbacks;

    // If we've called a command that requires us to call glFlush than this will be set to true
    // since we defer calling flush until submit time. When we call submitToGpu if this is true then
    // we call glFlush and reset this to false.
    bool fNeedsGLFlush = false;

    SkDEBUGCODE(bool fIsExecutingCommandBuffer_DebugOnly = false);

    friend class GrGLPathRendering; // For accessing setTextureUnit.

    typedef GrGpu INHERITED;
};

#endif
