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

#include "dawn/native/CommandValidation.h"

#include "dawn/common/BitSetIterator.h"
#include "dawn/native/BindGroup.h"
#include "dawn/native/Buffer.h"
#include "dawn/native/CommandBufferStateTracker.h"
#include "dawn/native/Commands.h"
#include "dawn/native/Device.h"
#include "dawn/native/PassResourceUsage.h"
#include "dawn/native/QuerySet.h"
#include "dawn/native/RenderBundle.h"
#include "dawn/native/RenderPipeline.h"
#include "dawn/native/ValidationUtils_autogen.h"

namespace dawn::native {

    // Performs validation of the "synchronization scope" rules of WebGPU.
    MaybeError ValidateSyncScopeResourceUsage(const SyncScopeResourceUsage& scope) {
        // Buffers can only be used as single-write or multiple read.
        for (size_t i = 0; i < scope.bufferUsages.size(); ++i) {
            const wgpu::BufferUsage usage = scope.bufferUsages[i];
            bool readOnly = IsSubset(usage, kReadOnlyBufferUsages);
            bool singleUse = wgpu::HasZeroOrOneBits(usage);

            DAWN_INVALID_IF(!readOnly && !singleUse,
                            "%s usage (%s) includes writable usage and another usage in the same "
                            "synchronization scope.",
                            scope.buffers[i], usage);
        }

        // Check that every single subresource is used as either a single-write usage or a
        // combination of readonly usages.
        for (size_t i = 0; i < scope.textureUsages.size(); ++i) {
            const TextureSubresourceUsage& textureUsage = scope.textureUsages[i];
            MaybeError error = {};
            textureUsage.Iterate([&](const SubresourceRange&, const wgpu::TextureUsage& usage) {
                bool readOnly = IsSubset(usage, kReadOnlyTextureUsages);
                bool singleUse = wgpu::HasZeroOrOneBits(usage);
                if (!readOnly && !singleUse && !error.IsError()) {
                    error = DAWN_FORMAT_VALIDATION_ERROR(
                        "%s usage (%s) includes writable usage and another usage in the same "
                        "synchronization scope.",
                        scope.textures[i], usage);
                }
            });
            DAWN_TRY(std::move(error));
        }
        return {};
    }

    MaybeError ValidateTimestampQuery(QuerySetBase* querySet, uint32_t queryIndex) {
        DAWN_INVALID_IF(querySet->GetQueryType() != wgpu::QueryType::Timestamp,
                        "The type of %s is not %s.", querySet, wgpu::QueryType::Timestamp);

        DAWN_INVALID_IF(queryIndex >= querySet->GetQueryCount(),
                        "Query index (%u) exceeds the number of queries (%u) in %s.", queryIndex,
                        querySet->GetQueryCount(), querySet);

        return {};
    }

    MaybeError ValidateWriteBuffer(const DeviceBase* device,
                                   const BufferBase* buffer,
                                   uint64_t bufferOffset,
                                   uint64_t size) {
        DAWN_TRY(device->ValidateObject(buffer));

        DAWN_INVALID_IF(bufferOffset % 4 != 0, "BufferOffset (%u) is not a multiple of 4.",
                        bufferOffset);

        DAWN_INVALID_IF(size % 4 != 0, "Size (%u) is not a multiple of 4.", size);

        uint64_t bufferSize = buffer->GetSize();
        DAWN_INVALID_IF(bufferOffset > bufferSize || size > (bufferSize - bufferOffset),
                        "Write range (bufferOffset: %u, size: %u) does not fit in %s size (%u).",
                        bufferOffset, size, buffer, bufferSize);

        DAWN_INVALID_IF(!(buffer->GetUsage() & wgpu::BufferUsage::CopyDst),
                        "%s usage (%s) does not include %s.", buffer, buffer->GetUsage(),
                        wgpu::BufferUsage::CopyDst);

        return {};
    }

    bool IsRangeOverlapped(uint32_t startA, uint32_t startB, uint32_t length) {
        uint32_t maxStart = std::max(startA, startB);
        uint32_t minStart = std::min(startA, startB);
        return static_cast<uint64_t>(minStart) + static_cast<uint64_t>(length) >
               static_cast<uint64_t>(maxStart);
    }

    template <typename A, typename B>
    DAWN_FORCE_INLINE uint64_t Safe32x32(A a, B b) {
        static_assert(std::is_same<A, uint32_t>::value, "'a' must be uint32_t");
        static_assert(std::is_same<B, uint32_t>::value, "'b' must be uint32_t");
        return uint64_t(a) * uint64_t(b);
    }

    ResultOrError<uint64_t> ComputeRequiredBytesInCopy(const TexelBlockInfo& blockInfo,
                                                       const Extent3D& copySize,
                                                       uint32_t bytesPerRow,
                                                       uint32_t rowsPerImage) {
        ASSERT(copySize.width % blockInfo.width == 0);
        ASSERT(copySize.height % blockInfo.height == 0);
        uint32_t widthInBlocks = copySize.width / blockInfo.width;
        uint32_t heightInBlocks = copySize.height / blockInfo.height;
        uint64_t bytesInLastRow = Safe32x32(widthInBlocks, blockInfo.byteSize);

        if (copySize.depthOrArrayLayers == 0) {
            return 0;
        }

        // Check for potential overflows for the rest of the computations. We have the following
        // inequalities:
        //
        //   bytesInLastRow <= bytesPerRow
        //   heightInBlocks <= rowsPerImage
        //
        // So:
        //
        //   bytesInLastImage  = bytesPerRow * (heightInBlocks - 1) + bytesInLastRow
        //                    <= bytesPerRow * heightInBlocks
        //                    <= bytesPerRow * rowsPerImage
        //                    <= bytesPerImage
        //
        // This means that if the computation of depth * bytesPerImage doesn't overflow, none of the
        // computations for requiredBytesInCopy will. (and it's not a very pessimizing check)
        ASSERT(copySize.depthOrArrayLayers <= 1 || (bytesPerRow != wgpu::kCopyStrideUndefined &&
                                                    rowsPerImage != wgpu::kCopyStrideUndefined));
        uint64_t bytesPerImage = Safe32x32(bytesPerRow, rowsPerImage);
        DAWN_INVALID_IF(
            bytesPerImage > std::numeric_limits<uint64_t>::max() / copySize.depthOrArrayLayers,
            "The number of bytes per image (%u) exceeds the maximum (%u) when copying %u images.",
            bytesPerImage, std::numeric_limits<uint64_t>::max() / copySize.depthOrArrayLayers,
            copySize.depthOrArrayLayers);

        uint64_t requiredBytesInCopy = bytesPerImage * (copySize.depthOrArrayLayers - 1);
        if (heightInBlocks > 0) {
            ASSERT(heightInBlocks <= 1 || bytesPerRow != wgpu::kCopyStrideUndefined);
            uint64_t bytesInLastImage = Safe32x32(bytesPerRow, heightInBlocks - 1) + bytesInLastRow;
            requiredBytesInCopy += bytesInLastImage;
        }
        return requiredBytesInCopy;
    }

    MaybeError ValidateCopySizeFitsInBuffer(const Ref<BufferBase>& buffer,
                                            uint64_t offset,
                                            uint64_t size) {
        uint64_t bufferSize = buffer->GetSize();
        bool fitsInBuffer = offset <= bufferSize && (size <= (bufferSize - offset));
        DAWN_INVALID_IF(!fitsInBuffer,
                        "Copy range (offset: %u, size: %u) does not fit in %s size (%u).", offset,
                        size, buffer.Get(), bufferSize);

        return {};
    }

    // Replace wgpu::kCopyStrideUndefined with real values, so backends don't have to think about
    // it.
    void ApplyDefaultTextureDataLayoutOptions(TextureDataLayout* layout,
                                              const TexelBlockInfo& blockInfo,
                                              const Extent3D& copyExtent) {
        ASSERT(layout != nullptr);
        ASSERT(copyExtent.height % blockInfo.height == 0);
        uint32_t heightInBlocks = copyExtent.height / blockInfo.height;

        if (layout->bytesPerRow == wgpu::kCopyStrideUndefined) {
            ASSERT(copyExtent.width % blockInfo.width == 0);
            uint32_t widthInBlocks = copyExtent.width / blockInfo.width;
            uint32_t bytesInLastRow = widthInBlocks * blockInfo.byteSize;

            ASSERT(heightInBlocks <= 1 && copyExtent.depthOrArrayLayers <= 1);
            layout->bytesPerRow = Align(bytesInLastRow, kTextureBytesPerRowAlignment);
        }
        if (layout->rowsPerImage == wgpu::kCopyStrideUndefined) {
            ASSERT(copyExtent.depthOrArrayLayers <= 1);
            layout->rowsPerImage = heightInBlocks;
        }
    }

    MaybeError ValidateLinearTextureData(const TextureDataLayout& layout,
                                         uint64_t byteSize,
                                         const TexelBlockInfo& blockInfo,
                                         const Extent3D& copyExtent) {
        ASSERT(copyExtent.height % blockInfo.height == 0);
        uint32_t heightInBlocks = copyExtent.height / blockInfo.height;

        // TODO(dawn:563): Right now kCopyStrideUndefined will be formatted as a large value in the
        // validation message. Investigate ways to make it print as a more readable symbol.
        DAWN_INVALID_IF(
            copyExtent.depthOrArrayLayers > 1 &&
                (layout.bytesPerRow == wgpu::kCopyStrideUndefined ||
                 layout.rowsPerImage == wgpu::kCopyStrideUndefined),
            "Copy depth (%u) is > 1, but bytesPerRow (%u) or rowsPerImage (%u) are not specified.",
            copyExtent.depthOrArrayLayers, layout.bytesPerRow, layout.rowsPerImage);

        DAWN_INVALID_IF(heightInBlocks > 1 && layout.bytesPerRow == wgpu::kCopyStrideUndefined,
                        "HeightInBlocks (%u) is > 1, but bytesPerRow is not specified.",
                        heightInBlocks);

        // Validation for other members in layout:
        ASSERT(copyExtent.width % blockInfo.width == 0);
        uint32_t widthInBlocks = copyExtent.width / blockInfo.width;
        ASSERT(Safe32x32(widthInBlocks, blockInfo.byteSize) <=
               std::numeric_limits<uint32_t>::max());
        uint32_t bytesInLastRow = widthInBlocks * blockInfo.byteSize;

        // These != wgpu::kCopyStrideUndefined checks are technically redundant with the > checks,
        // but they should get optimized out.
        DAWN_INVALID_IF(
            layout.bytesPerRow != wgpu::kCopyStrideUndefined && bytesInLastRow > layout.bytesPerRow,
            "The byte size of each row (%u) is > bytesPerRow (%u).", bytesInLastRow,
            layout.bytesPerRow);

        DAWN_INVALID_IF(layout.rowsPerImage != wgpu::kCopyStrideUndefined &&
                            heightInBlocks > layout.rowsPerImage,
                        "The height of each image in blocks (%u) is > rowsPerImage (%u).",
                        heightInBlocks, layout.rowsPerImage);

        // We compute required bytes in copy after validating texel block alignments
        // because the divisibility conditions are necessary for the algorithm to be valid,
        // also the bytesPerRow bound is necessary to avoid overflows.
        uint64_t requiredBytesInCopy;
        DAWN_TRY_ASSIGN(requiredBytesInCopy,
                        ComputeRequiredBytesInCopy(blockInfo, copyExtent, layout.bytesPerRow,
                                                   layout.rowsPerImage));

        bool fitsInData =
            layout.offset <= byteSize && (requiredBytesInCopy <= (byteSize - layout.offset));
        DAWN_INVALID_IF(
            !fitsInData,
            "Required size for texture data layout (%u) exceeds the linear data size (%u) with "
            "offset (%u).",
            requiredBytesInCopy, byteSize, layout.offset);

        return {};
    }

    MaybeError ValidateImageCopyBuffer(DeviceBase const* device,
                                       const ImageCopyBuffer& imageCopyBuffer) {
        DAWN_TRY(device->ValidateObject(imageCopyBuffer.buffer));
        if (imageCopyBuffer.layout.bytesPerRow != wgpu::kCopyStrideUndefined) {
            DAWN_INVALID_IF(imageCopyBuffer.layout.bytesPerRow % kTextureBytesPerRowAlignment != 0,
                            "bytesPerRow (%u) is not a multiple of %u.",
                            imageCopyBuffer.layout.bytesPerRow, kTextureBytesPerRowAlignment);
        }

        return {};
    }

    MaybeError ValidateImageCopyTexture(DeviceBase const* device,
                                        const ImageCopyTexture& textureCopy,
                                        const Extent3D& copySize) {
        const TextureBase* texture = textureCopy.texture;
        DAWN_TRY(device->ValidateObject(texture));

        DAWN_INVALID_IF(textureCopy.mipLevel >= texture->GetNumMipLevels(),
                        "MipLevel (%u) is greater than the number of mip levels (%u) in %s.",
                        textureCopy.mipLevel, texture->GetNumMipLevels(), texture);

        DAWN_TRY(ValidateTextureAspect(textureCopy.aspect));
        DAWN_INVALID_IF(
            SelectFormatAspects(texture->GetFormat(), textureCopy.aspect) == Aspect::None,
            "%s format (%s) does not have the selected aspect (%s).", texture,
            texture->GetFormat().format, textureCopy.aspect);

        if (texture->GetSampleCount() > 1 || texture->GetFormat().HasDepthOrStencil()) {
            Extent3D subresourceSize = texture->GetMipLevelPhysicalSize(textureCopy.mipLevel);
            ASSERT(texture->GetDimension() == wgpu::TextureDimension::e2D);
            DAWN_INVALID_IF(
                textureCopy.origin.x != 0 || textureCopy.origin.y != 0 ||
                    subresourceSize.width != copySize.width ||
                    subresourceSize.height != copySize.height,
                "Copy origin (%s) and size (%s) does not cover the entire subresource (origin: "
                "[x: 0, y: 0], size: %s) of %s. The entire subresource must be copied when the "
                "format (%s) is a depth/stencil format or the sample count (%u) is > 1.",
                &textureCopy.origin, &copySize, &subresourceSize, texture,
                texture->GetFormat().format, texture->GetSampleCount());
        }

        return {};
    }

    MaybeError ValidateTextureCopyRange(DeviceBase const* device,
                                        const ImageCopyTexture& textureCopy,
                                        const Extent3D& copySize) {
        const TextureBase* texture = textureCopy.texture;

        // Validation for the copy being in-bounds:
        Extent3D mipSize = texture->GetMipLevelPhysicalSize(textureCopy.mipLevel);
        // For 1D/2D textures, include the array layer as depth so it can be checked with other
        // dimensions.
        if (texture->GetDimension() != wgpu::TextureDimension::e3D) {
            mipSize.depthOrArrayLayers = texture->GetArrayLayers();
        }
        // All texture dimensions are in uint32_t so by doing checks in uint64_t we avoid
        // overflows.
        DAWN_INVALID_IF(
            static_cast<uint64_t>(textureCopy.origin.x) + static_cast<uint64_t>(copySize.width) >
                    static_cast<uint64_t>(mipSize.width) ||
                static_cast<uint64_t>(textureCopy.origin.y) +
                        static_cast<uint64_t>(copySize.height) >
                    static_cast<uint64_t>(mipSize.height) ||
                static_cast<uint64_t>(textureCopy.origin.z) +
                        static_cast<uint64_t>(copySize.depthOrArrayLayers) >
                    static_cast<uint64_t>(mipSize.depthOrArrayLayers),
            "Texture copy range (origin: %s, copySize: %s) touches outside of %s mip level %u "
            "size (%s).",
            &textureCopy.origin, &copySize, texture, textureCopy.mipLevel, &mipSize);

        // Validation for the texel block alignments:
        const Format& format = textureCopy.texture->GetFormat();
        if (format.isCompressed) {
            const TexelBlockInfo& blockInfo = format.GetAspectInfo(textureCopy.aspect).block;
            DAWN_INVALID_IF(
                textureCopy.origin.x % blockInfo.width != 0,
                "Texture copy origin.x (%u) is not a multiple of compressed texture format block "
                "width (%u).",
                textureCopy.origin.x, blockInfo.width);
            DAWN_INVALID_IF(
                textureCopy.origin.y % blockInfo.height != 0,
                "Texture copy origin.y (%u) is not a multiple of compressed texture format block "
                "height (%u).",
                textureCopy.origin.y, blockInfo.height);
            DAWN_INVALID_IF(
                copySize.width % blockInfo.width != 0,
                "copySize.width (%u) is not a multiple of compressed texture format block width "
                "(%u).",
                copySize.width, blockInfo.width);
            DAWN_INVALID_IF(
                copySize.height % blockInfo.height != 0,
                "copySize.height (%u) is not a multiple of compressed texture format block "
                "height (%u).",
                copySize.height, blockInfo.height);
        }

        return {};
    }

    // Always returns a single aspect (color, stencil, depth, or ith plane for multi-planar
    // formats).
    ResultOrError<Aspect> SingleAspectUsedByImageCopyTexture(const ImageCopyTexture& view) {
        const Format& format = view.texture->GetFormat();
        switch (view.aspect) {
            case wgpu::TextureAspect::All: {
                DAWN_INVALID_IF(
                    !HasOneBit(format.aspects),
                    "More than a single aspect (%s) is selected for multi-planar format (%s) in "
                    "%s <-> linear data copy.",
                    view.aspect, format.format, view.texture);

                Aspect single = format.aspects;
                return single;
            }
            case wgpu::TextureAspect::DepthOnly:
                ASSERT(format.aspects & Aspect::Depth);
                return Aspect::Depth;
            case wgpu::TextureAspect::StencilOnly:
                ASSERT(format.aspects & Aspect::Stencil);
                return Aspect::Stencil;
            case wgpu::TextureAspect::Plane0Only:
            case wgpu::TextureAspect::Plane1Only:
                break;
        }
        UNREACHABLE();
    }

    MaybeError ValidateLinearToDepthStencilCopyRestrictions(const ImageCopyTexture& dst) {
        Aspect aspectUsed;
        DAWN_TRY_ASSIGN(aspectUsed, SingleAspectUsedByImageCopyTexture(dst));
        DAWN_INVALID_IF(aspectUsed == Aspect::Depth, "Cannot copy into the depth aspect of %s.",
                        dst.texture);

        return {};
    }

    MaybeError ValidateTextureToTextureCopyCommonRestrictions(const ImageCopyTexture& src,
                                                              const ImageCopyTexture& dst,
                                                              const Extent3D& copySize) {
        const uint32_t srcSamples = src.texture->GetSampleCount();
        const uint32_t dstSamples = dst.texture->GetSampleCount();

        DAWN_INVALID_IF(
            srcSamples != dstSamples,
            "Source %s sample count (%u) and destination %s sample count (%u) does not match.",
            src.texture, srcSamples, dst.texture, dstSamples);

        // Metal cannot select a single aspect for texture-to-texture copies.
        const Format& format = src.texture->GetFormat();
        DAWN_INVALID_IF(
            SelectFormatAspects(format, src.aspect) != format.aspects,
            "Source %s aspect (%s) doesn't select all the aspects of the source format (%s).",
            src.texture, src.aspect, format.format);

        DAWN_INVALID_IF(
            SelectFormatAspects(format, dst.aspect) != format.aspects,
            "Destination %s aspect (%s) doesn't select all the aspects of the destination format "
            "(%s).",
            dst.texture, dst.aspect, format.format);

        if (src.texture == dst.texture) {
            switch (src.texture->GetDimension()) {
                case wgpu::TextureDimension::e1D:
                    ASSERT(src.mipLevel == 0 && src.origin.z == 0 && dst.origin.z == 0);
                    return DAWN_FORMAT_VALIDATION_ERROR("Copy is from %s to itself.", src.texture);

                case wgpu::TextureDimension::e2D:
                    DAWN_INVALID_IF(src.mipLevel == dst.mipLevel &&
                                        IsRangeOverlapped(src.origin.z, dst.origin.z,
                                                          copySize.depthOrArrayLayers),
                                    "Copy source and destination are overlapping layer ranges "
                                    "([%u, %u) and [%u, %u)) of %s mip level %u",
                                    src.origin.z, src.origin.z + copySize.depthOrArrayLayers,
                                    dst.origin.z, dst.origin.z + copySize.depthOrArrayLayers,
                                    src.texture, src.mipLevel);
                    break;

                case wgpu::TextureDimension::e3D:
                    DAWN_INVALID_IF(src.mipLevel == dst.mipLevel,
                                    "Copy is from %s mip level %u to itself.", src.texture,
                                    src.mipLevel);
                    break;
            }
        }

        return {};
    }

    MaybeError ValidateTextureToTextureCopyRestrictions(const ImageCopyTexture& src,
                                                        const ImageCopyTexture& dst,
                                                        const Extent3D& copySize) {
        // Metal requires texture-to-texture copies happens between texture formats that equal to
        // each other or only have diff on srgb-ness.
        DAWN_INVALID_IF(
            !src.texture->GetFormat().CopyCompatibleWith(dst.texture->GetFormat()),
            "Source %s format (%s) and destination %s format (%s) are not copy compatible.",
            src.texture, src.texture->GetFormat().format, dst.texture,
            dst.texture->GetFormat().format);

        return ValidateTextureToTextureCopyCommonRestrictions(src, dst, copySize);
    }

    MaybeError ValidateCanUseAs(const TextureBase* texture,
                                wgpu::TextureUsage usage,
                                UsageValidationMode mode) {
        ASSERT(wgpu::HasZeroOrOneBits(usage));
        switch (mode) {
            case UsageValidationMode::Default:
                DAWN_INVALID_IF(!(texture->GetUsage() & usage), "%s usage (%s) doesn't include %s.",
                                texture, texture->GetUsage(), usage);
                break;
            case UsageValidationMode::Internal:
                DAWN_INVALID_IF(!(texture->GetInternalUsage() & usage),
                                "%s internal usage (%s) doesn't include %s.", texture,
                                texture->GetInternalUsage(), usage);
                break;
        }

        return {};
    }

    MaybeError ValidateCanUseAs(const BufferBase* buffer, wgpu::BufferUsage usage) {
        ASSERT(wgpu::HasZeroOrOneBits(usage));
        DAWN_INVALID_IF(!(buffer->GetUsage() & usage), "%s usage (%s) doesn't include %s.", buffer,
                        buffer->GetUsage(), usage);
        return {};
    }

}  // namespace dawn::native
