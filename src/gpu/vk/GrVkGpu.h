/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrVkGpu_DEFINED
#define GrVkGpu_DEFINED

#include "include/gpu/vk/GrVkBackendContext.h"
#include "include/gpu/vk/GrVkTypes.h"
#include "src/gpu/GrGpu.h"
#include "src/gpu/vk/GrVkCaps.h"
#include "src/gpu/vk/GrVkMemory.h"
#include "src/gpu/vk/GrVkMeshBuffer.h"
#include "src/gpu/vk/GrVkResourceProvider.h"
#include "src/gpu/vk/GrVkSemaphore.h"
#include "src/gpu/vk/GrVkUtil.h"

class GrPipeline;

class GrVkBufferImpl;
class GrVkCommandPool;
class GrVkMemoryAllocator;
class GrVkPipeline;
class GrVkPipelineState;
class GrVkPrimaryCommandBuffer;
class GrVkOpsRenderPass;
class GrVkRenderPass;
class GrVkSecondaryCommandBuffer;
class GrVkTexture;
struct GrVkInterface;

namespace SkSL {
    class Compiler;
}

class GrVkGpu : public GrGpu {
public:
    static sk_sp<GrGpu> Make(const GrVkBackendContext&, const GrContextOptions&, GrContext*);

    ~GrVkGpu() override;

    void disconnect(DisconnectType) override;

    const GrVkInterface* vkInterface() const { return fInterface.get(); }
    const GrVkCaps& vkCaps() const { return *fVkCaps; }

    bool isDeviceLost() const override { return fDeviceIsLost; }
    void setDeviceLost() { fDeviceIsLost = true; }

    GrVkMemoryAllocator* memoryAllocator() const { return fMemoryAllocator.get(); }

    VkPhysicalDevice physicalDevice() const { return fPhysicalDevice; }
    VkDevice device() const { return fDevice; }
    VkQueue  queue() const { return fQueue; }
    uint32_t  queueIndex() const { return fQueueIndex; }
    GrVkCommandPool* cmdPool() const {
        return fTempCmdPool ? fTempCmdPool : fMainCmdPool;
    }
    const VkPhysicalDeviceProperties& physicalDeviceProperties() const {
        return fPhysDevProps;
    }
    const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties() const {
        return fPhysDevMemProps;
    }
    bool protectedContext() const { return fProtectedContext == GrProtected::kYes; }

    GrVkResourceProvider& resourceProvider() { return fResourceProvider; }

    GrVkPrimaryCommandBuffer* currentCommandBuffer() const {
        return fTempCmdBuffer ? fTempCmdBuffer : fMainCmdBuffer;
    }

    void querySampleLocations(GrRenderTarget*, SkTArray<SkPoint>*) override;

    void xferBarrier(GrRenderTarget*, GrXferBarrierType) override {}

    void deleteBackendTexture(const GrBackendTexture&) override;

    bool compile(const GrProgramDesc&, const GrProgramInfo&) override;

#if GR_TEST_UTILS
    bool isTestingOnlyBackendTexture(const GrBackendTexture&) const override;

    GrBackendRenderTarget createTestingOnlyBackendRenderTarget(int w, int h, GrColorType) override;
    void deleteTestingOnlyBackendRenderTarget(const GrBackendRenderTarget&) override;

    void testingOnly_flushGpuAndSync() override;

    void resetShaderCacheForTesting() const override {
        fResourceProvider.resetShaderCacheForTesting();
    }
#endif

    GrStencilAttachment* createStencilAttachmentForRenderTarget(
            const GrRenderTarget*, int width, int height, int numStencilSamples) override;

    GrOpsRenderPass* getOpsRenderPass(
            GrRenderTarget*, GrSurfaceOrigin, const SkIRect&,
            const GrOpsRenderPass::LoadAndStoreInfo&,
            const GrOpsRenderPass::StencilLoadAndStoreInfo&,
            const SkTArray<GrSurfaceProxy*, true>& sampledProxies) override;

    void addBufferMemoryBarrier(const GrManagedResource*,
                                VkPipelineStageFlags srcStageMask,
                                VkPipelineStageFlags dstStageMask,
                                bool byRegion,
                                VkBufferMemoryBarrier* barrier) const;
    void addImageMemoryBarrier(const GrManagedResource*,
                               VkPipelineStageFlags srcStageMask,
                               VkPipelineStageFlags dstStageMask,
                               bool byRegion,
                               VkImageMemoryBarrier* barrier) const;

    SkSL::Compiler* shaderCompiler() const {
        return fCompiler;
    }

    bool onRegenerateMipMapLevels(GrTexture* tex) override;

    void onResolveRenderTarget(GrRenderTarget* target, const SkIRect& resolveRect,
                               ForExternalIO) override;

    void submitSecondaryCommandBuffer(std::unique_ptr<GrVkSecondaryCommandBuffer>);

    void submit(GrOpsRenderPass*) override;

    GrFence SK_WARN_UNUSED_RESULT insertFence() override;
    bool waitFence(GrFence) override;
    void deleteFence(GrFence) const override;

    std::unique_ptr<GrSemaphore> SK_WARN_UNUSED_RESULT makeSemaphore(bool isOwned) override;
    std::unique_ptr<GrSemaphore> wrapBackendSemaphore(const GrBackendSemaphore& semaphore,
            GrResourceProvider::SemaphoreWrapType wrapType, GrWrapOwnership ownership) override;
    void insertSemaphore(GrSemaphore* semaphore) override;
    void waitSemaphore(GrSemaphore* semaphore) override;

    // These match the definitions in SkDrawable, from whence they came
    typedef void* SubmitContext;
    typedef void (*SubmitProc)(SubmitContext submitContext);

    // Adds an SkDrawable::GpuDrawHandler that we will delete the next time we submit the primary
    // command buffer to the gpu.
    void addDrawable(std::unique_ptr<SkDrawable::GpuDrawHandler> drawable);

    void checkFinishProcs() override { fResourceProvider.checkCommandBuffers(); }

    std::unique_ptr<GrSemaphore> prepareTextureForCrossContextUsage(GrTexture*) override;

    void copyBuffer(GrVkBuffer* srcBuffer, GrVkBuffer* dstBuffer, VkDeviceSize srcOffset,
                    VkDeviceSize dstOffset, VkDeviceSize size);
    bool updateBuffer(GrVkBuffer* buffer, const void* src, VkDeviceSize offset, VkDeviceSize size);

    enum PersistentCacheKeyType : uint32_t {
        kShader_PersistentCacheKeyType = 0,
        kPipelineCache_PersistentCacheKeyType = 1,
    };

    void storeVkPipelineCacheData() override;

    bool beginRenderPass(const GrVkRenderPass*,
                         const VkClearValue* colorClear,
                         GrVkRenderTarget*, GrSurfaceOrigin,
                         const SkIRect& bounds, bool forSecondaryCB);
    void endRenderPass(GrRenderTarget* target, GrSurfaceOrigin origin, const SkIRect& bounds);

private:
    enum SyncQueue {
        kForce_SyncQueue,
        kSkip_SyncQueue
    };

    GrVkGpu(GrContext*, const GrContextOptions&, const GrVkBackendContext&,
            sk_sp<const GrVkInterface>, uint32_t instanceVersion, uint32_t physicalDeviceVersion,
            sk_sp<GrVkMemoryAllocator>);

    void onResetContext(uint32_t resetBits) override {}

    void destroyResources();

    GrBackendTexture onCreateBackendTexture(SkISize dimensions,
                                            const GrBackendFormat&,
                                            GrRenderable,
                                            GrMipMapped,
                                            GrProtected) override;
    GrBackendTexture onCreateCompressedBackendTexture(SkISize dimensions,
                                                      const GrBackendFormat&,
                                                      GrMipMapped,
                                                      GrProtected,
                                                      sk_sp<GrRefCntedCallback> finishedCallbacks,
                                                      const BackendTextureData*) override;

    bool onUpdateBackendTexture(const GrBackendTexture&,
                                sk_sp<GrRefCntedCallback> finishedCallback,
                                const BackendTextureData*) override;

    sk_sp<GrTexture> onCreateTexture(SkISize,
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

    sk_sp<GrRenderTarget> onWrapVulkanSecondaryCBAsRenderTarget(const SkImageInfo&,
                                                                const GrVkDrawableInfo&) override;

    sk_sp<GrGpuBuffer> onCreateBuffer(size_t size, GrGpuBufferType type, GrAccessPattern,
                                      const void* data) override;

    bool onReadPixels(GrSurface* surface, int left, int top, int width, int height,
                      GrColorType surfaceColorType, GrColorType dstColorType, void* buffer,
                      size_t rowBytes) override;

    bool onWritePixels(GrSurface* surface, int left, int top, int width, int height,
                       GrColorType surfaceColorType, GrColorType srcColorType,
                       const GrMipLevel texels[], int mipLevelCount,
                       bool prepForTexSampling) override;

    bool onTransferPixelsTo(GrTexture*, int left, int top, int width, int height,
                            GrColorType textureColorType, GrColorType bufferColorType,
                            GrGpuBuffer* transferBuffer, size_t offset, size_t rowBytes) override;
    bool onTransferPixelsFrom(GrSurface* surface, int left, int top, int width, int height,
                              GrColorType surfaceColorType, GrColorType bufferColorType,
                              GrGpuBuffer* transferBuffer, size_t offset) override;

    bool onCopySurface(GrSurface* dst, GrSurface* src, const SkIRect& srcRect,
                       const SkIPoint& dstPoint) override;

    void addFinishedProc(GrGpuFinishedProc finishedProc,
                         GrGpuFinishedContext finishedContext) override;

    void prepareSurfacesForBackendAccessAndExternalIO(
            GrSurfaceProxy* proxies[], int numProxies, SkSurface::BackendSurfaceAccess access,
            const GrPrepareForExternalIORequests& externalRequests) override;

    bool onSubmitToGpu(bool syncCpu) override;

    // Ends and submits the current command buffer to the queue and then creates a new command
    // buffer and begins it. If sync is set to kForce_SyncQueue, the function will wait for all
    // work in the queue to finish before returning. If this GrVkGpu object has any semaphores in
    // fSemaphoreToSignal, we will add those signal semaphores to the submission of this command
    // buffer. If this GrVkGpu object has any semaphores in fSemaphoresToWaitOn, we will add those
    // wait semaphores to the submission of this command buffer.
    bool submitCommandBuffer(SyncQueue sync);

    void copySurfaceAsCopyImage(GrSurface* dst, GrSurface* src, GrVkImage* dstImage,
                                GrVkImage* srcImage, const SkIRect& srcRect,
                                const SkIPoint& dstPoint);

    void copySurfaceAsBlit(GrSurface* dst, GrSurface* src, GrVkImage* dstImage, GrVkImage* srcImage,
                           const SkIRect& srcRect, const SkIPoint& dstPoint);

    void copySurfaceAsResolve(GrSurface* dst, GrSurface* src, const SkIRect& srcRect,
                              const SkIPoint& dstPoint);

    // helpers for onCreateTexture and writeTexturePixels
    bool uploadTexDataLinear(GrVkTexture* tex, int left, int top, int width, int height,
                             GrColorType colorType, const void* data, size_t rowBytes);
    bool uploadTexDataOptimal(GrVkTexture* tex, int left, int top, int width, int height,
                              GrColorType colorType, const GrMipLevel texels[], int mipLevelCount);
    bool uploadTexDataCompressed(GrVkTexture* tex, VkFormat vkFormat, SkISize dimensions,
                                 GrMipMapped mipMapped, const void* data, size_t dataSize);
    void resolveImage(GrSurface* dst, GrVkRenderTarget* src, const SkIRect& srcRect,
                      const SkIPoint& dstPoint);

    bool createVkImageForBackendSurface(VkFormat,
                                        SkISize dimensions,
                                        GrTexturable,
                                        GrRenderable,
                                        GrMipMapped,
                                        GrVkImageInfo*,
                                        GrProtected);

    // Creates a new temporary primary command buffer that will be target of all subsequent commands
    // until it is submitted via submitTempCommandBuffer. When the temp command buffer gets
    // submitted the main command buffer will begin being the target of commands again. When using a
    // a temp command buffer, the caller should not use any resources that may have been used by the
    // unsubmitted main command buffer. The reason for this is we've already updated state, like
    // image layout, for the resources on the main command buffer even though we haven't submitted
    // it yet. Thus if the same resource gets used on the temp our tracking will get thosse state
    // updates out of order. It is legal to use a resource on either the temp or main command buffer
    // that was used on a previously submitted command buffer;
    GrVkPrimaryCommandBuffer* getTempCommandBuffer();
    bool submitTempCommandBuffer(SyncQueue sync, sk_sp<GrRefCntedCallback> finishedCallback);

    sk_sp<const GrVkInterface>                            fInterface;
    sk_sp<GrVkMemoryAllocator>                            fMemoryAllocator;
    sk_sp<GrVkCaps>                                       fVkCaps;
    bool                                                  fDeviceIsLost = false;

    VkPhysicalDevice                                      fPhysicalDevice;
    VkDevice                                              fDevice;
    VkQueue                                               fQueue;    // Must be Graphics queue
    uint32_t                                              fQueueIndex;

    // Created by GrVkGpu
    GrVkResourceProvider                                  fResourceProvider;

    GrVkCommandPool*                                      fMainCmdPool;
    // just a raw pointer; object's lifespan is managed by fCmdPool
    GrVkPrimaryCommandBuffer*                             fMainCmdBuffer;

    GrVkCommandPool*                                      fTempCmdPool = nullptr;
    // just a raw pointer; object's lifespan is managed by fCmdPool
    GrVkPrimaryCommandBuffer*                             fTempCmdBuffer = nullptr;

    SkSTArray<1, GrVkSemaphore::Resource*>                fSemaphoresToWaitOn;
    SkSTArray<1, GrVkSemaphore::Resource*>                fSemaphoresToSignal;

    SkTArray<std::unique_ptr<SkDrawable::GpuDrawHandler>> fDrawables;

    VkPhysicalDeviceProperties                            fPhysDevProps;
    VkPhysicalDeviceMemoryProperties                      fPhysDevMemProps;

    // compiler used for compiling sksl into spirv. We only want to create the compiler once since
    // there is significant overhead to the first compile of any compiler.
    SkSL::Compiler*                                       fCompiler;

    // We need a bool to track whether or not we've already disconnected all the gpu resources from
    // vulkan context.
    bool                                                  fDisconnected;

    GrProtected                                           fProtectedContext;

    std::unique_ptr<GrVkOpsRenderPass>                    fCachedOpsRenderPass;

    typedef GrGpu INHERITED;
};

#endif
