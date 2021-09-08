/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrVkAttachment_DEFINED
#define GrVkAttachment_DEFINED

#include "include/gpu/vk/GrVkTypes.h"
#include "src/gpu/GrAttachment.h"
#include "src/gpu/GrRefCnt.h"
#include "src/gpu/vk/GrVkDescriptorSet.h"
#include "src/gpu/vk/GrVkImage.h"

class GrVkImageView;
class GrVkGpu;

class GrVkAttachment : public GrAttachment, public GrVkImage {
public:
    static sk_sp<GrVkAttachment> MakeStencil(GrVkGpu* gpu,
                                             SkISize dimensions,
                                             int sampleCnt,
                                             VkFormat format);

    static sk_sp<GrVkAttachment> MakeMSAA(GrVkGpu* gpu,
                                          SkISize dimensions,
                                          int numSamples,
                                          VkFormat format,
                                          GrProtected isProtected);

    static sk_sp<GrVkAttachment> MakeTexture(GrVkGpu* gpu,
                                             SkISize dimensions,
                                             VkFormat format,
                                             uint32_t mipLevels,
                                             GrRenderable renderable,
                                             int numSamples,
                                             SkBudgeted budgeted,
                                             GrProtected isProtected);

    static sk_sp<GrVkAttachment> MakeWrapped(GrVkGpu* gpu,
                                             SkISize dimensions,
                                             const GrVkImageInfo&,
                                             sk_sp<GrBackendSurfaceMutableStateImpl>,
                                             UsageFlags attachmentUsages,
                                             GrWrapOwnership,
                                             GrWrapCacheable,
                                             bool forSecondaryCB = false);

    ~GrVkAttachment() override;

    GrBackendFormat backendFormat() const override { return this->getBackendFormat(); }

    const GrManagedResource* imageResource() const { return this->resource(); }
    const GrVkImageView* framebufferView() const { return fFramebufferView.get(); }
    const GrVkImageView* textureView() const { return fTextureView.get(); }

    // So that we don't need to rewrite descriptor sets each time, we keep cached input descriptor
    // sets on the attachment and simply reuse those descriptor sets for this attachment only. These
    // calls will fail if the attachment does not support being used as an input attachment. These
    // calls do not ref the GrVkDescriptorSet so they called will need to manually ref them if they
    // need to be kept alive.
    gr_rp<const GrVkDescriptorSet> inputDescSetForBlending(GrVkGpu* gpu);
    // Input descripotr set used when needing to read a resolve attachment to load data into a
    // discardable msaa attachment.
    gr_rp<const GrVkDescriptorSet> inputDescSetForMSAALoad(GrVkGpu* gpu);

protected:
    void onRelease() override;
    void onAbandon() override;

private:
    static sk_sp<GrVkAttachment> Make(GrVkGpu* gpu,
                                      SkISize dimensions,
                                      UsageFlags attachmentUsages,
                                      int sampleCnt,
                                      VkFormat format,
                                      uint32_t mipLevels,
                                      VkImageUsageFlags vkUsageFlags,
                                      GrProtected isProtected,
                                      SkBudgeted);

    GrVkAttachment(GrVkGpu* gpu,
                   SkISize dimensions,
                   UsageFlags supportedUsages,
                   const GrVkImageInfo&,
                   sk_sp<GrBackendSurfaceMutableStateImpl> mutableState,
                   sk_sp<const GrVkImageView> framebufferView,
                   sk_sp<const GrVkImageView> textureView,
                   SkBudgeted);

    GrVkAttachment(GrVkGpu* gpu,
                   SkISize dimensions,
                   UsageFlags supportedUsages,
                   const GrVkImageInfo&,
                   sk_sp<GrBackendSurfaceMutableStateImpl> mutableState,
                   sk_sp<const GrVkImageView> framebufferView,
                   sk_sp<const GrVkImageView> textureView,
                   GrBackendObjectOwnership,
                   GrWrapCacheable,
                   bool forSecondaryCB);

    GrVkGpu* getVkGpu() const;

    void release();

    sk_sp<const GrVkImageView> fFramebufferView;
    sk_sp<const GrVkImageView> fTextureView;

    // Descriptor set used when this is used as an input attachment for reading the dst in blending.
    gr_rp<const GrVkDescriptorSet> fCachedBlendingInputDescSet;
    // Descriptor set used when this is used as an input attachment for loading an msaa attachment.
    gr_rp<const GrVkDescriptorSet> fCachedMSAALoadInputDescSet;
};

#endif
