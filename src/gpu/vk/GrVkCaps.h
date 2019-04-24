/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrVkCaps_DEFINED
#define GrVkCaps_DEFINED

#include "GrCaps.h"
#include "GrVkStencilAttachment.h"
#include "vk/GrVkTypes.h"

class GrShaderCaps;
class GrVkExtensions;
struct GrVkInterface;

/**
 * Stores some capabilities of a Vk backend.
 */
class GrVkCaps : public GrCaps {
public:
    typedef GrVkStencilAttachment::Format StencilFormat;

    /**
     * Creates a GrVkCaps that is set such that nothing is supported. The init function should
     * be called to fill out the caps.
     */
    GrVkCaps(const GrContextOptions& contextOptions, const GrVkInterface* vkInterface,
             VkPhysicalDevice device, const VkPhysicalDeviceFeatures2& features,
             uint32_t instanceVersion, uint32_t physicalDeviceVersion,
             const GrVkExtensions& extensions);

    bool isConfigTexturable(GrPixelConfig config) const override {
        return SkToBool(ConfigInfo::kTextureable_Flag & fConfigTable[config].fOptimalFlags);
    }

    bool isConfigCopyable(GrPixelConfig config) const override {
        return true;
    }

    int getRenderTargetSampleCount(int requestedCount, GrPixelConfig config) const override;
    int maxRenderTargetSampleCount(GrPixelConfig config) const override;

    bool surfaceSupportsReadPixels(const GrSurface*) const override { return true; }

    bool isConfigTexturableLinearly(GrPixelConfig config) const {
        return SkToBool(ConfigInfo::kTextureable_Flag & fConfigTable[config].fLinearFlags);
    }

    bool isConfigRenderableLinearly(GrPixelConfig config, bool withMSAA) const {
        return !withMSAA && SkToBool(ConfigInfo::kRenderable_Flag &
                                     fConfigTable[config].fLinearFlags);
    }

    bool configCanBeDstofBlit(GrPixelConfig config, bool linearTiled) const {
        const uint16_t& flags = linearTiled ? fConfigTable[config].fLinearFlags :
                                              fConfigTable[config].fOptimalFlags;
        return SkToBool(ConfigInfo::kBlitDst_Flag & flags);
    }

    bool configCanBeSrcofBlit(GrPixelConfig config, bool linearTiled) const {
        const uint16_t& flags = linearTiled ? fConfigTable[config].fLinearFlags :
                                              fConfigTable[config].fOptimalFlags;
        return SkToBool(ConfigInfo::kBlitSrc_Flag & flags);
    }

    // On Adreno vulkan, they do not respect the imageOffset parameter at least in
    // copyImageToBuffer. This flag says that we must do the copy starting from the origin always.
    bool mustDoCopiesFromOrigin() const {
        return fMustDoCopiesFromOrigin;
    }

    // Sometimes calls to QueueWaitIdle return before actually signalling the fences
    // on the command buffers even though they have completed. This causes an assert to fire when
    // destroying the command buffers. Therefore we add a sleep to make sure the fence signals.
    bool mustSleepOnTearDown() const {
        return fMustSleepOnTearDown;
    }

    // Returns true if while adding commands to command buffers, we must make a new command buffer
    // everytime we want to bind a new VkPipeline. This is true for both primary and secondary
    // command buffers. This is to work around a driver bug specifically on AMD.
    bool newCBOnPipelineChange() const {
        return fNewCBOnPipelineChange;
    }

    // Returns true if we should always make dedicated allocations for VkImages.
    bool shouldAlwaysUseDedicatedImageMemory() const {
        return fShouldAlwaysUseDedicatedImageMemory;
    }

    /**
     * Returns both a supported and most preferred stencil format to use in draws.
     */
    const StencilFormat& preferredStencilFormat() const {
        return fPreferredStencilFormat;
    }

    // Returns whether the device supports VK_KHR_Swapchain. Internally Skia never uses any of the
    // swapchain functions, but we may need to transition to and from the
    // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR image layout, so we must know whether that layout is
    // supported.
    bool supportsSwapchain() const { return fSupportsSwapchain; }

    // Returns whether the device supports the ability to extend VkPhysicalDeviceProperties struct.
    bool supportsPhysicalDeviceProperties2() const { return fSupportsPhysicalDeviceProperties2; }
    // Returns whether the device supports the ability to extend VkMemoryRequirements struct.
    bool supportsMemoryRequirements2() const { return fSupportsMemoryRequirements2; }

    // Returns whether the device supports the ability to extend the vkBindMemory call.
    bool supportsBindMemory2() const { return fSupportsBindMemory2; }

    // Returns whether or not the device suports the various API maintenance fixes to Vulkan 1.0. In
    // Vulkan 1.1 all these maintenance are part of the core spec.
    bool supportsMaintenance1() const { return fSupportsMaintenance1; }
    bool supportsMaintenance2() const { return fSupportsMaintenance2; }
    bool supportsMaintenance3() const { return fSupportsMaintenance3; }

    // Returns true if the device supports passing in a flag to say we are using dedicated GPU when
    // allocating memory. For some devices this allows them to return more optimized memory knowning
    // they will never need to suballocate amonst multiple objects.
    bool supportsDedicatedAllocation() const { return fSupportsDedicatedAllocation; }

    // Returns true if the device supports importing of external memory into Vulkan memory.
    bool supportsExternalMemory() const { return fSupportsExternalMemory; }
    // Returns true if the device supports importing Android hardware buffers into Vulkan memory.
    bool supportsAndroidHWBExternalMemory() const { return fSupportsAndroidHWBExternalMemory; }

    // Returns true if it supports ycbcr conversion for samplers
    bool supportsYcbcrConversion() const { return fSupportsYcbcrConversion; }

    /**
     * Helpers used by canCopySurface. In all cases if the SampleCnt parameter is zero that means
     * the surface is not a render target, otherwise it is the number of samples in the render
     * target.
     */
    bool canCopyImage(GrPixelConfig dstConfig, int dstSampleCnt, GrSurfaceOrigin dstOrigin,
                      GrPixelConfig srcConfig, int srcSamplecnt, GrSurfaceOrigin srcOrigin) const;

    bool canCopyAsBlit(GrPixelConfig dstConfig, int dstSampleCnt, bool dstIsLinear,
                       GrPixelConfig srcConfig, int srcSampleCnt, bool srcIsLinear) const;

    bool canCopyAsResolve(GrPixelConfig dstConfig, int dstSampleCnt, GrSurfaceOrigin dstOrigin,
                          GrPixelConfig srcConfig, int srcSamplecnt,
                          GrSurfaceOrigin srcOrigin) const;

    bool canCopyAsDraw(GrPixelConfig dstConfig, bool dstIsRenderable,
                       GrPixelConfig srcConfig, bool srcIsTextureable) const;

    bool initDescForDstCopy(const GrRenderTargetProxy* src, GrSurfaceDesc* desc, GrSurfaceOrigin*,
                            bool* rectsMustMatch, bool* disallowSubrect) const override;

    GrPixelConfig validateBackendRenderTarget(const GrBackendRenderTarget&,
                                              SkColorType) const override;

    GrPixelConfig getConfigFromBackendFormat(const GrBackendFormat&, SkColorType) const override;
    GrPixelConfig getYUVAConfigFromBackendFormat(const GrBackendFormat&) const override;

    GrBackendFormat getBackendFormatFromGrColorType(GrColorType ct,
                                                    GrSRGBEncoded srgbEncoded) const override;

private:
    enum VkVendor {
        kAMD_VkVendor = 4098,
        kARM_VkVendor = 5045,
        kImagination_VkVendor = 4112,
        kIntel_VkVendor = 32902,
        kNvidia_VkVendor = 4318,
        kQualcomm_VkVendor = 20803,
    };

    void init(const GrContextOptions& contextOptions, const GrVkInterface* vkInterface,
              VkPhysicalDevice device, const VkPhysicalDeviceFeatures2&,
              uint32_t physicalDeviceVersion, const GrVkExtensions&);
    void initGrCaps(const GrVkInterface* vkInterface,
                    VkPhysicalDevice physDev,
                    const VkPhysicalDeviceProperties&,
                    const VkPhysicalDeviceMemoryProperties&,
                    const VkPhysicalDeviceFeatures2&,
                    const GrVkExtensions&);
    void initShaderCaps(const VkPhysicalDeviceProperties&, const VkPhysicalDeviceFeatures2&);

    void initConfigTable(const GrVkInterface*, VkPhysicalDevice, const VkPhysicalDeviceProperties&);
    void initStencilFormat(const GrVkInterface* iface, VkPhysicalDevice physDev);

    uint8_t getYcbcrKeyFromYcbcrInfo(const GrVkYcbcrConversionInfo& info);

    void applyDriverCorrectnessWorkarounds(const VkPhysicalDeviceProperties&);

    bool onSurfaceSupportsWritePixels(const GrSurface*) const override;
    bool onCanCopySurface(const GrSurfaceProxy* dst, const GrSurfaceProxy* src,
                          const SkIRect& srcRect, const SkIPoint& dstPoint) const override;

    struct ConfigInfo {
        ConfigInfo() : fOptimalFlags(0), fLinearFlags(0) {}

        void init(const GrVkInterface*, VkPhysicalDevice, const VkPhysicalDeviceProperties&,
                  VkFormat, bool disableRendering);
        static void InitConfigFlags(VkFormatFeatureFlags, uint16_t* flags, bool disableRendering);
        void initSampleCounts(const GrVkInterface*, VkPhysicalDevice,
                              const VkPhysicalDeviceProperties&, VkFormat);

        enum {
            kTextureable_Flag = 0x1,
            kRenderable_Flag  = 0x2,
            kBlitSrc_Flag     = 0x4,
            kBlitDst_Flag     = 0x8,
        };

        uint16_t fOptimalFlags;
        uint16_t fLinearFlags;

        SkTDArray<int> fColorSampleCounts;
    };
    ConfigInfo fConfigTable[kGrPixelConfigCnt];

    StencilFormat fPreferredStencilFormat;

    SkSTArray<1, GrVkYcbcrConversionInfo> fYcbcrInfos;

    bool fMustDoCopiesFromOrigin = false;
    bool fMustSleepOnTearDown = false;
    bool fNewCBOnPipelineChange = false;
    bool fShouldAlwaysUseDedicatedImageMemory = false;

    bool fSupportsSwapchain = false;

    bool fSupportsPhysicalDeviceProperties2 = false;
    bool fSupportsMemoryRequirements2 = false;
    bool fSupportsBindMemory2 = false;
    bool fSupportsMaintenance1 = false;
    bool fSupportsMaintenance2 = false;
    bool fSupportsMaintenance3 = false;

    bool fSupportsDedicatedAllocation = false;
    bool fSupportsExternalMemory = false;
    bool fSupportsAndroidHWBExternalMemory = false;

    bool fSupportsYcbcrConversion = false;

    typedef GrCaps INHERITED;
};

#endif
