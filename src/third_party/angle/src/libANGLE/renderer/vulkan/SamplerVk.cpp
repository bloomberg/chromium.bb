//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SamplerVk.cpp:
//    Implements the class methods for SamplerVk.
//

#include "libANGLE/renderer/vulkan/SamplerVk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace rx
{

SamplerVk::SamplerVk(const gl::SamplerState &state) : SamplerImpl(state), mSerial{} {}

SamplerVk::~SamplerVk() = default;

void SamplerVk::onDestroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    contextVk->releaseObject(contextVk->getCurrentQueueSerial(), &mSampler);
}

const vk::Sampler &SamplerVk::getSampler() const
{
    ASSERT(mSampler.valid());
    return mSampler;
}

angle::Result SamplerVk::syncState(const gl::Context *context, const bool dirty)
{
    ContextVk *contextVk = vk::GetImpl(context);

    RendererVk *renderer = contextVk->getRenderer();
    if (mSampler.valid())
    {
        if (!dirty)
        {
            return angle::Result::Continue;
        }
        contextVk->releaseObject(contextVk->getCurrentQueueSerial(), &mSampler);
    }

    const gl::Extensions &extensions = renderer->getNativeExtensions();
    float maxAnisotropy              = mState.getMaxAnisotropy();
    bool anisotropyEnable            = extensions.textureFilterAnisotropic && maxAnisotropy > 1.0f;

    VkSamplerCreateInfo samplerInfo     = {};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.flags                   = 0;
    samplerInfo.magFilter               = gl_vk::GetFilter(mState.getMagFilter());
    samplerInfo.minFilter               = gl_vk::GetFilter(mState.getMinFilter());
    samplerInfo.mipmapMode              = gl_vk::GetSamplerMipmapMode(mState.getMinFilter());
    samplerInfo.addressModeU            = gl_vk::GetSamplerAddressMode(mState.getWrapS());
    samplerInfo.addressModeV            = gl_vk::GetSamplerAddressMode(mState.getWrapT());
    samplerInfo.addressModeW            = gl_vk::GetSamplerAddressMode(mState.getWrapR());
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.anisotropyEnable        = anisotropyEnable;
    samplerInfo.maxAnisotropy           = maxAnisotropy;
    samplerInfo.compareEnable           = mState.getCompareMode() == GL_COMPARE_REF_TO_TEXTURE;
    samplerInfo.compareOp               = gl_vk::GetCompareOp(mState.getCompareFunc());
    samplerInfo.minLod                  = mState.getMinLod();
    samplerInfo.maxLod                  = mState.getMaxLod();
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    ANGLE_VK_TRY(contextVk, mSampler.init(contextVk->getDevice(), samplerInfo));
    // Regenerate the serial on a sampler change.
    mSerial = contextVk->generateTextureSerial();
    return angle::Result::Continue;
}

}  // namespace rx
