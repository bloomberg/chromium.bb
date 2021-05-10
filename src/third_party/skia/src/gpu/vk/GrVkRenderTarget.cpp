/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/vk/GrVkRenderTarget.h"

#include "include/gpu/GrBackendSurface.h"
#include "include/gpu/GrDirectContext.h"
#include "src/gpu/GrBackendSurfaceMutableStateImpl.h"
#include "src/gpu/GrDirectContextPriv.h"
#include "src/gpu/GrResourceProvider.h"
#include "src/gpu/vk/GrVkAttachment.h"
#include "src/gpu/vk/GrVkCommandBuffer.h"
#include "src/gpu/vk/GrVkDescriptorSet.h"
#include "src/gpu/vk/GrVkFramebuffer.h"
#include "src/gpu/vk/GrVkGpu.h"
#include "src/gpu/vk/GrVkImageView.h"
#include "src/gpu/vk/GrVkResourceProvider.h"
#include "src/gpu/vk/GrVkUtil.h"

#include "include/gpu/vk/GrVkTypes.h"

#define VK_CALL(GPU, X) GR_VK_CALL(GPU->vkInterface(), X)

static int renderpass_features_to_index(bool hasResolve, bool hasStencil,
                                        GrVkRenderPass::SelfDependencyFlags selfDepFlags,
                                        GrVkRenderPass::LoadFromResolve loadFromReslove) {
    int index = 0;
    if (hasResolve) {
        index += 1;
    }
    if (hasStencil) {
        index += 2;
    }
    if (selfDepFlags & GrVkRenderPass::SelfDependencyFlags::kForInputAttachment) {
        index += 4;
    }
    if (selfDepFlags & GrVkRenderPass::SelfDependencyFlags::kForNonCoherentAdvBlend) {
        index += 8;
    }
    if (loadFromReslove == GrVkRenderPass::LoadFromResolve::kLoad) {
        index += 16;
    }
    return index;
}

// We're virtually derived from GrSurface (via GrRenderTarget) so its
// constructor must be explicitly called.
GrVkRenderTarget::GrVkRenderTarget(GrVkGpu* gpu,
                                   SkISize dimensions,
                                   int sampleCnt,
                                   const GrVkImageInfo& info,
                                   sk_sp<GrBackendSurfaceMutableStateImpl> mutableState,
                                   sk_sp<GrVkAttachment> msaaAttachment,
                                   sk_sp<const GrVkImageView> colorAttachmentView,
                                   sk_sp<const GrVkImageView> resolveAttachmentView)
        : GrSurface(gpu, dimensions, info.fProtected)
        , GrVkImage(gpu, info, std::move(mutableState), GrBackendObjectOwnership::kBorrowed)
        // for the moment we only support 1:1 color to stencil
        , GrRenderTarget(gpu, dimensions, sampleCnt, info.fProtected)
        , fColorAttachmentView(std::move(colorAttachmentView))
        , fMSAAAttachment(std::move(msaaAttachment))
        , fResolveAttachmentView(std::move(resolveAttachmentView))
        , fCachedFramebuffers()
        , fCachedRenderPasses() {
    SkASSERT((info.fProtected == GrProtected::kYes) == fMSAAAttachment->isProtected());
    SkASSERT(sampleCnt > 1 && sampleCnt == fMSAAAttachment->numSamples());
    SkASSERT(SkToBool(info.fImageUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
    this->setFlags(info);
    this->registerWithCacheWrapped(GrWrapCacheable::kNo);
}

// We're virtually derived from GrSurface (via GrRenderTarget) so its
// constructor must be explicitly called.
GrVkRenderTarget::GrVkRenderTarget(GrVkGpu* gpu,
                                   SkISize dimensions,
                                   int sampleCnt,
                                   const GrVkImageInfo& info,
                                   sk_sp<GrBackendSurfaceMutableStateImpl> mutableState,
                                   sk_sp<GrVkAttachment> msaaAttachment,
                                   sk_sp<const GrVkImageView> colorAttachmentView,
                                   sk_sp<const GrVkImageView> resolveAttachmentView,
                                   GrBackendObjectOwnership ownership)
        : GrSurface(gpu, dimensions, info.fProtected)
        , GrVkImage(gpu, info, std::move(mutableState), ownership)
        // for the moment we only support 1:1 color to stencil
        , GrRenderTarget(gpu, dimensions, sampleCnt, info.fProtected)
        , fColorAttachmentView(std::move(colorAttachmentView))
        , fMSAAAttachment(std::move(msaaAttachment))
        , fResolveAttachmentView(std::move(resolveAttachmentView))
        , fCachedFramebuffers()
        , fCachedRenderPasses() {
    SkASSERT((info.fProtected == GrProtected::kYes) == fMSAAAttachment->isProtected());
    SkASSERT(sampleCnt > 1 && sampleCnt == fMSAAAttachment->numSamples());
    SkASSERT(SkToBool(info.fImageUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
    this->setFlags(info);
}

// We're virtually derived from GrSurface (via GrRenderTarget) so its
// constructor must be explicitly called.
GrVkRenderTarget::GrVkRenderTarget(GrVkGpu* gpu,
                                   SkISize dimensions,
                                   const GrVkImageInfo& info,
                                   sk_sp<GrBackendSurfaceMutableStateImpl> mutableState,
                                   sk_sp<const GrVkImageView> colorAttachmentView)
        : GrSurface(gpu, dimensions, info.fProtected)
        , GrVkImage(gpu, info, std::move(mutableState), GrBackendObjectOwnership::kBorrowed)
        , GrRenderTarget(gpu, dimensions, info.fSampleCount, info.fProtected)
        , fColorAttachmentView(std::move(colorAttachmentView))
        , fCachedFramebuffers()
        , fCachedRenderPasses() {
    SkASSERT(SkToBool(info.fImageUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
    this->setFlags(info);
    this->registerWithCacheWrapped(GrWrapCacheable::kNo);
}

// We're virtually derived from GrSurface (via GrRenderTarget) so its
// constructor must be explicitly called.
GrVkRenderTarget::GrVkRenderTarget(GrVkGpu* gpu,
                                   SkISize dimensions,
                                   const GrVkImageInfo& info,
                                   sk_sp<GrBackendSurfaceMutableStateImpl> mutableState,
                                   sk_sp<const GrVkImageView> colorAttachmentView,
                                   GrBackendObjectOwnership ownership)
        : GrSurface(gpu, dimensions, info.fProtected)
        , GrVkImage(gpu, info, std::move(mutableState), ownership)
        , GrRenderTarget(gpu, dimensions, info.fSampleCount, info.fProtected)
        , fColorAttachmentView(std::move(colorAttachmentView))
        , fCachedFramebuffers()
        , fCachedRenderPasses() {
    SkASSERT(SkToBool(info.fImageUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
    this->setFlags(info);
}

GrVkRenderTarget::GrVkRenderTarget(GrVkGpu* gpu,
                                   SkISize dimensions,
                                   const GrVkImageInfo& info,
                                   sk_sp<GrBackendSurfaceMutableStateImpl> mutableState,
                                   const GrVkRenderPass* renderPass,
                                   VkCommandBuffer secondaryCommandBuffer)
        : GrSurface(gpu, dimensions, info.fProtected)
        , GrVkImage(gpu, info, std::move(mutableState), GrBackendObjectOwnership::kBorrowed, true)
        , GrRenderTarget(gpu, dimensions, 1, info.fProtected)
        , fCachedFramebuffers()
        , fCachedRenderPasses()
        , fSecondaryCommandBuffer(secondaryCommandBuffer) {
    SkASSERT(info.fSampleCount == 1);
    SkASSERT(fSecondaryCommandBuffer != VK_NULL_HANDLE);
    SkASSERT(SkToBool(info.fImageUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
    this->setFlags(info);
    this->registerWithCacheWrapped(GrWrapCacheable::kNo);
    // We use the cached renderpass with no stencil and no extra dependencies to hold the external
    // render pass.
    int exteralRPIndex = renderpass_features_to_index(false, false, SelfDependencyFlags::kNone,
                                                      LoadFromResolve::kNo);
    fCachedRenderPasses[exteralRPIndex] = renderPass;
}

void GrVkRenderTarget::setFlags(const GrVkImageInfo& info) {
    if (info.fImageUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) {
        this->setVkRTSupportsInputAttachment();
    }
}

sk_sp<GrVkRenderTarget> GrVkRenderTarget::MakeWrappedRenderTarget(
        GrVkGpu* gpu,
        SkISize dimensions,
        int sampleCnt,
        const GrVkImageInfo& info,
        sk_sp<GrBackendSurfaceMutableStateImpl> mutableState) {
    SkASSERT(VK_NULL_HANDLE != info.fImage);
    SkASSERT(1 == info.fLevelCount);
    SkASSERT(sampleCnt >= 1 && info.fSampleCount >= 1);

    int wrappedImageSampleCnt = static_cast<int>(info.fSampleCount);
    if (sampleCnt != wrappedImageSampleCnt && wrappedImageSampleCnt != 1) {
        return nullptr;
    }

    VkFormat pixelFormat = info.fFormat;

    // create msaa surface if necessary
    sk_sp<GrVkAttachment> vkMSAAAttachment;
    sk_sp<const GrVkImageView> colorAttachmentView;
    sk_sp<const GrVkImageView> resolveAttachmentView;
    if (sampleCnt != wrappedImageSampleCnt) {
        auto rp = gpu->getContext()->priv().resourceProvider();
        sk_sp<GrAttachment> msaaAttachment =
                rp->makeMSAAAttachment(dimensions, GrBackendFormat::MakeVk(info.fFormat),
                                         sampleCnt, info.fProtected);
        if (!msaaAttachment) {
            return nullptr;
        }
        vkMSAAAttachment = sk_sp<GrVkAttachment>(
                static_cast<GrVkAttachment*>(msaaAttachment.release()));

        colorAttachmentView = sk_ref_sp<const GrVkImageView>(vkMSAAAttachment->view());

        // Create Resolve attachment view. Attachment views on framebuffers must have a single mip
        // level.
        resolveAttachmentView =
                GrVkImageView::Make(gpu, info.fImage, pixelFormat, GrVkImageView::kColor_Type,
                                    /*miplevels=*/1, GrVkYcbcrConversionInfo());
        if (!resolveAttachmentView) {
            return nullptr;
        }
    } else {
        // Attachment views on framebuffers must have a single mip level.
        colorAttachmentView = GrVkImageView::Make(gpu, info.fImage, pixelFormat,
                                                  GrVkImageView::kColor_Type, /*miplevels=*/1,
                                                  GrVkYcbcrConversionInfo());
    }

    if (!colorAttachmentView) {
        return nullptr;
    }

    GrVkRenderTarget* vkRT;
    if (resolveAttachmentView) {
        vkRT = new GrVkRenderTarget(gpu, dimensions, sampleCnt, info, std::move(mutableState),
                                    std::move(vkMSAAAttachment), std::move(colorAttachmentView),
                                    std::move(resolveAttachmentView));
    } else {
        vkRT = new GrVkRenderTarget(gpu, dimensions, info, std::move(mutableState),
                                    std::move(colorAttachmentView));
    }

    return sk_sp<GrVkRenderTarget>(vkRT);
}

sk_sp<GrVkRenderTarget> GrVkRenderTarget::MakeSecondaryCBRenderTarget(
        GrVkGpu* gpu, SkISize dimensions, const GrVkDrawableInfo& vkInfo) {
    const GrVkRenderPass* rp = gpu->resourceProvider().findCompatibleExternalRenderPass(
            vkInfo.fCompatibleRenderPass, vkInfo.fColorAttachmentIndex);
    if (!rp) {
        return nullptr;
    }

    if (vkInfo.fSecondaryCommandBuffer == VK_NULL_HANDLE) {
        return nullptr;
    }

    // We only set the few properties of the GrVkImageInfo that we know like layout and format. The
    // others we keep at the default "null" values.
    GrVkImageInfo info;
    info.fImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    info.fFormat = vkInfo.fFormat;
    info.fImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    sk_sp<GrBackendSurfaceMutableStateImpl> mutableState(new GrBackendSurfaceMutableStateImpl(
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_QUEUE_FAMILY_IGNORED));

    GrVkRenderTarget* vkRT = new GrVkRenderTarget(gpu, dimensions, info, std::move(mutableState),
                                                  rp, vkInfo.fSecondaryCommandBuffer);

    return sk_sp<GrVkRenderTarget>(vkRT);
}

bool GrVkRenderTarget::completeStencilAttachment() {
    SkASSERT(!this->wrapsSecondaryCommandBuffer());
    return true;
}

const GrVkRenderPass* GrVkRenderTarget::externalRenderPass() const {
    SkASSERT(this->wrapsSecondaryCommandBuffer());
    // We use the cached render pass with no attachments or self dependencies to hold the
    // external render pass.
    int exteralRPIndex = renderpass_features_to_index(false, false, SelfDependencyFlags::kNone,
                                                      LoadFromResolve::kNo);
    return fCachedRenderPasses[exteralRPIndex];
}

GrVkResourceProvider::CompatibleRPHandle GrVkRenderTarget::compatibleRenderPassHandle(
        bool withResolve,
        bool withStencil,
        SelfDependencyFlags selfDepFlags,
        LoadFromResolve loadFromResolve) {
    SkASSERT(!this->wrapsSecondaryCommandBuffer());

    int cacheIndex =
            renderpass_features_to_index(withResolve, withStencil, selfDepFlags, loadFromResolve);
    SkASSERT(cacheIndex < GrVkRenderTarget::kNumCachedRenderPasses);

    GrVkResourceProvider::CompatibleRPHandle* pRPHandle;
    pRPHandle = &fCompatibleRPHandles[cacheIndex];

    if (!pRPHandle->isValid()) {
        this->createSimpleRenderPass(withResolve, withStencil, selfDepFlags, loadFromResolve);
    }

#ifdef SK_DEBUG
    const GrVkRenderPass* rp = fCachedRenderPasses[cacheIndex];
    SkASSERT(pRPHandle->isValid() == SkToBool(rp));
    if (rp) {
        SkASSERT(selfDepFlags == rp->selfDependencyFlags());
    }
#endif

    return *pRPHandle;
}

const GrVkRenderPass* GrVkRenderTarget::getSimpleRenderPass(bool withResolve,
                                                            bool withStencil,
                                                            SelfDependencyFlags selfDepFlags,
                                                            LoadFromResolve loadFromResolve) {
    int cacheIndex = renderpass_features_to_index(withResolve, withStencil, selfDepFlags,
                                                  loadFromResolve);
    SkASSERT(cacheIndex < GrVkRenderTarget::kNumCachedRenderPasses);
    if (const GrVkRenderPass* rp = fCachedRenderPasses[cacheIndex]) {
        return rp;
    }

    return this->createSimpleRenderPass(withResolve, withStencil, selfDepFlags, loadFromResolve);
}

const GrVkRenderPass* GrVkRenderTarget::createSimpleRenderPass(bool withResolve,
                                                               bool withStencil,
                                                               SelfDependencyFlags selfDepFlags,
                                                               LoadFromResolve loadFromResolve) {
    SkASSERT(!this->wrapsSecondaryCommandBuffer());

    GrVkResourceProvider& rp = this->getVkGpu()->resourceProvider();
    int cacheIndex = renderpass_features_to_index(withResolve, withStencil, selfDepFlags,
                                                  loadFromResolve);
    SkASSERT(cacheIndex < GrVkRenderTarget::kNumCachedRenderPasses);
    SkASSERT(!fCachedRenderPasses[cacheIndex]);
    fCachedRenderPasses[cacheIndex] = rp.findCompatibleRenderPass(
            *this, &fCompatibleRPHandles[cacheIndex], withResolve, withStencil, selfDepFlags,
            loadFromResolve);
    return fCachedRenderPasses[cacheIndex];
}

const GrVkFramebuffer* GrVkRenderTarget::getFramebuffer(bool withResolve,
                                                        bool withStencil,
                                                        SelfDependencyFlags selfDepFlags,
                                                        LoadFromResolve loadFromResolve) {
    int cacheIndex =
            renderpass_features_to_index(withResolve, withStencil, selfDepFlags, loadFromResolve);
    SkASSERT(cacheIndex < GrVkRenderTarget::kNumCachedRenderPasses);
    if (auto fb = fCachedFramebuffers[cacheIndex]) {
        return fb;
    }

    return this->createFramebuffer(withResolve, withStencil, selfDepFlags, loadFromResolve);
}

const GrVkFramebuffer* GrVkRenderTarget::createFramebuffer(bool withResolve,
                                                           bool withStencil,
                                                           SelfDependencyFlags selfDepFlags,
                                                           LoadFromResolve loadFromResolve) {
    SkASSERT(!this->wrapsSecondaryCommandBuffer());
    GrVkGpu* gpu = this->getVkGpu();

    const GrVkRenderPass* renderPass =
            this->getSimpleRenderPass(withResolve, withStencil, selfDepFlags, loadFromResolve);
    if (!renderPass) {
        return nullptr;
    }

    int cacheIndex =
            renderpass_features_to_index(withResolve, withStencil, selfDepFlags, loadFromResolve);
    SkASSERT(cacheIndex < GrVkRenderTarget::kNumCachedRenderPasses);

    const GrVkImageView* resolveView = withResolve ? this->resolveAttachmentView() : nullptr;

    // Stencil attachment view is stored in the base RT stencil attachment
    const GrVkImageView* stencilView = withStencil ? this->stencilAttachmentView() : nullptr;
    fCachedFramebuffers[cacheIndex] =
            GrVkFramebuffer::Create(gpu, this->width(), this->height(), renderPass,
                                    fColorAttachmentView.get(), resolveView, stencilView);

    return fCachedFramebuffers[cacheIndex];
}

void GrVkRenderTarget::getAttachmentsDescriptor(GrVkRenderPass::AttachmentsDescriptor* desc,
                                                GrVkRenderPass::AttachmentFlags* attachmentFlags,
                                                bool withResolve,
                                                bool withStencil) const {
    SkASSERT(!this->wrapsSecondaryCommandBuffer());
    desc->fColor.fFormat = this->imageFormat();
    desc->fColor.fSamples = this->numSamples();
    *attachmentFlags = GrVkRenderPass::kColor_AttachmentFlag;
    uint32_t attachmentCount = 1;

    if (withResolve) {
        desc->fResolve.fFormat = desc->fColor.fFormat;
        desc->fResolve.fSamples = 1;
        *attachmentFlags |= GrVkRenderPass::kResolve_AttachmentFlag;
        ++attachmentCount;
    }

    if (withStencil) {
        const GrAttachment* stencil = this->getStencilAttachment();
        SkASSERT(stencil);
        const GrVkAttachment* vkStencil = static_cast<const GrVkAttachment*>(stencil);
        desc->fStencil.fFormat = vkStencil->imageFormat();
        desc->fStencil.fSamples = vkStencil->numSamples();
#ifdef SK_DEBUG
        if (this->getVkGpu()->caps()->mixedSamplesSupport()) {
            SkASSERT(desc->fStencil.fSamples >= desc->fColor.fSamples);
        } else {
            SkASSERT(desc->fStencil.fSamples == desc->fColor.fSamples);
        }
#endif
        *attachmentFlags |= GrVkRenderPass::kStencil_AttachmentFlag;
        ++attachmentCount;
    }
    desc->fAttachmentCount = attachmentCount;
}

void GrVkRenderTarget::ReconstructAttachmentsDescriptor(const GrVkCaps& vkCaps,
                                                        const GrProgramInfo& programInfo,
                                                        GrVkRenderPass::AttachmentsDescriptor* desc,
                                                        GrVkRenderPass::AttachmentFlags* flags) {
    VkFormat format;
    SkAssertResult(programInfo.backendFormat().asVkFormat(&format));

    desc->fColor.fFormat = format;
    desc->fColor.fSamples = programInfo.numSamples();
    *flags = GrVkRenderPass::kColor_AttachmentFlag;
    uint32_t attachmentCount = 1;

    if (programInfo.targetSupportsVkResolveLoad() && vkCaps.preferDiscardableMSAAAttachment()) {
        desc->fResolve.fFormat = desc->fColor.fFormat;
        desc->fResolve.fSamples = 1;
        *flags |= GrVkRenderPass::kResolve_AttachmentFlag;
        ++attachmentCount;
    }

    SkASSERT(!programInfo.isStencilEnabled() || programInfo.numStencilSamples());
    if (programInfo.numStencilSamples()) {
        VkFormat stencilFormat = vkCaps.preferredStencilFormat();
        desc->fStencil.fFormat = stencilFormat;
        desc->fStencil.fSamples = programInfo.numStencilSamples();
#ifdef SK_DEBUG
        if (vkCaps.mixedSamplesSupport()) {
            SkASSERT(desc->fStencil.fSamples >= desc->fColor.fSamples);
        } else {
            SkASSERT(desc->fStencil.fSamples == desc->fColor.fSamples);
        }
#endif
        *flags |= GrVkRenderPass::kStencil_AttachmentFlag;
        ++attachmentCount;
    }
    desc->fAttachmentCount = attachmentCount;
}

const GrVkDescriptorSet* GrVkRenderTarget::inputDescSet(GrVkGpu* gpu, bool forResolve) {
    SkASSERT(this->supportsInputAttachmentUsage());
    SkASSERT(this->numSamples() <= 1 || forResolve);
    if (fCachedInputDescriptorSet) {
        return fCachedInputDescriptorSet;
    }
    fCachedInputDescriptorSet = gpu->resourceProvider().getInputDescriptorSet();

    if (!fCachedInputDescriptorSet) {
        return nullptr;
    }

    VkDescriptorImageInfo imageInfo;
    memset(&imageInfo, 0, sizeof(VkDescriptorImageInfo));
    imageInfo.sampler = VK_NULL_HANDLE;
    imageInfo.imageView = forResolve ? this->resolveAttachmentView()->imageView()
                                     : this->colorAttachmentView()->imageView();
    imageInfo.imageLayout = forResolve ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                       : VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writeInfo;
    memset(&writeInfo, 0, sizeof(VkWriteDescriptorSet));
    writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.pNext = nullptr;
    writeInfo.dstSet = *fCachedInputDescriptorSet->descriptorSet();
    writeInfo.dstBinding = GrVkUniformHandler::kInputBinding;
    writeInfo.dstArrayElement = 0;
    writeInfo.descriptorCount = 1;
    writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    writeInfo.pImageInfo = &imageInfo;
    writeInfo.pBufferInfo = nullptr;
    writeInfo.pTexelBufferView = nullptr;

    GR_VK_CALL(gpu->vkInterface(), UpdateDescriptorSets(gpu->device(), 1, &writeInfo, 0, nullptr));

    return fCachedInputDescriptorSet;
}

GrVkRenderTarget::~GrVkRenderTarget() {
    // either release or abandon should have been called by the owner of this object.
    SkASSERT(!fResolveAttachmentView);
    SkASSERT(!fColorAttachmentView);

    for (int i = 0; i < kNumCachedRenderPasses; ++i) {
        SkASSERT(!fCachedFramebuffers[i]);
        SkASSERT(!fCachedRenderPasses[i]);
    }

    SkASSERT(!fCachedInputDescriptorSet);
}

void GrVkRenderTarget::addResources(GrVkCommandBuffer& commandBuffer,
                                    const GrVkRenderPass& renderPass) {
    commandBuffer.addGrSurface(sk_ref_sp<const GrSurface>(this));
    commandBuffer.addResource(this->getFramebuffer(renderPass));
    commandBuffer.addResource(this->colorAttachmentView());
    commandBuffer.addResource(fMSAAAttachment ? fMSAAAttachment->resource() : this->resource());
    if (this->stencilImageResource()) {
        commandBuffer.addResource(this->stencilImageResource());
        commandBuffer.addResource(this->stencilAttachmentView());
    }
    if (renderPass.hasResolveAttachment()) {
        commandBuffer.addResource(this->resource());
        commandBuffer.addResource(this->resolveAttachmentView());
    }
}

void GrVkRenderTarget::releaseInternalObjects() {
    if (fMSAAAttachment) {
        fMSAAAttachment.reset();
    }

    if (fResolveAttachmentView) {
        fResolveAttachmentView.reset();
    }
    if (fColorAttachmentView) {
        fColorAttachmentView.reset();
    }

    for (int i = 0; i < kNumCachedRenderPasses; ++i) {
        if (fCachedFramebuffers[i]) {
            fCachedFramebuffers[i]->unref();
            fCachedFramebuffers[i] = nullptr;
        }
        if (fCachedRenderPasses[i]) {
            fCachedRenderPasses[i]->unref();
            fCachedRenderPasses[i] = nullptr;
        }
    }

    if (fCachedInputDescriptorSet) {
        fCachedInputDescriptorSet->recycle();
        fCachedInputDescriptorSet = nullptr;
    }

    for (int i = 0; i < fGrSecondaryCommandBuffers.count(); ++i) {
        SkASSERT(fGrSecondaryCommandBuffers[i]);
        fGrSecondaryCommandBuffers[i]->releaseResources();
    }
    fGrSecondaryCommandBuffers.reset();
}

void GrVkRenderTarget::onRelease() {
    this->releaseInternalObjects();
    this->releaseImage();
    GrRenderTarget::onRelease();
}

void GrVkRenderTarget::onAbandon() {
    this->releaseInternalObjects();
    this->releaseImage();
    GrRenderTarget::onAbandon();
}

GrBackendRenderTarget GrVkRenderTarget::getBackendRenderTarget() const {
    SkASSERT(!this->wrapsSecondaryCommandBuffer());
    return GrBackendRenderTarget(this->width(), this->height(), fInfo, this->getMutableState());
}

GrVkImage* GrVkRenderTarget::msaaImage() {
    if (this->numSamples() == 1) {
        SkASSERT(fColorAttachmentView && !fMSAAAttachment && !fResolveAttachmentView);
        return nullptr;
    }
    if (!fMSAAAttachment) {
        // In this case *this* object is MSAA (there is not a separate color and resolve buffer)
        SkASSERT(!fResolveAttachmentView);
        return this;
    }
    SkASSERT(fMSAAAttachment);
    return fMSAAAttachment.get();
}

const GrManagedResource* GrVkRenderTarget::stencilImageResource() const {
    SkASSERT(!this->wrapsSecondaryCommandBuffer());
    const GrAttachment* stencil = this->getStencilAttachment();
    if (stencil) {
        const GrVkAttachment* vkStencil = static_cast<const GrVkAttachment*>(stencil);
        return vkStencil->imageResource();
    }

    return nullptr;
}

const GrVkImageView* GrVkRenderTarget::stencilAttachmentView() const {
    SkASSERT(!this->wrapsSecondaryCommandBuffer());
    const GrAttachment* stencil = this->getStencilAttachment();
    if (stencil) {
        const GrVkAttachment* vkStencil = static_cast<const GrVkAttachment*>(stencil);
        return vkStencil->view();
    }

    return nullptr;
}

GrVkImage* GrVkRenderTarget::colorAttachmentImage() {
    if (fMSAAAttachment) {
        return fMSAAAttachment.get();
    }
    return this;
}

GrVkGpu* GrVkRenderTarget::getVkGpu() const {
    SkASSERT(!this->wasDestroyed());
    return static_cast<GrVkGpu*>(this->getGpu());
}
