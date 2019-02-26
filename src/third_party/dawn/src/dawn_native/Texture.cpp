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

#include "common/Assert.h"
#include "dawn_native/Device.h"
#include "dawn_native/ValidationUtils_autogen.h"

namespace dawn_native {
    namespace {
        // TODO(jiawei.shao@intel.com): implement texture view format compatibility rule
        MaybeError ValidateTextureViewFormatCompatibility(const TextureBase* texture,
                                                          const TextureViewDescriptor* descriptor) {
            if (texture->GetFormat() != descriptor->format) {
                return DAWN_VALIDATION_ERROR(
                    "The format of texture view is not compatible to the original texture");
            }

            return {};
        }

        // TODO(jiawei.shao@intel.com): support validation on all texture view dimensions
        bool IsTextureViewDimensionCompatibleWithTextureDimension(
            dawn::TextureViewDimension textureViewDimension,
            dawn::TextureDimension textureDimension) {
            switch (textureViewDimension) {
                case dawn::TextureViewDimension::e2D:
                case dawn::TextureViewDimension::e2DArray:
                case dawn::TextureViewDimension::Cube:
                case dawn::TextureViewDimension::CubeArray:
                    return textureDimension == dawn::TextureDimension::e2D;
                default:
                    UNREACHABLE();
                    return false;
            }
        }

        // TODO(jiawei.shao@intel.com): support validation on all texture view dimensions
        bool IsArrayLayerValidForTextureViewDimension(
            dawn::TextureViewDimension textureViewDimension,
            uint32_t textureViewArrayLayer) {
            switch (textureViewDimension) {
                case dawn::TextureViewDimension::e2D:
                    return textureViewArrayLayer == 1u;
                case dawn::TextureViewDimension::e2DArray:
                    return true;
                case dawn::TextureViewDimension::Cube:
                    return textureViewArrayLayer == 6u;
                case dawn::TextureViewDimension::CubeArray:
                    return textureViewArrayLayer % 6 == 0;
                default:
                    UNREACHABLE();
                    return false;
            }
        }

        bool IsTextureSizeValidForTextureViewDimension(
            dawn::TextureViewDimension textureViewDimension,
            const Extent3D& textureSize) {
            switch (textureViewDimension) {
                case dawn::TextureViewDimension::Cube:
                case dawn::TextureViewDimension::CubeArray:
                    return textureSize.width == textureSize.height;
                case dawn::TextureViewDimension::e2D:
                case dawn::TextureViewDimension::e2DArray:
                    return true;
                default:
                    UNREACHABLE();
                    return false;
            }
        }

        MaybeError ValidateTextureViewDimensionCompatibility(
            const TextureBase* texture,
            const TextureViewDescriptor* descriptor) {
            if (!IsArrayLayerValidForTextureViewDimension(descriptor->dimension,
                                                          descriptor->layerCount)) {
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

        TextureViewDescriptor MakeDefaultTextureViewDescriptor(const TextureBase* texture) {
            TextureViewDescriptor descriptor;
            descriptor.format = texture->GetFormat();
            descriptor.baseArrayLayer = 0;
            descriptor.layerCount = texture->GetArrayLayers();
            descriptor.baseMipLevel = 0;
            descriptor.levelCount = texture->GetNumMipLevels();

            // TODO(jiawei.shao@intel.com): support all texture dimensions.
            switch (texture->GetDimension()) {
                case dawn::TextureDimension::e2D:
                    if (texture->GetArrayLayers() == 1u) {
                        descriptor.dimension = dawn::TextureViewDimension::e2D;
                    } else {
                        descriptor.dimension = dawn::TextureViewDimension::e2DArray;
                    }
                    break;
                default:
                    UNREACHABLE();
            }

            return descriptor;
        }

    }  // anonymous namespace

    MaybeError ValidateTextureDescriptor(DeviceBase*, const TextureDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        DAWN_TRY(ValidateTextureUsageBit(descriptor->usage));
        DAWN_TRY(ValidateTextureDimension(descriptor->dimension));
        DAWN_TRY(ValidateTextureFormat(descriptor->format));

        // TODO(jiawei.shao@intel.com): check stuff based on the dimension
        if (descriptor->size.width == 0 || descriptor->size.height == 0 ||
            descriptor->size.depth == 0 || descriptor->arrayLayer == 0 ||
            descriptor->levelCount == 0) {
            return DAWN_VALIDATION_ERROR("Cannot create an empty texture");
        }

        return {};
    }

    MaybeError ValidateTextureViewDescriptor(const DeviceBase*,
                                             const TextureBase* texture,
                                             const TextureViewDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        DAWN_TRY(ValidateTextureViewDimension(descriptor->dimension));
        DAWN_TRY(ValidateTextureFormat(descriptor->format));

        // TODO(jiawei.shao@intel.com): check stuff based on resource limits
        if (descriptor->layerCount == 0 || descriptor->levelCount == 0) {
            return DAWN_VALIDATION_ERROR("Cannot create an empty texture view");
        }

        if (uint64_t(descriptor->baseArrayLayer) + uint64_t(descriptor->layerCount) >
            uint64_t(texture->GetArrayLayers())) {
            return DAWN_VALIDATION_ERROR("Texture view array-layer out of range");
        }

        if (uint64_t(descriptor->baseMipLevel) + uint64_t(descriptor->levelCount) >
            uint64_t(texture->GetNumMipLevels())) {
            return DAWN_VALIDATION_ERROR("Texture view mip-level out of range");
        }

        DAWN_TRY(ValidateTextureViewFormatCompatibility(texture, descriptor));
        DAWN_TRY(ValidateTextureViewDimensionCompatibility(texture, descriptor));

        return {};
    }

    uint32_t TextureFormatPixelSize(dawn::TextureFormat format) {
        switch (format) {
            case dawn::TextureFormat::R8Unorm:
            case dawn::TextureFormat::R8Uint:
                return 1;
            case dawn::TextureFormat::R8G8Unorm:
            case dawn::TextureFormat::R8G8Uint:
                return 2;
            case dawn::TextureFormat::R8G8B8A8Unorm:
            case dawn::TextureFormat::R8G8B8A8Uint:
            case dawn::TextureFormat::B8G8R8A8Unorm:
                return 4;
            case dawn::TextureFormat::D32FloatS8Uint:
                return 8;
            default:
                UNREACHABLE();
        }
    }

    bool TextureFormatHasDepth(dawn::TextureFormat format) {
        switch (format) {
            case dawn::TextureFormat::D32FloatS8Uint:
                return true;
            default:
                return false;
        }
    }

    bool TextureFormatHasStencil(dawn::TextureFormat format) {
        switch (format) {
            case dawn::TextureFormat::D32FloatS8Uint:
                return true;
            default:
                return false;
        }
    }

    bool TextureFormatHasDepthOrStencil(dawn::TextureFormat format) {
        switch (format) {
            case dawn::TextureFormat::D32FloatS8Uint:
                return true;
            default:
                return false;
        }
    }

    bool IsColorRenderableTextureFormat(dawn::TextureFormat format) {
        switch (format) {
            case dawn::TextureFormat::B8G8R8A8Unorm:
            case dawn::TextureFormat::R8G8B8A8Uint:
            case dawn::TextureFormat::R8G8B8A8Unorm:
            case dawn::TextureFormat::R8G8Uint:
            case dawn::TextureFormat::R8G8Unorm:
            case dawn::TextureFormat::R8Uint:
            case dawn::TextureFormat::R8Unorm:
                return true;

            case dawn::TextureFormat::D32FloatS8Uint:
                return false;

            default:
                UNREACHABLE();
                return false;
        }
    }

    // TextureBase

    TextureBase::TextureBase(DeviceBase* device, const TextureDescriptor* descriptor)
        : ObjectBase(device),
          mDimension(descriptor->dimension),
          mFormat(descriptor->format),
          mSize(descriptor->size),
          mArrayLayers(descriptor->arrayLayer),
          mNumMipLevels(descriptor->levelCount),
          mUsage(descriptor->usage) {
    }

    dawn::TextureDimension TextureBase::GetDimension() const {
        return mDimension;
    }
    dawn::TextureFormat TextureBase::GetFormat() const {
        return mFormat;
    }
    const Extent3D& TextureBase::GetSize() const {
        return mSize;
    }
    uint32_t TextureBase::GetArrayLayers() const {
        return mArrayLayers;
    }
    uint32_t TextureBase::GetNumMipLevels() const {
        return mNumMipLevels;
    }
    dawn::TextureUsageBit TextureBase::GetUsage() const {
        return mUsage;
    }

    MaybeError TextureBase::ValidateCanUseInSubmitNow() const {
        return {};
    }

    TextureViewBase* TextureBase::CreateDefaultTextureView() {
        TextureViewDescriptor descriptor = MakeDefaultTextureViewDescriptor(this);
        return GetDevice()->CreateTextureView(this, &descriptor);
    }

    TextureViewBase* TextureBase::CreateTextureView(const TextureViewDescriptor* descriptor) {
        return GetDevice()->CreateTextureView(this, descriptor);
    }

    // TextureViewBase

    TextureViewBase::TextureViewBase(TextureBase* texture, const TextureViewDescriptor* descriptor)
        : ObjectBase(texture->GetDevice()),
          mTexture(texture),
          mFormat(descriptor->format),
          mLevelCount(descriptor->levelCount),
          mLayerCount(descriptor->layerCount) {
    }

    const TextureBase* TextureViewBase::GetTexture() const {
        return mTexture.Get();
    }

    TextureBase* TextureViewBase::GetTexture() {
        return mTexture.Get();
    }

    dawn::TextureFormat TextureViewBase::GetFormat() const {
        return mFormat;
    }

    uint32_t TextureViewBase::GetLevelCount() const {
        return mLevelCount;
    }

    uint32_t TextureViewBase::GetLayerCount() const {
        return mLayerCount;
    }
}  // namespace dawn_native
