// Copyright 2020 The Dawn Authors
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

#include "dawn/utils/TextureUtils.h"

namespace utils {
    bool TextureFormatSupportsStorageTexture(wgpu::TextureFormat format) {
        switch (format) {
            case wgpu::TextureFormat::R32Uint:
            case wgpu::TextureFormat::R32Sint:
            case wgpu::TextureFormat::R32Float:
            case wgpu::TextureFormat::RGBA8Unorm:
            case wgpu::TextureFormat::RGBA8Snorm:
            case wgpu::TextureFormat::RGBA8Uint:
            case wgpu::TextureFormat::RGBA8Sint:
            case wgpu::TextureFormat::RG32Uint:
            case wgpu::TextureFormat::RG32Sint:
            case wgpu::TextureFormat::RG32Float:
            case wgpu::TextureFormat::RGBA16Uint:
            case wgpu::TextureFormat::RGBA16Sint:
            case wgpu::TextureFormat::RGBA16Float:
            case wgpu::TextureFormat::RGBA32Uint:
            case wgpu::TextureFormat::RGBA32Sint:
            case wgpu::TextureFormat::RGBA32Float:
                return true;

            default:
                return false;
        }
    }

    bool IsBCTextureFormat(wgpu::TextureFormat textureFormat) {
        switch (textureFormat) {
            case wgpu::TextureFormat::BC1RGBAUnorm:
            case wgpu::TextureFormat::BC1RGBAUnormSrgb:
            case wgpu::TextureFormat::BC4RUnorm:
            case wgpu::TextureFormat::BC4RSnorm:
            case wgpu::TextureFormat::BC2RGBAUnorm:
            case wgpu::TextureFormat::BC2RGBAUnormSrgb:
            case wgpu::TextureFormat::BC3RGBAUnorm:
            case wgpu::TextureFormat::BC3RGBAUnormSrgb:
            case wgpu::TextureFormat::BC5RGUnorm:
            case wgpu::TextureFormat::BC5RGSnorm:
            case wgpu::TextureFormat::BC6HRGBUfloat:
            case wgpu::TextureFormat::BC6HRGBFloat:
            case wgpu::TextureFormat::BC7RGBAUnorm:
            case wgpu::TextureFormat::BC7RGBAUnormSrgb:
                return true;

            default:
                return false;
        }
    }

    bool IsETC2TextureFormat(wgpu::TextureFormat textureFormat) {
        switch (textureFormat) {
            case wgpu::TextureFormat::ETC2RGB8Unorm:
            case wgpu::TextureFormat::ETC2RGB8UnormSrgb:
            case wgpu::TextureFormat::ETC2RGB8A1Unorm:
            case wgpu::TextureFormat::ETC2RGB8A1UnormSrgb:
            case wgpu::TextureFormat::EACR11Unorm:
            case wgpu::TextureFormat::EACR11Snorm:
            case wgpu::TextureFormat::ETC2RGBA8Unorm:
            case wgpu::TextureFormat::ETC2RGBA8UnormSrgb:
            case wgpu::TextureFormat::EACRG11Unorm:
            case wgpu::TextureFormat::EACRG11Snorm:
                return true;

            default:
                return false;
        }
    }

    bool IsASTCTextureFormat(wgpu::TextureFormat textureFormat) {
        switch (textureFormat) {
            case wgpu::TextureFormat::ASTC4x4Unorm:
            case wgpu::TextureFormat::ASTC4x4UnormSrgb:
            case wgpu::TextureFormat::ASTC5x4Unorm:
            case wgpu::TextureFormat::ASTC5x4UnormSrgb:
            case wgpu::TextureFormat::ASTC5x5Unorm:
            case wgpu::TextureFormat::ASTC5x5UnormSrgb:
            case wgpu::TextureFormat::ASTC6x5Unorm:
            case wgpu::TextureFormat::ASTC6x5UnormSrgb:
            case wgpu::TextureFormat::ASTC6x6Unorm:
            case wgpu::TextureFormat::ASTC6x6UnormSrgb:
            case wgpu::TextureFormat::ASTC8x5Unorm:
            case wgpu::TextureFormat::ASTC8x5UnormSrgb:
            case wgpu::TextureFormat::ASTC8x6Unorm:
            case wgpu::TextureFormat::ASTC8x6UnormSrgb:
            case wgpu::TextureFormat::ASTC8x8Unorm:
            case wgpu::TextureFormat::ASTC8x8UnormSrgb:
            case wgpu::TextureFormat::ASTC10x5Unorm:
            case wgpu::TextureFormat::ASTC10x5UnormSrgb:
            case wgpu::TextureFormat::ASTC10x6Unorm:
            case wgpu::TextureFormat::ASTC10x6UnormSrgb:
            case wgpu::TextureFormat::ASTC10x8Unorm:
            case wgpu::TextureFormat::ASTC10x8UnormSrgb:
            case wgpu::TextureFormat::ASTC10x10Unorm:
            case wgpu::TextureFormat::ASTC10x10UnormSrgb:
            case wgpu::TextureFormat::ASTC12x10Unorm:
            case wgpu::TextureFormat::ASTC12x10UnormSrgb:
            case wgpu::TextureFormat::ASTC12x12Unorm:
            case wgpu::TextureFormat::ASTC12x12UnormSrgb:
                return true;

            default:
                return false;
        }
    }

    bool IsDepthOnlyFormat(wgpu::TextureFormat textureFormat) {
        switch (textureFormat) {
            case wgpu::TextureFormat::Depth16Unorm:
            case wgpu::TextureFormat::Depth24Plus:
            case wgpu::TextureFormat::Depth32Float:
                return true;
            default:
                return false;
        }
    }

    bool TextureFormatSupportsMultisampling(wgpu::TextureFormat textureFormat) {
        if (IsBCTextureFormat(textureFormat) || IsETC2TextureFormat(textureFormat) ||
            IsASTCTextureFormat(textureFormat)) {
            return false;
        }

        switch (textureFormat) {
            case wgpu::TextureFormat::R32Uint:
            case wgpu::TextureFormat::R32Sint:
            case wgpu::TextureFormat::RG32Uint:
            case wgpu::TextureFormat::RG32Sint:
            case wgpu::TextureFormat::RG32Float:
            case wgpu::TextureFormat::RGBA32Uint:
            case wgpu::TextureFormat::RGBA32Sint:
            case wgpu::TextureFormat::RGBA32Float:
            case wgpu::TextureFormat::RGB9E5Ufloat:
            case wgpu::TextureFormat::R8Snorm:
            case wgpu::TextureFormat::RG8Snorm:
            case wgpu::TextureFormat::RGBA8Snorm:
            case wgpu::TextureFormat::RG11B10Ufloat:
                return false;

            default:
                return true;
        }
    }

    bool TextureFormatSupportsRendering(wgpu::TextureFormat textureFormat) {
        switch (textureFormat) {
            case wgpu::TextureFormat::R8Unorm:
            case wgpu::TextureFormat::R8Uint:
            case wgpu::TextureFormat::R8Sint:
            case wgpu::TextureFormat::RG8Unorm:
            case wgpu::TextureFormat::RG8Uint:
            case wgpu::TextureFormat::RG8Sint:
            case wgpu::TextureFormat::RGBA8Unorm:
            case wgpu::TextureFormat::RGBA8Uint:
            case wgpu::TextureFormat::RGBA8Sint:
            case wgpu::TextureFormat::BGRA8Unorm:
            case wgpu::TextureFormat::BGRA8UnormSrgb:
            case wgpu::TextureFormat::R16Uint:
            case wgpu::TextureFormat::R16Sint:
            case wgpu::TextureFormat::R16Float:
            case wgpu::TextureFormat::RG16Uint:
            case wgpu::TextureFormat::RG16Sint:
            case wgpu::TextureFormat::RG16Float:
            case wgpu::TextureFormat::RGBA16Uint:
            case wgpu::TextureFormat::RGBA16Sint:
            case wgpu::TextureFormat::RGBA16Float:
            case wgpu::TextureFormat::R32Uint:
            case wgpu::TextureFormat::R32Sint:
            case wgpu::TextureFormat::R32Float:
            case wgpu::TextureFormat::RG32Uint:
            case wgpu::TextureFormat::RG32Sint:
            case wgpu::TextureFormat::RG32Float:
            case wgpu::TextureFormat::RGBA32Uint:
            case wgpu::TextureFormat::RGBA32Sint:
            case wgpu::TextureFormat::RGBA32Float:
            case wgpu::TextureFormat::RGB10A2Unorm:
                return true;

            default:
                return false;
        }
    }

    bool TextureFormatSupportsResolveTarget(wgpu::TextureFormat textureFormat) {
        switch (textureFormat) {
            case wgpu::TextureFormat::R8Unorm:
            case wgpu::TextureFormat::RG8Unorm:
            case wgpu::TextureFormat::RGBA8Unorm:
            case wgpu::TextureFormat::RGBA8UnormSrgb:
            case wgpu::TextureFormat::BGRA8Unorm:
            case wgpu::TextureFormat::BGRA8UnormSrgb:
            case wgpu::TextureFormat::R16Float:
            case wgpu::TextureFormat::RG16Float:
            case wgpu::TextureFormat::RGBA16Float:
            case wgpu::TextureFormat::RGB10A2Unorm:
                return true;

            default:
                return false;
        }
    }

    bool IsStencilOnlyFormat(wgpu::TextureFormat textureFormat) {
        return textureFormat == wgpu::TextureFormat::Stencil8;
    }

    uint32_t GetTexelBlockSizeInBytes(wgpu::TextureFormat textureFormat) {
        switch (textureFormat) {
            case wgpu::TextureFormat::R8Unorm:
            case wgpu::TextureFormat::R8Snorm:
            case wgpu::TextureFormat::R8Uint:
            case wgpu::TextureFormat::R8Sint:
            case wgpu::TextureFormat::Stencil8:
                return 1u;

            case wgpu::TextureFormat::R16Uint:
            case wgpu::TextureFormat::R16Sint:
            case wgpu::TextureFormat::R16Float:
            case wgpu::TextureFormat::RG8Unorm:
            case wgpu::TextureFormat::RG8Snorm:
            case wgpu::TextureFormat::RG8Uint:
            case wgpu::TextureFormat::RG8Sint:
                return 2u;

            case wgpu::TextureFormat::R32Float:
            case wgpu::TextureFormat::R32Uint:
            case wgpu::TextureFormat::R32Sint:
            case wgpu::TextureFormat::RG16Uint:
            case wgpu::TextureFormat::RG16Sint:
            case wgpu::TextureFormat::RG16Float:
            case wgpu::TextureFormat::RGBA8Unorm:
            case wgpu::TextureFormat::RGBA8UnormSrgb:
            case wgpu::TextureFormat::RGBA8Snorm:
            case wgpu::TextureFormat::RGBA8Uint:
            case wgpu::TextureFormat::RGBA8Sint:
            case wgpu::TextureFormat::BGRA8Unorm:
            case wgpu::TextureFormat::BGRA8UnormSrgb:
            case wgpu::TextureFormat::RGB10A2Unorm:
            case wgpu::TextureFormat::RG11B10Ufloat:
            case wgpu::TextureFormat::RGB9E5Ufloat:
                return 4u;

            case wgpu::TextureFormat::RG32Float:
            case wgpu::TextureFormat::RG32Uint:
            case wgpu::TextureFormat::RG32Sint:
            case wgpu::TextureFormat::RGBA16Uint:
            case wgpu::TextureFormat::RGBA16Sint:
            case wgpu::TextureFormat::RGBA16Float:
                return 8u;

            case wgpu::TextureFormat::RGBA32Float:
            case wgpu::TextureFormat::RGBA32Uint:
            case wgpu::TextureFormat::RGBA32Sint:
                return 16u;

            case wgpu::TextureFormat::Depth16Unorm:
                return 2u;

            case wgpu::TextureFormat::Depth24Plus:
            case wgpu::TextureFormat::Depth24UnormStencil8:
            case wgpu::TextureFormat::Depth32Float:
                return 4u;

            case wgpu::TextureFormat::BC1RGBAUnorm:
            case wgpu::TextureFormat::BC1RGBAUnormSrgb:
            case wgpu::TextureFormat::BC4RUnorm:
            case wgpu::TextureFormat::BC4RSnorm:
                return 8u;

            case wgpu::TextureFormat::BC2RGBAUnorm:
            case wgpu::TextureFormat::BC2RGBAUnormSrgb:
            case wgpu::TextureFormat::BC3RGBAUnorm:
            case wgpu::TextureFormat::BC3RGBAUnormSrgb:
            case wgpu::TextureFormat::BC5RGUnorm:
            case wgpu::TextureFormat::BC5RGSnorm:
            case wgpu::TextureFormat::BC6HRGBUfloat:
            case wgpu::TextureFormat::BC6HRGBFloat:
            case wgpu::TextureFormat::BC7RGBAUnorm:
            case wgpu::TextureFormat::BC7RGBAUnormSrgb:
                return 16u;

            case wgpu::TextureFormat::ETC2RGB8Unorm:
            case wgpu::TextureFormat::ETC2RGB8UnormSrgb:
            case wgpu::TextureFormat::ETC2RGB8A1Unorm:
            case wgpu::TextureFormat::ETC2RGB8A1UnormSrgb:
            case wgpu::TextureFormat::EACR11Unorm:
            case wgpu::TextureFormat::EACR11Snorm:
                return 8u;

            case wgpu::TextureFormat::ETC2RGBA8Unorm:
            case wgpu::TextureFormat::ETC2RGBA8UnormSrgb:
            case wgpu::TextureFormat::EACRG11Unorm:
            case wgpu::TextureFormat::EACRG11Snorm:
                return 16u;

            case wgpu::TextureFormat::ASTC4x4Unorm:
            case wgpu::TextureFormat::ASTC4x4UnormSrgb:
            case wgpu::TextureFormat::ASTC5x4Unorm:
            case wgpu::TextureFormat::ASTC5x4UnormSrgb:
            case wgpu::TextureFormat::ASTC5x5Unorm:
            case wgpu::TextureFormat::ASTC5x5UnormSrgb:
            case wgpu::TextureFormat::ASTC6x5Unorm:
            case wgpu::TextureFormat::ASTC6x5UnormSrgb:
            case wgpu::TextureFormat::ASTC6x6Unorm:
            case wgpu::TextureFormat::ASTC6x6UnormSrgb:
            case wgpu::TextureFormat::ASTC8x5Unorm:
            case wgpu::TextureFormat::ASTC8x5UnormSrgb:
            case wgpu::TextureFormat::ASTC8x6Unorm:
            case wgpu::TextureFormat::ASTC8x6UnormSrgb:
            case wgpu::TextureFormat::ASTC8x8Unorm:
            case wgpu::TextureFormat::ASTC8x8UnormSrgb:
            case wgpu::TextureFormat::ASTC10x5Unorm:
            case wgpu::TextureFormat::ASTC10x5UnormSrgb:
            case wgpu::TextureFormat::ASTC10x6Unorm:
            case wgpu::TextureFormat::ASTC10x6UnormSrgb:
            case wgpu::TextureFormat::ASTC10x8Unorm:
            case wgpu::TextureFormat::ASTC10x8UnormSrgb:
            case wgpu::TextureFormat::ASTC10x10Unorm:
            case wgpu::TextureFormat::ASTC10x10UnormSrgb:
            case wgpu::TextureFormat::ASTC12x10Unorm:
            case wgpu::TextureFormat::ASTC12x10UnormSrgb:
            case wgpu::TextureFormat::ASTC12x12Unorm:
            case wgpu::TextureFormat::ASTC12x12UnormSrgb:
                return 16u;

            case wgpu::TextureFormat::Depth24PlusStencil8:
            case wgpu::TextureFormat::Depth32FloatStencil8:

            // Block size of a multi-planar format depends on aspect.
            case wgpu::TextureFormat::R8BG8Biplanar420Unorm:

            case wgpu::TextureFormat::Undefined:
                break;
        }
        UNREACHABLE();
    }

    uint32_t GetTextureFormatBlockWidth(wgpu::TextureFormat textureFormat) {
        switch (textureFormat) {
            case wgpu::TextureFormat::R8Unorm:
            case wgpu::TextureFormat::R8Snorm:
            case wgpu::TextureFormat::R8Uint:
            case wgpu::TextureFormat::R8Sint:
            case wgpu::TextureFormat::R16Uint:
            case wgpu::TextureFormat::R16Sint:
            case wgpu::TextureFormat::R16Float:
            case wgpu::TextureFormat::RG8Unorm:
            case wgpu::TextureFormat::RG8Snorm:
            case wgpu::TextureFormat::RG8Uint:
            case wgpu::TextureFormat::RG8Sint:
            case wgpu::TextureFormat::R32Float:
            case wgpu::TextureFormat::R32Uint:
            case wgpu::TextureFormat::R32Sint:
            case wgpu::TextureFormat::RG16Uint:
            case wgpu::TextureFormat::RG16Sint:
            case wgpu::TextureFormat::RG16Float:
            case wgpu::TextureFormat::RGBA8Unorm:
            case wgpu::TextureFormat::RGBA8UnormSrgb:
            case wgpu::TextureFormat::RGBA8Snorm:
            case wgpu::TextureFormat::RGBA8Uint:
            case wgpu::TextureFormat::RGBA8Sint:
            case wgpu::TextureFormat::BGRA8Unorm:
            case wgpu::TextureFormat::BGRA8UnormSrgb:
            case wgpu::TextureFormat::RGB10A2Unorm:
            case wgpu::TextureFormat::RG11B10Ufloat:
            case wgpu::TextureFormat::RGB9E5Ufloat:
            case wgpu::TextureFormat::RG32Float:
            case wgpu::TextureFormat::RG32Uint:
            case wgpu::TextureFormat::RG32Sint:
            case wgpu::TextureFormat::RGBA16Uint:
            case wgpu::TextureFormat::RGBA16Sint:
            case wgpu::TextureFormat::RGBA16Float:
            case wgpu::TextureFormat::RGBA32Float:
            case wgpu::TextureFormat::RGBA32Uint:
            case wgpu::TextureFormat::RGBA32Sint:
            case wgpu::TextureFormat::Depth32Float:
            case wgpu::TextureFormat::Depth24Plus:
            case wgpu::TextureFormat::Depth24PlusStencil8:
            case wgpu::TextureFormat::Depth16Unorm:
            case wgpu::TextureFormat::Depth24UnormStencil8:
            case wgpu::TextureFormat::Depth32FloatStencil8:
            case wgpu::TextureFormat::Stencil8:
                return 1u;

            case wgpu::TextureFormat::BC1RGBAUnorm:
            case wgpu::TextureFormat::BC1RGBAUnormSrgb:
            case wgpu::TextureFormat::BC4RUnorm:
            case wgpu::TextureFormat::BC4RSnorm:
            case wgpu::TextureFormat::BC2RGBAUnorm:
            case wgpu::TextureFormat::BC2RGBAUnormSrgb:
            case wgpu::TextureFormat::BC3RGBAUnorm:
            case wgpu::TextureFormat::BC3RGBAUnormSrgb:
            case wgpu::TextureFormat::BC5RGUnorm:
            case wgpu::TextureFormat::BC5RGSnorm:
            case wgpu::TextureFormat::BC6HRGBUfloat:
            case wgpu::TextureFormat::BC6HRGBFloat:
            case wgpu::TextureFormat::BC7RGBAUnorm:
            case wgpu::TextureFormat::BC7RGBAUnormSrgb:
            case wgpu::TextureFormat::ETC2RGB8Unorm:
            case wgpu::TextureFormat::ETC2RGB8UnormSrgb:
            case wgpu::TextureFormat::ETC2RGB8A1Unorm:
            case wgpu::TextureFormat::ETC2RGB8A1UnormSrgb:
            case wgpu::TextureFormat::ETC2RGBA8Unorm:
            case wgpu::TextureFormat::ETC2RGBA8UnormSrgb:
            case wgpu::TextureFormat::EACR11Unorm:
            case wgpu::TextureFormat::EACR11Snorm:
            case wgpu::TextureFormat::EACRG11Unorm:
            case wgpu::TextureFormat::EACRG11Snorm:
                return 4u;

            case wgpu::TextureFormat::ASTC4x4Unorm:
            case wgpu::TextureFormat::ASTC4x4UnormSrgb:
                return 4u;
            case wgpu::TextureFormat::ASTC5x4Unorm:
            case wgpu::TextureFormat::ASTC5x4UnormSrgb:
            case wgpu::TextureFormat::ASTC5x5Unorm:
            case wgpu::TextureFormat::ASTC5x5UnormSrgb:
                return 5u;
            case wgpu::TextureFormat::ASTC6x5Unorm:
            case wgpu::TextureFormat::ASTC6x5UnormSrgb:
            case wgpu::TextureFormat::ASTC6x6Unorm:
            case wgpu::TextureFormat::ASTC6x6UnormSrgb:
                return 6u;
            case wgpu::TextureFormat::ASTC8x5Unorm:
            case wgpu::TextureFormat::ASTC8x5UnormSrgb:
            case wgpu::TextureFormat::ASTC8x6Unorm:
            case wgpu::TextureFormat::ASTC8x6UnormSrgb:
            case wgpu::TextureFormat::ASTC8x8Unorm:
            case wgpu::TextureFormat::ASTC8x8UnormSrgb:
                return 8u;
            case wgpu::TextureFormat::ASTC10x5Unorm:
            case wgpu::TextureFormat::ASTC10x5UnormSrgb:
            case wgpu::TextureFormat::ASTC10x6Unorm:
            case wgpu::TextureFormat::ASTC10x6UnormSrgb:
            case wgpu::TextureFormat::ASTC10x8Unorm:
            case wgpu::TextureFormat::ASTC10x8UnormSrgb:
            case wgpu::TextureFormat::ASTC10x10Unorm:
            case wgpu::TextureFormat::ASTC10x10UnormSrgb:
                return 10u;
            case wgpu::TextureFormat::ASTC12x10Unorm:
            case wgpu::TextureFormat::ASTC12x10UnormSrgb:
            case wgpu::TextureFormat::ASTC12x12Unorm:
            case wgpu::TextureFormat::ASTC12x12UnormSrgb:
                return 12u;

            // Block size of a multi-planar format depends on aspect.
            case wgpu::TextureFormat::R8BG8Biplanar420Unorm:

            case wgpu::TextureFormat::Undefined:
                break;
        }
        UNREACHABLE();
    }

    uint32_t GetTextureFormatBlockHeight(wgpu::TextureFormat textureFormat) {
        switch (textureFormat) {
            case wgpu::TextureFormat::R8Unorm:
            case wgpu::TextureFormat::R8Snorm:
            case wgpu::TextureFormat::R8Uint:
            case wgpu::TextureFormat::R8Sint:
            case wgpu::TextureFormat::R16Uint:
            case wgpu::TextureFormat::R16Sint:
            case wgpu::TextureFormat::R16Float:
            case wgpu::TextureFormat::RG8Unorm:
            case wgpu::TextureFormat::RG8Snorm:
            case wgpu::TextureFormat::RG8Uint:
            case wgpu::TextureFormat::RG8Sint:
            case wgpu::TextureFormat::R32Float:
            case wgpu::TextureFormat::R32Uint:
            case wgpu::TextureFormat::R32Sint:
            case wgpu::TextureFormat::RG16Uint:
            case wgpu::TextureFormat::RG16Sint:
            case wgpu::TextureFormat::RG16Float:
            case wgpu::TextureFormat::RGBA8Unorm:
            case wgpu::TextureFormat::RGBA8UnormSrgb:
            case wgpu::TextureFormat::RGBA8Snorm:
            case wgpu::TextureFormat::RGBA8Uint:
            case wgpu::TextureFormat::RGBA8Sint:
            case wgpu::TextureFormat::BGRA8Unorm:
            case wgpu::TextureFormat::BGRA8UnormSrgb:
            case wgpu::TextureFormat::RGB10A2Unorm:
            case wgpu::TextureFormat::RG11B10Ufloat:
            case wgpu::TextureFormat::RGB9E5Ufloat:
            case wgpu::TextureFormat::RG32Float:
            case wgpu::TextureFormat::RG32Uint:
            case wgpu::TextureFormat::RG32Sint:
            case wgpu::TextureFormat::RGBA16Uint:
            case wgpu::TextureFormat::RGBA16Sint:
            case wgpu::TextureFormat::RGBA16Float:
            case wgpu::TextureFormat::RGBA32Float:
            case wgpu::TextureFormat::RGBA32Uint:
            case wgpu::TextureFormat::RGBA32Sint:
            case wgpu::TextureFormat::Depth32Float:
            case wgpu::TextureFormat::Depth24Plus:
            case wgpu::TextureFormat::Depth24PlusStencil8:
            case wgpu::TextureFormat::Depth16Unorm:
            case wgpu::TextureFormat::Depth24UnormStencil8:
            case wgpu::TextureFormat::Depth32FloatStencil8:
            case wgpu::TextureFormat::Stencil8:
                return 1u;

            case wgpu::TextureFormat::BC1RGBAUnorm:
            case wgpu::TextureFormat::BC1RGBAUnormSrgb:
            case wgpu::TextureFormat::BC4RUnorm:
            case wgpu::TextureFormat::BC4RSnorm:
            case wgpu::TextureFormat::BC2RGBAUnorm:
            case wgpu::TextureFormat::BC2RGBAUnormSrgb:
            case wgpu::TextureFormat::BC3RGBAUnorm:
            case wgpu::TextureFormat::BC3RGBAUnormSrgb:
            case wgpu::TextureFormat::BC5RGUnorm:
            case wgpu::TextureFormat::BC5RGSnorm:
            case wgpu::TextureFormat::BC6HRGBUfloat:
            case wgpu::TextureFormat::BC6HRGBFloat:
            case wgpu::TextureFormat::BC7RGBAUnorm:
            case wgpu::TextureFormat::BC7RGBAUnormSrgb:
            case wgpu::TextureFormat::ETC2RGB8Unorm:
            case wgpu::TextureFormat::ETC2RGB8UnormSrgb:
            case wgpu::TextureFormat::ETC2RGB8A1Unorm:
            case wgpu::TextureFormat::ETC2RGB8A1UnormSrgb:
            case wgpu::TextureFormat::ETC2RGBA8Unorm:
            case wgpu::TextureFormat::ETC2RGBA8UnormSrgb:
            case wgpu::TextureFormat::EACR11Unorm:
            case wgpu::TextureFormat::EACR11Snorm:
            case wgpu::TextureFormat::EACRG11Unorm:
            case wgpu::TextureFormat::EACRG11Snorm:
                return 4u;

            case wgpu::TextureFormat::ASTC4x4Unorm:
            case wgpu::TextureFormat::ASTC4x4UnormSrgb:
            case wgpu::TextureFormat::ASTC5x4Unorm:
            case wgpu::TextureFormat::ASTC5x4UnormSrgb:
                return 4u;
            case wgpu::TextureFormat::ASTC5x5Unorm:
            case wgpu::TextureFormat::ASTC5x5UnormSrgb:
            case wgpu::TextureFormat::ASTC6x5Unorm:
            case wgpu::TextureFormat::ASTC6x5UnormSrgb:
            case wgpu::TextureFormat::ASTC8x5Unorm:
            case wgpu::TextureFormat::ASTC8x5UnormSrgb:
            case wgpu::TextureFormat::ASTC10x5Unorm:
            case wgpu::TextureFormat::ASTC10x5UnormSrgb:
                return 5u;
            case wgpu::TextureFormat::ASTC6x6Unorm:
            case wgpu::TextureFormat::ASTC6x6UnormSrgb:
            case wgpu::TextureFormat::ASTC8x6Unorm:
            case wgpu::TextureFormat::ASTC8x6UnormSrgb:
            case wgpu::TextureFormat::ASTC10x6Unorm:
            case wgpu::TextureFormat::ASTC10x6UnormSrgb:
                return 6u;
            case wgpu::TextureFormat::ASTC8x8Unorm:
            case wgpu::TextureFormat::ASTC8x8UnormSrgb:
            case wgpu::TextureFormat::ASTC10x8Unorm:
            case wgpu::TextureFormat::ASTC10x8UnormSrgb:
                return 8u;
            case wgpu::TextureFormat::ASTC10x10Unorm:
            case wgpu::TextureFormat::ASTC10x10UnormSrgb:
            case wgpu::TextureFormat::ASTC12x10Unorm:
            case wgpu::TextureFormat::ASTC12x10UnormSrgb:
                return 10u;
            case wgpu::TextureFormat::ASTC12x12Unorm:
            case wgpu::TextureFormat::ASTC12x12UnormSrgb:
                return 12u;

            // Block size of a multi-planar format depends on aspect.
            case wgpu::TextureFormat::R8BG8Biplanar420Unorm:

            case wgpu::TextureFormat::Undefined:
                break;
        }
        UNREACHABLE();
    }

    const char* GetWGSLColorTextureComponentType(wgpu::TextureFormat textureFormat) {
        switch (textureFormat) {
            case wgpu::TextureFormat::R8Unorm:
            case wgpu::TextureFormat::R8Snorm:
            case wgpu::TextureFormat::R16Float:
            case wgpu::TextureFormat::RG8Unorm:
            case wgpu::TextureFormat::RG8Snorm:
            case wgpu::TextureFormat::R32Float:
            case wgpu::TextureFormat::RG16Float:
            case wgpu::TextureFormat::RGBA8Unorm:
            case wgpu::TextureFormat::RGBA8Snorm:
            case wgpu::TextureFormat::RGB10A2Unorm:
            case wgpu::TextureFormat::RG11B10Ufloat:
            case wgpu::TextureFormat::RGB9E5Ufloat:
            case wgpu::TextureFormat::RG32Float:
            case wgpu::TextureFormat::RGBA16Float:
            case wgpu::TextureFormat::RGBA32Float:
            case wgpu::TextureFormat::BGRA8Unorm:
            case wgpu::TextureFormat::BGRA8UnormSrgb:
            case wgpu::TextureFormat::RGBA8UnormSrgb:
                return "f32";

            case wgpu::TextureFormat::R8Uint:
            case wgpu::TextureFormat::R16Uint:
            case wgpu::TextureFormat::RG8Uint:
            case wgpu::TextureFormat::R32Uint:
            case wgpu::TextureFormat::RG16Uint:
            case wgpu::TextureFormat::RGBA8Uint:
            case wgpu::TextureFormat::RG32Uint:
            case wgpu::TextureFormat::RGBA16Uint:
            case wgpu::TextureFormat::RGBA32Uint:
                return "u32";

            case wgpu::TextureFormat::R8Sint:
            case wgpu::TextureFormat::R16Sint:
            case wgpu::TextureFormat::RG8Sint:
            case wgpu::TextureFormat::R32Sint:
            case wgpu::TextureFormat::RG16Sint:
            case wgpu::TextureFormat::RGBA8Sint:
            case wgpu::TextureFormat::RG32Sint:
            case wgpu::TextureFormat::RGBA16Sint:
            case wgpu::TextureFormat::RGBA32Sint:
                return "i32";

            default:
                UNREACHABLE();
        }
    }

    uint32_t GetWGSLRenderableColorTextureComponentCount(wgpu::TextureFormat textureFormat) {
        switch (textureFormat) {
            case wgpu::TextureFormat::R8Unorm:
            case wgpu::TextureFormat::R8Uint:
            case wgpu::TextureFormat::R8Sint:
            case wgpu::TextureFormat::R16Uint:
            case wgpu::TextureFormat::R16Sint:
            case wgpu::TextureFormat::R16Float:
            case wgpu::TextureFormat::R32Float:
            case wgpu::TextureFormat::R32Uint:
            case wgpu::TextureFormat::R32Sint:
                return 1u;
            case wgpu::TextureFormat::RG8Unorm:
            case wgpu::TextureFormat::RG8Uint:
            case wgpu::TextureFormat::RG8Sint:
            case wgpu::TextureFormat::RG16Uint:
            case wgpu::TextureFormat::RG16Sint:
            case wgpu::TextureFormat::RG16Float:
            case wgpu::TextureFormat::RG32Float:
            case wgpu::TextureFormat::RG32Uint:
            case wgpu::TextureFormat::RG32Sint:
                return 2u;
            case wgpu::TextureFormat::RGBA8Unorm:
            case wgpu::TextureFormat::RGBA8UnormSrgb:
            case wgpu::TextureFormat::RGBA8Uint:
            case wgpu::TextureFormat::RGBA8Sint:
            case wgpu::TextureFormat::BGRA8Unorm:
            case wgpu::TextureFormat::BGRA8UnormSrgb:
            case wgpu::TextureFormat::RGB10A2Unorm:
            case wgpu::TextureFormat::RGBA16Uint:
            case wgpu::TextureFormat::RGBA16Sint:
            case wgpu::TextureFormat::RGBA16Float:
            case wgpu::TextureFormat::RGBA32Float:
            case wgpu::TextureFormat::RGBA32Uint:
            case wgpu::TextureFormat::RGBA32Sint:
                return 4u;
            default:
                UNREACHABLE();
        }
    }

    const char* GetWGSLImageFormatQualifier(wgpu::TextureFormat textureFormat) {
        switch (textureFormat) {
            case wgpu::TextureFormat::RGBA8Unorm:
                return "rgba8unorm";
            case wgpu::TextureFormat::RGBA8Snorm:
                return "rgba8snorm";
            case wgpu::TextureFormat::RGBA8Uint:
                return "rgba8uint";
            case wgpu::TextureFormat::RGBA8Sint:
                return "rgba8sint";
            case wgpu::TextureFormat::RGBA16Uint:
                return "rgba16uint";
            case wgpu::TextureFormat::RGBA16Sint:
                return "rgba16sint";
            case wgpu::TextureFormat::RGBA16Float:
                return "rgba16float";
            case wgpu::TextureFormat::R32Uint:
                return "r32uint";
            case wgpu::TextureFormat::R32Sint:
                return "r32sint";
            case wgpu::TextureFormat::R32Float:
                return "r32float";
            case wgpu::TextureFormat::RG32Uint:
                return "rg32uint";
            case wgpu::TextureFormat::RG32Sint:
                return "rg32sint";
            case wgpu::TextureFormat::RG32Float:
                return "rg32float";
            case wgpu::TextureFormat::RGBA32Uint:
                return "rgba32uint";
            case wgpu::TextureFormat::RGBA32Sint:
                return "rgba32sint";
            case wgpu::TextureFormat::RGBA32Float:
                return "rgba32float";

            // The below do not currently exist in the WGSL spec, but are used
            // for tests that expect compilation failure.
            case wgpu::TextureFormat::R8Unorm:
                return "r8unorm";
            case wgpu::TextureFormat::R8Snorm:
                return "r8snorm";
            case wgpu::TextureFormat::R8Uint:
                return "r8uint";
            case wgpu::TextureFormat::R8Sint:
                return "r8sint";
            case wgpu::TextureFormat::R16Uint:
                return "r16uint";
            case wgpu::TextureFormat::R16Sint:
                return "r16sint";
            case wgpu::TextureFormat::R16Float:
                return "r16float";
            case wgpu::TextureFormat::RG8Unorm:
                return "rg8unorm";
            case wgpu::TextureFormat::RG8Snorm:
                return "rg8snorm";
            case wgpu::TextureFormat::RG8Uint:
                return "rg8uint";
            case wgpu::TextureFormat::RG8Sint:
                return "rg8sint";
            case wgpu::TextureFormat::RG16Uint:
                return "rg16uint";
            case wgpu::TextureFormat::RG16Sint:
                return "rg16sint";
            case wgpu::TextureFormat::RG16Float:
                return "rg16float";
            case wgpu::TextureFormat::RGB10A2Unorm:
                return "rgb10a2unorm";
            case wgpu::TextureFormat::RG11B10Ufloat:
                return "rg11b10ufloat";

            default:
                UNREACHABLE();
        }
    }

    wgpu::TextureDimension ViewDimensionToTextureDimension(
        const wgpu::TextureViewDimension dimension) {
        switch (dimension) {
            case wgpu::TextureViewDimension::e2D:
            case wgpu::TextureViewDimension::e2DArray:
            case wgpu::TextureViewDimension::Cube:
            case wgpu::TextureViewDimension::CubeArray:
                return wgpu::TextureDimension::e2D;
            case wgpu::TextureViewDimension::e3D:
                return wgpu::TextureDimension::e3D;
            // TODO(crbug.com/dawn/814): Implement for 1D texture.
            case wgpu::TextureViewDimension::e1D:
            default:
                UNREACHABLE();
                break;
        }
    }

}  // namespace utils
