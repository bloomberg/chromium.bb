// Copyright 2019 The Dawn Authors
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

#include "dawn_native/Format.h"
#include "dawn_native/Device.h"
#include "dawn_native/Extensions.h"

#include <bitset>

namespace dawn_native {

    // Format

    bool Format::IsColor() const {
        return aspect == Aspect::Color;
    }

    bool Format::HasDepth() const {
        return aspect == Depth || aspect == DepthStencil;
    }

    bool Format::HasStencil() const {
        return aspect == Stencil || aspect == DepthStencil;
    }

    bool Format::HasDepthOrStencil() const {
        return aspect != Color;
    }

    bool Format::HasComponentType(dawn::TextureComponentType componentType) const {
        // Depth stencil textures need to be special cased but we don't support sampling them yet.
        if (aspect != Color) {
            return false;
        }

        // Check that Type is correctly mirrors TextureComponentType except for "Other".
        static_assert(static_cast<dawn::TextureComponentType>(Type::Float) ==
                          dawn::TextureComponentType::Float,
                      "");
        static_assert(
            static_cast<dawn::TextureComponentType>(Type::Sint) == dawn::TextureComponentType::Sint,
            "");
        static_assert(
            static_cast<dawn::TextureComponentType>(Type::Uint) == dawn::TextureComponentType::Uint,
            "");
        return static_cast<dawn::TextureComponentType>(type) == componentType;
    }

    size_t Format::GetIndex() const {
        return ComputeFormatIndex(format);
    }

    // Implementation details of the format table of the DeviceBase

    // For the enum for formats are packed but this might change when we have a broader extension
    // mechanism for webgpu.h. Formats start at 1 because 0 is the undefined format.
    size_t ComputeFormatIndex(dawn::TextureFormat format) {
        // This takes advantage of overflows to make the index of TextureFormat::Undefined outside
        // of the range of the FormatTable.
        static_assert(static_cast<uint32_t>(dawn::TextureFormat::Undefined) - 1 > kKnownFormatCount,
                      "");
        return static_cast<size_t>(static_cast<uint32_t>(format) - 1);
    }

    FormatTable BuildFormatTable(const DeviceBase* device) {
        FormatTable table;
        std::bitset<kKnownFormatCount> formatsSet;

        using Type = Format::Type;
        using Aspect = Format::Aspect;

        auto AddFormat = [&table, &formatsSet](Format format) {
            size_t index = ComputeFormatIndex(format.format);
            ASSERT(index < table.size());

            // This checks that each format is set at most once, the first part of checking that all
            // formats are set exactly once.
            ASSERT(!formatsSet[index]);

            table[index] = format;
            formatsSet.set(index);
        };

        auto AddColorFormat = [&AddFormat](dawn::TextureFormat format, bool renderable,
                                           uint32_t byteSize, Type type) {
            Format internalFormat;
            internalFormat.format = format;
            internalFormat.isRenderable = renderable;
            internalFormat.isCompressed = false;
            internalFormat.isSupported = true;
            internalFormat.aspect = Aspect::Color;
            internalFormat.type = type;
            internalFormat.blockByteSize = byteSize;
            internalFormat.blockWidth = 1;
            internalFormat.blockHeight = 1;
            AddFormat(internalFormat);
        };

        auto AddDepthStencilFormat = [&AddFormat](dawn::TextureFormat format, Format::Aspect aspect,
                                                  uint32_t byteSize) {
            Format internalFormat;
            internalFormat.format = format;
            internalFormat.isRenderable = true;
            internalFormat.isCompressed = false;
            internalFormat.isSupported = true;
            internalFormat.aspect = aspect;
            internalFormat.type = Type::Other;
            internalFormat.blockByteSize = byteSize;
            internalFormat.blockWidth = 1;
            internalFormat.blockHeight = 1;
            AddFormat(internalFormat);
        };

        auto AddCompressedFormat = [&AddFormat](dawn::TextureFormat format, uint32_t byteSize,
                                                uint32_t width, uint32_t height, bool isSupported) {
            Format internalFormat;
            internalFormat.format = format;
            internalFormat.isRenderable = false;
            internalFormat.isCompressed = true;
            internalFormat.isSupported = isSupported;
            internalFormat.aspect = Aspect::Color;
            internalFormat.type = Type::Float;
            internalFormat.blockByteSize = byteSize;
            internalFormat.blockWidth = width;
            internalFormat.blockHeight = height;
            AddFormat(internalFormat);
        };

        // clang-format off

        // 1 byte color formats
        AddColorFormat(dawn::TextureFormat::R8Unorm, true, 1, Type::Float);
        AddColorFormat(dawn::TextureFormat::R8Snorm, false, 1, Type::Float);
        AddColorFormat(dawn::TextureFormat::R8Uint, true, 1, Type::Uint);
        AddColorFormat(dawn::TextureFormat::R8Sint, true, 1, Type::Sint);

        // 2 bytes color formats
        AddColorFormat(dawn::TextureFormat::R16Uint, true, 2, Type::Uint);
        AddColorFormat(dawn::TextureFormat::R16Sint, true, 2, Type::Sint);
        AddColorFormat(dawn::TextureFormat::R16Float, true, 2, Type::Float);
        AddColorFormat(dawn::TextureFormat::RG8Unorm, true, 2, Type::Float);
        AddColorFormat(dawn::TextureFormat::RG8Snorm, false, 2, Type::Float);
        AddColorFormat(dawn::TextureFormat::RG8Uint, true, 2, Type::Uint);
        AddColorFormat(dawn::TextureFormat::RG8Sint, true, 2, Type::Sint);

        // 4 bytes color formats
        AddColorFormat(dawn::TextureFormat::R32Uint, true, 4, Type::Uint);
        AddColorFormat(dawn::TextureFormat::R32Sint, true, 4, Type::Sint);
        AddColorFormat(dawn::TextureFormat::R32Float, true, 4, Type::Float);
        AddColorFormat(dawn::TextureFormat::RG16Uint, true, 4, Type::Uint);
        AddColorFormat(dawn::TextureFormat::RG16Sint, true, 4, Type::Sint);
        AddColorFormat(dawn::TextureFormat::RG16Float, true, 4, Type::Float);
        AddColorFormat(dawn::TextureFormat::RGBA8Unorm, true, 4, Type::Float);
        AddColorFormat(dawn::TextureFormat::RGBA8UnormSrgb, true, 4, Type::Float);
        AddColorFormat(dawn::TextureFormat::RGBA8Snorm, false, 4, Type::Float);
        AddColorFormat(dawn::TextureFormat::RGBA8Uint, true, 4, Type::Uint);
        AddColorFormat(dawn::TextureFormat::RGBA8Sint, true, 4, Type::Sint);
        AddColorFormat(dawn::TextureFormat::BGRA8Unorm, true, 4, Type::Float);
        AddColorFormat(dawn::TextureFormat::BGRA8UnormSrgb, true, 4, Type::Float);
        AddColorFormat(dawn::TextureFormat::RGB10A2Unorm, true, 4, Type::Float);

        AddColorFormat(dawn::TextureFormat::RG11B10Float, false, 4, Type::Float);

        // 8 bytes color formats
        AddColorFormat(dawn::TextureFormat::RG32Uint, true, 8, Type::Uint);
        AddColorFormat(dawn::TextureFormat::RG32Sint, true, 8, Type::Sint);
        AddColorFormat(dawn::TextureFormat::RG32Float, true, 8, Type::Float);
        AddColorFormat(dawn::TextureFormat::RGBA16Uint, true, 8, Type::Uint);
        AddColorFormat(dawn::TextureFormat::RGBA16Sint, true, 8, Type::Sint);
        AddColorFormat(dawn::TextureFormat::RGBA16Float, true, 8, Type::Float);

        // 16 bytes color formats
        AddColorFormat(dawn::TextureFormat::RGBA32Uint, true, 16, Type::Uint);
        AddColorFormat(dawn::TextureFormat::RGBA32Sint, true, 16, Type::Sint);
        AddColorFormat(dawn::TextureFormat::RGBA32Float, true, 16, Type::Float);

        // Depth stencil formats
        AddDepthStencilFormat(dawn::TextureFormat::Depth32Float, Aspect::Depth, 4);
        AddDepthStencilFormat(dawn::TextureFormat::Depth24Plus, Aspect::Depth, 4);
        // TODO(cwallez@chromium.org): It isn't clear if this format should be copyable
        // because its size isn't well defined, is it 4, 5 or 8?
        AddDepthStencilFormat(dawn::TextureFormat::Depth24PlusStencil8, Aspect::DepthStencil, 4);

        // BC compressed formats
        bool isBCFormatSupported = device->IsExtensionEnabled(Extension::TextureCompressionBC);
        AddCompressedFormat(dawn::TextureFormat::BC1RGBAUnorm, 8, 4, 4, isBCFormatSupported);
        AddCompressedFormat(dawn::TextureFormat::BC1RGBAUnormSrgb, 8, 4, 4, isBCFormatSupported);
        AddCompressedFormat(dawn::TextureFormat::BC4RSnorm, 8, 4, 4, isBCFormatSupported);
        AddCompressedFormat(dawn::TextureFormat::BC4RUnorm, 8, 4, 4, isBCFormatSupported);
        AddCompressedFormat(dawn::TextureFormat::BC2RGBAUnorm, 16, 4, 4, isBCFormatSupported);
        AddCompressedFormat(dawn::TextureFormat::BC2RGBAUnormSrgb, 16, 4, 4, isBCFormatSupported);
        AddCompressedFormat(dawn::TextureFormat::BC3RGBAUnorm, 16, 4, 4, isBCFormatSupported);
        AddCompressedFormat(dawn::TextureFormat::BC3RGBAUnormSrgb, 16, 4, 4, isBCFormatSupported);
        AddCompressedFormat(dawn::TextureFormat::BC5RGSnorm, 16, 4, 4, isBCFormatSupported);
        AddCompressedFormat(dawn::TextureFormat::BC5RGUnorm, 16, 4, 4, isBCFormatSupported);
        AddCompressedFormat(dawn::TextureFormat::BC6HRGBSfloat, 16, 4, 4, isBCFormatSupported);
        AddCompressedFormat(dawn::TextureFormat::BC6HRGBUfloat, 16, 4, 4, isBCFormatSupported);
        AddCompressedFormat(dawn::TextureFormat::BC7RGBAUnorm, 16, 4, 4, isBCFormatSupported);
        AddCompressedFormat(dawn::TextureFormat::BC7RGBAUnormSrgb, 16, 4, 4, isBCFormatSupported);

        // clang-format on

        // This checks that each format is set at least once, the second part of checking that all
        // formats are checked exactly once.
        ASSERT(formatsSet.all());

        return table;
    }

}  // namespace dawn_native
