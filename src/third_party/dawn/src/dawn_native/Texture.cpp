// Copyright 2017 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dawn_native/Texture.h"

#include <algorithm>

#include "common/Assert.h"
#include "common/Constants.h"
#include "common/Math.h"
#include "dawn_native/Adapter.h"
#include "dawn_native/ChainUtils_autogen.h"
#include "dawn_native/Device.h"
#include "dawn_native/EnumMaskIterator.h"
#include "dawn_native/PassResourceUsage.h"
#include "dawn_native/ValidationUtils_autogen.h"

namespace dawn_native {
    namespace {
        // WebGPU currently does not have texture format reinterpretation. If it does, the
        // code to check for it might go here.
        MaybeError ValidateTextureViewFormatCompatibility(const TextureBase* texture,
                                                          const TextureViewDescriptor* descriptor) {
            if (texture->GetFormat().format != descriptor->format) {
                if (descriptor->aspect != wgpu::TextureAspect::All &&
                    texture->GetFormat().GetAspectInfo(descriptor->aspect).format ==
                        descriptor->format) {
                    return {};
                }

                return DAWN_VALIDATION_ERROR(
                    "The format of texture view is not compatible to the original texture");
            }

            return {};
        }

        // TODO(crbug.com/dawn/814): Implement for 1D texture.
        bool IsTextureViewDimensionCompatibleWithTextureDimension(
            wgpu::TextureViewDimension textureViewDimension,
            wgpu::TextureDimension textureDimension) {
            switch (textureViewDimension) {
                case wgpu::TextureViewDimension::e2D:
                case wgpu::TextureViewDimension::e2DArray:
                case wgpu::TextureViewDimension::Cube:
                case wgpu::TextureViewDimension::CubeArray:
                    return textureDimension == wgpu::TextureDimension::e2D;

                case wgpu::TextureViewDimension::e3D:
                    return textureDimension == wgpu::TextureDimension::e3D;

                case wgpu::TextureViewDimension::e1D:
                case wgpu::TextureViewDimension::Undefined:
                    UNREACHABLE();
            }
        }

        // TODO(crbug.com/dawn/814): Implement for 1D texture.
        bool IsArrayLayerValidForTextureViewDimension(
            wgpu::TextureViewDimension textureViewDimension,
            uint32_t textureViewArrayLayer) {
            switch (textureViewDimension) {
                case wgpu::TextureViewDimension::e2D:
                case wgpu::TextureViewDimension::e3D:
                    return textureViewArrayLayer == 1u;
                case wgpu::TextureViewDimension::e2DArray:
                    return true;
                case wgpu::TextureViewDimension::Cube:
                    return textureViewArrayLayer == 6u;
                case wgpu::TextureViewDimension::CubeArray:
                    return textureViewArrayLayer % 6 == 0;

                case wgpu::TextureViewDimension::e1D:
                case wgpu::TextureViewDimension::Undefined:
                    UNREACHABLE();
            }
        }

        bool IsTextureSizeValidForTextureViewDimension(
            wgpu::TextureViewDimension textureViewDimension,
            const Extent3D& textureSize) {
            switch (textureViewDimension) {
                case wgpu::TextureViewDimension::Cube:
                case wgpu::TextureViewDimension::CubeArray:
                    return textureSize.width == textureSize.height;
                case wgpu::TextureViewDimension::e2D:
                case wgpu::TextureViewDimension::e2DArray:
                case wgpu::TextureViewDimension::e3D:
                    return true;

                case wgpu::TextureViewDimension::e1D:
                case wgpu::TextureViewDimension::Undefined:
                    UNREACHABLE();
            }
        }

        MaybeError ValidateSampleCount(const TextureDescriptor* descriptor,
                                       wgpu::TextureUsage usage,
                                       const Format* format) {
            if (!IsValidSampleCount(descriptor->sampleCount)) {
                return DAWN_VALIDATION_ERROR("The sample count of the texture is not supported.");
            }

            if (descriptor->sampleCount > 1) {
                if (descriptor->mipLevelCount > 1) {
                    return DAWN_VALIDATION_ERROR(
                        "The mipmap level count of a multisampled texture must be 1.");
                }

                // Multisampled 1D and 3D textures are not supported in D3D12/Metal/Vulkan.
                // Multisampled 2D array texture is not supported because on Metal it requires the
                // version of macOS be greater than 10.14.
                if (descriptor->dimension != wgpu::TextureDimension::e2D ||
                    descriptor->size.depthOrArrayLayers > 1) {
                    return DAWN_VALIDATION_ERROR("Multisampled texture must be 2D with depth=1");
                }

                // If a format can support multisample, it must be renderable. Because Vulkan
                // requires that if the format is not color-renderable or depth/stencil renderable,
                // sampleCount must be 1.
                if (!format->isRenderable) {
                    return DAWN_VALIDATION_ERROR("This format cannot support multisample.");
                }
                // Compressed formats are not renderable. They cannot support multisample.
                ASSERT(!format->isCompressed);

                if (usage & wgpu::TextureUsage::StorageBinding) {
                    return DAWN_VALIDATION_ERROR(
                        "The sample counts of the storage textures must be 1.");
                }
            }

            return {};
        }

        MaybeError ValidateTextureViewDimensionCompatibility(
            const TextureBase* texture,
            const TextureViewDescriptor* descriptor) {
            if (!IsArrayLayerValidForTextureViewDimension(descriptor->dimension,
                                                          descriptor->arrayLayerCount)) {
                return DAWN_VALIDATION_ERROR(
                    "The dimension of the texture view is not compatible with the layer count");
            }

            if (!IsTextureViewDimensionCompatibleWithTextureDimension(descriptor->dimension,
                                                                      texture->GetDimension())) {
                return DAWN_VALIDATION_ERROR(
                    "The dimension of the texture view is not compatible with the dimension of the"
                    "original texture");
            }

            if (!IsTextureSizeValidForTextureViewDimension(descriptor->dimension,
                                                           texture->GetSize())) {
                return DAWN_VALIDATION_ERROR(
                    "The dimension of the texture view is not compatible with the size of the"
                    "original texture");
            }

            return {};
        }

        MaybeError ValidateTextureSize(const TextureDescriptor* descriptor, const Format* format) {
            ASSERT(descriptor->size.width != 0 && descriptor->size.height != 0 &&
                   descriptor->size.depthOrArrayLayers != 0);

            Extent3D maxExtent;
            switch (descriptor->dimension) {
                case wgpu::TextureDimension::e2D:
                    maxExtent = {kMaxTextureDimension2D, kMaxTextureDimension2D,
                                 kMaxTextureArrayLayers};
                    break;
                case wgpu::TextureDimension::e3D:
                    maxExtent = {kMaxTextureDimension3D, kMaxTextureDimension3D,
                                 kMaxTextureDimension3D};
                    break;
                case wgpu::TextureDimension::e1D:
                default:
                    UNREACHABLE();
            }
            if (descriptor->size.width > maxExtent.width ||
                descriptor->size.height > maxExtent.height ||
                descriptor->size.depthOrArrayLayers > maxExtent.depthOrArrayLayers) {
                return DAWN_VALIDATION_ERROR("Texture dimension (width, height or depth) exceeded");
            }

            uint32_t maxMippedDimension = descriptor->size.width;
            if (descriptor->dimension != wgpu::TextureDimension::e1D) {
                maxMippedDimension = std::max(maxMippedDimension, descriptor->size.height);
            }
            if (descriptor->dimension == wgpu::TextureDimension::e3D) {
                maxMippedDimension =
                    std::max(maxMippedDimension, descriptor->size.depthOrArrayLayers);
            }
            if (Log2(maxMippedDimension) + 1 < descriptor->mipLevelCount) {
                return DAWN_VALIDATION_ERROR("Texture has too many mip levels");
            }
            ASSERT(descriptor->mipLevelCount <= kMaxTexture2DMipLevels);

            if (format->isCompressed) {
                const TexelBlockInfo& blockInfo =
                    format->GetAspectInfo(wgpu::TextureAspect::All).block;
                if (descriptor->size.width % blockInfo.width != 0 ||
                    descriptor->size.height % blockInfo.height != 0) {
                    return DAWN_VALIDATION_ERROR(
                        "The size of the texture is incompatible with the texture format");
                }
            }

            return {};
        }

        MaybeError ValidateTextureUsage(const TextureDescriptor* descriptor,
                                        wgpu::TextureUsage usage,
                                        const Format* format) {
            DAWN_TRY(dawn_native::ValidateTextureUsage(usage));

            constexpr wgpu::TextureUsage kValidCompressedUsages =
                wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc |
                wgpu::TextureUsage::CopyDst;
            if (format->isCompressed && !IsSubset(usage, kValidCompressedUsages)) {
                return DAWN_VALIDATION_ERROR(
                    "Compressed texture format is incompatible with the texture usage");
            }

            if (!format->isRenderable && (usage & wgpu::TextureUsage::RenderAttachment)) {
                return DAWN_VALIDATION_ERROR(
                    "Non-renderable format used with RenderAttachment usage");
            }

            if (!format->supportsStorageUsage && (usage & wgpu::TextureUsage::StorageBinding)) {
                return DAWN_VALIDATION_ERROR("Format cannot be used in storage textures");
            }

            constexpr wgpu::TextureUsage kValidMultiPlanarUsages =
                wgpu::TextureUsage::TextureBinding;
            if (format->IsMultiPlanar() && !IsSubset(usage, kValidMultiPlanarUsages)) {
                return DAWN_VALIDATION_ERROR("Multi-planar format doesn't have valid usage.");
            }

            return {};
        }

    }  // anonymous namespace

    MaybeError ValidateTextureDescriptor(const DeviceBase* device,
                                         const TextureDescriptor* descriptor) {
        DAWN_TRY(ValidateSingleSType(descriptor->nextInChain,
                                     wgpu::SType::DawnTextureInternalUsageDescriptor));

        const DawnTextureInternalUsageDescriptor* internalUsageDesc = nullptr;
        FindInChain(descriptor->nextInChain, &internalUsageDesc);

        if (descriptor->dimension == wgpu::TextureDimension::e1D) {
            return DAWN_VALIDATION_ERROR("1D textures aren't supported (yet).");
        }

        if (internalUsageDesc != nullptr &&
            !device->IsExtensionEnabled(Extension::DawnInternalUsages)) {
            return DAWN_VALIDATION_ERROR("The dawn-internal-usages feature is not enabled");
        }

        const Format* format;
        DAWN_TRY_ASSIGN(format, device->GetInternalFormat(descriptor->format));

        wgpu::TextureUsage usage = descriptor->usage;
        if (internalUsageDesc != nullptr) {
            usage |= internalUsageDesc->internalUsage;
        }

        DAWN_TRY(ValidateTextureUsage(descriptor, usage, format));
        DAWN_TRY(ValidateTextureDimension(descriptor->dimension));
        DAWN_TRY(ValidateSampleCount(descriptor, usage, format));

        if (descriptor->size.width == 0 || descriptor->size.height == 0 ||
            descriptor->size.depthOrArrayLayers == 0 || descriptor->mipLevelCount == 0) {
            return DAWN_VALIDATION_ERROR("Cannot create an empty texture");
        }

        if (descriptor->dimension != wgpu::TextureDimension::e2D && format->isCompressed) {
            return DAWN_VALIDATION_ERROR("Compressed texture must be 2D");
        }

        // Depth/stencil formats are valid for 2D textures only. Metal has this limit. And D3D12
        // doesn't support depth/stencil formats on 3D textures.
        if (descriptor->dimension != wgpu::TextureDimension::e2D &&
            (format->aspects & (Aspect::Depth | Aspect::Stencil)) != 0) {
            return DAWN_VALIDATION_ERROR("Depth/stencil formats are valid for 2D textures only");
        }

        DAWN_TRY(ValidateTextureSize(descriptor, format));

        if (device->IsToggleEnabled(Toggle::DisallowUnsafeAPIs) && format->HasStencil() &&
            descriptor->mipLevelCount > 1 &&
            device->GetAdapter()->GetBackendType() == wgpu::BackendType::Metal) {
            // TODO(crbug.com/dawn/838): Implement a workaround for this issue.
            // Readbacks from the non-zero mip of a stencil texture may contain
            // garbage data.
            return DAWN_VALIDATION_ERROR(
                "crbug.com/dawn/838: Stencil textures with more than one mip level are "
                "disabled on Metal.");
        }

        return {};
    }

    MaybeError ValidateTextureViewDescriptor(const DeviceBase* device,
                                             const TextureBase* texture,
                                             const TextureViewDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        // Parent texture should have been already validated.
        ASSERT(texture);
        ASSERT(!texture->IsError());

        DAWN_TRY(ValidateTextureViewDimension(descriptor->dimension));
        if (descriptor->dimension == wgpu::TextureViewDimension::e1D) {
            return DAWN_VALIDATION_ERROR("1D texture views aren't supported (yet).");
        }

        DAWN_TRY(ValidateTextureFormat(descriptor->format));

        DAWN_TRY(ValidateTextureAspect(descriptor->aspect));
        if (SelectFormatAspects(texture->GetFormat(), descriptor->aspect) == Aspect::None) {
            return DAWN_VALIDATION_ERROR("Texture does not have selected aspect for texture view.");
        }

        if (descriptor->arrayLayerCount == 0 || descriptor->mipLevelCount == 0) {
            return DAWN_VALIDATION_ERROR("Cannot create an empty texture view");
        }

        if (uint64_t(descriptor->baseArrayLayer) + uint64_t(descriptor->arrayLayerCount) >
            uint64_t(texture->GetArrayLayers())) {
            return DAWN_VALIDATION_ERROR("Texture view array-layer out of range");
        }

        if (uint64_t(descriptor->baseMipLevel) + uint64_t(descriptor->mipLevelCount) >
            uint64_t(texture->GetNumMipLevels())) {
            return DAWN_VALIDATION_ERROR("Texture view mip-level out of range");
        }

        DAWN_TRY(ValidateTextureViewFormatCompatibility(texture, descriptor));
        DAWN_TRY(ValidateTextureViewDimensionCompatibility(texture, descriptor));

        return {};
    }

    TextureViewDescriptor GetTextureViewDescriptorWithDefaults(
        const TextureBase* texture,
        const TextureViewDescriptor* descriptor) {
        ASSERT(texture);

        TextureViewDescriptor desc = {};
        if (descriptor) {
            desc = *descriptor;
        }

        // The default value for the view dimension depends on the texture's dimension with a
        // special case for 2DArray being chosen automatically if arrayLayerCount is unspecified.
        if (desc.dimension == wgpu::TextureViewDimension::Undefined) {
            switch (texture->GetDimension()) {
                case wgpu::TextureDimension::e1D:
                    desc.dimension = wgpu::TextureViewDimension::e1D;
                    break;

                case wgpu::TextureDimension::e2D:
                    desc.dimension = wgpu::TextureViewDimension::e2D;
                    break;

                case wgpu::TextureDimension::e3D:
                    desc.dimension = wgpu::TextureViewDimension::e3D;
                    break;
            }
        }

        if (desc.format == wgpu::TextureFormat::Undefined) {
            // TODO(dawn:682): Use GetAspectInfo(aspect).
            desc.format = texture->GetFormat().format;
        }
        if (desc.arrayLayerCount == 0) {
            switch (desc.dimension) {
                case wgpu::TextureViewDimension::e1D:
                case wgpu::TextureViewDimension::e2D:
                case wgpu::TextureViewDimension::e3D:
                    desc.arrayLayerCount = 1;
                    break;
                case wgpu::TextureViewDimension::Cube:
                    desc.arrayLayerCount = 6;
                    break;
                case wgpu::TextureViewDimension::e2DArray:
                case wgpu::TextureViewDimension::CubeArray:
                    desc.arrayLayerCount = texture->GetArrayLayers() - desc.baseArrayLayer;
                    break;
                default:
                    // We don't put UNREACHABLE() here because we validate enums only after this
                    // function sets default values. Otherwise, the UNREACHABLE() will be hit.
                    break;
            }
        }
        if (desc.mipLevelCount == 0) {
            desc.mipLevelCount = texture->GetNumMipLevels() - desc.baseMipLevel;
        }
        return desc;
    }

    // WebGPU only supports sample counts of 1 and 4. We could expand to more based on
    // platform support, but it would probably be an extension.
    bool IsValidSampleCount(uint32_t sampleCount) {
        switch (sampleCount) {
            case 1:
            case 4:
                return true;

            default:
                return false;
        }
    }

    // TextureBase

    TextureBase::TextureBase(DeviceBase* device,
                             const TextureDescriptor* descriptor,
                             TextureState state)
        : ObjectBase(device),
          mDimension(descriptor->dimension),
          mFormat(device->GetValidInternalFormat(descriptor->format)),
          mSize(descriptor->size),
          mMipLevelCount(descriptor->mipLevelCount),
          mSampleCount(descriptor->sampleCount),
          mUsage(descriptor->usage),
          mInternalUsage(mUsage),
          mState(state) {
        uint32_t subresourceCount =
            mMipLevelCount * GetArrayLayers() * GetAspectCount(mFormat.aspects);
        mIsSubresourceContentInitializedAtIndex = std::vector<bool>(subresourceCount, false);

        const DawnTextureInternalUsageDescriptor* internalUsageDesc = nullptr;
        FindInChain(descriptor->nextInChain, &internalUsageDesc);
        if (internalUsageDesc != nullptr) {
            mInternalUsage |= internalUsageDesc->internalUsage;
        }

        // Add readonly storage usage if the texture has a storage usage. The validation rules in
        // ValidateSyncScopeResourceUsage will make sure we don't use both at the same time.
        if (mInternalUsage & wgpu::TextureUsage::StorageBinding) {
            mInternalUsage |= kReadOnlyStorageTexture;
        }
    }

    static Format kUnusedFormat;

    TextureBase::TextureBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : ObjectBase(device, tag), mFormat(kUnusedFormat) {
    }

    // static
    TextureBase* TextureBase::MakeError(DeviceBase* device) {
        return new TextureBase(device, ObjectBase::kError);
    }

    wgpu::TextureDimension TextureBase::GetDimension() const {
        ASSERT(!IsError());
        return mDimension;
    }

    const Format& TextureBase::GetFormat() const {
        ASSERT(!IsError());
        return mFormat;
    }
    const Extent3D& TextureBase::GetSize() const {
        ASSERT(!IsError());
        return mSize;
    }
    uint32_t TextureBase::GetWidth() const {
        ASSERT(!IsError());
        return mSize.width;
    }
    uint32_t TextureBase::GetHeight() const {
        ASSERT(!IsError());
        ASSERT(mDimension != wgpu::TextureDimension::e1D);
        return mSize.height;
    }
    uint32_t TextureBase::GetDepth() const {
        ASSERT(!IsError());
        ASSERT(mDimension == wgpu::TextureDimension::e3D);
        return mSize.depthOrArrayLayers;
    }
    uint32_t TextureBase::GetArrayLayers() const {
        ASSERT(!IsError());
        // TODO(crbug.com/dawn/814): Update for 1D textures when they are supported.
        ASSERT(mDimension != wgpu::TextureDimension::e1D);
        if (mDimension == wgpu::TextureDimension::e3D) {
            return 1;
        }
        return mSize.depthOrArrayLayers;
    }
    uint32_t TextureBase::GetNumMipLevels() const {
        ASSERT(!IsError());
        return mMipLevelCount;
    }
    SubresourceRange TextureBase::GetAllSubresources() const {
        ASSERT(!IsError());
        return {mFormat.aspects, {0, GetArrayLayers()}, {0, mMipLevelCount}};
    }
    uint32_t TextureBase::GetSampleCount() const {
        ASSERT(!IsError());
        return mSampleCount;
    }
    uint32_t TextureBase::GetSubresourceCount() const {
        ASSERT(!IsError());
        return static_cast<uint32_t>(mIsSubresourceContentInitializedAtIndex.size());
    }
    wgpu::TextureUsage TextureBase::GetUsage() const {
        ASSERT(!IsError());
        return mUsage;
    }
    wgpu::TextureUsage TextureBase::GetInternalUsage() const {
        ASSERT(!IsError());
        return mInternalUsage;
    }

    TextureBase::TextureState TextureBase::GetTextureState() const {
        ASSERT(!IsError());
        return mState;
    }

    uint32_t TextureBase::GetSubresourceIndex(uint32_t mipLevel,
                                              uint32_t arraySlice,
                                              Aspect aspect) const {
        ASSERT(arraySlice <= kMaxTextureArrayLayers);
        ASSERT(mipLevel <= kMaxTexture2DMipLevels);
        ASSERT(HasOneBit(aspect));
        static_assert(
            kMaxTexture2DMipLevels <= std::numeric_limits<uint32_t>::max() / kMaxTextureArrayLayers,
            "texture size overflows uint32_t");
        return mipLevel +
               GetNumMipLevels() * (arraySlice + GetArrayLayers() * GetAspectIndex(aspect));
    }

    bool TextureBase::IsSubresourceContentInitialized(const SubresourceRange& range) const {
        ASSERT(!IsError());
        for (Aspect aspect : IterateEnumMask(range.aspects)) {
            for (uint32_t arrayLayer = range.baseArrayLayer;
                 arrayLayer < range.baseArrayLayer + range.layerCount; ++arrayLayer) {
                for (uint32_t mipLevel = range.baseMipLevel;
                     mipLevel < range.baseMipLevel + range.levelCount; ++mipLevel) {
                    uint32_t subresourceIndex = GetSubresourceIndex(mipLevel, arrayLayer, aspect);
                    ASSERT(subresourceIndex < mIsSubresourceContentInitializedAtIndex.size());
                    if (!mIsSubresourceContentInitializedAtIndex[subresourceIndex]) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    void TextureBase::SetIsSubresourceContentInitialized(bool isInitialized,
                                                         const SubresourceRange& range) {
        ASSERT(!IsError());
        for (Aspect aspect : IterateEnumMask(range.aspects)) {
            for (uint32_t arrayLayer = range.baseArrayLayer;
                 arrayLayer < range.baseArrayLayer + range.layerCount; ++arrayLayer) {
                for (uint32_t mipLevel = range.baseMipLevel;
                     mipLevel < range.baseMipLevel + range.levelCount; ++mipLevel) {
                    uint32_t subresourceIndex = GetSubresourceIndex(mipLevel, arrayLayer, aspect);
                    ASSERT(subresourceIndex < mIsSubresourceContentInitializedAtIndex.size());
                    mIsSubresourceContentInitializedAtIndex[subresourceIndex] = isInitialized;
                }
            }
        }
    }

    MaybeError TextureBase::ValidateCanUseInSubmitNow() const {
        ASSERT(!IsError());
        if (mState == TextureState::Destroyed) {
            return DAWN_VALIDATION_ERROR("Destroyed texture used in a submit");
        }
        return {};
    }

    bool TextureBase::IsMultisampledTexture() const {
        ASSERT(!IsError());
        return mSampleCount > 1;
    }

    Extent3D TextureBase::GetMipLevelVirtualSize(uint32_t level) const {
        Extent3D extent = {std::max(mSize.width >> level, 1u), 1u, 1u};
        if (mDimension == wgpu::TextureDimension::e1D) {
            return extent;
        }

        extent.height = std::max(mSize.height >> level, 1u);
        if (mDimension == wgpu::TextureDimension::e2D) {
            return extent;
        }

        extent.depthOrArrayLayers = std::max(mSize.depthOrArrayLayers >> level, 1u);
        return extent;
    }

    Extent3D TextureBase::GetMipLevelPhysicalSize(uint32_t level) const {
        Extent3D extent = GetMipLevelVirtualSize(level);

        // Compressed Textures will have paddings if their width or height is not a multiple of
        // 4 at non-zero mipmap levels.
        if (mFormat.isCompressed && level != 0) {
            // If |level| is non-zero, then each dimension of |extent| is at most half of
            // the max texture dimension. Computations here which add the block width/height
            // to the extent cannot overflow.
            const TexelBlockInfo& blockInfo = mFormat.GetAspectInfo(wgpu::TextureAspect::All).block;
            extent.width = (extent.width + blockInfo.width - 1) / blockInfo.width * blockInfo.width;
            extent.height =
                (extent.height + blockInfo.height - 1) / blockInfo.height * blockInfo.height;
        }

        return extent;
    }

    Extent3D TextureBase::ClampToMipLevelVirtualSize(uint32_t level,
                                                     const Origin3D& origin,
                                                     const Extent3D& extent) const {
        const Extent3D virtualSizeAtLevel = GetMipLevelVirtualSize(level);
        ASSERT(origin.x <= virtualSizeAtLevel.width);
        ASSERT(origin.y <= virtualSizeAtLevel.height);
        uint32_t clampedCopyExtentWidth = (extent.width > virtualSizeAtLevel.width - origin.x)
                                              ? (virtualSizeAtLevel.width - origin.x)
                                              : extent.width;
        uint32_t clampedCopyExtentHeight = (extent.height > virtualSizeAtLevel.height - origin.y)
                                               ? (virtualSizeAtLevel.height - origin.y)
                                               : extent.height;
        return {clampedCopyExtentWidth, clampedCopyExtentHeight, extent.depthOrArrayLayers};
    }

    TextureViewBase* TextureBase::APICreateView(const TextureViewDescriptor* descriptor) {
        DeviceBase* device = GetDevice();

        Ref<TextureViewBase> result;
        if (device->ConsumedError(device->CreateTextureView(this, descriptor), &result)) {
            return TextureViewBase::MakeError(device);
        }
        return result.Detach();
    }

    void TextureBase::APIDestroy() {
        if (GetDevice()->ConsumedError(ValidateDestroy())) {
            return;
        }
        ASSERT(!IsError());
        DestroyInternal();
    }

    void TextureBase::DestroyImpl() {
    }

    void TextureBase::DestroyInternal() {
        DestroyImpl();
        mState = TextureState::Destroyed;
    }

    MaybeError TextureBase::ValidateDestroy() const {
        DAWN_TRY(GetDevice()->ValidateObject(this));
        return {};
    }

    // TextureViewBase

    TextureViewBase::TextureViewBase(TextureBase* texture, const TextureViewDescriptor* descriptor)
        : ObjectBase(texture->GetDevice()),
          mTexture(texture),
          mFormat(GetDevice()->GetValidInternalFormat(descriptor->format)),
          mDimension(descriptor->dimension),
          mRange({ConvertViewAspect(mFormat, descriptor->aspect),
                  {descriptor->baseArrayLayer, descriptor->arrayLayerCount},
                  {descriptor->baseMipLevel, descriptor->mipLevelCount}}) {
    }

    TextureViewBase::TextureViewBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : ObjectBase(device, tag), mFormat(kUnusedFormat) {
    }

    // static
    TextureViewBase* TextureViewBase::MakeError(DeviceBase* device) {
        return new TextureViewBase(device, ObjectBase::kError);
    }

    const TextureBase* TextureViewBase::GetTexture() const {
        ASSERT(!IsError());
        return mTexture.Get();
    }

    TextureBase* TextureViewBase::GetTexture() {
        ASSERT(!IsError());
        return mTexture.Get();
    }

    Aspect TextureViewBase::GetAspects() const {
        ASSERT(!IsError());
        return mRange.aspects;
    }

    const Format& TextureViewBase::GetFormat() const {
        ASSERT(!IsError());
        return mFormat;
    }

    wgpu::TextureViewDimension TextureViewBase::GetDimension() const {
        ASSERT(!IsError());
        return mDimension;
    }

    uint32_t TextureViewBase::GetBaseMipLevel() const {
        ASSERT(!IsError());
        return mRange.baseMipLevel;
    }

    uint32_t TextureViewBase::GetLevelCount() const {
        ASSERT(!IsError());
        return mRange.levelCount;
    }

    uint32_t TextureViewBase::GetBaseArrayLayer() const {
        ASSERT(!IsError());
        return mRange.baseArrayLayer;
    }

    uint32_t TextureViewBase::GetLayerCount() const {
        ASSERT(!IsError());
        return mRange.layerCount;
    }

    const SubresourceRange& TextureViewBase::GetSubresourceRange() const {
        ASSERT(!IsError());
        return mRange;
    }

}  // namespace dawn_native
