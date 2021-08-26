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

#include "dawn_native/CommandValidation.h"

#include "common/BitSetIterator.h"
#include "dawn_native/BindGroup.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandBufferStateTracker.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/PassResourceUsage.h"
#include "dawn_native/QuerySet.h"
#include "dawn_native/RenderBundle.h"
#include "dawn_native/RenderPipeline.h"
#include "dawn_native/ValidationUtils_autogen.h"

namespace dawn_native {

    // Performs validation of the "synchronization scope" rules of WebGPU.
    MaybeError ValidateSyncScopeResourceUsage(const SyncScopeResourceUsage& scope) {
        // Buffers can only be used as single-write or multiple read.
        for (wgpu::BufferUsage usage : scope.bufferUsages) {
            bool readOnly = IsSubset(usage, kReadOnlyBufferUsages);
            bool singleUse = wgpu::HasZeroOrOneBits(usage);

            if (!readOnly && !singleUse) {
                return DAWN_VALIDATION_ERROR(
                    "Buffer used as writable usage and another usage in the same synchronization "
                    "scope");
            }
        }

        // Check that every single subresource is used as either a single-write usage or a
        // combination of readonly usages.
        for (const TextureSubresourceUsage& textureUsage : scope.textureUsages) {
            MaybeError error = {};
            textureUsage.Iterate([&](const SubresourceRange&, const wgpu::TextureUsage& usage) {
                bool readOnly = IsSubset(usage, kReadOnlyTextureUsages);
                bool singleUse = wgpu::HasZeroOrOneBits(usage);
                if (!readOnly && !singleUse && !error.IsError()) {
                    error = DAWN_VALIDATION_ERROR(
                        "Texture used as writable usage and another usage in the same "
                        "synchronization scope");
                }
            });
            DAWN_TRY(std::move(error));
        }
        return {};
    }

    MaybeError ValidateTimestampQuery(QuerySetBase* querySet, uint32_t queryIndex) {
        if (querySet->GetQueryType() != wgpu::QueryType::Timestamp) {
            return DAWN_VALIDATION_ERROR("The type of query set must be Timestamp");
        }

        if (queryIndex >= querySet->GetQueryCount()) {
            return DAWN_VALIDATION_ERROR("Query index exceeds the number of queries in query set");
        }

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
        if (bytesPerImage > std::numeric_limits<uint64_t>::max() / copySize.depthOrArrayLayers) {
            return DAWN_VALIDATION_ERROR("requiredBytesInCopy is too large.");
        }

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
        if (!fitsInBuffer) {
            return DAWN_VALIDATION_ERROR("Copy would overflow the buffer");
        }

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

        if (copyExtent.depthOrArrayLayers > 1 &&
            (layout.bytesPerRow == wgpu::kCopyStrideUndefined ||
             layout.rowsPerImage == wgpu::kCopyStrideUndefined)) {
            return DAWN_VALIDATION_ERROR(
                "If copy depth > 1, bytesPerRow and rowsPerImage must be specified.");
        }
        if (heightInBlocks > 1 && layout.bytesPerRow == wgpu::kCopyStrideUndefined) {
            return DAWN_VALIDATION_ERROR("If heightInBlocks > 1, bytesPerRow must be specified.");
        }

        // Validation for other members in layout:
        ASSERT(copyExtent.width % blockInfo.width == 0);
        uint32_t widthInBlocks = copyExtent.width / blockInfo.width;
        ASSERT(Safe32x32(widthInBlocks, blockInfo.byteSize) <=
               std::numeric_limits<uint32_t>::max());
        uint32_t bytesInLastRow = widthInBlocks * blockInfo.byteSize;

        // These != wgpu::kCopyStrideUndefined checks are technically redundant with the > checks,
        // but they should get optimized out.
        if (layout.bytesPerRow != wgpu::kCopyStrideUndefined &&
            bytesInLastRow > layout.bytesPerRow) {
            return DAWN_VALIDATION_ERROR("The byte size of each row must be <= bytesPerRow.");
        }
        if (layout.rowsPerImage != wgpu::kCopyStrideUndefined &&
            heightInBlocks > layout.rowsPerImage) {
            return DAWN_VALIDATION_ERROR(
                "The height of each image, in blocks, must be <= rowsPerImage.");
        }

        // We compute required bytes in copy after validating texel block alignments
        // because the divisibility conditions are necessary for the algorithm to be valid,
        // also the bytesPerRow bound is necessary to avoid overflows.
        uint64_t requiredBytesInCopy;
        DAWN_TRY_ASSIGN(requiredBytesInCopy,
                        ComputeRequiredBytesInCopy(blockInfo, copyExtent, layout.bytesPerRow,
                                                   layout.rowsPerImage));

        bool fitsInData =
            layout.offset <= byteSize && (requiredBytesInCopy <= (byteSize - layout.offset));
        if (!fitsInData) {
            return DAWN_VALIDATION_ERROR(
                "Required size for texture data layout exceeds the linear data size.");
        }

        return {};
    }

    MaybeError ValidateImageCopyBuffer(DeviceBase const* device,
                                       const ImageCopyBuffer& imageCopyBuffer) {
        DAWN_TRY(device->ValidateObject(imageCopyBuffer.buffer));
        if (imageCopyBuffer.layout.bytesPerRow != wgpu::kCopyStrideUndefined) {
            if (imageCopyBuffer.layout.bytesPerRow % kTextureBytesPerRowAlignment != 0) {
                return DAWN_VALIDATION_ERROR("bytesPerRow must be a multiple of 256");
            }
        }

        return {};
    }

    MaybeError ValidateImageCopyTexture(DeviceBase const* device,
                                        const ImageCopyTexture& textureCopy,
                                        const Extent3D& copySize) {
        const TextureBase* texture = textureCopy.texture;
        DAWN_TRY(device->ValidateObject(texture));
        if (textureCopy.mipLevel >= texture->GetNumMipLevels()) {
            return DAWN_VALIDATION_ERROR("mipLevel out of range");
        }

        DAWN_TRY(ValidateTextureAspect(textureCopy.aspect));
        if (SelectFormatAspects(texture->GetFormat(), textureCopy.aspect) == Aspect::None) {
            return DAWN_VALIDATION_ERROR("Texture does not have selected aspect for texture copy.");
        }

        if (texture->GetSampleCount() > 1 || texture->GetFormat().HasDepthOrStencil()) {
            Extent3D subresourceSize = texture->GetMipLevelPhysicalSize(textureCopy.mipLevel);
            ASSERT(texture->GetDimension() == wgpu::TextureDimension::e2D);
            if (textureCopy.origin.x != 0 || textureCopy.origin.y != 0 ||
                subresourceSize.width != copySize.width ||
                subresourceSize.height != copySize.height) {
                return DAWN_VALIDATION_ERROR(
                    "The entire subresource must be copied when using a depth/stencil texture, or "
                    "when sample count is greater than 1.");
            }
        }

        return {};
    }

    MaybeError ValidateTextureCopyRange(DeviceBase const* device,
                                        const ImageCopyTexture& textureCopy,
                                        const Extent3D& copySize) {
        const TextureBase* texture = textureCopy.texture;

        ASSERT(texture->GetDimension() != wgpu::TextureDimension::e1D);

        // Validation for the copy being in-bounds:
        Extent3D mipSize = texture->GetMipLevelPhysicalSize(textureCopy.mipLevel);
        // For 1D/2D textures, include the array layer as depth so it can be checked with other
        // dimensions.
        if (texture->GetDimension() != wgpu::TextureDimension::e3D) {
            mipSize.depthOrArrayLayers = texture->GetArrayLayers();
        }
        // All texture dimensions are in uint32_t so by doing checks in uint64_t we avoid
        // overflows.
        if (static_cast<uint64_t>(textureCopy.origin.x) + static_cast<uint64_t>(copySize.width) >
                static_cast<uint64_t>(mipSize.width) ||
            static_cast<uint64_t>(textureCopy.origin.y) + static_cast<uint64_t>(copySize.height) >
                static_cast<uint64_t>(mipSize.height) ||
            static_cast<uint64_t>(textureCopy.origin.z) +
                    static_cast<uint64_t>(copySize.depthOrArrayLayers) >
                static_cast<uint64_t>(mipSize.depthOrArrayLayers)) {
            return DAWN_VALIDATION_ERROR("Touching outside of the texture");
        }

        // Validation for the texel block alignments:
        const Format& format = textureCopy.texture->GetFormat();
        if (format.isCompressed) {
            const TexelBlockInfo& blockInfo = format.GetAspectInfo(textureCopy.aspect).block;
            if (textureCopy.origin.x % blockInfo.width != 0) {
                return DAWN_VALIDATION_ERROR(
                    "Offset.x must be a multiple of compressed texture format block width");
            }
            if (textureCopy.origin.y % blockInfo.height != 0) {
                return DAWN_VALIDATION_ERROR(
                    "Offset.y must be a multiple of compressed texture format block height");
            }
            if (copySize.width % blockInfo.width != 0) {
                return DAWN_VALIDATION_ERROR(
                    "copySize.width must be a multiple of compressed texture format block width");
            }

            if (copySize.height % blockInfo.height != 0) {
                return DAWN_VALIDATION_ERROR(
                    "copySize.height must be a multiple of compressed texture format block height");
            }
        }

        return {};
    }

    // Always returns a single aspect (color, stencil, depth, or ith plane for multi-planar
    // formats).
    ResultOrError<Aspect> SingleAspectUsedByImageCopyTexture(const ImageCopyTexture& view) {
        const Format& format = view.texture->GetFormat();
        switch (view.aspect) {
            case wgpu::TextureAspect::All:
                if (HasOneBit(format.aspects)) {
                    Aspect single = format.aspects;
                    return single;
                }
                return DAWN_VALIDATION_ERROR(
                    "A single aspect must be selected for multi-planar formats in "
                    "texture <-> linear data copies");
            case wgpu::TextureAspect::DepthOnly:
                ASSERT(format.aspects & Aspect::Depth);
                return Aspect::Depth;
            case wgpu::TextureAspect::StencilOnly:
                ASSERT(format.aspects & Aspect::Stencil);
                return Aspect::Stencil;
            case wgpu::TextureAspect::Plane0Only:
            case wgpu::TextureAspect::Plane1Only:
                UNREACHABLE();
        }
    }

    MaybeError ValidateLinearToDepthStencilCopyRestrictions(const ImageCopyTexture& dst) {
        Aspect aspectUsed;
        DAWN_TRY_ASSIGN(aspectUsed, SingleAspectUsedByImageCopyTexture(dst));
        if (aspectUsed == Aspect::Depth) {
            return DAWN_VALIDATION_ERROR("Cannot copy into the depth aspect of a texture");
        }

        return {};
    }

    MaybeError ValidateTextureToTextureCopyCommonRestrictions(const ImageCopyTexture& src,
                                                              const ImageCopyTexture& dst,
                                                              const Extent3D& copySize) {
        const uint32_t srcSamples = src.texture->GetSampleCount();
        const uint32_t dstSamples = dst.texture->GetSampleCount();

        if (srcSamples != dstSamples) {
            return DAWN_VALIDATION_ERROR(
                "Source and destination textures must have matching sample counts.");
        }

        // Metal cannot select a single aspect for texture-to-texture copies.
        const Format& format = src.texture->GetFormat();
        if (SelectFormatAspects(format, src.aspect) != format.aspects) {
            return DAWN_VALIDATION_ERROR(
                "Source aspect doesn't select all the aspects of the source format.");
        }
        if (SelectFormatAspects(format, dst.aspect) != format.aspects) {
            return DAWN_VALIDATION_ERROR(
                "Destination aspect doesn't select all the aspects of the destination format.");
        }

        if (src.texture == dst.texture && src.mipLevel == dst.mipLevel) {
            wgpu::TextureDimension dimension = src.texture->GetDimension();
            ASSERT(dimension != wgpu::TextureDimension::e1D);
            if ((dimension == wgpu::TextureDimension::e2D &&
                 IsRangeOverlapped(src.origin.z, dst.origin.z, copySize.depthOrArrayLayers)) ||
                dimension == wgpu::TextureDimension::e3D) {
                return DAWN_VALIDATION_ERROR(
                    "Cannot copy between overlapping subresources of the same texture.");
            }
        }

        return {};
    }

    MaybeError ValidateTextureToTextureCopyRestrictions(const ImageCopyTexture& src,
                                                        const ImageCopyTexture& dst,
                                                        const Extent3D& copySize) {
        if (src.texture->GetFormat().format != dst.texture->GetFormat().format) {
            // Metal requires texture-to-texture copies be the same format
            return DAWN_VALIDATION_ERROR("Source and destination texture formats must match.");
        }

        return ValidateTextureToTextureCopyCommonRestrictions(src, dst, copySize);
    }

    // CopyTextureForBrowser could handle color conversion during the copy and it
    // requires the source must be sampleable and the destination must be writable
    // using a render pass
    MaybeError ValidateCopyTextureForBrowserRestrictions(const ImageCopyTexture& src,
                                                         const ImageCopyTexture& dst,
                                                         const Extent3D& copySize) {
        if (!(src.texture->GetUsage() & wgpu::TextureUsage::TextureBinding)) {
            return DAWN_VALIDATION_ERROR("Source texture must have sampled usage");
        }

        if (!(dst.texture->GetUsage() & wgpu::TextureUsage::RenderAttachment)) {
            return DAWN_VALIDATION_ERROR("Dest texture must have RenderAttachment usage");
        }

        return ValidateTextureToTextureCopyCommonRestrictions(src, dst, copySize);
    }

    MaybeError ValidateCanUseAs(const TextureBase* texture, wgpu::TextureUsage usage) {
        ASSERT(wgpu::HasZeroOrOneBits(usage));
        if (!(texture->GetUsage() & usage)) {
            return DAWN_VALIDATION_ERROR("texture doesn't have the required usage.");
        }

        return {};
    }

    MaybeError ValidateInternalCanUseAs(const TextureBase* texture, wgpu::TextureUsage usage) {
        ASSERT(wgpu::HasZeroOrOneBits(usage));
        if (!(texture->GetInternalUsage() & usage)) {
            return DAWN_VALIDATION_ERROR("texture doesn't have the required usage.");
        }

        return {};
    }

    MaybeError ValidateCanUseAs(const BufferBase* buffer, wgpu::BufferUsage usage) {
        ASSERT(wgpu::HasZeroOrOneBits(usage));
        if (!(buffer->GetUsage() & usage)) {
            return DAWN_VALIDATION_ERROR("buffer doesn't have the required usage.");
        }

        return {};
    }

}  // namespace dawn_native
